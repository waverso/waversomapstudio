// Source Repository: https://github.com/waverso/waversomapstudio

enum class RangeSliderCurve : std::uint8_t
{
    Linear,
    SineIn,
    QuadIn,
    CubicIn,
    CircIn,
    QuartIn,
    QuintIn,
    ExpoIn,
    SignedCubicIn
};

constexpr RangeSliderCurve FloatRangeCurve = RangeSliderCurve::QuadIn;

static float ApplyRangeCurve(RangeSliderCurve curve, float t)
{
    t = ImClamp(t, 0.0f, 1.0f);
    switch (curve)
    {
        case RangeSliderCurve::SineIn:
            return 1.0f - cosf(t * (0.5f * std::numbers::pi_v<float>));
        case RangeSliderCurve::QuadIn:
            return t * t;
        case RangeSliderCurve::CubicIn:
            return t * t * t;
        case RangeSliderCurve::CircIn:
            return 1.0f - sqrtf(ImMax(0.0f, 1.0f - t * t));
        case RangeSliderCurve::QuartIn:
        {
            const float t2 = t * t;
            return t2 * t2;
        }
        case RangeSliderCurve::QuintIn:
        {
            const float t2 = t * t;
            return t2 * t2 * t;
        }
        case RangeSliderCurve::ExpoIn:
            return t <= 0.0f ? 0.0f : powf(2.0f, 10.0f * t - 10.0f);
        case RangeSliderCurve::SignedCubicIn:
        {
            const float signedT = t * 2.0f - 1.0f;
            return 0.5f + 0.5f * signedT * signedT * signedT;
        }
        case RangeSliderCurve::Linear:
        default:
            return t;
    }
}

static float InverseRangeCurve(RangeSliderCurve curve, float value)
{
    value = ImClamp(value, 0.0f, 1.0f);
    if (curve == RangeSliderCurve::Linear)
    {
        return value;
    }
    if (curve == RangeSliderCurve::SignedCubicIn)
    {
        return 0.5f + 0.5f * cbrtf(value * 2.0f - 1.0f);
    }
    float lo = 0.0f, hi = 1.0f;
    for (int i = 0; i < 20; ++i)
    {
        const float mid = (lo + hi) * 0.5f;
        if (ApplyRangeCurve(curve, mid) < value)
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }
    return (lo + hi) * 0.5f;
}

static void DrawRangeSliderLabel(ImDrawList* dl, const char* label, float x0, float x1, float y0, float height)
{
    if (!label || !label[0])
    {
        return;
    }
    const ImVec2 size = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(floorf((x0 + x1 - size.x) * 0.5f), floorf(y0 + (height - size.y) * 0.5f)), Theme::g_text, label);
}

static void DrawRangeSliderTrack(ImDrawList* dl, const ImRect& rect)
{
    dl->AddRectFilled(rect.Min, rect.Max, ImGui::GetColorU32(ImGuiCol_FrameBg));
    const float shadowOffset = 1.0f;
    dl->AddRect(rect.Min + ImVec2(shadowOffset, shadowOffset), rect.Max + ImVec2(shadowOffset, shadowOffset),
        Theme::g_borderShadow, 0.0f, 0, Ui::Hairline());
    dl->AddRect(rect.Min, rect.Max, Theme::g_border, 0.0f, 0, Ui::Hairline());
}

