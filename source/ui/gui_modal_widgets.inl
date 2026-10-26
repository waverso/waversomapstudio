// Source Repository: https://github.com/waverso/waversomapstudio

static float ModalFooterHeight()
{
    return Ui::Hairline() + Theme::g_itemSpacing + Theme::g_frameHeight + Theme::g_outerPadding;
}

static float ModalHeight(float contentHeight)
{
    return ceilf(Theme::g_tabHeight + Theme::g_padding + contentHeight + Theme::g_itemSpacing + ModalFooterHeight());
}

static void PushModalStyle()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::g_panelBackground);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::g_border);
}

static void PopModalStyle(int extraStyleVars = 0)
{
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2 + extraStyleVars);
}

static float DrawModalFooter()
{
    const ImVec2 pos = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();
    const float buttonY = pos.y + ImGui::GetWindowHeight() - Theme::g_outerPadding - Theme::g_frameHeight;
    const float separatorY = buttonY - Theme::g_itemSpacing - Ui::Hairline();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x + Theme::g_outerPadding, separatorY),
        ImVec2(pos.x + width - Theme::g_outerPadding, separatorY), Theme::g_border, Ui::Hairline());
    ImGui::SetCursorScreenPos(ImVec2(pos.x + Theme::g_outerPadding, buttonY));
    return buttonY;
}

static bool ClosePopupOnEscape()
{
    if (!ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        return false;
    }
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (!context || context->OpenPopupStack.Size <= 0)
    {
        return false;
    }
    int level = 0;
    for (const auto& popup : context->OpenPopupStack)
    {
        if (popup.PopupId == g_actorAnimationPopupId)
        {
            level = context->OpenPopupStack.Size - 1;
            break;
        }
    }
    ImGui::ClosePopupToLevel(level, true);
    return true;
}

static void DrawModalBackdrop(const char* id)
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin(id, nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
                | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        if (!ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        }
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize, IM_COL32(0, 0, 0, 140));
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

static void DrawModalTitleBar(ImDrawList* dl, const ImVec2& winPos, float winW, const char* title)
{
    dl->PushClipRect(winPos, winPos + ImVec2(winW, Theme::g_tabHeight), false);
    const ImVec2 tMin = winPos;
    const ImVec2 tMax = ImVec2(winPos.x + winW, winPos.y + Theme::g_tabHeight);
    dl->AddRectFilled(tMin, tMax, Theme::g_panelBackground);
    dl->AddRectFilled(tMin, ImVec2(tMax.x, tMin.y + Theme::g_accentSize), Theme::g_accent);
    const ImVec2 ts = ImGui::CalcTextSize(title);
    const ImVec2 tp(
        floorf(tMin.x + (winW - ts.x) * 0.5f), floorf(tMin.y + (Theme::g_tabHeight + Theme::g_padding - ts.y) * 0.5f));
    dl->AddText(ImVec2(tp.x, tp.y + Ui::Hairline()), Theme::g_shadow, title);
    dl->AddText(g_gui->m_fontBold, ImGui::GetFontSize(), tp, Theme::g_text, title);
    dl->PopClipRect();
}

struct ToastState
{
    char message[256] = {};
    double endTime = -1.0;
    bool isError = true;

    void ShowError(const char* msg)
    {
        strncpy_s(message, sizeof(message), msg, _TRUNCATE);
        endTime = ImGui::GetTime() + 3.0;
        isError = true;
    }
    void ShowInfo(const char* msg)
    {
        strncpy_s(message, sizeof(message), msg, _TRUNCATE);
        endTime = ImGui::GetTime() + 3.0;
        isError = false;
    }
    bool IsVisible() const
    {
        return endTime > 0.0 && ImGui::GetTime() < endTime;
    }
    float Alpha() const
    {
        if (!IsVisible())
        {
            return 0.0f;
        }
        double rem = endTime - ImGui::GetTime();
        return rem < 0.5 ? (float)(rem / 0.5) : 1.0f;
    }
};
static ToastState g_toast;

static void DrawToast()
{
    const float alpha = g_toast.Alpha();
    if (alpha <= 0.0f)
    {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const float padX = Ui::Px(16.0f), padY = Ui::Px(9.0f);
    const ImVec2 textSz = ImGui::CalcTextSize(g_toast.message);
    const float toastW = textSz.x + padX * 2.0f;
    const float toastH = textSz.y + padY * 2.0f;
    const ImVec2 pos(floorf((io.DisplaySize.x - toastW) * 0.5f), io.DisplaySize.y - toastH - Ui::Px(24.0f));
    const ImVec2 posMax(pos.x + toastW, pos.y + toastH);

    const auto A = [&](ImU32 base, float mul = 1.0f) -> ImU32
    {
        return (base & 0x00FFFFFF) | ((ImU32)(((base >> 24) & 0xFF) * alpha * mul) << 24);
    };

    const ImU32 colBg = g_toast.isError ? Theme::g_errorBackground : Theme::g_toastBackground;
    const ImU32 colBorder = g_toast.isError ? Theme::g_danger : Theme::g_accentMuted;
    const ImU32 colText = g_toast.isError ? Theme::g_errorText : Theme::g_toastText;

    ImDrawList* fg = ImGui::GetForegroundDrawList();
    fg->AddRectFilled(pos, posMax, A(colBg));
    fg->AddRect(pos, posMax, A(colBorder), 0.0f, 0, Ui::Hairline(1.5f));
    fg->AddText(ImVec2(pos.x + padX, pos.y + padY), A(colText), g_toast.message);
}

static bool ContextMenuItem(const char* label, const char* shortcut, bool enabled)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
    {
        return false;
    }

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::MenuItem(label, nullptr, false, enabled);
    if (ImGui::IsItemVisible())
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float right = window->WorkRect.Max.x;
        const ImVec2 textSize = ImGui::CalcTextSize(shortcut);
        const ImVec4 clip(pos.x + ImGui::CalcTextSize(label).x + style.ItemSpacing.x, pos.y, right, pos.y + textSize.y);
        window->DrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(right - textSize.x, pos.y),
            ImGui::GetColorU32(ImGuiCol_TextDisabled, enabled ? 1.0f : style.DisabledAlpha), shortcut, nullptr, 0.0f, &clip);
    }
    return pressed;
}
