// Source Repository: https://github.com/waverso/waversomapstudio

#include "gui.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <functional>
#include <iterator>
#include <memory>
#include <new>
#include <numbers>
#include <optional>
#include <random>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../shared/skin_instance.h"
#include "../utils/text_search.h"
#include "../utils/utils.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_impl_dx9.h"
#include "../vendor/imgui/imgui_impl_win32.h"
#include "../vendor/imgui/imgui_internal.h"
#include "CBaseModelInfo.h"
#include "CClock.h"
#include "CColPoint.h"
#include "CEntity.h"
#include "CMenuManager.h"
#include "CModelInfo.h"
#include "CStreaming.h"
#include "CWeather.h"
#include "CWorld.h"
#include "../scene/actions.h"
#include "../actors/actor_runtime.h"
#include "../actors/animation_browser.h"
#include "../rendering/collision_renderer.h"
#include "common.h"
#include "../layout/curves.h"
#include "../catalog/database.h"
#include "../scene/entity_raycast.h"
#include "extensions/ScriptCommands.h"
#include "font_preview_atlas.h"
#include "../scene/free_camera.h"
#include "../scene/gizmos.h"
#include "../hooks/game_hooks.h"
#include "icons.h"
#include "imgui_custom.h"
#include "../catalog/interiors.h"
#include "../layout/layout.h"
#include "../objects/model_constraints.h"
#include "../rendering/model_preview.h"
#include "../rendering/model_thumbnails.h"
#include "../objects/object_browser.h"
#include "../objects/object_runtime.h"
#include "list_clipper.h"
#include "outliner_rows.h"
#include "../scene/picker.h"
#include "../pickups/pickup_position.h"
#include "../pickups/pickup_runtime.h"
#include "plugin.h"
#include "../project/projects.h"
#include "../scene/property_edit.h"
#include "../hooks/samp.h"
#include "../scene/scene.h"
#include "../scene/scene_tools.h"
#include "../scene/selection.h"
#include "../actors/skin_browser.h"
#include "../editor/studio_info.h"
#include "../textures/texture_browser.h"
#include "theme.h"
#include "../hooks/game_clock_controller.h"
#include "ui_scale.h"
#include "../editor/user_storage.h"
#include "../vehicles/vehicle_browser.h"
#include "../vehicles/vehicle_runtime.h"
#include "../catalog/zones.h"

#include "gui_cursor.inl"

#include "gui_style.inl"

namespace
{
#include "gui_project_state.inl"

    static bool g_popupClosedOnEscape = false;
    static bool g_helpOpen = false;
    static bool g_helpFocusSearch = false;
    static bool g_studioInfoOpen = false;
    static bool g_updateAvailableOpen = false;
    static bool g_updateAvailablePending = false;
    static bool g_updateCheckAutomatic = false;
    static bool g_themesOpen = false;
    static bool g_themesFocusSearch = false;
    static std::uint32_t g_themeFocusId = 0;
    static ImGuiID g_actorAnimationPopupId = 0;
    static ImRect g_toolbarRect;
    static bool g_bookmarkViewRequest = false;
    struct AddPopupContext
    {
        bool useMouseRay = false;
        CVector rayOrigin = {};
        CVector rayDir = {};
    };
    static void UpdateToolbarLayout(float sidebarRatio);
    static int FindSingleSelectedAddParent();
    static void DrawSceneToolsMenu(int target, SceneTools::DuplicateScope scope);
    static void DrawAddMenus(int childParent, const AddPopupContext& context);
    static void DrawSelectionContextMenuItems(
        int target, SceneTools::DuplicateScope scope, const std::optional<CVector>* scenePoint = nullptr);
    static void OpenQuickAddModal();
    static void QueueProjectCommand(ProjectCommand command);

#include "gui_viewport_state.inl"

#include "gui_input_widgets.inl"

#include "gui_layout_widgets.inl"

#include "gui_search_widgets.inl"

#include "gui_viewport_widgets.inl"

#include "gui_list_widgets.inl"

#include "gui_outliner_state.inl"

    enum PendingModalType
    {
        PendingModalNone = 0,
        PendingModalAddItem,
    };

