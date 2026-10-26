// Source Repository: https://github.com/waverso/waversomapstudio

namespace Theme
{
    namespace Native
    {
        constexpr float Pad = 8.0f;
        constexpr float PopUpPad = 6.0f;
        constexpr float OuterPad = 10.0f;
        constexpr float Spacing = 10.0f;
        constexpr float GroupGap = 7.0f;
        constexpr float IconSz = 15.0f;
        constexpr float IconSzSmall = 11.5f;
        constexpr float FrameH = 24.0f;
        constexpr float HeaderH = 28.0f;
        constexpr float TabH = 31.0f;
        constexpr float GripV = 7.0f;
        constexpr float GripH = 8.0f;
        constexpr float PropLabelW = 79.0f;
        constexpr float AccentSz = 2.0f;
        constexpr float SidebarMinWidth = 328.0f;
    }

    float g_padding = Native::Pad;
    float g_popupPadding = Native::PopUpPad;
    float g_outerPadding = Native::OuterPad;
    float g_itemSpacing = Native::Spacing;
    float g_groupSpacing = Native::GroupGap;
    float g_iconSize = Native::IconSz;
    float g_smallIconSize = Native::IconSzSmall;
    float g_frameHeight = Native::FrameH;
    float g_headerHeight = Native::HeaderH;
    float g_tabHeight = Native::TabH;
    float g_verticalGrip = Native::GripV;
    float g_horizontalGrip = Native::GripH;
    float g_propertyLabelWidth = Native::PropLabelW;
    float g_accentSize = Native::AccentSz;

    void UpdateMetrics()
    {
        g_padding = Ui::Px(Native::Pad);
        g_popupPadding = Ui::Px(Native::PopUpPad);
        g_outerPadding = Ui::Px(Native::OuterPad);
        g_itemSpacing = Ui::Px(Native::Spacing);
        g_groupSpacing = Ui::Px(Native::GroupGap);
        g_iconSize = Ui::Px(Native::IconSz);
        g_smallIconSize = Ui::Px(Native::IconSzSmall);
        g_frameHeight = Ui::Px(Native::FrameH);
        g_headerHeight = Ui::Px(Native::HeaderH);
        g_tabHeight = Ui::Px(Native::TabH);
        g_verticalGrip = Ui::Px(Native::GripV);
        g_horizontalGrip = Ui::Px(Native::GripH);
        g_propertyLabelWidth = Ui::Px(Native::PropLabelW);
        g_accentSize = Ui::Px(Native::AccentSz);
    }
}

namespace
{
    static ImGuiStyle g_baseStyle;
    static bool g_baseStyleReady = false;
    static Theme::Index g_appliedThemeIndex = Theme::Index::Dark;
    static Theme::BaseColors g_appliedThemeColors = Theme::Presets[0];

    static void UpdateLayoutMetrics();

    static void ApplyCustomStyleMetrics()
    {
        ImCustom::Style cs;
        cs.frameHeight = Theme::g_frameHeight;
        cs.fieldPadX = Theme::g_popupPadding;
        cs.itemPadX = Theme::g_popupPadding / 2.0f;
        cs.popupPad = Theme::g_popupPadding / 2.0f;
        cs.popupSpacing = Theme::g_popupPadding;
        cs.maxVisibleItems = 10;
        cs.boxBg = Theme::g_controlBackground;
        cs.boxBgHovered = Theme::g_hoveredControlBackground;
        cs.boxBorder = Theme::g_border;
        cs.boxBorderHovered = Theme::g_borderStrong;
        cs.boxShadow = Theme::g_borderShadow;
        cs.text = Theme::g_text;
        cs.textCurrent = Theme::g_textDim;
        cs.itemHovered = Theme::g_hoveredControlBackground;
        cs.itemCurrentBg = Theme::g_alternatePanelBackground;
        cs.buttonSelected = Theme::g_success;
        cs.buttonSelectedHovered = Theme::g_successHover;
        cs.buttonSelectedActive = Theme::g_successActive;
        cs.buttonSelectedText = Theme::g_statusText;
        ImCustom::SetStyle(cs);
    }

    static void ApplyScaledMetrics()
    {
        UpdateLayoutMetrics();
        if (g_baseStyleReady)
        {
            ImGuiStyle scaled = g_baseStyle;
            scaled.ScaleAllSizes(Ui::Scale());
            scaled.MouseCursorScale = 1.0f;
            scaled.PopupBorderSize = Ui::Hairline(1.0f);
            scaled.FrameBorderSize = Ui::Hairline(1.0f);
            ImGui::GetStyle() = scaled;
        }
        ApplyCustomStyleMetrics();
    }

    inline float FrameTextPadY()
    {
        return floorf((Theme::g_frameHeight - ImGui::GetFontSize()) * 0.5f);
    }

