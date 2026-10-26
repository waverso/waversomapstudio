// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "../scene/scene_data.h"

class EditorScene;
struct CVector;

namespace Layout
{
    struct Children
    {
        std::vector<int> indices;
        int modelId = -1;
        bool uniform = true;
        bool nested = false;
        Children() : indices(0)
        {
        }

        bool CanGenerate(bool copyFromFirst = false) const noexcept
        {
            return copyFromFirst ? !indices.empty() : uniform && !nested;
        }
    };

    Children Inspect(const EditorScene& scene, int parent);
    void Synchronize(EditorScene& scene);
    LayoutData EffectiveSettings(const LayoutData& data, const Children& children);
    float PathStep(const EditorScene& scene, const LayoutData& data);
    bool ResolveGeometry(const EditorScene& scene, const Children& children, LayoutData& data, std::string& error);
    bool ComputeBounds(const EditorScene& scene, int parent, CVector& minimum, CVector& maximum);
    void UpdateAutoApply(std::uint64_t time, bool editing);
    void ResetAutoApply(bool dirty = false, bool keepTiming = false);
    bool SettleAutoApply();
    void BuildPreview(const EditorScene& scene, int parent, std::vector<std::array<float, 6>>& lines);
}
