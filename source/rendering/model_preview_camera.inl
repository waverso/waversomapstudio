// Source Repository: https://github.com/waverso/waversomapstudio

class ActiveCameraPause
{
    public:
    ActiveCameraPause()
    {
        previous = RwCameraGetCurrentCamera();
        paused = previous && RwCameraEndUpdate(previous);
    }
    ~ActiveCameraPause()
    {
        if (!paused)
        {
            return;
        }
        if (RwCamera* current = RwCameraGetCurrentCamera())
        {
            if (current != previous)
            {
                RwCameraEndUpdate(current);
            }
        }
        if (!RwCameraGetCurrentCamera())
        {
            RwCameraBeginUpdate(previous);
        }
        if (!RwCameraGetCurrentCamera())
        {
            RwCameraBeginUpdate(previous);
        }
    }
    bool Ready() const
    {
        return paused;
    }

    private:
    RwCamera* previous = nullptr;
    bool paused = false;
};

RpWorld* GameWorld()
{
    return *reinterpret_cast<RpWorld**>(0xC17038);
}

class PreviewAmbientScope
{
    public:
    explicit PreviewAmbientScope(PreviewTarget& target) : world(target.world), light(target.ambient)
    {
        attached = world && light && RpWorldAddLight(world, light);
    }

    ~PreviewAmbientScope()
    {
        if (attached)
        {
            RpWorldRemoveLight(world, light);
        }
    }

    private:
    RpWorld* world = nullptr;
    RpLight* light = nullptr;
    bool attached = false;
};

IDirect3DTexture9* PreviewD3DTexture()
{
    return g_preview.target.color ? *reinterpret_cast<IDirect3DTexture9**>(g_preview.target.color + 1) : nullptr;
}

bool HasPreviewResources()
{
    const PreviewTarget& target = g_preview.target;
    return g_preview.instance || g_preview.modelRefAdded || target.world || target.camera || target.frame || target.color
        || target.depth || target.ambient;
}

void DestroyPreviewTargetNow()
{
    PreviewTarget& target = g_preview.target;
    if (target.world && target.camera)
    {
        RpWorldRemoveCamera(target.world, target.camera);
    }
    if (target.camera)
    {
        RwCameraSetRaster(target.camera, nullptr);
        RwCameraSetZRaster(target.camera, nullptr);
        RwCameraSetFrame(target.camera, nullptr);
        RwCameraDestroy(target.camera);
    }
    if (target.frame)
    {
        RwFrameDestroy(target.frame);
    }
    if (target.depth)
    {
        RwRasterDestroy(target.depth);
    }
    if (target.color)
    {
        RwRasterDestroy(target.color);
    }
    if (target.ambient)
    {
        RpLightDestroy(target.ambient);
    }
    target = {};
}

RwRaster* CreatePreviewDepthRaster(IDirect3DDevice9* device)
{
    IDirect3DSurface9* currentDepth = nullptr;
    if (FAILED(device->GetDepthStencilSurface(&currentDepth)))
    {
        return nullptr;
    }
    D3DSURFACE_DESC description {};
    const HRESULT result = currentDepth->GetDesc(&description);
    currentDepth->Release();
    if (FAILED(result))
    {
        return nullptr;
    }
    RwRaster* raster = RwRasterCreate(PreviewTargetSize, PreviewTargetSize, 0, rwRASTERTYPEZBUFFER | rwRASTERDONTALLOCATE);
    if (!raster)
    {
        return nullptr;
    }
    auto* extension = reinterpret_cast<unsigned char*>(raster) + *reinterpret_cast<int*>(0xB4E9E0);
    if (FAILED(device->CreateDepthStencilSurface(PreviewTargetSize, PreviewTargetSize, description.Format, D3DMULTISAMPLE_NONE, 0,
            FALSE, reinterpret_cast<IDirect3DSurface9**>(extension), nullptr)))
    {
        RwRasterDestroy(raster);
        return nullptr;
    }
    *reinterpret_cast<D3DFORMAT*>(extension + 0x18) = description.Format;
    raster->cFlags &= ~rwRASTERDONTALLOCATE;
    plugin::Call<0x4CB530>(raster);
    return raster;
}

bool EnsurePreviewTarget(IDirect3DDevice9* device)
{
    PreviewTarget& target = g_preview.target;
    if (target.camera && target.device == device)
    {
        return true;
    }
    DestroyPreviewTargetNow();
    RpWorld* world = GameWorld();
    if (!device || !world)
    {
        return false;
    }

    target.color = plugin::CallAndReturn<RwRaster*, 0x4CD050>(
        PreviewTargetSize, PreviewTargetSize, D3DFMT_A8R8G8B8, rwRASTERTYPECAMERATEXTURE);
    target.depth = CreatePreviewDepthRaster(device);
    target.camera = RwCameraCreate();
    target.frame = RwFrameCreate();
    if (!target.color || !target.depth || !target.camera || !target.frame)
    {
        DestroyPreviewTargetNow();
        return false;
    }

    target.ambient = RpLightCreate(rpLIGHTAMBIENT);
    if (target.ambient)
    {
        const RwRGBAReal fill { 0.45f, 0.45f, 0.45f, 1.0f };
        RpLightSetColor(target.ambient, &fill);
    }

    RwCameraSetRaster(target.camera, target.color);
    RwCameraSetZRaster(target.camera, target.depth);
    RwCameraSetFrame(target.camera, target.frame);
    RwCameraSetProjection(target.camera, rwPERSPECTIVE);
    if (!RpWorldAddCamera(world, target.camera))
    {
        DestroyPreviewTargetNow();
        return false;
    }
    target.world = world;
    target.device = device;
    return true;
}

