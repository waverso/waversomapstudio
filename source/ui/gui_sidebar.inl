// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    struct SidebarWidthLimits
    {
        float minWidth;
        float maxWidth;
        float minRatio;
        float maxRatio;
    };

    static SidebarWidthLimits CalculateSidebarWidthLimits(float displayWidth)
    {
        displayWidth = std::max(1.0f, displayWidth);
        const float desiredMinWidth = Ui::Px(Theme::Native::SidebarMinWidth);
        const float minRatio = std::clamp(desiredMinWidth / displayWidth, 0.10f, 0.80f);
        const float desiredMaxWidth = desiredMinWidth * 1.75f;
        const float maxRatioLimit = std::max(minRatio, 0.55f);
        const float maxRatio = std::clamp(desiredMaxWidth / displayWidth, minRatio, maxRatioLimit);
        return { displayWidth * minRatio, displayWidth * maxRatio, minRatio, maxRatio };
    }

    static float SidebarRatioFromResizeT(const SidebarWidthLimits& limits, float resizeT)
    {
        return std::lerp(limits.minRatio, limits.maxRatio, std::clamp(resizeT, 0.0f, 1.0f));
    }

    static float SidebarResizeTFromRatio(const SidebarWidthLimits& limits, float ratio)
    {
        const float span = limits.maxRatio - limits.minRatio;
        if (span <= FLT_EPSILON)
        {
            return 0.0f;
        }
        return std::clamp((ratio - limits.minRatio) / span, 0.0f, 1.0f);
    }
}

void Gui::UpdateSidebarInteractionState()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (!g_freeCamera || !g_freeCamera->m_active || g_freeCamera->m_panHasAnchor || display.x <= 0.0f || display.y <= 0.0f)
    {
        m_sidebarResizing = false;
        m_sidebarSplitting = false;
        m_sidebarInteracting = false;
        return;
    }

    const SidebarWidthLimits widthLimits = CalculateSidebarWidthLimits(display.x);
    if (!m_sidebarResizeInitialized)
    {
        m_sidebarResizeProgress = SidebarResizeTFromRatio(widthLimits, m_sidebarWidthRatio);
        m_sidebarResizeInitialized = true;
    }
    m_sidebarResizeProgress = std::clamp(m_sidebarResizeProgress, 0.0f, 1.0f);
    m_sidebarWidthRatio = SidebarRatioFromResizeT(widthLimits, m_sidebarResizeProgress);
    m_sidebarSplitRatio = std::clamp(m_sidebarSplitRatio, 0.30f, 0.70f);

    const float sidebarW = Ui::Round(display.x * m_sidebarWidthRatio);
    const float left = display.x - sidebarW;
    const float visualSidebarW = Ui::Round(display.x * GetSidebarVisualWidthRatio());
    const float visualLeft = display.x - visualSidebarW;
    const float splitY = Ui::Round(display.y * m_sidebarSplitRatio);
    const float handleY = splitY - Theme::g_itemSpacing * 0.5f;
    const float contentW = ImMax(Ui::Hairline(), sidebarW - Theme::g_outerPadding * 2.0f);
    const ImVec2 mouse = ImGui::GetIO().MousePos;

    const bool overWidthSplitter = fabsf(mouse.x - left) < Theme::g_verticalGrip && mouse.y >= 0.0f && mouse.y <= display.y;
    const bool overHeightSplitter = fabsf(mouse.y - handleY) < Theme::g_horizontalGrip && mouse.x >= left + Theme::g_outerPadding
        && mouse.x <= left + Theme::g_outerPadding + contentW;
    const bool overSidebar = mouse.x >= visualLeft && mouse.x <= display.x && mouse.y >= 0.0f && mouse.y <= display.y;
    const bool overGizmoPanel = g_gizmoPanelLayout.Rect(display).Contains(mouse);
    const bool overUtilityPanel = g_utilityPanelLayout.Rect(display, visualSidebarW).Contains(mouse);
    UpdateToolbarLayout(GetSidebarVisualWidthRatio());
    const bool overProjectPanel = g_toolbarRect.Contains(mouse);

    m_sidebarInteracting = m_sidebarResizing || m_sidebarSplitting || overWidthSplitter || overHeightSplitter || overSidebar
        || overGizmoPanel || overUtilityPanel || overProjectPanel || IsAnyModalOpen();
}

