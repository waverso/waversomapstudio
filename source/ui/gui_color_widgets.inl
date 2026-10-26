// Source Repository: https://github.com/waverso/waversomapstudio

static bool ParseHexColor(const char* buf, ImVec4& out, bool rgba = false)
{
    if (!buf)
    {
        return false;
    }
    const char* digits = buf;
    if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
    {
        digits += 2;
    }
    const size_t length = strlen(digits);
    if (!length || length > 8)
    {
        return false;
    }
    for (size_t i = 0; i < length; ++i)
    {
        if (!((digits[i] >= '0' && digits[i] <= '9') || (digits[i] >= 'a' && digits[i] <= 'f')
                || (digits[i] >= 'A' && digits[i] <= 'F')))
        {
            return false;
        }
    }
    unsigned int raw = static_cast<unsigned int>(strtoul(digits, nullptr, 16));
    if (rgba)
    {
        raw = (raw >> 8) | (raw << 24);
    }
    out = ImVec4(
        ((raw >> 16) & 0xFF) / 255.0f, ((raw >> 8) & 0xFF) / 255.0f, ((raw >> 0) & 0xFF) / 255.0f, ((raw >> 24) & 0xFF) / 255.0f);
    return true;
}

static void Vec4ToHex(char* buf, size_t sz, const ImVec4& col, bool rgba = false)
{
    if (sz < 9)
    {
        return;
    }
    const ImVec4 ordered = rgba ? col : ImVec4(col.w, col.x, col.y, col.z);
    snprintf(buf, sz, "%02X%02X%02X%02X", (int)(ordered.x * 255), (int)(ordered.y * 255), (int)(ordered.z * 255),
        (int)(ordered.w * 255));
}

