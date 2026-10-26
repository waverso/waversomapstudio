// Source Repository: https://github.com/waverso/waversomapstudio

struct ComponentInfoChip
{
    const char* label;
    ImU32 color;
};

struct ComponentInfoRow
{
    const char* label;
    const char* value;
};

struct ComponentInfoImage
{
    IDirect3DTexture9* texture = nullptr;
    ImVec2 uvMin = ImVec2(0.0f, 0.0f);
    ImVec2 uvMax = ImVec2(1.0f, 1.0f);
    float aspect = 1.0f;
};

struct ComponentInfoResult
{
    bool action = false;
    bool details = false;
};

static bool ComponentIconButton(const char* id, IDirect3DTexture9* icon, ImU32 color)
{
    const bool clicked = ImGui::Button(id, ImVec2(Theme::g_frameHeight, Theme::g_frameHeight));
    if (icon)
    {
        DrawIconCentered(ImGui::GetWindowDrawList(), icon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Theme::g_iconSize,
            ImGui::GetColorU32(color));
    }
    return clicked;
}

static void ComponentInfoText(const char* id, const char* text, const ImVec2& pos, float width, float height, ImFont* font,
    ImU32 color, bool rightAligned = false)
{
    if (width <= 0.0f)
    {
        return;
    }
    const float size = ScaledFontSize(font);
    const float textWidth = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
    const float padding = rightAligned ? ImMax(0.0f, width - textWidth) : 0.0f;
    ImGui::SetCursorScreenPos(pos);
    SelectableField(id, text, width, height, font, color, padding, false);
    if (textWidth > width && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        SetStyledTooltip(text);
    }
}

