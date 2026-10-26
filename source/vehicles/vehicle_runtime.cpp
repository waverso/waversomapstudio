// Source Repository: https://github.com/waverso/waversomapstudio

#include "vehicle_runtime.h"

#include <algorithm>
#include <memory>
#include <unordered_map>

#include "../shared/vehicle_catalog.h"
#include "../shared/vehicle_instance.h"
#include "../utils/d3dx_math_compat.h"
#include "CBaseModelInfo.h"
#include "CCamera.h"
#include "CEntity.h"
#include "CGame.h"
#include "CModelInfo.h"
#include "CPools.h"
#include "CStreaming.h"
#include "CVehicle.h"
#include "CVehicleModelInfo.h"
#include "CWorld.h"
#include "../actors/actor_runtime.h"
#include "../scene/entity_runtime.h"
#include "extensions/ScriptCommands.h"
#include "../scene/free_camera.h"
#include "../scene/gizmos.h"
#include "../ui/gui_api.h"
#include "../objects/object_runtime.h"
#include "../pickups/pickup_runtime.h"
#include "plugin.h"
#include "../scene/streaming_budget.h"
#include "../scene/scene.h"
#include "vehicle_browser.h"

VehicleRuntime* g_vehicleRuntime = nullptr;

struct VehicleRuntime::Impl
{
    struct Entry
    {
        CVehicle* vehicle = nullptr;
        int handle = -1;
        int modelId = -1;
        int loadingId = -1;
        CBaseModelInfo* loadingInfo = nullptr;
        int index = -1;
        CVector position = {};
        float yaw = 0.0f;
        ULONGLONG freezeUntil = 0;
        unsigned char randomColors[2] = { 1, 1 };
        ULONGLONG retryAtMs = 0;
        bool seen = false;
        bool frozen = false;
        bool gravity = true;
        bool moveForce = false;
        bool turnForce = false;
        bool dontApplySpeed = false;
    };
    struct ModelRequest
    {
        int users = 0;
        bool owned = false;
    };
    StreamingBudget::Selection<EditorScene::MaxItems> preview;
    int previewCapacity = 0;
    std::unordered_map<int, Entry> entries;
    std::unordered_map<CEntity*, int> owners;
    std::unordered_map<int, ModelRequest> requests;

    bool Alive(const Entry& entry) const
    {
        return entry.vehicle && CPools::ms_pVehiclePool && CPools::ms_pVehiclePool->GetAtRef(entry.handle) == entry.vehicle;
    }

