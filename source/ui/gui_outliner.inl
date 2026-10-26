// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    static void DrawOutlinerRows(const char* search, SearchMode mode, float width)
    {
        EditorScene& sc = *g_scene;
        OutlinerUiState& ui = g_outlinerUi;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const bool filtering = search && search[0];
        const float rowH = Ui::Px(21.0f);
        const float indentW = Ui::Px(14.0f);
        const float iconW = Ui::Px(16.0f);
        ImVec2 dropMouse;
        const bool dropInside = ItemListDropPosition(width, dropMouse);

        const auto& rows = g_outlinerRows.rows;
        DrawClippedOutlinerRows(
            static_cast<int>(rows.size()), rowH,
            [&](int row)
            {
                const int index = rows[row].index;
                return index == ui.renameIndex || index == ui.dragIndex;
            },
            [&](int rowIndex)
            {
                const int idx = rows[rowIndex].index;
                const int depth = rows[rowIndex].depth;
                SceneItem& it = sc.items[idx];
                ui.visibleRow = rowIndex;

                ImGui::PushID(it.id);

                const bool hasChildren = sc.HasChildren(idx);
                const bool selfMatch = rows[rowIndex].selfMatch;
                const ImVec2 rowPos = ImGui::GetCursorScreenPos();
                ImRect row(rowPos, ImVec2(rowPos.x + width, rowPos.y + rowH));
                ImRect iconRect(ImVec2(row.Min.x + Theme::g_padding + depth * indentW, row.Min.y),
                    ImVec2(row.Min.x + Theme::g_padding + depth * indentW + iconW, row.Max.y));

                const bool renaming = ui.renameIndex == idx;
                if (renaming)
                {
                    ImGui::SetNextItemAllowOverlap();
                }

                ImGui::InvisibleButton("##row", ImVec2(width, rowH));

                const bool mouseOverRow = ImGui::IsMouseHoveringRect(row.Min, row.Max);
                const bool dropOverRow = dropInside && row.Contains(dropMouse);
                const bool hovered = ui.dragActive ? mouseOverRow : ImGui::IsItemHovered();
                if (mouseOverRow)
                {
                    ui.hoveredRow = true;
                }
                const bool clickedLeft = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                const bool clickedRight = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
                const bool overIcon = hasChildren && ImGui::IsMouseHoveringRect(iconRect.Min, iconRect.Max);

                bool inBetweenZone = false;
                if (ui.dragActive && dropOverRow)
                {
                    inBetweenZone = ItemListDropMode(row, true) != SceneDropInto;
                }

                const bool highlight = ui.dragActive ? hovered && !inBetweenZone : ui.navigation.hover == rowIndex;
                DrawItemListRowBackground(dl, row, ui.visibleRow, it.selected, highlight);

                if (!renaming)
                {
                    if (clickedLeft && overIcon)
                    {
                        if (!filtering)
                        {
                            it.open = !it.open;
                        }
                    }
                    else if (clickedLeft)
                    {
                        const ImGuiIO& gio = ImGui::GetIO();
                        if (gio.KeyShift)
                        {
                            sc.ClearSelection();
                            if (g_scenePicker)
                            {
                                g_scenePicker->ClearSelection();
                            }
                            if (!g_outlinerRows.SelectRange(sc, ui.rangeAnchorId, it.id))
                            {
                                it.selected = true;
                                ui.rangeAnchorId = it.id;
                            }
                        }
                        else if (gio.KeyCtrl)
                        {
                            it.selected = !it.selected;
                            ui.rangeAnchorId = it.id;
                        }
                        else if (it.selected && sc.SelectionCount() > 1)
                        {
                            ui.pendingSingleId = it.id;
                        }
                        else
                        {
                            sc.ClearSelection();
                            if (g_scenePicker)
                            {
                                g_scenePicker->ClearSelection();
                            }
                            it.selected = true;
                            ui.rangeAnchorId = it.id;
                        }

                        ui.dragIndex = idx;
                        if (!gio.KeyShift && !gio.KeyCtrl && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            BeginOutlinerRename(idx);
                        }
                    }

                    if (clickedRight)
                    {
                        if (!it.selected)
                        {
                            sc.ClearSelection();
                            if (g_scenePicker)
                            {
                                g_scenePicker->ClearSelection();
                            }
                            it.selected = true;
                        }
                        ui.popupIndex = idx;
                        ui.openContextPopup = true;
                    }

                    if (((clickedLeft && !overIcon) || clickedRight) && it.selected && g_gui)
                    {
                        g_gui->RequestProperties();
                    }

                    if (ui.dragIndex == idx && ImGui::IsMouseDragging(ImGuiMouseButton_Left, Ui::Px(5.0f)))
                    {
                        if (!it.selected)
                        {
                            sc.ClearSelection();
                            if (g_scenePicker)
                            {
                                g_scenePicker->ClearSelection();
                            }
                            it.selected = true;
                        }
                        ui.dragActive = true;
                        ui.pendingSingleId = -1;
                    }
                }

                if (ui.dragActive)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Drag);
                }

                if (ui.dragActive && dropOverRow)
                {
                    SceneDropMode dropMode = ItemListDropMode(row, true);

                    if (dropMode == SceneDropInto && it.type == SceneItemRemoveBuilding)
                    {
                        dropMode = SceneDropNone;
                    }

                    ui.dropTarget = idx;
                    ui.dropMode = dropMode;
                    ui.dropBoundary = dropMode == SceneDropBefore ? rowIndex : dropMode == SceneDropAfter ? rowIndex + 1 : -1;
                }

                if (ui.dragActive && ui.dropTarget == idx && ui.dropMode == SceneDropInto
                    && sc.CanDropSelection(ui.dropTarget, ui.dropMode))
                {
                    DrawItemListDropPreview(ImGui::GetForegroundDrawList(), row, ui.dropMode, depth * indentW);
                }

                if (hasChildren)
                {
                    DrawRotatedArrow(dl,
                        ImVec2(iconRect.Min.x + Theme::g_smallIconSize * 0.5f, iconRect.Min.y + (iconRect.GetHeight() * 0.5f)),
                        it.open ? 1.0f : 0.0f, Theme::g_textDim);
                }

                const float labelX = iconRect.Max.x + Ui::Px(2.0f);
                if (renaming)
                {
                    ImGui::SetCursorScreenPos(ImVec2(labelX, row.Min.y + Ui::Hairline()));
                    ImGui::SetNextItemWidth(ImMax(Ui::Px(40.0f), row.Max.x - labelX - Theme::g_padding));
                    const bool askedFocus = ui.focusRename;
                    if (ui.focusRename)
                    {
                        ImGui::SetWindowFocus();
                        ImGui::SetKeyboardFocusHere();
                        ui.focusRename = false;
                    }
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding * 0.5f, Ui::Px(2.0f)));
                    const bool enter = ImGui::InputText("##rename", ui.renameBuf,
                        it.type == SceneItemRace ? RaceData::MaxName + 1 : sizeof(ui.renameBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue);
                    const bool active = ImGui::IsItemActive();
                    ImGui::PopStyleVar();

                    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !g_popupClosedOnEscape)
                    {
                        ui.renameIndex = -1;
                    }
                    else if (enter || (!active && !askedFocus))
                    {
                        if (ui.renameBuf[0] && strcmp(it.name, ui.renameBuf) != 0 && g_sceneActions)
                        {
                            g_sceneActions->Execute(std::make_unique<RenameItemAction>(it.id, it.name, ui.renameBuf));
                        }
                        ui.renameIndex = -1;
                    }
                }
                else
                {
                    const bool isRemove = it.type == SceneItemRemoveBuilding;
                    const bool isRace = it.type == SceneItemRace;
                    const bool isRaceCheckpoint = it.type == SceneItemRaceCheckpoint;
                    const ImU32 baseTextCol = isRemove ? Theme::g_textRemove
                        : isRace                       ? Theme::g_textRace
                        : isRaceCheckpoint             ? Theme::g_textRaceCheckpoint
                                                       : Theme::g_text;
                    const ImU32 hoverTextCol = isRemove ? Theme::g_dangerHover
                        : isRace                        ? Theme::g_textRaceHover
                        : isRaceCheckpoint              ? Theme::g_textRaceCheckpointHover
                                                        : Theme::g_textHover;
                    const ImU32 textCol = selfMatch ? (highlight ? hoverTextCol : baseTextCol) : Theme::g_textDim;
                    const float textY = row.Min.y + floorf((rowH - ImGui::GetFontSize()) * 0.5f);
                    dl->AddText(ImVec2(labelX, textY), textCol, it.name);
                }

                ImGui::PopID();
            });
    }

    static bool SearchInputWithMode(const char* id, const char* hint, char* buf, size_t bufSize, float w, ImTextureID leftIcon,
        SearchMode& mode, bool& outFocused, ImTextureID clearIcon)
    {
        static bool pendingClear = false;
        static bool previousFocused = false;

        ImFont* labelFont = g_gui->m_fontRegular;
        const float labelFontSz = ImGui::GetFontSize() * 0.9f;

        const bool hasText = buf[0] != '\0';
        bool clearClicked = false;

        if (!previousFocused && hasText)
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

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CallbackAlways;
        if (mode == SearchModeModel)
        {
            flags |= ImGuiInputTextFlags_CharsDecimal;
        }

        auto cb = [](ImGuiInputTextCallbackData* data) -> int
        {
            bool* p = (bool*)data->UserData;
            if (*p)
            {
                data->DeleteChars(0, data->BufTextLen);
                *p = false;
            }
            return 0;
        };

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_iconSize + Theme::g_padding * 2.0f, FrameTextPadY()));
        bool changed = ImGui::InputTextEx(id, hint, buf, (int)bufSize, ImVec2(w, Theme::g_frameHeight), flags, cb, &pendingClear);
        ImGui::PopStyleVar();

        if (clearClicked)
        {
            ImGui::ClearActiveID();
        }

        const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        const bool focused = ImGui::IsItemActive();
        outFocused = focused;

        if (focused && !previousFocused && buf[0] == '\0')
        {
            mode = SearchModeName;
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawIcon(dl, leftIcon, ImVec2(mn.x + Theme::g_padding, mn.y + (mx.y - mn.y - Theme::g_iconSize) * 0.5f),
            Theme::g_iconSize, Theme::g_textDim);

        const char* label = mode == SearchModeName ? "Name" : "Model ID";
        const ImVec2 ts = labelFont->CalcTextSizeA(labelFontSz, FLT_MAX, 0.0f, label);
        const float rbW = focused ? ImMax(Ui::Px(45.0f), ts.x + Ui::Px(8.0f)) : Ui::Px(20.0f);
        const float inset = Ui::Px(4.0f);
        const ImVec2 hitMin(mx.x - rbW - inset, mn.y);
        const ImVec2 hitMax(mx.x, mx.y);
        const ImVec2 rbMin(mx.x - rbW - inset, mn.y + inset);
        const ImVec2 rbMax(mx.x - inset, mx.y - inset);
        const bool hov = ImGui::IsMouseHoveringRect(hitMin, hitMax);

        if (hov)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
        }

        if (focused)
        {
            dl->AddRectFilled(rbMin, rbMax, hov ? Theme::g_alternatePanelBackground : Theme::g_panelBackground);
            dl->AddRect(rbMin, rbMax, Theme::g_border, 0.0f, 0, Ui::Hairline());
            dl->AddText(labelFont, labelFontSz,
                ImVec2(floorf(rbMin.x + (rbMax.x - rbMin.x - ts.x) * 0.5f), floorf(rbMin.y + (rbMax.y - rbMin.y - ts.y) * 0.5f)),
                hov ? Theme::g_textHover : Theme::g_textDim, label);

            if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && previousFocused)
            {
                mode = (mode == SearchModeName) ? SearchModeModel : SearchModeName;
                if (mode == SearchModeModel)
                {
                    for (const char* p = buf; *p; ++p)
                    {
                        if (*p < '0' || *p > '9')
                        {
                            pendingClear = true;
                            break;
                        }
                    }
                }
                changed = true;
            }
        }
        else if (buf[0] != '\0')
        {
            const ImVec2 center(floorf((rbMin.x + rbMax.x) * 0.5f), floorf((rbMin.y + rbMax.y) * 0.5f));
            DrawIconRotated45Centered(dl, clearIcon, center, Theme::g_iconSize, hov ? Theme::g_dangerHover : Theme::g_textRemove);
        }

        previousFocused = focused;
        return changed;
    }

    static void DrawOutlinerSearchStatus(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float fontSz, SearchMode mode, int foundItems)
    {
        ImFont* font = g_gui->m_fontRegular;
        const char* modeLabel = mode == SearchModeName ? "Name Search" : "Model Search";
        char status[64];
        snprintf(status, sizeof(status), "%s \xC2\xB7 %d found", modeLabel, foundItems);
        const ImVec2 statusSize = font->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, status);
        const float x = floorf((mn.x + mx.x - statusSize.x) * 0.5f);
        const float y = floorf((mn.y + mx.y - fontSz) * 0.5f);
        const ImU32 statusColor = foundItems > 0 ? Theme::g_axisY : Theme::g_textRemove;

        dl->AddText(font, fontSz, ImVec2(x, y), statusColor, status);
    }
}

