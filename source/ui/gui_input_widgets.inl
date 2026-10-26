// Source Repository: https://github.com/waverso/waversomapstudio

static void SetStyledTooltip(const char* text)
{
    if (g_dragCursorLockActive)
    {
        return;
    }
    const ImGuiContext& context = *ImGui::GetCurrentContext();
    const float alpha
        = (context.CurrentItemFlags & ImGuiItemFlags_Disabled) ? context.DisabledAlphaBackup : ImGui::GetStyle().Alpha;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_padding, Theme::g_popupPadding));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
    ImGui::SetTooltip("%s", text);
    ImGui::PopStyleVar(3);
}

static void SetBrowserTooltip(const char* text)
{
    if (g_dragCursorLockActive)
    {
        return;
    }
    const ImGuiContext& context = *ImGui::GetCurrentContext();
    const float alpha
        = (context.CurrentItemFlags & ImGuiItemFlags_Disabled) ? context.DisabledAlphaBackup : ImGui::GetStyle().Alpha;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_padding, Theme::g_popupPadding));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::g_popupPadding, Theme::g_popupPadding));
    if (ImGui::BeginTooltip())
    {
        const char* separator = strchr(text, '\n');
        ImGui::PushFont(g_gui->m_fontBold);
        ImGui::TextUnformatted(text, separator);
        ImGui::PopFont();
        if (separator)
        {
            ImGui::TextUnformatted(separator + 1);
        }
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar(3);
}

inline void DrawIcon(ImDrawList* dl, ImTextureID tex, ImVec2 pos, float size, ImU32 col = IM_COL32_WHITE)
{
    const bool accent = g_icons && g_icons->UsesAccent(static_cast<IDirect3DTexture9*>(tex));
    if (accent)
    {
        dl->AddCallback(g_icons->BeginAccent, g_icons);
    }
    dl->AddImage(tex, pos, ImVec2(pos.x + size, pos.y + size), ImVec2(0, 0), ImVec2(1, 1), col);
    if (accent)
    {
        dl->AddCallback(g_icons->EndAccent, g_icons);
    }
}

