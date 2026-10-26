// Source Repository: https://github.com/waverso/waversomapstudio

class BookmarkAction : public IAction
{
    public:
    BookmarkAction(const ViewBookmark& bm, int index, bool add) : m_bookmark(bm), m_index(index), m_add(add)
    {
    }

    size_t MemoryUsage() const noexcept override
    {
        return sizeof(*this);
    }

    void Apply() override
    {
        if (m_add)
        {
            Insert();
        }
        else
        {
            Erase();
        }
    }
    void Undo() override
    {
        if (m_add)
        {
            Erase();
        }
        else
        {
            Insert();
        }
    }
    void Focus() override
    {
        GuiRequestBookmarkFocus(m_bookmark.id);
    }

    private:
    void Insert()
    {
        std::vector<ViewBookmark>& v = g_viewUi.bookmarks;
        int i = m_index < 0 ? 0 : (m_index > (int)v.size() ? (int)v.size() : m_index);
        v.insert(v.begin() + i, m_bookmark);
        ++UserData::g_bookmarksRevision;
    }
    void Erase()
    {
        std::vector<ViewBookmark>& v = g_viewUi.bookmarks;
        if (m_index >= 0 && m_index < (int)v.size())
        {
            v.erase(v.begin() + m_index);
            ++UserData::g_bookmarksRevision;
        }
    }

    ViewBookmark m_bookmark;
    int m_index;
    bool m_add;
};

class BookmarkRenameAction : public IAction
{
    public:
    BookmarkRenameAction(std::uint64_t id, const char* before, const char* after) : m_bookmarkId(id)
    {
        strncpy_s(m_before, sizeof(m_before), before, _TRUNCATE);
        strncpy_s(m_after, sizeof(m_after), after, _TRUNCATE);
    }

    size_t MemoryUsage() const noexcept override
    {
        return sizeof(*this);
    }

    void Apply() override
    {
        SetName(m_after);
    }

    void Undo() override
    {
        SetName(m_before);
    }

    void Focus() override
    {
        GuiRequestBookmarkFocus(m_bookmarkId);
    }

    private:
    int FindBookmark() const
    {
        std::vector<ViewBookmark>& bookmarks = g_viewUi.bookmarks;
        for (int i = 0; i < static_cast<int>(bookmarks.size()); ++i)
        {
            if (bookmarks[i].id == m_bookmarkId)
            {
                return i;
            }
        }
        return -1;
    }

    void SetName(const char* name)
    {
        const int index = FindBookmark();
        if (index >= 0)
        {
            strncpy_s(g_viewUi.bookmarks[index].name, sizeof(g_viewUi.bookmarks[index].name), name, _TRUNCATE);
            ++UserData::g_bookmarksRevision;
        }
    }

    std::uint64_t m_bookmarkId;
    char m_before[64] = {};
    char m_after[64] = {};
};

static float ViewBmListHeight()
{
    return Ui::Px(125.0f);
}

static void AddViewBookmark(ViewUiState& ui)
{
    if (ui.bookmarks.size() >= UserData::MaxBookmarks || g_nextBookmarkId == UINT64_MAX)
    {
        g_toast.ShowError("The bookmark limit has been reached.");
        return;
    }
    const CVector pos = g_freeCamera->m_position;
    float yaw, pitch;
    g_freeCamera->GetYawPitch(yaw, pitch);

    for (const ViewBookmark& b : ui.bookmarks)
    {
        if (fabsf(b.pos.x - pos.x) < 0.01f && fabsf(b.pos.y - pos.y) < 0.01f && fabsf(b.pos.z - pos.z) < 0.01f
            && fabsf(b.yaw - yaw) < 0.001f && fabsf(b.pitch - pitch) < 0.001f)
        {
            g_toast.ShowError("This viewpoint is already bookmarked.");
            return;
        }
    }

    ViewBookmark b {};
    b.id = g_nextBookmarkId++;
    b.zone = (pos.z < -242.990005f || pos.z >= 900.0f) ? Zones::InvalidZone : Zones::GetZoneAtPoint2D(pos.x, pos.y);
    strncpy_s(b.name, sizeof(b.name), Zones::GetZoneName(b.zone), _TRUNCATE);
    b.pos = pos;
    b.yaw = yaw;
    b.pitch = pitch;
    g_userActions->Execute(std::make_unique<BookmarkAction>(b, (int)ui.bookmarks.size(), true));
    ui.bmScrollPending = true;
}

