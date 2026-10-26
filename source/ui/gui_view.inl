// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    struct ViewBookmark
    {
        std::uint64_t id = 0;
        char name[64];
        CVector pos;
        float yaw;
        float pitch;
        int zone;
    };

    struct ViewUiState
    {
        bool vpOpen = true;
        bool locOpen = true;
        bool bmOpen = true;
        Vec3State posState;
        Vec3State lookState;
        int placesMode = 0;
        char locSearch[64] = {};
        bool locFocused = false;
        char bmSearch[64] = {};
        bool bmFocused = false;
        std::vector<ViewBookmark> bookmarks;
        std::uint64_t bookmarkFocusId = 0;
        bool bookmarkFocusPending = false;
        bool bookmarkAreaFocusPending = false;
        int renameIndex = -1;
        bool focusRename = false;
        char renameBuf[64] = {};
        int popupIndex = -1;
        bool openPopup = false;
        bool bmScrollPending = false;
        bool bmWasAtBottom = false;
    };
    static ViewUiState g_viewUi;
    static std::uint64_t g_nextBookmarkId = 1;

#include "gui_view_bookmarks.inl"

    static void DrawViewViewpoint(ViewUiState& ui, ImDrawList* dl, float innerW, float rowPadY)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, rowPadY));

        const CVector pos = g_freeCamera->m_position;
        CVector fwd, right, up;
        g_freeCamera->GetCameraVectors(fwd, right, up);
        const CVector lookAt = pos + fwd;

        float posEdit[3] = { pos.x, pos.y, pos.z };
        float lookEdit[3] = { lookAt.x, lookAt.y, lookAt.z };

        DrawLabel(dl, "Pos");
        DrawVec3Field("##view_pos", ui.posState, posEdit, innerW, true);
        if (posEdit[0] != pos.x || posEdit[1] != pos.y || posEdit[2] != pos.z)
        {
            g_freeCamera->m_position = CVector(posEdit[0], posEdit[1], posEdit[2]);
            g_freeCamera->m_focus = {};
        }

        DrawLabel(dl, "Look At");
        DrawVec3Field("##view_look", ui.lookState, lookEdit, innerW, true);
        if (lookEdit[0] != lookAt.x || lookEdit[1] != lookAt.y || lookEdit[2] != lookAt.z)
        {
            g_freeCamera->SetLookAt(CVector(lookEdit[0], lookEdit[1], lookEdit[2]));
        }

        ImGui::PopStyleVar();
        ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
        if (IconLabelButton("##view_copy", "Copy Camera Code", g_icons->copyTexture, innerW - Theme::g_propertyLabelWidth))
        {
            char px[32], py[32], pz[32], lx[32], ly[32], lz[32];
            SmartFormat(px, sizeof(px), pos.x);
            SmartFormat(py, sizeof(py), pos.y);
            SmartFormat(pz, sizeof(pz), pos.z);
            SmartFormat(lx, sizeof(lx), lookAt.x);
            SmartFormat(ly, sizeof(ly), lookAt.y);
            SmartFormat(lz, sizeof(lz), lookAt.z);

            char clip[256];
            snprintf(clip, sizeof(clip),
                "SetPlayerCameraPos(playerid, %s, %s, %s);\nSetPlayerCameraLookAt(playerid, %s, %s, %s);", px, py, pz, lx, ly,
                lz);
            ImGui::SetClipboardText(clip);
            g_toast.ShowInfo("Camera code copied to the clipboard.");
        }
    }

#include "gui_view_teleport.inl"

}

void Gui::DrawView()
{
    ViewUiState& ui = g_viewUi;

    ImGui::BeginChild("##View", ImGui::GetContentRegionAvail(), false);

    const float fullW = ImGui::GetContentRegionAvail().x;
    const float innerW = fullW - Theme::g_padding * 2.0f;

    ImGui::SetCursorPos(ImVec2(Theme::g_padding, Theme::g_padding));
    ImGui::BeginChild("##ViewContent", ImVec2(innerW + Theme::g_padding, 0.0f), false);
    ImGuiWindow* viewContentWindow = ImGui::GetCurrentWindow();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, Theme::g_padding));

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float rowPadY = Theme::g_padding * 0.5f;

        if (DrawViewSectionHeader("hdr_viewpoint", "Viewpoint", &ui.vpOpen, innerW))
        {
            DrawViewViewpoint(ui, dl, innerW, rowPadY);
        }

        if (DrawViewSectionHeader("hdr_mapregions", "Teleport", &ui.locOpen, innerW))
        {
            DrawViewMapRegions(ui, innerW);
        }

        const float bookmarkTop = ImGui::GetCursorScreenPos().y;
        if (DrawViewSectionHeader("hdr_bookmarks", "Bookmarks", &ui.bmOpen, innerW))
        {
            DrawViewBookmarks(ui, innerW);
        }
        const float bookmarkBottom = ImGui::GetCursorScreenPos().y;

        if (ui.bookmarkAreaFocusPending)
        {
            const ImRect visible = viewContentWindow->InnerClipRect;
            float target = viewContentWindow->Scroll.y;
            if (bookmarkTop < visible.Min.y)
            {
                target += bookmarkTop - visible.Min.y;
            }
            else if (bookmarkBottom > visible.Max.y)
            {
                target += bookmarkBottom - visible.Max.y;
            }
            ImGui::SetScrollY(ImMax(0.0f, target));
            ui.bookmarkAreaFocusPending = false;
        }
    }

    ImGui::Spacing();
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::EndChild();
}
