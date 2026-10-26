// Source Repository: https://github.com/waverso/waversomapstudio

class ThemeAction : public IAction
{
    public:
    explicit ThemeAction(unsigned option, std::optional<Theme::CustomTheme> other = {})
        : m_themeId(Theme::Colors(static_cast<Theme::Index>(option)).themeId), m_other(std::move(other)),
          m_nameBytes(std::max(Theme::g_customThemes[option - std::size(Theme::Presets)].name.capacity(),
                          m_other ? m_other->name.capacity() : 0)
              + 1)
    {
    }

    size_t MemoryUsage() const noexcept override
    {
        return sizeof(*this) + m_nameBytes;
    }

    void Apply() override
    {
        Exchange();
    }

    void Undo() override
    {
        Exchange();
    }

    void Focus() override
    {
        g_themesOpen = true;
        g_themeFocusId = m_themeId;
    }

    private:
    void Exchange()
    {
        const int option = Theme::FindIndexById(m_themeId);
        const unsigned presetCount = static_cast<unsigned>(std::size(Theme::Presets));
        if (option >= 0)
        {
            const auto current = static_cast<unsigned>(option);
            auto& theme = Theme::g_customThemes[current - presetCount];
            if (m_other)
            {
                std::swap(theme, *m_other);
                ++Theme::g_revision;
            }
            else
            {
                m_index = current - presetCount;
                m_position
                    = static_cast<unsigned>(std::ranges::find(Theme::g_displayOrder, current) - Theme::g_displayOrder.begin());
                m_hidden = Theme::g_hidden[current];
                m_selected = static_cast<unsigned>(Theme::g_selectedIndex) == current;
                m_other.emplace(std::move(theme));
                Theme::Delete(current);
            }
        }
        else if (m_other)
        {
            const unsigned index = std::min(m_index, static_cast<unsigned>(Theme::g_customThemes.size()));
            const unsigned restored = presetCount + index;
            Theme::g_customThemes.reserve(Theme::g_customThemes.size() + 1);
            Theme::g_hidden.reserve(Theme::g_hidden.size() + 1);
            Theme::g_displayOrder.reserve(Theme::g_displayOrder.size() + 1);
            Theme::g_customThemes.insert(Theme::g_customThemes.begin() + index, std::move(*m_other));
            m_other.reset();
            Theme::g_hidden.insert(Theme::g_hidden.begin() + restored, m_hidden);
            for (auto& value : Theme::g_displayOrder)
            {
                value += value >= restored;
            }
            Theme::g_displayOrder.insert(
                Theme::g_displayOrder.begin() + std::min(m_position, static_cast<unsigned>(Theme::g_displayOrder.size())),
                restored);
            const unsigned selected = static_cast<unsigned>(Theme::g_selectedIndex);
            Theme::g_selectedIndex = static_cast<Theme::Index>(m_selected ? restored : selected + (selected >= restored));
            ++Theme::g_revision;
        }
    }

    std::uint32_t m_themeId;
    std::optional<Theme::CustomTheme> m_other;
    size_t m_nameBytes;
    unsigned m_index = 0, m_position = 0;
    bool m_hidden = false, m_selected = false;
};

