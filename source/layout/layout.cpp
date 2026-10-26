// Source Repository: https://github.com/waverso/waversomapstudio

#include "layout.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <numbers>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../utils/transform_math.h"
#include "../scene/actions.h"
#include "curves.h"
#include "../ui/gui_api.h"
#include "../objects/model_constraints.h"
#include "../objects/object_browser.h"
#include "../objects/object_runtime.h"
#include "../scene/scene.h"
#include "../scene/selection.h"
#include "../textures/texture_browser.h"

#include "layout_geometry.inl"
#include "layout_actions.inl"
#include "layout_auto_apply.inl"

namespace Layout
{
    namespace
    {
        bool IsModelPiece(const EditorScene& scene, int index)
        {
            const auto& data = scene.data[index];
            return scene.items[index].type == SceneItemNormal && data.model.HasValue() && !data.vehicle.HasValue()
                && !data.pickup.HasValue() && !data.actor.HasValue() && !data.boxArea.HasValue()
                && !data.removeBuilding.HasValue() && !data.layout.HasValue();
        }
    }

    Children Inspect(const EditorScene& scene, int parent)
    {
        Children result;
        for (const int index : scene.Children(parent))
        {
            if (scene.items[index].parent != parent)
            {
                continue;
            }
            result.indices.push_back(index);
            result.nested |= scene.HasChildren(index);
            if (!IsModelPiece(scene, index))
            {
                result.uniform = false;
            }
            else
            {
                const int model = scene.data[index].model.Get().modelId;
                if (result.indices.size() == 1)
                {
                    result.modelId = model;
                }
                else if (model != result.modelId)
                {
                    result.uniform = false;
                }
            }
        }
        return result;
    }

    void Synchronize(EditorScene& scene)
    {
        struct Summary
        {
            int count = 0;
            int model = -1;
            bool uniform = true;
        };
        std::array<Summary, EditorScene::MaxItems> children {};
        for (int index = 0; index < scene.count; ++index)
        {
            const int parent = scene.items[index].parent;
            if (parent < 0 || !scene.data[parent].layout.HasValue())
            {
                continue;
            }
            auto& summary = children[parent];
            ++summary.count;
            if (!IsModelPiece(scene, index))
            {
                summary.uniform = false;
            }
            else
            {
                const int model = scene.data[index].model.Get().modelId;
                if (summary.count == 1)
                {
                    summary.model = model;
                }
                else if (model != summary.model)
                {
                    summary.uniform = false;
                }
            }
        }
        for (int index = 0; index < scene.count; ++index)
        {
            if (!scene.data[index].layout.HasValue())
            {
                continue;
            }
            auto& settings = scene.data[index].layout.Edit();
            const auto& summary = children[index];
            if (!settings.piecesPending)
            {
                settings.pieces = summary.count;
            }
            if (!settings.modelPending && summary.uniform && summary.count > 0)
            {
                settings.modelId = summary.model;
            }
        }
    }

    LayoutData EffectiveSettings(const LayoutData& data, const Children& children)
    {
        auto result = data;
        if (!data.piecesPending || !children.CanGenerate(result.copyFromFirst))
        {
            result.pieces = static_cast<int>(children.indices.size());
        }
        if ((!data.modelPending || result.copyFromFirst || !children.CanGenerate()) && children.uniform && children.modelId >= 0)
        {
            result.modelId = children.modelId;
        }
        return result;
    }
}
