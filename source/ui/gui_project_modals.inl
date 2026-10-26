// Source Repository: https://github.com/waverso/waversomapstudio

static bool ProjectNameInput(bool exporting)
{
    auto& ui = g_projectUi;
    const char* prefix = exporting ? MOD_NAME "/exports/" : MOD_NAME "/projects/";
    const char* suffix = exporting ? ".pwn" : ".bin";
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float prefixWidth = ImGui::CalcTextSize(prefix).x + Theme::g_padding;
    const float suffixWidth = ImGui::CalcTextSize(suffix).x + Theme::g_padding;
    const float inputWidth = width - prefixWidth - suffixWidth;
    std::string reason;
    const bool wasValid = Projects::ValidName(ui.name, reason);
    ImGui::RenderFrame(position, position + ImVec2(width, Theme::g_frameHeight), ImGui::GetColorU32(ImGuiCol_FrameBg), true);
    ImGui::SetCursorScreenPos(position + ImVec2(prefixWidth, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, wasValid ? Theme::g_text : Theme::g_textRemove);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, FrameTextPadY()));
    auto* draw = ImGui::GetWindowDrawList();
    const int firstVertex = draw->VtxBuffer.Size;
    const ImU32 previousColor = ImGui::GetColorU32(ImGuiCol_Text);
    if (ui.focusName)
    {
        ImGui::SetKeyboardFocusHere();
        ui.focusName = false;
    }
    const bool enter = ImGui::InputTextEx("##Name", nullptr, ui.name, sizeof(ui.name), ImVec2(inputWidth, Theme::g_frameHeight),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    const bool valid = Projects::ValidName(ui.name, reason);
    if (valid != wasValid)
    {
        const ImU32 color = ImGui::GetColorU32(valid ? Theme::g_text : Theme::g_textRemove);
        for (int i = firstVertex; i < draw->VtxBuffer.Size; ++i)
        {
            if (draw->VtxBuffer[i].col == previousColor)
            {
                draw->VtxBuffer[i].col = color;
            }
        }
    }
    const ImU32 fixedColor = ImGui::GetColorU32(valid ? Theme::g_textDim : Theme::g_textRemove);
    const float textY = position.y + FrameTextPadY();
    draw->AddText(ImVec2(position.x + Theme::g_padding, textY), fixedColor, prefix);
    draw->AddText(ImVec2(position.x + width - suffixWidth, textY), fixedColor, suffix);
    return enter;
}

