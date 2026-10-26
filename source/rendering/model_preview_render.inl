// Source Repository: https://github.com/waverso/waversomapstudio

struct AtomicRenderStats
{
    int visible = 0;
};

RpAtomic* CountVisibleAtomic(RpAtomic* atomic, void* data)
{
    AtomicRenderStats& stats = *static_cast<AtomicRenderStats*>(data);
    if (atomic && !ModelChecks::IsGunflashAtomic(atomic) && (RpAtomicGetFlags(atomic) & rpATOMICRENDER))
    {
        ++stats.visible;
    }
    return atomic;
}

RpAtomic* RenderPreviewAtomic(RpAtomic* atomic, void*)
{
    if (!atomic || ModelChecks::IsGunflashAtomic(atomic) || !(RpAtomicGetFlags(atomic) & rpATOMICRENDER))
    {
        return atomic;
    }

    AtomicDefaultRenderCallBack(atomic);
    return atomic;
}

class PreviewVehiclePipelineScope
{
    public:
    PreviewVehiclePipelineScope(PreviewTarget& target, bool enabled) : active(enabled)
    {
        if (!active)
        {
            return;
        }
        previousCamera = sceneCamera;
        sceneCamera = target.camera;
        previousAmbient = ambient;
        previewAmbient = target.ambient ? target.ambient : ambient;
        ambient = previewAmbient;
        direct = directSlot;
        if (previewAmbient)
        {
            ambientFlags = RpLightGetFlags(previewAmbient);
            RpLightSetFlags(previewAmbient, ambientFlags | rpLIGHTLIGHTATOMICS);
        }
        if (direct)
        {
            directFlags = RpLightGetFlags(direct);
            RpLightSetFlags(direct, directFlags | rpLIGHTLIGHTATOMICS);
        }
    }

    ~PreviewVehiclePipelineScope()
    {
        if (!active)
        {
            return;
        }
        if (direct)
        {
            RpLightSetFlags(direct, directFlags);
        }
        if (previewAmbient)
        {
            RpLightSetFlags(previewAmbient, ambientFlags);
        }
        ambient = previousAmbient;
        sceneCamera = previousCamera;
    }

    private:
    RwCamera*& sceneCamera = *reinterpret_cast<RwCamera**>(0xC1703C);
    RpLight*& ambient = *reinterpret_cast<RpLight**>(0xC886E8);
    RpLight*& directSlot = *reinterpret_cast<RpLight**>(0xC886EC);
    RwCamera* previousCamera = nullptr;
    RpLight* previousAmbient = nullptr;
    RpLight* previewAmbient = nullptr;
    RpLight* direct = nullptr;
    RwUInt32 ambientFlags = 0;
    RwUInt32 directFlags = 0;
    bool active = false;
};

bool PreviewUsesVehicleLighting()
{
    return g_preview.vehicle || (g_preview.modelId >= 1000 && g_preview.modelId <= 1193);
}

bool RenderPreviewInstance()
{
    if (!g_preview.instance)
    {
        return false;
    }
    const RwUInt8 type = RwObjectGetType(g_preview.instance);
    if (g_preview.vehicle && type == rpCLUMP)
    {
        RpClump* clump = reinterpret_cast<RpClump*>(g_preview.instance);
        AtomicRenderStats stats {};
        RpClumpForAllAtomics(clump, CountVisibleAtomic, &stats);
        if (stats.visible == 0)
        {
            return false;
        }
        VehicleInstance::UpdatePreview(g_preview.vehicle);
        if (RwFrame* frame = RpClumpGetFrame(clump))
        {
            RwFrameUpdateObjects(frame);
        }
        UpdatePreviewSkinHierarchy(clump);
        CVisibilityPlugins::InitAlphaAtomicList();
        CVisibilityPlugins::SetupVehicleVariables(clump);
        g_preview.vehicle->SetupRender();
        g_preview.vehicle->Render();
        CVisibilityPlugins::RenderAlphaAtomics();
        g_preview.vehicle->ResetAfterRender();
        return true;
    }
    VehicleInstance::EditableMaterialsScope materials(g_preview.vehicle);
    if (type == rpATOMIC)
    {
        RpAtomic* atomic = reinterpret_cast<RpAtomic*>(g_preview.instance);
        if (ModelChecks::IsGunflashAtomic(atomic) || !(RpAtomicGetFlags(atomic) & rpATOMICRENDER))
        {
            return false;
        }
        if (RwFrame* frame = RpAtomicGetFrame(atomic))
        {
            RwFrameUpdateObjects(RwFrameGetRoot(frame));
        }
        AtomicDefaultRenderCallBack(atomic);
        return true;
    }
    if (type == rpCLUMP)
    {
        RpClump* clump = reinterpret_cast<RpClump*>(g_preview.instance);
        AtomicRenderStats stats {};
        RpClumpForAllAtomics(clump, CountVisibleAtomic, &stats);
        if (stats.visible == 0)
        {
            return false;
        }
        if (RwFrame* frame = RpClumpGetFrame(clump))
        {
            RwFrameUpdateObjects(frame);
        }
        UpdatePreviewSkinHierarchy(clump);
        RpClumpForAllAtomics(clump, RenderPreviewAtomic, nullptr);
        return true;
    }
    return false;
}

