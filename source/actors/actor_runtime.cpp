// Source Repository: https://github.com/waverso/waversomapstudio

#include "actor_runtime.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../shared/animation_catalog.h"
#include "../shared/skin_catalog.h"
#include "../shared/skin_instance.h"
#include "../utils/d3dx_math_compat.h"
#include "CAnimBlendHierarchy.h"
#include "CAnimManager.h"
#include "CBaseModelInfo.h"
#include "CCamera.h"
#include "CCivilianPed.h"
#include "CEntity.h"
#include "CGame.h"
#include "CPed.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CTimer.h"
#include "CWorld.h"
#include "../scene/entity_runtime.h"
#include "extensions/ScriptCommands.h"
#include "../scene/free_camera.h"
#include "../scene/gizmos.h"
#include "../ui/gui_api.h"
#include "plugin.h"
#include "../scene/streaming_budget.h"
#include "../scene/scene.h"
#include "skin_browser.h"

ActorRuntime* g_actorRuntime = nullptr;

struct ActorRuntime::Impl
{
    struct MeshSource
    {
        RpAtomic* atomic = nullptr;
        RpGeometry* geometry = nullptr;
        const RpTriangle* triangles = nullptr;
        int vertices = 0;
        int trianglesCount = 0;
        bool operator==(const MeshSource&) const = default;
    };
    struct Entry
    {
        CPed* ped = nullptr;
        int handle = -1;
        int modelId = -1;
        int loadingId = -1;
        CBaseModelInfo* loadingInfo = nullptr;
        int index = -1;
        CVector position {};
        float yaw = 0;
        ULONGLONG freezeUntil = 0;
        unsigned poseFrame = UINT_MAX;
        ULONGLONG retryAtMs = 0;
        bool seen = false;
        bool frozen = false;
        bool gravity = true;
        bool moveForce = false;
        bool turnForce = true;
        bool dontApplySpeed = false;
        ComponentValue<ActorAnimationData> animation;
        int animationBlock = -1;
        bool animationApplied = false;
        std::vector<ActorMesh> meshes;
        std::vector<MeshSource> meshSources;
    };
    struct Request
    {
        int users = 0;
        bool owned = false;
    };
    StreamingBudget::Selection<EditorScene::MaxItems> preview;
    int capacity = 0;
    std::unordered_map<int, Entry> entries;
    std::unordered_map<CEntity*, int> owners;
    std::unordered_map<int, Request> requests;

    bool Alive(const Entry& entry) const
    {
        return entry.ped && CPools::ms_pPedPool && CPools::ms_pPedPool->GetAtRef(entry.handle) == entry.ped;
    }

    void Release(Entry& entry)
    {
        if (entry.loadingInfo)
        {
            entry.loadingInfo->RemoveRef();
        }
        entry.loadingInfo = nullptr;
        if (entry.loadingId >= 0)
        {
            auto request = requests.find(entry.loadingId);
            if (request != requests.end() && --request->second.users == 0)
            {
                if (request->second.owned)
                {
                    CStreaming::SetModelIsDeletable(entry.loadingId);
                }
                requests.erase(request);
            }
        }
        entry.loadingId = -1;
    }

    void Destroy(Entry& entry)
    {
        if (entry.ped)
        {
            owners.erase(entry.ped);
            if (Alive(entry))
            {
                CWorld::Remove(entry.ped);
                delete entry.ped;
            }
        }
        if (entry.animationBlock >= 0)
        {
            CAnimManager::RemoveAnimBlockRef(entry.animationBlock);
        }
        Release(entry);
        entry = {};
    }

