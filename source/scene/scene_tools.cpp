// Source Repository: https://github.com/waverso/waversomapstudio

#include "scene_tools.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

#include "../utils/transform_math.h"
#include "actions.h"
#include "../objects/model_constraints.h"
#include "scene.h"
#include "selection.h"

namespace
{
    constexpr float PositionTolerance = 0.001f;
    constexpr float RotationTolerance = 0.00001f;

    int ChildCount(const EditorScene& sc, int target)
    {
        int count = 0;
        for (int i = 0; i < sc.count; ++i)
        {
            count += sc.items[i].parent == target;
        }
        return count;
    }

    bool NearVector(const CVector& first, const CVector& second, float tolerance)
    {
        return std::fabs(first.x - second.x) <= tolerance && std::fabs(first.y - second.y) <= tolerance
            && std::fabs(first.z - second.z) <= tolerance;
    }

    size_t MaterialHash(const ItemData& data)
    {
        size_t hash = data.MaterialCount();
        const auto add = [&](size_t value)
        {
            hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        };
        const std::hash<std::string_view> textHash;
        add(data.MaterialTextCount());
        for (int i = 0; i < data.MaterialCount(); ++i)
        {
            const auto& material = data.materials[i];
            add(material.materialIndex);
            add(material.colorArgb);
            add(material.textureModelId);
            add(textHash(material.txdName));
            add(textHash(material.textureName));
        }
        for (int i = 0; i < data.MaterialTextCount(); ++i)
        {
            const auto& material = data.materialTexts[i];
            add(material.materialIndex);
            add(textHash(material.text));
            add(material.materialSize);
            add(textHash(material.fontFace));
            add(material.fontSize);
            add(material.bold);
            add(material.fontColorArgb);
            add(material.backgroundColorArgb);
            add(material.alignment);
        }
        return hash;
    }

