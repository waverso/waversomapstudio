// Source Repository: https://github.com/waverso/waversomapstudio

void DestroyPreviewInstanceNow()
{
    if (g_preview.vehicle)
    {
        delete g_preview.vehicle;
        g_preview.vehicle = nullptr;
        g_preview.instance = nullptr;
        return;
    }
    if (!g_preview.instance)
    {
        return;
    }
    const RwUInt8 type = RwObjectGetType(g_preview.instance);
    if (type == rpATOMIC)
    {
        RpAtomic* atomic = reinterpret_cast<RpAtomic*>(g_preview.instance);
        RwFrame* frame = RpAtomicGetFrame(atomic);
        RpAtomicSetFrame(atomic, nullptr);
        RpAtomicDestroy(atomic);
        if (frame)
        {
            RwFrameDestroy(frame);
        }
    }
    else if (type == rpCLUMP)
    {
        RpClumpDestroy(reinterpret_cast<RpClump*>(g_preview.instance));
    }
    g_preview.instance = nullptr;
}

void ReleasePreviewNow(bool releaseTarget = true)
{
    if (releaseTarget)
    {
        DestroyPreviewTargetNow();
    }
    DestroyPreviewFxSystems();
    DestroyPreviewInstanceNow();
    CBaseModelInfo* information = g_preview.referencedInfo;
    if (g_preview.modelRefAdded && g_preview.referencedInfo)
    {
        g_preview.referencedInfo->RemoveRef();
    }
    g_preview.referencedInfo = nullptr;
    g_preview.modelRefAdded = false;

    if (g_preview.ownsModelRequest && CStreaming::ms_aInfoForModel && g_preview.modelId >= 0)
    {
        constexpr std::uint8_t protectedFlags
            = eStreamingFlags::GAME_REQUIRED | eStreamingFlags::MISSION_REQUIRED | eStreamingFlags::KEEP_IN_MEMORY;
        CStreamingInfo& model = CStreaming::ms_aInfoForModel[g_preview.modelId];
        if (g_preview.keepsModelInMemory && !(g_preview.modelInitialFlags & eStreamingFlags::KEEP_IN_MEMORY))
        {
            model.m_nFlags &= ~static_cast<std::uint8_t>(eStreamingFlags::KEEP_IN_MEMORY);
        }
        if ((!information || information->m_nRefCount == 0) && (model.m_nFlags & protectedFlags) == 0)
        {
            CStreaming::RemoveModel(g_preview.modelId);
            CStreaming::ms_aInfoForModel[g_preview.modelId].m_nFlags = g_preview.modelInitialFlags;
        }
        if (g_preview.txdSlot >= 0)
        {
            CStreamingInfo& txd = CStreaming::ms_aInfoForModel[20000 + g_preview.txdSlot];
            if (g_preview.keepsModelInMemory && !(g_preview.txdInitialFlags & eStreamingFlags::KEEP_IN_MEMORY))
            {
                txd.m_nFlags &= ~static_cast<std::uint8_t>(eStreamingFlags::KEEP_IN_MEMORY);
            }
            if (g_preview.txdInitialState == LOADSTATE_NOT_LOADED && CTxdStore::GetNumRefs(g_preview.txdSlot) == 0
                && (txd.m_nFlags & protectedFlags) == 0)
            {
                CStreaming::RemoveModel(20000 + g_preview.txdSlot);
                CStreaming::ms_aInfoForModel[20000 + g_preview.txdSlot].m_nFlags = g_preview.txdInitialFlags;
            }
        }
    }
    g_preview.published = false;
}