void Gui::DrawProjectModal()
{
    ExecuteProjectCommand();
    auto& ui = g_projectUi;
    if (ui.modal == ProjectModal::None)
    {
        return;
    }
    const char* title = "Project";
    switch (ui.modal)
    {
        case ProjectModal::SaveChanges:
            title = "Unsaved Changes";
            break;
        case ProjectModal::SaveName:
            title = ui.saveAs ? "Save Project As" : "Save Project";
            break;
        case ProjectModal::Open:
            title = "Open Project";
            break;
        case ProjectModal::Import:
            title = "Import Pawn From Clipboard";
            break;
        case ProjectModal::Export:
            title = "Export Pawn Code";
            break;
        case ProjectModal::OverwriteExport:
            title = "Replace Export";
            break;
        case ProjectModal::OverwriteProject:
            title = "Replace Project";
            break;
        case ProjectModal::DeleteProject:
            title = "Delete Project";
            break;
        default:
            break;
    }
    const auto stage = ui.modal;
    const bool enter = !g_popupClosedOnEscape
        && !ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)
        && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
    const auto display = ImGui::GetIO().DisplaySize;
    const float width = floorf(ImMin(Ui::Px(500), display.x - Theme::g_outerPadding * 2));
    const float innerWidth = floorf(width - Theme::g_outerPadding * 2);
    const float listHeight = floorf(ImMin(Ui::Px(240), display.y * 0.4f));
    std::string description;
    const std::string error = stage == ProjectModal::SaveChanges ? ui.error : "";
    float contentHeight = 0;
    int rows = 0;
    const float rowSpacing = stage == ProjectModal::Export ? Theme::g_padding : Theme::g_itemSpacing;
    const auto addRow = [&](float height)
    {
        if (rows++)
        {
            contentHeight += rowSpacing;
        }
        contentHeight += height;
    };
    const auto addText = [&](const std::string& text)
    {
        addRow(ImGui::CalcTextSize(text.c_str(), nullptr, false, innerWidth).y);
    };
    switch (stage)
    {
        case ProjectModal::SaveChanges:
            description = "Save changes to \"" + Projects::Name() + "\" before continuing?";
            addText(description);
            break;
        case ProjectModal::OverwriteExport:
            description = "Replace the existing export \"" + std::string(ui.name) + ".pwn\"?";
            addText(description);
            break;
        case ProjectModal::OverwriteProject:
            description = "Replace the existing project \"" + std::string(ui.name) + "\"?";
            addText(description);
            break;
        case ProjectModal::DeleteProject:
            description = "Delete project \"" + ui.projectToDelete.name + "\"? Its file will be permanently deleted.";
            if (Projects::IsCurrent(ui.projectToDelete))
            {
                description += " The scene will remain as Untitled with unsaved changes.";
            }
            addText(description);
            break;
        case ProjectModal::SaveName:
        case ProjectModal::Export:
            addRow(Theme::g_frameHeight);
            if (stage == ProjectModal::Export)
            {
                addRow(Theme::g_frameHeight);
                addRow(Theme::g_frameHeight);
                addRow(Theme::g_frameHeight);
                description = "The \"" MOD_NAME "\" folder is in the game's root directory.";
                addRow(ImGui::CalcTextSize(description.c_str(), nullptr, false, ImMax(1.0f, innerWidth - Theme::g_padding * 2)).y
                    + Theme::g_popupPadding * 2);
            }
            break;
        case ProjectModal::Open:
            addRow(Theme::g_frameHeight);
            addRow(listHeight);
            break;
        case ProjectModal::Import:
            description = "Import " + std::to_string(ui.imported.items.size())
                + (ui.imported.items.size() == 1 ? " item from the clipboard?" : " items from the clipboard?");
            addText(description);
            break;
        default:
            break;
    }
    if (!error.empty())
    {
        addText(error);
    }
    const float bodyHeight
        = ImMax(1.0f, ImMin(ceilf(contentHeight), floorf(display.y - Theme::g_outerPadding * 2 - ModalHeight(0))));
    DrawModalBackdrop("##ProjectBackdrop");
    ImGui::SetNextWindowPos(display * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, ModalHeight(bodyHeight)), ImGuiCond_Always);
    ImGui::SetNextWindowScroll(ImVec2(0, 0));
    PushModalStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::g_padding, rowSpacing));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
    if (ImGui::Begin("##ProjectModal", nullptr,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoFocusOnAppearing))
    {
        if (!ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        }
        DrawModalTitleBar(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), ImGui::GetWindowWidth(), title);
        ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, Theme::g_tabHeight + Theme::g_padding));
        const int buttonCount = stage == ProjectModal::Open ? 4
            : stage == ProjectModal::SaveChanges || stage == ProjectModal::OverwriteExport
                || stage == ProjectModal::OverwriteProject || stage == ProjectModal::Export
            ? 3
            : 2;
        const float buttonWidth = (innerWidth - Theme::g_padding * (buttonCount - 1)) / buttonCount;
        const auto text = [](const std::string& value, ImU32 color = Theme::g_textDim)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
            ImGui::TextUnformatted(value.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        };
        ImGui::PushID(static_cast<int>(stage));
        ImGui::BeginChild("##ProjectBody", ImVec2(innerWidth, bodyHeight), false);
        const auto footer = [&]
        {
            if (!error.empty())
            {
                text(error, Theme::g_textRemove);
            }
            ImGui::EndChild();
            ImGui::PopID();
            DrawModalFooter();
        };
        if (stage == ProjectModal::SaveChanges)
        {
            text(description);
            footer();
            if (ImGui::Button("Save", ImVec2(buttonWidth, Theme::g_frameHeight)) || enter)
            {
                SaveCurrentProject();
            }
            ImGui::SameLine(0, Theme::g_padding);
            if (ImGui::Button("Don't Save", ImVec2(buttonWidth, Theme::g_frameHeight)))
            {
                ContinueProjectCommand(ui.afterSave);
            }
            ImGui::SameLine(0, Theme::g_padding);
        }
        else if (stage == ProjectModal::SaveName || stage == ProjectModal::Export)
        {
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const bool nameEntered = ProjectNameInput(stage == ProjectModal::Export);
            if (stage == ProjectModal::Export)
            {
                auto& options = ui.exportOptions;
                const float rowHeight = Theme::g_frameHeight + rowSpacing;
                ImGui::SetCursorScreenPos(position + ImVec2(0, rowHeight));
                static const char* categories[]
                    = { "Actors", "Objects", "Vehicles", "Pickups", "Removals", "Box Areas", "Races" };
                DrawMaskFilter<128, 160>(
                    "##ExportCategories", "Export", options.categories, IM_ARRAYSIZE(categories),
                    [](int i)
                    {
                        return categories[i];
                    },
                    innerWidth);
                static const char* platforms[] = { "SA-MP", "open.mp" };
                static const char* loops[] = { "for", "foreach", "y_iterate" };
                const float columnWidth = (innerWidth - rowSpacing) * 0.5f;
                int platform = options.openMp ? 1 : 0;
                ImGui::SetCursorScreenPos(position + ImVec2(0, rowHeight * 2));
                if (ImCustom::ChoiceRow("##ExportPlatform", platforms, IM_ARRAYSIZE(platforms), platform, columnWidth))
                {
                    options.openMp = platform == 1;
                }
                ImGui::SameLine(0, rowSpacing);
                if (ImCustom::ToggleButton(
                        "Incognito's Streamer Plugin", options.streamer, ImVec2(columnWidth, Theme::g_frameHeight)))
                {
                    options.streamer = !options.streamer;
                }
                ImGui::SetCursorScreenPos(position + ImVec2(0, rowHeight * 3));
                const float toggleWidth = (columnWidth - rowSpacing) * 0.5f;
                if (ImCustom::ToggleButton("y_timers", options.yTimers, ImVec2(toggleWidth, Theme::g_frameHeight)))
                {
                    options.yTimers = !options.yTimers;
                }
                ImGui::SameLine(0, rowSpacing);
                if (ImCustom::ToggleButton("Comments", options.comments, ImVec2(toggleWidth, Theme::g_frameHeight)))
                {
                    options.comments = !options.comments;
                }
                ImGui::SameLine(0, rowSpacing);
                int loop = static_cast<int>(options.loopStyle);
                if (ImCustom::ChoiceRow("##ExportLoop", loops, IM_ARRAYSIZE(loops), loop, columnWidth))
                {
                    options.loopStyle = static_cast<PawnExport::LoopStyle>(loop);
                }
                ImGui::SetCursorScreenPos(position + ImVec2(0, rowHeight * 4 + ImMax(0.0f, bodyHeight - contentHeight)));
                DrawInspectionNotice("##ExportDirectoryNotice", description.c_str(), innerWidth);
            }
            footer();
            const bool confirm
                = ImGui::Button(stage == ProjectModal::SaveName ? "Save" : "Export", ImVec2(buttonWidth, Theme::g_frameHeight));
            if (confirm || nameEntered || enter)
            {
                std::string name;
                std::string failure;
                if (!Projects::NormalizeName(ui.name, name, failure))
                {
                    GuiShowToast(failure.c_str(), true);
                }
                else
                {
                    strncpy_s(ui.name, name.c_str(), _TRUNCATE);
                    bool exists = false;
                    const bool exporting = stage == ProjectModal::Export;
                    const bool checked = exporting ? Projects::ExportExists(ui.name, exists, failure)
                                                   : Projects::ProjectExists(ui.name, exists, failure);
                    if (!checked)
                    {
                        GuiShowToast(failure.c_str(), true);
                    }
                    else if (exists)
                    {
                        SetProjectModal(exporting ? ProjectModal::OverwriteExport : ProjectModal::OverwriteProject);
                    }
                    else if (exporting ? Projects::Export(ui.name, ui.exportOptions, false, failure)
                                       : Projects::Save(ui.name, failure, false, ui.saveAs))
                    {
                        GuiShowToast(exporting ? "Pawn export saved." : "Project saved.", false);
                        ContinueProjectCommand(exporting ? ProjectCommand::None : ui.afterSave);
                    }
                    else
                    {
                        GuiShowToast(failure.c_str(), true);
                    }
                }
            }
            ImGui::SameLine(0, Theme::g_padding);
            if (stage == ProjectModal::Export)
            {
                if (ImGui::Button("Reset", ImVec2(buttonWidth, Theme::g_frameHeight)))
                {
                    ui.exportOptions = {};
                    strncpy_s(ui.name, Projects::IsNamed() ? Projects::Name().c_str() : "", _TRUNCATE);
                    ui.focusName = true;
                }
                ImGui::SameLine(0, Theme::g_padding);
            }
        }
        else if (stage == ProjectModal::Open)
        {
            DrawProjectBrowser(listHeight);
            footer();
            ImGui::BeginDisabled(ui.selectedProjectIndex < 0);
            if (ImGui::Button("Open", ImVec2(buttonWidth, Theme::g_frameHeight)) || (enter && ui.selectedProjectIndex >= 0))
            {
                ui.projectToOpen = ui.entries[ui.selectedProjectIndex];
                if (ui.confirmBeforeOpen && Projects::IsDirty(true))
                {
                    ui.afterSave = ProjectCommand::Open;
                    SetProjectModal(ProjectModal::SaveChanges);
                }
                else
                {
                    ContinueProjectCommand(ProjectCommand::Open);
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine(0, Theme::g_padding);
            if (ImGui::Button("Refresh", ImVec2(buttonWidth, Theme::g_frameHeight)))
            {
                RefreshProjectList();
            }
            ImGui::SameLine(0, Theme::g_padding);
            ImGui::BeginDisabled(ui.selectedProjectIndex < 0);
            ImGui::PushStyleColor(ImGuiCol_Button, Theme::g_danger);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::g_dangerHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::g_dangerActive);
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::g_statusText);
            if (ImGui::Button("Delete", ImVec2(buttonWidth, Theme::g_frameHeight)))
            {
                ui.projectToDelete = ui.entries[ui.selectedProjectIndex];
                SetProjectModal(ProjectModal::DeleteProject);
            }
            ImGui::PopStyleColor(4);
            ImGui::EndDisabled();
            ImGui::SameLine(0, Theme::g_padding);
        }
        else if (stage == ProjectModal::Import)
        {
            text(description);
            footer();
            if (ImGui::Button("Import", ImVec2(buttonWidth, Theme::g_frameHeight)) || enter)
            {
                std::string failure;
                if (Projects::ApplyImport(ui.imported, failure))
                {
                    GuiShowToast("Items imported.", false);
                    ui.modal = ProjectModal::None;
                }
                else
                {
                    GuiShowToast(failure.c_str(), true);
                }
            }
            ImGui::SameLine(0, Theme::g_padding);
        }
        else if (stage == ProjectModal::DeleteProject)
        {
            text(description);
            footer();
            ImGui::PushStyleColor(ImGuiCol_Button, Theme::g_danger);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::g_dangerHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::g_dangerActive);
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::g_statusText);
            if (ImGui::Button("Delete", ImVec2(buttonWidth, Theme::g_frameHeight)) || enter)
            {
                std::string failure;
                const bool current = Projects::IsCurrent(ui.projectToDelete);
                if (Projects::Delete(ui.projectToDelete, failure))
                {
                    ui.confirmBeforeOpen = ui.confirmBeforeOpen || current;
                    GuiShowToast("Project deleted.", false);
                    RefreshProjectList();
                    SetProjectModal(ProjectModal::Open);
                }
                else
                {
                    GuiShowToast(failure.c_str(), true);
                }
            }
            ImGui::PopStyleColor(4);
            ImGui::SameLine(0, Theme::g_padding);
        }
        else if (stage == ProjectModal::OverwriteExport || stage == ProjectModal::OverwriteProject)
        {
            text(description);
            footer();
            if (ImGui::Button("Replace", ImVec2(buttonWidth, Theme::g_frameHeight)) || enter)
            {
                const bool exporting = stage == ProjectModal::OverwriteExport;
                std::string failure;
                if (exporting ? Projects::Export(ui.name, ui.exportOptions, true, failure)
                              : Projects::Save(ui.name, failure, true, ui.saveAs))
                {
                    GuiShowToast(exporting ? "Pawn export saved." : "Project saved.", false);
                    ContinueProjectCommand(exporting ? ProjectCommand::None : ui.afterSave);
                }
                else
                {
                    GuiShowToast(failure.c_str(), true);
                }
            }
            ImGui::SameLine(0, Theme::g_padding);
            if (ImGui::Button("Back", ImVec2(buttonWidth, Theme::g_frameHeight)))
            {
                SetProjectModal(stage == ProjectModal::OverwriteExport ? ProjectModal::Export : ProjectModal::SaveName);
            }
            ImGui::SameLine(0, Theme::g_padding);
        }
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, Theme::g_frameHeight))
            || (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !g_popupClosedOnEscape
                && !ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)))
        {
            ui.modal = stage == ProjectModal::DeleteProject || (stage == ProjectModal::SaveChanges && ui.projectToOpen)
                ? ProjectModal::Open
                : ProjectModal::None;
            ui.projectToOpen.reset();
            ui.afterSave = ProjectCommand::None;
        }
        if (ui.modal == ProjectModal::None)
        {
            ui.imported = {};
            ui.entries.clear();
            ui.visibleProjects.clear();
            ui.projectRowsReady = false;
            ui.projectToDelete = {};
            ui.projectToOpen.reset();
        }
    }
    ImGui::End();
    PopModalStyle(2);
}
