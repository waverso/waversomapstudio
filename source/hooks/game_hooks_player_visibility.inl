// Source Repository: https://github.com/waverso/waversomapstudio

namespace PlayerVisibility
{
    static CEntity* AttachmentParent(CEntity* entity)
    {
        if (!entity
            || (entity->m_nType != ENTITY_TYPE_OBJECT && entity->m_nType != ENTITY_TYPE_PED
                && entity->m_nType != ENTITY_TYPE_VEHICLE))
        {
            return nullptr;
        }
        return static_cast<CPhysical*>(entity)->m_pAttachedTo;
    }

    static bool HidesEntity(CEntity* entity, CPed* player, CVehicle* vehicle)
    {
        auto* fast = entity;
        CEntity* cycle = nullptr;
        while (entity)
        {
            if (entity == player || entity == vehicle
                || (entity->m_nType == ENTITY_TYPE_OBJECT && g_sampIntegration
                    && g_sampIntegration->IsLocalPlayerAccessory(entity)))
            {
                return true;
            }
            entity = AttachmentParent(entity);
            if (cycle && entity == cycle)
            {
                break;
            }
            if (!cycle)
            {
                fast = AttachmentParent(AttachmentParent(fast));
                if (entity && entity == fast)
                {
                    cycle = entity;
                }
            }
        }
        return false;
    }

    static RwMatrix* EntityMatrix(CEntity* entity)
    {
        if (!entity || !entity->m_pRwObject)
        {
            return nullptr;
        }
        auto* frame = static_cast<RwFrame*>(entity->m_pRwObject->parent);
        return frame ? RwFrameGetMatrix(frame) : nullptr;
    }

    static const std::vector<FxSystem_c*>& HiddenEffects(FxManager_c& manager)
    {
        static thread_local std::vector<FxSystem_c*> hidden;
        hidden.clear();
        auto add = [&](FxSystem_c* system)
        {
            if (system && std::find(hidden.begin(), hidden.end(), system) == hidden.end())
            {
                hidden.push_back(system);
            }
        };
        auto* player = FindPlayerPed();
        auto* vehicle = FindPlayerVehicle(-1);
        if (player)
        {
            if (player->m_pFire)
            {
                add(player->m_pFire->m_pFxSystem);
            }
            for (const auto& weapon : player->m_aWeapons)
            {
                add(weapon.m_pFxSystem);
            }
            if (player->m_pIntelligence)
            {
                if (auto* jetpack = player->m_pIntelligence->GetTaskJetPack())
                {
                    add(jetpack->m_pFxSysL);
                    add(jetpack->m_pFxSysR);
                }
            }
        }
        if (vehicle)
        {
            add(vehicle->m_pOverheatParticle);
            add(vehicle->m_pFireParticle);
            add(vehicle->m_pDustParticle);
            if (vehicle->m_pFire)
            {
                add(vehicle->m_pFire->m_pFxSystem);
            }
        }
        const auto* playerMatrix = EntityMatrix(player);
        const auto* vehicleMatrix = EntityMatrix(vehicle);
        for (auto* system = manager.m_fxSystemList.GetHead(); system; system = manager.m_fxSystemList.GetNext(system))
        {
            if (system->m_pParentMatrix && (system->m_pParentMatrix == playerMatrix || system->m_pParentMatrix == vehicleMatrix))
            {
                add(system);
            }
        }
        for (auto* entry = g_fx.m_entityFxList.GetHead(); entry; entry = g_fx.m_entityFxList.GetNext(entry))
        {
            const auto address = reinterpret_cast<std::uintptr_t>(entry);
            auto* entity = *reinterpret_cast<CEntity**>(address + 0x0C);
            if (HidesEntity(entity, player, vehicle))
            {
                add(*reinterpret_cast<FxSystem_c**>(address + 0x08));
            }
        }
        return hidden;
    }
}

static void(__thiscall* g_originalFxRender)(FxManager_c*, RwCamera*, unsigned char)
    = reinterpret_cast<void(__thiscall*)(FxManager_c*, RwCamera*, unsigned char)>(0x4A92A0);

static void __fastcall EditorFxRender(FxManager_c* manager, void*, RwCamera* camera, unsigned char heatHaze)
{
    if (g_freeCamera && g_freeCamera->m_active)
    {
        const auto& hidden = PlayerVisibility::HiddenEffects(*manager);
        if (!hidden.empty())
        {
            PlayerVisibility::ParticleRenderFilter filter(*manager, hidden);
            g_originalFxRender(manager, camera, heatHaze);
            return;
        }
    }
    g_originalFxRender(manager, camera, heatHaze);
}

static void(__thiscall* g_originalJetPackRender)(CTaskSimpleJetPack*, CPed*)
    = reinterpret_cast<void(__thiscall*)(CTaskSimpleJetPack*, CPed*)>(0x67F6A0);

static void __fastcall EditorJetPackRender(CTaskSimpleJetPack* task, void*, CPed* ped)
{
    if (g_freeCamera && g_freeCamera->m_active && ped == FindPlayerPed())
    {
        return;
    }
    g_originalJetPackRender(task, ped);
}
