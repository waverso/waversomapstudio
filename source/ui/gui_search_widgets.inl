// Source Repository: https://github.com/waverso/waversomapstudio

static bool TextInput(const char* id, const char* hint, char* buf, size_t bufSize, float w, ImGuiInputTextFlags flags = 0)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
    bool changed = ImGui::InputTextEx(id, hint, buf, (int)bufSize, ImVec2(w, Theme::g_frameHeight), flags);
    ImGui::PopStyleVar();
    return changed;
}

static bool TextInputIcon(const char* id, const char* hint, char* buf, size_t bufSize, float w, ImTextureID icon,
    ImU32 iconColor = Theme::g_textDim, ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_iconSize + Theme::g_padding * 2.0f, FrameTextPadY()));
    bool changed = ImGui::InputTextEx(id, hint, buf, (int)bufSize, ImVec2(w, Theme::g_frameHeight), flags);
    ImGui::PopStyleVar();

    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    DrawIcon(ImGui::GetWindowDrawList(), icon, ImVec2(mn.x + Theme::g_padding, mn.y + (mx.y - mn.y - Theme::g_iconSize) * 0.5f),
        Theme::g_iconSize, iconColor);
    return changed;
}

struct ComboSearch
{
    char text[64] = {};

    bool Draw()
    {
        if (ImGui::IsWindowAppearing())
        {
            text[0] = '\0';
        }
        const auto* window = ImGui::GetCurrentWindow();
        const float rightEdge = window->ScrollbarY ? window->InnerClipRect.Max.x : window->WorkRect.Max.x;
        const bool changed = TextInputIcon("##combo_search", "Search", text, sizeof(text),
            ImMax(0.0f, rightEdge - window->DC.CursorPos.x), g_icons->searchTexture);
        return changed || ImGui::IsWindowAppearing();
    }

    bool Matches(const char* label) const
    {
        return TextSearch::ContainsAsciiNoCase(label, text);
    }
};

static bool SearchInput(const char* id, const char* hint, char* buf, size_t bufSize, float w, bool& ioFocused)
{
    const bool prevFocused = ioFocused;
    const bool hasText = buf[0] != '\0';
    bool clearClicked = false;

    if (!prevFocused && hasText)
    {
        const ImVec2 preMn = ImGui::GetCursorScreenPos();
        const ImVec2 preMx = ImVec2(preMn.x + w, preMn.y + Theme::g_frameHeight);
        const ImVec2 hitMin(preMx.x - Ui::Px(24.0f), preMn.y);
        if (ImGui::IsMouseHoveringRect(hitMin, preMx) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            buf[0] = '\0';
            clearClicked = true;
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_iconSize + Theme::g_padding * 2.0f, FrameTextPadY()));
    bool changed
        = ImGui::InputTextEx(id, hint, buf, (int)bufSize, ImVec2(w, Theme::g_frameHeight), ImGuiInputTextFlags_AutoSelectAll);
    ImGui::PopStyleVar();

    if (clearClicked)
    {
        ImGui::ClearActiveID();
        changed = true;
    }

    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    ioFocused = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawIcon(dl, g_icons->searchTexture, ImVec2(mn.x + Theme::g_padding, mn.y + (mx.y - mn.y - Theme::g_iconSize) * 0.5f),
        Theme::g_iconSize, Theme::g_textDim);

    if (!ioFocused && buf[0] != '\0')
    {
        const float hitWidth = Ui::Px(24.0f);
        const float inset = Ui::Px(4.0f);
        const ImVec2 rbMin(mx.x - hitWidth, mn.y + inset);
        const ImVec2 rbMax(mx.x - inset, mx.y - inset);
        const bool hov = ImGui::IsMouseHoveringRect(ImVec2(mx.x - hitWidth, mn.y), mx);
        if (hov)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
        }
        const ImVec2 center(floorf((rbMin.x + rbMax.x) * 0.5f), floorf((rbMin.y + rbMax.y) * 0.5f));
        DrawIconRotated45Centered(
            dl, g_icons->addTexture, center, Theme::g_iconSize, hov ? Theme::g_dangerHover : Theme::g_textRemove);
    }
    return changed;
}
