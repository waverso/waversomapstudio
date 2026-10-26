// Source Repository: https://github.com/waverso/waversomapstudio

struct AnimationBrowserUiState
{
    ItemListNavigation navigation;
    ComboSearch librarySearch;
    AnimationBrowser::Filter filter;
    AnimationBrowser::Filter draft;
    std::vector<int> results;
    int cursor = 0;
    bool dirty = true;
    bool scrollToCursor = false;
    bool focusSearch = false;
    bool searchFocused = false;
    char rangeInt[4][16] = {};
    bool rangeIntFocus[4] = {};
    char rangeFloat[2][32] = {};
    bool rangeFloatFocus[2] = {};
};
static AnimationBrowserUiState g_animationBrowserUi;

static bool AnimationFiltersActive(const AnimationBrowser::Filter& filter)
{
    const AnimationBrowser::Filter defaults;
    return filter.searchFields != defaults.searchFields || filter.missing != defaults.missing || filter.bugged != defaults.bugged
        || filter.libraries != defaults.libraries || filter.idMin != defaults.idMin || filter.idMax != defaults.idMax
        || filter.framesMin != defaults.framesMin || filter.framesMax != defaults.framesMax
        || filter.durationMin != defaults.durationMin || filter.durationMax != defaults.durationMax;
}

static void DrawAnimationFiltersPopup(float width)
{
    auto& ui = g_animationBrowserUi;
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
    if (!ImGui::BeginPopup("##animation_filters",
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        return;
    }
    ImGuiContext& context = *ImGui::GetCurrentContext();
    const int comboLevel = context.BeginPopupStack.Size;
    const bool enter = context.OpenPopupStack.Size == comboLevel && !g_popupClosedOnEscape && !ImGui::GetIO().WantTextInput
        && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
    if (context.OpenPopupStack.Size == comboLevel + 1
        && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
    {
        ImGuiWindow* filters = ImGui::GetCurrentWindow();
        const ImGuiWindow* combo = context.OpenPopupStack.back().Window;
        if (combo && combo->ParentWindow == filters && !filters->Rect().Contains(context.IO.MousePos)
            && !combo->Rect().Contains(context.IO.MousePos))
        {
            ImGui::ClosePopupToLevel(comboLevel, true);
            ImGui::SetActiveID(filters->MoveId, filters);
        }
    }
    auto& filter = ui.draft;
    const float inner = ImGui::GetContentRegionAvail().x;
    const float half = (inner - Theme::g_popupPadding) * 0.5f;
    const char* indexing[] = { "Library Name", "Anim ID", "Anim Name" };
    char parts[256] = {}, raw[288], label[288];
    for (int i = 0; i < AnimationCatalog::LibraryCount; ++i)
    {
        if (filter.libraries[i])
        {
            AppendLabelPart(parts, sizeof(parts), AnimationCatalog::Libraries[i]);
        }
    }
    snprintf(raw, sizeof(raw), "Library: %s", *parts ? parts : "All");
    EllipsizeComboLabel(label, sizeof(label), raw, inner);
    if (ImCustom::BeginCombo("##animation_libraries", label, inner, true))
    {
        ImCustom::BeginComboList(ui.librarySearch.Draw());
        if (ImCustom::ComboMultiItem("All", !*parts))
        {
            filter.libraries.fill(false);
        }
        for (int i = 0; i < AnimationCatalog::LibraryCount; ++i)
        {
            if (!ui.librarySearch.Matches(AnimationCatalog::Libraries[i]))
            {
                continue;
            }
            if (ImCustom::ComboMultiItem(AnimationCatalog::Libraries[i], filter.libraries[i]))
            {
                filter.libraries[i] = !filter.libraries[i];
            }
        }
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetScrollY(0.0f);
        }
        ImCustom::EndComboList();
        ImCustom::EndCombo();
    }
    DrawMaskFilter(
        "##animation_indexing", "Search In", filter.searchFields, 3,
        [&](int i)
        {
            return indexing[i];
        },
        inner, nullptr, AnimationBrowser::SearchName, "");

    DrawVisibilityFilter("##animation_missing", "Unavailable", filter.missing, half);
    ImGui::SameLine(0, Theme::g_popupPadding);
    DrawVisibilityFilter("##animation_bugged", "Deformed", filter.bugged, half);
    RangeSliderInt("##animation_id_range", "Anim ID", &filter.idMin, &filter.idMax, 1, AnimationCatalog::Count, inner,
        ui.rangeInt[0], ui.rangeInt[1], ui.rangeIntFocus[0], ui.rangeIntFocus[1], Ui::Px(64.0f), Theme::g_popupPadding);
    RangeSliderInt("##animation_frames_range", "Frames", &filter.framesMin, &filter.framesMax, 0, AnimationCatalog::MaxFrames,
        inner, ui.rangeInt[2], ui.rangeInt[3], ui.rangeIntFocus[2], ui.rangeIntFocus[3], Ui::Px(64.0f), Theme::g_popupPadding);
    RangeSliderFloat("##animation_duration_range", "Duration", &filter.durationMin, &filter.durationMax, 0.0f,
        AnimationCatalog::MaxDuration, inner, ui.rangeFloat[0], ui.rangeFloat[1], ui.rangeFloatFocus[0], ui.rangeFloatFocus[1],
        FloatRangeCurve, Ui::Px(64.0f), Theme::g_popupPadding);
    const ImVec2 rangeMin = ImGui::GetItemRectMin(), rangeMax = ImGui::GetItemRectMax();
    const float separatorY = rangeMax.y + Theme::g_popupPadding;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(rangeMin.x, separatorY), ImVec2(rangeMax.x, separatorY), ImGui::GetColorU32(Theme::g_border), Ui::Hairline());
    ImGui::SetCursorScreenPos(ImVec2(rangeMin.x, separatorY + Ui::Hairline() + Theme::g_popupPadding));
    if (ImGui::Button("Apply Filters##animation", ImVec2(inner, Theme::g_frameHeight)) || enter)
    {
        ui.filter = ui.draft;
        ui.dirty = true;
        ui.cursor = 0;
        ui.scrollToCursor = true;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

static void DrawAnimationResultCount(int count, float width, float gap)
{
    char text[48];
    snprintf(text, sizeof(text), "Animations Found: %d", count);
    ImGui::PushFont(g_gui->m_fontSmall);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, gap));
    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const float left = floorf(min.x + (width - size.x) * 0.5f);
    ImGui::GetWindowDrawList()->AddText(ImVec2(left, min.y), ImGui::GetColorU32(Theme::g_textDim), text);
    ImGui::Dummy(ImVec2(width, size.y));
    ImGui::SetCursorScreenPos(ImVec2(min.x, min.y + size.y + gap));
    ImGui::PopStyleVar();
    ImGui::PopFont();
}