inline void DrawIconCentered(
    ImDrawList* dl, ImTextureID tex, const ImVec2& mn, const ImVec2& mx, float size, ImU32 col = IM_COL32_WHITE)
{
    const ImVec2 c((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f);
    DrawIcon(dl, tex, ImVec2(c.x - size * 0.5f, c.y - size * 0.5f), size, col);
}

static void DrawIconRotated45Centered(ImDrawList* dl, ImTextureID tex, ImVec2 center, float size, ImU32 col)
{
    const bool accent = g_icons && g_icons->UsesAccent(static_cast<IDirect3DTexture9*>(tex));
    if (accent)
    {
        dl->AddCallback(g_icons->BeginAccent, g_icons);
    }
    const float hs = size * 0.5f;
    dl->AddImageQuad(tex, { center.x, center.y - hs }, { center.x + hs, center.y }, { center.x, center.y + hs },
        { center.x - hs, center.y }, { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, col);
    if (accent)
    {
        dl->AddCallback(g_icons->EndAccent, g_icons);
    }
}

static void SmartFormat(char* buf, size_t sz, float v)
{
    if (sz == 0)
    {
        return;
    }
    if (std::fabs(v) < 0.0005f)
    {
        v = 0.0f;
    }
    snprintf(buf, sz, "%.3f", v);
    char* dot = strchr(buf, '.');
    if (dot)
    {
        char* end = buf + strlen(buf) - 1;
        while (end > dot + 1 && *end == '0')
        {
            *end-- = '\0';
        }
        if (end == dot)
        {
            *end = '\0';
        }
    }
}

static void RebuildVec3Buf(char* buf, size_t sz, const CVector& v)
{
    if (sz < 32)
    {
        return;
    }
    char sx[32], sy[32], sz2[32];
    SmartFormat(sx, sizeof(sx), v.x);
    SmartFormat(sy, sizeof(sy), v.y);
    SmartFormat(sz2, sizeof(sz2), v.z);
    snprintf(buf, sz, "%s, %s, %s", sx, sy, sz2);
}

struct Vec3State
{
    float prev[3] = { 0.0f, 0.0f, 0.0f };
    char buf[128] = {};
    char axisBuf[3][32] = {};
    bool hasFocus = false;
    bool interacting = false;
    int interactionFrame = -1;
    bool dirty = true;
    bool axisDirty[3] = { true, true, true };
    bool textSizeDirty = true;
    ImVec2 textSize = ImVec2(0, 0);
    unsigned int textSizeGeneration = 0;

    void OnValuesChanged(const float* src)
    {
        for (int i = 0; i < 3; i++)
        {
            if (src[i] != prev[i])
            {
                prev[i] = src[i];
                dirty = true;
                axisDirty[i] = true;
            }
        }
    }

    void RebuildIfNeeded(const float* src)
    {
        OnValuesChanged(src);
        if (dirty && !hasFocus)
        {
            RebuildVec3Buf(buf, sizeof(buf), *reinterpret_cast<const CVector*>(src));
            dirty = false;
            textSizeDirty = true;
        }
    }

    const ImVec2& GetTextSize()
    {
        if (textSizeDirty || textSizeGeneration != Ui::Generation())
        {
            textSize = ImGui::CalcTextSize(buf);
            textSizeDirty = false;
            textSizeGeneration = Ui::Generation();
        }
        return textSize;
    }

    const char* GetAxisText(int i, float value)
    {
        if (axisDirty[i])
        {
            SmartFormat(axisBuf[i], sizeof(axisBuf[i]), value);
            axisDirty[i] = false;
        }
        return axisBuf[i];
    }

    void MarkDirty()
    {
        dirty = true;
        textSizeDirty = true;
    }
};

static bool CenteredDisplayInput(const char* id, char* buf, size_t bufSize, float w, const char* displayStr, bool& hasFocus,
    ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* userData = nullptr)
{
    if (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled)
    {
        std::string displayBuffer(displayStr);
        const float textWidth = ImGui::CalcTextSize(displayStr).x;
        const float paddingX = ImMax(0.0f, (w - textWidth) * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(paddingX, FrameTextPadY()));
        ImGui::SetNextItemWidth(w);
        ImGui::InputTextEx(id, nullptr, displayBuffer.data(), (int)displayBuffer.size() + 1, ImVec2(w, Theme::g_frameHeight),
            ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar();
        hasFocus = false;
        return false;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 textSz = ImGui::CalcTextSize(displayStr);
    const float centerPadX = ImMax(0.0f, (w - textSz.x) * 0.5f);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(hasFocus ? Theme::g_padding : centerPadX, FrameTextPadY()));
    ImGui::SetNextItemWidth(w);
    const bool changed
        = ImGui::InputTextEx(id, nullptr, buf, (int)bufSize, ImVec2(w, Theme::g_frameHeight), flags, callback, userData);
    hasFocus = ImGui::IsItemActive();

    if (!hasFocus)
    {
        const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        const float inset = Ui::Hairline(2.0f);
        dl->AddRectFilled(
            ImVec2(mn.x + inset, mn.y + inset), ImVec2(mx.x - inset, mx.y - inset), ImGui::GetColorU32(ImGuiCol_FrameBg));
        const ImVec4 clip(mn.x, mn.y, mx.x, mx.y);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(mn.x + centerPadX, mn.y + (Theme::g_frameHeight - textSz.y) * 0.5f), ImGui::GetColorU32(Theme::g_text),
            displayStr, nullptr, 0.0f, &clip);
    }
    ImGui::PopStyleVar();
    return changed;
}

static bool IconButton(const char* id, ImTextureID icon, float size, ImU32 iconColor = Theme::g_text)
{
    const bool clicked = ImGui::Button(id, ImVec2(size, size));

    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    const ImU32 hoverColor = Theme::g_isLight && iconColor == IM_COL32_WHITE ? IM_COL32_WHITE : Theme::g_textHover;
    const ImU32 col = ImGui::IsItemActive() || ImGui::IsItemHovered() ? hoverColor : iconColor;
    DrawIconCentered(ImGui::GetWindowDrawList(), icon, mn, mx, Theme::g_iconSize, ImGui::GetColorU32(col));
    return clicked;
}

static bool IconSquareButton(const char* id, ImTextureID icon, float size, ImU32 iconColor = Theme::g_text)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    const bool clicked = IconButton(id, icon, size, iconColor);
    ImGui::PopStyleVar();
    return clicked;
}

static bool ParseModelIdInput(const char* text, int& out)
{
    if (!text || !text[0])
    {
        return false;
    }

    int value = 0;
    for (const char* p = text; *p; ++p)
    {
        if (*p < '0' || *p > '9')
        {
            return false;
        }
        if (value > 1999)
        {
            out = 20000;
            return true;
        }
        value = value * 10 + (*p - '0');
    }
    out = value;
    return true;
}

static int StepRemoveModelId(int modelId, bool increase)
{
    if (increase)
    {
        if (modelId < -1)
        {
            return -1;
        }
        if (modelId < 321)
        {
            return 321;
        }
        if (modelId >= 373 && modelId < 615)
        {
            return 615;
        }
        return modelId < 19999 ? modelId + 1 : 19999;
    }
    if (modelId > 19999)
    {
        return 19999;
    }
    if (modelId <= 321)
    {
        return -1;
    }
    if (modelId > 373 && modelId <= 615)
    {
        return 373;
    }
    return modelId - 1;
}

static int FilterIntegerModelInput(ImGuiInputTextCallbackData* data)
{
    return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
}

struct PropertyWidgetResult
{
    bool changed = false;
    bool active = false;
    unsigned parts = 0;
    void Capture(bool edited, unsigned part = 1)
    {
        changed |= edited;
        active |= ImGui::IsItemActive();
        if (edited)
        {
            parts |= part;
        }
    }
};

template <typename Step>
static void DrawNumericStepButtons(const char* id, float btnSz, float rowPad, PropertyWidgetResult* result, Step step)
{
    ImGui::SameLine(0, rowPad);
    ImGui::PushButtonRepeat(true);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, FrameTextPadY()));

    char decId[64], incId[64];
    snprintf(decId, sizeof(decId), "##dec%.50s", id);
    snprintf(incId, sizeof(incId), "##inc%.50s", id);

    const bool decrement = IconButton(decId, g_icons->subtractTexture, btnSz);
    if (result)
    {
        result->Capture(decrement);
    }
    if (decrement)
    {
        step(false);
    }
    ImGui::SameLine(0, rowPad);
    const bool increment = IconButton(incId, g_icons->addTexture, btnSz);
    if (result)
    {
        result->Capture(increment);
    }
    if (increment)
    {
        step(true);
    }

    ImGui::PopStyleVar();
    ImGui::PopButtonRepeat();
}