    struct PendingModalState
    {
        PendingModalType activeType = PendingModalNone;
        bool focusFirst = false;
        char modelBuf[8] = {};
        int contextParent = -1;
        bool useMouseRay = false;
        CVector rayOrigin = {};
        CVector rayDir = {};
    };
    static PendingModalState g_modal;
    static void OpenObjectBrowser(int changeItemId = -1);
    static bool IsObjectBrowserOpen();
    static bool IsVehicleBrowserOpen();
    static bool IsSkinBrowserOpen();
    static void OpenSkinBrowser(int changeItemId = -1);
    static void CloseSkinBrowser();
    static void OpenVehicleBrowser(int changeItemId = -1);
    static bool AddVehicleToScene(int modelId);
    static bool AddActorToScene(int skinId);

#include "gui_modal_widgets.inl"

#include "gui_properties_state.inl"

#include "gui_scene_edit.inl"

#include "gui_section_widgets.inl"

#include "gui_vector_widgets.inl"

#include "gui_quick_add.inl"

#include "gui_browser_state.inl"

#include "gui_browser_paging.inl"

#include "gui_filter_widgets.inl"

#include "gui_component_cards.inl"

#include "gui_browser_catalog.inl"

#include "gui_texture_filters.inl"
#include "gui_texture_details.inl"
#include "gui_texture_browser.inl"

#include "gui_model_preview.inl"

#include "gui_object_filters.inl"
#include "gui_object_details.inl"
#include "gui_object_browser.inl"

    static void HandleEditorShortcuts(bool textInput)
    {
        if (!g_freeCamera || !g_freeCamera->m_active || SampTextInputActive() || IsAnyModalOpen()
            || ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            return;
        }
        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsKeyPressed(ImGuiKey_F1, false) && GuiFinishPropertyEdit())
        {
            ImGui::ClearActiveID();
            g_helpOpen = true;
        }
        else if (!textInput && ImGui::IsKeyPressed(ImGuiKey_F2, false) && g_scene && g_scene->SelectionCount() == 1
            && GuiFinishPropertyEdit())
        {
            BeginOutlinerRename(g_scene->SingleSelectedIndex(), true);
        }
        else if (!textInput && !io.WantCaptureKeyboard && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && !io.KeySuper
            && ImGui::IsKeyPressed(ImGuiKey_P, false) && !g_outlinerUi.dragActive
            && !(g_gizmos && (g_gizmos->IsDragging() || g_gizmos->IsRadiusDragging() || g_gizmos->IsBoxAreaDragging())))
        {
            GuiSelectParentOutliner();
        }
    }

    static void HandleBrowserSearchShortcut(const ImGuiIO& io)
    {
        if (!io.KeyCtrl || io.KeyAlt || !ImGui::IsKeyPressed(ImGuiKey_F, false))
        {
            return;
        }
        if (ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            return;
        }

        if (g_helpOpen)
        {
            g_helpFocusSearch = true;
            return;
        }
        if (g_themesOpen)
        {
            g_themesFocusSearch = true;
            return;
        }
        if (g_objectBrowserUi.open && g_objectBrowserUi.detailModelId < 0 && !g_objectBrowserUi.filtersOpen)
        {
            g_objectBrowserUi.focusSearch = true;
            return;
        }
        if (g_vehicleBrowserUi.open && g_vehicleBrowserUi.detailModelId < 0 && !g_vehicleBrowserUi.filtersOpen)
        {
            g_vehicleBrowserUi.focusSearch = true;
            return;
        }
        if (g_textureBrowserUi.open && g_textureBrowserUi.detailRow < 0 && !g_textureBrowserUi.advOpen)
        {
            g_textureBrowserUi.focusSearch = true;
            return;
        }
        if (g_skinBrowserUi.open && g_skinBrowserUi.detailModelId < 0 && !g_skinBrowserUi.filtersOpen)
        {
            g_skinBrowserUi.focusSearch = true;
            return;
        }
        if (g_projectUi.modal == ProjectModal::Open && !g_projectUi.confirmBeforeOpen)
        {
            g_projectUi.focusSearch = true;
        }
    }

#include "gui_vehicle_browser.inl"

#include "gui_skin_browser.inl"

#include "gui_color_widgets.inl"

#include "gui_animation_browser.inl"

#include "property_inspector.inl"

#include "gui_help.inl"
#include "gui_themes.inl"

}

#include "gui_view.inl"

#include "gui_fonts.inl"