static void DrawAnimationBrowserPopup(ActorAnimationData& data, const ImRect& anchor, int itemIdx)
{
    if (!ImGui::IsPopupOpen("##actor_animation_popup"))
    {
        return;
    }
    auto& ui = g_animationBrowserUi;
    const auto display = ImGui::GetIO().DisplaySize;
    const float width = ImMin(Ui::Px(310.0f), display.x - Theme::g_outerPadding * 2.0f);
    const float rowHeight = Ui::Px(21.0f);
    const float height = ImMin(ceilf(Ui::Px(440.0f)), display.y - Theme::g_outerPadding * 2.0f);
    float x = anchor.Max.x + Ui::Hairline();
    if (x + width > display.x - Theme::g_outerPadding)
    {
        x = anchor.Min.x - width - Ui::Hairline();
    }
    x = ImClamp(x, Theme::g_outerPadding, display.x - width - Theme::g_outerPadding);
    const float y = ImClamp(anchor.Min.y, Theme::g_outerPadding, display.y - height - Theme::g_outerPadding);
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
    if (ImGui::BeginPopup(
            "##actor_animation_popup", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar))
    {
        const float inner = ImGui::GetContentRegionAvail().x;
        const auto* current = AnimationCatalog::Find(data.library, data.name);
        bool bulkSelected = false;
        bool changed = false;
        if (ui.focusSearch)
        {
            ImGui::SetKeyboardFocusHere();
            ui.focusSearch = false;
        }
        changed
            |= SearchInput("##animation_search", "Search", ui.filter.search, sizeof(ui.filter.search), inner, ui.searchFocused);
        const float filtersWidth = ImGui::CalcTextSize("Filters").x + Theme::g_padding * 2.0f;
        const float sortWidth = inner - filtersWidth - Theme::g_frameHeight * 3.0f - Theme::g_popupPadding * 4.0f;
        const char* sortNames[] = { "Anim Name", "Library", "Anim ID", "Frames", "Duration" };
        char label[80], sortLabel[80];
        snprintf(sortLabel, sizeof(sortLabel), "Sort: %s", sortNames[ui.filter.sortKey]);
        EllipsizeComboLabel(label, sizeof(label), sortLabel, sortWidth);
        if (ImCustom::BeginCombo("##animation_sort", label, sortWidth))
        {
            for (int i = 0; i < IM_ARRAYSIZE(sortNames); ++i)
            {
                if (ImCustom::ComboItem(sortNames[i], ui.filter.sortKey == i))
                {
                    ui.filter.sortKey = i;
                    changed = true;
                }
            }
            ImCustom::EndCombo();
        }
        ImGui::SameLine(0, Theme::g_popupPadding);
        if (ImGui::Button("##animation_direction", ImVec2(Theme::g_frameHeight, Theme::g_frameHeight)))
        {
            ui.filter.sortDesc = !ui.filter.sortDesc;
            changed = true;
        }
        const auto directionMin = ImGui::GetItemRectMin(), directionMax = ImGui::GetItemRectMax();
        DrawRotatedArrow(ImGui::GetWindowDrawList(),
            ImVec2((directionMin.x + directionMax.x) * 0.5f, (directionMin.y + directionMax.y) * 0.5f),
            ui.filter.sortDesc ? 1.0f : -1.0f, ImGui::GetColorU32(Theme::g_text));
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            SetStyledTooltip(ui.filter.sortDesc ? "Descending" : "Ascending");
        }
        ImGui::SameLine(0, Theme::g_popupPadding);
        const bool favoritesOnly = ui.filter.favoritesOnly;
        if (favoritesOnly)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, Theme::g_success);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::g_successHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::g_successActive);
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::g_statusText);
        }
        if (ComponentIconButton("##animation_favorites", g_icons ? g_icons->heartTexture : nullptr,
                favoritesOnly ? Theme::g_statusText : Theme::g_text))
        {
            ui.filter.favoritesOnly = !ui.filter.favoritesOnly;
            changed = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            SetStyledTooltip("Favorites Only");
        }
        if (favoritesOnly)
        {
            ImGui::PopStyleColor(4);
        }
        ImGui::SameLine(0, Theme::g_popupPadding);
        if (AnimationFiltersActive(ui.filter))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::g_accent);
        }
        if (ImGui::Button("Filters##animation", ImVec2(filtersWidth, Theme::g_frameHeight)))
        {
            ui.draft = ui.filter;
            std::fill(std::begin(ui.rangeIntFocus), std::end(ui.rangeIntFocus), false);
            std::fill(std::begin(ui.rangeFloatFocus), std::end(ui.rangeFloatFocus), false);
            ImGui::OpenPopup("##animation_filters");
        }
        if (AnimationFiltersActive(ui.filter))
        {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine(0, Theme::g_popupPadding);
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::g_danger);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::g_dangerHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::g_dangerActive);
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::g_statusText);
        const bool clearFilters
            = ComponentIconButton("##animation_clear", g_icons ? g_icons->closeTexture : nullptr, Theme::g_statusText);
        if (clearFilters)
        {
            ui.filter = {};
            changed = true;
            ImGui::ClearActiveID();
        }
        const float toolbarBottom = ImGui::GetItemRectMax().y;
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            SetStyledTooltip("Clear Filters");
        }
        if (changed)
        {
            ui.dirty = true;
            ui.cursor = 0;
        }
        const bool appearing = ImGui::IsWindowAppearing();
        const bool resetScroll = ui.scrollToCursor || changed || appearing;
        if (resetScroll)
        {
            ui.navigation = {};
        }
        ui.scrollToCursor = false;
        if (ui.dirty)
        {
            AnimationBrowser::RunFilter(ui.filter, ui.results);
            ui.dirty = false;
        }
        if (clearFilters)
        {
            const auto found = std::find(ui.results.begin(), ui.results.end(), current ? current->id : 0);
            ui.cursor = found == ui.results.end() ? 0 : static_cast<int>(found - ui.results.begin());
        }
        const int total = static_cast<int>(ui.results.size());
        ui.cursor = ImClamp(ui.cursor, 0, ImMax(0, total - 1));
        const bool keyboard = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && !ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId);
        const ImVec2 popupPos = ImGui::GetWindowPos();
        ImGui::SetCursorScreenPos(ImVec2(popupPos.x + Theme::g_popupPadding, toolbarBottom + Theme::g_popupPadding));
        DrawAnimationResultCount(total, inner, Theme::g_popupPadding);
        const float listHeight = ImMax(rowHeight,
            floorf(popupPos.y + ImGui::GetWindowHeight() - Theme::g_popupPadding) - floorf(ImGui::GetCursorScreenPos().y));
        const auto found = std::find(ui.results.begin(), ui.results.end(), current ? current->id : 0);
        const int selectedRow = found == ui.results.end() ? 0 : static_cast<int>(found - ui.results.begin());
        const int keyRow = NavigateItemList("##animation_list", ui.navigation, total, selectedRow, keyboard);
        if (keyRow >= 0)
        {
            ui.cursor = keyRow;
        }
        const ImVec2 scrollRange
            = total && (resetScroll || keyRow >= 0) ? ImVec2(ui.cursor * rowHeight, (ui.cursor + 1) * rowHeight) : ImVec2(0, 0);
        bool selected = false;
        BeginItemList("##animation_list", ImVec2(inner, listHeight), total, scrollRange, appearing || clearFilters);
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            const float fontSize = ImGui::GetFontSize();
            const float idWidth = ImGui::CalcTextSize("1812").x + Theme::g_padding * 2.0f;
            const float durationWidth = ImGui::CalcTextSize("51.47s").x + Theme::g_padding;
            const float listWidth = ImGui::GetContentRegionAvail().x;
            DrawClippedOutlinerRows(
                total, rowHeight,
                [](int)
                {
                    return false;
                },
                [&](int index)
                {
                    const auto& animation = *AnimationCatalog::Get(ui.results[index]);
                    ImGui::PushID(animation.id);
                    const ImVec2 min = ImGui::GetCursorScreenPos();
                    const ImVec2 max(min.x + listWidth, min.y + rowHeight);
                    const bool click = ImGui::InvisibleButton("##animation_row", ImVec2(listWidth - rowHeight, rowHeight));
                    const bool hover = index == ui.navigation.hover && (!ui.navigation.keyboard || keyboard);
                    auto* dl = ImGui::GetWindowDrawList();
                    const bool active = current && current->id == animation.id;
                    DrawItemListRowBackground(dl, ImRect(min, max), index, active, hover);
                    char id[16], name[112], duration[24];
                    snprintf(id, sizeof(id), "%d", animation.id);
                    snprintf(name, sizeof(name), "%s > %s", AnimationCatalog::Libraries[animation.library], animation.name);
                    char visible[112];
                    EllipsizeText(visible, sizeof(visible), name,
                        listWidth - idWidth - durationWidth - rowHeight - Theme::g_padding, fontSize);
                    AnimationBrowser::FormatDuration(duration, sizeof(duration), animation.duration);
                    const float textY = min.y + floorf((rowHeight - fontSize) * 0.5f);
                    dl->AddText(ImVec2(min.x + Theme::g_padding, textY), ImGui::GetColorU32(Theme::g_textDim), id);
                    const ImU32 textColor = animation.missing != AnimationCatalog::Missing::None ? Theme::g_textDim
                        : hover                                                                  ? Theme::g_textHover
                                                                                                 : Theme::g_text;
                    dl->AddText(ImVec2(min.x + idWidth, textY), ImGui::GetColorU32(textColor), visible);
                    dl->AddText(ImVec2(max.x - rowHeight - Theme::g_padding - ImGui::CalcTextSize(duration).x, textY),
                        ImGui::GetColorU32(Theme::g_textDim), duration);
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                    {
                        char tooltip[320];
                        snprintf(tooltip, sizeof(tooltip), "%s\n%s\n%d frames\n%s duration", animation.name,
                            AnimationCatalog::Libraries[animation.library], animation.frames, duration);
                        ImGui::PushFont(g_gui->m_fontRegular);
                        SetBrowserTooltip(tooltip);
                        ImGui::PopFont();
                    }
                    ImGui::SameLine(0, 0);
                    ImGui::SetCursorScreenPos(ImVec2(max.x - rowHeight, min.y));
                    if (ImGui::InvisibleButton("##animation_favorite", ImVec2(rowHeight, rowHeight)))
                    {
                        AnimationBrowser::ToggleFavorite(animation.id);
                        ui.dirty = true;
                    }
                    DrawIconCentered(dl, g_icons ? g_icons->heartTexture : nullptr, ImVec2(max.x - rowHeight, min.y), max,
                        Theme::g_smallIconSize,
                        ImGui::GetColorU32(
                            AnimationBrowser::IsFavorite(animation.id) ? Theme::g_chipFavorited : Theme::g_textDim));
                    if (click)
                    {
                        bulkSelected = InspectionConfirmAnimation(animation.id);
                        if (!bulkSelected)
                        {
                            AnimationBrowser::Select(data, animation.id);
                        }
                        ui.cursor = index;
                        ui.navigation.hover = index;
                        ui.navigation.keyboard = false;
                        selected = true;
                    }
                    ImGui::PopID();
                });
            if (!total)
            {
                const ImVec2 size = ImGui::CalcTextSize("No animations found.");
                const ImVec2 pos = ImGui::GetWindowPos();
                ImGui::SetCursorScreenPos(
                    ImVec2(pos.x + floorf((listWidth - size.x) * 0.5f), pos.y + floorf((listHeight - size.y) * 0.5f)));
                ImGui::TextDisabled("No animations found.");
            }
            ImGui::PopStyleVar();
        }
        EndItemList();
        if (keyboard && !g_popupClosedOnEscape && !SampTextInputActive() && total
            && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
        {
            ui.cursor = ui.navigation.hover >= 0 ? ui.navigation.hover : selectedRow;
            bulkSelected = InspectionConfirmAnimation(ui.results[ui.cursor]);
            if (!bulkSelected)
            {
                AnimationBrowser::Select(data, ui.results[ui.cursor]);
            }
            selected = true;
        }
        DrawAnimationFiltersPopup(ImMin(Ui::Px(360.0f), display.x - Theme::g_outerPadding * 2.0f));
        if (selected && g_actorRuntime && !bulkSelected)
        {
            g_actorRuntime->ReplayAnimation(itemIdx);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(3);
}

static void DrawAnimationComponentInfoCard(const ActorAnimationData& data, float innerW)
{
    const auto* animation = AnimationCatalog::Find(data.library, data.name);
    if (!animation)
    {
        return;
    }
    char frames[16], duration[24];
    snprintf(frames, sizeof(frames), "%d", animation->frames);
    AnimationBrowser::FormatDuration(duration, sizeof(duration), animation->duration);
    const ComponentInfoRow rows[3]
        = { { "Library", AnimationCatalog::Libraries[animation->library] }, { "Frames", frames }, { "Duration", duration } };
    const bool favorite = AnimationBrowser::IsFavorite(animation->id);
    const auto result = DrawComponentInfoCard(
        "##animation_info", innerW, animation->name, nullptr, 0, rows,
        []
        {
            return ComponentInfoImage {};
        },
        g_icons ? g_icons->heartTexture : nullptr, favorite ? Theme::g_chipFavorited : Theme::g_textDim, true, false, false);
    if (result.action)
    {
        AnimationBrowser::ToggleFavorite(animation->id);
        g_animationBrowserUi.dirty = true;
    }
}