    void Animate(Entry& entry, const ComponentValue<ActorAnimationData>& animation)
    {
        if (entry.animation != animation)
        {
            if (entry.animation.HasValue())
            {
                entry.ped->m_pIntelligence->FlushImmediately(true);
            }
            if (entry.animationBlock >= 0)
            {
                CAnimManager::RemoveAnimBlockRef(entry.animationBlock);
            }
            entry.animationBlock = -1;
            entry.animationApplied = false;
            entry.animation = animation;
        }
        if (!animation.HasValue() || entry.animationApplied)
        {
            return;
        }
        const auto& data = animation.Get();
        const auto* catalog = AnimationCatalog::Find(data.library, data.name);
        if (!catalog)
        {
            return;
        }
        auto* block = CAnimManager::GetAnimationBlock(data.library);
        if (!block)
        {
            return;
        }
        if (entry.animationBlock < 0)
        {
            const int index = CAnimManager::GetAnimationBlockIndex(data.library);
            if (index < 0)
            {
                return;
            }
            CAnimManager::AddAnimBlockRef(index);
            entry.animationBlock = index;
        }
        if (!block->bLoaded)
        {
            CStreaming::RequestModel(25575 + entry.animationBlock, KEEP_IN_MEMORY | PRIORITY_REQUEST);
            return;
        }
        auto* hierarchy = CAnimManager::GetAnimation(data.name, block);
        if (!hierarchy || !hierarchy->m_pSequences || !hierarchy->m_nSeqCount)
        {
            return;
        }
        plugin::Command<plugin::Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(
            entry.handle, data.name, data.library, data.delta, data.loop, data.lockX, data.lockY, data.freeze, data.time);
        entry.animationApplied = true;
        entry.poseFrame = UINT_MAX;
    }

    void Place(Entry& entry, const CVector& position, float yaw)
    {
        auto* ped = entry.ped;
        CWorld::Remove(ped);
        ped->SetPosn(position);
        const float radians = D3DXToRadian(yaw);
        ped->SetOrientation(0, 0, radians);
        ped->m_fCurrentRotation = ped->m_fAimingRotation = radians;
        ped->m_vecMoveSpeed = {};
        ped->m_vecTurnSpeed = {};
        ped->m_vecFrictionMoveSpeed = {};
        ped->m_vecFrictionTurnSpeed = {};
        ped->bIsStanding = ped->bWasStanding = false;
        ped->bProcessCollisionEvenIfStationary = true;
        ped->bCalledPreRender = ped->bDontUpdateHierarchy = false;
        ped->UpdateRwMatrix();
        ped->UpdateRwFrame();
        SkinInstance::UpdateHierarchy(ped->m_pRwClump, entry.modelId);
        CWorld::Add(ped);
        entry.position = position;
        entry.yaw = yaw;
        entry.poseFrame = UINT_MAX;
    }
};

ActorRuntime::ActorRuntime() : impl(std::make_unique<Impl>())
{
}

ActorRuntime::~ActorRuntime()
{
    Clear();
}

void ActorRuntime::Clear()
{
    for (auto& [id, entry] : impl->entries)
    {
        impl->Destroy(entry);
    }
    impl->entries.clear();
    impl->owners.clear();
    impl->capacity = 0;
}

int ActorRuntime::LookupItemIndex(CEntity* entity) const
{
    return EntityRuntime::LookupItemIndex(*impl, entity, g_scene);
}

CPed* ActorRuntime::GetPed(int itemIndex) const
{
    if (!g_scene || itemIndex < 0 || itemIndex >= g_scene->count)
    {
        return nullptr;
    }
    const auto found = impl->entries.find(g_scene->items[itemIndex].id);
    return found != impl->entries.end() && impl->Alive(found->second) ? found->second.ped : nullptr;
}

int ActorRuntime::PreviewCapacity() const
{
    return impl->capacity;
}

void ActorRuntime::ReplayAnimation(int itemIndex)
{
    auto* ped = GetPed(itemIndex);
    if (!ped)
    {
        return;
    }
    auto& entry = impl->entries.at(g_scene->items[itemIndex].id);
    if (entry.animation.HasValue())
    {
        ped->m_pIntelligence->FlushImmediately(true);
        entry.animationApplied = false;
    }
}

