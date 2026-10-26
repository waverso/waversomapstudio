// Source Repository: https://github.com/waverso/waversomapstudio

static void DrawCustomTabs(
    const char* id, const char* labels[], const bool visible[], int count, int* active, const bool enabled[] = nullptr)
{
    int visCount = 0;
    for (int i = 0; i < count; i++)
    {
        if (visible[i])
        {
            visCount++;
        }
    }
    if (visCount == 0)
    {
        return;
    }
    if (!visible[*active])
    {
        for (int i = 0; i < count; i++)
        {
            if (visible[i])
            {
                *active = i;
                break;
            }
        }
    }

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float totalW = ImGui::GetContentRegionAvail().x;
    const float tabW = totalW / (float)visCount;
    float x = cursor.x;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    int drawn = 0;
    for (int i = 0; i < count; i++)
    {
        if (!visible[i])
        {
            continue;
        }
        const float w = (drawn == visCount - 1) ? (totalW - tabW * drawn) : tabW;
        ImRect bb(ImVec2(x, cursor.y), ImVec2(x + w, cursor.y + Theme::g_tabHeight));

        char tidBuf[64];
        snprintf(tidBuf, sizeof(tidBuf), "%s_%d", id, i);
        ImGuiID tid = window->GetID(tidBuf);

        ImGui::BeginDisabled(enabled && !enabled[i]);
        ImGui::ItemSize(bb);
        ImGui::ItemAdd(bb, tid);

        const bool isActive = (*active == i);
        bool hovered, held;
        if (ImGui::ButtonBehavior(bb, tid, &hovered, &held))
        {
            *active = i;
        }

        dl->AddRectFilled(bb.Min, bb.Max, isActive ? Theme::g_panelBackground : Theme::g_alternatePanelBackground);
        if (isActive)
        {
            dl->AddRectFilled(bb.Min, ImVec2(bb.Max.x, bb.Min.y + Theme::g_accentSize), Theme::g_accent);
        }

        const ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        const ImU32 txtCol = isActive ? Theme::g_text : (hovered ? Theme::g_textHover : Theme::g_textDim);
        const ImVec2 tp(floorf(bb.Min.x + (bb.GetWidth() - ts.x) * 0.5f), floorf(bb.Min.y + (Theme::g_tabHeight - ts.y) * 0.5f));
        if (isActive)
        {
            dl->AddText(ImVec2(tp.x, tp.y + Ui::Hairline()), Theme::g_shadow, labels[i]);
        }
        dl->AddText(g_gui->m_fontBold, ImGui::GetFontSize(), tp, ImGui::GetColorU32(txtCol), labels[i]);
        ImGui::EndDisabled();

        x += w;
        drawn++;
    }
    ImGui::PopStyleVar();
    ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + Theme::g_tabHeight));
}