void Gui::DrawOutliner()
{
    EditorScene& sc = *g_scene;
    OutlinerUiState& ui = g_outlinerUi;
    char* searchBuf = ui.searchBuf;
    SearchMode& searchMode = ui.searchMode;
    bool& searchFocused = ui.searchFocused;

    const float fullW = ImGui::GetContentRegionAvail().x;
    const float innerW = fullW - Theme::g_padding * 2.0f;
    const float btnW = Theme::g_frameHeight;
    const float inputW = innerW - btnW - Theme::g_padding;
    const ImVec2 size = ImVec2(fullW, ImGui::GetContentRegionAvail().y - Theme::g_padding);

    ImGui::BeginChild("##Outliner", size, false);

    ImGui::SetCursorPos(ImVec2(Theme::g_padding, ImGui::GetCursorPosY() + Theme::g_padding));
    if (SearchInputWithMode("##Search", "Search", searchBuf, sizeof(ui.searchBuf), inputW, g_icons->searchTexture, searchMode,
            searchFocused, g_icons->addTexture))
    {
        ui.navigation = {};
    }

    ImGui::SameLine(0, Theme::g_padding);
    if (IconSquareButton("##AddBtn", g_icons->addTexture, btnW))
    {
        ImGui::OpenPopup("outliner_add_popup");
    }

    if (ImCustom::BeginPopup("outliner_add_popup"))
    {
        AddPopupContext context;
        DrawAddMenus(FindSingleSelectedAddParent(), context);
        ImCustom::EndPopup();
    }

    const bool searching = searchBuf[0] != '\0';
    const float fontSz = ImGui::GetFontSize() * 0.8f;
    const float statusH = ImMax(Theme::g_padding, fontSz);
    ImGui::SetCursorPosX(Theme::g_padding);
    const float statusY = ImGui::GetCursorPosY();
    const ImVec2 statusMn = ImGui::GetCursorScreenPos();
    const int foundItems = BuildOutlinerRows(searchBuf, searchMode);

    if (searching)
    {
        DrawOutlinerSearchStatus(ImGui::GetWindowDrawList(), statusMn, ImVec2(statusMn.x + innerW, statusMn.y + statusH * 0.5f),
            fontSz, searchMode, foundItems);
        ImGui::SetCursorPos(ImVec2(Theme::g_padding, statusY + statusH));
    }

    const float rowH = Ui::Px(21.0f);
    ImVec2 scrollRange(FLT_MAX, -FLT_MAX);
    int focusRow = -1;
    int selectedRow = -1;
    for (int row = 0; row < static_cast<int>(g_outlinerRows.rows.size()); ++row)
    {
        const auto& item = sc.items[g_outlinerRows.rows[row].index];
        if (item.selected && (selectedRow < 0 || item.id == ui.rangeAnchorId))
        {
            selectedRow = row;
        }
        if (ui.newIdWatermark > 0 && item.id >= ui.newIdWatermark)
        {
            scrollRange.x = ImMin(scrollRange.x, row * rowH);
            scrollRange.y = ImMax(scrollRange.y, (row + 1) * rowH);
        }
        if (item.id == ui.scrollToId)
        {
            focusRow = row;
            ui.scrollToId = -1;
        }
    }
    const bool center = scrollRange.y <= scrollRange.x && focusRow >= 0;
    if (center)
    {
        scrollRange = ImVec2(focusRow * rowH, (focusRow + 1) * rowH);
    }
    ui.newIdWatermark = sc.nextId;
    const bool keyboard = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && !IsAnyModalOpen()
        && !ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && ui.renameIndex < 0
        && !ui.dragActive && (!ImGui::IsAnyItemActive() || searchFocused) && !SampTextInputActive();
    const int keyRow
        = NavigateItemList("##OutTree", ui.navigation, static_cast<int>(g_outlinerRows.rows.size()), selectedRow, keyboard);
    if (keyRow >= 0)
    {
        scrollRange = ImVec2(keyRow * rowH, (keyRow + 1) * rowH);
    }
    if (ui.navigation.keyboard && !keyboard)
    {
        ui.navigation.hover = -1;
    }
    if (keyboard && !g_popupClosedOnEscape
        && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
    {
        const int row = ui.navigation.hover >= 0 ? ui.navigation.hover : selectedRow;
        if (row >= 0)
        {
            if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift)
            {
                sc.ClearSelection();
                if (g_scenePicker)
                {
                    g_scenePicker->ClearSelection();
                }
            }
            auto& item = sc.items[g_outlinerRows.rows[row].index];
            item.selected = true;
            ui.rangeAnchorId = item.id;
            if (g_gui)
            {
                g_gui->RequestProperties();
            }
        }
    }
    BeginItemList("##OutTree", ImVec2(innerW, 0), g_outlinerRows.rows.size(), scrollRange, center && keyRow < 0);
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        const ImVec2 mn = ImGui::GetWindowPos();
        const ImVec2 mx = ImVec2(mn.x + ImGui::GetWindowSize().x, mn.y + ImGui::GetWindowSize().y);

        ui.visibleRow = 0;
        ui.hoveredRow = false;
        ui.dropTarget = -1;
        ui.dropBoundary = -1;
        ui.dropMode = SceneDropNone;
        ui.openContextPopup = false;

        const float rowWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 rowsPos = ImGui::GetCursorScreenPos();
        DrawOutlinerRows(searchBuf, searchMode, rowWidth);
        ImGui::PopStyleVar();

        const ImVec2 contentMx(mn.x + rowWidth, mx.y);
        const bool mouseInsideTree = ImGui::IsMouseHoveringRect(mn, contentMx);
        ImVec2 dropMouse;
        const bool dropInside = ItemListDropPosition(rowWidth, dropMouse);
        const bool anyPopupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
        const bool modalOpen = IsAnyModalOpen();

        if (!ui.dragActive && mouseInsideTree && !ui.hoveredRow && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyCtrl && !anyPopupOpen && !modalOpen)
        {
            Selection::ClearAll();
        }

        const float lastBottom = rowsPos.y + static_cast<float>(g_outlinerRows.rows.size()) * rowH;
        const int boundary
            = ImGui::GetIO().MousePos.y >= lastBottom ? static_cast<int>(g_outlinerRows.rows.size()) : ui.dropBoundary;
        if (ui.dragActive && dropInside && boundary >= 0)
        {
            const int depth = ItemListDropInEmptySpace(lastBottom)
                ? 0
                : ImMax(0, static_cast<int>(floorf((dropMouse.x - mn.x - Theme::g_padding) / Ui::Px(14.0f))));
            const auto drop = g_outlinerRows.DropAt(sc, boundary, depth);
            ui.dropTarget = drop.target;
            ui.dropMode = drop.mode;
            if (sc.CanDropSelection(ui.dropTarget, ui.dropMode))
            {
                const float lineY = rowsPos.y + boundary * rowH;
                DrawItemListDropPreview(ImGui::GetForegroundDrawList(),
                    ImRect(ImVec2(mn.x, lineY - rowH), ImVec2(contentMx.x, lineY)), SceneDropAfter, drop.depth * Ui::Px(14.0f));
            }
        }

        if (ui.openContextPopup && !modalOpen)
        {
            ImGui::OpenPopup("outliner_item_popup");
        }

        if (mouseInsideTree && !ui.hoveredRow && !ui.openContextPopup && !ui.dragActive && !anyPopupOpen && !modalOpen
            && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("outliner_bg_popup");
        }

        if (ImCustom::BeginPopup("outliner_item_popup"))
        {
            const bool validPopupItem = ui.popupIndex >= 0 && ui.popupIndex < sc.count;
            const int itemParent = !validPopupItem ? -1
                : IsValidAddParent(ui.popupIndex)  ? ui.popupIndex
                : (sc.items[ui.popupIndex].type == SceneItemRaceCheckpoint || sc.items[ui.popupIndex].type == SceneItemCurveNode)
                ? sc.items[ui.popupIndex].parent
                : -1;
            AddPopupContext context;
            DrawAddMenus(itemParent, context);
            DrawSelectionContextMenuItems(validPopupItem ? ui.popupIndex : -2, SceneTools::DuplicateScope::Selection);

            ImCustom::EndPopup();
        }

        if (ImCustom::BeginPopup("outliner_bg_popup"))
        {
            AddPopupContext context;
            DrawAddMenus(FindSingleSelectedAddParent(), context);
            DrawSelectionContextMenuItems(-1, SceneTools::DuplicateScope::Scene);

            ImCustom::EndPopup();
        }

        if (ui.dragActive && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (dropInside && sc.CanDropSelection(ui.dropTarget, ui.dropMode))
            {
                static std::vector<HierarchyEditAction::Entry> sHierCapture;
                CaptureHierarchyBefore(sHierCapture);
                sc.ApplyDrop(ui.dropTarget, ui.dropMode);
                CommitHierarchyDiff(sHierCapture);
            }
            ui.dragActive = false;
            ui.dragIndex = -1;
            ui.dropTarget = -1;
            ui.dropBoundary = -1;
            ui.dropMode = SceneDropNone;
            ui.renameIndex = -1;
            ui.focusRename = false;
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (ui.pendingSingleId >= 0)
            {
                const int pidx = sc.FindIndexById(ui.pendingSingleId);
                if (pidx >= 0)
                {
                    sc.ClearSelection();
                    if (g_scenePicker)
                    {
                        g_scenePicker->ClearSelection();
                    }
                    sc.items[pidx].selected = true;
                    ui.rangeAnchorId = ui.pendingSingleId;
                }
                ui.pendingSingleId = -1;
            }
            ui.dragIndex = -1;
        }
    }
    EndItemList();

    ImGui::EndChild();
}

