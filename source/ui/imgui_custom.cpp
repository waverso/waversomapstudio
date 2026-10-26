// Source Repository: https://github.com/waverso/waversomapstudio

#include "imgui_custom.h"

#include "../vendor/imgui/imgui_internal.h"
#include "ui_scale.h"

namespace
{
    ImCustom::Style g_style = { 24.0f, 8.0f, 2.0f, 6.0f, 6.0f, 10, IM_COL32(32, 37, 44, 255), IM_COL32(41, 48, 57, 255),
        IM_COL32(47, 53, 60, 255), IM_COL32(70, 79, 91, 255), IM_COL32(0, 0, 0, 145), IM_COL32(230, 237, 243, 255),
        IM_COL32(139, 148, 158, 255), IM_COL32(41, 48, 57, 255), IM_COL32(25, 31, 39, 255) };

    void DrawComboContent(const char* label, const ImCustom::ComboImage* image, const ImRect& content, ImU32 color)
    {
        if (content.GetWidth() <= 0.0f || content.GetHeight() <= 0.0f)
        {
            return;
        }
        ImGuiContext& g = *GImGui;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (image && image->texture && image->size.x > 0.0f && image->size.y > 0.0f)
        {
            const float scale = ImMin(content.GetHeight() / image->size.y, content.GetWidth() / image->size.x);
            const ImVec2 size(image->size.x * scale, image->size.y * scale);
            const ImVec2 p(content.Min.x, content.Min.y + (content.GetHeight() - size.y) * 0.5f);
            dl->PushClipRect(content.Min, content.Max, true);
            dl->AddImage(image->texture, p, ImVec2(p.x + size.x, p.y + size.y), image->uvMin, image->uvMax, color);
            dl->PopClipRect();
        }
        else if (label && label[0])
        {
            const float y = content.Min.y + (content.GetHeight() - g.FontSize) * 0.5f;
            const ImVec4 clip(content.Min.x, content.Min.y, content.Max.x, content.Max.y);
            dl->AddText(g.Font, g.FontSize, ImVec2(content.Min.x, y), color, label, nullptr, 0.0f, &clip);
        }
    }

    bool BeginComboItem(const char* label, bool selected, ImRect& bb, ImGuiID& id)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
        {
            return false;
        }
        const float rightEdge = window->ScrollbarY ? window->InnerClipRect.Max.x : window->WorkRect.Max.x;
        const float w = ImMax(0.0f, rightEdge - window->DC.CursorPos.x);
        const ImVec2 p = window->DC.CursorPos;
        bb = ImRect(p, ImVec2(p.x + w, p.y + g_style.frameHeight));
        id = window->GetID(label);
        ImGui::ItemSize(bb, 0.0f);
        const bool added = ImGui::ItemAdd(bb, id);
        if (selected && ImGui::IsWindowAppearing())
        {
            ImGui::SetScrollHereY(0.5f);
        }
        return added;
    }
}

void ImCustom::SetStyle(const Style& style)
{
    g_style = style;
}

const ImCustom::Style& ImCustom::GetStyle()
{
    return g_style;
}