static void DrawViewBookmarks(ViewUiState& ui, float innerW)
{
    const float btnW = Ui::Px(61.0f);
    const float inputW = innerW - btnW - Theme::g_padding;

    SearchInput("##bm_search", "Search", ui.bmSearch, sizeof(ui.bmSearch), inputW, ui.bmFocused);
    ImGui::SameLine(0, Theme::g_padding);
    if (IconLabelButton("##bm_add", "Add", g_icons->addTexture, btnW))
    {
        AddViewBookmark(ui);
    }

    const float rowH = Ui::Px(34.0f);
    const float fontSize = ImGui::GetFontSize();
    const float smallSz = fontSize * 0.8f;

    int nVisible = 0;
    for (const ViewBookmark& b : ui.bookmarks)
    {
        if (TextSearch::ContainsAsciiNoCase(b.name, ui.bmSearch))
        {
            nVisible++;
        }
    }

    const float listH = ViewBmListHeight();

    ImDrawList* pdl = ImGui::GetWindowDrawList();
    const ImVec2 clipMn = pdl->GetClipRectMin();
    const ImVec2 clipMx = pdl->GetClipRectMax();
    const float viewBottom = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y;
    float forwardWheel = 0.0f;
    float forwardDrag = 0.0f;
    const bool focusPendingThisFrame = ui.bookmarkFocusPending;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::g_alternatePanelBackground);
    ImVec2 listMn(0, 0), listMx(0, 0);
    ImGui::BeginChild("##bm_list", ImVec2(innerW, listH), false);
    {
        listMn = ImGui::GetWindowPos();
        listMx = ImVec2(listMn.x + ImGui::GetWindowSize().x, listMn.y + ImGui::GetWindowSize().y);
        int focusRow = -1;

        if (ui.bookmarks.empty())
        {
            DrawCenteredMessage("No bookmarks yet.");
        }
        else if (nVisible == 0)
        {
            DrawCenteredMessage("No bookmarks found.");
        }
        else
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImFont* font = g_gui->m_fontRegular;
            const float w = ImGui::GetContentRegionAvail().x;

            ImGui::SetCursorPos(ImVec2(0, 0));
            int row = 0;
            for (int i = 0; i < (int)ui.bookmarks.size(); i++)
            {
                ViewBookmark& b = ui.bookmarks[i];
                if (!TextSearch::ContainsAsciiNoCase(b.name, ui.bmSearch))
                {
                    continue;
                }

                ImGui::PushID(i);
                ImGui::SetCursorPos(ImVec2(0, row * rowH));
                const ImVec2 rmn = ImGui::GetCursorScreenPos();
                const ImVec2 rmx = ImVec2(rmn.x + w, rmn.y + rowH);
                const bool pressed = ImGui::InvisibleButton("##bm_row", ImVec2(w, rowH), ImGuiButtonFlags_NoNavFocus);
                const bool hovered = ImGui::IsItemHovered();
                const bool renaming = ui.renameIndex == i;
                const bool focused = focusPendingThisFrame && b.id == ui.bookmarkFocusId;

                if (focusPendingThisFrame && b.id == ui.bookmarkFocusId)
                {
                    focusRow = row;
                }

                if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                {
                    ui.popupIndex = i;
                    ui.openPopup = true;
                }

                if (focused)
                {
                    dl->AddRectFilled(rmn, rmx, hovered ? Theme::g_accentSoftHover : Theme::g_accentSoft);
                }
                else if (hovered)
                {
                    dl->AddRectFilled(rmn, rmx, Theme::g_hoveredControlBackground);
                }
                else if (row & 1)
                {
                    dl->AddRectFilled(rmn, rmx, Theme::g_alternateListRowBackground);
                }

                const float lineOffset = Ui::Hairline();
                const float line1Y = rmn.y + floorf((rowH * 0.55f - fontSize) * 0.5f) + lineOffset;
                const float line2Y = rmn.y + rowH * 0.55f + floorf((rowH * 0.45f - smallSz) * 0.5f) - lineOffset;

                if (renaming)
                {
                    ImGui::SetCursorScreenPos(ImVec2(rmn.x + Theme::g_padding * 0.5f, rmn.y + Ui::Hairline()));
                    ImGui::SetNextItemWidth(ImMax(Ui::Px(40.0f), w - Theme::g_padding));
                    const bool askedFocus = ui.focusRename;
                    if (ui.focusRename)
                    {
                        ImGui::SetKeyboardFocusHere();
                        ui.focusRename = false;
                    }
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding * 0.5f, Ui::Px(2.0f)));
                    const bool enter = ImGui::InputText(
                        "##bm_rename", ui.renameBuf, sizeof(ui.renameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
                    const bool active = ImGui::IsItemActive();
                    ImGui::PopStyleVar();

                    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !g_popupClosedOnEscape)
                    {
                        ui.renameIndex = -1;
                    }
                    else if (enter || (!active && !askedFocus))
                    {
                        if (ui.renameBuf[0] && strcmp(b.name, ui.renameBuf) != 0)
                        {
                            g_userActions->Execute(std::make_unique<BookmarkRenameAction>(b.id, b.name, ui.renameBuf));
                        }
                        ui.renameIndex = -1;
                    }
                }
                else
                {
                    const char* region = Zones::GetZoneRegionName(b.zone);
                    const ImVec2 rs = ImGui::CalcTextSize(region);
                    dl->AddText(ImVec2(rmx.x - Theme::g_padding - rs.x, line1Y), Theme::g_textDim, region);
                    dl->AddText(ImVec2(rmn.x + Theme::g_padding, line1Y),
                        focused ? Theme::g_text : (hovered ? Theme::g_textHover : Theme::g_text), b.name);
                }

                char coords[64];
                snprintf(coords, sizeof(coords), "%.2f, %.2f, %.2f", b.pos.x, b.pos.y, b.pos.z);
                const ImVec2 cs = font->CalcTextSizeA(smallSz, FLT_MAX, 0.0f, coords);
                dl->AddText(font, smallSz, ImVec2(rmn.x + (w - cs.x) * 0.5f, line2Y), Theme::g_textDim, coords);

                if (pressed && !renaming)
                {
                    g_freeCamera->StartFlyTo(b.pos, b.yaw, b.pitch);
                }

                ImGui::PopID();
                row++;
            }
            ImGui::PopStyleVar();
        }

        if (focusPendingThisFrame)
        {
            if (focusRow >= 0)
            {
                const float rowTop = focusRow * rowH;
                const float rowBottom = rowTop + rowH;
                const float scrollY = ImGui::GetScrollY();
                float target = scrollY;
                if (rowTop < scrollY)
                {
                    target = rowTop;
                }
                else if (rowBottom > scrollY + listH)
                {
                    target = rowBottom - listH;
                }
                ImGui::SetScrollY(ImMax(0.0f, target));
            }
            ui.bookmarkFocusPending = false;
            ui.bookmarkFocusId = 0;
            ui.bmScrollPending = false;
        }
        else if (ui.bmScrollPending)
        {
            ImGui::SetScrollY(ImMax(0.0f, nVisible * rowH + Ui::Px(2.0f) - listH));
            ui.bmScrollPending = false;
        }

        {
            const float wheel = ImGui::GetIO().MouseWheel;
            const float s = ImGui::GetScrollY();
            const float sMax = ImGui::GetScrollMaxY();
            const bool atBottom = s >= sMax - Ui::Hairline();

            if (wheel < 0.0f && sMax > 0.0f && ImGui::IsWindowHovered() && ui.bmWasAtBottom)
            {
                forwardWheel = wheel;
            }

            if (sMax > 0.0f && ImGui::GetActiveID() == ImGui::GetWindowScrollbarID(ImGui::GetCurrentWindow(), ImGuiAxis_Y))
            {
                if (ImGui::GetIO().MousePos.y > viewBottom)
                {
                    forwardDrag = ImGui::GetIO().DeltaTime * Ui::Px(500.0f);
                }
                else if (atBottom && ImGui::GetIO().MouseDelta.y > 0.0f)
                {
                    forwardDrag = ImGui::GetIO().MouseDelta.y;
                }
            }

            ui.bmWasAtBottom = atBottom;
        }

        if (ui.openPopup)
        {
            ImGui::OpenPopup("view_bm_popup");
            ui.openPopup = false;
        }

        if (ImCustom::BeginPopup("view_bm_popup"))
        {
            const bool valid = ui.popupIndex >= 0 && ui.popupIndex < (int)ui.bookmarks.size();
            if (valid && ImGui::MenuItem("Rename"))
            {
                ui.renameIndex = ui.popupIndex;
                ui.focusRename = true;
                strncpy_s(ui.renameBuf, sizeof(ui.renameBuf), ui.bookmarks[ui.popupIndex].name, _TRUNCATE);
            }
            if (valid && ImGui::MenuItem("Delete"))
            {
                g_userActions->Execute(std::make_unique<BookmarkAction>(ui.bookmarks[ui.popupIndex], ui.popupIndex, false));
                if (ui.renameIndex == ui.popupIndex)
                {
                    ui.renameIndex = -1;
                }
                else if (ui.renameIndex > ui.popupIndex)
                {
                    ui.renameIndex--;
                }
            }
            ImCustom::EndPopup();
        }
        DrawClippedListBorder(listMn, listMx, clipMn, clipMx);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (forwardWheel < 0.0f)
    {
        const float step = ImTrunc(5.0f * ImGui::GetFontSize());
        ImGui::SetScrollY(ImClamp(ImGui::GetScrollY() - forwardWheel * step, 0.0f, ImGui::GetScrollMaxY()));
    }

    if (forwardDrag > 0.0f)
    {
        ImGui::SetScrollY(ImClamp(ImGui::GetScrollY() + forwardDrag, 0.0f, ImGui::GetScrollMaxY()));
    }
}