void GuiDeleteSceneSelection()
{
    if (!g_scene)
    {
        return;
    }
    GuiDeleteSelectionNoUndo();
}

void GuiClearSceneSelection()
{
    if (!g_scene)
    {
        return;
    }
    g_scene->ClearSelection();
    g_outlinerUi.renameIndex = -1;
    g_outlinerUi.focusRename = false;
}

void GuiOnItemsAppended(int firstIdx, int endIdx)
{
    ResetItemWidgetsRange(firstIdx, endIdx);
    g_outlinerUi.showRequested = true;
}

void GuiRequestOutlinerScrollTo(int itemId)
{
    g_outlinerUi.scrollToId = itemId;
}

void GuiSelectAllOutliner()
{
    if (!g_scene)
    {
        return;
    }
    BuildOutlinerRows(g_outlinerUi.searchBuf, g_outlinerUi.searchMode);
    g_outlinerRows.SelectAll(*g_scene);
}

bool GuiCanSelectParentOutliner()
{
    if (g_scene)
    {
        for (int index = 0; index < g_scene->count; ++index)
        {
            if (g_scene->items[index].selected && g_scene->items[index].parent >= 0)
            {
                return true;
            }
        }
    }
    return false;
}

void GuiSelectParentOutliner()
{
    if (!GuiCanSelectParentOutliner() || !GuiFinishPropertyEdit())
    {
        return;
    }
    EditorScene& sc = *g_scene;
    std::array<bool, EditorScene::MaxItems> parents {};
    for (int index = 0; index < sc.count; ++index)
    {
        if (sc.items[index].selected && sc.items[index].parent >= 0)
        {
            parents[sc.items[index].parent] = true;
        }
    }
    Selection::ClearAll();
    int focusId = -1;
    for (int index : sc.DisplayOrder())
    {
        if (!parents[index])
        {
            continue;
        }
        sc.items[index].selected = true;
        if (focusId < 0)
        {
            focusId = sc.items[index].id;
        }
        if (!MatchesSearchMode(sc.items[index], g_outlinerUi.searchBuf, g_outlinerUi.searchMode))
        {
            g_outlinerUi.searchBuf[0] = '\0';
        }
        for (int parent = sc.items[index].parent; parent >= 0; parent = sc.items[parent].parent)
        {
            sc.items[parent].open = true;
        }
    }
    g_outlinerUi.pendingSingleId = -1;
    g_outlinerUi.rangeAnchorId = focusId;
    GuiRequestOutlinerScrollTo(focusId);
}

