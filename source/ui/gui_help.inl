// Source Repository: https://github.com/waverso/waversomapstudio

static void DrawHelpModal()
{
    if (!g_helpOpen)
    {
        return;
    }
    static constexpr struct
    {
        const char* label;
        const char* keys;
    } shortcuts[] = {
        { "General", nullptr },
        { "Toggle Editor", "F4" },
        { "Open Help", "F1" },
        { "Add Item", "Insert" },
        { "Undo", "Ctrl+Z" },
        { "Redo", "Ctrl+Y" },
        { "Projects", nullptr },
        { "New Project", "Ctrl+N" },
        { "Open Project", "Ctrl+O" },
        { "Save Project", "Ctrl+S" },
        { "Save Project As", "Ctrl+Shift+S" },
        { "Import Pawn From Clipboard", "Ctrl+I" },
        { "Export Pawn", "Ctrl+E" },
        { "Camera", nullptr },
        { "Move Camera", "W / A / S / D" },
        { "Move Up", "E / Space" },
        { "Move Down", "Q" },
        { "Faster Movement", "Hold Shift" },
        { "Slower Movement", "Hold Alt" },
        { "Focus Selection", "F" },
        { "Rotate Camera", "RMB + Drag" },
        { "Pan Camera", "MMB + Drag" },
        { "Orbit Camera", "Alt + MMB + Drag" },
        { "Move Forward / Backward", "Mouse Wheel" },
        { "Adjust Field Of View", "Ctrl + Mouse Wheel" },
        { "Selection & Outliner", nullptr },
        { "Select Item", "LMB" },
        { "Rename Item", "F2 / Double-Click" },
        { "Open Context Menu", "RMB" },
        { "Select All Scene Items", "Ctrl+A" },
        { "Select Parent", "P" },
        { "Clone Selection", "Ctrl+C" },
        { "Delete Selection", "Delete" },
        { "Gizmo Transform", nullptr },
        { "No Gizmo", "1" },
        { "Position Mode", "2" },
        { "Rotation Mode", "3" },
        { "Position + Rotation Mode", "4" },
        { "Toggle Center / Pivot", "Z" },
        { "Toggle Global / Local", "X" },
        { "Apply Grid Snap", "Hold Ctrl + Drag Gizmo" },
        { "GUI Value Editing", nullptr },
        { "Fine Adjustment", "Alt + Drag" },
        { "Fast Adjustment", "Shift + Drag" },
        { "Type Numeric Value", "Ctrl+Click / Double-click" },
        { "Browsers", nullptr },
        { "Focus Search", "Ctrl+F" },
        { "Previous / Next Page", "Left / Right" },
        { "Previews", nullptr },
        { "Orbit Preview", "RMB + Drag" },
        { "Pan Preview", "LMB/MMB + Drag" },
        { "Zoom Preview", "Mouse Wheel" },
        { "Faster Zoom", "Shift + Mouse Wheel" },
        { "Finer Zoom", "Alt + Mouse Wheel" },
        { "Reset Preview View", "R" },
        { "Toggle Favorite", "F" },
    };
    static std::array<bool, 8> expanded;
    static char search[128] = {};
    static bool searchFocused = false;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float width = floorf(ImMin(Ui::Px(440.0f), display.x - Theme::g_outerPadding * 2));
    const float bodyHeight = ImMax(1.0f, ImMin(Ui::Px(460.0f), display.y - Theme::g_outerPadding * 2 - ModalHeight(0)));
    DrawModalBackdrop("##HelpBackdrop");
    ImGui::SetNextWindowPos(display * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, ModalHeight(bodyHeight)), ImGuiCond_Always);
    PushModalStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, Theme::g_padding));
    if (ImGui::Begin("##HelpModal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        DrawModalTitleBar(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), width, "Help");
        const bool appearing = ImGui::IsWindowAppearing();
        if (appearing)
        {
            expanded.fill(true);
            search[0] = '\0';
            searchFocused = false;
        }
        const float innerWidth = floorf(width - Theme::g_outerPadding * 2);
        ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, Theme::g_tabHeight + Theme::g_padding));
        if (g_helpFocusSearch)
        {
            ImGui::SetKeyboardFocusHere();
            g_helpFocusSearch = false;
        }
        const bool changed = SearchInput("##help_search", "Search", search, sizeof(search), innerWidth, searchFocused);
        if (changed || appearing)
        {
            expanded.fill(true);
            ImGui::SetNextWindowScroll(ImVec2(0, 0));
        }
        ImGui::SetCursorPosX(Theme::g_outerPadding);
        ImGui::BeginChild("##HelpBody",
            ImVec2(
                innerWidth + ImGui::GetStyle().ScrollbarSize, ImMax(1.0f, bodyHeight - Theme::g_frameHeight - Theme::g_padding)),
            false);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float listWidth = innerWidth;
        const float rowHeight = Ui::Px(21.0f);
        int section = 0;
        bool found = false;
        bool listGap = false;
        for (int first = 0; first < std::size(shortcuts);)
        {
            const auto& heading = shortcuts[first++];
            const int category = section++;
            int last = first;
            while (last < std::size(shortcuts) && shortcuts[last].keys)
            {
                ++last;
            }
            const bool categoryMatch = TextSearch::ContainsAsciiNoCase(heading.label, search);
            const auto matches = [&](int index)
            {
                return categoryMatch || TextSearch::ContainsAsciiNoCase(shortcuts[index].label, search)
                    || TextSearch::ContainsAsciiNoCase(shortcuts[index].keys, search);
            };
            int count = 0;
            for (int i = first; i < last; ++i)
            {
                count += matches(i);
            }
            if (count)
            {
                if (listGap)
                {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Theme::g_padding);
                }
                found = true;
                listGap = DrawViewSectionHeader(heading.label, heading.label, &expanded[category], listWidth);
                if (listGap)
                {
                    const ImVec2 min = ImGui::GetCursorScreenPos();
                    const ImVec2 max(min.x + listWidth, min.y + count * rowHeight);
                    dl->AddRectFilled(min, max, Theme::g_alternatePanelBackground);
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                    int row = 0;
                    for (int i = first; i < last; ++i)
                    {
                        if (!matches(i))
                        {
                            continue;
                        }
                        const auto& shortcut = shortcuts[i];
                        ImGui::PushID(shortcut.label);
                        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
                        const ImVec2 rowMax(rowMin.x + listWidth, rowMin.y + rowHeight);
                        ImGui::ItemSize(ImVec2(listWidth, rowHeight));
                        ImGui::ItemAdd(ImRect(rowMin, rowMax), ImGui::GetID("##help_row"), nullptr, ImGuiItemFlags_NoNav);
                        if (row & 1)
                        {
                            dl->AddRectFilled(rowMin, rowMax, Theme::g_alternateListRowBackground);
                        }
                        const float keysWidth = ImGui::CalcTextSize(shortcut.keys).x;
                        const float textY = rowMin.y + floorf((rowHeight - ImGui::GetFontSize()) * 0.5f);
                        dl->AddText(ImVec2(rowMax.x - Theme::g_padding - keysWidth, textY), Theme::g_textDim, shortcut.keys);
                        const ImVec4 clip(rowMin.x, rowMin.y, rowMax.x - Theme::g_padding * 2 - keysWidth, rowMax.y);
                        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(rowMin.x + Theme::g_padding, textY),
                            Theme::g_text, shortcut.label, nullptr, 0, &clip);
                        ImGui::PopID();
                        ++row;
                    }
                    ImGui::PopStyleVar();
                    DrawClippedListBorder(min, max, dl->GetClipRectMin(), dl->GetClipRectMax());
                    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
                }
            }
            first = last;
        }
        if (!found)
        {
            DrawCenteredMessage("No shortcuts found.");
        }
        ImGui::EndChild();
        DrawModalFooter();
        if (ImGui::Button("Close##help", ImVec2(innerWidth, Theme::g_frameHeight))
            || (!g_popupClosedOnEscape
                && (ImGui::IsKeyPressed(ImGuiKey_Escape, false)
                    || (!ImGui::GetIO().WantTextInput
                        && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))))))
        {
            g_helpOpen = false;
        }
    }
    ImGui::End();
    PopModalStyle(1);
    if (g_helpOpen)
    {
        ImGui::SetNextFrameWantCaptureKeyboard(true);
    }
}

