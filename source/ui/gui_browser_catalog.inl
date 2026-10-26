// Source Repository: https://github.com/waverso/waversomapstudio

struct BrowserCell
{
    const char* name = "";
    char subtitle[48] = {};
    char tooltip[256] = {};
    ComponentInfoImage image;
    bool favorite = false;
};

struct BrowserLayout
{
    float thumbnail = Ui::Px(96.0f);
    float gap = Theme::g_padding;
    float cellHeight;
    float gridWidth;
    float gridHeight;
    float top;
    ImVec2 windowSize;
    int columns;
    int rows;

    BrowserLayout()
    {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float fontSize = ImGui::GetFontSize();
        cellHeight = thumbnail + Ui::Px(8.0f) + fontSize * 1.85f;
        const float availableW = ImMax(1.0f, display.x - Ui::Px(40.0f));
        const float availableH = ImMax(1.0f, display.y - Ui::Px(40.0f));
        const float maxW = ImClamp(display.x * 0.65f, ImMin(Ui::Px(960.0f), availableW), availableW);
        const float maxH = ImClamp(display.y * 0.85f, ImMin(Ui::Px(480.0f), availableH), availableH);
        columns = ImClamp(static_cast<int>((maxW - Theme::g_outerPadding * 2.0f + gap) / (thumbnail + gap)), 5, 15);
        top = Theme::g_tabHeight + Theme::g_padding * 2.0f + Theme::g_frameHeight;
        const float bottom = ModalFooterHeight() + Theme::g_itemSpacing;
        rows = ImClamp(static_cast<int>((maxH - top - bottom + gap) / (cellHeight + gap)), 2, 6);
        gridWidth = columns * thumbnail + (columns - 1) * gap;
        gridHeight = rows * cellHeight + (rows - 1) * gap;
        windowSize = ImVec2(Theme::g_outerPadding * 2.0f + gridWidth, top + gridHeight + bottom);
    }
};

template <typename State, size_t SortCount>
static void DrawBrowserToolbar(State& ui, const char* hint, float width, const char* const (&sortNames)[SortCount],
    bool filtersActive, bool canClear, void (*openFilters)(), void (*clearFilters)(State&))
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float sortW = Ui::Px(130.0f);
    const float directionW = Theme::g_frameHeight;
    const float favoriteW = ImGui::CalcTextSize("Favorites Only").x + Theme::g_padding * 2.0f;
    const float filtersW = ImGui::CalcTextSize("Filters").x + Theme::g_padding * 2.0f;
    const float clearW = ImGui::CalcTextSize("Clear Filters").x + Theme::g_padding * 2.0f;
    const float searchW = ImMax(1.0f, width - sortW - directionW - favoriteW - filtersW - clearW - Theme::g_padding * 5.0f);
    bool changed = false;
    if (ui.focusSearch)
    {
        ImGui::SetKeyboardFocusHere();
        ui.focusSearch = false;
    }
    changed |= SearchInput("##search", hint, ui.search, sizeof(ui.search), searchW, ui.searchFocused);
    float x = start.x + searchW + Theme::g_padding;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
    ui.sortKey = ImClamp(ui.sortKey, 0, static_cast<int>(SortCount) - 1);
    char label[64];
    snprintf(label, sizeof(label), "Sort: %s", sortNames[ui.sortKey]);
    ImGui::SetCursorScreenPos(ImVec2(x, start.y));
    if (ImCustom::BeginCombo("##sort", label, sortW))
    {
        for (int index = 0; index < static_cast<int>(SortCount); ++index)
        {
            if (ImCustom::ComboItem(sortNames[index], ui.sortKey == index))
            {
                ui.sortKey = index;
                changed = true;
            }
        }
        ImCustom::EndCombo();
    }
    ImGui::PopStyleVar(2);
    x += sortW + Theme::g_padding;
    ImGui::SetCursorScreenPos(ImVec2(x, start.y));
    if (ImGui::Button("##direction", ImVec2(directionW, Theme::g_frameHeight)))
    {
        ui.sortDesc = !ui.sortDesc;
        changed = true;
    }
    const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    DrawRotatedArrow(ImGui::GetWindowDrawList(), ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f),
        ui.sortDesc ? 1.0f : -1.0f, ImGui::GetColorU32(Theme::g_text));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        SetStyledTooltip(ui.sortDesc ? "Descending" : "Ascending");
    }
    x += directionW + Theme::g_padding;
    const bool favorite = ui.favoritesOnly;
    if (favorite)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::g_success);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::g_successHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::g_successActive);
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::g_statusText);
    }
    ImGui::SetCursorScreenPos(ImVec2(x, start.y));
    if (ImGui::Button("Favorites Only", ImVec2(favoriteW, Theme::g_frameHeight)))
    {
        ui.favoritesOnly = !ui.favoritesOnly;
        changed = true;
    }
    if (favorite)
    {
        ImGui::PopStyleColor(4);
    }
    x += favoriteW + Theme::g_padding;
    if (filtersActive)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::g_accent);
    }
    ImGui::SetCursorScreenPos(ImVec2(x, start.y));
    if (ImGui::Button("Filters", ImVec2(filtersW, Theme::g_frameHeight)))
    {
        openFilters();
    }
    if (filtersActive)
    {
        ImGui::PopStyleColor();
    }
    x += filtersW + Theme::g_padding;
    ImGui::BeginDisabled(!canClear);
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::g_danger);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::g_dangerHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::g_dangerActive);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::g_statusText);
    ImGui::SetCursorScreenPos(ImVec2(x, start.y));
    if (ImGui::Button("Clear Filters", ImVec2(clearW, Theme::g_frameHeight)))
    {
        clearFilters(ui);
        ImGui::ClearActiveID();
    }
    ImGui::PopStyleColor(4);
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
    if (changed)
    {
        ui.dirty = true;
        ui.page = 0;
    }
}

