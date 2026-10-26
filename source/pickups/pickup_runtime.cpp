// Source Repository: https://github.com/waverso/waversomapstudio

#include "pickup_runtime.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "CBaseModelInfo.h"
#include "CCamera.h"
#include "CEntity.h"
#include "CModelInfo.h"
#include "CObject.h"
#include "CPickup.h"
#include "CPickups.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CTimer.h"
#include "CWorld.h"
#include "../catalog/database.h"
#include "../scene/entity_runtime.h"
#include "extensions/ScriptCommands.h"
#include "../scene/free_camera.h"
#include "pickup_position.h"
#include "plugin.h"
#include "../scene/streaming_budget.h"
#include "../scene/scene.h"

PickupRuntime* g_pickupRuntime = nullptr;

struct PickupRuntime::Impl
{
    struct Entry
    {
        int handle = -1;
        int index = -1;
        int modelId = -1;
        int loadingId = -1;
        CBaseModelInfo* heldInformation = nullptr;
        CPickup* pickup = nullptr;
        CObject* object = nullptr;
        int objectHandle = -1;
        ULONGLONG retryAtMs = 0;
        bool seen = false;
    };
    struct ModelRequest
    {
        int users = 0;
        bool owned = false;
    };
    StreamingBudget::Selection<EditorScene::MaxItems> preview;
    std::array<CVector, EditorScene::MaxItems> positions;
    std::unordered_map<int, Entry> entries;
    std::unordered_map<CEntity*, int> owners;
    std::unordered_map<CPickup*, int> pickupOwners;
    std::unordered_map<int, ModelRequest> requests;
    int previewCapacity = 0;
    bool spawning = false;

    CPickup* Slot(const Entry& entry) const
    {
        if (entry.handle < 0 || !CPickups::aPickUps)
        {
            return nullptr;
        }
        const int index = CPickups::GetActualPickupIndex(entry.handle);
        if (index < 0 || static_cast<unsigned>(index) >= MAX_NUM_PICKUPS)
        {
            return nullptr;
        }
        auto* pickup = &CPickups::aPickUps[index];
        return pickup == entry.pickup && pickup->m_nPickupType == PICKUP_IN_SHOP && pickup->m_nModelIndex == entry.modelId
            ? pickup
            : nullptr;
    }

    bool Alive(const Entry& entry) const
    {
        auto* pickup = Slot(entry);
        return pickup && entry.object && pickup->m_pObject == entry.object && CPools::ms_pObjectPool
            && CPools::ms_pObjectPool->GetAtRef(entry.objectHandle) == entry.object;
    }

    void ReleaseRequest(Entry& entry)
    {
        if (entry.loadingId < 0)
        {
            return;
        }
        const auto request = requests.find(entry.loadingId);
        if (request != requests.end() && --request->second.users == 0)
        {
            if (request->second.owned)
            {
                CStreaming::SetModelIsDeletable(entry.loadingId);
            }
            requests.erase(request);
        }
        entry.loadingId = -1;
    }

    void Destroy(Entry& entry)
    {
        owners.erase(entry.object);
        pickupOwners.erase(entry.pickup);
        if (Slot(entry))
        {
            CPickups::RemovePickUp(entry.handle);
        }
        if (entry.heldInformation)
        {
            entry.heldInformation->RemoveRef();
        }
        ReleaseRequest(entry);
        entry = {};
    }

    void RegisterObject(Entry& entry, int id)
    {
        auto* pickup = Slot(entry);
        auto* object = pickup ? pickup->m_pObject : nullptr;
        if (entry.object == object && Alive(entry))
        {
            return;
        }
        owners.erase(entry.object);
        entry.object = object;
        entry.objectHandle = object && CPools::ms_pObjectPool ? CPools::ms_pObjectPool->GetRef(object) : -1;
        if (object)
        {
            owners[object] = id;
        }
    }