const std::vector<ActorMesh>* ActorRuntime::Meshes(CEntity* entity)
{
    const int index = LookupItemIndex(entity);
    if (index < 0)
    {
        return nullptr;
    }
    auto& entry = impl->entries.at(g_scene->items[index].id);
    if (entry.poseFrame == CTimer::m_FrameCounter)
    {
        return &entry.meshes;
    }
    if (!entry.ped->bCalledPreRender)
    {
        SkinInstance::UpdateHierarchy(entry.ped->m_pRwClump, entry.modelId);
    }
    struct Context
    {
        Impl::Entry& entry;
        size_t count = 0;
    } context { entry };
    RpClumpForAllAtomics(
        entry.ped->m_pRwClump,
        [](RpAtomic* atomic, void* data) -> RpAtomic*
        {
            if (!(RpAtomicGetFlags(atomic) & rpATOMICRENDER))
            {
                return atomic;
            }
            auto& context = *static_cast<Context*>(data);
            auto& entry = context.entry;
            const size_t index = context.count;
            if (index >= entry.meshes.size())
            {
                entry.meshes.resize(index + 1);
            }
            auto& mesh = entry.meshes[index];
            if (!SkinInstance::Vertices(atomic, mesh.vertices))
            {
                return atomic;
            }
            ++context.count;
            if (index >= entry.meshSources.size())
            {
                entry.meshSources.resize(index + 1);
            }
            auto* geometry = RpAtomicGetGeometry(atomic);
            const Impl::MeshSource source { atomic, geometry, geometry->triangles, geometry->numVertices,
                geometry->numTriangles };
            if (entry.meshSources[index] != source)
            {
                mesh.indices.resize(static_cast<size_t>(geometry->numTriangles) * 3);
                for (int triangle = 0; triangle < geometry->numTriangles; ++triangle)
                {
                    for (int corner = 0; corner < 3; ++corner)
                    {
                        mesh.indices[triangle * 3 + corner] = geometry->triangles[triangle].vertIndex[corner];
                    }
                }
                entry.meshSources[index] = source;
            }
            return atomic;
        },
        &context);
    entry.meshes.resize(context.count);
    entry.meshSources.resize(context.count);
    entry.poseFrame = CTimer::m_FrameCounter;
    return &entry.meshes;
}