static bool DrawBrowserCell(const BrowserCell& cell, const BrowserLayout& layout, const ImVec2& min, bool& favoriteClicked)
{
    ImGui::SetCursorScreenPos(min);
    const bool clicked = ImGui::InvisibleButton("##cell", ImVec2(layout.thumbnail, layout.cellHeight));
    const bool hovered = ImGui::IsItemHovered();
    favoriteClicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 max(min.x + layout.thumbnail, min.y + layout.thumbnail);
    dl->AddRectFilled(min, max, ImGui::GetColorU32(Theme::g_controlBackground));
    if (cell.image.texture)
    {
        const float aspect = cell.image.aspect > 0.0f ? cell.image.aspect : 1.0f;
        const ImVec2 size(layout.thumbnail * ImMin(1.0f, aspect), layout.thumbnail / ImMax(1.0f, aspect));
        const ImVec2 imageMin(min.x + (layout.thumbnail - size.x) * 0.5f, min.y + (layout.thumbnail - size.y) * 0.5f);
        dl->AddImage((ImTextureID)cell.image.texture, imageMin, ImVec2(imageMin.x + size.x, imageMin.y + size.y),
            cell.image.uvMin, cell.image.uvMax, ImGui::GetColorU32(IM_COL32_WHITE));
    }
    else
    {
        const char* text = "No Preview";
        const ImVec2 size = ImGui::CalcTextSize(text);
        dl->AddText(ImVec2(min.x + (layout.thumbnail - size.x) * 0.5f, min.y + (layout.thumbnail - size.y) * 0.5f),
            ImGui::GetColorU32(Theme::g_textDim), text);
    }
    if (ImGui::IsItemActive() || hovered)
    {
        dl->AddRectFilled(min, max, ImGui::GetColorU32(Theme::Alpha(Theme::g_highlight, ImGui::IsItemActive() ? 50 : 18)));
    }
    dl->AddRect(min, max, ImGui::GetColorU32(hovered ? Theme::g_borderStrong : Theme::g_border), 0.0f, 0, Ui::Hairline());
    if (cell.favorite && g_icons)
    {
        const float size = Theme::g_smallIconSize;
        const ImVec2 pos(max.x - size - Ui::Px(3.0f), min.y + Ui::Px(3.0f));
        DrawIcon(dl, g_icons->heartTexture, ImVec2(pos.x + Ui::Hairline(), pos.y + Ui::Hairline()), size,
            ImGui::GetColorU32(IM_COL32(0, 0, 0, 150)));
        DrawIcon(dl, g_icons->heartTexture, pos, size, ImGui::GetColorU32(Theme::g_chipFavorited));
    }
    const float fontSize = ImGui::GetFontSize();
    char name[128];
    EllipsizeText(name, sizeof(name), cell.name, layout.thumbnail, fontSize);
    const float textWidth = ImGui::CalcTextSize(name).x;
    const ImU32 nameColor = cell.favorite ? (hovered ? Theme::g_dangerHover : Theme::g_chipFavorited)
                                          : (hovered ? Theme::g_textHover : Theme::g_text);
    dl->AddText(
        ImVec2(min.x + floorf((layout.thumbnail - textWidth) * 0.5f), max.y + Ui::Px(4.0f)), ImGui::GetColorU32(nameColor), name);
    const float smallSize = fontSize * 0.85f;
    const float subtitleWidth = ImGui::GetFont()->CalcTextSizeA(smallSize, FLT_MAX, 0.0f, cell.subtitle).x;
    dl->AddText(ImGui::GetFont(), smallSize,
        ImVec2(min.x + floorf((layout.thumbnail - subtitleWidth) * 0.5f), max.y + Ui::Px(6.0f) + fontSize),
        ImGui::GetColorU32(Theme::g_textDim), cell.subtitle);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        SetBrowserTooltip(cell.tooltip[0] ? cell.tooltip : cell.name);
    }
    return clicked;
}

