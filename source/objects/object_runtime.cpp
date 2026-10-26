// Source Repository: https://github.com/waverso/waversomapstudio

#include "object_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <set>
#include <utility>
#include <vector>
#include <windows.h>

#include "../hooks/game_hooks.h"
#include "../scene/free_camera.h"
#include "../scene/gizmos.h"
#include "../scene/scene.h"
#include "../scene/streaming_budget.h"
#include "../shared/model_checks.h"
#include "../vendor/imgui/imgui.h"
#include "CBaseModelInfo.h"
#include "CEntity.h"
#include "CModelInfo.h"
#include "CObject.h"
#include "CPed.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CTxdStore.h"
#include "common.h"
#include "extensions/ScriptCommands.h"
#include "model_constraints.h"
#include "plugin.h"
#include "removal_manager.h"

namespace
{
    void SetObjectRotation(int handle, int modelId, const float rotation[3])
    {
        const auto applied = ModelConstraints::AppliedRotation(modelId, rotation);
        plugin::Command<plugin::Commands::SET_OBJECT_ROTATION>(handle, applied[0], applied[1], applied[2]);
    }

    constexpr float StreamHysteresis = 25.0f;
    constexpr int MaxRequestsPerFrame = 4;
    constexpr unsigned int SpawnRetryMs = 2000;
    constexpr unsigned int RbDebounceMs = 250;
    constexpr unsigned int MaterialRetryMs = 1000;
}

#include "object_runtime_textures.inl"

bool IsBreakableModel(unsigned short modelId)
{
    return ModelChecks::IsBreakable(modelId);
}

int MaterialOrderState::Update(const ItemData& data)
{
    constexpr int textOffset = ItemData::MaxMaterials;
    const int materials = data.MaterialCount();
    const int texts = data.MaterialTextCount();
    const int count = materials + texts;
    const auto sequence = [&](int entry)
    {
        return entry < textOffset ? data.materials[entry].sequence : data.materialTexts[entry - textOffset].sequence;
    };
    if (materialCount == materials && textCount == texts)
    {
        bool sorted = true;
        if (count > 1)
        {
            int previous = entries[0];
            int previousSequence = sequence(previous);
            for (int index = 1; index < count; ++index)
            {
                const int entry = entries[index];
                const int currentSequence = sequence(entry);
                if (currentSequence < previousSequence || (currentSequence == previousSequence && entry < previous))
                {
                    sorted = false;
                    break;
                }
                previous = entry;
                previousSequence = currentSequence;
            }
        }
        if (sorted)
        {
            return count;
        }
    }
    else
    {
        materialCount = materials;
        textCount = texts;
        for (int index = 0; index < materials; ++index)
        {
            entries[index] = static_cast<unsigned char>(index);
        }
        for (int index = 0; index < texts; ++index)
        {
            entries[materials + index] = static_cast<unsigned char>(textOffset + index);
        }
    }
    std::sort(entries, entries + count,
        [&](int a, int b)
        {
            return sequence(a) != sequence(b) ? sequence(a) < sequence(b) : a < b;
        });
    return count;
}

ObjectRuntime::ObjectRuntime()
{
    auto* pool = CPools::ms_pObjectPool;
    m_slotCount = pool ? pool->m_nSize : 0;
    m_poolBase = pool ? (uintptr_t)pool->m_pObjects : 0;
    m_poolStride = sizeof(*CPools::ms_pObjectPool->m_pObjects);
    m_poolSpan = (uintptr_t)m_slotCount * m_poolStride;

    if (m_slotCount > 0)
    {
        m_slotToItem = new int[m_slotCount];
        m_slotOwner = new CObject*[m_slotCount];
        for (int i = 0; i < m_slotCount; ++i)
        {
            m_slotToItem[i] = -1;
            m_slotOwner[i] = nullptr;
        }
    }
}

ObjectRuntime::~ObjectRuntime()
{
    for (int i = 0; i < m_knownCount; ++i)
    {
        DespawnItem(i);
        RevokeRemoveBuilding(i);
    }
    delete[] m_slotToItem;
    delete[] m_slotOwner;
    if (m_blankTxdSlot >= 0)
    {
        CTxdStore::RemoveTxdSlot(m_blankTxdSlot);
    }
}

void ObjectRuntime::ClearSlot(int slot)
{
    if (slot < 0 || slot >= m_slotCount)
    {
        return;
    }
    m_slotToItem[slot] = -1;
    m_slotOwner[slot] = nullptr;
}

#include "object_runtime_streaming.inl"