static void OpenStudioLink(const char* url)
{
    if (!StudioInfo::OpenLink(url))
    {
        g_toast.ShowError("Cannot open the link in your browser.");
    }
}

static void PollStudioUpdate()
{
    if (StudioInfo::PollUpdate())
    {
        if (StudioInfo::Status() == StudioInfo::UpdateState::Available)
        {
            g_updateAvailablePending = true;
            if (!g_updateCheckAutomatic)
            {
                g_studioInfoOpen = false;
            }
        }
        else if (!g_updateCheckAutomatic && StudioInfo::Status() == StudioInfo::UpdateState::Current)
        {
            g_toast.ShowInfo("You are using the latest version.");
        }
        else if (!g_updateCheckAutomatic)
        {
            g_toast.ShowError("Cannot check for updates. Try again later.");
        }
    }
}

static void DrawUpdateAvailableModal()
{
    if (g_updateAvailablePending && !IsAnyModalOpen() && !SampTextInputActive() && !ImGui::IsAnyItemActive()
        && !ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsKeyPressed(ImGuiKey_Enter, false)
        && !ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)
        && !ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && GuiFinishPropertyEdit())
    {
        ImGui::ClearActiveID();
        g_updateAvailablePending = false;
        g_updateAvailableOpen = true;
    }
    if (!g_updateAvailableOpen)
    {
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float width = floorf(ImMin(Ui::Px(420.0f), display.x - Theme::g_outerPadding * 2));
    const float innerWidth = width - Theme::g_outerPadding * 2;
    DrawModalBackdrop("##UpdateAvailableBackdrop");
    ImGui::SetNextWindowPos(display * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, ModalHeight(ImGui::GetTextLineHeight())), ImGuiCond_Always);
    PushModalStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, Theme::g_padding));
    if (ImGui::Begin("##UpdateAvailableModal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        DrawModalTitleBar(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), width, "Update");
        ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, Theme::g_tabHeight + Theme::g_padding));
        ImGui::Text("A new version is available: %s", StudioInfo::LatestVersion());
        DrawModalFooter();
        const float buttonWidth = (innerWidth - Theme::g_padding) * 0.5f;
        if (ImGui::Button("Download##update_available", ImVec2(buttonWidth, Theme::g_frameHeight)))
        {
            OpenStudioLink(StudioInfo::ReleasesUrl);
        }
        ImGui::SameLine(0, Theme::g_padding);
        if (ImGui::Button("Later##update_available", ImVec2(buttonWidth, Theme::g_frameHeight))
            || (!g_popupClosedOnEscape
                && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false)
                    || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))))
        {
            g_updateAvailableOpen = false;
        }
    }
    ImGui::End();
    PopModalStyle(1);
    if (g_updateAvailableOpen)
    {
        ImGui::SetNextFrameWantCaptureKeyboard(true);
    }
}

