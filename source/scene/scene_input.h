// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include "../vendor/imgui/imgui_internal.h"

namespace SceneInput
{
    inline void ConsumeMouseClick(ImGuiMouseButton button)
    {
        ImGui::SetKeyOwner(ImGui::MouseButtonToKey(button), ImGuiKeyOwner_Any, ImGuiInputFlags_LockUntilRelease);
    }

    inline bool IsMouseClicked(ImGuiMouseButton button)
    {
        const ImGuiIO& io = ImGui::GetIO();
        return !io.MouseDownOwned[button] && !io.WantCaptureMouse && ImGui::IsMouseClicked(button, ImGuiKeyOwner_None);
    }
}
