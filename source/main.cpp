// Source Repository: https://github.com/waverso/waversomapstudio

#include <cwchar>
#include <string>

#include "CPed.h"
#include "scene/actions.h"
#include "actors/actor_runtime.h"
#include "rendering/collision_renderer.h"
#include "layout/curves.h"
#include "ui/font_preview_atlas.h"
#include "scene/free_camera.h"
#include "hooks/game_patches.h"
#include "scene/gizmos.h"
#include "ui/gui.h"
#include "hooks/game_hooks.h"
#include "ui/icons.h"
#include "rendering/material_text_renderer.h"
#include "rendering/model_preview.h"
#include "rendering/model_thumbnails.h"
#include "objects/object_runtime.h"
#include "rendering/outline_renderer.h"
#include "scene/picker.h"
#include "pickups/pickup_runtime.h"
#include "races/races.h"
#include "objects/removal_manager.h"
#include "hooks/samp.h"
#include "scene/scene.h"
#include "scene/selection.h"
#include "editor/studio_info.h"
#include "hooks/game_clock_controller.h"
#include "vehicles/vehicle_runtime.h"
#include "plugin.h"
#include "shared/private_render_cache.h"
#include "utils/utils.h"
#include "vendor/imgui/imgui.h"

GameHooks* g_gameHooks = nullptr;
Gui* g_gui = nullptr;
FreeCamera* g_freeCamera = nullptr;
ScenePicker* g_scenePicker = nullptr;
RemovalManager* g_removalManager = nullptr;
ActionManager* g_sceneActions = nullptr;
ActionManager* g_userActions = nullptr;
Gizmos* g_gizmos = nullptr;
CollisionRenderer* g_collisionRenderer = nullptr;
OutlineRenderer* g_outlineRenderer = nullptr;
Icons* g_icons = nullptr;
EditorScene* g_scene = nullptr;
ObjectRuntime* g_objectRuntime = nullptr;
GamePatches* g_gamePatches = nullptr;
SampIntegration* g_sampIntegration = nullptr;

bool g_renderInteriors = false;

static void Initialize()
{
    const unsigned int gv = plugin::GetGameVersion();
    if (gv != GAME_10US_HOODLUM && gv != GAME_10US_COMPACT)
    {
        Utils::FatalExit("This plugin requires GTA San Andreas v1.0 US (HOODLUM or Compact).");
    }

    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&Initialize), &module))
    {
        Utils::FatalExit("Cannot determine the plugin file name.");
    }
    std::wstring modulePath(32768, L'\0');
    const DWORD length = GetModuleFileNameW(module, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (!length || length >= modulePath.size())
    {
        Utils::FatalExit("Cannot determine the plugin file name.");
    }
    const wchar_t* name = std::wcsrchr(modulePath.c_str(), L'\\');
    if (_wcsicmp(name ? name + 1 : modulePath.c_str(), L"waversomapstudio.asi") != 0)
    {
        Utils::FatalExit("The plugin file has been renamed. Restore its original name.");
    }

    IDirect3DDevice9* device = *(IDirect3DDevice9**)0xC97C28;

    g_gameClockController = new GameClockController();
    g_gameHooks = new GameHooks();
    g_sampIntegration = new SampIntegration();
    g_removalManager = new RemovalManager();
    g_sceneActions = new ActionManager();
    g_userActions = new ActionManager();
    g_scene = new EditorScene();
    g_objectRuntime = new ObjectRuntime();
    g_vehicleRuntime = new VehicleRuntime();
    g_pickupRuntime = new PickupRuntime();
    g_actorRuntime = new ActorRuntime();
    g_gui = new Gui(device);
    g_freeCamera = new FreeCamera();
    g_scenePicker = new ScenePicker();
    g_gizmos = new Gizmos();
    g_collisionRenderer = new CollisionRenderer();
    g_outlineRenderer = new OutlineRenderer();
    plugin::Events::pedRenderEvent += [](CPed* ped)
    {
        if (g_actorRuntime && g_actorRuntime->LookupItemIndex(ped) >= 0 && g_outlineRenderer
            && Selection::IsEntityHighlighted(ped))
        {
            g_outlineRenderer->OnSelectedEntityRender(ped);
        }
    };
    g_icons = new Icons(device);
    FontPreviewAtlas::Load(device);
    g_gamePatches = new GamePatches();
    g_gui->LoadUserData();
}

