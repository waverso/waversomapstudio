// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <array>
#include <random>
#include <vector>

class EditorScene;
struct CVector;

namespace Utils
{
    struct RotationBasis;
}

namespace SceneTools
{
    enum class DuplicateScope
    {
        Scene,
        Selection
    };
    enum class CenterParentMode
    {
        Bounds,
        Pivot
    };
    enum class CloneOffsetMode
    {
        Fixed,
        RandomRange
    };
    enum class CloneOffsetSpace
    {
        Global,
        Local
    };

    struct CloneOffsetSettings
    {
        bool enabled = false;
        CloneOffsetMode mode = CloneOffsetMode::Fixed;
        CloneOffsetSpace space = CloneOffsetSpace::Global;
        std::array<float, 6> fixed {};
        std::array<std::array<float, 2>, 6> ranges {};

        std::array<float, 6> Sample(std::mt19937& random) const;
    };

    bool CanDetectDuplicates(const EditorScene& sc, DuplicateScope scope);
    std::vector<int> FindDuplicates(EditorScene& sc, DuplicateScope scope);
    bool CanCenterParent(const EditorScene& sc, int target);
    bool CenterParent(int target, CenterParentMode mode);
    void ApplyCloneOffset(
        EditorScene& sc, int first, const std::array<float, 6>& offset, const CVector& pivot, const Utils::RotationBasis& basis);
}
