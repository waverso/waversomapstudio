// Source Repository: https://github.com/waverso/waversomapstudio

static bool(__thiscall* g_originalFxSystemUpdate)(FxSystem_c*, RwCamera*, float)
    = reinterpret_cast<bool(__thiscall*)(FxSystem_c*, RwCamera*, float)>(0x4AAF70);

static bool __fastcall EditorFxSystemUpdate(FxSystem_c* system, void*, RwCamera* camera, float timeDelta)
{
    return ModelPreview::ShouldUpdateFx(system, camera) && g_originalFxSystemUpdate(system, camera, timeDelta);
}

static void(__cdecl* g_originalDrawRadarMap)() = reinterpret_cast<void(__cdecl*)()>(0x586880);
static void(__cdecl* g_originalDrawRadarBlips)() = reinterpret_cast<void(__cdecl*)()>(0x588050);

static void __cdecl EditorDrawRadarMap()
{
    g_originalDrawRadarMap();
    BoxArea::DrawGangZones();
}

static void __cdecl EditorDrawRadarBlips()
{
    if (FrontEndMenuManager.m_bDrawRadarOrMap)
    {
        BoxArea::DrawGangZones();
    }
    g_originalDrawRadarBlips();
}

HRESULT __stdcall GameHooks::HookPresent(
    IDirect3DDevice9* pDevice, const RECT* pSrcRect, const RECT* pDestRect, HWND hDestWindow, const RGNDATA* pDirtyRegion)
{
    if (g_gui)
    {
        g_gui->Update();
    }
    return g_gameHooks->m_originalPresent(pDevice, pSrcRect, pDestRect, hDestWindow, pDirtyRegion);
}

HRESULT __stdcall GameHooks::HookReset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentParams)
{
    if (g_gui)
    {
        g_gui->CancelDragCursor();
    }
    if (g_gameHooks)
    {
        g_gameHooks->ResetGizmoHotkeyInput();
    }
    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        ImGui::GetIO().ClearInputKeys();
    }
    if (g_outlineRenderer)
    {
        g_outlineRenderer->OnDeviceLost();
    }
    HRESULT hr = g_gameHooks->m_originalReset(pDevice, pPresentParams);
    if (SUCCEEDED(hr) && ImGui::GetCurrentContext())
    {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    return hr;
}

void __fastcall GameHooks::HookEntityRender(CEntity* pEntity, void* edx)
{
    if (g_freeCamera && g_freeCamera->m_active && PlayerVisibility::HidesEntity(pEntity, FindPlayerPed(), FindPlayerVehicle(-1)))
    {
        return;
    }

    if (g_outlineRenderer && g_outlineRenderer->IsInMaskPass())
    {
        g_gameHooks->m_originalEntityRender(pEntity);
        return;
    }

    struct MaterialBackup
    {
        RpMaterial* mat;
        RwTexture* tex;
        RwRGBA color;
    };
    constexpr int textOffset = ItemData::MaxMaterials;
    MaterialBackup backups[textOffset * 2];
    int backupCount = 0;
    RpGeometry* patchedGeom = nullptr;
    RwUInt32 savedGeomFlags = 0;

    const int itemIdx = g_objectRuntime ? g_objectRuntime->LookupItemIndex(pEntity) : -1;
    if (itemIdx >= 0 && (g_scene->data[itemIdx].MaterialCount() > 0 || g_scene->data[itemIdx].MaterialTextCount() > 0)
        && pEntity->m_pRwObject && RwObjectGetType(pEntity->m_pRwObject) == rpATOMIC && pEntity->m_pRwAtomic->geometry
        && pEntity->m_pRwAtomic->geometry->matList.materials)
    {
        const ItemData& d = g_scene->data[itemIdx];
        const bool materialColorSupported = d.model.HasValue() && d.model.Get().modelId >= 0
            && d.model.Get().modelId < CatalogDatabase::MaxModelId
            && CatalogDatabase::ModelSupportsMaterialColor(static_cast<std::uint16_t>(d.model.Get().modelId));
        RpGeometry* geom = pEntity->m_pRwAtomic->geometry;
        const int numMats = RpGeometryGetNumMaterials(geom);
        savedGeomFlags = geom->flags;

        auto& runtime = g_objectRuntime->Runtime(itemIdx);
        const int count = runtime.materialOrder.Update(d);
        const auto& ordered = runtime.materialOrder.entries;

        for (int i = 0; i < count; ++i)
        {
            const int entry = ordered[i];
            const bool text = entry >= textOffset;
            const int slot = text ? entry - textOffset : entry;
            const int matIndex = text ? d.materialTexts[slot].materialIndex : d.materials[slot].materialIndex;
            if (matIndex < 0 || matIndex >= numMats)
            {
                continue;
            }
            RpMaterial* pMat = geom->matList.materials[matIndex];
            if (!pMat)
            {
                continue;
            }

            RwTexture* tex = text ? runtime.matTexts[slot].tex : runtime.mats[slot].tex;
            const unsigned color = text ? 0 : d.materials[slot].colorArgb;
            const bool hasColor = materialColorSupported && color != 0;
            if (!tex && !hasColor)
            {
                continue;
            }

            backups[backupCount++] = { pMat, pMat->texture, pMat->color };
            if (tex)
            {
                pMat->texture = tex;
            }
            if (hasColor)
            {
                pMat->color.red = (RwUInt8)((color >> 16) & 0xFF);
                pMat->color.green = (RwUInt8)((color >> 8) & 0xFF);
                pMat->color.blue = (RwUInt8)(color & 0xFF);
                pMat->color.alpha = (RwUInt8)((color >> 24) & 0xFF);
                geom->flags |= rpGEOMETRYMODULATEMATERIALCOLOR;
                patchedGeom = geom;
            }
        }
    }

    g_gameHooks->m_originalEntityRender(pEntity);

    IDirect3DDevice9* dev = g_gameHooks->m_device;
    if (dev && g_outlineRenderer && Selection::IsEntityHighlighted(pEntity))
    {
        g_outlineRenderer->OnSelectedEntityRender(pEntity);
    }
    if (dev && g_collisionRenderer && g_collisionRenderer->IsEnabled())
    {
        g_collisionRenderer->OnEntityRender(pEntity);
    }

    for (int i = backupCount - 1; i >= 0; --i)
    {
        backups[i].mat->texture = backups[i].tex;
        backups[i].mat->color = backups[i].color;
    }
    if (patchedGeom)
    {
        patchedGeom->flags = savedGeomFlags;
    }
}