void Gui::DrawRightSidebar()
{
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 display = io.DisplaySize;

    if (display.x <= 0.0f || display.y <= 0.0f)
    {
        m_sidebarResizing = false;
        m_sidebarSplitting = false;
        m_sidebarInteracting = false;
        return;
    }

    static int topTab = 0, botTab = 1;
    if (g_outlinerUi.focusRename || g_outlinerUi.showRequested)
    {
        topTab = 0;
        g_outlinerUi.showRequested = false;
    }

    const SidebarWidthLimits widthLimits = CalculateSidebarWidthLimits(display.x);
    m_sidebarResizeProgress = std::clamp(m_sidebarResizeProgress, 0.0f, 1.0f);
    m_sidebarWidthRatio = SidebarRatioFromResizeT(widthLimits, m_sidebarResizeProgress);
    m_sidebarSplitRatio = std::clamp(m_sidebarSplitRatio, 0.30f, 0.70f);

    const float minS = display.y * 0.30f, maxS = display.y * 0.70f;

    float sidebarW = Ui::Round(display.x * m_sidebarWidthRatio);
    float splitY = Ui::Round(display.y * m_sidebarSplitRatio);
    float left = display.x - sidebarW;
    const bool blocked = g_freeCamera->m_panHasAnchor;
    const float visualSidebarRatio = GetSidebarVisualWidthRatio();
    const float visualSidebarW = Ui::Round(display.x * visualSidebarRatio);
    const float visualLeft = display.x - visualSidebarW;

    bool wAct = false;
    {
        float linePos = left;
        wAct = Splitter(true, linePos, display.x - widthLimits.maxWidth, display.x - widthLimits.minWidth, 0.0f, display.y,
            blocked, m_sidebarResizing, m_sidebarResizeGrabOffset, &visualLeft);
        sidebarW = Ui::Round(display.x - linePos);
        m_sidebarResizeProgress = SidebarResizeTFromRatio(widthLimits, sidebarW / display.x);
        m_sidebarWidthRatio = SidebarRatioFromResizeT(widthLimits, m_sidebarResizeProgress);
        left = display.x - sidebarW;
    }

    const float interactionContentW = ImMax(Ui::Hairline(), sidebarW - Theme::g_outerPadding * 2.0f);
    float handleY = splitY - Theme::g_itemSpacing * 0.5f;

    bool sAct = false;
    {
        float linePos = handleY;
        sAct = Splitter(false, linePos, minS - Theme::g_itemSpacing * 0.5f, maxS - Theme::g_itemSpacing * 0.5f,
            left + Theme::g_outerPadding, left + Theme::g_outerPadding + interactionContentW, blocked, m_sidebarSplitting,
            m_sidebarSplitGrabOffset);
        handleY = linePos;
        splitY = Ui::Round(handleY + Theme::g_itemSpacing * 0.5f);
        m_sidebarSplitRatio = std::clamp(splitY / display.y, 0.30f, 0.70f);
    }

    m_sidebarInteracting = m_sidebarInteracting || wAct || sAct;

    const float contentW = ImMax(Ui::Hairline(), visualSidebarW - Theme::g_outerPadding * 2.0f);

    ImGui::SetNextWindowPos(ImVec2(visualLeft, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(visualSidebarW, display.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::g_windowBackground);
    ImGui::Begin("##RightSidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    dl->AddRectFilled(ImVec2(wp.x, wp.y), ImVec2(wp.x + Ui::Hairline(), wp.y + display.y), Theme::g_borderStrong);

    float topY = Theme::g_popupPadding;
    if (g_icons && g_icons->logoTexture)
    {
        const float logoW = ImMax(Ui::Hairline(), ImMin(contentW - Theme::g_padding * 2.0f, Ui::Px(200.0f)));
        const float logoH = logoW * g_icons->logoAspectRatio;
        const ImVec2 logoMin(wp.x + (visualSidebarW - logoW) * 0.5f, wp.y + topY);
        ImGui::SetCursorScreenPos(logoMin);
        if (ImGui::InvisibleButton("##sidebar_logo", ImVec2(logoW, logoH)) && !IsAnyModalOpen() && !SampTextInputActive()
            && GuiFinishPropertyEdit())
        {
            ImGui::ClearActiveID();
            g_studioInfoOpen = true;
        }
        const bool logoHovered = ImGui::IsItemHovered();
        if (logoHovered)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        dl->AddImage(g_icons->logoTexture, logoMin, ImVec2(logoMin.x + logoW, logoMin.y + logoH), ImVec2(0, 0), ImVec2(1, 1),
            ImGui::GetColorU32(logoHovered ? Theme::g_accentHover : Theme::g_accent));
        topY += logoH + Theme::g_popupPadding;
    }

    const float topH = ImMax(Ui::Hairline(), splitY - topY - Theme::g_itemSpacing * 0.5f);
    const float botH = display.y - splitY - Theme::g_outerPadding - Theme::g_itemSpacing * 0.5f;

    ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, topY));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::g_panelBackground);
    ImGui::BeginChild("##TopGroup", ImVec2(contentW, topH), false);
    {
        const char* labels[] = { "Outliner", "Editor" };
        const bool vis[] = { true, true };
        DrawCustomTabs("top", labels, vis, 2, &topTab);

        if (topTab == 0)
        {
            DrawOutliner();
        }
        else
        {
            DrawEditor();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, splitY + Theme::g_itemSpacing * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::g_panelBackground);
    ImGui::BeginChild("##BottomGroup", ImVec2(contentW, botH), false);
    {
        const bool hasSelection = Selection::TotalCount() > 0;
        if (!hasSelection)
        {
            botTab = 1;
        }
        else if (m_propertiesRequested)
        {
            botTab = 0;
        }
        m_propertiesRequested = false;
        if (g_bookmarkViewRequest)
        {
            botTab = 1;
            g_bookmarkViewRequest = false;
        }

        const char* labels[] = { "Properties", "View" };
        const bool vis[] = { true, true };
        const bool enabled[] = { hasSelection, true };
        DrawCustomTabs("bot", labels, vis, 2, &botTab, enabled);

        ImGui::BeginChild("##BottomContent", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
        if (botTab == 0)
        {
            DrawProperties();
        }
        else
        {
            DrawView();
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void Gui::DrawEditor()
{
    static bool sInterfaceOpen = true;
    static bool sCamOpen = true;
    static bool sSelOpen = true;

    ImGui::BeginChild("##Editor", ImGui::GetContentRegionAvail(), false);

    const float fullW = ImGui::GetContentRegionAvail().x;
    const float innerW = fullW - Theme::g_padding * 2.0f;

    ImGui::SetCursorPos(ImVec2(Theme::g_padding, Theme::g_padding));
    ImGui::BeginChild("##EditorContent", ImVec2(innerW + Theme::g_padding, 0.0f), false);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, Theme::g_padding));

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float rowPadY = Theme::g_padding * 0.5f;

        bool resetInterface = false, resetCam = false, resetSel = false;
        if (DrawEditorSectionHeader("hdr_interface", "Interface", &sInterfaceOpen, innerW, &resetInterface))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, rowPadY));

            DrawLabel(dl, "Theme");
            const float themeButtonGap = ImGui::GetStyle().ItemSpacing.y;
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            if (ImCustom::BeginCombo("##interface_theme", Theme::Name(static_cast<unsigned>(Theme::g_selectedIndex)),
                    innerW - Theme::g_propertyLabelWidth - Theme::g_frameHeight - themeButtonGap, true))
            {
                static ComboSearch search;
                ImCustom::BeginComboList(search.Draw());
                for (unsigned option : Theme::g_displayOrder)
                {
                    if (!Theme::g_hidden[option] && search.Matches(Theme::Name(option))
                        && ImCustom::ComboItem(Theme::Name(option), option == static_cast<unsigned>(Theme::g_selectedIndex)))
                    {
                        Theme::g_selectedIndex = static_cast<Theme::Index>(option);
                    }
                }
                ImCustom::EndComboList();
                ImCustom::EndCombo();
            }

            ImGui::SameLine(0, themeButtonGap);
            if (IconSquareButton("##edit_themes", g_icons->editTexture, Theme::g_frameHeight))
            {
                g_themesOpen = true;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                SetStyledTooltip("Edit Themes");
            }

            float uiScale = Ui::UserMultiplier();
            DrawLabel(dl, "UI Scale");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            ImGui::SetNextItemWidth(innerW - Theme::g_propertyLabelWidth);
            const bool uiScaleChanged = ImGui::DragFloat("##ui_scale", &uiScale, 0.004f, Ui::MinUserMultiplier,
                Ui::MaxUserMultiplier, "%.2fx", ImGuiSliderFlags_AlwaysClamp);
            LockMouseCursorForActiveDrag();
            m_uiScaleDragActive = ImGui::IsItemActive();
            if (uiScaleChanged)
            {
                Ui::SetUserMultiplier(uiScale);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                m_pendingFontRasterizerDensity = Ui::CalculateScale(ImGui::GetIO().DisplaySize.y, uiScale) * 1.08f;
                m_fontAtlasRebuildPending = true;
            }

            float gizmoScale = g_gizmos->GetGizmoScale();
            DrawLabel(dl, "Gizmo Scale");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            ImGui::SetNextItemWidth(innerW - Theme::g_propertyLabelWidth);
            if (ImGui::SliderFloat(
                    "##gizmo_scale", &gizmoScale, Gizmos::MinScale, Gizmos::MaxScale, "%.2fx", ImGuiSliderFlags_AlwaysClamp))
            {
                g_gizmos->SetGizmoScale(gizmoScale);
            }

            float indicatorScale = g_gizmos->GetAxisIndicatorScale();
            DrawLabel(dl, "Indic Scale");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            ImGui::SetNextItemWidth(innerW - Theme::g_propertyLabelWidth);
            if (ImGui::SliderFloat("##indicator_scale", &indicatorScale, Gizmos::MinScale, Gizmos::MaxScale, "%.2fx",
                    ImGuiSliderFlags_AlwaysClamp))
            {
                g_gizmos->SetAxisIndicatorScale(indicatorScale);
            }

            DrawLabel(dl, "Veh Color");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            const int pickerStyle = static_cast<int>(g_vehicleColorPickerStyle);
            if (ImCustom::BeginCombo("##vehicle_color_picker_style", VehicleColorPickerStyleNames[pickerStyle],
                    innerW - Theme::g_propertyLabelWidth))
            {
                for (int option = 0; option < 2; ++option)
                {
                    if (ImCustom::ComboItem(VehicleColorPickerStyleNames[option], option == pickerStyle))
                    {
                        g_vehicleColorPickerStyle = static_cast<VehicleColorPickerStyle>(option);
                    }
                }
                ImCustom::EndCombo();
            }

            DrawLabel(dl, "Updates");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            static constexpr unsigned intervals[] = { 0, 1, 7, 30 };
            static constexpr const char* labels[] = { "Never", "Every Day", "Every Week", "Every Month" };
            const auto selected
                = std::find(std::begin(intervals), std::end(intervals), m_updateCheckDays) - std::begin(intervals);
            if (ImCustom::BeginCombo("##update_check_interval", labels[selected], innerW - Theme::g_propertyLabelWidth))
            {
                for (unsigned option = 0; option < std::size(intervals); ++option)
                {
                    if (ImCustom::ComboItem(labels[option], option == selected))
                    {
                        m_updateCheckDays = intervals[option];
                    }
                }
                ImCustom::EndCombo();
            }

            ImGui::PopStyleVar();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rowPadY);
        }

        if (DrawEditorSectionHeader("hdr_cam", "Camera", &sCamOpen, innerW, &resetCam))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, rowPadY));

            DrawLabel(dl, "FOV");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            ImGui::SetNextItemWidth(innerW - Theme::g_propertyLabelWidth);
            ImGui::SliderFloat("##fov", &g_freeCamera->m_targetFovDegrees, FreeCamera::MinFovDegrees, FreeCamera::MaxFovDegrees,
                "%.0f", ImGuiSliderFlags_AlwaysClamp);

            DrawLabel(dl, "Mouse Sens");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            ImGui::SetNextItemWidth(innerW - Theme::g_propertyLabelWidth);
            ImGui::SliderFloat("##mousesens", &g_freeCamera->m_mouseSensitivity, FreeCamera::MinMouseSensitivity,
                FreeCamera::MaxMouseSensitivity, "%.1f", ImGuiSliderFlags_AlwaysClamp);

            DrawLabel(dl, "Base Speed");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            ImGui::SetNextItemWidth(innerW - Theme::g_propertyLabelWidth);
            ImGui::SliderFloat("##basespeed", &g_freeCamera->m_baseSpeed, FreeCamera::MinBaseSpeed, FreeCamera::MaxBaseSpeed,
                "%.0f", ImGuiSliderFlags_AlwaysClamp);

            DrawLabel(dl, "Scroll Speed");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            ImGui::SetNextItemWidth(innerW - Theme::g_propertyLabelWidth);
            ImGui::SliderFloat("##scrollspd", &g_freeCamera->m_scrollSpeed, FreeCamera::MinScrollSpeed,
                FreeCamera::MaxScrollSpeed, "%.0f", ImGuiSliderFlags_AlwaysClamp);

            DrawLabel(dl, "Fast Mult");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            ImGui::SetNextItemWidth(innerW - Theme::g_propertyLabelWidth);
            ImGui::SliderFloat("##shiftmult", &g_freeCamera->m_fastMultiplier, FreeCamera::MinFastMultiplier,
                FreeCamera::MaxFastMultiplier, "%.1fx", ImGuiSliderFlags_AlwaysClamp);

            DrawLabel(dl, "Slow Mult");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            ImGui::SetNextItemWidth(innerW - Theme::g_propertyLabelWidth);
            ImGui::SliderFloat("##altmult", &g_freeCamera->m_slowMultiplier, FreeCamera::MinSlowMultiplier,
                FreeCamera::MaxSlowMultiplier, "%.2fx", ImGuiSliderFlags_AlwaysClamp);

            ImGui::PopStyleVar();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rowPadY);
        }

        if (DrawEditorSectionHeader("hdr_selection", "Selection", &sSelOpen, innerW, &resetSel))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, rowPadY));

            bool drawHighlight = Selection::GetDrawHighlight();
            DrawLabel(dl, "Outline");
            ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
            if (ImGui::Checkbox("##view_drawhighlight", &drawHighlight))
            {
                Selection::SetDrawHighlight(drawHighlight);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                SetStyledTooltip("Draw the outline on selected objects in the world");
            }

            ImGui::PopStyleVar();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rowPadY);
        }

        if (resetInterface)
        {
            Theme::g_selectedIndex = Theme::Index::Dark;
            Ui::SetUserMultiplier(Ui::DefaultUserMultiplier);
            g_gizmos->SetGizmoScale(Gizmos::DefaultScale);
            g_gizmos->SetAxisIndicatorScale(Gizmos::DefaultScale);
            g_vehicleColorPickerStyle = VehicleColorPickerStyle::OklabColumns;
            m_updateCheckDays = UserData::Settings {}.updateCheckDays;
            m_pendingFontRasterizerDensity = Ui::CalculateScale(ImGui::GetIO().DisplaySize.y, Ui::DefaultUserMultiplier) * 1.08f;
            m_fontAtlasRebuildPending = true;
        }

        if (resetCam)
        {
            g_freeCamera->m_targetFovDegrees = FreeCamera::DefaultFovDegrees;
            g_freeCamera->m_mouseSensitivity = FreeCamera::DefaultMouseSensitivity;
            g_freeCamera->m_baseSpeed = FreeCamera::DefaultBaseSpeed;
            g_freeCamera->m_scrollSpeed = FreeCamera::DefaultScrollSpeed;
            g_freeCamera->m_fastMultiplier = FreeCamera::DefaultFastMultiplier;
            g_freeCamera->m_slowMultiplier = FreeCamera::DefaultSlowMultiplier;
        }

        if (resetSel)
        {
            Selection::SetDrawHighlight(true);
        }
    }

    ImGui::Spacing();
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::EndChild();
}
