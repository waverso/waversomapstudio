// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cmath>

namespace PickupPosition
{
    inline constexpr float MinCoordinate = -4096.0f;
    inline constexpr float MaxCoordinate = 4095.875f;
    inline constexpr float MinGroundClearance = 0.2f;

    inline bool InRange(float value)
    {
        return std::isfinite(value) && value >= MinCoordinate && value <= MaxCoordinate;
    }

    inline float VisualScale(int modelId, double largestDimension)
    {
        if (modelId == 366)
        {
            return 1.2f;
        }
        if (!std::isfinite(largestDimension) || largestDimension <= 0.0f)
        {
            return 1.0f;
        }
        return static_cast<float>(1.0 + (std::max(1.0, static_cast<double>(1.2f) / largestDimension) - 1.0) * 0.6f);
    }

    inline bool SnapHeight(float proposedZ, float groundZ, float bottomOffset, float& result)
    {
        if (!std::isfinite(proposedZ) || !std::isfinite(groundZ) || !std::isfinite(bottomOffset))
        {
            return false;
        }
        const double minimumZ = static_cast<double>(groundZ) - bottomOffset + MinGroundClearance;
        const double targetZ = std::max({ minimumZ, static_cast<double>(proposedZ), -99.875 });
        float height = static_cast<float>(std::ceil(targetZ * 8.0) * 0.125);
        if (!InRange(height))
        {
            return false;
        }
        if ((height + bottomOffset) - groundZ < MinGroundClearance)
        {
            height += 0.125f;
        }
        if (!InRange(height))
        {
            return false;
        }
        result = height;
        return true;
    }

    inline bool ValidInput(const float position[3])
    {
        return InRange(position[0]) && InRange(position[1]) && std::isfinite(position[2])
            && (position[2] <= -100.0f || InRange(position[2]));
    }

    inline float Quantize(float value)
    {
        return static_cast<short>(value * 8.0f) * 0.125f;
    }

    template <class GroundHeight>
    bool Resolve(const float position[3], float result[3], GroundHeight groundHeight)
    {
        if (!ValidInput(position))
        {
            return false;
        }
        std::copy_n(position, 3, result);
        if (result[2] <= -100.0f)
        {
            result[2] = groundHeight(result[0], result[1]) + 0.5f;
        }
        if (!InRange(result[2]))
        {
            return false;
        }
        for (int axis = 0; axis < 3; ++axis)
        {
            result[axis] = Quantize(result[axis]);
        }
        return true;
    }
}
