// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include "../vendor/imgui/imgui.h"

template <class Include, class Draw>
void DrawClippedOutlinerRows(int count, float height, Include include, Draw draw)
{
    ImGuiListClipper clipper;
    clipper.Begin(count, height);
    for (int row = 0; row < count; ++row)
    {
        if (include(row))
        {
            clipper.IncludeItemByIndex(row);
        }
    }
    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
        {
            const auto position = ImGui::GetCursorScreenPos();
            draw(row);
            ImGui::SetCursorScreenPos(ImVec2(position.x, position.y + height));
        }
    }
}