static bool RangeSliderInt(const char* id, const char* label, int* vMin, int* vMax, int lo, int hi, float totalW, char* bufMin,
    char* bufMax, bool& focusMin, bool& focusMax, float inW = Ui::Px(64.0f), float gap = Theme::g_padding)
{
    if (hi <= lo)
    {
        return false;
    }
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID(id);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float y0 = origin.y;
    const float fh = Theme::g_frameHeight;
    const float bgX0 = origin.x + inW + gap;
    const float bgX1 = origin.x + totalW - inW - gap;
    bool changed = false;

    char dispMin[24];
    snprintf(dispMin, sizeof(dispMin), "%d", *vMin);
    if (!focusMin)
    {
        snprintf(bufMin, 16, "%d", *vMin);
    }
    ImGui::SetCursorScreenPos(origin);
    if (CenteredDisplayInput(
            "##min", bufMin, 16, inW, dispMin, focusMin, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll))
    {
        *vMin = ImClamp(atoi(bufMin), lo, *vMax);
        changed = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        snprintf(bufMin, 16, "%d", *vMin);
    }

    DrawRangeSliderTrack(dl, ImRect(ImVec2(bgX0, y0), ImVec2(bgX1, y0 + fh)));

    const float gw = Ui::Px(9.0f);
    const float gh = fh - Ui::Px(6.0f);
    const float gcy = y0 + fh * 0.5f;
    const float trackInset = Ui::Px(2.0f);
    const float trackLeft = bgX0 + gw * 0.5f + trackInset;
    const float trackRight = bgX1 - gw * 0.5f - trackInset;
    const float trackLen = ImMax(Ui::Hairline(), trackRight - trackLeft);
    auto posOf = [&](int v)
    {
        return trackLeft + (float)(v - lo) / (float)(hi - lo) * trackLen;
    };

    dl->AddRectFilled(ImVec2(posOf(*vMin), gcy - gh * 0.5f), ImVec2(posOf(*vMax), gcy + gh * 0.5f),
        ImGui::GetColorU32(ImGuiCol_SliderGrab, 0.45f));

    const float midpoint = (posOf(*vMin) + posOf(*vMax)) * 0.5f;
    bool lhOn = false, rhOn = false;
    auto submit = [&](bool minimum)
    {
        const float x0 = minimum ? bgX0 : midpoint;
        const float x1 = minimum ? midpoint : bgX1;
        ImGui::SetCursorScreenPos(ImVec2(x0, y0));
        ImGui::InvisibleButton(minimum ? "##lh" : "##rh", ImVec2(x1 - x0, fh));
        bool& active = minimum ? lhOn : rhOn;
        active = ImGui::IsItemActive();
        if (active)
        {
            const float t = ImClamp((io.MousePos.x - trackLeft) / trackLen, 0.0f, 1.0f);
            int& value = minimum ? *vMin : *vMax;
            const int next = ImClamp(lo + (int)(t * (hi - lo) + 0.5f), minimum ? lo : *vMin, minimum ? *vMax : hi);
            if (next != value)
            {
                value = next;
                changed = true;
            }
        }
    };
    submit(true);
    submit(false);

    auto grab = [&](float hx, bool on)
    {
        const ImU32 c = on ? ImGui::GetColorU32(ImGuiCol_SliderGrabActive) : ImGui::GetColorU32(ImGuiCol_SliderGrab);
        dl->AddRectFilled(ImVec2(hx - gw * 0.5f, gcy - gh * 0.5f), ImVec2(hx + gw * 0.5f, gcy + gh * 0.5f), c);
    };
    grab(posOf(*vMin), lhOn);
    grab(posOf(*vMax), rhOn);
    DrawRangeSliderLabel(dl, label, bgX0, bgX1, y0, fh);

    char dispMax[24];
    snprintf(dispMax, sizeof(dispMax), "%d", *vMax);
    if (!focusMax)
    {
        snprintf(bufMax, 16, "%d", *vMax);
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x + totalW - inW, y0));
    if (CenteredDisplayInput(
            "##max", bufMax, 16, inW, dispMax, focusMax, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll))
    {
        *vMax = ImClamp(atoi(bufMax), *vMin, hi);
        changed = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        snprintf(bufMax, 16, "%d", *vMax);
    }

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(totalW, Theme::g_frameHeight));
    ImGui::PopID();
    return changed;
}

