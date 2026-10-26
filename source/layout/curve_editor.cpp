// Source Repository: https://github.com/waverso/waversomapstudio

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "../utils/transform_math.h"
#include "../scene/actions.h"
#include "curves.h"
#include "../scene/gizmos.h"
#include "../ui/gui_api.h"
#include "../scene/scene.h"

namespace Curves
{
    void ResetCache();
    void Prune(const EditorScene& scene);
    namespace
    {
        int g_handleNodeId = -1;
        int g_activeHandle = -1;
        int g_hoverNodeId = -1;
        int g_hoverHandle = -1;
        bool g_dragging = false;
        ItemData g_beforeDrag;
        GizmoMode g_previousMode = GizmoMode::Both;
        constexpr unsigned CurveColor = 0x469FA5u;
        constexpr unsigned CurveSelected = 0x5BDDE5u;
        constexpr unsigned CurveHover = 0xA4F1F5u;
        constexpr unsigned HandleColor = 0xFFB45Au;
        constexpr unsigned HandleHover = 0xFFD099u;

        bool Manual(const CurveNodeData& node)
        {
            return node.mode == CurveHandle::Aligned || node.mode == CurveHandle::Free;
        }

        template <typename Visitor>
        void ForEachMarker(const EditorScene& scene, const Visitor& visit)
        {
            for (int node = 0; node < scene.count; ++node)
            {
                const int parent = scene.items[node].parent;
                if (!scene.data[node].curveNode.HasValue() || parent < 0 || parent >= scene.count
                    || !scene.data[parent].curve.HasValue())
                {
                    continue;
                }
                visit(node, -1, scene.world[node].pos);
                if (!scene.items[node].selected || !Manual(scene.data[node].curveNode.Get())
                    || (g_gizmos && g_gizmos->GetGizmoTransformMode() == GizmoMode::None))
                {
                    continue;
                }
                CVector incoming, outgoing;
                if (Handles(scene, node, incoming, outgoing))
                {
                    visit(node, 0, incoming);
                    visit(node, 1, outgoing);
                }
            }
        }

        int MarkerHit(const CVector& origin, const CVector& direction, float& distance, int& handle)
        {
            if (!g_scene || !g_gizmos)
            {
                return -1;
            }
            int hit = -1;
            Gizmos::HandleBatch handles(g_gizmos);
            ForEachMarker(*g_scene,
                [&](int node, int part, const CVector& position)
                {
                    if (handles.HitTest(position, origin, direction, distance))
                    {
                        hit = node;
                        handle = part;
                    }
                });
            return hit;
        }

        float RaySegment(const CVector& origin, const CVector& direction, const CVector& a, const CVector& b, float& distance)
        {
            const CVector segment = b - a, delta = origin - a;
            const float length = Utils::Dot3(segment, segment);
            const float projected = Utils::Dot3(direction, segment);
            const float offset = Utils::Dot3(direction, delta);
            const float along = Utils::Dot3(segment, delta);
            const float denominator = length - projected * projected;
            float ray = denominator > 1e-8f ? (projected * along - length * offset) / denominator : 0;
            float t = length > 1e-8f ? std::clamp((along + projected * ray) / length, 0.0f, 1.0f) : 0;
            ray = (std::max)(0.0f, projected * t - offset);
            t = length > 1e-8f ? std::clamp((along + projected * ray) / length, 0.0f, 1.0f) : 0;
            distance = ray;
            return (origin + direction * ray - a - segment * t).Magnitude();
        }
    }

    void SetHandle(int nodeId, int handle)
    {
        if (g_gizmos && g_activeHandle >= 0 && handle < 0)
        {
            g_gizmos->SetGizmoTransformMode(g_previousMode);
        }
        if (g_gizmos && g_activeHandle < 0 && handle >= 0)
        {
            g_previousMode = g_gizmos->GetGizmoTransformMode();
            g_gizmos->SetGizmoTransformMode(GizmoMode::Position);
        }
        g_handleNodeId = handle >= 0 ? nodeId : -1;
        g_activeHandle = handle;
    }

    bool GetHandlePosition(CVector& position)
    {
        if (!g_scene || g_activeHandle < 0)
        {
            return false;
        }
        const int node = g_scene->FindIndexById(g_handleNodeId);
        if (node < 0 || g_scene->SingleSelectedIndex() != node || !g_scene->data[node].curveNode.HasValue()
            || !Manual(g_scene->data[node].curveNode.Get()))
        {
            return false;
        }
        CVector incoming, outgoing;
        if (!Handles(*g_scene, node, incoming, outgoing))
        {
            return false;
        }
        position = g_activeHandle == 0 ? incoming : outgoing;
        return true;
    }

