// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../vendor/imgui/imgui.h"

namespace Ui
{
    inline constexpr float NativeHeight = 1080.0f;
    inline constexpr float MinUserMultiplier = 0.75f;
    inline constexpr float MaxUserMultiplier = 1.50f;
    inline constexpr float DefaultUserMultiplier = 1.0f;

    struct State
    {
        float requestedUserMultiplier = DefaultUserMultiplier;
        float appliedUserMultiplier = DefaultUserMultiplier;
        float effectiveScale = 1.0f;
        ImVec2 displaySize = ImVec2(0.0f, 0.0f);
        bool displayAvailable = false;
        std::uint32_t generation = 1;
    };

    struct UpdateResult
    {
        bool displayAvailable = false;
        bool displayChanged = false;
        bool scaleChanged = false;
        float previousScale = 1.0f;
    };

    inline State g_state;

    inline float CalculateScale(float displayHeight, float userMultiplier)
    {
        return (displayHeight / NativeHeight) * userMultiplier;
    }

    inline UpdateResult BeginFrame(const ImVec2& displaySize)
    {
        UpdateResult result;
        result.previousScale = g_state.effectiveScale;
        result.displayAvailable = displaySize.x > 0.0f && displaySize.y > 0.0f;
        if (!result.displayAvailable)
        {
            result.displayChanged = g_state.displayAvailable;
            g_state.displayAvailable = false;
            if (result.displayChanged)
            {
                ++g_state.generation;
            }
            return result;
        }

        const float multiplier = std::clamp(g_state.requestedUserMultiplier, MinUserMultiplier, MaxUserMultiplier);
        const float effectiveScale = CalculateScale(displaySize.y, multiplier);
        result.displayChanged
            = !g_state.displayAvailable || displaySize.x != g_state.displaySize.x || displaySize.y != g_state.displaySize.y;
        result.scaleChanged = std::fabs(effectiveScale - g_state.effectiveScale) > 0.0001f;

        g_state.displaySize = displaySize;
        g_state.appliedUserMultiplier = multiplier;
        g_state.effectiveScale = effectiveScale;
        g_state.displayAvailable = true;
        if (result.displayChanged || result.scaleChanged)
        {
            ++g_state.generation;
        }
        return result;
    }

    inline void SetUserMultiplier(float multiplier)
    {
        g_state.requestedUserMultiplier = std::clamp(multiplier, MinUserMultiplier, MaxUserMultiplier);
    }

    inline float UserMultiplier()
    {
        return g_state.requestedUserMultiplier;
    }

    inline float AppliedUserMultiplier()
    {
        return g_state.appliedUserMultiplier;
    }

    inline float Scale()
    {
        return g_state.effectiveScale;
    }

    inline float Px(float nativePixels)
    {
        return nativePixels * Scale();
    }

    inline ImVec2 Px(const ImVec2& nativePixels)
    {
        return ImVec2(Px(nativePixels.x), Px(nativePixels.y));
    }

    inline float Round(float pixels)
    {
        return std::floor(pixels + 0.5f);
    }

    inline float RoundedPx(float nativePixels)
    {
        return Round(Px(nativePixels));
    }

    inline float Hairline(float nativePixels = 1.0f)
    {
        return std::max(1.0f, RoundedPx(nativePixels));
    }

    inline std::uint32_t Generation()
    {
        return g_state.generation;
    }
}