static bool RangeSliderFloat(const char* id, const char* label, float* vMin, float* vMax, float lo, float hi, float totalW,
    char* bufMin, char* bufMax, bool& focusMin, bool& focusMax, RangeSliderCurve curve, float inW = Ui::Px(64.0f),
    float gap = Theme::g_padding, unsigned mixed = 0, PropertyWidgetResult* result = nullptr,
    const std::array<float, 4>* limits = nullptr)
{
    if (!std::isfinite(lo) || !std::isfinite(hi) || hi <= lo)
    {
        return false;
    }
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID(id);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float y0 = origin.y, fh = Theme::g_frameHeight;
    const float bgX0 = origin.x + inW + gap;
    const float bgX1 = origin.x + totalW - inW - gap;
    bool changed = false;

    auto format = [](char* out, size_t outSize, float value)
    {
        SmartFormat(out, outSize, value);
    };
    char dispMin[24];
    format(dispMin, sizeof(dispMin), *vMin);
    if (!focusMin)
    {
        snprintf(bufMin, 16, "%s", dispMin);
    }
    ImGui::SetCursorScreenPos(origin);
    const bool minEdited = CenteredDisplayInput("##min", bufMin, 16, inW, (mixed & 1u) ? PropertyEdit::MixedText : dispMin,
        focusMin, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll);
    if (result)
    {
        result->Capture(false, 1);
    }
    if (minEdited)
    {
        char* end = nullptr;
        const float value = strtof(bufMin, &end);
        if (end != bufMin && *end == 0 && std::isfinite(value))
        {
            *vMin = ImClamp(value, limits ? (*limits)[0] : lo, limits ? (*limits)[1] : *vMax);
            changed = true;
            if (result)
            {
                result->Capture(true, 1);
            }
        }
    }
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        format(dispMin, sizeof(dispMin), *vMin);
        snprintf(bufMin, 16, "%s", dispMin);
    }

    DrawRangeSliderTrack(dl, ImRect(ImVec2(bgX0, y0), ImVec2(bgX1, y0 + fh)));

    const float gw = Ui::Px(9.0f), gh = fh - Ui::Px(6.0f), gcy = y0 + fh * 0.5f;
    const float trackLeft = bgX0 + gw * 0.5f + Ui::Px(2.0f);
    const float trackRight = bgX1 - gw * 0.5f - Ui::Px(2.0f);
    const float trackLen = ImMax(Ui::Hairline(), trackRight - trackLeft);
    const float rangeCenter = (lo + hi) * 0.5f;
    const float rangeRadius = (hi - lo) * 0.5f;
    auto posOf = [&](float value)
    {
        const float normalized = curve == RangeSliderCurve::SignedCubicIn
            ? 0.5f + 0.5f * cbrtf((value - rangeCenter) / rangeRadius)
            : InverseRangeCurve(curve, (value - lo) / (hi - lo));
        return trackLeft + normalized * trackLen;
    };
    auto valueOf = [&](float t)
    {
        if (curve == RangeSliderCurve::SignedCubicIn)
        {
            const float signedT = t * 2.0f - 1.0f;
            return rangeCenter + rangeRadius * signedT * signedT * signedT;
        }
        return lo + ApplyRangeCurve(curve, t) * (hi - lo);
    };
    if (!mixed)
    {
        dl->AddRectFilled(ImVec2(posOf(*vMin), gcy - gh * 0.5f), ImVec2(posOf(*vMax), gcy + gh * 0.5f),
            ImGui::GetColorU32(ImGuiCol_SliderGrab, 0.45f));
    }

    const float midpoint = (posOf(*vMin) + posOf(*vMax)) * 0.5f;
    bool minActive = false, maxActive = false;
    auto submit = [&](bool minimum)
    {
        const float x0 = minimum ? bgX0 : midpoint;
        const float x1 = minimum ? midpoint : bgX1;
        ImGui::SetCursorScreenPos(ImVec2(x0, y0));
        ImGui::InvisibleButton(minimum ? "##lh" : "##rh", ImVec2(x1 - x0, fh));
        bool& active = minimum ? minActive : maxActive;
        active = ImGui::IsItemActive();
        const unsigned part = minimum ? 1u : 2u;
        bool edited = false;
        if (active)
        {
            const float t = ImClamp((io.MousePos.x - trackLeft) / trackLen, 0.0f, 1.0f);
            const int endpoint = minimum ? 0 : 2;
            const float next = ImClamp(valueOf(t), limits ? (*limits)[endpoint] : (minimum ? lo : *vMin),
                limits ? (*limits)[endpoint + 1] : (minimum ? *vMax : hi));
            float& value = minimum ? *vMin : *vMax;
            edited = next != value || (mixed & part);
            if (edited)
            {
                value = next;
                changed = true;
            }
        }
        if (result)
        {
            result->Capture(edited, part);
        }
    };
    submit(true);
    submit(false);
    auto drawGrab = [&](float x, bool active)
    {
        dl->AddRectFilled(ImVec2(x - gw * 0.5f, gcy - gh * 0.5f), ImVec2(x + gw * 0.5f, gcy + gh * 0.5f),
            active ? ImGui::GetColorU32(ImGuiCol_SliderGrabActive) : ImGui::GetColorU32(ImGuiCol_SliderGrab));
    };
    if (!(mixed & 1u) || minActive)
    {
        drawGrab(posOf(*vMin), minActive);
    }
    if (!(mixed & 2u) || maxActive)
    {
        drawGrab(posOf(*vMax), maxActive);
    }
    DrawRangeSliderLabel(dl, label, bgX0, bgX1, y0, fh);

    char dispMax[24];
    format(dispMax, sizeof(dispMax), *vMax);
    if (!focusMax)
    {
        snprintf(bufMax, 16, "%s", dispMax);
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x + totalW - inW, y0));
    const bool maxEdited = CenteredDisplayInput("##max", bufMax, 16, inW, (mixed & 2u) ? PropertyEdit::MixedText : dispMax,
        focusMax, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll);
    if (result)
    {
        result->Capture(false, 2);
    }
    if (maxEdited)
    {
        char* end = nullptr;
        const float value = strtof(bufMax, &end);
        if (end != bufMax && *end == 0 && std::isfinite(value))
        {
            *vMax = ImClamp(value, limits ? (*limits)[2] : *vMin, limits ? (*limits)[3] : hi);
            changed = true;
            if (result)
            {
                result->Capture(true, 2);
            }
        }
    }
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        format(dispMax, sizeof(dispMax), *vMax);
        snprintf(bufMax, 16, "%s", dispMax);
    }

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(totalW, Theme::g_frameHeight));
    ImGui::PopID();
    return changed;
}

