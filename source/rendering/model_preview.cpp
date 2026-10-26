// Source Repository: https://github.com/waverso/waversomapstudio

#include "model_preview.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <d3d9.h>
#include <numbers>

#include "../shared/dff_effects.h"
#include "../shared/model_checks.h"
#include "../shared/model_effects.h"
#include "../shared/private_render_state.h"
#include "../shared/skin_catalog.h"
#include "../shared/skin_instance.h"
#include "../shared/vehicle_catalog.h"
#include "../shared/vehicle_instance.h"
#include "../utils/d3dx_math_compat.h"
#include "CCoronas.h"
#include "CModelInfo.h"
#include "CTimer.h"
#include "CTxdStore.h"
#include "CWeather.h"
#include "FxManager_c.h"
#include "common.h"
#include "../catalog/database.h"
#include "../objects/object_browser.h"
#include "../actors/skin_browser.h"
#include "../ui/theme.h"

namespace
{
    struct Vec3
    {
        float x = 0, y = 0, z = 0;
    };
    Vec3 operator+(Vec3 a, Vec3 b)
    {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }
    Vec3 operator-(Vec3 a, Vec3 b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }
    Vec3 operator*(Vec3 a, float b)
    {
        return { a.x * b, a.y * b, a.z * b };
    }
    float Dot(Vec3 a, Vec3 b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    Vec3 Cross(Vec3 a, Vec3 b)
    {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }
    float Length(Vec3 a)
    {
        return std::sqrt(Dot(a, a));
    }
    Vec3 Normalize(Vec3 a)
    {
        const float n = std::sqrt(Dot(a, a));
        return n > 0.00001f ? a * (1.0f / n) : Vec3 {};
    }

    struct Basis
    {
        Vec3 forward, right, up;
    };
    Basis MakeBasis(float yawDeg, float pitchDeg)
    {
        const float yaw = D3DXToRadian(yawDeg), pitch = D3DXToRadian(pitchDeg);
        Basis b;
        b.forward = Normalize({ -std::cos(pitch) * std::cos(yaw), -std::cos(pitch) * std::sin(yaw), -std::sin(pitch) });
        b.right = Normalize(Cross({ 0, 0, 1 }, b.forward));
        if (Dot(b.right, b.right) < 0.5f)
        {
            b.right = { 1, 0, 0 };
        }
        b.up = Normalize(Cross(b.forward, b.right));
        return b;
    }

    constexpr std::uint16_t PreviewTargetSize = 512;
    constexpr float PreviewViewWindow = 0.55f;

    enum class PreviewState
    {
        Closed,
        Loading,
        Ready,
        Failed
    };

    struct PreviewTarget
    {
        RpWorld* world = nullptr;
        RwCamera* camera = nullptr;
        RwFrame* frame = nullptr;
        RwRaster* color = nullptr;
        RwRaster* depth = nullptr;
        RpLight* ambient = nullptr;
        IDirect3DDevice9* device = nullptr;
    };

    struct PreviewLive
    {
        int desiredModelId = -1;
        int modelId = -1;
        bool desiredSkinModel = false;
        bool skinModel = false;
        int txdSlot = -1;
        std::uint8_t modelInitialState = LOADSTATE_NOT_LOADED;
        std::uint8_t modelInitialFlags = 0;
        std::uint8_t txdInitialState = LOADSTATE_NOT_LOADED;
        std::uint8_t txdInitialFlags = 0;
        bool requestSent = false;
        bool ownsModelRequest = false;
        bool keepsModelInMemory = false;
        bool modelRefAdded = false;
        bool published = false;
        bool dirty = false;
        ImU32 background = 0;
        bool animated = false;
        bool vehicleAnimation = false;
        bool uvAnim = false;
        std::uint8_t failures = 0;
        RwObject* instance = nullptr;
        CVehicle* vehicle = nullptr;
        CBaseModelInfo* referencedInfo = nullptr;
        ModelPreview::Profile initial {};
        ModelPreview::Profile current {};
        ModelPreview::Profile goal {};
        PreviewTarget target {};
        PreviewState state = PreviewState::Closed;

        ModelEffects::Summary effects {};
        ModelEffects::Bounds vertexBounds {};
        ModelEffects::FramingMode framingMode = ModelEffects::FramingMode::None;
        double effectClock = 0.0;
        std::vector<ModelEffects::LightFadeState> lightFade;
        RwRaster* fallbackGlowRaster = nullptr;
        char modelName[32] {};

        struct PreviewFx
        {
            FxSystem_c* system = nullptr;
            bool dead = false;
            double deadAt = 0.0;
        };
        std::vector<PreviewFx> fxSystems;
    };

    PreviewLive g_preview;

    void DestroyPreviewFxSystems();

#include "model_preview_camera.inl"
#include "model_preview_effects.inl"
#include "model_preview_instance.inl"
#include "model_preview_render.inl"

    void ServiceLoadedPreview(IDirect3DDevice9* device)
    {
        if (!AcquirePreviewInstance())
        {
            FailPreview();
            return;
        }
        if (!EnsurePreviewTarget(device))
        {
            if (++g_preview.failures >= 3)
            {
                ReleasePreviewNow();
                FailPreview();
            }
            return;
        }

        g_preview.state = PreviewState::Ready;
        if (!RenderPreviewCameraPass(device))
        {
            if (++g_preview.failures >= 3)
            {
                ReleasePreviewNow();
                FailPreview();
            }
            return;
        }

        IDirect3DTexture9* texture = PreviewD3DTexture();
        D3DSURFACE_DESC description {};
        if (!texture || FAILED(texture->GetLevelDesc(0, &description)))
        {
            ReleasePreviewNow();
            FailPreview();
            return;
        }
        g_preview.failures = 0;
        g_preview.published = true;
        g_preview.dirty = false;
        g_preview.background = Theme::g_controlBackground;
    }

    float PreviewEasingDelta()
    {
        static std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        return dt;
    }

    void ServiceLivePreview(IDirect3DDevice9* device, bool cameraAlreadyPaused = false)
    {
        if (g_preview.modelId < 0 || g_preview.state == PreviewState::Failed)
        {
            return;
        }
        if (g_preview.state == PreviewState::Ready && g_preview.instance)
        {
            if (g_preview.desiredModelId >= 0 && g_preview.background != Theme::g_controlBackground)
            {
                g_preview.dirty = true;
            }
            const float fTimeStepInSeconds = CTimer::ms_fTimeStep / 50.0f;
            if (g_preview.animated && RwObjectGetType(g_preview.instance) == rpCLUMP)
            {
                RpAnimBlendClumpUpdateAnimations(reinterpret_cast<RpClump*>(g_preview.instance), fTimeStepInSeconds, true);
                UpdatePreviewSkinHierarchy(reinterpret_cast<RpClump*>(g_preview.instance));
                g_preview.dirty = true;
            }
            if (g_preview.vehicleAnimation)
            {
                g_preview.dirty = true;
            }
            if (g_preview.uvAnim)
            {
                if (!(g_preview.referencedInfo && g_preview.referencedInfo->m_nRefCount > 1))
                {
                    AdvanceInstanceUVAnim(g_preview.instance);
                }
                g_preview.dirty = true;
            }
            if (g_preview.effects.Valid())
            {
                AdvancePreviewFx(static_cast<double>(fTimeStepInSeconds));
                g_preview.dirty = true;
            }
            {
                const float dt = std::clamp(PreviewEasingDelta(), 0.0f, 0.05f);
                const float k = 1.0f - std::exp(-dt * 28.0f);
                const float base = std::max(0.01f, g_preview.initial.distance);
                ModelPreview::Profile& c = g_preview.current;
                const ModelPreview::Profile& g = g_preview.goal;
                float dYaw = g.yaw - c.yaw;
                dYaw -= 360.0f * std::round(dYaw / 360.0f);
                c.yaw = std::fmod(c.yaw + dYaw * k, 360.0f);
                c.pitch += (g.pitch - c.pitch) * k;
                c.distance += (g.distance - c.distance) * k;
                c.target[0] += (g.target[0] - c.target[0]) * k;
                c.target[1] += (g.target[1] - c.target[1]) * k;
                c.target[2] += (g.target[2] - c.target[2]) * k;
                const float eps = base * 0.0005f;
                const bool settled = std::fabs(dYaw) < 0.05f && std::fabs(g.pitch - c.pitch) < 0.05f
                    && std::fabs(g.distance - c.distance) < eps && std::fabs(g.target[0] - c.target[0]) < eps
                    && std::fabs(g.target[1] - c.target[1]) < eps && std::fabs(g.target[2] - c.target[2]) < eps;
                if (settled)
                {
                    c = g;
                }
                else
                {
                    g_preview.dirty = true;
                }
            }
        }
        if (g_preview.state == PreviewState::Ready && !g_preview.dirty)
        {
            return;
        }
        if (!CStreaming::ms_aInfoForModel)
        {
            FailPreview();
            return;
        }

        const std::uint8_t loadState = CStreaming::ms_aInfoForModel[g_preview.modelId].m_nLoadState;
        if (loadState != LOADSTATE_LOADED || (g_preview.skinModel && !SkinInstance::AnimationReady(g_preview.modelId)))
        {
            if (!g_preview.requestSent)
            {
                const int requestFlags = g_preview.ownsModelRequest
                    ? eStreamingFlags::PRIORITY_REQUEST | eStreamingFlags::KEEP_IN_MEMORY
                    : eStreamingFlags::PRIORITY_REQUEST;
                if (g_preview.skinModel)
                {
                    SkinInstance::Request(g_preview.modelId, requestFlags);
                }
                else
                {
                    CStreaming::RequestModel(g_preview.modelId, requestFlags);
                }
                g_preview.keepsModelInMemory = g_preview.ownsModelRequest;
                CStreaming::LoadAllRequestedModels(true);
                g_preview.requestSent = true;
            }
            return;
        }
        if (!device || device->TestCooperativeLevel() != D3D_OK)
        {
            return;
        }

        if (cameraAlreadyPaused)
        {
            if (!RwCameraGetCurrentCamera())
            {
                ServiceLoadedPreview(device);
            }
            return;
        }
        ActiveCameraPause cameraPause;
        if (!cameraPause.Ready())
        {
            return;
        }
        ServiceLoadedPreview(device);
    }
}

bool ModelPreview::GetProfile(int modelId, Profile& out)
{
    out = {};
    if (modelId < 0 || modelId >= CatalogDatabase::MaxModelId)
    {
        return false;
    }
    const CatalogDatabase::ObjectModelMeta* meta = CatalogDatabase::ObjectMeta(static_cast<std::uint16_t>(modelId));
    if (!meta)
    {
        return false;
    }
    std::memcpy(out.target, meta->target, sizeof(out.target));
    out.yaw = meta->yaw;
    out.pitch = meta->pitch;
    out.distance = std::max(0.01f, meta->distance);
    return true;
}

void ModelPreview::Open(int modelId, bool skin)
{
    if (skin)
    {
        if (!SkinCatalog::IsValid(modelId))
        {
            return;
        }
    }
    else if (!VehicleCatalog::IsValid(modelId)
        && (modelId < 0 || modelId >= CatalogDatabase::MaxModelId
            || !CatalogDatabase::ModelExists(static_cast<std::uint16_t>(modelId))))
    {
        return;
    }
    if (g_preview.desiredModelId == modelId && g_preview.desiredSkinModel == skin)
    {
        return;
    }
    g_preview.desiredModelId = modelId;
    g_preview.desiredSkinModel = skin;
    g_preview.published = false;
}

void ModelPreview::Close()
{
    g_preview.desiredModelId = -1;
    g_preview.desiredSkinModel = false;
    g_preview.published = false;
}

void ModelPreview::Reset()
{
    if (g_preview.modelId < 0)
    {
        return;
    }
    g_preview.goal = g_preview.initial;
    g_preview.dirty = true;
}

void ModelPreview::Orbit(float deltaYaw, float deltaPitch)
{
    if (g_preview.modelId < 0)
    {
        return;
    }
    g_preview.goal.yaw = std::fmod(g_preview.goal.yaw + deltaYaw, 360.0f);
    g_preview.goal.pitch = std::clamp(g_preview.goal.pitch + deltaPitch, -89.0f, 89.0f);
    g_preview.dirty = true;
}

void ModelPreview::Pan(float deltaX, float deltaY, float viewportSize)
{
    if (g_preview.modelId < 0 || viewportSize < 1.0f)
    {
        return;
    }
    const Basis basis = MakeBasis(g_preview.goal.yaw, g_preview.goal.pitch);
    const float scale = g_preview.goal.distance * 1.1f / viewportSize;
    Vec3 target { g_preview.goal.target[0], g_preview.goal.target[1], g_preview.goal.target[2] };
    target = target + basis.right * (deltaX * scale) + basis.up * (deltaY * scale);
    g_preview.goal.target[0] = target.x;
    g_preview.goal.target[1] = target.y;
    g_preview.goal.target[2] = target.z;
    g_preview.dirty = true;
}

void ModelPreview::Zoom(float wheelDelta, float step)
{
    if (g_preview.modelId < 0 || wheelDelta == 0.0f)
    {
        return;
    }
    const float base = std::max(0.01f, g_preview.initial.distance);
    g_preview.goal.distance = std::clamp(g_preview.goal.distance * std::pow(step, -wheelDelta), base * 0.05f, base * 20.0f);
    g_preview.dirty = true;
}

void ModelPreview::UpdateFrame(IDirect3DDevice9* device)
{
    if (g_preview.desiredModelId != g_preview.modelId)
    {
        if (HasPreviewResources())
        {
            ActiveCameraPause cameraPause;
            if (!cameraPause.Ready())
            {
                return;
            }
            ApplyDesiredSelectionNow();
            ServiceLivePreview(device, true);
            return;
        }
        else
        {
            ApplyDesiredSelectionNow();
        }
    }
    ServiceLivePreview(device);
}

IDirect3DTexture9* ModelPreview::Texture()
{
    return g_preview.published ? PreviewD3DTexture() : nullptr;
}

bool ModelPreview::ShouldUpdateFx(FxSystem_c* system, RwCamera* camera)
{
    for (const PreviewLive::PreviewFx& fx : g_preview.fxSystems)
    {
        if (fx.system == system)
        {
            return camera && camera == g_preview.target.camera;
        }
    }
    return true;
}

void ModelPreview::OnDeviceLost()
{
    if (!g_preview.target.camera && !g_preview.target.frame && !g_preview.target.depth && !g_preview.target.color)
    {
        return;
    }
    g_preview.published = false;
    DestroyPreviewTargetNow();
    if (g_preview.instance)
    {
        g_preview.state = PreviewState::Ready;
    }
    g_preview.dirty = g_preview.modelId >= 0;
}