void ObjectRuntime::OnSceneRemap(const int* remap, int oldCount)
{
    for (int i = 0; i < oldCount; ++i)
    {
        if (remap[i] < 0)
        {
            DespawnItem(i);
            RevokeRemoveBuilding(i);
        }
    }

    static ObjectRuntimeState kept[EditorScene::MaxItems];
    int newCount = 0;
    for (int i = 0; i < oldCount; ++i)
    {
        if (remap[i] < 0)
        {
            continue;
        }
        kept[remap[i]] = m_itemStates[i];
        if (remap[i] + 1 > newCount)
        {
            newCount = remap[i] + 1;
        }
    }
    for (int i = 0; i < newCount; ++i)
    {
        m_itemStates[i] = kept[i];
    }
    for (int i = newCount; i < oldCount; ++i)
    {
        m_itemStates[i] = ObjectRuntimeState {};
    }

    for (int slot = 0; slot < m_slotCount; ++slot)
    {
        if (m_slotToItem[slot] < 0)
        {
            continue;
        }
        const int old = m_slotToItem[slot];
        m_slotToItem[slot] = (old < oldCount) ? remap[old] : -1;
        if (m_slotToItem[slot] < 0)
        {
            m_slotOwner[slot] = nullptr;
        }
    }

    m_knownCount = newCount;
}

void ObjectRuntime::RevokeRemoveBuilding(int idx)
{
    ObjectRuntimeState& rt = m_itemStates[idx];
    if (rt.rb.instanceId >= 0 && g_removalManager)
    {
        g_removalManager->RemoveInstruction(rt.rb.instanceId);
    }
    rt.rb = RemovalRuntimeState {};
}

void ObjectRuntime::SyncRemoveBuilding(int idx, ULONGLONG now)
{
    if (!g_removalManager)
    {
        return;
    }

    const SceneItem& it = g_scene->items[idx];
    const ItemData& d = g_scene->data[idx];
    ObjectRuntimeState& rt = m_itemStates[idx];

    const bool wants = it.type == SceneItemRemoveBuilding && d.removeBuilding.HasValue();
    if (!wants)
    {
        if (rt.rb.instanceId >= 0)
        {
            RevokeRemoveBuilding(idx);
        }
        return;
    }

    const CVector& wp = g_scene->world[idx].pos;
    const float pos[3] = { wp.x, wp.y, wp.z };
    const int modelId = d.removeBuilding.Get().modelId;
    const float radius = d.removeBuilding.Get().radius;

    const bool issuedSame = rt.rb.instanceId >= 0 && rt.rb.issuedModelId == modelId && rt.rb.issuedRadius == radius
        && memcmp(rt.rb.issuedPosition, pos, sizeof(pos)) == 0;
    if (issuedSame)
    {
        return;
    }

    if (rt.rb.instanceId >= 0)
    {
        if (rt.rb.pendingModelId != modelId || rt.rb.pendingRadius != radius
            || memcmp(rt.rb.pendingPosition, pos, sizeof(pos)) != 0)
        {
            rt.rb.pendingModelId = modelId;
            rt.rb.pendingRadius = radius;
            memcpy(rt.rb.pendingPosition, pos, sizeof(pos));
            rt.rb.stableAtMs = now + RbDebounceMs;
            return;
        }
        if (now < rt.rb.stableAtMs)
        {
            return;
        }
        if (g_gizmos && (g_gizmos->IsDragging() || g_gizmos->IsRadiusDragging() || g_gizmos->IsBoxAreaDragging()))
        {
            return;
        }
    }

    if (rt.rb.instanceId >= 0)
    {
        g_removalManager->RemoveInstruction(rt.rb.instanceId);
    }

    rt.rb.instanceId = g_removalManager->AddInstruction(modelId, wp, radius);
    rt.rb.issuedModelId = modelId;
    rt.rb.issuedRadius = radius;
    memcpy(rt.rb.issuedPosition, pos, sizeof(pos));
}

#include "object_runtime_transforms.inl"

