// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    static void RefreshProjectList()
    {
        auto& ui = g_projectUi;
        const auto selectedPath = ui.selectedProjectIndex >= 0 && ui.selectedProjectIndex < static_cast<int>(ui.entries.size())
            ? ui.entries[ui.selectedProjectIndex].path
            : std::filesystem::path {};
        ui.selectedProjectIndex = -1;
        ui.resetProjectScroll = true;
        std::string error;
        if (!Projects::List(ui.entries, error))
        {
            GuiShowToast(error.c_str(), true);
        }
        for (int i = 0; i < static_cast<int>(ui.entries.size()); ++i)
        {
            if (ui.entries[i].path == selectedPath)
            {
                ui.selectedProjectIndex = i;
                break;
            }
        }
    }
}

static std::string ProjectSavedAgo(std::filesystem::file_time_type saved)
{
    const auto now = std::filesystem::file_time_type::clock::now();
    const auto elapsed = (std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
        - std::chrono::duration_cast<std::chrono::seconds>(saved.time_since_epoch()))
                             .count();
    const auto seconds = std::max<std::int64_t>(0, elapsed);
    if (seconds < 60)
    {
        return std::to_string(seconds) + "s ago";
    }
    if (seconds < 3600)
    {
        return std::to_string(seconds / 60) + "m ago";
    }
    if (seconds < 86400)
    {
        return std::to_string(seconds / 3600) + "h ago";
    }
    if (seconds < 31536000)
    {
        return std::to_string(seconds / 86400) + "d ago";
    }
    return std::to_string(seconds / 31536000) + "y ago";
}

static const std::vector<int>& ProjectRows(ProjectUiState& ui)
{
    if (!ui.projectRowsReady || ui.resetProjectScroll)
    {
        auto& visible = ui.visibleProjects;
        visible.clear();
        visible.reserve(ui.entries.size());
        for (int i = 0; i < static_cast<int>(ui.entries.size()); ++i)
        {
            if (TextSearch::ContainsAsciiNoCase(ui.entries[i].name.c_str(), ui.search))
            {
                visible.push_back(i);
            }
        }
        std::sort(visible.begin(), visible.end(),
            [&](int left, int right)
            {
                const auto& a = ui.entries[left];
                const auto& b = ui.entries[right];
                if (ui.sortKey == 1 && a.lastSaved != b.lastSaved)
                {
                    return ui.sortDesc ? a.lastSaved > b.lastSaved : a.lastSaved < b.lastSaved;
                }
                if (ui.sortKey == 2 && a.itemCount != b.itemCount)
                {
                    return ui.sortDesc ? a.itemCount > b.itemCount : a.itemCount < b.itemCount;
                }
                const int names = _stricmp(a.name.c_str(), b.name.c_str());
                if (names != 0)
                {
                    return ui.sortKey == 0 && ui.sortDesc ? names > 0 : names < 0;
                }
                return a.path < b.path;
            });
        ui.projectRowsReady = true;
    }
    const auto& visible = ui.visibleProjects;
    if (std::find(visible.begin(), visible.end(), ui.selectedProjectIndex) == visible.end())
    {
        ui.selectedProjectIndex = -1;
    }
    return visible;
}