static void EllipsizeComboLabel(char* out, size_t outSz, const char* text, float comboW)
{
    const float maxW = ImMax(1.0f, comboW - Theme::g_frameHeight - Theme::g_padding);
    EllipsizeText(out, outSz, text, maxW, ImGui::GetFontSize());
}

static void AppendLabelPart(char* out, size_t outSz, const char* part)
{
    const size_t len = strlen(out);
    if (len >= outSz)
    {
        return;
    }
    snprintf(out + len, outSz - len, "%s%s", len ? ", " : "", part);
}

template <size_t PartsSize = 256, size_t LabelSize = 288, typename Mask, typename Name>
static void DrawMaskFilter(const char* id, const char* title, Mask& mask, int count, Name name, float width,
    ComboSearch* search = nullptr, unsigned fallback = 0, const char* emptyLabel = "All", bool useEmptyLabel = false)
{
    char parts[PartsSize] = {}, raw[LabelSize], visible[LabelSize];
    for (int i = 0; i < count; ++i)
    {
        const unsigned bit = i < 32 ? 1u << i : 0;
        if (mask & bit)
        {
            AppendLabelPart(parts, sizeof(parts), name(i));
        }
    }
    if (!mask || (useEmptyLabel && !parts[0]))
    {
        snprintf(parts, sizeof(parts), "%s", emptyLabel);
    }
    snprintf(raw, sizeof(raw), "%s: %s", title, parts);
    EllipsizeComboLabel(visible, sizeof(visible), raw, width);
    if (ImCustom::BeginCombo(id, visible, width, search != nullptr))
    {
        if (search)
        {
            ImCustom::BeginComboList(search->Draw());
        }
        if (!fallback && ImCustom::ComboMultiItem("All", !mask))
        {
            mask = 0;
        }
        for (int i = 0; i < count; ++i)
        {
            if (search && !search->Matches(name(i)))
            {
                continue;
            }
            const unsigned bit = i < 32 ? 1u << i : 0;
            if (ImCustom::ComboMultiItem(name(i), (mask & bit) != 0))
            {
                mask = static_cast<Mask>(mask ^ bit);
                if (fallback && !mask)
                {
                    mask = static_cast<Mask>(fallback);
                }
            }
        }
        if (search)
        {
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetScrollY(0.0f);
            }
            ImCustom::EndComboList();
        }
        ImCustom::EndCombo();
    }
}

template <typename Value>
static void DrawVisibilityFilter(const char* id, const char* title, Value& value, float width)
{
    static const char* const names[] = { "Any", "Only", "Hide" };
    char label[64];
    snprintf(label, sizeof(label), "%s: %s", title, names[value]);
    if (ImCustom::BeginCombo(id, label, width))
    {
        for (int option = 0; option < 3; ++option)
        {
            if (ImCustom::ComboItem(names[option], value == option))
            {
                value = static_cast<Value>(option);
            }
        }
        ImCustom::EndCombo();
    }
}

static void NormalizeFloatLimits(float& lo, float& hi)
{
    if (!std::isfinite(lo))
    {
        lo = 0.0f;
    }
    if (!std::isfinite(hi))
    {
        hi = lo;
    }
    lo = ImMax(0.0f, lo);
    if (hi <= lo)
    {
        hi = lo + 1.0f;
    }
}

static void NormalizeIntLimits(int& lo, int& hi)
{
    lo = ImMax(0, lo);
    if (hi <= lo)
    {
        hi = lo + 1;
    }
}