static void DrawThemesModal()
{
    static ItemListNavigation navigation;
    static char search[64] = {};
    static bool searchFocused = false;
    static int dragOption = -1;
    static bool dragActive = false;
    static bool showPressed = false;
    static int contextOption = -1;
    static int renameOption = -1;
    static int scrollOption = -1;
    static char renameBuf[64] = {};
    static bool focusRename = false;
    static std::vector<unsigned> visible;
    static std::optional<Theme::CustomTheme> colorBefore;
    static ImVec4 pickerColor;
    static bool colorCloned = false;
    if (!g_themesOpen)
    {
        navigation = {};
        dragOption = -1;
        dragActive = false;
        renameOption = -1;
        scrollOption = -1;
        colorBefore.reset();
        colorCloned = false;
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 display = io.DisplaySize;
    constexpr int colorCount = static_cast<int>(std::size(Theme::ColorFields));
    const float swatchesWidth = colorCount * Theme::g_frameHeight + (colorCount - 1) * Theme::g_padding;
    const ImVec2 size(floorf(ImMin(Ui::Px(400.0f), display.x - Theme::g_outerPadding * 2)),
        floorf(ImMin(Ui::Px(400.0f), display.y - Theme::g_outerPadding * 2)));
    DrawModalBackdrop("##ThemesBackdrop");
    ImGui::SetNextWindowPos(display * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    PushModalStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, Theme::g_padding));
    if (ImGui::Begin("##ThemesModal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        DrawModalTitleBar(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), size.x, "Themes");
        const bool blockedKeys
            = renameOption >= 0 || ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        const auto beginRename = [&](unsigned option)
        {
            if (option >= std::size(Theme::Presets))
            {
                renameOption = static_cast<int>(option);
                strncpy_s(renameBuf, Theme::Name(option), _TRUNCATE);
                focusRename = true;
                scrollOption = renameOption;
                if (!TextSearch::ContainsAsciiNoCase(Theme::Name(option), search))
                {
                    search[0] = '\0';
                }
            }
        };
        const auto cloneTheme = [&](unsigned option)
        {
            const unsigned cloned = Theme::Clone(option);
            Theme::g_selectedIndex = static_cast<Theme::Index>(cloned);
            std::unique_ptr<IAction> action = std::make_unique<ThemeAction>(cloned);
            g_userActions->RecordApplied(action);
            scrollOption = static_cast<int>(cloned);
            if (!TextSearch::ContainsAsciiNoCase(Theme::Name(cloned), search))
            {
                search[0] = '\0';
            }
        };
        const auto deleteTheme = [&](unsigned option)
        {
            if (option >= std::size(Theme::Presets))
            {
                g_userActions->Execute(std::make_unique<ThemeAction>(option));
                contextOption = renameOption = -1;
            }
        };
        if (ImGui::IsWindowAppearing())
        {
            search[0] = '\0';
            searchFocused = false;
            ImGui::SetWindowFocus();
        }
        const float width = size.x - Theme::g_outerPadding * 2;
        ImGui::SetCursorPos(ImVec2(Theme::g_outerPadding, Theme::g_tabHeight + Theme::g_padding));
        if (g_themesFocusSearch)
        {
            ImGui::SetKeyboardFocusHere();
            g_themesFocusSearch = false;
        }
        if (SearchInput("##themes_search", "Search", search, sizeof(search), width, searchFocused))
        {
            navigation = {};
            dragOption = -1;
            dragActive = false;
            ImGui::SetNextWindowScroll(ImVec2(0, 0));
        }
        if (!blockedKeys && !searchFocused)
        {
            const unsigned selected = static_cast<unsigned>(Theme::g_selectedIndex);
            if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
            {
                beginRename(selected);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            {
                deleteTheme(selected);
            }
            else if (io.KeyCtrl && !io.KeyAlt && !io.KeyShift && !io.KeySuper)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_C, false))
                {
                    cloneTheme(selected);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
                {
                    ActionHistoryUndo();
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
                {
                    ActionHistoryRedo();
                }
            }
        }
        if (g_themeFocusId)
        {
            navigation = {};
            scrollOption = Theme::FindIndexById(g_themeFocusId);
            if (scrollOption < 0)
            {
                scrollOption = static_cast<int>(Theme::g_selectedIndex);
            }
            g_themeFocusId = 0;
            search[0] = '\0';
            renameOption = dragOption = -1;
            dragActive = false;
        }
        ImGui::SetCursorPosX(Theme::g_outerPadding);
        const float swatchY = size.y - ModalFooterHeight() - Theme::g_itemSpacing - Theme::g_frameHeight;
        const float listHeight = swatchY - Theme::g_padding - ImGui::GetCursorPosY();
        const float rowH = Ui::Px(21.0f);
        ImVec2 scrollRange(0, 0);
        visible.clear();
        for (unsigned option : Theme::g_displayOrder)
        {
            if (TextSearch::ContainsAsciiNoCase(Theme::Name(option), search))
            {
                if (option == scrollOption)
                {
                    const float y = static_cast<float>(visible.size()) * rowH;
                    scrollRange = ImVec2(y, y + rowH);
                    scrollOption = -1;
                }
                visible.push_back(option);
            }
        }
        const bool keyboard = !blockedKeys && renameOption < 0 && !dragActive && !SampTextInputActive()
            && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && (!ImGui::IsAnyItemActive() || searchFocused);
        const auto selectedOption = std::ranges::find(visible, static_cast<unsigned>(Theme::g_selectedIndex));
        const int selectedRow = selectedOption == visible.end() ? -1 : static_cast<int>(selectedOption - visible.begin());
        const int keyRow = NavigateItemList("##ThemesList", navigation, static_cast<int>(visible.size()), selectedRow, keyboard);
        if (keyRow >= 0)
        {
            scrollRange = ImVec2(keyRow * rowH, (keyRow + 1) * rowH);
        }
        if (keyboard && !g_popupClosedOnEscape
            && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
        {
            const int row = navigation.hover >= 0 ? navigation.hover : selectedRow;
            if (row >= 0)
            {
                Theme::g_selectedIndex = static_cast<Theme::Index>(visible[row]);
            }
        }
        BeginItemList("##ThemesList", ImVec2(width, ImMax(1.0f, listHeight)), visible.size(), scrollRange, keyRow < 0);
        const float dragThreshold = Ui::Px(5.0f);
        const bool mouseDragged = ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left, dragThreshold)
            || ImLengthSqr(io.MousePos - io.MouseClickedPos[ImGuiMouseButton_Left]) >= dragThreshold * dragThreshold;
        if (dragOption >= 0 && mouseDragged)
        {
            dragActive = true;
        }
        int dropTarget = -1;
        SceneDropMode dropMode = SceneDropNone;
        bool hoveredRow = false;
        bool openContext = false;
        const float rowWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 rowsPos = ImGui::GetCursorScreenPos();
        ImVec2 dropMouse;
        const bool dropInside = ItemListDropPosition(rowWidth, dropMouse);
        const float customWidth = ImGui::CalcTextSize("Custom").x;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        DrawClippedOutlinerRows(
            static_cast<int>(visible.size()), rowH,
            [&](int row)
            {
                return visible[row] == dragOption || visible[row] == renameOption;
            },
            [&](int index)
            {
                const unsigned option = visible[index];
                ImGui::PushID(static_cast<int>(option));
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const ImRect row(pos, pos + ImVec2(rowWidth, rowH));
                const bool renaming = renameOption == option;
                if (renaming)
                {
                    ImGui::SetNextItemAllowOverlap();
                }
                const bool clicked = ImGui::InvisibleButton("##row", row.GetSize());
                const bool mouseOver = ImGui::IsMouseHoveringRect(row.Min, row.Max);
                const bool hovered = dragActive ? mouseOver : ImGui::IsItemHovered();
                const ImRect showRect(row.Min, ImVec2(row.Min.x + Theme::g_iconSize + Theme::g_padding * 2, row.Max.y));
                const bool overShow = ImGui::IsMouseHoveringRect(showRect.Min, showRect.Max);
                hoveredRow |= mouseOver;
                const bool highlight = index == navigation.hover && (!navigation.keyboard || keyboard) && !dragActive;
                DrawItemListRowBackground(
                    ImGui::GetWindowDrawList(), row, index, option == static_cast<unsigned>(Theme::g_selectedIndex), highlight);
                if (clicked && !renaming && !dragActive && !mouseDragged)
                {
                    if (showPressed && overShow)
                    {
                        Theme::g_hidden[option] = !Theme::g_hidden[option];
                        ++Theme::g_revision;
                    }
                    else if (!showPressed && !overShow)
                    {
                        Theme::g_selectedIndex = static_cast<Theme::Index>(option);
                    }
                }
                if (hovered && !renaming && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    showPressed = overShow;
                    dragOption = overShow ? -1 : static_cast<int>(option);
                }
                if (hovered && !renaming && !dragActive && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    contextOption = static_cast<int>(option);
                    openContext = true;
                }
                if (dragActive && dropInside && row.Contains(dropMouse) && dragOption != option)
                {
                    dropTarget = static_cast<int>(option);
                    dropMode = ItemListDropMode(row, false);
                    DrawItemListDropPreview(ImGui::GetForegroundDrawList(), row, dropMode);
                }
                const ImU32 color = Theme::g_hidden[option] ? Theme::Alpha(Theme::g_text, 96) : Theme::g_text;
                DrawIconCentered(ImGui::GetWindowDrawList(),
                    Theme::g_hidden[option] ? g_icons->hideTexture : g_icons->showTexture, showRect.Min, showRect.Max,
                    Theme::g_iconSize, hovered && overShow ? Theme::g_textHover : color);
                const float textY = row.Min.y + floorf((rowH - ImGui::GetFontSize()) * 0.5f);
                const bool custom = option >= std::size(Theme::Presets);
                const float customX = row.Max.x - Theme::g_padding - customWidth;
                const float textRight = custom ? customX - Theme::g_padding : row.Max.x - Theme::g_padding;
                if (renaming)
                {
                    ImGui::SetCursorScreenPos(ImVec2(showRect.Max.x, row.Min.y + Ui::Hairline()));
                    ImGui::SetNextItemWidth(ImMax(Ui::Px(40), textRight - showRect.Max.x));
                    const bool askedFocus = focusRename;
                    if (focusRename)
                    {
                        ImGui::SetWindowFocus();
                        ImGui::SetKeyboardFocusHere();
                        focusRename = false;
                    }
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding * 0.5f, Ui::Px(2)));
                    const bool enter = ImGui::InputText("##rename", renameBuf, sizeof(renameBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
                    const bool active = ImGui::IsItemActive();
                    ImGui::PopStyleVar();
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !g_popupClosedOnEscape)
                    {
                        renameOption = -1;
                    }
                    else if (enter || (!active && !askedFocus))
                    {
                        auto before = Theme::g_customThemes[option - std::size(Theme::Presets)];
                        const bool renamed = Theme::Rename(option, renameBuf);
                        if (renamed && before.name != renameBuf)
                        {
                            std::unique_ptr<IAction> action = std::make_unique<ThemeAction>(option, std::move(before));
                            g_userActions->RecordApplied(action);
                        }
                        if (renamed || !enter)
                        {
                            renameOption = -1;
                        }
                        else
                        {
                            g_toast.ShowError("Theme names must be nonempty and unique.");
                            focusRename = true;
                        }
                    }
                }
                else
                {
                    const ImVec4 clip(showRect.Max.x, row.Min.y, textRight, row.Max.y);
                    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(showRect.Max.x, textY),
                        highlight && !Theme::g_hidden[option] ? Theme::g_textHover : color, Theme::Name(option), nullptr, 0.0f,
                        &clip);
                }
                if (custom)
                {
                    ImGui::GetWindowDrawList()->AddText(ImVec2(customX, textY), Theme::g_textDim, "Custom");
                }
                ImGui::PopID();
            });
        ImGui::PopStyleVar();
        const ImVec2 listPos = ImGui::GetWindowPos();
        const ImRect bounds(listPos, listPos + ImVec2(rowWidth, ImGui::GetWindowHeight()));
        if (dragActive)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Drag);
            const float lastBottom = rowsPos.y + static_cast<float>(visible.size()) * rowH;
            if (dropInside && io.MousePos.y >= lastBottom)
            {
                dropMode = SceneDropRoot;
                dropTarget = -1;
                DrawItemListDropPreview(
                    ImGui::GetForegroundDrawList(), ImRect(bounds.Min, ImVec2(bounds.Max.x, lastBottom)), dropMode);
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && dropInside && dropMode != SceneDropNone)
            {
                auto& order = Theme::g_displayOrder;
                auto from = std::ranges::find(order, static_cast<unsigned>(dragOption));
                auto to = dropTarget < 0 ? order.end() : std::ranges::find(order, static_cast<unsigned>(dropTarget));
                if (dropMode == SceneDropAfter)
                {
                    ++to;
                }
                if (from < to)
                {
                    std::rotate(from, from + 1, to);
                }
                else
                {
                    std::rotate(to, from, from + 1);
                }
                ++Theme::g_revision;
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            dragOption = -1;
            dragActive = false;
        }
        if (!hoveredRow && !dragActive && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            contextOption = -1;
            openContext = true;
        }
        if (openContext)
        {
            ImGui::OpenPopup("##themes_context");
        }
        if (ImCustom::BeginPopup("##themes_context"))
        {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            const bool canShowAll = std::ranges::find(Theme::g_hidden, true) != Theme::g_hidden.end();
            const bool canHideAll = std::ranges::find(Theme::g_hidden, false) != Theme::g_hidden.end();
            if (ImGui::MenuItem("Show All", nullptr, false, canShowAll))
            {
                std::fill(Theme::g_hidden.begin(), Theme::g_hidden.end(), false);
                ++Theme::g_revision;
            }
            if (ImGui::MenuItem("Hide All", nullptr, false, canHideAll))
            {
                std::fill(Theme::g_hidden.begin(), Theme::g_hidden.end(), true);
                ++Theme::g_revision;
            }
            if (ImGui::BeginMenu("Sort"))
            {
                ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
                if (ImGui::MenuItem("Alphabetical"))
                {
                    Theme::Sort(Theme::SortOrder::Alphabetical);
                }
                if (ImGui::MenuItem("Original Order"))
                {
                    Theme::Sort(Theme::SortOrder::Original);
                }
                if (ImGui::MenuItem("Visible First"))
                {
                    Theme::Sort(Theme::SortOrder::Visible);
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            const bool custom = contextOption >= static_cast<int>(std::size(Theme::Presets));
            if (ContextMenuItem("Rename", "F2", custom))
            {
                beginRename(static_cast<unsigned>(contextOption));
            }
            if (ContextMenuItem("Clone", "Ctrl+C", contextOption >= 0))
            {
                cloneTheme(static_cast<unsigned>(contextOption));
            }
            if (ContextMenuItem("Delete", "Del", custom))
            {
                deleteTheme(static_cast<unsigned>(contextOption));
            }
            ImCustom::EndPopup();
        }
        EndItemList();
        ImGui::SetCursorPos(ImVec2(floorf((size.x - swatchesWidth) * 0.5f), swatchY));
        static constexpr const char* colorNames[]
            = { "Window Background", "Panel Background", "Control Background", "Border", "Text", "Secondary Text", "Accent" };
        static_assert(std::size(colorNames) == std::size(Theme::ColorFields));
        unsigned selected = static_cast<unsigned>(Theme::g_selectedIndex);
        bool colorPopupOpen = false;
        for (int i = 0; i < static_cast<int>(std::size(colorNames)); ++i)
        {
            if (i)
            {
                ImGui::SameLine(0, Theme::g_padding);
            }
            ImGui::PushID(i);
            const ImU32 color = Theme::Colors(Theme::g_selectedIndex).*Theme::ColorFields[i];
            const bool clicked = ColorSwatchButton("##theme_swatch", color, ImVec2(Theme::g_frameHeight, Theme::g_frameHeight));
            const ImVec2 swatchMin = ImGui::GetItemRectMin(), swatchMax = ImGui::GetItemRectMax();
            if (ImGui::IsItemHovered())
            {
                SetStyledTooltip(colorNames[i]);
            }
            if (clicked)
            {
                colorBefore.reset();
                colorCloned = false;
                if (selected >= std::size(Theme::Presets))
                {
                    colorBefore = Theme::g_customThemes[selected - std::size(Theme::Presets)];
                }
                pickerColor = ImGui::ColorConvertU32ToFloat4(color);
                ImGui::OpenPopup("##theme_color_popup");
            }
            if (ImGui::IsPopupOpen("##theme_color_popup"))
            {
                colorPopupOpen = true;
                const ImVec2 pickerSize = ColorPickerPopupSize(false);
                ImGui::SetNextWindowPos(ColorPopupPosition(swatchMin, swatchMax, pickerSize, Ui::Px(2)), ImGuiCond_Always);
                ImGui::SetNextWindowSize(pickerSize, ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_padding, Theme::g_padding));
                if (ImGui::BeginPopup("##theme_color_popup", ImGuiWindowFlags_NoMove))
                {
                    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::ColorPicker4("##cp", &pickerColor.x,
                            ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_DisplayRGB
                                | ImGuiColorEditFlags_NoDragDrop))
                    {
                        const ImU32 changed = ImGui::ColorConvertFloat4ToU32(pickerColor);
                        if (color != changed)
                        {
                            if (selected < std::size(Theme::Presets))
                            {
                                selected = Theme::Clone(selected);
                                Theme::g_selectedIndex = static_cast<Theme::Index>(selected);
                                colorCloned = true;
                                scrollOption = static_cast<int>(selected);
                                if (!TextSearch::ContainsAsciiNoCase(Theme::Name(selected), search))
                                {
                                    search[0] = '\0';
                                }
                            }
                            Theme::g_customThemes[selected - std::size(Theme::Presets)].colors.*Theme::ColorFields[i] = changed;
                            ++Theme::g_revision;
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();
            }
            ImGui::PopID();
        }
        if ((colorBefore || colorCloned) && !colorPopupOpen)
        {
            const int option = colorBefore ? Theme::FindIndexById(colorBefore->colors.themeId) : static_cast<int>(selected);
            if (option >= static_cast<int>(std::size(Theme::Presets))
                && (colorCloned || Theme::Colors(static_cast<Theme::Index>(option)) != colorBefore->colors))
            {
                std::unique_ptr<IAction> action
                    = std::make_unique<ThemeAction>(static_cast<unsigned>(option), std::move(colorBefore));
                g_userActions->RecordApplied(action);
            }
            colorBefore.reset();
            colorCloned = false;
        }
        DrawModalFooter();
        if (ImGui::Button("Close##themes", ImVec2(size.x - Theme::g_outerPadding * 2, Theme::g_frameHeight))
            || (!blockedKeys && !g_popupClosedOnEscape && ImGui::IsKeyPressed(ImGuiKey_Escape, false)))
        {
            g_themesOpen = false;
        }
    }
    ImGui::End();
    PopModalStyle(1);
    if (g_themesOpen)
    {
        ImGui::SetNextFrameWantCaptureKeyboard(true);
    }
}