static void DrawProjectBrowser(float listHeight)
{
    auto& ui = g_projectUi;
    const float width = ImGui::GetContentRegionAvail().x;
    const float sortWidth
        = ImMax(Ui::Px(130), ImGui::CalcTextSize("Sort: Last Saved").x + Theme::g_frameHeight + Theme::g_padding * 2);
    const float directionWidth = Theme::g_frameHeight;
    const float searchWidth = ImMax(1.0f, width - sortWidth - directionWidth - Theme::g_padding * 2);
    if (ui.focusSearch)
    {
        ImGui::SetKeyboardFocusHere();
        ui.focusSearch = false;
    }
    if (SearchInput("##ProjectSearch", "Search", ui.search, sizeof(ui.search), searchWidth, ui.searchFocused))
    {
        ui.resetProjectScroll = true;
    }
    ImGui::SameLine(0, Theme::g_padding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
    static const char* sortNames[] = { "Name", "Last Saved", "Items" };
    const std::string sortLabel = std::string("Sort: ") + sortNames[ui.sortKey];
    if (ImCustom::BeginCombo("##ProjectSort", sortLabel.c_str(), sortWidth))
    {
        for (int i = 0; i < IM_ARRAYSIZE(sortNames); ++i)
        {
            if (ImCustom::ComboItem(sortNames[i], ui.sortKey == i))
            {
                ui.sortKey = i;
                ui.resetProjectScroll = true;
            }
        }
        ImCustom::EndCombo();
    }
    ImGui::PopStyleVar(2);
    ImGui::SameLine(0, Theme::g_padding);
    if (ImGui::Button("##ProjectDirection", ImVec2(directionWidth, Theme::g_frameHeight)))
    {
        ui.sortDesc = !ui.sortDesc;
        ui.resetProjectScroll = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        SetStyledTooltip(ui.sortDesc ? "Descending" : "Ascending");
    }
    const ImVec2 directionMin = ImGui::GetItemRectMin();
    const ImVec2 directionMax = ImGui::GetItemRectMax();
    DrawRotatedArrow(ImGui::GetWindowDrawList(), (directionMin + directionMax) * 0.5f, ui.sortDesc ? 1.0f : -1.0f, Theme::g_text);

    const auto& visible = ProjectRows(ui);

    const float rowHeight = Ui::Px(21);
    static ItemListNavigation navigation;
    const bool keyboard = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && !ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)
        && (!ImGui::IsAnyItemActive() || ui.searchFocused);
    const auto selected = std::find(visible.begin(), visible.end(), ui.selectedProjectIndex);
    const int selectedRow = selected == visible.end() ? -1 : static_cast<int>(selected - visible.begin());
    const int keyRow = NavigateItemList("##Projects", navigation, static_cast<int>(visible.size()), selectedRow, keyboard, true);
    if (keyRow >= 0)
    {
        ui.selectedProjectIndex = visible[keyRow];
    }
    if (ui.resetProjectScroll)
    {
        ImGui::SetNextWindowScroll(ImVec2(0, 0));
        ui.resetProjectScroll = false;
    }
    const ImVec2 scrollRange = keyRow >= 0 ? ImVec2(keyRow * rowHeight, (keyRow + 1) * rowHeight) : ImVec2(0, 0);
    BeginItemList("##Projects", ImVec2(width, listHeight), visible.size(), scrollRange);
    if (visible.empty())
    {
        DrawCenteredMessage(ui.entries.empty() ? "No projects yet." : "No projects found.");
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::SetCursorPos(ImVec2(0, 0));
        auto* drawList = ImGui::GetWindowDrawList();
        const float rowWidth = ImGui::GetContentRegionAvail().x;
        const float countWidth = ImGui::CalcTextSize((std::to_string(EditorScene::MaxItems) + " Items").c_str()).x;
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible.size()), rowHeight);
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const int index = visible[row];
                const auto& entry = ui.entries[index];
                ImGui::PushID(index);
                ImGui::SetCursorPos(ImVec2(0, row * rowHeight));
                const ImVec2 rowMin = ImGui::GetCursorScreenPos();
                const ImVec2 rowMax(rowMin.x + rowWidth, rowMin.y + rowHeight);
                if (ImGui::InvisibleButton("##ProjectRow", ImVec2(rowWidth, rowHeight)))
                {
                    ui.selectedProjectIndex = index;
                }
                const bool hovered = ImGui::IsItemHovered();
                if (ui.selectedProjectIndex == index)
                {
                    drawList->AddRectFilled(rowMin, rowMax, hovered ? Theme::g_accentSoftHover : Theme::g_accentSoft);
                }
                else if (hovered)
                {
                    drawList->AddRectFilled(rowMin, rowMax, Theme::g_hoveredControlBackground);
                }
                else if (row & 1)
                {
                    drawList->AddRectFilled(rowMin, rowMax, Theme::g_alternateListRowBackground);
                }
                const float textY = rowMin.y + floorf((rowHeight - ImGui::GetFontSize()) * 0.5f);
                const std::string count = std::to_string(entry.itemCount) + (entry.itemCount == 1 ? " Item" : " Items");
                const std::string saved = ProjectSavedAgo(entry.lastSaved);
                const float savedX = rowMax.x - Theme::g_padding - ImGui::CalcTextSize(saved.c_str()).x;
                const float nameX = rowMin.x + Theme::g_padding * 2 + countWidth;
                const float nameEnd = savedX - Theme::g_padding;
                drawList->AddText(ImVec2(rowMin.x + Theme::g_padding, textY), Theme::g_textDim, count.c_str());
                drawList->AddText(ImVec2(savedX, textY), Theme::g_textDim, saved.c_str());
                if (nameEnd > nameX)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, hovered ? Theme::g_textHover : Theme::g_text);
                    drawList->PushClipRect(ImVec2(nameX, rowMin.y), ImVec2(nameEnd, rowMax.y), true);
                    ImGui::RenderTextEllipsis(drawList, ImVec2(nameX, textY), ImVec2(nameEnd, rowMax.y), nameEnd, nameEnd,
                        entry.name.c_str(), nullptr, nullptr);
                    drawList->PopClipRect();
                    ImGui::PopStyleColor();
                }
                ImGui::PopID();
            }
        }
        ImGui::PopStyleVar();
    }
    EndItemList();
}