void PreviewBounds(Vec3& center, float& radius)
{
    if (g_preview.skinModel)
    {
        const auto model = SkinBrowser::GetModel(g_preview.modelId);
        center = { (model.boundsMin[0] + model.boundsMax[0]) * .5f, (model.boundsMin[1] + model.boundsMax[1]) * .5f,
            (model.boundsMin[2] + model.boundsMax[2]) * .5f };
        radius = std::max(0.05f, model.radius);
        return;
    }
    const ObjectBrowser::ModelInfo model = ObjectBrowser::GetModel(g_preview.modelId);
    if (model.manualCenter)
    {
        center = { (model.boundsMin[0] + model.boundsMax[0]) * 0.5f, (model.boundsMin[1] + model.boundsMax[1]) * 0.5f,
            (model.boundsMin[2] + model.boundsMax[2]) * 0.5f };
        radius = std::max(0.05f, model.radius);
        return;
    }
    if (g_preview.framingMode == ModelEffects::FramingMode::EffectsOnly)
    {
        const ModelEffects::Bounds bounds = ModelEffects::EffectBounds(g_preview.effects, true);
        if (bounds.valid)
        {
            center = { (bounds.minimum[0] + bounds.maximum[0]) * 0.5f, (bounds.minimum[1] + bounds.maximum[1]) * 0.5f,
                (bounds.minimum[2] + bounds.maximum[2]) * 0.5f };
            const float halfX = (bounds.maximum[0] - bounds.minimum[0]) * 0.5f;
            const float halfY = (bounds.maximum[1] - bounds.minimum[1]) * 0.5f;
            const float halfZ = (bounds.maximum[2] - bounds.minimum[2]) * 0.5f;
            radius = std::max(0.05f, std::sqrt(halfX * halfX + halfY * halfY + halfZ * halfZ));
            return;
        }
    }
    if (g_preview.referencedInfo && g_preview.referencedInfo->m_pColModel)
    {
        const CSphere& sphere = g_preview.referencedInfo->m_pColModel->m_boundSphere;
        if (std::isfinite(sphere.m_fRadius) && sphere.m_fRadius > 0.001f)
        {
            center = { sphere.m_vecCenter.x, sphere.m_vecCenter.y, sphere.m_vecCenter.z };
            radius = sphere.m_fRadius;
            return;
        }
    }

    center = { (model.boundsMin[0] + model.boundsMax[0]) * 0.5f, (model.boundsMin[1] + model.boundsMax[1]) * 0.5f,
        (model.boundsMin[2] + model.boundsMax[2]) * 0.5f };
    const float halfX = std::fabs(model.boundsMax[0] - model.boundsMin[0]) * 0.5f;
    const float halfY = std::fabs(model.boundsMax[1] - model.boundsMin[1]) * 0.5f;
    const float halfZ = std::fabs(model.boundsMax[2] - model.boundsMin[2]) * 0.5f;
    radius = std::max(0.05f, std::sqrt(halfX * halfX + halfY * halfY + halfZ * halfZ));
}

bool ConfigurePreviewCamera()
{
    if (!g_preview.target.camera || !g_preview.target.frame)
    {
        return false;
    }
    const ModelPreview::Profile& profile = g_preview.current;
    if (!std::isfinite(profile.yaw) || !std::isfinite(profile.pitch) || !std::isfinite(profile.distance)
        || profile.distance <= 0.0f || !std::isfinite(profile.target[0]) || !std::isfinite(profile.target[1])
        || !std::isfinite(profile.target[2]))
    {
        return false;
    }

    const Basis basis = MakeBasis(profile.yaw, profile.pitch);
    const Vec3 target { profile.target[0], profile.target[1], profile.target[2] };
    const Vec3 eye = target - basis.forward * profile.distance;
    RwMatrix matrix {};
    RwMatrixSetIdentity(&matrix);
    matrix.right = { basis.right.x, basis.right.y, basis.right.z };
    matrix.up = { basis.up.x, basis.up.y, basis.up.z };
    matrix.at = { basis.forward.x, basis.forward.y, basis.forward.z };
    matrix.pos = { eye.x, eye.y, eye.z };
    RwFrameTransform(g_preview.target.frame, &matrix, rwCOMBINEREPLACE);
    RwFrameOrthoNormalize(g_preview.target.frame);
    RwFrameUpdateObjects(g_preview.target.frame);

    RwV2d viewWindow { PreviewViewWindow, PreviewViewWindow };
    RwCameraSetViewWindow(g_preview.target.camera, &viewWindow);

    Vec3 boundsCenter {};
    float radius = 0.05f;
    PreviewBounds(boundsCenter, radius);
    if (!std::isfinite(boundsCenter.x) || !std::isfinite(boundsCenter.y) || !std::isfinite(boundsCenter.z)
        || !std::isfinite(radius) || radius <= 0.0f)
    {
        boundsCenter = target;
        radius = std::max(0.05f, g_preview.initial.distance);
    }
    const float centerDistance = Length(boundsCenter - eye);
    const float nearestBound = centerDistance - radius * 1.5f;
    const float nearPlane = std::max(0.01f, std::min(profile.distance * 0.05f, nearestBound));
    const float farPlane = std::max({ 1000.0f, centerDistance + radius * 4.0f, profile.distance * 8.0f, nearPlane + 100.0f });
    RwCameraSetNearClipPlane(g_preview.target.camera, nearPlane);
    RwCameraSetFarClipPlane(g_preview.target.camera, farPlane);
    return true;
}