static void DrawStudioInfoModal()
{
    if (!g_studioInfoOpen)
    {
        return;
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float width = floorf(ImMin(Ui::Px(380.0f), display.x - Theme::g_outerPadding * 2));
    const float innerWidth = width - Theme::g_outerPadding * 2;
    const float rowHeight = ImGui::GetFontSize() + Ui::Px(3.0f);
    DrawModalBackdrop("##StudioInfoBackdrop");
    ImGui::SetNextWindowPos(display * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(
        ImVec2(width, ModalHeight(rowHeight * 2 + Theme::g_frameHeight + Theme::g_padding)), ImGuiCond_Always);
    PushModalStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, Theme::g_padding));
    if (ImGui::Begin("##StudioInfoModal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        DrawModalTitleBar(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), width, "Information");
        ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, Theme::g_tabHeight + Theme::g_padding));
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::BeginChild("##StudioInfoList", ImVec2(innerWidth, rowHeight * 2), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetCursorScreenPos(), max = min + ImVec2(innerWidth, rowHeight * 2);
        draw->AddRectFilled(min, max, Theme::g_alternatePanelBackground);
        const char* labels[] = { "Version", "Compile Date (UTC)" };
        const char* values[] = { StudioInfo::Version, StudioInfo::CompileDateUtc() };
        for (int row = 0; row < 2; ++row)
        {
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::PushID(labels[row]);
            ImGui::ItemSize(ImVec2(innerWidth, rowHeight));
            ImGui::ItemAdd(
                ImRect(pos, pos + ImVec2(innerWidth, rowHeight)), ImGui::GetID("##info_row"), nullptr, ImGuiItemFlags_NoNav);
            if (row & 1)
            {
                draw->AddRectFilled(pos, pos + ImVec2(innerWidth, rowHeight), Theme::g_alternateListRowBackground);
            }
            const float y = pos.y + floorf((rowHeight - ImGui::GetFontSize()) * 0.5f);
            draw->AddText(ImVec2(pos.x + Theme::g_padding, y), Theme::g_text, labels[row]);
            draw->AddText(ImVec2(pos.x + innerWidth - Theme::g_padding - ImGui::CalcTextSize(values[row]).x, y), Theme::g_textDim,
                values[row]);
            ImGui::PopID();
        }
        DrawClippedListBorder(min, max, draw->GetClipRectMin(), draw->GetClipRectMax());
        ImGui::PopStyleVar(2);
        ImGui::EndChild();
        const char* buttonLabels[] = { "Check Update", "GitHub", "YouTube" };
        float buttonWidths[std::size(buttonLabels)];
        float rowWidth = Theme::g_padding * (std::size(buttonLabels) - 1);
        for (int i = 0; i < std::size(buttonLabels); ++i)
        {
            buttonWidths[i] = ImGui::CalcTextSize(buttonLabels[i]).x + Theme::g_iconSize + Theme::g_padding * 3;
            rowWidth += buttonWidths[i];
        }
        for (float& buttonWidth : buttonWidths)
        {
            buttonWidth += (innerWidth - rowWidth) / std::size(buttonLabels);
        }
        const bool checking = StudioInfo::Status() == StudioInfo::UpdateState::Checking;
        ImGui::BeginDisabled(checking);
        if (IconLabelButton(
                "##studio_update", checking ? "Checking..." : buttonLabels[0], g_icons->rotationTexture, buttonWidths[0]))
        {
            if (StudioInfo::CheckUpdate())
            {
                g_updateCheckAutomatic = false;
            }
            else
            {
                g_toast.ShowError("Cannot check for updates. Try again later.");
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, Theme::g_padding);
        if (IconLabelButton("##studio_github", buttonLabels[1], g_icons->gitHubTexture, buttonWidths[1]))
        {
            OpenStudioLink(StudioInfo::RepositoryUrl);
        }
        ImGui::SameLine(0, Theme::g_padding);
        if (IconLabelButton("##studio_youtube", buttonLabels[2], g_icons->youTubeTexture, buttonWidths[2]))
        {
            OpenStudioLink(StudioInfo::YouTubeUrl);
        }
        ImGui::EndGroup();
        DrawModalFooter();
        if (ImGui::Button("Close##studio_info", ImVec2(innerWidth, Theme::g_frameHeight))
            || (!g_popupClosedOnEscape
                && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false)
                    || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))))
        {
            g_studioInfoOpen = false;
        }
    }
    ImGui::End();
    PopModalStyle(1);
    if (g_studioInfoOpen)
    {
        ImGui::SetNextFrameWantCaptureKeyboard(true);
    }
}
