// Source Repository: https://github.com/waverso/waversomapstudio

static void EllipsizeText(char* out, size_t outSz, const char* text, float maxW, float fontSize)
{
    if (!outSz)
    {
        return;
    }
    text = text ? text : "";
    ImFont* font = ImGui::GetFont();
    snprintf(out, outSz, "%s", text);
    size_t copied = strlen(out);
    while (copied > 0 && (static_cast<unsigned char>(text[copied]) & 0xc0) == 0x80)
    {
        out[--copied] = '\0';
    }
    if (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, out).x <= maxW)
    {
        return;
    }

    if (outSz < 4)
    {
        snprintf(out, outSz, "...");
        return;
    }
    int first = 0;
    int last = static_cast<int>(ImMin(strlen(out), outSz - 4));
    int keep = 0;
    const bool multiline = strchr(out, '\n') != nullptr;
    while (first <= last)
    {
        const int middle = multiline ? last : first + (last - first) / 2;
        int end = middle;
        while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
        {
            --end;
        }
        snprintf(out, outSz, "%.*s...", end, text);
        if (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, out).x <= maxW)
        {
            keep = end;
            if (multiline)
            {
                break;
            }
            first = middle + 1;
        }
        else
        {
            last = end - 1;
        }
    }
    snprintf(out, outSz, "%.*s...", keep, text);
}

static bool PageNavigationButton(const char* id, const char* label, float width, bool previous)
{
    const bool clicked = ImGui::Button(id, ImVec2(width, Theme::g_frameHeight));
    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const float iconSize = Theme::g_smallIconSize;
    const float gap = Theme::g_padding * 0.5f;
    const float x = mn.x + (mx.x - mn.x - textSize.x - gap - iconSize) * 0.5f;
    const float iconX = previous ? x : x + textSize.x + gap;
    const float textX = previous ? x + iconSize + gap : x;
    const ImU32 color = ImGui::GetColorU32(ImGui::IsItemActive() || ImGui::IsItemHovered() ? Theme::g_textHover : Theme::g_text);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawRotatedArrow(dl, ImVec2(iconX + iconSize * 0.5f, (mn.y + mx.y) * 0.5f), previous ? 2.0f : 0.0f, color);
    dl->AddText(ImVec2(textX, mn.y + (mx.y - mn.y - textSize.y) * 0.5f), color, label);
    return clicked;
}

struct PageJumpState
{
    char buf[16] = {};
    int pageCount = 0;
    int* page = nullptr;
    ImVec2 pagePos = {};
    ImVec2 pageSize = {};
    bool shouldOpen = false;
};
static PageJumpState g_pageJump;

static int FilterPageInput(ImGuiInputTextCallbackData* data)
{
    return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
}

static float PageLinkWidth()
{
    char wide[32];
    snprintf(wide, sizeof(wide), "Page 9999/9999");
    return ImGui::CalcTextSize(wide).x + Ui::Px(8.0f);
}

static void DrawPageLink(const char* linkId, const char* popupId, int currentPage, int pageCount, int* page)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    char pageBuf[32];
    snprintf(pageBuf, sizeof(pageBuf), "Page %d/%d", currentPage + 1, pageCount);
    const float textW = ImGui::CalcTextSize(pageBuf).x;
    const float linkW = PageLinkWidth();
    const bool hovered = ImGui::InvisibleButton(linkId, ImVec2(linkW, Theme::g_frameHeight));
    const ImU32 col = ImGui::IsItemHovered() ? Theme::g_textHover : Theme::g_accent;
    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    const ImVec2 textPos(mn.x + (linkW - textW) * 0.5f, mn.y + (mx.y - mn.y - ImGui::GetFontSize()) * 0.5f);
    dl->AddText(textPos, col, pageBuf);
    if (hovered)
    {
        g_pageJump.pageCount = pageCount;
        g_pageJump.page = page;
        g_pageJump.pagePos = mn;
        g_pageJump.pageSize = ImVec2(mx.x - mn.x, mx.y - mn.y);
        snprintf(g_pageJump.buf, sizeof(g_pageJump.buf), "%d", currentPage + 1);
        g_pageJump.shouldOpen = true;
    }
}

static void DrawPageJumpPopup(const char* popupId)
{
    if (g_pageJump.shouldOpen)
    {
        ImGui::OpenPopup(popupId);
        g_pageJump.shouldOpen = false;
    }
    if (!ImGui::IsPopupOpen(popupId))
    {
        return;
    }
    const auto& style = ImCustom::GetStyle();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 text = ImGui::CalcTextSize("Page:");
    const ImVec2 size(ceilf(ImMax(text.x, Ui::Px(80.0f)) + style.popupPad * 4.0f),
        ceilf(text.y + style.popupSpacing + Theme::g_frameHeight + style.popupPad * 4.0f));
    const float gap = Ui::Px(2.0f);
    float x = g_pageJump.pagePos.x + (g_pageJump.pageSize.x - size.x) * 0.5f;
    float y = g_pageJump.pagePos.y + g_pageJump.pageSize.y + gap;
    if (y + size.y > display.y - Theme::g_outerPadding)
    {
        y = g_pageJump.pagePos.y - size.y - gap;
    }
    x = ImClamp(x, Theme::g_outerPadding, ImMax(Theme::g_outerPadding, display.x - Theme::g_outerPadding - size.x));
    y = ImClamp(y, Theme::g_outerPadding, ImMax(Theme::g_outerPadding, display.y - Theme::g_outerPadding - size.y));
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
    if (!ImCustom::BeginPopup(popupId))
    {
        ImGui::PopStyleVar();
        return;
    }
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImGui::TextUnformatted("Page:");
    ImGui::SetNextItemWidth(Ui::Px(80.0f));
    if (ImGui::IsWindowAppearing())
    {
        ImGui::SetKeyboardFocusHere();
    }
    const bool enter = ImGui::InputText("##page_jump_input", g_pageJump.buf, sizeof(g_pageJump.buf),
        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue
            | ImGuiInputTextFlags_CallbackCharFilter,
        FilterPageInput);
    if (g_pageJump.page && ImGui::IsItemActive() && g_pageJump.buf[0])
    {
        unsigned long long typed = 0;
        for (const char* q = g_pageJump.buf; *q; ++q)
        {
            typed = ImMin(typed * 10 + static_cast<unsigned>(*q - '0'), static_cast<unsigned long long>(g_pageJump.pageCount));
        }
        *g_pageJump.page = ImMax(static_cast<int>(typed) - 1, 0);
    }
    if (enter)
    {
        ImGui::CloseCurrentPopup();
    }
    ImCustom::EndPopup();
    ImGui::PopStyleVar();
}
