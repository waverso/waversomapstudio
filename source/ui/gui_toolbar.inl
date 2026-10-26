// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    static const char* ToolbarLabels[]
        = { "New", "Open", "Save", "Save As", "Import", "Export", "Undo", "Redo", "Add", "Clone", "Delete", "Help" };
    static const char* ToolbarTooltips[] = { "New project (Ctrl+N)", "Open project (Ctrl+O)", "Save project (Ctrl+S)",
        "Save project as (Ctrl+Shift+S)", "Import Pawn code from clipboard (Ctrl+I)", "Export project to Pawn (Ctrl+E)",
        "Undo (Ctrl+Z)", "Redo (Ctrl+Y)", "Add (Insert)", "Clone (Ctrl+C)", "Delete (Del)", "Help (F1)" };
    static constexpr ProjectCommand ProjectCommands[] = { ProjectCommand::New, ProjectCommand::Open, ProjectCommand::Save,
        ProjectCommand::SaveAs, ProjectCommand::Import, ProjectCommand::Export };
    static constexpr int ToolbarGroupEnds[] = { 4, 6, 8, 11, static_cast<int>(std::size(ToolbarLabels)) };
    static constexpr int ToolbarGroupCount = static_cast<int>(std::size(ToolbarGroupEnds));
    static float g_toolbarButtonWidths[std::size(ToolbarLabels)];
    static int g_toolbarLabelMode = 0;

    static bool ToolbarLabelVisible(int button)
    {
        return g_toolbarLabelMode < ToolbarGroupCount && button < ToolbarGroupEnds[ToolbarGroupCount - g_toolbarLabelMode - 1];
    }

    static bool ToolbarGroupSeparator(int button)
    {
        return std::find(std::begin(ToolbarGroupEnds), std::end(ToolbarGroupEnds), button) != std::end(ToolbarGroupEnds);
    }

    static void UpdateToolbarLayout(float sidebarRatio)
    {
        const auto display = ImGui::GetIO().DisplaySize;
        const float right = g_gizmos ? g_gizmos->GetAxisIndicatorLeft() : display.x * (1.0f - sidebarRatio) - Ui::Px(136);
        const float available = right - Theme::g_outerPadding * 2;
        float width = 0;
        for (g_toolbarLabelMode = 0; g_toolbarLabelMode <= ToolbarGroupCount; ++g_toolbarLabelMode)
        {
            width = Theme::g_outerPadding * 2;
            for (int i = 0; i < std::size(ToolbarLabels); ++i)
            {
                g_toolbarButtonWidths[i] = ToolbarLabelVisible(i)
                    ? Theme::g_iconSize + Theme::g_padding * 3 + ImGui::CalcTextSize(ToolbarLabels[i]).x
                    : Theme::g_frameHeight;
                width += g_toolbarButtonWidths[i];
                if (i > 0)
                {
                    width += ToolbarGroupSeparator(i) ? Theme::g_popupPadding * 2 : Theme::g_popupPadding;
                }
            }
            if (width <= available || g_toolbarLabelMode == ToolbarGroupCount)
            {
                break;
            }
        }
        const ImVec2 origin(Theme::g_outerPadding, Theme::g_outerPadding);
        g_toolbarRect
            = ImRect(origin, origin + ImVec2(width, Theme::g_accentSize + Theme::g_outerPadding * 2 + Theme::g_frameHeight));
    }
}

bool Gui::IsMouseOverToolbar() const
{
    if (!m_initialized || !g_freeCamera || !g_freeCamera->m_active || !ImGui::GetCurrentContext() || !RsGlobal.ps)
    {
        return false;
    }
    POINT cursor;
    if (!GetCursorPos(&cursor) || !ScreenToClient(RsGlobal.ps->window, &cursor))
    {
        return false;
    }
    return g_toolbarRect.Contains(ImVec2(static_cast<float>(cursor.x), static_cast<float>(cursor.y)));
}