    int FreeSlots() const
    {
        int count = 0;
        for (unsigned index = 0; index < MAX_NUM_PICKUPS; ++index)
        {
            count += CPickups::aPickUps[index].m_nPickupType == PICKUP_NONE;
        }
        return count;
    }

    void Place(Entry& entry, const CVector& position)
    {
        auto* pickup = Slot(entry);
        if (!pickup)
        {
            return;
        }
        pickup->SetPosn(position.x, position.y, position.z);
        if (auto* object = pickup->m_pObject)
        {
            if ((object->GetPosition() - position).Magnitude() > 0.00001f)
            {
                CWorld::Remove(object);
                object->SetPosn(position);
                object->UpdateRwMatrix();
                object->UpdateRwFrame();
                CWorld::Add(object);
            }
            object->m_nAreaCode = 13;
            object->bUsesCollision = false;
            object->bApplyGravity = false;
            object->m_vecMoveSpeed = {};
            object->m_vecTurnSpeed = {};
        }
    }
};

PickupRuntime::PickupRuntime() : impl(std::make_unique<Impl>())
{
}

PickupRuntime::~PickupRuntime()
{
    Clear();
}

void PickupRuntime::Clear()
{
    for (auto& [id, entry] : impl->entries)
    {
        impl->Destroy(entry);
    }
    impl->entries.clear();
    impl->owners.clear();
    impl->pickupOwners.clear();
    impl->previewCapacity = 0;
}

bool PickupRuntime::IsValidModel(int modelId)
{
    if (modelId < 321 || modelId >= CatalogDatabase::MaxModelId || (modelId >= 400 && modelId <= 614)
        || !CatalogDatabase::ModelExists(static_cast<std::uint16_t>(modelId)))
    {
        return false;
    }
    auto* information = CModelInfo::GetModelInfo(modelId);
    return information && information->GetModelType() != MODEL_INFO_PED && information->GetModelType() != MODEL_INFO_VEHICLE;
}

int PickupRuntime::LookupItemIndex(CEntity* entity) const
{
    return EntityRuntime::LookupItemIndex(*impl, entity, g_scene);
}

float PickupRuntime::GetVisualScale(int modelId)
{
    if (modelId < 0 || modelId >= CatalogDatabase::MaxModelId)
    {
        return 0.0f;
    }
    auto* information = CModelInfo::GetModelInfo(modelId);
    if (!information || !information->m_pColModel)
    {
        return 0.0f;
    }
    const auto& box = information->m_pColModel->m_boundBox;
    const double largestDimension = std::max({ static_cast<double>(box.m_vecMax.x) - box.m_vecMin.x,
        static_cast<double>(box.m_vecMax.y - box.m_vecMin.y), static_cast<double>(box.m_vecMax.z - box.m_vecMin.z) });
    return PickupPosition::VisualScale(modelId, largestDimension);
}

CObject* PickupRuntime::GetObject(int itemIndex) const
{
    if (!g_scene || itemIndex < 0 || itemIndex >= g_scene->count)
    {
        return nullptr;
    }
    const auto entry = impl->entries.find(g_scene->items[itemIndex].id);
    return entry != impl->entries.end() && impl->Alive(entry->second) ? entry->second.object : nullptr;
}

bool PickupRuntime::GetPosition(int itemIndex, CVector& position) const
{
    if (!g_scene || itemIndex < 0 || itemIndex >= g_scene->count || !g_scene->data[itemIndex].pickup.HasValue())
    {
        return false;
    }
    const auto& world = g_scene->world[itemIndex].pos;
    const float input[3] = { world.x, world.y, world.z };
    float result[3];
    if (!PickupPosition::Resolve(input, result, CWorld::FindGroundZForCoord))
    {
        return false;
    }
    position = CVector(result[0], result[1], result[2]);
    return true;
}

bool PickupRuntime::OwnsPickup(CPickup* pickup) const
{
    const auto owner = impl->pickupOwners.find(pickup);
    if (owner == impl->pickupOwners.end())
    {
        return false;
    }
    const auto entry = impl->entries.find(owner->second);
    return entry != impl->entries.end() && impl->Slot(entry->second) == pickup;
}