    bool SameMaterials(const ItemData& first, const ItemData& second)
    {
        if (first.MaterialCount() != second.MaterialCount() || first.MaterialTextCount() != second.MaterialTextCount())
        {
            return false;
        }
        for (int i = 0; i < first.MaterialCount(); ++i)
        {
            const auto& a = first.materials[i];
            const auto& b = second.materials[i];
            if (a.materialIndex != b.materialIndex || a.colorArgb != b.colorArgb || a.textureModelId != b.textureModelId
                || std::strcmp(a.txdName, b.txdName) != 0 || std::strcmp(a.textureName, b.textureName) != 0)
            {
                return false;
            }
        }
        for (int i = 0; i < first.MaterialTextCount(); ++i)
        {
            const auto& a = first.materialTexts[i];
            const auto& b = second.materialTexts[i];
            if (a.materialIndex != b.materialIndex || std::strcmp(a.text, b.text) != 0 || a.materialSize != b.materialSize
                || std::strcmp(a.fontFace, b.fontFace) != 0 || a.fontSize != b.fontSize || a.bold != b.bold
                || a.fontColorArgb != b.fontColorArgb || a.backgroundColorArgb != b.backgroundColorArgb
                || a.alignment != b.alignment)
            {
                return false;
            }
        }
        const int textures = first.MaterialCount();
        const int count = textures + first.MaterialTextCount();
        const auto slot = [&](int entry)
        {
            return entry < textures ? first.materials[entry].materialIndex : first.materialTexts[entry - textures].materialIndex;
        };
        const auto sequence = [&](const ItemData& data, int entry)
        {
            return entry < textures ? data.materials[entry].sequence : data.materialTexts[entry - textures].sequence;
        };
        for (int i = 0; i < count; ++i)
        {
            for (int j = i + 1; j < count; ++j)
            {
                if (slot(i) == slot(j)
                    && (sequence(first, i) > sequence(first, j)) != (sequence(second, i) > sequence(second, j)))
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool HasContent(const ItemData& data)
    {
        return data.model.HasValue() || data.vehicle.HasValue() || data.pickup.HasValue() || data.actor.HasValue()
            || data.boxArea.HasValue() || data.removeBuilding.HasValue() || data.raceCheckpoint.HasValue();
    }
}

namespace SceneTools
{
    std::array<float, 6> CloneOffsetSettings::Sample(std::mt19937& random) const
    {
        if (!enabled)
        {
            return {};
        }
        if (mode == CloneOffsetMode::Fixed)
        {
            return fixed;
        }
        std::array<float, 6> offset {};
        for (size_t i = 0; i < offset.size(); ++i)
        {
            const auto& range = ranges[i];
            offset[i] = range[0] == range[1] ? range[0] : std::uniform_real_distribution<float>(range[0], range[1])(random);
        }
        return offset;
    }

    void ApplyCloneOffset(
        EditorScene& sc, int first, const std::array<float, 6>& offset, const CVector& pivot, const Utils::RotationBasis& basis)
    {
        if (first < 0 || first >= sc.count
            || std::all_of(
                offset.begin(), offset.end(),
                [](float value)
                {
                    return value == 0.0f;
                }))
        {
            return;
        }
        if (!std::all_of(offset.begin(), offset.end(),
                [](float value)
                {
                    return std::isfinite(value);
                }))
        {
            return;
        }
        sc.UpdateWorldTransforms();
        const CVector translation = Utils::TransformDirection(basis, CVector(offset[0], offset[1], offset[2]));
        const bool rotate = offset[3] != 0.0f || offset[4] != 0.0f || offset[5] != 0.0f;
        const auto rotation
            = Utils::ComposeRotationBases(Utils::ComposeRotationBases(basis, Utils::EulerDegreesToBasis(offset.data() + 3)),
                Utils::TransposeRotationBasis(basis));
        for (int index = first; index < sc.count; ++index)
        {
            if (sc.items[index].parent >= first)
            {
                continue;
            }
            const auto world = sc.world[index];
            if (!rotate)
            {
                const CVector local = sc.ComposesWithParent(index)
                    ? Utils::InverseTransformDirection(sc.world[sc.items[index].parent].rot, translation)
                    : translation;
                auto& position = sc.data[index].transform.pos;
                position[0] += local.x;
                position[1] += local.y;
                position[2] += local.z;
                continue;
            }
            float euler[3];
            Utils::BasisToEulerDegrees(Utils::ComposeRotationBases(rotation, world.rot), euler);
            auto positionRotation = rotation;
            const auto& data = sc.data[index];
            if (data.vehicle.HasValue() || data.actor.HasValue()
                || (data.model.HasValue() && !ModelConstraints::SupportsRotationXY(data.model.Get().modelId)))
            {
                const float yaw[3] = { 0.0f, 0.0f, euler[2] - world.eulerDeg[2] };
                positionRotation = Utils::EulerDegreesToBasis(yaw);
                if (data.model.HasValue())
                {
                    euler[0] = world.eulerDeg[0];
                    euler[1] = world.eulerDeg[1];
                }
            }
            const CVector position = pivot + translation + Utils::TransformDirection(positionRotation, world.pos - pivot);
            sc.SetWorldTransform(index, position, euler);
        }
        sc.UpdateWorldTransforms();
    }

    bool CanDetectDuplicates(const EditorScene& sc, DuplicateScope scope)
    {
        if (scope == DuplicateScope::Scene)
        {
            return ChildCount(sc, -1) > 2;
        }
        const int selected = sc.SelectionCount();
        return selected > 1 || (selected == 1 && ChildCount(sc, sc.SingleSelectedIndex()) > 0);
    }

    std::vector<int> FindDuplicates(EditorScene& sc, DuplicateScope scope)
    {
        std::vector<int> duplicates;
        if (!CanDetectDuplicates(sc, scope))
        {
            return duplicates;
        }
        sc.UpdateWorldTransforms();
        int ordered[EditorScene::MaxItems];
        int count = 0;
        sc.CollectDisplayOrder(-1, ordered, count);
        if (scope == DuplicateScope::Selection)
        {
            int selected = 0;
            for (int i = 0; i < count; ++i)
            {
                const int index = ordered[i];
                if (sc.items[index].selected || sc.HasSelectedAncestor(index))
                {
                    ordered[selected++] = index;
                }
            }
            count = selected;
        }
        struct ObjectPose
        {
            int index;
            Utils::RotationBasis rotation;
        };
        std::array<std::optional<size_t>, EditorScene::MaxItems> materialHashes;
        const auto hash = [&](int index)
        {
            auto& value = materialHashes[index];
            if (!value)
            {
                value = MaterialHash(sc.data[index]);
            }
            return *value;
        };
        const auto sameMaterials = [&](int first, int second)
        {
            if ((materialHashes[first] || materialHashes[second]) && hash(first) != hash(second))
            {
                return false;
            }
            if (SameMaterials(sc.data[first], sc.data[second]))
            {
                return true;
            }
            hash(first);
            hash(second);
            return false;
        };
        std::vector<ObjectPose> unique;
        unique.reserve(count);
        duplicates.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            const int index = ordered[i];
            const auto& data = sc.data[index];
            const bool pickup = data.pickup.HasValue();
            if (sc.items[index].type != SceneItemNormal || (!pickup && (!data.model.HasValue() || data.model.Get().modelId <= 0))
                || sc.HasChildren(index))
            {
                continue;
            }
            const auto rotation = pickup ? std::array<float, 3> {}
                                         : ModelConstraints::AppliedRotation(data.model.Get().modelId, sc.world[index].eulerDeg);
            const auto matrix = Utils::EulerDegreesToBasis(rotation.data());
            const auto match = std::find_if(unique.begin(), unique.end(),
                [&](const ObjectPose& pose)
                {
                    const auto& other = sc.data[pose.index];
                    return data.model == other.model
                        && NearVector(sc.world[index].pos, sc.world[pose.index].pos, PositionTolerance)
                        && data.pickup == other.pickup && NearVector(matrix.right, pose.rotation.right, RotationTolerance)
                        && NearVector(matrix.forward, pose.rotation.forward, RotationTolerance)
                        && NearVector(matrix.up, pose.rotation.up, RotationTolerance) && data.slide == other.slide
                        && sameMaterials(index, pose.index);
                });
            if (match != unique.end())
            {
                duplicates.push_back(index);
            }
            else
            {
                unique.push_back({ index, matrix });
            }
        }
        return duplicates;
    }

    bool CanCenterParent(const EditorScene& sc, int target)
    {
        return target >= 0 && target < sc.count && sc.SelectionCount() == 1 && sc.items[target].selected
            && sc.HasChildren(target);
    }

    bool CenterParent(int target, CenterParentMode mode)
    {
        if (!g_scene || !g_sceneActions || !CanCenterParent(*g_scene, target))
        {
            return false;
        }
        EditorScene& sc = *g_scene;
        sc.UpdateWorldTransforms();
        int ordered[EditorScene::MaxItems];
        int count = 0;
        sc.CollectDisplayOrder(target, ordered, count);
        const float limit = std::numeric_limits<float>::max();
        CVector minimum(limit, limit, limit);
        CVector maximum(-limit, -limit, -limit);
        if (mode == CenterParentMode::Bounds)
        {
            if (!Selection::ComputeChildrenBounds(sc, target, minimum, maximum))
            {
                return false;
            }
        }
        else
        {
            for (int i = 0; i < count; ++i)
            {
                const int index = ordered[i];
                if (!HasContent(sc.data[index]) && sc.HasChildren(index))
                {
                    continue;
                }
                const auto& position = sc.world[index].pos;
                minimum.x = (std::min)(minimum.x, position.x);
                minimum.y = (std::min)(minimum.y, position.y);
                minimum.z = (std::min)(minimum.z, position.z);
                maximum.x = (std::max)(maximum.x, position.x);
                maximum.y = (std::max)(maximum.y, position.y);
                maximum.z = (std::max)(maximum.z, position.z);
            }
        }
        const CVector center = minimum + (maximum - minimum) * 0.5f;
        const auto& world = sc.world[target];
        if (NearVector(center, world.pos, PositionTolerance))
        {
            return false;
        }
        const CVector childOffset = Utils::InverseTransformDirection(world.rot, world.pos - center);
        CVector localPosition = center;
        if (sc.ComposesWithParent(target))
        {
            const auto& parent = sc.world[sc.items[target].parent];
            localPosition = Utils::InverseTransformDirection(parent.rot, center - parent.pos);
        }
        int affected[EditorScene::MaxItems];
        int affectedCount = 1;
        affected[0] = target;
        for (int i = 0; i < count; ++i)
        {
            const int index = ordered[i];
            if (sc.items[index].parent == target && sc.ComposesWithParent(index))
            {
                affected[affectedCount++] = index;
            }
        }
        g_sceneActions->Prepare(sizeof(TransformEditAction) + affectedCount * sizeof(TransformEditAction::Entry));
        auto action = std::make_unique<TransformEditAction>(affected, affectedCount);
        auto& position = sc.data[target].transform.pos;
        position[0] = localPosition.x;
        position[1] = localPosition.y;
        position[2] = localPosition.z;
        for (int i = 1; i < affectedCount; ++i)
        {
            auto& child = sc.data[affected[i]].transform.pos;
            child[0] += childOffset.x;
            child[1] += childOffset.y;
            child[2] += childOffset.z;
        }
        const bool changed = action->CaptureAfter();
        if (changed)
        {
            g_sceneActions->Execute(std::move(action));
        }
        sc.UpdateWorldTransforms();
        return changed;
    }
}