static bool Splitter(bool vertical, float& linePos, float minLine, float maxLine, float crossA, float crossB, bool blocked,
    bool& dragging, float& grabOffset, const float* drawLinePos = nullptr)
{
    if (blocked)
    {
        dragging = false;
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float grip = vertical ? Theme::g_verticalGrip : Theme::g_horizontalGrip;
    const float onAxis = vertical ? io.MousePos.x : io.MousePos.y;
    const float crsAx = vertical ? io.MousePos.y : io.MousePos.x;

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    ImGuiWindow* hoveredWin = ctx ? ctx->HoveredWindow : nullptr;
    bool isPopupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    bool blockedByOtherWin = hoveredWin && hoveredWin->Name && strcmp(hoveredWin->Name, "##RightSidebar") != 0;

    const bool inRange = fabsf(onAxis - linePos) < grip && crsAx >= crossA && crsAx <= crossB;
    const bool mouseDown = io.MouseDown[0];
    const bool mouseClicked = ImGui::IsMouseClicked(0);

    const bool canHover = inRange && !blockedByOtherWin && !isPopupOpen && (!mouseDown || mouseClicked || dragging);

    if (canHover && mouseClicked)
    {
        dragging = true;
        grabOffset = onAxis - linePos;
    }

    if (dragging)
    {
        if (ImGui::IsMouseDown(0))
        {
            linePos = ImClamp(onAxis - grabOffset, minLine, maxLine);
        }
        else
        {
            dragging = false;
            grabOffset = 0.0f;
        }
    }

    const bool active = canHover || dragging;
    if (active)
    {
        ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        const ImU32 col = dragging ? Theme::g_accentDrag : Theme::g_accentSoft;
        const float stroke = Ui::Hairline(2.0f);
        const float renderedLinePos = drawLinePos ? *drawLinePos : linePos;
        if (vertical)
        {
            fg->AddRectFilled(ImVec2(renderedLinePos, crossA), ImVec2(renderedLinePos + stroke, crossB), col);
        }
        else
        {
            fg->AddRectFilled(ImVec2(crossA, renderedLinePos - stroke), ImVec2(crossB, renderedLinePos), col);
        }
    }
    return active;
}

static void DrawLabel(ImDrawList* dl, const char* label)
{
    const float fontSize = ImGui::GetFontSize();
    const ImVec2 cur = ImGui::GetCursorScreenPos();
    dl->AddText(
        ImVec2(cur.x, cur.y + floorf((Theme::g_frameHeight - fontSize) * 0.5f)), ImGui::GetColorU32(Theme::g_textDim), label);
}

template <size_t N>
static void DrawToggleButtonRow(ImDrawList* draw, const char* label, const char* const (&labels)[N], bool* const (&values)[N],
    float fieldWidth, float gap, unsigned mixed = 0, PropertyWidgetResult* result = nullptr)
{
    const int count = static_cast<int>(N);
    const float availableWidth = ImMax(0.0f, fieldWidth - gap * (count - 1));
    float buttonWidths[N];
    float buttonsWidth = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        buttonWidths[i] = ImGui::CalcTextSize(labels[i]).x + Theme::g_padding * 2.0f;
        buttonsWidth += buttonWidths[i];
    }
    const float adjustment = (availableWidth - buttonsWidth) / count;
    const float padding = ImMax(0.0f, Theme::g_padding * 2.0f + adjustment);
    for (int i = 0; i < count; ++i)
    {
        buttonWidths[i] = ImGui::CalcTextSize(labels[i]).x + padding;
    }

    DrawLabel(draw, label);
    ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, FrameTextPadY()));
    for (int i = 0; i < count; ++i)
    {
        ImGui::PushID(i);
        const bool isMixed = (mixed & (1u << i)) != 0;
        const bool enabled = *values[i] && !isMixed;
        if (enabled)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, Theme::g_success);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::g_successHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::g_successActive);
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::g_statusText);
        }
        const bool changed = ImGui::Button(labels[i], ImVec2(buttonWidths[i], Theme::g_frameHeight));
        if (result)
        {
            result->Capture(changed, 1u << i);
        }
        if (changed)
        {
            *values[i] = isMixed || !enabled;
        }
        if (isMixed && !changed)
        {
            const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
            const bool active = ImGui::IsItemActive(), hovered = ImGui::IsItemHovered();
            const ImU32 checked = active ? Theme::g_successActive : hovered ? Theme::g_successHover : Theme::g_success;
            draw->AddRectFilled(minimum, maximum,
                ImGui::GetColorU32(active ? ImGuiCol_ButtonActive
                        : hovered         ? ImGuiCol_ButtonHovered
                                          : ImGuiCol_Button));
            draw->AddTriangleFilled(minimum, ImVec2(maximum.x, minimum.y), maximum,
                ImGui::GetColorU32(Theme::Alpha(checked, Theme::g_isLight ? 72 : 255)));
            draw->AddRect(minimum, maximum, ImGui::GetColorU32(ImGuiCol_Border), 0, 0, ImGui::GetStyle().FrameBorderSize);
            const auto text = ImGui::CalcTextSize(labels[i]);
            draw->AddText(ImVec2((minimum.x + maximum.x - text.x) * 0.5f, (minimum.y + maximum.y - text.y) * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text), labels[i]);
        }
        if (enabled)
        {
            ImGui::PopStyleColor(4);
        }
        ImGui::PopID();
        if (i + 1 < count)
        {
            ImGui::SameLine(0, gap);
        }
    }
    ImGui::PopStyleVar();
}