void BeginPreviewSelection(int modelId)
{
    g_preview.modelId = modelId;
    g_preview.requestSent = false;
    g_preview.state = PreviewState::Loading;
    {
        const ObjectBrowser::ModelInfo info = ObjectBrowser::GetModel(modelId);
        strncpy_s(g_preview.modelName, sizeof(g_preview.modelName),
            g_preview.skinModel                    ? SkinCatalog::Get(modelId)->modelName
                : VehicleCatalog::IsValid(modelId) ? VehicleCatalog::Get(modelId)->modelName
                                                   : (info.name ? info.name : ""),
            _TRUNCATE);
    }
    if (CStreaming::ms_aInfoForModel)
    {
        CStreamingInfo& streaming = CStreaming::ms_aInfoForModel[modelId];
        g_preview.modelInitialState = streaming.m_nLoadState;
        g_preview.modelInitialFlags = streaming.m_nFlags;
        g_preview.ownsModelRequest = streaming.m_nLoadState == LOADSTATE_NOT_LOADED;
    }
    if (CModelInfo::ms_modelInfoPtrs && modelId < CModelInfo::ms_modelInfoCount)
    {
        CBaseModelInfo* information = CModelInfo::GetModelInfo(modelId);
        if (information)
        {
            if (CTxdStore::ms_pTxdPool && information->m_nTxdIndex >= 0
                && information->m_nTxdIndex < CTxdStore::ms_pTxdPool->m_nSize)
            {
                g_preview.txdSlot = information->m_nTxdIndex;
                if (CStreaming::ms_aInfoForModel)
                {
                    CStreamingInfo& txd = CStreaming::ms_aInfoForModel[20000 + g_preview.txdSlot];
                    g_preview.txdInitialState = txd.m_nLoadState;
                    g_preview.txdInitialFlags = txd.m_nFlags;
                }
            }
        }
    }
    const bool generatedProfile = ModelPreview::GetProfile(modelId, g_preview.initial)
        && std::isfinite(g_preview.initial.distance) && g_preview.initial.distance > 0.01f;
    if (!generatedProfile)
    {
        const ObjectBrowser::ModelInfo model = ObjectBrowser::GetModel(modelId);
        g_preview.initial.target[0] = (model.boundsMin[0] + model.boundsMax[0]) * 0.5f;
        g_preview.initial.target[1] = (model.boundsMin[1] + model.boundsMax[1]) * 0.5f;
        g_preview.initial.target[2] = (model.boundsMin[2] + model.boundsMax[2]) * 0.5f;
        g_preview.initial.yaw = 135.0f;
        g_preview.initial.pitch = 22.0f;
        const float extent = std::max({ std::fabs(model.boundsMax[0] - model.boundsMin[0]),
            std::fabs(model.boundsMax[1] - model.boundsMin[1]), std::fabs(model.boundsMax[2] - model.boundsMin[2]), 0.1f });
        g_preview.initial.distance = extent * 1.5f;
    }
    if (VehicleCatalog::IsValid(modelId))
    {
        g_preview.initial.yaw = VehicleCatalog::PreviewYaw;
        g_preview.initial.pitch = VehicleCatalog::PreviewPitch;
        if (const auto* stats = CatalogDatabase::ObjectStats(static_cast<std::uint16_t>(modelId));
            !stats || !(stats->flags & CatalogDatabase::ObjectStatsIndexed))
        {
            if (auto* info = CModelInfo::GetModelInfo(modelId); info && info->m_pColModel)
            {
                const auto& box = info->m_pColModel->m_boundBox;
                g_preview.initial.target[0] = (box.m_vecMin.x + box.m_vecMax.x) * 0.5f;
                g_preview.initial.target[1] = (box.m_vecMin.y + box.m_vecMax.y) * 0.5f;
                g_preview.initial.target[2] = (box.m_vecMin.z + box.m_vecMax.z) * 0.5f;
                g_preview.initial.distance = std::max(1.0f, info->m_pColModel->m_boundSphere.m_fRadius * 3.0f);
            }
        }
    }
    if (g_preview.skinModel)
    {
        g_preview.initial.yaw = SkinCatalog::PreviewYaw;
        g_preview.initial.pitch = SkinCatalog::PreviewPitch;
        g_preview.initial.distance = SkinCatalog::PreviewDistance;
        if (!generatedProfile)
        {
            g_preview.initial.target[2] = 0.0f;
        }
    }
    g_preview.current = g_preview.goal = g_preview.initial;
    g_preview.dirty = true;
}

void ApplyDesiredSelectionNow()
{
    if (g_preview.desiredModelId == g_preview.modelId && g_preview.desiredSkinModel == g_preview.skinModel)
    {
        return;
    }
    const int desired = g_preview.desiredModelId;
    const bool skin = g_preview.desiredSkinModel;
    const PreviewTarget target = desired >= 0 ? g_preview.target : PreviewTarget {};
    ReleasePreviewNow(desired < 0);
    g_preview = {};
    g_preview.target = target;
    g_preview.desiredModelId = desired;
    g_preview.desiredSkinModel = g_preview.skinModel = skin;
    if (desired >= 0)
    {
        BeginPreviewSelection(desired);
    }
}

void FailPreview()
{
    g_preview.state = PreviewState::Failed;
    g_preview.published = false;
    g_preview.dirty = false;
}

RpMaterial* CountUVAnimMaterial(RpMaterial* material, void* data)
{
    if (RpMaterialUVAnimExists(material))
    {
        *static_cast<bool*>(data) = true;
    }
    return material;
}

RpAtomic* AtomicHasUVAnim(RpAtomic* atomic, void* data)
{
    if (RpGeometry* geometry = RpAtomicGetGeometry(atomic))
    {
        RpGeometryForAllMaterials(geometry, CountUVAnimMaterial, data);
    }
    return atomic;
}