static void DrawDragIntWithButtons(const char* id, int* value, float speed, int min, int max, const char* format, int stepDec,
    int stepInc, float fieldW, float btnSz, float rowPad, bool mixed = false, PropertyWidgetResult* result = nullptr)
{
    const float dragW = fieldW - (btnSz * 2.0f) - (rowPad * 2.0f);

    ImGui::SetNextItemWidth(dragW);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, FrameTextPadY()));
    ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixed);
    const bool dragChanged = ImGui::DragInt(id, value, speed, min, max, format, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopItemFlag();
    if (result)
    {
        result->Capture(dragChanged);
    }
    LockMouseCursorForActiveDrag();
    ImGui::PopStyleVar();
    *value = ImClamp(*value, min, max);

    DrawNumericStepButtons(id, btnSz, rowPad, result,
        [&](bool increment)
        {
            const long long delta = increment ? static_cast<long long>(stepInc) : -static_cast<long long>(stepDec);
            *value = static_cast<int>(
                std::clamp(static_cast<long long>(*value) + delta, static_cast<long long>(min), static_cast<long long>(max)));
        });
}

static bool ValidateScalarInput(float& value, float previous, bool changed, ImGuiID id, bool wasInput)
{
    if (!changed)
    {
        return false;
    }
    bool valid = std::isfinite(value);
    if (wasInput || ImGui::TempInputIsActive(id))
    {
        const auto* input = ImGui::GetInputTextState(id);
        if (input)
        {
            char text[64];
            ImTextStrToUtf8(text, sizeof(text), input->TextW.Data, input->TextW.Data + input->CurLenW);
            valid &= std::isfinite(strtof(text, nullptr));
        }
    }
    if (valid)
    {
        return true;
    }
    value = previous;
    GuiShowToast("Invalid numeric value.", true);
    return false;
}