void ActorRuntime::Update()
{
    if (!g_scene || !CPools::ms_pPedPool)
    {
        return;
    }
    const auto now = GetTickCount64();
    const bool editing = g_freeCamera && g_freeCamera->m_active;
    const CVector camera = editing ? g_freeCamera->m_position : TheCamera.GetPosition();
    int spawned = 0;
    for (auto& [id, entry] : impl->entries)
    {
        entry.seen = false;
    }
    impl->preview.Begin(*g_scene);
    for (int index = 0; index < g_scene->count; ++index)
    {
        const auto& actor = g_scene->data[index].actor;
        if (!actor.HasValue() || !SkinCatalog::IsValid(actor.Get().skinId))
        {
            continue;
        }
        const int id = g_scene->items[index].id;
        const auto found = impl->entries.find(id);
        if (found != impl->entries.end())
        {
            auto& entry = found->second;
            if ((entry.ped && (!impl->Alive(entry) || entry.modelId != actor.Get().skinId))
                || (entry.loadingId >= 0 && entry.loadingId != actor.Get().skinId))
            {
                impl->Destroy(entry);
            }
            entry.index = index;
            entry.seen = true;
            spawned += entry.ped != nullptr;
            if (now < entry.retryAtMs)
            {
                continue;
            }
        }
        const bool retained = found != impl->entries.end() && (found->second.ped || found->second.loadingId >= 0);
        const CVector delta = g_scene->world[index].pos - camera;
        const float distance = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        const float range = actor.Get().streamDistance + (retained ? 25.0f : 0.0f);
        if (distance < range * range)
        {
            impl->preview.Add(index, id, distance, retained);
        }
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
    impl->capacity = StreamingBudget::Capacity(StreamingBudget::Actors, spawned,
        static_cast<int>(CPools::ms_pPedPool->GetNoOfFreeSpaces()), StreamingBudget::PedPoolReserve);
    impl->preview.Select(impl->capacity);
    int inFlight = 0;
    for (auto it = impl->entries.begin(); it != impl->entries.end();)
    {
        if (!impl->preview.Contains(it->second.index) && now >= it->second.retryAtMs)
        {
            impl->Destroy(it->second);
            it = impl->entries.erase(it);
        }
        else
        {
            inFlight += it->second.loadingId >= 0;
            ++it;
        }
    }
    int requests = 4, spawns = 2;
    for (int candidate = 0; candidate < impl->preview.Count(); ++candidate)
    {
        const int index = impl->preview.Index(candidate);
        const auto& actor = g_scene->data[index].actor.Get();
        const auto& world = g_scene->world[index];
        auto& entry = impl->entries[g_scene->items[index].id];
        entry.index = index;
        if (!entry.ped)
        {
            if (entry.loadingId < 0)
            {
                if (!requests || inFlight >= 8)
                {
                    continue;
                }
                const bool owned = !(CStreaming::ms_aInfoForModel[actor.skinId].m_nFlags & GAME_REQUIRED);
                if (!SkinInstance::Request(actor.skinId, GAME_REQUIRED))
                {
                    entry.retryAtMs = now + 2000;
                    continue;
                }
                --requests;
                ++inFlight;
                entry.loadingId = actor.skinId;
                auto& request = impl->requests[actor.skinId];
                if (request.users++ == 0)
                {
                    request.owned = owned;
                }
            }
            auto* info = SkinInstance::Model(actor.skinId);
            if (!spawns || !info || !info->m_pRwClump || !SkinInstance::AnimationReady(actor.skinId)
                || CPools::ms_pPedPool->GetNoOfFreeSpaces() <= StreamingBudget::PedPoolReserve)
            {
                continue;
            }
            --spawns;
            info->AddRef();
            entry.loadingInfo = info;
            auto* ped = new CCivilianPed(PED_TYPE_CIVFEMALE, actor.skinId);
            if (!ped || !ped->m_pRwClump || !RpAnimBlendClumpGetAssociation(ped->m_pRwClump, 3))
            {
                delete ped;
                impl->Release(entry);
                --inFlight;
                entry.retryAtMs = now + 2000;
                continue;
            }
            RpAnimBlendClumpUpdateAnimations(ped->m_pRwClump, 0.0f, true);
            entry.ped = ped;
            entry.handle = CPools::ms_pPedPool->GetRef(ped);
            entry.modelId = actor.skinId;
            ped->SetCharCreatedBy(2);
            ped->m_pIntelligence->SetPedDecisionMakerType(eDecisionMakerType::PED_EMPTY);
            ped->m_pIntelligence->FlushImmediately(true);
            ped->m_nStatus = STATUS_PHYSICS;
            ped->m_nAreaCode = CGame::currArea;
            ped->bUsesCollision = ped->bApplyGravity = true;
            ped->bDisableMoveForce = ped->bDontApplySpeed = false;
            ped->bDisableTurnForce = true;
            ped->bBulletProof = ped->bFireProof = ped->bExplosionProof = true;
            ped->bCollisionProof = ped->bMeleeProof = true;
            ped->bNoCriticalHits = true;
            ped->bDontAcceptIKLookAts = true;
            ped->bDrownsInWater = ped->bAllowMedicsToReviveMe = false;
            impl->owners[ped] = g_scene->items[index].id;
            impl->Place(entry, world.pos, world.eulerDeg[2]);
            entry.freezeUntil = editing ? now + 1000 : 0;
            impl->Release(entry);
            --inFlight;
        }
        auto* ped = entry.ped;
        ped->m_fHealth = 100.0f;
        const bool moved = (world.pos - entry.position).Magnitude() > 0.00001f || world.eulerDeg[2] != entry.yaw;
        const bool dragging = editing
            && ((g_gizmos && g_gizmos->IsDragging() && (g_scene->items[index].selected || g_scene->HasSelectedAncestor(index)))
                || GuiIsTransformEditing(index));
        if (moved || dragging)
        {
            impl->Place(entry, world.pos, world.eulerDeg[2]);
            entry.freezeUntil = editing ? now + 1000 : 0;
        }
        if (!editing)
        {
            entry.freezeUntil = 0;
        }
        EntityRuntime::Freeze(entry, entry.ped, editing && now < entry.freezeUntil);
        if (entry.frozen && !moved && !dragging)
        {
            impl->Place(entry, world.pos, world.eulerDeg[2]);
        }
        const float metadataRadius = SkinBrowser::GetModel(actor.skinId).radius;
        const float radius = std::max(0.1f, metadataRadius > 0 ? metadataRadius : ped->GetBoundRadius());
        if (editing && (ped->GetPosition() - world.pos).Magnitude() > radius)
        {
            impl->Place(entry, world.pos, world.eulerDeg[2]);
        }
        impl->Animate(entry, g_scene->data[index].actorAnimation);
        if (!ped->bCalledPreRender)
        {
            SkinInstance::UpdateHierarchy(ped->m_pRwClump, entry.modelId);
        }
        entry.poseFrame = UINT_MAX;
    }
}
