// Source Repository: https://github.com/waverso/waversomapstudio

struct ItemListNavigation
{
    int hover = -1;
    int frame = -2;
    int count = 0;
    bool inside = false;
    bool keyboard = false;
};

static void DrawRotatedArrow(ImDrawList* dl, ImVec2 center, float openFraction, ImU32 col)
{
    const float ang = openFraction * IM_PI * 0.5f;
    const float s = sinf(ang), c = cosf(ang);
    const float hs = Theme::g_smallIconSize * 0.5f;
    ImVec2 q[4] = { ImVec2(center.x + (-hs * c - -hs * s), center.y + (-hs * s + -hs * c)),
        ImVec2(center.x + (hs * c - -hs * s), center.y + (hs * s + -hs * c)),
        ImVec2(center.x + (hs * c - hs * s), center.y + (hs * s + hs * c)),
        ImVec2(center.x + (-hs * c - hs * s), center.y + (-hs * s + hs * c)) };
    dl->AddImageQuad(g_icons->arrowTexture, q[0], q[1], q[2], q[3], ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1), col);
}

static ImGuiWindow* FindItemListWindow(const char* id)
{
    ImGuiWindow* parent = ImGui::GetCurrentWindow();
    const char* name;
    ImFormatStringToTempBuffer(&name, nullptr, "%s/%s_%08X", parent->Name, id, parent->GetID(id));
    return ImGui::FindWindowByName(name);
}