bool ImCustom::BeginCombo(
    const char* id, const char* preview, float width, bool hasSearch, const ComboImage* image, float popupWidth)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    const ImGuiNextWindowDataFlags backup = g.NextWindowData.Flags;
    g.NextWindowData.ClearFlags();
    if (window->SkipItems)
    {
        return false;
    }

    const ImGuiID box_id = window->GetID(id);
    const ImGuiID popup_id = ImHashStr("##ComboPopup", 0, box_id);

    const float w = (width > 0.0f) ? width : ImGui::CalcItemWidth();
    const ImVec2 p = window->DC.CursorPos;
    const ImRect bb(p, ImVec2(p.x + w, p.y + g_style.frameHeight));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, box_id, &bb))
    {
        return false;
    }

    bool hovered, held;
    const bool pressed = ImGui::ButtonBehavior(bb, box_id, &hovered, &held);
    bool popup_open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    if (pressed && !popup_open)
    {
        ImGui::OpenPopupEx(popup_id, ImGuiPopupFlags_None);
        popup_open = true;
    }

    const bool active = hovered || popup_open;
    ImDrawList* dl = window->DrawList;
    dl->AddRectFilled(bb.Min, bb.Max, active ? g_style.boxBgHovered : g_style.boxBg);
    const float shadowOffset = 1.0f;
    dl->AddRect(ImVec2(bb.Min.x + shadowOffset, bb.Min.y + shadowOffset),
        ImVec2(bb.Max.x + shadowOffset, bb.Max.y + shadowOffset), g_style.boxShadow, 0.0f, 0, Ui::Hairline());
    dl->AddRect(bb.Min, bb.Max, active ? g_style.boxBorderHovered : g_style.boxBorder, 0.0f, 0, Ui::Hairline());

    const float arrowZone = g_style.frameHeight;
    const float textRight = ImMax(bb.Min.x, bb.Max.x - arrowZone);
    DrawComboContent(
        preview, image, ImRect(ImVec2(bb.Min.x + g_style.fieldPadX, bb.Min.y), ImVec2(textRight, bb.Max.y)), g_style.text);
    {
        const float fs = g.FontSize;
        const ImVec2 ap(bb.Max.x - arrowZone + (arrowZone - fs) * 0.5f, bb.Min.y + (g_style.frameHeight - fs) * 0.5f);
        ImGui::RenderArrow(dl, ap, g_style.text, ImGuiDir_Down, 1.0f);
    }

    if (!popup_open)
    {
        return false;
    }

    g.NextWindowData.Flags = backup;

    const float popupW
        = popupWidth > 0.0f ? ImMin(ImMax(w, popupWidth), ImMax(1.0f, ImGui::GetPopupAllowedExtentRect(window).GetWidth())) : w;
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(popupW, 0.0f), ImVec2(popupW, g_style.frameHeight * (float)g_style.maxVisibleItems + g_style.popupPad * 2.0f));

    char name[16];
    ImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d", g.BeginPopupStack.Size);
    if (ImGuiWindow* pw = ImGui::FindWindowByName(name))
    {
        if (pw->WasActive)
        {
            const ImVec2 sizeExpected = ImGui::CalcWindowNextAutoFitSize(pw);
            pw->AutoPosLastDirection = ImGuiDir_Down;
            const ImRect rOuter = ImGui::GetPopupAllowedExtentRect(pw);
            const ImVec2 pos = ImGui::FindBestWindowPosForPopupEx(
                bb.GetBL(), sizeExpected, &pw->AutoPosLastDirection, rOuter, bb, ImGuiPopupPositionPolicy_ComboBox);
            ImGui::SetNextWindowPos(pos);
        }
    }

    ImGuiWindowFlags wf = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
    if (hasSearch)
    {
        wf |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::SetNextWindowScroll(ImVec2(0, 0));
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(g_style.popupPad, g_style.popupPad));
    const bool ret = ImGui::Begin(name, nullptr, wf);
    ImGui::PopStyleVar();

    if (!ret)
    {
        ImGui::EndPopup();
        return false;
    }

    if (hasSearch && ImGui::IsWindowAppearing())
    {
        ImGui::GetCurrentWindow()->HiddenFramesCannotSkipItems = 2;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    return true;
}

void ImCustom::EndCombo()
{
    ImGui::PopStyleVar();
    ImGui::EndPopup();
}

void ImCustom::BeginComboList(bool resetScroll)
{
    ImGuiWindow* popup = ImGui::GetCurrentWindow();
    const float headerHeight = popup->DC.CursorPos.y - popup->DC.CursorStartPos.y;
    const float availableHeight = ImGui::GetPopupAllowedExtentRect(popup).GetHeight() - popup->WindowPadding.y * 2 - headerHeight;
    const float maxHeight = ImMax(1.0f, ImMin(g_style.frameHeight * ImMax(1, g_style.maxVisibleItems - 1), availableHeight));
    const float minHeight = ImGui::IsWindowAppearing() || popup->HiddenFramesCannotSkipItems > 0
        ? ImMin(g_style.frameHeight, maxHeight)
        : ImClamp(popup->ContentSize.y - floorf(headerHeight), ImMin(g_style.frameHeight, maxHeight), maxHeight);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(0, minHeight), ImVec2(FLT_MAX, maxHeight),
        [](ImGuiSizeCallbackData* data)
        {
            if (ImGui::GetCurrentWindowRead()->ContentSize.y > data->DesiredSize.y)
            {
                const auto* parent = static_cast<const ImGuiWindow*>(data->UserData);
                data->DesiredSize.x += ImMax(0.0f, parent->WindowPadding.x - parent->WindowBorderSize);
            }
        },
        popup);
    if (resetScroll || ImGui::IsWindowAppearing())
    {
        ImGui::SetNextWindowScroll(ImVec2(0, 0));
    }
    ImGui::PushClipRect(popup->ClipRect.Min, ImVec2(popup->Rect().Max.x - popup->WindowBorderSize, popup->ClipRect.Max.y), false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##ComboList", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NavFlattened);
    ImGui::PopStyleVar();
}

void ImCustom::EndComboList()
{
    ImGui::EndChild();
    ImGui::PopClipRect();
}