void GuiCloneUnifiedSelection()
{
    if (!g_scene)
    {
        return;
    }
    const size_t needed = SceneCloneItemCount() + (g_scenePicker ? g_scenePicker->GetSelection().size() : 0);
    if (!CheckCloneCapacity(needed))
    {
        return;
    }
    EditorScene& sc = *g_scene;
    const int before = sc.count;
    CloneState state;
    try
    {
        static int clones[EditorScene::MaxItems];
        int cloneCount = 0;
        if (sc.CanCloneSelection())
        {
            cloneCount = sc.CloneSelection(clones, state.preserveSelection);
        }

        bool createdFromNative = false;
        if (g_scenePicker && !g_scenePicker->GetSelection().empty())
        {
            if (cloneCount == 0)
            {
                sc.ClearSelection();
            }
            for (const auto& e : g_scenePicker->GetSelection())
            {
                char itemName[64];
                TextureBrowser::GetModelDisplay(e.nModelId, itemName, sizeof(itemName));

                const int idx = sc.AppendItem(-1, SceneItemNormal, itemName);
                if (idx < 0)
                {
                    g_toast.ShowError("Item limit reached.");
                    break;
                }
                sc.InitItemNormal(idx, true, e.nModelId);

                CVector pos = e.posPivot;
                float rot[3] = { 0.0f, 0.0f, 0.0f };
                Utils::BasisToEulerDegrees(e.rot, rot);
                sc.SetWorldTransform(idx, pos, rot);
                sc.items[idx].selected = true;
                createdFromNative = true;
            }
        }

        if (sc.count > before)
        {
            ResetItemWidgetsRange(before, sc.count);
        }
        if (cloneCount > 0 || createdFromNative)
        {
            state.Commit();
        }
        if (createdFromNative && !state.preserveSelection)
        {
            g_scenePicker->ClearSelection();
        }
    }
    catch (const std::bad_alloc&)
    {
        g_toast.ShowError("Not enough memory to clone the selection.");
    }
}

bool GuiCanCloneUnifiedSelection()
{
    if (!g_scene)
    {
        return false;
    }
    const size_t needed = SceneCloneItemCount() + (g_scenePicker ? g_scenePicker->GetSelection().size() : 0);
    return needed > 0 && needed <= static_cast<std::size_t>(EditorScene::MaxItems - g_scene->count);
}