static bool DrawBrowserFooter(int total, const char* noun, int& page, int pages, bool keyboardEnabled)
{
    const float y = DrawModalFooter();
    const ImVec2 pos = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();
    char found[64];
    snprintf(found, sizeof(found), "%d %s%s found", total, noun, total == 1 ? "" : "s");
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(pos.x + Theme::g_outerPadding, y + FrameTextPadY()), ImGui::GetColorU32(Theme::g_textDim), found);
    const float buttonW = Ui::Px(64.0f);
    const float pagerW = buttonW * 2.0f + Theme::g_padding * 2.0f + PageLinkWidth();
    float x = pos.x + floorf((width - pagerW) * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::BeginDisabled(page == 0);
    if (PageNavigationButton("##browser_prev", "Prev", buttonW, true))
    {
        --page;
    }
    ImGui::EndDisabled();
    x += buttonW + Theme::g_padding;
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    DrawPageLink("##page_link", "##page_popup", page, pages, &page);
    x += PageLinkWidth() + Theme::g_padding;
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::BeginDisabled(page == pages - 1);
    if (PageNavigationButton("##browser_next", "Next", buttonW, false))
    {
        ++page;
    }
    ImGui::EndDisabled();
    const float cancelW = Ui::Px(90.0f);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + width - Theme::g_outerPadding - cancelW, y));
    const bool cancel = ImGui::Button("Cancel", ImVec2(cancelW, Theme::g_frameHeight));
    ImGui::PopStyleVar();
    if (keyboardEnabled && !ImGui::GetIO().WantTextInput
        && !ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        {
            --page;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        {
            ++page;
        }
    }
    page = ImClamp(page, 0, pages - 1);
    DrawPageJumpPopup("##page_popup");
    return cancel;
}

template <typename State, size_t SortCount, typename Prepare, typename GetCell, typename ToggleFavorite>
static int DrawBrowserCatalog(const char* id, const char* title, const char* hint, const char* noun, State& ui,
    const char* const (&sortNames)[SortCount], bool filtersActive, bool canClear, bool blocked, void (*openFilters)(),
    void (*clearFilters)(State&), Prepare prepare, GetCell getCell, ToggleFavorite toggleFavorite, bool& cancelled)
{
    const BrowserLayout layout;
    const auto& io = ImGui::GetIO();
    char backdrop[96];
    snprintf(backdrop, sizeof(backdrop), "%sBackdrop", id);
    DrawModalBackdrop(backdrop);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(layout.windowSize, ImGuiCond_Always);
    PushModalStyle();
    int selected = -1;
    if (ImGui::Begin(id, nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
                | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
    {
        if (!ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetWindowPos();
        DrawModalTitleBar(dl, pos, layout.windowSize.x, title);
        ImGui::BeginDisabled(blocked);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, pos.y + Theme::g_tabHeight + Theme::g_padding));
        DrawBrowserToolbar(ui, hint, layout.gridWidth, sortNames, filtersActive, canClear, openFilters, clearFilters);
        const int total = static_cast<int>(ui.filtered.size());
        const int perPage = layout.columns * layout.rows;
        const int pages = ImMax(1, (total + perPage - 1) / perPage);
        ui.page = ImClamp(ui.page, 0, pages - 1);
        const int first = ui.page * perPage;
        const int last = ImMin(total, first + perPage);
        prepare(last > first ? ui.filtered.data() + first : nullptr, static_cast<size_t>(last - first));
        const ImVec2 grid(pos.x + Theme::g_outerPadding, pos.y + layout.top);
        for (int index = first; index < last; ++index)
        {
            const auto key = ui.filtered[index];
            const int column = (index - first) % layout.columns;
            const int row = (index - first) / layout.columns;
            const ImVec2 min(grid.x + column * (layout.thumbnail + layout.gap), grid.y + row * (layout.cellHeight + layout.gap));
            ImGui::PushID(static_cast<int>(key));
            bool favoriteClicked = false;
            if (DrawBrowserCell(getCell(key), layout, min, favoriteClicked))
            {
                selected = static_cast<int>(key);
            }
            if (favoriteClicked)
            {
                toggleFavorite(key);
                if (ui.favoritesOnly)
                {
                    ui.dirty = true;
                }
            }
            ImGui::PopID();
        }
        if (total == 0)
        {
            char empty[96];
            snprintf(empty, sizeof(empty), "No %ss match the current filters.", noun);
            const ImVec2 size = ImGui::CalcTextSize(empty);
            dl->AddText(ImVec2(grid.x + (layout.gridWidth - size.x) * 0.5f, grid.y + (layout.gridHeight - size.y) * 0.5f),
                ImGui::GetColorU32(Theme::g_textDim), empty);
        }
        cancelled = DrawBrowserFooter(total, noun, ui.page, pages, !blocked);
        ImGui::EndDisabled();
    }
    ImGui::End();
    PopModalStyle();
    return selected;
}

static ComponentInfoImage GetBrowserModelImage(int modelId)
{
    ComponentInfoImage image;
    ModelThumbnails::View thumbnail;
    if (ModelThumbnails::Get(modelId, thumbnail))
    {
        image.texture = thumbnail.texture;
        image.uvMin = ImVec2(thumbnail.uvMin[0], thumbnail.uvMin[1]);
        image.uvMax = ImVec2(thumbnail.uvMax[0], thumbnail.uvMax[1]);
    }
    return image;
}