    void MoveHandleTo(const CVector& position)
    {
        CVector current;
        if (!g_sceneActions || !GetHandlePosition(current))
        {
            return;
        }
        const CVector delta = position - current;
        if (Utils::Dot3(delta, delta) <= 0.000001f)
        {
            return;
        }
        const int node = g_scene->FindIndexById(g_handleNodeId);
        try
        {
            g_sceneActions->Prepare(
                sizeof(ItemDataEditAction) + sizeof(ItemDataEditAction::Entry) + 2 * g_scene->data[node].DynamicMemoryUsage());
            std::vector<ItemDataEditAction::Entry> changes(1);
            auto& change = changes.front();
            change.itemId = g_handleNodeId;
            change.before = change.after = g_scene->data[node];
            const auto& world = g_scene->world[node];
            const CVector local = Utils::TransformDirection(Utils::TransposeRotationBasis(world.rot), position - world.pos);
            auto& data = change.after.curveNode.Edit();
            float* handle = g_activeHandle == 0 ? data.incoming : data.outgoing;
            handle[0] = std::clamp(local.x, -20000.0f, 20000.0f);
            handle[1] = std::clamp(local.y, -20000.0f, 20000.0f);
            handle[2] = std::clamp(local.z, -20000.0f, 20000.0f);
            data.Align(g_activeHandle == 1);
            if (change.before != change.after)
            {
                g_sceneActions->Execute(std::make_unique<ItemDataEditAction>(std::move(changes)));
            }
            UpdateHandleGizmoBinding();
        }
        catch (const std::bad_alloc&)
        {
            GuiShowToast("Not enough memory to move the curve handle.", true);
        }
    }

    void AlignHandleTo(const CVector& normal)
    {
        CVector position;
        const float length = normal.Magnitude();
        if (length < 1e-8f || !GetHandlePosition(position))
        {
            return;
        }
        const int node = g_scene->FindIndexById(g_handleNodeId);
        const CVector anchor = g_scene->world[node].pos;
        MoveHandleTo(anchor + normal * ((position - anchor).Magnitude() / length));
    }

    bool UpdateHandleGizmoBinding()
    {
        if (!g_scene || !g_gizmos)
        {
            return false;
        }
        const int node = g_scene->FindIndexById(g_handleNodeId);
        if (g_dragging && !g_gizmos->IsDragging())
        {
            g_dragging = false;
            if (node >= 0 && g_sceneActions && g_beforeDrag != g_scene->data[node])
            {
                std::vector<ItemDataEditAction::Entry> changes(1);
                changes[0].itemId = g_handleNodeId;
                changes[0].before = std::move(g_beforeDrag);
                changes[0].after = g_scene->data[node];
                g_sceneActions->Execute(std::make_unique<ItemDataEditAction>(std::move(changes)));
            }
            g_beforeDrag = {};
        }
        if (g_activeHandle < 0 || node < 0 || g_scene->SingleSelectedIndex() != node || !g_scene->data[node].curveNode.HasValue()
            || !Manual(g_scene->data[node].curveNode.Get()))
        {
            SetHandle(-1, -1);
            return false;
        }
        g_gizmos->SetGizmoVisible(true);
        g_gizmos->SetGizmoEnabled(g_sceneActions != nullptr);
        g_gizmos->SetRotationEnabled(false);
        g_gizmos->SetRadiusGizmo(false, {}, 0);
        g_gizmos->SetBoxAreaGizmo(false, {}, {});
        if (g_gizmos->IsDragging())
        {
            if (!g_dragging)
            {
                g_beforeDrag = g_scene->data[node];
                g_dragging = true;
            }
            const auto& world = g_scene->world[node];
            const CVector position
                = Utils::TransformDirection(Utils::TransposeRotationBasis(world.rot), g_gizmos->GetGizmoPosition() - world.pos);
            auto& data = g_scene->data[node].curveNode.Edit();
            float* handle = g_activeHandle == 0 ? data.incoming : data.outgoing;
            handle[0] = std::clamp(position.x, -20000.0f, 20000.0f);
            handle[1] = std::clamp(position.y, -20000.0f, 20000.0f);
            handle[2] = std::clamp(position.z, -20000.0f, 20000.0f);
            data.Align(g_activeHandle == 1);
        }
        else
        {
            CVector incoming, outgoing;
            if (Handles(*g_scene, node, incoming, outgoing))
            {
                g_gizmos->SetGizmoPosition(g_activeHandle == 0 ? incoming : outgoing);
            }
            g_gizmos->SetGizmoRotation(g_scene->world[node].rot);
        }
        return true;
    }

    void ClearHover()
    {
        g_hoverNodeId = g_hoverHandle = -1;
    }

