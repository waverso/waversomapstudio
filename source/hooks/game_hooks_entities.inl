// Source Repository: https://github.com/waverso/waversomapstudio

static bool(__thiscall* g_originalPickupUpdate)(CPickup*, CPlayerPed*, CVehicle*, int)
    = reinterpret_cast<bool(__thiscall*)(CPickup*, CPlayerPed*, CVehicle*, int)>(0x457410);

static bool __fastcall EditorPickupUpdate(CPickup* pickup, void*, CPlayerPed* player, CVehicle* vehicle, int playerId)
{
    if (g_pickupRuntime && g_pickupRuntime->OwnsPickup(pickup))
    {
        return false;
    }
    return g_originalPickupUpdate(pickup, player, vehicle, playerId);
}

static bool(__cdecl* g_originalVehicleHealthy)(const CVehicle*) = reinterpret_cast<bool(__cdecl*)(const CVehicle*)>(0x64EEC0);

static bool __cdecl EditorVehicleHealthy(const CVehicle* vehicle)
{
    if (g_vehicleRuntime && g_vehicleRuntime->LookupItemIndex(const_cast<CVehicle*>(vehicle)) >= 0)
    {
        return false;
    }
    return g_originalVehicleHealthy(vehicle);
}

static void FanOutEntityGone(CEntity* pEntity, bool bDestroyed)
{
    if (g_scenePicker)
    {
        g_scenePicker->OnEntityRemove(pEntity);
    }
    if (g_removalManager)
    {
        g_removalManager->OnEntityRemove(pEntity);
    }
    if (g_objectRuntime)
    {
        g_objectRuntime->OnEntityRemove(pEntity);
    }
    if (g_sampIntegration)
    {
        g_sampIntegration->OnEntityRemove(pEntity);
    }
    if (g_outlineRenderer)
    {
        g_outlineRenderer->OnEntityRemove(pEntity);
    }
    if (g_collisionRenderer)
    {
        g_collisionRenderer->OnEntityRemove(pEntity);
    }
    if (bDestroyed && g_scenePicker)
    {
        g_scenePicker->OnEntityDestroy(pEntity);
    }

    if (pEntity->m_pLod)
    {
        auto it = g_gameHooks->m_lodHighMap.find(pEntity->m_pLod);
        if (it != g_gameHooks->m_lodHighMap.end())
        {
            auto& vec = it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), pEntity), vec.end());
            if (vec.empty())
            {
                g_gameHooks->m_lodHighMap.erase(it);
            }
        }
    }

    auto it = g_gameHooks->m_lodHighMap.find(pEntity);
    if (it != g_gameHooks->m_lodHighMap.end())
    {
        g_gameHooks->m_lodHighMap.erase(it);
    }
}

void __fastcall GameHooks::HookEntityRemove(CEntity* pEntity, void* edx)
{
    if (pEntity)
    {
        FanOutEntityGone(pEntity, false);
    }
    g_gameHooks->m_originalEntityRemove(pEntity);
}

void __fastcall GameHooks::HookEntityDtor(CEntity* pEntity, void* edx)
{
    if (pEntity)
    {
        FanOutEntityGone(pEntity, true);
    }
    g_gameHooks->m_originalEntityDtor(pEntity);
}

void __cdecl GameHooks::HookWorldAddEntity(CEntity* entity)
{
    g_gameHooks->m_originalWorldAddEntity(entity);
    if (entity)
    {
        if (entity->m_nType == ENTITY_TYPE_OBJECT || entity->m_nType == ENTITY_TYPE_DUMMY)
        {
            if (g_scenePicker)
            {
                g_scenePicker->OnEntityAdd(entity);
            }
            if (g_sampIntegration && entity->m_nType == ENTITY_TYPE_OBJECT)
            {
                g_sampIntegration->OnObjectAdded((CObject*)entity);
            }
            if (g_removalManager && entity->m_nType == ENTITY_TYPE_OBJECT)
            {
                g_removalManager->OnEntityAdd(entity);
            }
        }
    }
}

CLink<CEntity*>* __cdecl GameHooks::HookStreamingAddEntity(CEntity* entity)
{
    if (entity)
    {
        if (entity->m_nType == ENTITY_TYPE_OBJECT || entity->m_nType == ENTITY_TYPE_DUMMY
            || entity->m_nType == ENTITY_TYPE_BUILDING)
        {
            if (entity->m_nType == ENTITY_TYPE_BUILDING)
            {
                if (entity->m_pLod)
                {
                    auto& vec = g_gameHooks->m_lodHighMap[entity->m_pLod];
                    if (std::find(vec.begin(), vec.end(), entity) == vec.end())
                    {
                        vec.push_back(entity);
                    }
                }
                if (g_scenePicker)
                {
                    g_scenePicker->OnEntityAdd(entity);
                }
            }
            else if (entity->m_nType == ENTITY_TYPE_DUMMY)
            {
                if (g_scenePicker)
                {
                    g_scenePicker->OnEntityAdd(entity);
                }
            }
            if (g_removalManager)
            {
                g_removalManager->OnEntityAdd(entity);
            }
        }
    }
    return g_gameHooks->m_originalStreamingAddEntity(entity);
}

void __fastcall GameHooks::HookProcessControlLogic(CObject* obj, void* edx)
{
    if (g_removalManager)
    {
        g_removalManager->OnControlLogicProcess(obj);
    }

    if (Selection::IsEntitySelected(obj))
    {
        return;
    }

    g_gameHooks->m_originalProcessControlLogic(obj);
}