Gui::Gui(IDirect3DDevice9* pDevice)
{
    if (!ImGui::CreateContext())
    {
        return;
    }

    LoadFonts(1.08f);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = io.LogFilename = nullptr;
    ImGuiContext& imguiContext = *ImGui::GetCurrentContext();
    imguiContext.ConfigNavWindowingKeyNext = ImGuiKey_None;
    imguiContext.ConfigNavWindowingKeyPrev = ImGuiKey_None;

    plugin::patch::Nop(0x531155, 5);
    ImGui_ImplWin32_Init(RsGlobal.ps->window);
    ImGui_ImplDX9_Init(pDevice);

    io.Fonts->Build();
    ApplyTheme();
    m_initialized = true;
}

#include "gui_user_data.inl"

Gui::~Gui()
{
    ShutdownUserData();
    if (!m_initialized)
    {
        return;
    }
    CancelDragCursor();
    m_initialized = false;
    ModelThumbnails::Shutdown();
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void Gui::Update()
{
    PollStudioUpdate();
    static bool wasActive = false;
    if (g_freeCamera->m_active)
    {
        wasActive = true;
        Utils::AllowMouseMovement(false);
    }
    else
    {
        CancelDragCursor();
        if (wasActive)
        {
            wasActive = false;
            Utils::AllowMouseMovement(true);
            ImGui::GetIO().MouseDrawCursor = false;
        }
        m_sidebarResizing = false;
        m_sidebarSplitting = false;
        m_sidebarInteracting = false;
    }

    if (FrontEndMenuManager.m_bMenuActive)
    {
        CancelDragCursor();
        m_sidebarResizing = false;
        m_sidebarSplitting = false;
        m_sidebarInteracting = false;
        return;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    const bool prevWantTextInput = io.WantTextInput;
    if (g_appliedThemeIndex != Theme::g_selectedIndex || g_appliedThemeColors != Theme::Colors(Theme::g_selectedIndex))
    {
        ApplyTheme();
    }
    const Ui::UpdateResult uiUpdate = Ui::BeginFrame(io.DisplaySize);
    if (uiUpdate.displayChanged)
    {
        m_fontAtlasRebuildPending = true;
        m_pendingFontRasterizerDensity = Ui::Scale() * 1.08f;
    }
    if (m_fontAtlasRebuildPending && !m_uiScaleDragActive)
    {
        const ULONGLONG now = GetTickCount64();
        if (m_lastFontAtlasRebuildTimeMs == 0 || now - m_lastFontAtlasRebuildTimeMs >= 1000)
        {
            RebuildFontAtlas(m_pendingFontRasterizerDensity);
            m_lastFontAtlasRebuildTimeMs = now;
            m_fontAtlasRebuildPending = false;
        }
    }
    io.FontGlobalScale = Ui::Scale();
    if (uiUpdate.scaleChanged)
    {
        const float scaleRatio = uiUpdate.previousScale > 0.0f ? Ui::Scale() / uiUpdate.previousScale : 1.0f;
        g_textureBrowserUi.previewPan.x *= scaleRatio;
        g_textureBrowserUi.previewPan.y *= scaleRatio;
        g_textureBrowserUi.previewPanTarget.x *= scaleRatio;
        g_textureBrowserUi.previewPanTarget.y *= scaleRatio;
        ApplyScaledMetrics();
    }
    if (uiUpdate.displayChanged)
    {
        m_sidebarResizing = false;
        m_sidebarSplitting = false;
        m_sidebarInteracting = false;
    }
    io.ConfigInputTrickleEventQueue
        = !g_freeCamera->m_active || io.WantCaptureMouse || io.WantCaptureKeyboard || IsDragCursorInputBlocked();
    PrepareDragCursorInput();
    ImGui::NewFrame();

    if (!uiUpdate.displayAvailable)
    {
        CancelDragCursor();
        m_sidebarResizing = false;
        m_sidebarSplitting = false;
        m_sidebarInteracting = false;
        g_popupClosedOnEscape = false;
        ImGui::EndFrame();
        return;
    }
    g_popupClosedOnEscape = ClosePopupOnEscape();
    const bool canceledPropertyEdit = ImGui::IsKeyPressed(ImGuiKey_Escape, false) && g_propertyInspector.session.Active();
    if (canceledPropertyEdit)
    {
        g_propertyInspector.session.Cancel(*g_scene);
        g_scene->UpdateWorldTransforms();
        g_propertyInspector.buffers.clear();
        ImGui::ClearActiveID();
    }
    const bool canceledPropertyName
        = !g_popupClosedOnEscape && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && g_propertyInspector.nameDirty;
    if (canceledPropertyName)
    {
        g_propertyInspector.nameDirty = false;
        g_propertyInspector.buffers.clear();
        ImGui::ClearActiveID();
    }
    m_sidebarVisualWidthRatio = m_sidebarWidthRatio;
    UpdateSidebarInteractionState();
    HandleEditorShortcuts(prevWantTextInput);
    HandleBrowserSearchShortcut(io);

    if (g_freeCamera->m_active && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !prevWantTextInput && !g_helpOpen
        && !g_studioInfoOpen && !g_updateAvailableOpen && !g_themesOpen && g_projectUi.modal == ProjectModal::None
        && !g_popupClosedOnEscape && !canceledPropertyEdit && !canceledPropertyName
        && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
    {
        if (IsSkinBrowserOpen())
        {
            if (g_skinBrowserUi.filtersOpen)
            {
                g_skinBrowserUi.filtersOpen = false;
            }
            else if (g_skinBrowserUi.detailModelId >= 0 && !g_skinBrowserUi.viewOnly)
            {
                ModelPreview::Close();
                g_skinBrowserUi.detailModelId = -1;
            }
            else
            {
                CloseSkinBrowser();
            }
        }
        else if (IsVehicleBrowserOpen())
        {
            if (g_vehicleBrowserUi.filtersOpen)
            {
                g_vehicleBrowserUi.filtersOpen = false;
            }
            else if (g_vehicleBrowserUi.detailModelId >= 0 && !g_vehicleBrowserUi.viewOnly)
            {
                ModelPreview::Close();
                g_vehicleBrowserUi.detailModelId = -1;
            }
            else
            {
                CloseVehicleBrowser();
            }
        }
        else if (g_objectBrowserUi.viewOnly)
        {
            CloseObjectBrowser();
        }
        else if (g_objectBrowserUi.open)
        {
            if (g_objectBrowserUi.filtersOpen)
            {
                g_objectBrowserUi.filtersOpen = false;
            }
            else if (g_objectBrowserUi.detailModelId >= 0)
            {
                ModelPreview::Close();
                g_objectBrowserUi.detailModelId = -1;
            }
            else
            {
                CloseObjectBrowser();
            }
        }
        else if (g_modal.activeType != PendingModalNone)
        {
            g_modal.activeType = PendingModalNone;
        }
        else if (g_textureBrowserUi.open)
        {
            if (g_textureBrowserUi.advOpen)
            {
                g_textureBrowserUi.advOpen = false;
            }
            else if (g_textureBrowserUi.detailRow >= 0)
            {
                g_textureBrowserUi.detailRow = -1;
            }
            else
            {
                CloseTextureBrowser();
            }
        }
        else if (Selection::TotalCount() > 0)
        {
            Selection::ClearAll();
        }
        else
        {
            g_freeCamera->m_active = false;
        }
    }

    const bool overlayActive = IsAnyModalOpen() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

    if (g_freeCamera->m_active && !overlayActive && !prevWantTextInput && !SampTextInputActive() && io.KeyCtrl && !io.KeyAlt)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            QueueProjectCommand(io.KeyShift ? ProjectCommand::SaveAs : ProjectCommand::Save);
        }
        if (!io.KeyShift)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_N, false))
            {
                QueueProjectCommand(ProjectCommand::New);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_O, false))
            {
                QueueProjectCommand(ProjectCommand::Open);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_I, false))
            {
                QueueProjectCommand(ProjectCommand::Import);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_E, false))
            {
                QueueProjectCommand(ProjectCommand::Export);
            }
        }
    }

    if (g_freeCamera->m_active && !overlayActive && !prevWantTextInput && ImGui::IsKeyPressed(ImGuiKey_Insert, false))
    {
        OpenQuickAddModal();
    }

    g_freeCamera->UpdateInput(io);
    UpdateDragCursorInput(io);
    ApplyDragCursorHide();

    if (!overlayActive)
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !(g_gizmos && (g_gizmos->IsDragging() || g_gizmos->IsRadiusDragging() || g_gizmos->IsBoxAreaDragging())))
        {
            GuiFinishPropertyEdit();
        }
        ActionHistoryHandleInput();
        Selection::HandleInput(io);
    }
    g_scenePicker->HandleInput(io);
    Render();
    if (g_propertyInspector.session.Active() && g_propertyInspector.drawFrame != ImGui::GetFrameCount())
    {
        GuiFinishPropertyEdit();
    }
    UpdateDragCursorLock();
    AdvanceDragCursorHide();
    UpdateEditTracker();
    const int curveId = std::exchange(g_pendingCurveLocateId, -1);
    const int curve = g_scene->FindIndexById(curveId);
    if (curve >= 0)
    {
        Selection::ClearAll();
        g_scene->items[curve].selected = true;
        for (int parent = g_scene->items[curve].parent; parent >= 0; parent = g_scene->items[parent].parent)
        {
            g_scene->items[parent].open = true;
        }
        GuiRequestOutlinerScrollTo(curveId);
    }
    const bool editing = ImGui::IsAnyItemActive()
        || (g_gizmos && (g_gizmos->IsDragging() || g_gizmos->IsRadiusDragging() || g_gizmos->IsBoxAreaDragging()));
    try
    {
        Layout::UpdateAutoApply(static_cast<std::uint64_t>(ImGui::GetTime() * 1000.0), editing);
    }
    catch (const std::bad_alloc&)
    {
        g_toast.ShowError("Not enough memory to update the layout.");
    }
    Projects::UpdateDirty();
    UpdateUserData();
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void Gui::Render()
{
    if (!g_freeCamera->m_active)
    {
        return;
    }

    if (g_textureBrowserUi.pendingRelease)
    {
        TextureBrowser::ReleaseAll();
        g_textureBrowserUi.pendingRelease = false;
    }
    TextureBrowser::BeginFrame();
    ObjectBrowser::EnsureIndex();
    ModelThumbnails::BeginFrame(g_gameHooks ? g_gameHooks->m_device : nullptr);
    DrawRightSidebar();

    if (g_gizmos)
    {
        g_gizmos->RenderAxisIndicator();
        g_gizmos->DrawAxisIndicatorLabels();
    }
    DrawGizmoPanel();
    DrawUtilityPanel();
    DrawViewportStatus();
    DrawToolbar();
    DrawProjectModal();
    DrawAppModal();
    DrawTextureBrowser();
    DrawObjectBrowser();
    DrawVehicleBrowser();
    DrawSkinBrowser();
    DrawHelpModal();
    DrawStudioInfoModal();
    DrawThemesModal();
    DrawUpdateAvailableModal();
    if (SampTextInputActive())
    {
        DrawModalBackdrop("##SampInputBackdrop");
    }
    DrawToast();

    if (m_sceneContextMenuPending)
    {
        ImGui::SetNextWindowPos(m_sceneContextMenuPosition, ImGuiCond_Appearing);
        ImGui::OpenPopup("##SceneContextMenu");
        m_sceneContextMenuPending = false;
    }
    if (ImCustom::BeginPopup("##SceneContextMenu"))
    {
        const bool hasSelection = Selection::TotalCount() > 0;

        AddPopupContext context;
        CVector mouseOrigin, mouseDir;
        if (g_freeCamera
            && g_freeCamera->ScreenToWorldRayForPicking(
                m_sceneContextMenuPosition.x, m_sceneContextMenuPosition.y, mouseOrigin, mouseDir))
        {
            context.useMouseRay = true;
            context.rayOrigin = mouseOrigin;
            context.rayDir = mouseDir;
        }

        DrawAddMenus(FindSingleSelectedAddParent(), context);
        DrawSelectionContextMenuItems(g_scene->SelectionCount() == 1 ? g_scene->SingleSelectedIndex() : -1,
            hasSelection ? SceneTools::DuplicateScope::Selection : SceneTools::DuplicateScope::Scene, &m_sceneContextPoint);

        ImCustom::EndPopup();
    }
}

void Gui::QueueSceneContextMenu(const ImVec2& pos)
{
    if (IsDragCursorInputBlocked())
    {
        return;
    }
    m_sceneContextMenuPosition = pos;
    CVector point;
    m_sceneContextPoint = ResolveSceneContextPoint(pos, point) ? std::optional<CVector>(point) : std::nullopt;
    m_sceneContextMenuPending = true;
}

#include "gui_viewport.inl"

#include "gui_sidebar.inl"

namespace
{
#include "gui_scene_menus.inl"
}

#include "gui_outliner.inl"

#include "gui_properties.inl"

float Gui::GetSidebarVisualWidthRatio() const
{
    return m_sidebarVisualWidthRatio;
}

#include "gui_project_browser.inl"
#include "gui_project_commands.inl"
#include "gui_toolbar.inl"
#include "gui_project_modals.inl"

void GuiShowToast(const char* msg, bool error)
{
    if (msg == nullptr || msg[0] == '\0')
    {
        return;
    }
    if (error)
    {
        g_toast.ShowError(msg);
    }
    else
    {
        g_toast.ShowInfo(msg);
    }
}