static int NavigateItemList(
    const char* id, ItemListNavigation& navigation, int count, int selected, bool keys, bool selectionOnly = false)
{
    const ImGuiIO& io = ImGui::GetIO();
    const int frame = ImGui::GetFrameCount();
    if (navigation.frame != frame - 1 || navigation.count != count)
    {
        navigation = {};
    }
    navigation.frame = frame;
    navigation.count = count;
    ImGuiWindow* window = FindItemListWindow(id);
    const bool inside = window && GImGui->HoveredWindow == window
        && ImGui::IsWindowContentHoverable(window, ImGuiHoveredFlags_None)
        && ImRect(window->Pos, ImVec2(window->InnerRect.Max.x, window->Pos.y + window->Size.y)).Contains(io.MousePos);
    if (inside
        && (!navigation.inside || io.MouseDelta.x != 0 || io.MouseDelta.y != 0 || ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
    {
        const int row = static_cast<int>(floorf((io.MousePos.y - window->Pos.y + window->Scroll.y) / Ui::Px(21)));
        navigation.hover = row >= 0 && row < count ? row : -1;
        navigation.keyboard = false;
    }
    else if (!inside && navigation.inside)
    {
        navigation.hover = -1;
        navigation.keyboard = false;
    }
    navigation.inside = inside;
    if (!keys || SampTextInputActive())
    {
        return -1;
    }
    const int page = int(ImGui::IsKeyPressed(ImGuiKey_PageDown)) - int(ImGui::IsKeyPressed(ImGuiKey_PageUp));
    if (page && window)
    {
        const float distance = ImMax(Ui::Px(21), floorf(window->Size.y / Ui::Px(21)) * Ui::Px(21));
        ImGui::SetNextWindowScroll(ImVec2(-1, ImClamp(window->Scroll.y + page * distance, 0.0f, window->ScrollMax.y)));
    }
    const int direction = int(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) - int(ImGui::IsKeyPressed(ImGuiKey_UpArrow));
    if (!direction || !count)
    {
        return -1;
    }
    const int origin = selectionOnly || navigation.hover < 0 ? selected : navigation.hover;
    const int row = origin < 0 ? 0 : ImClamp(origin + direction, 0, count - 1);
    if (!selectionOnly)
    {
        navigation.hover = row;
        navigation.keyboard = true;
    }
    return row;
}

static void BeginItemList(const char* id, ImVec2 size, std::size_t count, ImVec2 scrollRange = ImVec2(0, 0), bool center = false)
{
    ImGui::SetNextWindowContentSize(ImVec2(0, ImMax(1.0f, static_cast<float>(count) * Ui::Px(21.0f))));
    if (scrollRange.y > scrollRange.x)
    {
        const float height = floorf(ImMax(4.0f, ImGui::CalcItemSize(size, 0, ImGui::GetContentRegionAvail().y).y));
        if (center)
        {
            ImGui::SetNextWindowScroll(ImVec2(-1, ImMax(0.0f, (scrollRange.x + scrollRange.y - height) * 0.5f)));
        }
        else
        {
            const ImGuiWindow* child = FindItemListWindow(id);
            const float scroll = child ? child->Scroll.y : 0.0f;
            if (scrollRange.x < scroll || scrollRange.y > scroll + height)
            {
                ImGui::SetNextWindowScroll(
                    ImVec2(-1, ImMax(0.0f, scrollRange.x < scroll ? scrollRange.x : scrollRange.y - height)));
            }
        }
    }
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::g_alternatePanelBackground);
    ImGui::BeginChild(id, size, false);
    ImGui::SetCursorPos(ImVec2(0, 0));
}

static void EndItemList()
{
    const ImVec2 mn = ImGui::GetWindowPos(), mx = mn + ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(mn, mx, false);
    dl->AddRect(mn, mx, Theme::g_border, 0, 0, Ui::Hairline());
    dl->PopClipRect();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void DrawItemListRowBackground(ImDrawList* dl, const ImRect& row, int index, bool selected, bool hovered)
{
    const ImU32 bg = selected ? (hovered ? Theme::g_accentSoftHover : Theme::g_accentSoft)
        : hovered             ? Theme::g_hoveredControlBackground
        : (index & 1)         ? Theme::g_alternateListRowBackground
                              : 0;
    if (bg)
    {
        const float inset = Ui::Hairline();
        dl->AddRectFilled(ImVec2(row.Min.x + inset, row.Min.y), ImVec2(row.Max.x - inset, row.Max.y), bg);
    }
}

static bool ItemListDropPosition(float width, ImVec2& mouse)
{
    const ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImRect bounds(window->Pos, window->Pos + ImVec2(width, window->Size.y));
    mouse = ImGui::GetIO().MousePos;
    if (mouse.x < bounds.Min.x || mouse.x >= bounds.Max.x || mouse.y < bounds.Min.y - Theme::g_padding
        || mouse.y > bounds.Max.y + Theme::g_padding)
    {
        return false;
    }
    mouse.y = ImClamp(mouse.y, bounds.Min.y, bounds.Max.y - Ui::Hairline());
    return true;
}

static bool ItemListDropInEmptySpace(float contentBottom)
{
    const ImGuiWindow* window = ImGui::GetCurrentWindow();
    return window->Pos.y + window->Size.y - contentBottom > Theme::g_padding && ImGui::GetIO().MousePos.y >= contentBottom;
}

static SceneDropMode ItemListDropMode(const ImRect& row, bool allowInto)
{
    const ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (ImGui::GetIO().MousePos.y < window->Pos.y)
    {
        return SceneDropBefore;
    }
    if (ImGui::GetIO().MousePos.y >= window->Pos.y + window->Size.y)
    {
        return SceneDropAfter;
    }
    const float t = (ImGui::GetIO().MousePos.y - row.Min.y) / row.GetHeight();
    if (!allowInto)
    {
        return t < 0.50f ? SceneDropBefore : SceneDropAfter;
    }
    return t < 0.30f ? SceneDropBefore : t > 0.70f ? SceneDropAfter : SceneDropInto;
}

static void DrawItemListDropPreview(ImDrawList* dl, const ImRect& row, SceneDropMode mode, float indent = 0)
{
    const float inset = Ui::Hairline();
    const float stroke = Ui::Hairline(2.0f);
    const ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float top = window->Pos.y + stroke * 0.5f;
    const float bottom = window->Pos.y + window->Size.y - stroke * 0.5f;
    const ImVec2 lineMin(ImMin(row.Min.x + inset + indent, row.Max.x - inset), ImClamp(row.Min.y, top, bottom));
    const ImVec2 lineMax(row.Max.x - inset, ImClamp(row.Max.y, top, bottom));
    if (mode == SceneDropBefore)
    {
        dl->AddLine(lineMin, ImVec2(lineMax.x, lineMin.y), Theme::g_accentDrag, stroke);
    }
    else if (mode == SceneDropAfter || mode == SceneDropRoot)
    {
        dl->AddLine(ImVec2(lineMin.x, lineMax.y), ImVec2(lineMax.x, lineMax.y), Theme::g_accentDrag, stroke);
    }
    else if (mode == SceneDropInto)
    {
        dl->AddRect(lineMin, lineMax, Theme::g_accentDrag, 0, 0, stroke);
    }
}

static void DrawCenteredMessage(const char* msg)
{
    const ImVec2 textSz = ImGui::CalcTextSize(msg);
    const ImVec2 pos = ImGui::GetCurrentWindow()->InnerRect.GetCenter() - textSz * 0.5f;
    ImGui::GetWindowDrawList()->AddText(pos, Theme::g_textDim, msg);
}

static void DrawClippedListBorder(ImVec2 listMn, ImVec2 listMx, ImVec2 clipMn, ImVec2 clipMx)
{
    const ImVec2 cMn(ImMax(listMn.x, clipMn.x), ImMax(listMn.y, clipMn.y));
    const ImVec2 cMx(ImMin(listMx.x, clipMx.x), ImMin(listMx.y, clipMx.y));
    if (cMx.x <= cMn.x || cMx.y <= cMn.y)
    {
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(cMn, cMx, false);
    dl->AddRect(listMn, listMx, Theme::g_border, 0.0f, 0, Ui::Hairline());
    dl->PopClipRect();
}