void ObjectRuntime::Update()
{
    m_textCache.BeginFrame();
    if (!g_scene)
    {
        return;
    }
    EditorScene& sc = *g_scene;
    for (int i = m_knownCount; i < sc.count; ++i)
    {
        m_itemStates[i] = ObjectRuntimeState {};
    }
    m_knownCount = sc.count;

    CVector center;
    if (g_freeCamera && g_freeCamera->m_active)
    {
        center = g_freeCamera->m_position;
    }
    else
    {
        CPed* player = FindPlayerPed();
        if (!player)
        {
            return;
        }
        center = player->GetPosition();
    }

    const ULONGLONG now = GetTickCount64();
    static ULONGLONG lastSlideTick = 0;
    if (!lastSlideTick)
    {
        lastSlideTick = now;
    }
    const float slideDt = std::min(0.2f, static_cast<float>(now - lastSlideTick) / 1000.0f);
    lastSlideTick = now;

    auto* pool = CPools::ms_pObjectPool;
    int spawned = 0;
    m_preview.Begin(sc);
    for (int i = 0; i < sc.count; ++i)
    {
        auto& rt = m_itemStates[i];
        const auto& data = sc.data[i];
        SyncRemoveBuilding(i, now);
        if (rt.state == ObjectStreamState::Spawned && (!rt.obj || !pool || pool->GetAtRef(rt.scriptHandle) != rt.obj))
        {
            DespawnItem(i);
        }
        if (rt.state == ObjectStreamState::Spawned)
        {
            ++spawned;
        }
        if (sc.items[i].type != SceneItemNormal || !data.model.HasValue() || data.model.Get().modelId <= 0 || now < rt.retryAtMs)
        {
            continue;
        }
        const auto& model = data.model.Get();
        const CVector delta = sc.world[i].pos - center;
        const float distanceSquared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        const bool retained = rt.state != ObjectStreamState::Unspawned;
        const float range = model.streamDistance + (retained ? StreamHysteresis : 0.0f);
        if (distanceSquared < range * range)
        {
            m_preview.Add(i, sc.items[i].id, distanceSquared, retained);
        }
    }
    m_previewCapacity = pool ? StreamingBudget::Capacity(StreamingBudget::Objects, spawned,
                                   static_cast<int>(pool->GetNoOfFreeSpaces()), StreamingBudget::ObjectPoolReserve)
                             : 0;
    m_preview.Select(m_previewCapacity);

    int loadingInFlight = 0;
    for (int i = 0; i < sc.count; ++i)
    {
        auto& rt = m_itemStates[i];
        if (!m_preview.Contains(i))
        {
            if (rt.state != ObjectStreamState::Unspawned || rt.materialsActive)
            {
                DespawnItem(i);
            }
            continue;
        }
        const int modelId = sc.data[i].model.Get().modelId;
        if ((rt.state == ObjectStreamState::Spawned && rt.spawnedModelId != modelId)
            || (rt.state == ObjectStreamState::Loading && rt.loadingModelId != modelId))
        {
            DespawnItem(i);
            rt.boundRadius = 0.0f;
        }
        if (rt.state == ObjectStreamState::Loading)
        {
            ++loadingInFlight;
        }
    }

    int requestsThisFrame = 0;
    for (int candidate = 0; candidate < m_preview.Count(); ++candidate)
    {
        const int i = m_preview.Index(candidate);
        auto& rt = m_itemStates[i];
        const int modelId = sc.data[i].model.Get().modelId;
        switch (rt.state)
        {
            case ObjectStreamState::Unspawned:
                rt.state = ObjectStreamState::WantSpawn;
                break;
            case ObjectStreamState::WantSpawn:
                if (requestsThisFrame >= MaxRequestsPerFrame || loadingInFlight >= MaxLoadingRequests)
                {
                    break;
                }
                if (!plugin::Command<plugin::Commands::IS_MODEL_AVAILABLE>(modelId))
                {
                    rt.state = ObjectStreamState::Unspawned;
                    rt.retryAtMs = now + SpawnRetryMs;
                    break;
                }
                if (plugin::Command<plugin::Commands::HAS_MODEL_LOADED>(modelId))
                {
                    SpawnNow(i);
                }
                else if (RequestModel(i, modelId))
                {
                    rt.state = ObjectStreamState::Loading;
                    ++loadingInFlight;
                }
                ++requestsThisFrame;
                break;
            case ObjectStreamState::Loading:
                if (plugin::Command<plugin::Commands::HAS_MODEL_LOADED>(modelId))
                {
                    SpawnNow(i);
                    if (rt.state != ObjectStreamState::Loading)
                    {
                        --loadingInFlight;
                    }
                }
                break;
            case ObjectStreamState::Spawned:
                if (CBaseModelInfo* info = CModelInfo::GetModelInfo(modelId); info && info->GetModelType() == MODEL_INFO_WEAPON)
                {
                    ModelChecks::HideGunflash(rt.obj->m_pRwObject);
                }
                PushTransformIfChanged(i);
                break;
        }
        if (rt.state == ObjectStreamState::Spawned)
        {
            rt.materialsActive = true;
            SyncItemTextures(i);
            SyncItemMaterialTexts(i);
        }
    }
    UpdateSlides(slideDt);
}