bool RenderPreviewCameraPass(IDirect3DDevice9* device)
{
    if (RwCameraGetCurrentCamera() || !ConfigurePreviewCamera())
    {
        return false;
    }
    SharedPrivateRender::RenderWareStateGuard rwGuard;
    if (rwGuard.ValidCount() != SharedPrivateRender::SafeRwStates.size())
    {
        return false;
    }
    SharedPrivateRender::StateBlockGuard d3dGuard(device);
    if (!d3dGuard.IsValid())
    {
        return false;
    }
    SharedPrivateRender::InvalidateRwShaderCaches();
    SharedPrivateRender::InvalidateRwLastSentRenderStates();
    SharedPrivateRender::EstablishFixedFunctionPipeline(device);

    RwRGBA background { static_cast<RwUInt8>(Theme::g_controlBackground >> IM_COL32_R_SHIFT),
        static_cast<RwUInt8>(Theme::g_controlBackground >> IM_COL32_G_SHIFT),
        static_cast<RwUInt8>(Theme::g_controlBackground >> IM_COL32_B_SHIFT), 255 };
    if (!RwCameraClear(g_preview.target.camera, &background, rwCAMERACLEARIMAGE | rwCAMERACLEARZ)
        || !RwCameraBeginUpdate(g_preview.target.camera))
    {
        return false;
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
    SharedPrivateRender::ForceRenderState(
        rwRENDERSTATEZTESTENABLE, reinterpret_cast<void*>(TRUE), reinterpret_cast<void*>(FALSE));
    SharedPrivateRender::ForceRenderState(
        rwRENDERSTATEZWRITEENABLE, reinterpret_cast<void*>(TRUE), reinterpret_cast<void*>(FALSE));
    SharedPrivateRender::ForceRenderState(rwRENDERSTATEFOGENABLE, reinterpret_cast<void*>(FALSE), reinterpret_cast<void*>(TRUE));
    SharedPrivateRender::ForceRenderState(
        rwRENDERSTATEVERTEXALPHAENABLE, reinterpret_cast<void*>(TRUE), reinterpret_cast<void*>(FALSE));
    SharedPrivateRender::ForceRenderState(
        rwRENDERSTATESRCBLEND, reinterpret_cast<void*>(rwBLENDSRCALPHA), reinterpret_cast<void*>(rwBLENDONE));
    SharedPrivateRender::ForceRenderState(
        rwRENDERSTATEDESTBLEND, reinterpret_cast<void*>(rwBLENDINVSRCALPHA), reinterpret_cast<void*>(rwBLENDONE));
    SharedPrivateRender::ForceRenderState(rwRENDERSTATETEXTUREADDRESS, reinterpret_cast<void*>(rwTEXTUREADDRESSWRAP),
        reinterpret_cast<void*>(rwTEXTUREADDRESSCLAMP));
    SharedPrivateRender::ForceRenderState(
        rwRENDERSTATETEXTUREFILTER, reinterpret_cast<void*>(rwFILTERLINEAR), reinterpret_cast<void*>(rwFILTERNEAREST));
    SharedPrivateRender::ForceRenderState(rwRENDERSTATEALPHATESTFUNCTIONREF, reinterpret_cast<void*>(2), nullptr);
    const RwCullMode cull
        = g_preview.referencedInfo && g_preview.referencedInfo->bIsBackfaceCulled ? rwCULLMODECULLBACK : rwCULLMODECULLNONE;
    SharedPrivateRender::ForceRenderState(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(cull),
        reinterpret_cast<void*>(cull == rwCULLMODECULLBACK ? rwCULLMODECULLNONE : rwCULLMODECULLBACK));
    RwD3D9SetRenderState(D3DRS_LIGHTING, FALSE);
    RwD3D9SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(255, 255, 255));
    RwD3D9SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    RwD3D9SetRenderState(D3DRS_FOGENABLE, FALSE);
    RwD3D9SetRenderState(D3DRS_STENCILENABLE, FALSE);

    bool rendered = false;
    {
        if (g_preview.uvAnim)
        {
            SharedPrivateRender::InvalidateRwTextureMatrixCache(0x10);
        }
        PreviewAmbientScope ambient(g_preview.target);
        {
            PreviewVehiclePipelineScope vehiclePipeline(g_preview.target, PreviewUsesVehicleLighting());
            RwCamera* previousVisibilityCamera = CVisibilityPlugins::ms_pCamera;
            CVisibilityPlugins::SetRenderWareCamera(g_preview.target.camera);
            rendered = RenderPreviewInstance();
            if (previousVisibilityCamera)
            {
                CVisibilityPlugins::SetRenderWareCamera(previousVisibilityCamera);
            }
        }
        if (g_preview.effects.Valid())
        {
            if (DrawPreviewEffectSprites())
            {
                rendered = true;
            }
            if (!g_preview.fxSystems.empty())
            {
                bool anyAlive = false;
                for (const PreviewLive::PreviewFx& fx : g_preview.fxSystems)
                {
                    anyAlive = anyAlive || fx.system != nullptr;
                }
                if (anyAlive)
                {
                    g_fxMan.CalcFrustumInfo(g_preview.target.camera);
                    g_fxMan.Render(g_preview.target.camera, 0);
                    rendered = true;
                }
            }
        }
    }
    const bool ended = RwCameraEndUpdate(g_preview.target.camera) != nullptr;
    return rendered && ended;
}