template <typename GetImage>
static ComponentInfoResult DrawComponentInfoCard(const char* id, float width, const char* name, const ComponentInfoChip* chips,
    int chipCount, const ComponentInfoRow (&rows)[3], GetImage getImage, IDirect3DTexture9* actionIcon, ImU32 actionColor,
    bool actionEnabled = true, bool detailsEnabled = true, bool showThumbnail = true)
{
    ComponentInfoResult result;
    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, FrameTextPadY()));

    ImFont* font = ImGui::GetFont();
    ImFont* smallFont = g_gui->m_fontSmall;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float sm = ScaledFontSize(smallFont);
    const float pad = Theme::g_padding;
    const float gap = Theme::g_padding;
    const float headerH = ImMax(ImGui::GetFontSize(), Theme::g_iconSize);
    const float thumbSize = floorf(Theme::g_frameHeight * 2.8f);
    const float rowH = sm + Ui::Px(8.0f);
    const float bodyH = showThumbnail ? ImMax(thumbSize, rowH * 3.0f) : rowH * 3.0f;
    const float contentW = ImMax(1.0f, width - pad * 2.0f);
    const float chipH = sm + Ui::Px(8.0f);
    const float chipPadX = Ui::Px(10.0f);
    const float chipGap = Theme::g_padding * 0.5f;

    auto chipWidth = [&](int index)
    {
        return ImMin(contentW, smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, chips[index].label).x + chipPadX * 2.0f);
    };
    auto lineEnd = [&](int first, float& lineWidth)
    {
        int end = first + 1;
        lineWidth = chipWidth(first);
        while (end < chipCount && lineWidth + chipGap + chipWidth(end) <= contentW)
        {
            lineWidth += chipGap + chipWidth(end);
            ++end;
        }
        return end;
    };

    int lineCount = 0;
    for (int first = 0; first < chipCount; ++lineCount)
    {
        float lineWidth;
        first = lineEnd(first, lineWidth);
    }
    const float chipsH = lineCount ? lineCount * chipH + (lineCount - 1) * chipGap : 0.0f;
    const float cardH = pad * 2.0f + headerH + gap + bodyH + (lineCount ? chipsH + gap : 0.0f);

    ImGui::SetCursorPosX(0.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 maximum(origin.x + width, origin.y + cardH);
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(width, cardH));
    dl->AddRectFilled(origin, maximum, ImGui::GetColorU32(Theme::g_infoCardBackground));
    dl->AddRect(origin, maximum, ImGui::GetColorU32(Theme::g_border), 0.0f, 0, Ui::Hairline());
    ImGui::PushClipRect(origin, maximum, true);

    const float left = origin.x + pad;
    const float right = maximum.x - pad;
    float y = origin.y + pad;
    const float nameInset = Theme::g_iconSize + gap;
    const float nameWidth = ImMax(1.0f, contentW - nameInset * 2.0f);
    const float textWidth = font->CalcTextSizeA(ScaledFontSize(font), FLT_MAX, 0.0f, name).x;
    ImGui::SetCursorScreenPos(ImVec2(left + nameInset, y));
    SelectableField("##name", name, nameWidth, headerH, font, Theme::g_text, ImMax(0.0f, (nameWidth - textWidth) * 0.5f), false);
    if (textWidth > nameWidth && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        SetStyledTooltip(name);
    }
    ImGui::SetCursorScreenPos(ImVec2(right - Theme::g_iconSize, y + (headerH - Theme::g_iconSize) * 0.5f));
    ImGui::BeginDisabled(!actionEnabled);
    result.action = ImGui::InvisibleButton("##action", ImVec2(Theme::g_iconSize, Theme::g_iconSize));
    if (actionIcon)
    {
        DrawIconCentered(
            dl, actionIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Theme::g_iconSize, ImGui::GetColorU32(actionColor));
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImGui::EndDisabled();
    y += headerH + gap;

    for (int first = 0; first < chipCount;)
    {
        float lineWidth;
        const int end = lineEnd(first, lineWidth);
        float x = left + (contentW - lineWidth) * 0.5f;
        for (int index = first; index < end; ++index)
        {
            const float chipW = chipWidth(index);
            const ImVec2 chipMin(x, y);
            const ImVec2 chipMax(x + chipW, y + chipH);
            const ImU32 background = (chips[index].color & 0x00FFFFFFu) | (32u << 24);
            dl->AddRectFilled(chipMin, chipMax, ImGui::GetColorU32(background));
            dl->PushClipRect(chipMin, chipMax, true);
            const float textW = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, chips[index].label).x;
            dl->AddText(smallFont, sm, ImVec2(x + ImMax(0.0f, (chipW - textW) * 0.5f), y + (chipH - sm) * 0.5f),
                ImGui::GetColorU32(chips[index].color), chips[index].label);
            dl->PopClipRect();
            x += chipW + chipGap;
        }
        first = end;
        y += chipH + (first < chipCount ? chipGap : gap);
    }

    if (showThumbnail)
    {
        const ImVec2 thumbMin(left, y + (bodyH - thumbSize) * 0.5f);
        const ImVec2 thumbMax(thumbMin.x + thumbSize, thumbMin.y + thumbSize);
        ImGui::SetCursorScreenPos(thumbMin);
        ImGui::BeginDisabled(!detailsEnabled);
        result.details = ImGui::InvisibleButton("##thumbnail", ImVec2(thumbSize, thumbSize));
        dl->AddRectFilled(thumbMin, thumbMax, ImGui::GetColorU32(Theme::g_controlBackground));
        ComponentInfoImage image;
        if (ImGui::IsRectVisible(thumbMin, thumbMax))
        {
            image = getImage();
        }
        if (image.texture)
        {
            const float aspect = image.aspect > 0.0f ? image.aspect : 1.0f;
            const ImVec2 size(thumbSize * ImMin(aspect, 1.0f), thumbSize / ImMax(aspect, 1.0f));
            const ImVec2 imageMin(thumbMin.x + (thumbSize - size.x) * 0.5f, thumbMin.y + (thumbSize - size.y) * 0.5f);
            const ImVec2 imageMax(imageMin.x + size.x, imageMin.y + size.y);
            dl->AddImage(
                (ImTextureID)image.texture, imageMin, imageMax, image.uvMin, image.uvMax, ImGui::GetColorU32(IM_COL32_WHITE));
        }
        else
        {
            const char* text = "No Preview";
            const float textW = smallFont->CalcTextSizeA(sm, FLT_MAX, 0.0f, text).x;
            dl->PushClipRect(thumbMin, thumbMax, true);
            dl->AddText(smallFont, sm, ImVec2(thumbMin.x + (thumbSize - textW) * 0.5f, thumbMin.y + (thumbSize - sm) * 0.5f),
                ImGui::GetColorU32(Theme::g_textDim), text);
            dl->PopClipRect();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            dl->AddRectFilled(
                thumbMin, thumbMax, ImGui::GetColorU32(Theme::Alpha(Theme::g_highlight, ImGui::IsItemActive() ? 50 : 18)));
        }
        dl->AddRect(thumbMin, thumbMax, ImGui::GetColorU32(Theme::g_border), 0.0f, 0, Ui::Hairline());
        ImGui::EndDisabled();
    }
    const float metaX = showThumbnail ? left + thumbSize + gap : left;
    const float metaW = ImMax(0.0f, right - metaX);
    const float labelW = ImMin(Ui::Px(68.0f), ImMax(0.0f, metaW - gap));
    const float valueX = metaX + labelW + gap;
    const float valueW = ImMax(0.0f, right - valueX);
    y += (bodyH - rowH * 3.0f) * 0.5f;
    for (int index = 0; index < 3; ++index)
    {
        ImGui::PushID(index);
        ComponentInfoText("##label", rows[index].label, ImVec2(metaX, y), labelW, rowH, smallFont, Theme::g_textDim);
        ComponentInfoText("##value", rows[index].value, ImVec2(valueX, y), valueW, rowH, smallFont, Theme::g_text, true);
        ImGui::PopID();
        y += rowH;
    }

    ImGui::PopClipRect();
    ImGui::SetCursorScreenPos(ImVec2(origin.x, maximum.y));
    ImGui::Dummy(ImVec2(width, 0.0f));
    ImGui::PopStyleVar(2);
    ImGui::EndGroup();
    ImGui::PopID();
    return result;
}
