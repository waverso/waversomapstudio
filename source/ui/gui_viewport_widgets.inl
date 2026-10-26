// Source Repository: https://github.com/waverso/waversomapstudio

static bool UtilityIconButton(const char* id, ImTextureID icon, bool selected, float size, const char* tooltip,
    bool border = true, float iconSize = Theme::g_iconSize)
{
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    const bool tooltipHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);
    const bool active = ImGui::IsItemActive();
    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bg = Theme::g_headerBackground;
    if (active)
    {
        bg = Theme::g_activeControlBackground;
    }
    else if (hovered)
    {
        bg = Theme::g_hoveredHeaderBackground;
    }
    dl->AddRectFilled(mn, mx, bg);
    if (selected)
    {
        dl->AddRectFilled(mn, mx, hovered ? Theme::g_accentSoftHover : Theme::g_accentSoft);
    }
    if (border)
    {
        const float shadowOffset = 1.0f;
        dl->AddRect(ImVec2(mn.x + shadowOffset, mn.y + shadowOffset), ImVec2(mx.x + shadowOffset, mx.y + shadowOffset),
            Theme::g_borderShadow, 0.0f, 0, Ui::Hairline());
        dl->AddRect(mn, mx, hovered ? Theme::g_borderStrong : Theme::g_border, 0.0f, 0, Ui::Hairline());
    }

    DrawIconCentered(dl, icon, mn, mx, iconSize, selected ? Theme::g_text : Theme::g_textDim);
    if (tooltipHovered)
    {
        SetStyledTooltip(tooltip);
    }
    return clicked;
}

static void DrawUtilityValuePopup(const char* popupId, const char* title, const ImRect& panelRect, int& draftValue,
    int& appliedValue, int minValue, int maxValue, const char* format, bool& enabled)
{
    const UtilityPanelLayout& layout = g_utilityPanelLayout;
    ImGui::SetNextWindowPos(
        ImVec2(panelRect.Max.x, panelRect.Min.y - Theme::g_popupPadding), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(layout.popupWidth, 0.0f), ImVec2(layout.popupWidth, FLT_MAX));

    if (!ImCustom::BeginPopup(
            popupId, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        return;
    }

    const float contentX = ImGui::GetCursorPosX();
    const float contentWidth = ImGui::GetContentRegionAvail().x;
    const float titleWidth = ImGui::CalcTextSize(title).x;
    ImGui::SetCursorPosX(contentX + floorf((contentWidth - titleWidth) * 0.5f));
    ImGui::TextUnformatted(title);
    ImGui::SetCursorPosX(contentX);

    const char resetBtnText[] = "Disable";
    const float buttonWidth = ImGui::CalcTextSize(resetBtnText).x + Theme::g_padding * 2.0f;
    const float sliderWidth = contentWidth - Theme::g_popupPadding - buttonWidth;
    ImGui::SetNextItemWidth(sliderWidth);
    if (ImGui::DragInt("##UtilityValue", &draftValue, 0.1f, minValue, maxValue, format, ImGuiSliderFlags_AlwaysClamp))
    {
        appliedValue = draftValue;
        enabled = true;
    }
    LockMouseCursorForActiveDrag();

    ImGui::SameLine(0.0f, Theme::g_popupPadding);
    if (!enabled)
    {
        ImGui::BeginDisabled();
    }
    const bool reset = ImGui::Button(resetBtnText, ImVec2(buttonWidth, Theme::g_frameHeight));
    if (!enabled)
    {
        ImGui::EndDisabled();
    }

    if (reset)
    {
        enabled = false;
        ImGui::CloseCurrentPopup();
    }

    ImCustom::EndPopup();
}

enum class GizmoGridSnap
{
    Translation,
    Rotation
};

class GizmoGridSnapAction : public IAction
{
    public:
    GizmoGridSnapAction(GizmoGridSnap kind, float before, float after) : m_kind(kind), m_before(before), m_after(after)
    {
    }

    void Apply() override
    {
        SetValue(m_after);
    }
    void Undo() override
    {
        SetValue(m_before);
    }
    size_t MemoryUsage() const noexcept override
    {
        return sizeof(*this);
    }

    private:
    void SetValue(float value)
    {
        if (!g_gizmos)
        {
            return;
        }
        if (m_kind == GizmoGridSnap::Translation)
        {
            g_gizmos->SetTranslationGridSnap(value);
        }
        else
        {
            g_gizmos->SetRotationGridSnap(value);
        }
    }

    GizmoGridSnap m_kind;
    float m_before;
    float m_after;
};

struct GizmoGridEditState
{
    float before = 0.0f;
    bool active = false;
};

static bool GizmoGridDragFloat(const char* id, float* value, float speed, float v_min, float v_max, float width, ImTextureID icon,
    float iconSize, const char* tooltip, GizmoGridEditState& edit)
{
    const float valueBefore = *value;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(iconSize + Theme::g_padding * 2.0f, FrameTextPadY()));
    ImGui::SetNextItemWidth(width);
    bool changed = ImGui::DragFloat(id, value, speed, v_min, v_max, "", ImGuiSliderFlags_AlwaysClamp);
    LockMouseCursorForActiveDrag();
    ImGui::PopStyleVar();

    if (ImGui::IsItemActivated())
    {
        edit.before = valueBefore;
        edit.active = true;
    }

    if (!std::isfinite(*value))
    {
        *value = valueBefore;
    }
    *value = ImClamp(*value, v_min, v_max);
    changed |= *value != valueBefore;

    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    const bool tooltipHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);
    const bool isEditing = ImGui::IsItemActive() && ImGui::TempInputIsActive(ImGui::GetItemID());
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (!isEditing)
    {
        char valueBuf[32];
        SmartFormat(valueBuf, sizeof(valueBuf), *value);
        const ImVec2 valueSize = ImGui::CalcTextSize(valueBuf);
        const float iconRight = mn.x + Theme::g_padding + iconSize;
        const float valueX = ImMax(iconRight, (iconRight + mx.x - valueSize.x) * 0.5f);
        dl->AddText(ImVec2(valueX, mn.y + floorf((mx.y - mn.y - valueSize.y) * 0.5f)), Theme::g_text, valueBuf);
    }

    DrawIcon(dl, icon, ImVec2(mn.x + Theme::g_padding, mn.y + (mx.y - mn.y - iconSize) * 0.5f), iconSize, Theme::g_text);
    if (tooltipHovered)
    {
        SetStyledTooltip(tooltip);
    }
    return changed;
}

static void CommitGizmoGridEdit(GizmoGridSnap kind, GizmoGridEditState& edit, float value, bool deactivatedAfterEdit)
{
    if (!deactivatedAfterEdit)
    {
        return;
    }

    if (edit.active && edit.before != value && g_userActions)
    {
        g_userActions->Execute(std::make_unique<GizmoGridSnapAction>(kind, edit.before, value));
    }
    edit.active = false;
}
