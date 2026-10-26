// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include "../vendor/imgui/imgui.h"

namespace ImCustom
{
    struct ComboImage
    {
        ImTextureID texture = nullptr;
        ImVec2 uvMin = ImVec2(0.0f, 0.0f);
        ImVec2 uvMax = ImVec2(1.0f, 1.0f);
        ImVec2 size = ImVec2(0.0f, 0.0f);
    };

    struct Style
    {
        float frameHeight;
        float fieldPadX;
        float itemPadX;
        float popupPad;
        float popupSpacing;
        int maxVisibleItems;

        ImU32 boxBg, boxBgHovered, boxBorder, boxBorderHovered, boxShadow;
        ImU32 text, textCurrent;
        ImU32 itemHovered, itemCurrentBg;
        ImU32 buttonSelected = IM_COL32(22, 68, 38, 255);
        ImU32 buttonSelectedHovered = IM_COL32(28, 85, 48, 255);
        ImU32 buttonSelectedActive = IM_COL32(34, 102, 57, 255);
        ImU32 buttonSelectedText = IM_COL32(230, 237, 243, 255);
    };

    void SetStyle(const Style& style);
    const Style& GetStyle();

    bool BeginCombo(const char* id, const char* preview, float width = 0.0f, bool hasSearch = false,
        const ComboImage* image = nullptr, float popupWidth = 0.0f);
    void EndCombo();
    void BeginComboList(bool resetScroll = false);
    void EndComboList();
    bool ComboItem(const char* label, bool isCurrent, const ComboImage* image = nullptr);
    bool ComboMultiItem(const char* label, bool isChecked);

    bool ToggleButton(const char* label, bool selected, const ImVec2& size);
    bool ChoiceRow(const char* id, const char* const* labels, int count, int& selected, float width);

    bool BeginPopup(const char* id, ImGuiWindowFlags flags = 0);
    void EndPopup();
}