void Gui::DrawToolbar()
{
    if (!g_scene || !g_icons || !g_sceneActions)
    {
        return;
    }
    UpdateToolbarLayout(GetSidebarVisualWidthRatio());
    const bool blocked = IsAnyModalOpen() || SampTextInputActive();
    const bool changed = Projects::IsDirty() || PendingProjectEdit();
    const bool hasItems = g_scene->count > 0;
    const bool enabled[] = { Projects::IsNamed() || changed, true, changed, true, g_scene->count < EditorScene::MaxItems,
        hasItems, ActionHistoryCanUndo(), ActionHistoryCanRedo(), g_scene->count < EditorScene::MaxItems,
        GuiCanCloneUnifiedSelection(), Selection::TotalCount() > 0, true };
    ImTextureID icons[] = { g_icons->newTexture, g_icons->openTexture, g_icons->saveTexture, g_icons->saveTexture,
        g_icons->importTexture, g_icons->exportTexture, g_icons->undoTexture, g_icons->undoTexture, g_icons->addTexture,
        g_icons->copyTexture, g_icons->deleteTexture, g_icons->helpTexture };
    const std::string projectLabel = Projects::Name() + (changed ? "*" : "");
    const ImVec2 labelPosition(g_toolbarRect.Min.x, g_toolbarRect.Max.y + Theme::g_popupPadding);
    ImFont* labelFont = m_fontSmall ? m_fontSmall : ImGui::GetFont();
    const float shadowOffset = Ui::Hairline();
    ImGui::GetBackgroundDrawList()->AddText(labelFont, ScaledFontSize(labelFont),
        ImVec2(labelPosition.x + shadowOffset, labelPosition.y + shadowOffset), IM_COL32(0, 0, 0, 160), projectLabel.c_str());
    ImGui::GetBackgroundDrawList()->AddText(
        labelFont, ScaledFontSize(labelFont), labelPosition, IM_COL32_WHITE, projectLabel.c_str());
    ImGui::SetNextWindowPos(g_toolbarRect.Min);
    ImGui::SetNextWindowSize(g_toolbarRect.GetSize());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::g_panelBackground);
    ImGui::Begin("##ProjectPanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 panelMin = ImGui::GetWindowPos();
    const ImVec2 panelMax = panelMin + ImGui::GetWindowSize();
    draw->AddRect(panelMin, panelMax, Theme::g_border, 0, 0, Ui::Hairline());
    draw->AddRectFilled(panelMin, ImVec2(panelMax.x, panelMin.y + Theme::g_accentSize), Theme::g_accent);
    float x = Theme::g_outerPadding;
    const float y = Theme::g_accentSize + Theme::g_outerPadding;
    for (int i = 0; i < std::size(ToolbarLabels); ++i)
    {
        if (i > 0)
        {
            const bool separator = ToolbarGroupSeparator(i);
            const float gap = separator ? Theme::g_popupPadding * 2 : Theme::g_popupPadding;
            if (separator)
            {
                const float sx = g_toolbarRect.Min.x + x + gap * 0.5f;
                draw->AddLine(ImVec2(sx, g_toolbarRect.Min.y + y + Theme::g_padding),
                    ImVec2(sx, g_toolbarRect.Min.y + y + Theme::g_frameHeight - Theme::g_padding), Theme::g_border,
                    Ui::Hairline());
            }
            x += gap;
        }
        ImGui::SetCursorPos(ImVec2(x, y));
        ImGui::PushID(i);
        ImGui::BeginDisabled(blocked || !enabled[i]);
        const bool clicked = ImGui::Button("##Command", ImVec2(g_toolbarButtonWidths[i], Theme::g_frameHeight));
        const auto minimum = ImGui::GetItemRectMin();
        const float iconX = ToolbarLabelVisible(i) ? Theme::g_padding : (g_toolbarButtonWidths[i] - Theme::g_iconSize) * 0.5f;
        const ImVec2 iconMin(minimum.x + iconX, minimum.y + (Theme::g_frameHeight - Theme::g_iconSize) * 0.5f);
        const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
        draw->AddImage(icons[i], iconMin, iconMin + ImVec2(Theme::g_iconSize, Theme::g_iconSize),
            i == 7 ? ImVec2(1, 0) : ImVec2(0, 0), i == 7 ? ImVec2(0, 1) : ImVec2(1, 1), color);
        if (ToolbarLabelVisible(i))
        {
            draw->AddText(ImVec2(iconMin.x + Theme::g_iconSize + Theme::g_padding,
                              minimum.y + (Theme::g_frameHeight - ImGui::GetFontSize()) * 0.5f),
                color, ToolbarLabels[i]);
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        {
            SetStyledTooltip(ToolbarTooltips[i]);
        }
        if (clicked)
        {
            if (i < std::size(ProjectCommands))
            {
                QueueProjectCommand(ProjectCommands[i]);
            }
            else if (i == 6)
            {
                if (FlushProjectEdit())
                {
                    ActionHistoryUndo();
                }
            }
            else if (i == 7)
            {
                if (FlushProjectEdit())
                {
                    ActionHistoryRedo();
                }
            }
            else if (i == 8)
            {
                OpenQuickAddModal();
            }
            else if (i == 9)
            {
                GuiCloneUnifiedSelection();
            }
            else if (i == 10)
            {
                Selection::DeleteUnifiedSelection();
            }
            else if (i == 11 && GuiFinishPropertyEdit())
            {
                ImGui::ClearActiveID();
                g_helpOpen = true;
            }
        }
        ImGui::PopID();
        x += g_toolbarButtonWidths[i];
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