    void ReleaseRequest(Entry& entry)
    {
        if (entry.loadingInfo)
        {
            entry.loadingInfo->RemoveRef();
        }
        entry.loadingInfo = nullptr;
        if (entry.loadingId >= 0)
        {
            const auto request = requests.find(entry.loadingId);
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
        if (entry.vehicle)
        {
            owners.erase(entry.vehicle);
            if (Alive(entry))
            {
                CWorld::Remove(entry.vehicle);
                delete entry.vehicle;
            }
        }
        ReleaseRequest(entry);
        entry = {};
    }

    void Place(Entry& entry, const CVector& position, float yaw)
    {
        auto* vehicle = entry.vehicle;
        CWorld::Remove(vehicle);
        vehicle->SetPosn(position);
        vehicle->SetOrientation(0.0f, 0.0f, D3DXToRadian(yaw));
        vehicle->m_vecMoveSpeed = {};
        vehicle->m_vecTurnSpeed = {};
        vehicle->UpdateRwMatrix();
        vehicle->UpdateRwFrame();
        CWorld::Add(vehicle);
        entry.position = position;
        entry.yaw = yaw;
    }
};

VehicleRuntime::VehicleRuntime() : impl(std::make_unique<Impl>())
{
}

VehicleRuntime::~VehicleRuntime()
{
    Clear();
}

void VehicleRuntime::Clear()
{
    for (auto& [id, entry] : impl->entries)
    {
        impl->Destroy(entry);
    }
    impl->entries.clear();
    impl->owners.clear();
    impl->previewCapacity = 0;
}

int VehicleRuntime::LookupItemIndex(CEntity* entity) const
{
    return EntityRuntime::LookupItemIndex(*impl, entity, g_scene);
}

CVehicle* VehicleRuntime::GetVehicle(int itemIndex) const
{
    if (!g_scene || itemIndex < 0 || itemIndex >= g_scene->count)
    {
        return nullptr;
    }
    const auto it = impl->entries.find(g_scene->items[itemIndex].id);
    return it != impl->entries.end() && impl->Alive(it->second) ? it->second.vehicle : nullptr;
}

int LookupEditorItemIndex(CEntity* entity)
{
    if (entity && entity->m_nType == ENTITY_TYPE_PED)
    {
        return g_actorRuntime ? g_actorRuntime->LookupItemIndex(entity) : -1;
    }
    if (entity && entity->m_nType == ENTITY_TYPE_VEHICLE)
    {
        return g_vehicleRuntime ? g_vehicleRuntime->LookupItemIndex(entity) : -1;
    }
    if (g_pickupRuntime)
    {
        const int index = g_pickupRuntime->LookupItemIndex(entity);
        if (index >= 0)
        {
            return index;
        }
    }
    return g_objectRuntime ? g_objectRuntime->LookupItemIndex(entity) : -1;
}

int VehicleRuntime::PreviewCapacity() const
{
    return impl->previewCapacity;
}

void VehicleRuntime::Update()
{
    if (!g_scene || !CPools::ms_pVehiclePool)
    {
        return;
    }
    const bool editing = g_freeCamera && g_freeCamera->m_active;
    const auto now = GetTickCount64();
    const CVector camera = editing ? g_freeCamera->m_position : TheCamera.GetPosition();
    int spawned = 0;
    for (auto& [id, entry] : impl->entries)
    {
        entry.seen = false;
    }

    impl->preview.Begin(*g_scene);
    for (int i = 0; i < g_scene->count; ++i)
    {
        const auto& data = g_scene->data[i];
        if (!data.vehicle.HasValue() || !VehicleCatalog::IsValid(data.vehicle.Get().modelId))
        {
            continue;
        }
        const int id = g_scene->items[i].id;
        const auto existing = impl->entries.find(id);
        if (existing != impl->entries.end())
        {
            auto& entry = existing->second;
            const int modelId = data.vehicle.Get().modelId;
            if ((entry.vehicle && (!impl->Alive(entry) || entry.modelId != modelId))
                || (entry.loadingId >= 0 && entry.loadingId != modelId))
            {
                impl->Destroy(entry);
            }
            entry.index = i;
            entry.seen = true;
            if (entry.vehicle)
            {
                ++spawned;
            }
            if (now < entry.retryAtMs)
            {
                continue;
            }
        }
        const bool retained = existing != impl->entries.end() && (existing->second.vehicle || existing->second.loadingId >= 0);
        const CVector delta = g_scene->world[i].pos - camera;
        const float distanceSquared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        const float range = retained ? 375.0f : 350.0f;
        if (distanceSquared < range * range)
        {
            impl->preview.Add(i, id, distanceSquared, retained);
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
    impl->previewCapacity = StreamingBudget::Capacity(StreamingBudget::Vehicles, spawned,
        static_cast<int>(CPools::ms_pVehiclePool->GetNoOfFreeSpaces()), StreamingBudget::VehiclePoolReserve);
    impl->preview.Select(impl->previewCapacity);

    int loadingInFlight = 0;
    for (auto it = impl->entries.begin(); it != impl->entries.end();)
    {
        if (!impl->preview.Contains(it->second.index) && now >= it->second.retryAtMs)
        {
            impl->Destroy(it->second);
            it = impl->entries.erase(it);
        }
        else
        {
            if (it->second.loadingId >= 0)
            {
                ++loadingInFlight;
            }
            ++it;
        }
    }

    int requestBudget = 4;
    int spawnBudget = 2;
    for (int candidate = 0; candidate < impl->preview.Count(); ++candidate)
    {
        const int i = impl->preview.Index(candidate);
        const int id = g_scene->items[i].id;
        const auto& data = g_scene->data[i];
        const auto& world = g_scene->world[i];
        auto& entry = impl->entries[id];
        entry.index = i;
        if (!entry.vehicle)
        {
            const int modelId = data.vehicle.Get().modelId;
            if (entry.loadingId < 0)
            {
                if (!requestBudget || loadingInFlight >= 8)
                {
                    continue;
                }
                --requestBudget;
                ++loadingInFlight;
                entry.loadingId = modelId;
                auto& request = impl->requests[modelId];
                if (request.users++ == 0 && !plugin::Command<plugin::Commands::HAS_MODEL_LOADED>(modelId))
                {
                    request.owned = !(CStreaming::ms_aInfoForModel[modelId].m_nFlags & GAME_REQUIRED);
                    CStreaming::RequestModel(modelId, GAME_REQUIRED);
                }
            }
            if (!spawnBudget || !plugin::Command<plugin::Commands::HAS_MODEL_LOADED>(modelId)
                || CPools::ms_pVehiclePool->GetNoOfFreeSpaces() <= StreamingBudget::VehiclePoolReserve)
            {
                continue;
            }
            --spawnBudget;
            entry.loadingInfo = CModelInfo::GetModelInfo(modelId);
            if (!entry.loadingInfo)
            {
                continue;
            }
            entry.loadingInfo->AddRef();
            auto* vehicle = VehicleInstance::Create(modelId);
            if (!vehicle)
            {
                impl->ReleaseRequest(entry);
                --loadingInFlight;
                entry.retryAtMs = now + 2000;
                continue;
            }
            entry.vehicle = vehicle;
            entry.handle = CPools::ms_pVehiclePool->GetRef(vehicle);
            entry.modelId = modelId;
            vehicle->m_nStatus = STATUS_PHYSICS;
            vehicle->m_nAreaCode = CGame::currArea;
            auto* info = static_cast<CVehicleModelInfo*>(entry.loadingInfo);
            unsigned char tertiary = 1, quaternary = 1;
            info->ChooseVehicleColour(entry.randomColors[0], entry.randomColors[1], tertiary, quaternary, 1);
            impl->owners[vehicle] = id;
            impl->Place(entry, world.pos, world.eulerDeg[2]);
            entry.freezeUntil = editing ? now + 1000 : 0;
            impl->ReleaseRequest(entry);
            --loadingInFlight;
        }
        auto* vehicle = entry.vehicle;
        vehicle->m_eDoorLock = DOORLOCK_LOCKED;
        vehicle->m_nPrimaryColor
            = data.vehicle.Get().colors[0] < 0 ? entry.randomColors[0] : static_cast<unsigned char>(data.vehicle.Get().colors[0]);
        vehicle->m_nSecondaryColor
            = data.vehicle.Get().colors[1] < 0 ? entry.randomColors[1] : static_cast<unsigned char>(data.vehicle.Get().colors[1]);
        const bool moved = (world.pos - entry.position).Magnitude() > 0.00001f || world.eulerDeg[2] != entry.yaw;
        const bool dragging = editing
            && ((g_gizmos && g_gizmos->IsDragging() && (g_scene->items[i].selected || g_scene->HasSelectedAncestor(i)))
                || GuiIsTransformEditing(i));
        if (moved || dragging)
        {
            impl->Place(entry, world.pos, world.eulerDeg[2]);
            entry.freezeUntil = editing ? now + 1000 : 0;
        }
        if (!editing)
        {
            entry.freezeUntil = 0;
        }
        EntityRuntime::Freeze(entry, entry.vehicle, editing && now < entry.freezeUntil);
        if (entry.frozen && !moved && !dragging)
        {
            impl->Place(entry, world.pos, world.eulerDeg[2]);
        }
        const float metadataRadius = VehicleBrowser::GetModel(data.vehicle.Get().modelId).radius;
        const float radius = std::max(0.1f, metadataRadius > 0 ? metadataRadius : vehicle->GetBoundRadius());
        if (editing && (vehicle->GetPosition() - world.pos).Magnitude() > radius)
        {
            impl->Place(entry, world.pos, world.eulerDeg[2]);
        }
    }
}
