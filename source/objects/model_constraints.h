// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <array>

namespace ModelConstraints
{
    bool SupportsRotationXY(int modelId);

    inline std::array<float, 3> AppliedRotation(int modelId, const float rotation[3])
    {
        const bool supportsXY = SupportsRotationXY(modelId);
        return { supportsXY ? rotation[0] : 0.0f, supportsXY ? rotation[1] : 0.0f, rotation[2] };
    }
}
