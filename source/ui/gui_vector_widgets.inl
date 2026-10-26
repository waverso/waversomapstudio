// Source Repository: https://github.com/waverso/waversomapstudio

static void DrawVec3Field(const char* idGroup, Vec3State& state, float* vec3, float innerW, bool compact = false,
    bool disabled = false, unsigned disabledAxes = 0, const char* disabledAxisTooltip = nullptr, bool normalizeRotation = false,
    float minValue = -20000.0f, float maxValue = 20000.0f, unsigned mixedAxes = 0, PropertyWidgetResult* result = nullptr)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float valueX = Theme::g_propertyLabelWidth;
    const float fieldW = innerW - valueX;
    const float fontSize = ImGui::GetFontSize();

    state.interacting = false;
    state.interactionFrame = ImGui::GetFrameCount();
    state.RebuildIfNeeded(vec3);
    if (mixedAxes && !state.hasFocus)
    {
        char values[3][48];
        for (int axis = 0; axis < 3; ++axis)
        {
            if (mixedAxes & (1u << axis))
            {
                snprintf(values[axis], sizeof(values[axis]), "%s", PropertyEdit::MixedText);
            }
            else
            {
                SmartFormat(values[axis], sizeof(values[axis]), vec3[axis]);
            }
        }
        snprintf(state.buf, sizeof(state.buf), "%s, %s, %s", values[0], values[1], values[2]);
        state.textSizeDirty = true;
    }

    if (disabled)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
            ImGui::GetStyle().Alpha
                * ((ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) ? 1.0f
                                                                                            : ImGui::GetStyle().DisabledAlpha));
        ImFont* roFont = ImGui::GetFont();
        {
            const ImVec2& textSz = state.GetTextSize();
            const float centerPadX = ImMax(Theme::g_padding, (fieldW - textSz.x) * 0.5f);
            ImGui::SetCursorPosX(valueX);
            SelectableField(idGroup, state.buf, fieldW, Theme::g_frameHeight, roFont, Theme::g_text, centerPadX, true, true);
        }

        if (compact)
        {
            ImGui::PopStyleVar();
            return;
        }

        const ImU32 axisColorsRO[3] = { Theme::g_axisX, Theme::g_axisY, Theme::g_axisZ };
        const char* axisLabelsRO[3] = { "X", "Y", "Z" };

        for (int i = 0; i < 3; i++)
        {
            const char* valStr = (mixedAxes & (1u << i)) ? PropertyEdit::MixedText : state.GetAxisText(i, vec3[i]);
            const float letterW = ImGui::CalcTextSize(axisLabelsRO[i]).x;
            const float valueW = ImGui::CalcTextSize(valStr).x;
            const float gapRO = Theme::g_padding * 0.5f;
            const float total = letterW + gapRO + valueW;
            const float blockLeft = ImMax(0.0f, (fieldW - total) * 0.5f);

            char roId[64];
            snprintf(roId, sizeof(roId), "##%s_ro%d", idGroup, i);
            ImGui::SetCursorPosX(valueX);
            SelectableField(
                roId, valStr, fieldW, Theme::g_frameHeight, roFont, Theme::g_text, blockLeft + letterW + gapRO, true, true);

            const ImVec2 mn = ImGui::GetItemRectMin();
            const ImVec2 mx = ImGui::GetItemRectMax();
            const float midY = (mn.y + mx.y) * 0.5f;
            dl->AddText(ImVec2(mn.x + blockLeft, midY - fontSize * 0.5f), ImGui::GetColorU32(axisColorsRO[i]), axisLabelsRO[i]);
        }

        ImGui::PopStyleVar();
        return;
    }

    {
        const ImVec2& textSz = state.GetTextSize();
        const float centerPadX = state.hasFocus ? Theme::g_padding : ImMax(Theme::g_padding, (fieldW - textSz.x) * 0.5f);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(centerPadX, FrameTextPadY()));
        ImGui::SetNextItemWidth(fieldW);
        ImGui::SetCursorPosX(valueX);
        ImGui::InputTextEx(idGroup, nullptr, state.buf, (int)sizeof(state.buf), ImVec2(fieldW, Theme::g_frameHeight),
            ImGuiInputTextFlags_AutoSelectAll);
        ImGui::PopStyleVar();
    }

    state.hasFocus = ImGui::IsItemActive();
    state.interacting = state.hasFocus;
    if (result)
    {
        result->active |= state.hasFocus;
    }

    auto constrainValue = [&](float value)
    {
        return normalizeRotation ? std::remainder(value, 360.0f) : ImClamp(value, minValue, maxValue);
    };

    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        float parsed[3];
        unsigned parts = 0;
        if (PropertyEdit::ParseVector(state.buf, parsed, parts))
        {
            parts &= ~disabledAxes;
            for (int axis = 0; axis < 3; ++axis)
            {
                if (parts & (1u << axis))
                {
                    vec3[axis] = constrainValue(parsed[axis]);
                }
            }
            if (result)
            {
                result->changed |= parts != 0;
                result->parts |= parts;
            }
        }
        else
        {
            g_toast.ShowError("Invalid numeric value.");
        }
        state.MarkDirty();
    }

    if (compact)
    {
        return;
    }

    const ImU32 axisColors[3] = { Theme::g_axisX, Theme::g_axisY, Theme::g_axisZ };
    const char* axisLabels[3] = { "X", "Y", "Z" };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, FrameTextPadY()));

    for (int i = 0; i < 3; i++)
    {
        ImGui::PushID(i);

        ImGui::SetCursorPosX(valueX);
        ImGui::SetNextItemWidth(fieldW);

        char dragId[64];
        snprintf(dragId, sizeof(dragId), "##%s_drag%d", idGroup, i);

        const bool axisDisabled = (disabledAxes & (1u << i)) != 0;
        ImGui::BeginDisabled(axisDisabled);
        const float valueBefore = vec3[i];
        ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, (mixedAxes & (1u << i)) != 0);
        const bool wasInput = ImGui::TempInputIsActive(ImGui::GetID(dragId));
        bool changed = ImGui::DragFloat(dragId, &vec3[i], 0.1f, normalizeRotation ? 0.0f : minValue,
            normalizeRotation ? 0.0f : maxValue, "", normalizeRotation ? 0 : ImGuiSliderFlags_AlwaysClamp);
        ImGui::PopItemFlag();
        changed = ValidateScalarInput(vec3[i], valueBefore, changed, ImGui::GetItemID(), wasInput);
        if (result)
        {
            result->Capture(changed, 1u << i);
        }
        if (changed)
        {
            vec3[i] = constrainValue(vec3[i]);
        }
        state.OnValuesChanged(vec3);
        LockMouseCursorForActiveDrag();
        state.interacting |= ImGui::IsItemActive();

        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        const float midY = (mn.y + mx.y) * 0.5f;

        const bool isEditing = ImGui::IsItemActive() && ImGui::TempInputIsActive(ImGui::GetItemID());

        if (!isEditing)
        {
            const ImVec2 axisLabelSz = ImGui::CalcTextSize(axisLabels[i]);
            const char* valStr = (mixedAxes & (1u << i)) && !changed ? PropertyEdit::MixedText : state.GetAxisText(i, vec3[i]);
            const ImVec2 valSz = ImGui::CalcTextSize(valStr);

            const float totalContentW = axisLabelSz.x + Theme::g_padding * 0.5f + valSz.x;
            const float startX = (mn.x + mx.x) * 0.5f - totalContentW * 0.5f;

            dl->AddText(ImVec2(startX, midY - fontSize * 0.5f), ImGui::GetColorU32(axisColors[i]), axisLabels[i]);
            dl->AddText(ImVec2(startX + axisLabelSz.x + Theme::g_padding * 0.5f, midY - fontSize * 0.5f),
                ImGui::GetColorU32(Theme::g_text), valStr);
        }

        if (axisDisabled && disabledAxisTooltip
            && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        {
            SetStyledTooltip(disabledAxisTooltip);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    ImGui::PopStyleVar();
}