static ImVec4 ARGBToVec4(unsigned int c)
{
    return ImVec4(((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f, ((c >> 24) & 0xFF) / 255.0f);
}

static unsigned int Vec4ToARGB(const ImVec4& v)
{
    return ((unsigned int)(v.w * 255) << 24) | ((unsigned int)(v.x * 255) << 16) | ((unsigned int)(v.y * 255) << 8)
        | (unsigned int)(v.z * 255);
}

enum class VehicleColorPickerStyle : std::uint8_t
{
    Quadrant,
    OklabColumns
};
static VehicleColorPickerStyle g_vehicleColorPickerStyle = VehicleColorPickerStyle::OklabColumns;
static constexpr const char* VehicleColorPickerStyleNames[] = { "Quadrant Picker", "Oklab Picker" };

static constexpr std::array<short, 256> VehicleColorGridOrderQuadrant = { -1, 40, 75, 164, 37, 254, 35, 13, 62, 74, 242, 43, 249,
    173, 66, 129, 0, 150, 252, 25, 122, 105, 11, 39, 88, 18, 22, 212, 121, 174, 179, 244, 148, 83, 24, 52, 250, 140, 120, 111,
    180, 147, 78, 190, 17, 117, 30, 230, 36, 251, 27, 33, 213, 48, 63, 69, 158, 175, 233, 85, 239, 58, 132, 168, 4, 119, 92, 193,
    29, 96, 15, 56, 220, 143, 178, 182, 80, 82, 31, 224, 113, 34, 253, 23, 19, 89, 26, 90, 55, 146, 161, 222, 70, 3, 115, 248,
    109, 41, 50, 192, 38, 64, 138, 14, 185, 177, 232, 237, 183, 42, 124, 45, 72, 9, 196, 60, 76, 49, 8, 1, 126, 218, 176, 5, 184,
    21, 181, 84, 51, 238, 197, 104, 46, 77, 145, 68, 118, 32, 240, 135, 163, 166, 208, 91, 151, 128, 61, 228, 57, 107, 6, 73, 155,
    157, 71, 12, 2, 167, 130, 247, 131, 231, 81, 137, 221, 156, 102, 99, 97, 217, 98, 7, 59, 165, 144, 207, 189, 86, 219, 142,
    154, 65, 195, 194, 169, 67, 170, 100, 10, 152, 94, 54, 159, 44, 16, 110, 47, 200, 241, 226, 136, 112, 93, 108, 139, 171, 116,
    101, 149, 227, 199, 153, 229, 191, 243, 141, 246, 87, 204, 162, 198, 125, 134, 201, 215, 187, 172, 202, 123, 214, 114, 216,
    20, 106, 28, 103, 79, 211, 53, 236, 133, 186, 245, 160, 188, 234, 235, 225, 209, 95, 255, 210, 203, 223, 206, 205 };

static constexpr std::array<short, 256> VehicleColorGridOrderOklabColumns = { -1, 0, 40, 230, 66, 129, 244, 149, 215, 133, 227,
    236, 206, 201, 205, 179, 4, 75, 148, 249, 30, 224, 173, 159, 172, 186, 189, 207, 203, 53, 223, 242, 25, 36, 164, 252, 121, 43,
    168, 231, 199, 245, 202, 94, 210, 255, 101, 190, 24, 37, 83, 248, 117, 132, 174, 238, 225, 187, 44, 130, 95, 79, 211, 147, 13,
    35, 109, 251, 74, 45, 131, 81, 214, 160, 151, 165, 209, 116, 134, 233, 27, 72, 119, 22, 62, 113, 239, 61, 47, 150, 51, 52,
    103, 125, 54, 178, 11, 34, 122, 254, 31, 84, 180, 141, 142, 188, 153, 166, 106, 247, 198, 237, 9, 41, 105, 80, 115, 212, 123,
    200, 197, 234, 16, 93, 204, 91, 171, 143, 23, 33, 92, 85, 124, 58, 182, 104, 228, 235, 114, 100, 87, 28, 144, 5, 29, 50, 140,
    21, 18, 17, 183, 57, 65, 86, 191, 2, 246, 20, 139, 220, 19, 48, 111, 184, 82, 181, 216, 102, 221, 128, 243, 163, 59, 152, 167,
    232, 15, 63, 192, 250, 88, 3, 55, 107, 195, 229, 137, 12, 112, 162, 136, 146, 26, 60, 96, 253, 78, 175, 219, 99, 194, 241,
    154, 98, 67, 208, 170, 176, 8, 56, 76, 193, 42, 158, 110, 77, 156, 226, 240, 135, 39, 108, 169, 177, 14, 64, 90, 213, 70, 222,
    120, 6, 46, 73, 155, 217, 71, 10, 157, 185, 1, 49, 138, 196, 161, 218, 69, 89, 68, 145, 38, 97, 32, 7, 118, 126 };

static constexpr std::array<ImU32, 128> SampVehicleColorTable
    = { IM_COL32(23, 117, 23, 255), IM_COL32(33, 6, 6, 255), IM_COL32(18, 84, 120, 255), IM_COL32(69, 42, 13, 255),
          IM_COL32(87, 30, 30, 255), IM_COL32(1, 7, 1, 255), IM_COL32(37, 34, 90, 255), IM_COL32(44, 137, 170, 255),
          IM_COL32(138, 77, 189, 255), IM_COL32(53, 150, 58, 255), IM_COL32(183, 183, 183, 255), IM_COL32(70, 76, 141, 255),
          IM_COL32(132, 136, 140, 255), IM_COL32(129, 120, 103, 255), IM_COL32(129, 122, 38, 255), IM_COL32(106, 80, 111, 255),
          IM_COL32(88, 62, 111, 255), IM_COL32(140, 185, 114, 255), IM_COL32(130, 79, 120, 255), IM_COL32(109, 39, 106, 255),
          IM_COL32(30, 29, 19, 255), IM_COL32(30, 19, 6, 255), IM_COL32(31, 37, 24, 255), IM_COL32(44, 69, 49, 255),
          IM_COL32(30, 76, 153, 255), IM_COL32(46, 95, 67, 255), IM_COL32(30, 153, 72, 255), IM_COL32(30, 153, 153, 255),
          IM_COL32(153, 153, 118, 255), IM_COL32(124, 132, 153, 255), IM_COL32(153, 46, 30, 255), IM_COL32(44, 30, 8, 255),
          IM_COL32(20, 36, 7, 255), IM_COL32(153, 62, 77, 255), IM_COL32(30, 76, 153, 255), IM_COL32(25, 129, 129, 255),
          IM_COL32(26, 41, 42, 255), IM_COL32(22, 97, 111, 255), IM_COL32(27, 102, 135, 255), IM_COL32(108, 63, 153, 255),
          IM_COL32(72, 26, 14, 255), IM_COL32(122, 115, 153, 255), IM_COL32(116, 109, 153, 255), IM_COL32(83, 56, 126, 255),
          IM_COL32(34, 36, 7, 255), IM_COL32(62, 25, 12, 255), IM_COL32(70, 33, 14, 255), IM_COL32(153, 30, 30, 255),
          IM_COL32(141, 76, 141, 255), IM_COL32(128, 91, 128, 255), IM_COL32(123, 62, 126, 255), IM_COL32(60, 23, 55, 255),
          IM_COL32(115, 53, 23, 255), IM_COL32(120, 24, 24, 255), IM_COL32(131, 52, 26, 255), IM_COL32(142, 47, 28, 255),
          IM_COL32(126, 62, 83, 255), IM_COL32(124, 109, 124, 255), IM_COL32(2, 12, 2, 255), IM_COL32(7, 36, 7, 255),
          IM_COL32(22, 48, 18, 255), IM_COL32(22, 48, 27, 255), IM_COL32(100, 43, 79, 255), IM_COL32(54, 132, 82, 255),
          IM_COL32(153, 149, 144, 255), IM_COL32(129, 141, 150, 255), IM_COL32(153, 153, 30, 255), IM_COL32(127, 153, 76, 255),
          IM_COL32(131, 146, 146, 255), IM_COL32(120, 130, 34, 255), IM_COL32(43, 60, 153, 255), IM_COL32(58, 58, 11, 255),
          IM_COL32(138, 121, 78, 255), IM_COL32(14, 31, 73, 255), IM_COL32(21, 55, 28, 255), IM_COL32(21, 39, 58, 255),
          IM_COL32(55, 87, 117, 255), IM_COL32(6, 8, 32, 255), IM_COL32(7, 19, 38, 255), IM_COL32(32, 57, 75, 255),
          IM_COL32(44, 80, 137, 255), IM_COL32(21, 66, 108, 255), IM_COL32(16, 50, 80, 255), IM_COL32(36, 22, 99, 255),
          IM_COL32(105, 32, 21, 255), IM_COL32(140, 141, 148, 255), IM_COL32(81, 96, 19, 255), IM_COL32(9, 15, 2, 255),
          IM_COL32(140, 87, 58, 255), IM_COL32(82, 136, 142, 255), IM_COL32(153, 92, 82, 255), IM_COL32(153, 88, 30, 255),
          IM_COL32(153, 58, 99, 255), IM_COL32(153, 143, 78, 255), IM_COL32(153, 49, 30, 255), IM_COL32(13, 24, 66, 255),
          IM_COL32(82, 30, 30, 255), IM_COL32(66, 66, 13, 255), IM_COL32(76, 153, 30, 255), IM_COL32(8, 42, 29, 255),
          IM_COL32(150, 130, 29, 255), IM_COL32(25, 127, 25, 255), IM_COL32(59, 20, 31, 255), IM_COL32(116, 82, 23, 255),
          IM_COL32(137, 63, 141, 255), IM_COL32(126, 26, 108, 255), IM_COL32(11, 55, 11, 255), IM_COL32(39, 69, 13, 255),
          IM_COL32(7, 31, 36, 255), IM_COL32(120, 69, 115, 255), IM_COL32(138, 101, 58, 255), IM_COL32(115, 38, 23, 255),
          IM_COL32(49, 148, 144, 255), IM_COL32(86, 148, 29, 255), IM_COL32(89, 22, 61, 255), IM_COL32(27, 138, 47, 255),
          IM_COL32(56, 22, 11, 255), IM_COL32(4, 24, 4, 255), IM_COL32(53, 93, 142, 255), IM_COL32(46, 63, 91, 255),
          IM_COL32(86, 26, 40, 255), IM_COL32(78, 14, 39, 255), IM_COL32(112, 108, 103, 255), IM_COL32(59, 62, 66, 255),
          IM_COL32(46, 45, 51, 255), IM_COL32(123, 126, 125, 255), IM_COL32(74, 68, 66, 255), IM_COL32(40, 52, 78, 255) };

static ImU32 VehicleColorToU32(int color)
{
    if (color < 128)
    {
        const auto* colors = reinterpret_cast<const RwRGBA*>(0xB4E480);
        return IM_COL32(colors[color].red, colors[color].green, colors[color].blue, 255);
    }
    return SampVehicleColorTable[color - 128];
}

static ImU32 ApplyCurrentAlpha(ImU32 color)
{
    const ImU32 alpha = static_cast<ImU32>(((color >> 24) & 0xFF) * ImGui::GetStyle().Alpha);
    return (color & 0x00FFFFFFu) | (alpha << 24);
}

static ImVec2 ColorPopupPosition(const ImVec2& anchorMin, const ImVec2& anchorMax, const ImVec2& size, float gap)
{
    const auto display = ImGui::GetIO().DisplaySize;
    float x = anchorMax.x + gap;
    float y = anchorMin.y;
    if (x + size.x > display.x)
    {
        x = anchorMin.x - size.x - gap;
    }
    if (x < 0.0f)
    {
        x = ImClamp(anchorMin.x, 0.0f, display.x - size.x);
    }
    if (y + size.y > display.y)
    {
        y = anchorMax.y - size.y;
    }
    y = ImClamp(y, 0.0f, display.y - size.y);
    return ImVec2(x, y);
}

static ImVec2 ColorPickerPopupSize(bool alpha)
{
    const auto& style = ImGui::GetStyle();
    const float width = floorf(Ui::Px(305.0f));
    const float frame = ImGui::GetFrameHeight();
    const float square
        = ImMax(frame, floorf(width - Theme::g_padding * 2) - (alpha ? 2 : 1) * (frame + style.ItemInnerSpacing.x));
    return ImVec2(width, floorf(Theme::g_padding + square + style.ItemSpacing.y) + frame + Theme::g_padding + 1.0f);
}

static void DrawVehicleColorPicker(
    int& color, float width, float btnSz, float rowPad, bool mixed = false, PropertyWidgetResult* result = nullptr)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float swatchX = ImGui::GetCursorPosX() + width - btnSz;
    const float inputW = width - btnSz - rowPad;
    ImGui::SetNextItemWidth(inputW);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::g_padding, FrameTextPadY()));
    ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixed);
    const bool changed = ImGui::DragInt("##vehicle_color", &color, 0.1f, -1, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopItemFlag();
    if (result)
    {
        result->Capture(changed);
    }
    LockMouseCursorForActiveDrag();
    ImGui::PopStyleVar();
    const ImVec2 inputMin = ImGui::GetItemRectMin();
    const ImVec2 inputMax = ImGui::GetItemRectMax();

    ImGui::SameLine(0.0f, rowPad);
    ImGui::SetCursorPosX(swatchX);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, FrameTextPadY()));
    if (ImGui::Button("##vehicle_color_picker", ImVec2(btnSz, btnSz)))
    {
        ImGui::OpenPopup("##vehicle_color_popup");
    }
    const ImVec2 swatchMin = ImGui::GetItemRectMin();
    const ImVec2 swatchMax = ImGui::GetItemRectMax();
    const auto swatchRect = ImGui::GetColorSwatchRect(ImRect(swatchMin, swatchMax), Ui::Px(2.0f));
    const ImVec2 colorMin = swatchRect.Min, colorMax = swatchRect.Max;
    const ImU32 swatchColor = ApplyCurrentAlpha(mixed ? Theme::g_controlBackground
            : color < 0                               ? Theme::g_textDim
                                                      : VehicleColorToU32(color));
    dl->AddRectFilled(colorMin, colorMax, swatchColor);
    if (mixed)
    {
        const auto size = ImGui::CalcTextSize(PropertyEdit::MixedText);
        dl->AddText(ImVec2((colorMin.x + colorMax.x - size.x) * 0.5f, (colorMin.y + colorMax.y - size.y) * 0.5f),
            ImGui::GetColorU32(Theme::g_text), PropertyEdit::MixedText);
    }
    else if (color < 0 && g_icons && g_icons->randomTexture)
    {
        dl->AddImage(g_icons->randomTexture, colorMin, colorMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
            ApplyCurrentAlpha(Theme::g_windowBackground));
    }

    ImGui::PopStyleVar();

    const float cellSize = Ui::Px(18.0f);
    const float cellGap = Ui::Px(1.0f);
    const float popupSize = Theme::g_padding * 2.0f + (cellSize + cellGap) * 16.0f;
    const ImVec2 anchorMin(ImMin(inputMin.x, swatchMin.x), ImMin(inputMin.y, swatchMin.y));
    const ImVec2 anchorMax(ImMax(inputMax.x, swatchMax.x), ImMax(inputMax.y, swatchMax.y));
    ImGui::SetNextWindowPos(ColorPopupPosition(anchorMin, anchorMax, ImVec2(popupSize, popupSize), cellGap), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_padding, Theme::g_padding));
    if (ImGui::BeginPopup(
            "##vehicle_color_popup", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
    {
        ImDrawList* popupDl = ImGui::GetWindowDrawList();
        const auto& gridOrder = g_vehicleColorPickerStyle == VehicleColorPickerStyle::OklabColumns
            ? VehicleColorGridOrderOklabColumns
            : VehicleColorGridOrderQuadrant;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(cellGap, cellGap));
        for (int gridIndex = 0; gridIndex < 256; ++gridIndex)
        {
            const int colorIndex = gridOrder[gridIndex];
            ImGui::PushID(colorIndex);
            const bool clicked = ImGui::InvisibleButton("##vehicle_color", ImVec2(cellSize, cellSize));
            if (result)
            {
                result->Capture(clicked);
            }
            if (clicked)
            {
                color = colorIndex;
            }
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const bool selected = !mixed && color == colorIndex;
            popupDl->AddRectFilled(min, max, colorIndex < 0 ? Theme::g_textDim : VehicleColorToU32(colorIndex));
            if (colorIndex < 0 && g_icons && g_icons->randomTexture)
            {
                popupDl->AddImage(
                    g_icons->randomTexture, min, max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), Theme::g_windowBackground);
            }
            popupDl->AddRect(
                min, max, selected ? Theme::g_accent : Theme::g_border, 0.0f, 0, selected ? Ui::Px(2.0f) : Ui::Hairline());
            if (gridIndex % 16 != 15)
            {
                ImGui::SameLine(0.0f, cellGap);
            }
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

static bool ColorSwatchButton(const char* id, ImU32 color, ImVec2 size)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, FrameTextPadY()));
    const bool clicked = ImGui::Button(id, size);
    ImGui::PopStyleVar();
    const auto rect = ImGui::GetColorSwatchRect(ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), Ui::Px(3.0f));
    ImGui::GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max, ImGui::GetColorU32(color));
    return clicked;
}

