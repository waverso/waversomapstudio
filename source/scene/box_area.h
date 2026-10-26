// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <limits>

#include "scene_data.h"

namespace BoxArea
{
    void DrawGangZones();

    inline bool IntersectRay(const BoxAreaData& area, const float center[3], const float origin[3], const float direction[3],
        float maxDistance, float& distance)
    {
        double entry = -std::numeric_limits<double>::infinity();
        double exit = std::numeric_limits<double>::infinity();
        double lengthSq = 0.0;
        for (int axis = 0; axis < 3; ++axis)
        {
            const double half = area.GetSize(axis) * 0.5;
            const double offset = static_cast<double>(origin[axis]) - center[axis];
            lengthSq += static_cast<double>(direction[axis]) * direction[axis];
            if (std::abs(direction[axis]) < 1e-7f)
            {
                if (offset < -half || offset > half)
                {
                    return false;
                }
                continue;
            }
            double axisEntry = (-half - offset) / direction[axis];
            double axisExit = (half - offset) / direction[axis];
            if (axisEntry > axisExit)
            {
                std::swap(axisEntry, axisExit);
            }
            entry = (std::max)(entry, axisEntry);
            exit = (std::min)(exit, axisExit);
            if (entry > exit)
            {
                return false;
            }
        }
        const double hit = entry >= 0.0 ? entry : exit;
        if (lengthSq < 1e-12 || !std::isfinite(hit) || hit < 0.0 || hit > maxDistance)
        {
            return false;
        }
        distance = static_cast<float>(hit);
        return true;
    }
}
