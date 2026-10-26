// Source Repository: https://github.com/waverso/waversomapstudio

bool ObjectRuntime::SpawnNow(int idx)
{
    if (!CPools::ms_pObjectPool || CPools::ms_pObjectPool->GetNoOfFreeSpaces() <= StreamingBudget::ObjectPoolReserve)
    {
        return false;
    }

    const ItemData& d = g_scene->data[idx];
    const EditorScene::WorldTransform& w = g_scene->world[idx];
    ObjectRuntimeState& rt = m_itemStates[idx];
    const int modelId = d.model.Get().modelId;

    int objHandle = 0;
    m_spawning = true;
    plugin::Command<plugin::Commands::CREATE_OBJECT>(modelId, w.pos.x, w.pos.y, w.pos.z, &objHandle);
    ReleaseModelRequest(idx);

    CObject* pObj = CPools::GetObject(objHandle);
    if (pObj == nullptr)
    {
        m_spawning = false;
        rt.state = ObjectStreamState::Unspawned;
        rt.retryAtMs = GetTickCount64() + SpawnRetryMs;
        return false;
    }

    plugin::Command<plugin::Commands::SET_OBJECT_COORDINATES>(objHandle, w.pos.x, w.pos.y, w.pos.z);
    SetObjectRotation(objHandle, modelId, w.eulerDeg);

    ModelChecks::HideGunflash(pObj->m_pRwObject);

    if (IsBreakableModel((unsigned short)modelId))
    {
        pObj->bInvulnerable = true;
        pObj->m_nColDamageEffect = 0;
        pObj->bDisableCollisionForce = pObj->bDisableTurnForce = 1;
    }
    else
    {
        pObj->bDisableTurnForce = 0;
    }

    const uintptr_t off = (uintptr_t)pObj - m_poolBase;
    const int slot = (off < m_poolSpan) ? (int)(off / m_poolStride) : -1;
    if (slot >= 0)
    {
        m_slotToItem[slot] = idx;
        m_slotOwner[slot] = pObj;
    }
    m_spawning = false;

    rt.state = ObjectStreamState::Spawned;
    rt.obj = pObj;
    rt.scriptHandle = objHandle;
    rt.poolSlot = slot;
    rt.spawnedModelId = modelId;
    rt.loadingModelId = -1;
    rt.boundRadius = pObj->GetBoundRadius();
    rt.lastPos[0] = w.pos.x;
    rt.lastPos[1] = w.pos.y;
    rt.lastPos[2] = w.pos.z;
    memcpy(rt.lastRot, w.eulerDeg, sizeof(rt.lastRot));
    return true;
}

bool ObjectRuntime::RequestModel(int idx, int modelId)
{
    ModelRequest* available = nullptr;
    for (auto& request : m_modelRequests)
    {
        if (request.modelId == modelId)
        {
            ++request.users;
            m_itemStates[idx].loadingModelId = modelId;
            return true;
        }
        if (!request.users)
        {
            available = &request;
        }
    }
    if (!available)
    {
        return false;
    }
    available->modelId = modelId;
    available->users = 1;
    available->owned = !(CStreaming::ms_aInfoForModel[modelId].m_nFlags & GAME_REQUIRED);
    plugin::Command<plugin::Commands::REQUEST_MODEL>(modelId);
    m_itemStates[idx].loadingModelId = modelId;
    return true;
}

void ObjectRuntime::ReleaseModelRequest(int idx)
{
    const int modelId = m_itemStates[idx].loadingModelId;
    if (modelId < 0)
    {
        return;
    }
    m_itemStates[idx].loadingModelId = -1;
    for (auto& request : m_modelRequests)
    {
        if (request.modelId != modelId)
        {
            continue;
        }
        if (--request.users == 0)
        {
            if (request.owned)
            {
                plugin::Command<plugin::Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(modelId);
            }
            request = {};
        }
        return;
    }
}

void ObjectRuntime::DespawnItem(int idx)
{
    ObjectRuntimeState& rt = m_itemStates[idx];
    if (rt.state == ObjectStreamState::Spawned && rt.scriptHandle >= 0)
    {
        ClearSlot(rt.poolSlot);
        m_despawning = true;
        if (CPools::ms_pObjectPool && CPools::ms_pObjectPool->GetAtRef(rt.scriptHandle) == rt.obj)
        {
            plugin::Command<plugin::Commands::DELETE_OBJECT>(rt.scriptHandle);
        }
        m_despawning = false;
    }
    ReleaseModelRequest(idx);
    if (rt.materialsActive)
    {
        ReleaseItemTextures(idx);
        ReleaseItemMaterialTexts(idx);
        rt.materialsActive = false;
    }
    rt.state = ObjectStreamState::Unspawned;
    rt.obj = nullptr;
    rt.scriptHandle = -1;
    rt.poolSlot = -1;
    rt.spawnedModelId = -1;
    rt.loadingModelId = -1;
    rt.slide.playing = false;
    rt.escalatorDirty = false;
}

void ObjectRuntime::OnEntityRemove(CEntity* ent)
{
    if (m_despawning)
    {
        return;
    }
    const int idx = LookupItemIndex(ent);
    if (idx < 0)
    {
        return;
    }

    ObjectRuntimeState& rt = m_itemStates[idx];
    ClearSlot(rt.poolSlot);
    rt.state = ObjectStreamState::Unspawned;
    rt.obj = nullptr;
    rt.scriptHandle = -1;
    rt.poolSlot = -1;
    rt.spawnedModelId = -1;
    rt.loadingModelId = -1;
}