static void UpdateAfter()
{
    if (!g_freeCamera)
    {
        Initialize();
    }
    g_freeCamera->Update();
    if (g_sampIntegration)
    {
        g_sampIntegration->Update();
    }

    static bool editMode = false;
    const bool editActive = g_freeCamera->m_active;

    if (editActive != editMode)
    {
        if (g_gamePatches)
        {
            g_gamePatches->SetEditMode(editActive ? PatchState::On : PatchState::RestoreOriginal);
        }
        editMode = editActive;
    }

    static bool riApplied = false;
    const bool riDesired = editActive && g_renderInteriors;
    if (riDesired != riApplied)
    {
        if (g_gamePatches)
        {
            g_gamePatches->SetRenderInteriors(riDesired ? PatchState::On : PatchState::RestoreOriginal);
        }
        riApplied = riDesired;
    }

    const bool gizmoHotkeysAllowed = g_freeCamera->m_active && ImGui::GetCurrentContext() && !ImGui::GetIO().WantCaptureKeyboard
        && !ImGui::GetIO().WantTextInput && !SampTextInputActive() && !g_gizmos->IsDragging() && !g_gizmos->IsRadiusDragging()
        && !g_gizmos->IsBoxAreaDragging();
    GizmoHotkeyEvent gizmoHotkey;
    while (g_gameHooks && g_gameHooks->PopGizmoHotkeyEvent(gizmoHotkey))
    {
        if (!gizmoHotkeysAllowed)
        {
            continue;
        }
        switch (gizmoHotkey)
        {
            case GizmoHotkeyEvent::None:
                Curves::SetHandle(-1, -1);
                g_gizmos->SetGizmoTransformMode(GizmoMode::None);
                break;
            case GizmoHotkeyEvent::Position:
                g_gizmos->SetGizmoTransformMode(GizmoMode::Position);
                break;
            case GizmoHotkeyEvent::Rotation:
                g_gizmos->SetGizmoTransformMode(GizmoMode::Rotation);
                break;
            case GizmoHotkeyEvent::Both:
                g_gizmos->SetGizmoTransformMode(GizmoMode::Both);
                break;
            case GizmoHotkeyEvent::TogglePivot:
                Selection::TogglePivotMode();
                break;
            case GizmoHotkeyEvent::ToggleSpace:
                g_gizmos->SetGizmoSpace(
                    (g_gizmos->GetGizmoSpace() == GizmoSpace::Local) ? GizmoSpace::Global : GizmoSpace::Local);
                break;
        }
    }

    g_gizmos->ProcessInput();
    if (g_scenePicker)
    {
        g_scenePicker->ValidateSelection();
    }
    Selection::UpdateGizmoBinding();
    g_scene->UpdateWorldTransforms();
    Selection::UpdateBoxAreaGizmoBinding();
    Races::Update();
    g_objectRuntime->Update();
    g_vehicleRuntime->Update();
    g_pickupRuntime->Update();
    g_actorRuntime->Update();
    g_gizmos->Render();
}

struct MapStudioPlugin
{
    MapStudioPlugin()
    {
        plugin::Events::shutdownRwEvent.before += []
        {
            StudioInfo::Shutdown();
            FontPreviewAtlas::Shutdown();
            ReleaseMaterialTextRenderer();
            if (g_gui)
            {
                g_gui->ShutdownUserData();
            }
        };
        plugin::Events::d3dLostEvent.after += []
        {
            ReleaseMaterialTextRenderer();
            ModelThumbnails::Shutdown();
            ModelPreview::OnDeviceLost();
        };
        plugin::Events::drawingEvent.after += []
        {
            if (g_freeCamera)
            {
                g_freeCamera->SetVisualWorldProjectionPass(false);
                g_freeCamera->ApplyVisualNavigationRender();
            }
            if (g_collisionRenderer)
            {
                g_collisionRenderer->Render();
            }
            if (g_outlineRenderer)
            {
                g_outlineRenderer->Render();
            }
            UpdateAfter();
            if (g_freeCamera)
            {
                g_freeCamera->EndVisualNavigationRender();
            }
            ModelPreview::UpdateFrame(g_gameHooks ? g_gameHooks->m_device : nullptr);
        };
        plugin::Events::renderSceneEvent.before += []
        {
            Selection::BeginRenderSelection();
            if (g_freeCamera && g_freeCamera->m_active)
            {
                SharedPrivateRender::InvalidateGeometryBindings();
            }
            if (g_freeCamera)
            {
                g_freeCamera->BeginVisualNavigationRender();
                g_freeCamera->SetVisualWorldProjectionPass(true);
            }
        };
        plugin::Events::renderSceneEvent.after += []
        {
            Selection::EndRenderSelection();
        };
        plugin::Events::renderEffectsEvent.before += []
        {
            Races::Render();
        };
        plugin::Events::renderEffectsEvent.after += []
        {
            if (g_freeCamera)
            {
                g_freeCamera->SetVisualWorldProjectionPass(false);
            }
        };
        plugin::Events::processScriptsEvent.before += []
        {
            if (g_gui)
            {
                g_gui->UpdateUtilityModifiers();
            }
        };
    }
} g_plugin;