bool __fastcall GameHooks::HookEntityIsOccluded(CEntity* pEntity, void* edx)
{
    if (pEntity && Selection::IsEntityHighlighted(pEntity))
    {
        return false;
    }
    return g_gameHooks->m_originalEntityIsOccluded(pEntity);
}

void __cdecl GameHooks::HookVisibilityRenderEntity(void* obj, float dist)
{
    CEntity* entity = static_cast<CEntity*>(obj);
    if (g_freeCamera && g_freeCamera->m_active && PlayerVisibility::HidesEntity(entity, FindPlayerPed(), FindPlayerVehicle(-1)))
    {
        return;
    }

    bool savedDistanceFade = false;
    bool distanceFadePatched = false;
    if (entity && g_scene && g_objectRuntime)
    {
        const int idx = g_objectRuntime->LookupItemIndex(entity);
        if (idx >= 0 && idx < g_scene->count)
        {
            const ItemData& data = g_scene->data[idx];
            if (data.MaterialCount() > 0 || data.MaterialTextCount() > 0)
            {
                savedDistanceFade = entity->bDistanceFade;
                entity->bDistanceFade = false;
                distanceFadePatched = true;
            }
        }
    }
    g_gameHooks->m_originalVisibilityRenderEntity(obj, dist);
    if (distanceFadePatched)
    {
        entity->bDistanceFade = savedDistanceFade;
    }

    if (!entity || (g_outlineRenderer && g_outlineRenderer->IsInMaskPass()))
    {
        return;
    }
    if (g_outlineRenderer && Selection::IsEntityHighlighted(entity))
    {
        g_outlineRenderer->OnSelectedEntityRender(entity);
    }
    if (g_collisionRenderer && g_collisionRenderer->IsEnabled())
    {
        g_collisionRenderer->OnEntityRender(entity);
    }
}

int32_t __cdecl GameHooks::HookSetupMapEntityVisibility(
    CEntity* entity, CBaseModelInfo* baseModelInfo, float fDistance, bool bIsTimeInRange)
{
    float savedDD = 0.0f;
    bool patched = false;
    const int idx = g_objectRuntime ? g_objectRuntime->LookupItemIndex(entity) : -1;
    if (idx >= 0 && baseModelInfo)
    {
        const float dd = g_scene->data[idx].model.Get().drawDistance;
        if (dd > 0.0f)
        {
            savedDD = baseModelInfo->m_fDrawDistance;
            baseModelInfo->m_fDrawDistance = dd;
            patched = true;
        }
    }

    const int32_t result = g_gameHooks->m_originalSetupMapEntityVisibility(entity, baseModelInfo, fDistance, bIsTimeInRange);

    if (patched)
    {
        baseModelInfo->m_fDrawDistance = savedDD;
    }
    return result;
}

int32_t __cdecl GameHooks::HookCalculateFadingAtomicAlpha(CBaseModelInfo* modelInfo, CEntity* entity, float distance)
{
    float savedDD = 0.0f;
    bool patched = false;
    const int idx = g_objectRuntime ? g_objectRuntime->LookupItemIndex(entity) : -1;
    if (idx >= 0 && modelInfo)
    {
        const float dd = g_scene->data[idx].model.Get().drawDistance;
        if (dd > 0.0f)
        {
            savedDD = modelInfo->m_fDrawDistance;
            modelInfo->m_fDrawDistance = dd;
            patched = true;
        }
    }

    const int32_t alpha = g_gameHooks->m_originalCalculateFadingAtomicAlpha(modelInfo, entity, distance);

    if (patched)
    {
        modelInfo->m_fDrawDistance = savedDD;
    }
    return alpha;
}