static bool DrawDragFloatWithButtons(const char* id, float* value, float speed, float min, float max, const char* format,
    float stepDec, float stepInc, float fieldW, float btnSz, float rowPad, bool smartFormat = false, bool mixed = false,
    PropertyWidgetResult* result = nullptr)
{
    const float dragW = fieldW - (btnSz * 2.0f) - (rowPad * 2.0f);

    char displayBuf[48];
    if (mixed)
    {
        snprintf(displayBuf, sizeof(displayBuf), "%s", PropertyEdit::MixedText);
    }
    else if (smartFormat)
    {
        SmartFormat(displayBuf, sizeof(displayBuf), *value);
    }
    else
    {
        snprintf(displayBuf, sizeof(displayBuf), format, *value);
    }
    const ImVec2 textSz = ImGui::CalcTextSize(displayBuf);
    const float centerPadX = ImMax(Theme::g_padding, (dragW - textSz.x) * 0.5f);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(centerPadX, FrameTextPadY()));
    ImGui::SetNextItemWidth(dragW);
    ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixed);
    const float previous = *value;
    const bool wasInput = ImGui::TempInputIsActive(ImGui::GetID(id));
    bool dragChanged = ImGui::DragFloat(id, value, speed, min, max, smartFormat ? "" : format, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopItemFlag();
    dragChanged = ValidateScalarInput(*value, previous, dragChanged, ImGui::GetItemID(), wasInput);
    if (result)
    {
        result->Capture(dragChanged);
    }
    LockMouseCursorForActiveDrag();
    ImGui::PopStyleVar();
    const bool dragHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);
    *value = ImClamp(*value, min, max);
    if (smartFormat && !ImGui::TempInputIsActive(ImGui::GetItemID()))
    {
        if (mixed && !dragChanged)
        {
            snprintf(displayBuf, sizeof(displayBuf), "%s", PropertyEdit::MixedText);
        }
        else
        {
            SmartFormat(displayBuf, sizeof(displayBuf), *value);
        }
        const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        const ImVec2 size = ImGui::CalcTextSize(displayBuf);
        auto* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(min, max, true);
        dl->AddText(ImVec2(floorf(min.x + (max.x - min.x - size.x) * 0.5f), min.y + FrameTextPadY()),
            ImGui::GetColorU32(Theme::g_text), displayBuf);
        dl->PopClipRect();
    }

    DrawNumericStepButtons(id, btnSz, rowPad, result,
        [&](bool increment)
        {
            *value = increment ? ImMin(max, *value + stepInc) : ImMax(min, *value - stepDec);
        });
    return dragHovered;
}

static bool IconLabelButton(
    const char* id, const char* label, ImTextureID icon, float w, float iconSize = Theme::g_iconSize, bool centerText = true)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
    bool clicked = ImGui::Button(id, ImVec2(w, Theme::g_frameHeight));
    ImGui::PopStyleVar();

    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const ImU32 txtCol
        = ImGui::IsItemActive() ? Theme::g_textHover : (ImGui::IsItemHovered() ? Theme::g_textHover : Theme::g_text);
    DrawIcon(dl, icon, ImVec2(mn.x + Theme::g_padding, mn.y + (mx.y - mn.y - iconSize) * 0.5f), iconSize, txtCol);

    const ImVec2 ts = ImGui::CalcTextSize(label);
    const float iconRight = mn.x + Theme::g_padding + iconSize;
    const float textX = centerText ? ImMax(iconRight, (iconRight + mx.x - ts.x) * 0.5f) : iconRight + Theme::g_padding * 0.75f;
    dl->AddText(ImVec2(textX, mn.y + (mx.y - mn.y - ts.y) * 0.5f), txtCol, label);
    return clicked;
}

static void SelectableField(
    const char* id, const char* text, float w, float h, ImFont* font, ImU32 col, float padX, bool frame, bool fixedH = false)
{
    char buf[256];
    strncpy_s(buf, sizeof(buf), text, _TRUNCATE);
    const float fontSize = ScaledFontSize(font);
    const float padY = ImMax(0.0f, ((frame ? Theme::g_frameHeight : h) - fontSize) * 0.5f);

    ImGui::PushFont(font);
    if (!frame)
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0, 0, 0, 0));
    }
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, Theme::g_accentSoft);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, padY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, frame ? Ui::Hairline() : 0.0f);
    ImGui::PushTabStop(false);
    ImGui::SetNextItemWidth(w);
    const ImGuiInputTextFlags flags = ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoHorizontalScroll;
    if (fixedH)
    {
        ImGui::InputTextEx(id, nullptr, buf, (int)sizeof(buf), ImVec2(w, Theme::g_frameHeight), flags);
    }
    else
    {
        ImGui::InputText(id, buf, sizeof(buf), flags);
    }
    ImGui::PopTabStop();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(frame ? 2 : 5);
    ImGui::PopFont();
}