bool PickupRuntime::IsSpawning() const
{
    return impl->spawning;
}

int PickupRuntime::PreviewCapacity() const
{
    return impl->previewCapacity;
}

void PickupRuntime::Update()
{
    if (!g_scene || !CPickups::aPickUps || !CPools::ms_pObjectPool)
    {
        return;
    }
    const auto now = GetTickCount64();
    const CVector camera = g_freeCamera && g_freeCamera->m_active ? g_freeCamera->m_position : TheCamera.GetPosition();
    int reserved = 0;
    int objects = 0;
    for (auto& [id, entry] : impl->entries)
    {
        entry.seen = false;
    }
    impl->preview.Begin(*g_scene);
    for (int index = 0; index < g_scene->count; ++index)
    {
        const auto& data = g_scene->data[index];
        if (!data.pickup.HasValue() || !IsValidModel(data.pickup.Get().modelId))
        {
            continue;
        }
        const int id = g_scene->items[index].id;
        const auto existing = impl->entries.find(id);
        if (existing != impl->entries.end())
        {
            auto& entry = existing->second;
            if (entry.modelId != data.pickup.Get().modelId || (entry.handle >= 0 && !impl->Slot(entry)))
            {
                impl->Destroy(entry);
            }
        }
        const bool retained
            = existing != impl->entries.end() && (existing->second.handle >= 0 || existing->second.loadingId >= 0);
        const float range = data.pickup.Get().streamDistance + (retained ? 25.0f : 0.0f);
        const auto& world = g_scene->world[index].pos;
        const float input[3] = { world.x, world.y, world.z };
        if (!PickupPosition::ValidInput(input))
        {
            continue;
        }
        const float dx = PickupPosition::Quantize(world.x) - camera.x;
        const float dy = PickupPosition::Quantize(world.y) - camera.y;
        if (dx * dx + dy * dy >= range * range)
        {
            continue;
        }
        auto& position = impl->positions[index];
        if (!GetPosition(index, position))
        {
            continue;
        }
        const CVector delta = position - camera;
        const float distance = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        if (!(distance < range * range))
        {
            continue;
        }
        if (existing != impl->entries.end())
        {
            auto& entry = existing->second;
            entry.index = index;
            entry.seen = true;
            reserved += impl->Slot(entry) != nullptr;
            impl->RegisterObject(entry, id);
            objects += impl->Alive(entry);
            if (now < entry.retryAtMs)
            {
                continue;
            }
        }
        impl->preview.Add(index, id, distance, retained);
    }
    for (auto it = impl->entries.begin(); it != impl->entries.end();)
    {
        if (!it->second.seen)
        {
            impl->Destroy(it->second);
            it = impl->entries.erase(it);
        }
        else
        {
            ++it;
        }
    }
    impl->previewCapacity = std::min(
        StreamingBudget::Capacity(StreamingBudget::Pickups, reserved, impl->FreeSlots(), StreamingBudget::PickupPoolReserve),
        StreamingBudget::Capacity(
            StreamingBudget::Pickups, objects, CPools::ms_pObjectPool->GetNoOfFreeSpaces(), StreamingBudget::ObjectPoolReserve));
    impl->preview.Select(impl->previewCapacity);
    int loading = 0;
    for (auto it = impl->entries.begin(); it != impl->entries.end();)
    {
        if (!impl->preview.Contains(it->second.index) && now >= it->second.retryAtMs)
        {
            impl->Destroy(it->second);
            it = impl->entries.erase(it);
        }
        else
        {
            loading += it->second.loadingId >= 0;
            ++it;
        }
    }
    int requests = 4;
    int spawns = 2;
    for (int candidate = 0; candidate < impl->preview.Count(); ++candidate)
    {
        const int index = impl->preview.Index(candidate);
        const int id = g_scene->items[index].id;
        auto& entry = impl->entries[id];
        entry.index = index;
        const auto& data = g_scene->data[index].pickup.Get();
        entry.modelId = data.modelId;
        const auto& position = impl->positions[index];
        if (!impl->Slot(entry))
        {
            if (entry.loadingId < 0)
            {
                if (!requests || loading >= 8)
                {
                    continue;
                }
                --requests;
                ++loading;
                entry.loadingId = data.modelId;
                auto& request = impl->requests[data.modelId];
                if (request.users++ == 0 && !plugin::Command<plugin::Commands::HAS_MODEL_LOADED>(data.modelId))
                {
                    request.owned = !(CStreaming::ms_aInfoForModel[data.modelId].m_nFlags & GAME_REQUIRED);
                    CStreaming::RequestModel(data.modelId, GAME_REQUIRED);
                }
            }
            if (!spawns || !plugin::Command<plugin::Commands::HAS_MODEL_LOADED>(data.modelId)
                || impl->FreeSlots() <= StreamingBudget::PickupPoolReserve
                || CPools::ms_pObjectPool->GetNoOfFreeSpaces() <= StreamingBudget::ObjectPoolReserve)
            {
                continue;
            }
            auto* information = CModelInfo::GetModelInfo(data.modelId);
            if (!information || !information->m_pRwObject || !information->m_pColModel)
            {
                impl->ReleaseRequest(entry);
                --loading;
                entry.retryAtMs = now + 2000;
                continue;
            }
            --spawns;
            impl->spawning = true;
            const int handle = CPickups::GenerateNewOne(position, data.modelId, PICKUP_IN_SHOP, 0, 0, false, nullptr);
            impl->spawning = false;
            const int slot = handle >= 0 ? CPickups::GetActualPickupIndex(handle) : -1;
            if (slot < 0 || static_cast<unsigned>(slot) >= MAX_NUM_PICKUPS)
            {
                if (handle >= 0)
                {
                    CPickups::RemovePickUp(handle);
                }
                impl->ReleaseRequest(entry);
                --loading;
                entry.retryAtMs = now + 2000;
                continue;
            }
            entry.handle = handle;
            entry.heldInformation = information;
            information->AddRef();
            entry.pickup = &CPickups::aPickUps[slot];
            impl->pickupOwners[entry.pickup] = id;
            const float dx = position.x - camera.x, dy = position.y - camera.y;
            if (!entry.pickup->m_pObject && dx * dx + dy * dy < 10000.0f)
            {
                impl->spawning = true;
                entry.pickup->GiveUsAPickUpObject(&entry.pickup->m_pObject, -1);
                if (entry.pickup->m_pObject)
                {
                    CWorld::Add(entry.pickup->m_pObject);
                }
                impl->spawning = false;
            }
            impl->RegisterObject(entry, id);
            impl->ReleaseRequest(entry);
            --loading;
        }
        impl->Place(entry, position);
    }
}

static std::vector<CObject*> g_pickupObjs;
static unsigned int g_lastFrame = 0xFFFFFFFFU;

static void ReconcilePickups()
{
    g_lastFrame = CTimer::m_FrameCounter;
    g_pickupObjs.clear();

    CPickup* arr = CPickups::aPickUps;
    if (!arr)
    {
        return;
    }
    g_pickupObjs.reserve(MAX_NUM_PICKUPS);
    for (unsigned int i = 0; i < MAX_NUM_PICKUPS; ++i)
    {
        CObject* obj = arr[i].m_pObject;
        if (obj)
        {
            g_pickupObjs.push_back(obj);
        }
    }
    std::sort(g_pickupObjs.begin(), g_pickupObjs.end(), std::less<CObject*>());
}

bool PickupIsPickupObject(CObject* obj)
{
    if (!obj)
    {
        return false;
    }
    if (CTimer::m_FrameCounter != g_lastFrame)
    {
        ReconcilePickups();
    }
    return std::binary_search(g_pickupObjs.begin(), g_pickupObjs.end(), obj, std::less<CObject*>());
}