    bool UpdateHover(const CVector& origin, const CVector& direction, bool selectedOnly)
    {
        ClearHover();
        float distance = (std::numeric_limits<float>::max)();
        int handle = -1;
        const int node = MarkerHit(origin, direction, distance, handle);
        if (node < 0 || (selectedOnly && !g_scene->items[node].selected))
        {
            return false;
        }
        g_hoverNodeId = g_scene->items[node].id;
        g_hoverHandle = handle;
        if (g_scene->SingleSelectedIndex() != node || g_scene->SelectionCount() != 1)
        {
            return true;
        }
        return handle >= 0 ? g_hoverNodeId != g_handleNodeId || handle != g_activeHandle : g_activeHandle >= 0;
    }

    int Pick(const CVector& origin, const CVector& direction, float& maximum)
    {
        if (!g_scene || !g_gizmos)
        {
            return -1;
        }
        const float length = direction.Magnitude();
        if (length < 1e-8f)
        {
            return -1;
        }
        const CVector ray = direction * (1.0f / length);
        float controlDistance = (std::numeric_limits<float>::max)();
        int controlHandle = -1;
        const int control = MarkerHit(origin, ray, controlDistance, controlHandle);
        if (control >= 0)
        {
            maximum = controlDistance;
            SetHandle(controlHandle >= 0 ? g_scene->items[control].id : -1, controlHandle);
            return control;
        }
        int hit = -1;
        for (int curve = 0; curve < g_scene->count; ++curve)
        {
            if (!g_scene->data[curve].curve.HasValue())
            {
                continue;
            }
            std::string error;
            const auto* path = Resolve(*g_scene, curve, error);
            if (!path)
            {
                continue;
            }
            const auto& samples = path->Samples();
            for (size_t sample = 1; sample < samples.size(); ++sample)
            {
                const auto& a = samples[sample - 1].position;
                const auto& b = samples[sample].position;
                float distance;
                const float separation
                    = RaySegment(origin, ray, { static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z) },
                        { static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.z) }, distance);
                if (distance > 0 && distance <= maximum && separation <= (std::max)(0.1f, distance * 0.002f))
                {
                    maximum = distance;
                    hit = curve;
                }
            }
        }
        if (hit >= 0)
        {
            SetHandle(-1, -1);
        }
        return hit;
    }

    void BuildDisplay(const EditorScene& scene, std::vector<std::array<float, 7>>& lines, std::vector<Marker>& markers)
    {
        Prune(scene);
        std::array<int, EditorScene::MaxItems> curves;
        int curveCount = 0;
        for (int index = 0; index < scene.count; ++index)
        {
            if (scene.data[index].curve.HasValue())
            {
                curves[curveCount++] = index;
            }
        }
        if (curveCount == 0)
        {
            return;
        }
        std::array<bool, EditorScene::MaxItems> selected {};
        for (int index = 0; index < scene.count; ++index)
        {
            if (!scene.items[index].selected)
            {
                continue;
            }
            selected[index] = true;
            const int parent = scene.items[index].parent;
            if (parent >= 0 && parent < scene.count)
            {
                selected[parent] = true;
            }
            const auto& layout = scene.data[index].layout;
            if (layout.HasValue() && layout.Get().type == LayoutType::Curve)
            {
                const int curve = scene.FindIndexById(layout.Get().curveId);
                if (curve >= 0)
                {
                    selected[curve] = true;
                }
            }
        }
        const auto add = [&](const CVector& a, const CVector& b, unsigned color)
        {
            lines.push_back({ a.x, a.y, a.z, b.x, b.y, b.z, static_cast<float>(color) });
        };
        for (int entry = 0; entry < curveCount; ++entry)
        {
            const int curve = curves[entry];
            std::string error;
            const auto* path = Resolve(scene, curve, error);
            if (path)
            {
                const auto& samples = path->Samples();
                for (size_t i = 1; i < samples.size(); ++i)
                {
                    const auto& a = samples[i - 1].position;
                    const auto& b = samples[i].position;
                    add({ static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z) },
                        { static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.z) },
                        selected[curve] ? CurveSelected : CurveColor);
                }
            }
        }
        Gizmos::HandleBatch handles(g_gizmos);
        ForEachMarker(scene,
            [&](int node, int part, const CVector& position)
            {
                const bool hovered = scene.items[node].id == g_hoverNodeId && g_hoverHandle == part;
                unsigned color;
                if (part < 0)
                {
                    color = hovered ? CurveHover : scene.items[node].selected ? CurveSelected : CurveColor;
                }
                else
                {
                    add(position, scene.world[node].pos, HandleColor);
                    const bool active = scene.items[node].id == g_handleNodeId && g_activeHandle == part;
                    color = hovered || active ? HandleHover : HandleColor;
                }
                markers.push_back({ { position.x, position.y, position.z }, handles.Radius(position), color });
            });
    }

    void Reset()
    {
        SetHandle(-1, -1);
        ClearHover();
        g_dragging = false;
        g_beforeDrag = {};
        ResetCache();
    }
}
