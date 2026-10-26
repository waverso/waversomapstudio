// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>
#include <d3d9.h>
#include <memory>
#include <optional>

#include "../editor/user_data.h"
#include "../vendor/imgui/imgui.h"
#include "CVector.h"
#include "gui_api.h"

namespace UserData
{
    class Storage;
}

class Gui
{
    public:
    Gui(IDirect3DDevice9* pDevice);
    ~Gui();

    void Render();
    void Update();
    void UpdateUtilityModifiers();
    void LoadUserData();
    void ShutdownUserData();
    void QueueSceneContextMenu(const ImVec2& pos);
    void RequestProperties()
    {
        m_propertiesRequested = true;
    }
    void CancelDragCursor();
    bool IsDragCursorInputBlocked() const;
    bool IsMouseOverGizmoPanel() const;
    bool IsMouseOverUtilityPanel() const;
    bool IsMouseOverToolbar() const;
    float GetSidebarWidthRatio() const
    {
        return m_sidebarWidthRatio;
    }
    float GetSidebarVisualWidthRatio() const;

    float m_sidebarWidthRatio = 0.15f;
    float m_sidebarVisualWidthRatio = 0.15f;
    float m_sidebarResizeProgress = 0.0f;
    bool m_sidebarResizeInitialized = false;
    bool m_uiScaleDragActive = false;
    bool m_fontAtlasRebuildPending = false;
    float m_pendingFontRasterizerDensity = 1.08f;
    ULONGLONG m_lastFontAtlasRebuildTimeMs = 0;
    float m_sidebarOffsetRatio = 0.0f;
    bool m_sidebarInteracting = false;

    ImFont* m_fontRegular = nullptr;
    ImFont* m_fontBold = nullptr;
    ImFont* m_fontSmall = nullptr;

    private:
    UserData::Settings CaptureUserSettings() const;
    void UpdateUserData();
    std::unique_ptr<UserData::Storage> m_userStorage;
    UserData::Settings m_lastUserSettings;
    std::uint32_t m_updateCheckDays = 1;
    std::uint64_t m_lastUpdateCheck = 0;
    std::uint64_t m_favoritesRevision = 0, m_bookmarksRevision = 0;
    std::uint64_t m_themesRevision = 0;
    std::uint32_t m_selectedThemeId = 0;
    std::string m_userDataError;
    bool m_initialized = false;
    void LoadFonts(float rasterizerDensity);
    void RebuildFontAtlas(float rasterizerDensity);
    void ApplyTheme();
    void DrawGizmoPanel();
    void DrawUtilityPanel();
    void DrawViewportStatus();
    void DrawToolbar();
    void DrawProjectModal();
    void UpdateSidebarInteractionState();
    void DrawRightSidebar();

    void DrawOutliner();
    void DrawProperties();
    void DrawView();
    void DrawEditor();

    float m_sidebarSplitRatio = 0.38f;
    bool m_sidebarResizing = false;
    bool m_sidebarSplitting = false;
    float m_sidebarResizeGrabOffset = 0.0f;
    float m_sidebarSplitGrabOffset = 0.0f;
    bool m_sceneContextMenuPending = false;
    bool m_propertiesRequested = false;
    ImVec2 m_sceneContextMenuPosition = {};
    std::optional<CVector> m_sceneContextPoint;
};

extern Gui* g_gui;