static void DrawArgbColorRow(const char* label, const char* idbase, unsigned int& argb, char* buf, ImVec4& col, bool& hasFocus,
    unsigned int& lastARGB, float valueX, float fieldW, float btnSz, float rowPad, unsigned mixed = 0,
    PropertyWidgetResult* result = nullptr, bool rgba = false)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID(idbase);

    if (lastARGB != argb)
    {
        col = ARGBToVec4(argb);
        Vec4ToHex(buf, 12, col, rgba);
        lastARGB = argb;
    }

    DrawLabel(dl, label);
    ImVec2 colorInputMin, colorInputMax;
    const float colorInputW = fieldW - btnSz - rowPad;
    ImGui::SetCursorPosX(valueX);
    {
        char displayBuf[48];
        snprintf(displayBuf, sizeof(displayBuf), "0x%s (%s)", buf, rgba ? "RGBA" : "ARGB");
        const bool edited = CenteredDisplayInput("##col", buf, 12, colorInputW, mixed ? PropertyEdit::MixedText : displayBuf,
            hasFocus, ImGuiInputTextFlags_AutoSelectAll);
        if (result)
        {
            result->active |= hasFocus;
        }
        if (edited)
        {
            if (ParseHexColor(buf, col, rgba))
            {
                argb = Vec4ToARGB(col);
                lastARGB = argb;
                if (result)
                {
                    result->changed = true;
                    result->parts |= 15u;
                }
            }
        }
        colorInputMin = ImGui::GetItemRectMin();
        colorInputMax = ImGui::GetItemRectMax();
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            ImVec4 tmp;
            if (ParseHexColor(buf, tmp, rgba))
            {
                col = tmp;
                argb = Vec4ToARGB(col);
                lastARGB = argb;
                if (result)
                {
                    result->changed = true;
                    result->parts |= 15u;
                }
            }
            else
            {
                g_toast.ShowError(rgba ? "Invalid RGBA color." : "Invalid ARGB color.");
            }
            Vec4ToHex(buf, 12, col, rgba);
        }
    }

    ImGui::SameLine(0, rowPad);
    if (ColorSwatchButton(
            "##picker_btn", mixed ? Theme::g_controlBackground : ImGui::ColorConvertFloat4ToU32(col), ImVec2(btnSz, btnSz)))
    {
        ImGui::OpenPopup("##color_popup");
    }
    ImVec2 swatchMin = ImGui::GetItemRectMin();
    ImVec2 swatchMax = ImGui::GetItemRectMax();
    if (mixed)
    {
        const auto size = ImGui::CalcTextSize(PropertyEdit::MixedText);
        dl->AddText(ImVec2((swatchMin.x + swatchMax.x - size.x) * 0.5f, (swatchMin.y + swatchMax.y - size.y) * 0.5f),
            ImGui::GetColorU32(Theme::g_text), PropertyEdit::MixedText);
    }
    {
        const ImVec2 pickerSize = ColorPickerPopupSize(true);
        const float gap = Ui::Px(2.0f);
        ImVec2 anchorMin(ImMin(colorInputMin.x, swatchMin.x), ImMin(colorInputMin.y, swatchMin.y));
        ImVec2 anchorMax(ImMax(colorInputMax.x, swatchMax.x), ImMax(colorInputMax.y, swatchMax.y));
        ImGui::SetNextWindowPos(ColorPopupPosition(anchorMin, anchorMax, pickerSize, gap), ImGuiCond_Always);
        ImGui::SetNextWindowSize(pickerSize, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::g_padding, Theme::g_padding));
        if (ImGui::BeginPopup("##color_popup", ImGuiWindowFlags_NoMove))
        {
            unsigned channels = 0;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            const bool changed = ImGui::ColorPicker4("##cp", (float*)&col,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_DisplayRGB
                    | ImGuiColorEditFlags_NoDragDrop,
                nullptr, mixed, &channels);
            if (result)
            {
                result->Capture(changed, channels);
                result->active |= ImGui::GetCurrentContext()->ActiveIdWindow == ImGui::GetCurrentWindow();
            }
            if (changed)
            {
                Vec4ToHex(buf, 12, col, rgba);
                argb = Vec4ToARGB(col);
                lastARGB = argb;
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
    }

    ImGui::PopID();
}