bool ImCustom::ComboItem(const char* label, bool isCurrent, const ComboImage* image)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImRect bb;
    ImGuiID id;
    if (!BeginComboItem(label, isCurrent, bb, id))
    {
        return false;
    }

    bool clicked = false, hovered = false, held = false;
    if (!isCurrent)
    {
        if (ImGui::ButtonBehavior(bb, id, &hovered, &held))
        {
            clicked = true;
            ImGui::CloseCurrentPopup();
        }
    }

    ImDrawList* dl = window->DrawList;
    if (hovered)
    {
        dl->AddRectFilled(bb.Min, bb.Max, g_style.itemHovered);
    }
    else if (isCurrent)
    {
        dl->AddRectFilled(bb.Min, bb.Max, g_style.itemCurrentBg);
    }

    DrawComboContent(label, image,
        ImRect(ImVec2(bb.Min.x + g_style.itemPadX, bb.Min.y), ImVec2(bb.Max.x - g_style.itemPadX, bb.Max.y)),
        isCurrent ? g_style.textCurrent : g_style.text);

    return clicked;
}

bool ImCustom::ComboMultiItem(const char* label, bool isChecked)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImRect bb;
    ImGuiID id;
    if (!BeginComboItem(label, isChecked, bb, id))
    {
        return false;
    }

    bool hovered = false, held = false;
    const bool clicked = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = window->DrawList;
    if (hovered)
    {
        dl->AddRectFilled(bb.Min, bb.Max, g_style.itemHovered);
    }
    else if (isChecked)
    {
        dl->AddRectFilled(bb.Min, bb.Max, g_style.itemCurrentBg);
    }

    const float checkSize = ImMax(1.0f, g_style.frameHeight - g_style.itemPadX * 2.0f);
    const ImVec2 checkMin(bb.Min.x + g_style.itemPadX, bb.Min.y + (g_style.frameHeight - checkSize) * 0.5f);
    const ImRect checkBb(checkMin, ImVec2(checkMin.x + checkSize, checkMin.y + checkSize));
    if (isChecked)
    {
        dl->AddRectFilled(checkBb.Min, checkBb.Max, ImGui::GetColorU32(ImGuiCol_CheckMark, 0.45f));
    }
    dl->AddRect(checkBb.Min, checkBb.Max, g_style.boxBorder, 0.0f, 0, Ui::Hairline());
    if (isChecked)
    {
        ImGui::RenderCheckMark(dl, ImVec2(checkBb.Min.x + checkSize * 0.18f, checkBb.Min.y + checkSize * 0.18f),
            ImGui::GetColorU32(ImGuiCol_CheckMark), checkSize * 0.70f);
    }

    const float ty = bb.Min.y + (g_style.frameHeight - g.FontSize) * 0.5f;
    const float textX = checkBb.Max.x + g_style.itemPadX;
    const ImVec4 clip(textX, bb.Min.y, bb.Max.x, bb.Max.y);
    dl->AddText(
        g.Font, g.FontSize, ImVec2(textX, ty), isChecked ? g_style.textCurrent : g_style.text, label, nullptr, 0.0f, &clip);

    return clicked;
}

bool ImCustom::ToggleButton(const char* label, bool selected, const ImVec2& size)
{
    if (selected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, g_style.buttonSelected);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_style.buttonSelectedHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_style.buttonSelectedActive);
        ImGui::PushStyleColor(ImGuiCol_Text, g_style.buttonSelectedText);
    }
    const bool pressed = ImGui::Button(label, size);
    if (selected)
    {
        ImGui::PopStyleColor(4);
    }
    return pressed;
}

bool ImCustom::ChoiceRow(const char* id, const char* const* labels, int count, int& selected, float width)
{
    const ImVec2 position = ImGui::GetCursorScreenPos();
    float textWidth = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        textWidth += ImGui::CalcTextSize(labels[i]).x;
    }
    const float padding = ImMax(0.0f, (width - textWidth) / count);
    const float textPadY = ImFloor((g_style.frameHeight - ImGui::GetFontSize()) * 0.5f);
    bool changed = false;
    ImGui::PushID(id);
    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, textPadY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    for (int i = 0; i < count; ++i)
    {
        if (i)
        {
            ImGui::SameLine(0, 0);
        }
        const float buttonWidth
            = i + 1 == count ? position.x + width - ImGui::GetCursorScreenPos().x : ImGui::CalcTextSize(labels[i]).x + padding;
        if (ToggleButton(labels[i], selected == i, ImVec2(buttonWidth, g_style.frameHeight)) && selected != i)
        {
            selected = i;
            changed = true;
        }
    }
    ImGui::PopStyleVar(2);
    ImGui::EndGroup();
    ImGui::RenderFrameBorder(position, ImVec2(position.x + width, position.y + g_style.frameHeight));
    ImGui::PopID();
    return changed;
}

bool ImCustom::BeginPopup(const char* id, ImGuiWindowFlags flags)
{
    flags |= ImGuiWindowFlags_NoMove;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(g_style.popupPad * 2, g_style.popupPad * 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(g_style.popupSpacing, g_style.popupSpacing));
    if (ImGui::BeginPopup(id, flags))
    {
        return true;
    }
    ImGui::PopStyleVar(2);
    return false;
}

void ImCustom::EndPopup()
{
    ImGui::EndPopup();
    ImGui::PopStyleVar(2);
}