bool InstanceHasUVAnim(RwObject* object)
{
    if (!object)
    {
        return false;
    }
    bool has = false;
    const RwUInt8 type = RwObjectGetType(object);
    if (type == rpATOMIC)
    {
        AtomicHasUVAnim(reinterpret_cast<RpAtomic*>(object), &has);
    }
    else if (type == rpCLUMP)
    {
        RpClumpForAllAtomics(reinterpret_cast<RpClump*>(object), AtomicHasUVAnim, &has);
    }
    return has;
}

RpAtomic* AdvanceUVAnimAtomic(RpAtomic* atomic, void*)
{
    if (RpGeometry* geometry = RpAtomicGetGeometry(atomic))
    {
        RpGeometryForAllMaterials(geometry, reinterpret_cast<RpMaterialCallBack>(0x532D70), nullptr);
    }
    return atomic;
}

void AdvanceInstanceUVAnim(RwObject* object)
{
    if (!object)
    {
        return;
    }
    const RwUInt8 type = RwObjectGetType(object);
    if (type == rpATOMIC)
    {
        AdvanceUVAnimAtomic(reinterpret_cast<RpAtomic*>(object), nullptr);
    }
    else if (type == rpCLUMP)
    {
        RpClumpForAllAtomics(reinterpret_cast<RpClump*>(object), AdvanceUVAnimAtomic, nullptr);
    }
}

bool AcquirePreviewInstance()
{
    if (g_preview.instance)
    {
        return true;
    }
    if (!CModelInfo::ms_modelInfoPtrs || g_preview.modelId >= CModelInfo::ms_modelInfoCount)
    {
        return false;
    }
    CBaseModelInfo* information = g_preview.modelId >= 0 ? CModelInfo::GetModelInfo(g_preview.modelId) : nullptr;
    if (!information || !information->m_pRwObject)
    {
        return false;
    }

    information->AddRef();
    RwObject* instance = nullptr;
    if (g_preview.skinModel)
    {
        instance = reinterpret_cast<RwObject*>(SkinInstance::Create(g_preview.modelId));
    }
    else if (VehicleCatalog::IsValid(g_preview.modelId))
    {
        g_preview.vehicle = VehicleInstance::Create(g_preview.modelId);
        if (g_preview.vehicle)
        {
            instance = g_preview.vehicle->m_pRwObject;
        }
    }
    else if (information->GetRwModelType() == rpATOMIC)
    {
        RpAtomic* atomic = RpAtomicClone(information->m_pRwAtomic);
        if (atomic)
        {
            RwFrame* frame = RwFrameCreate();
            if (frame)
            {
                RpAtomicSetFrame(atomic, frame);
            }
            else
            {
                RpAtomicDestroy(atomic);
                atomic = nullptr;
            }
        }
        instance = reinterpret_cast<RwObject*>(atomic);
    }
    else if (information->GetRwModelType() == rpCLUMP)
    {
        instance = information->CreateInstance();
    }

    if (!instance)
    {
        information->RemoveRef();
        return false;
    }
    g_preview.referencedInfo = information;
    g_preview.modelRefAdded = true;
    g_preview.instance = instance;
    if (g_preview.skinModel)
    {
        const auto bounds = SkinInstance::Measure(reinterpret_cast<RpClump*>(instance));
        if (bounds.valid)
        {
            const float low[] = { bounds.minimum.x, bounds.minimum.y, bounds.minimum.z };
            const float high[] = { bounds.maximum.x, bounds.maximum.y, bounds.maximum.z };
            for (int axis = 0; axis < 3; ++axis)
            {
                g_preview.initial.target[axis] = (low[axis] + high[axis]) * 0.5f;
            }
            g_preview.initial.distance = SkinCatalog::PreviewDistance;
            g_preview.current = g_preview.goal = g_preview.initial;
        }
    }
    ModelChecks::HideGunflash(instance);
    if (g_preview.vehicle)
    {
        ModelChecks::HideVehicleLod(instance);
    }
    if (RwObjectGetType(instance) == rpCLUMP)
    {
        g_preview.animated = RpAnimBlendClumpGetFirstAssociation(reinterpret_cast<RpClump*>(instance)) != nullptr;
    }
    if (g_preview.vehicle)
    {
        if (const auto* stats = CatalogDatabase::ObjectStats(static_cast<std::uint16_t>(g_preview.modelId)))
        {
            g_preview.vehicleAnimation = (stats->behaviourFlags & CatalogDatabase::ObjectBehaviourAnimation) != 0;
        }
    }
    g_preview.uvAnim = InstanceHasUVAnim(instance);
    RefreshPreviewEffectState();
    return true;
}

void UpdatePreviewSkinHierarchy(RpClump* clump)
{
    if (g_preview.skinModel)
    {
        SkinInstance::UpdateHierarchy(clump, g_preview.modelId);
        return;
    }
    if (clump)
    {
        if (RpHAnimHierarchy* hierarchy = GetAnimHierarchyFromSkinClump(clump))
        {
            RpHAnimHierarchyUpdateMatrices(hierarchy);
        }
    }
}