    inline float ScaledFontSize(ImFont* font)
    {
        if (!font || font == ImGui::GetFont())
        {
            return ImGui::GetFontSize();
        }
        ImGui::PushFont(font);
        const float size = ImGui::GetFontSize();
        ImGui::PopFont();
        return size;
    }
}

void Gui::ApplyTheme()
{
    g_appliedThemeColors = Theme::Colors(Theme::g_selectedIndex);
    Theme::Apply(g_appliedThemeColors);
    g_appliedThemeIndex = Theme::g_selectedIndex;
    ImGuiStyle& style = ImGui::GetStyle();
    if (g_baseStyleReady)
    {
        style = g_baseStyle;
    }

    style.ScrollbarSize = 7.0f;
    style.ScrollbarRounding = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.GrabRounding = 0.0f;

    auto C = [](ImU32 c)
    {
        return ImGui::ColorConvertU32ToFloat4(c);
    };
    ImVec4* col = style.Colors;

    col[ImGuiCol_WindowBg] = C(Theme::g_windowBackground);
    col[ImGuiCol_ChildBg] = C(Theme::g_panelBackground);
    col[ImGuiCol_PopupBg] = C(Theme::g_windowBackground);
    col[ImGuiCol_Border] = C(Theme::g_border);
    col[ImGuiCol_BorderShadow] = C(Theme::g_borderShadow);
    col[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.45f);

    col[ImGuiCol_FrameBg] = C(Theme::g_controlBackground);
    col[ImGuiCol_FrameBgHovered] = C(Theme::g_hoveredControlBackground);
    col[ImGuiCol_FrameBgActive] = C(Theme::g_activeControlBackground);

    col[ImGuiCol_Button] = C(Theme::g_controlBackground);
    col[ImGuiCol_ButtonHovered] = C(Theme::g_hoveredControlBackground);
    col[ImGuiCol_ButtonActive] = C(Theme::g_activeControlBackground);

    col[ImGuiCol_Header] = C(Theme::g_headerBackground);
    col[ImGuiCol_HeaderHovered] = C(Theme::g_hoveredHeaderBackground);
    col[ImGuiCol_HeaderActive] = C(Theme::g_activeControlBackground);

    col[ImGuiCol_Separator] = C(Theme::g_border);
    col[ImGuiCol_SeparatorHovered] = C(Theme::g_accent);
    col[ImGuiCol_SeparatorActive] = C(Theme::g_accentHover);

    col[ImGuiCol_ResizeGrip] = C(Theme::g_border);
    col[ImGuiCol_ResizeGripHovered] = C(Theme::g_accent);
    col[ImGuiCol_ResizeGripActive] = C(Theme::g_accentHover);

    col[ImGuiCol_Text] = C(Theme::g_text);
    col[ImGuiCol_TextDisabled] = C(Theme::g_textDim);
    col[ImGuiCol_TextSelectedBg] = C(Theme::g_accentSoft);

    col[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    col[ImGuiCol_ScrollbarGrab] = C(Theme::g_hoveredControlBackground);
    col[ImGuiCol_ScrollbarGrabHovered] = C(Theme::g_activeControlBackground);
    col[ImGuiCol_ScrollbarGrabActive] = C(Theme::g_accentMuted);

    col[ImGuiCol_CheckMark] = C(Theme::g_accent);
    col[ImGuiCol_SliderGrab] = C(Theme::g_isLight ? Theme::ControlFill(Theme::g_accent, 0.25f) : Theme::g_accent);
    col[ImGuiCol_SliderGrabActive] = C(Theme::g_isLight ? Theme::ControlFill(Theme::g_accentHover, 0.4f) : Theme::g_accentHover);

    col[ImGuiCol_TitleBg] = C(Theme::g_windowBackground);
    col[ImGuiCol_TitleBgActive] = C(Theme::g_windowBackground);
    col[ImGuiCol_TitleBgCollapsed] = C(Theme::g_windowBackground);
    col[ImGuiCol_MenuBarBg] = C(Theme::g_alternatePanelBackground);

    col[ImGuiCol_Tab] = C(Theme::g_headerBackground);
    col[ImGuiCol_TabHovered] = C(Theme::g_hoveredHeaderBackground);
    col[ImGuiCol_TabActive] = C(Theme::g_activeControlBackground);
    col[ImGuiCol_TabUnfocused] = C(Theme::g_headerBackground);
    col[ImGuiCol_TabUnfocusedActive] = C(Theme::g_hoveredControlBackground);

    col[ImGuiCol_NavHighlight] = C(Theme::g_accent);
    col[ImGuiCol_NavWindowingHighlight] = C(Theme::g_accentSoft);
    col[ImGuiCol_DragDropTarget] = C(Theme::g_accent);

    g_baseStyle = style;
    g_baseStyleReady = true;
    ApplyScaledMetrics();
}
