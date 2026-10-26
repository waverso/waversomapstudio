// Source Repository: https://github.com/waverso/waversomapstudio

#include "curves.h"

#include <algorithm>
#include <bit>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <new>
#include <numbers>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../utils/transform_math.h"
#include "../scene/actions.h"
#include "../ui/gui_api.h"
#include "../scene/scene.h"

namespace Curves
{
    namespace
    {
        struct Cache
        {
            std::uint64_t signature = 0;
            CurveGeometry::Path path;
            std::string error;
        };
        std::unordered_map<int, Cache> g_caches;

        class SubdivideAction final : public IAction
        {
            public:
            struct Entry
            {
                int id, beforeOrder, afterOrder;
                CurveNodeData before, after;
            };
            SubdivideAction(std::vector<Entry>&& changes, std::vector<ItemSnapshot>&& items)
                : entries(0), created(std::move(items))
            {
                entries.swap(changes);
            }
            void Apply() override
            {
                created.Apply();
                Set(true);
            }
            void Undo() override
            {
                created.Undo();
                Set(false);
            }
            size_t MemoryUsage() const noexcept override
            {
                return sizeof(*this) + entries.capacity() * sizeof(Entry) + created.MemoryUsage();
            }

            private:
            void Set(bool after)
            {
                if (!g_scene)
                {
                    return;
                }
                for (const auto& entry : entries)
                {
                    const int index = g_scene->FindIndexById(entry.id);
                    if (index < 0)
                    {
                        continue;
                    }
                    g_scene->items[index].order = after ? entry.afterOrder : entry.beforeOrder;
                    g_scene->data[index].curveNode.Edit() = after ? entry.after : entry.before;
                }
                g_scene->InvalidateHierarchy();
                g_scene->UpdateWorldTransforms();
            }
            std::vector<Entry> entries;
            CreateItemsAction created;
        };

        CurveGeometry::Vec Vector(const CVector& value)
        {
            return { value.x, value.y, value.z };
        }
        CVector Vector(CurveGeometry::Vec value)
        {
            return { static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z) };
        }

        std::vector<int> Nodes(const EditorScene& scene, int curve)
        {
            std::vector<int> result(0);
            for (const int index : scene.Children(curve))
            {
                if (scene.items[index].parent == curve && scene.data[index].curveNode.HasValue())
                {
                    result.push_back(index);
                }
            }
            return result;
        }

        CurveGeometry::Knot Knot(const EditorScene& scene, int node, int previous, int next, CurveHandle mode)
        {
            const auto& data = scene.data[node].curveNode.Get();
            const auto& world = scene.world[node];
            CurveGeometry::Knot knot;
            knot.position = Vector(world.pos);
            knot.tilt = data.tilt * std::numbers::pi / 180.0;
            if (mode == CurveHandle::Auto || mode == CurveHandle::Vector)
            {
                const auto before = Vector(scene.world[previous].pos);
                const auto after = Vector(scene.world[next].pos);
                if (mode == CurveHandle::Vector)
                {
                    knot.incoming = knot.position + (before - knot.position) * (1.0 / 3.0);
                    knot.outgoing = knot.position + (after - knot.position) * (1.0 / 3.0);
                }
                else
                {
                    const auto direction = (after - before).Unit();
                    knot.incoming = knot.position - direction * ((knot.position - before).Length() / 3.0);
                    knot.outgoing = knot.position + direction * ((after - knot.position).Length() / 3.0);
                }
            }
            else
            {
                knot.incoming = knot.position
                    + Vector(Utils::TransformDirection(world.rot, { data.incoming[0], data.incoming[1], data.incoming[2] }));
                knot.outgoing = knot.position
                    + Vector(Utils::TransformDirection(world.rot, { data.outgoing[0], data.outgoing[1], data.outgoing[2] }));
            }
            return knot;
        }

        Utils::RotationBasis NodeRotation(const EditorScene& scene, int curve, const CurveGeometry::Knot& knot)
        {
            const auto tangent = (knot.outgoing - knot.position).Unit((knot.position - knot.incoming).Unit());
            const auto normal = CurveGeometry::Normal(tangent, Vector(scene.world[curve].rot.up));
            return { Vector(tangent), Vector(normal.Cross(tangent)), Vector(normal) };
        }

        std::vector<CurveGeometry::Knot> Knots(const EditorScene& scene, int curve, const std::vector<int>& nodes)
        {
            std::vector<CurveGeometry::Knot> result(nodes.size());
            const bool closed = scene.data[curve].curve.Get().closed;
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                const size_t previous = i > 0 ? i - 1 : closed ? nodes.size() - 1 : i;
                const size_t next = i + 1 < nodes.size() ? i + 1 : closed ? 0 : i;
                result[i] = Knot(scene, nodes[i], nodes[previous], nodes[next], scene.data[nodes[i]].curveNode.Get().mode);
            }
            return result;
        }
    }

    bool CanUse(const EditorScene& scene, int layout, int curve)
    {
        return layout >= 0 && layout < scene.count && curve >= 0 && curve < scene.count && scene.data[curve].curve.HasValue()
            && curve != layout && !scene.IsDescendantOf(curve, layout);
    }

    std::uint64_t Signature(const EditorScene& scene, int curve)
    {
        if (curve < 0 || curve >= scene.count || !scene.data[curve].curve.HasValue())
        {
            return 0;
        }
        std::uint64_t hash = 14695981039346656037ull;
        const auto add = [&](std::uint32_t value)
        {
            hash ^= value;
            hash *= 1099511628211ull;
        };
        const auto vector = [&](const CVector& value)
        {
            add(std::bit_cast<std::uint32_t>(value.x));
            add(std::bit_cast<std::uint32_t>(value.y));
            add(std::bit_cast<std::uint32_t>(value.z));
        };
        add(scene.items[curve].id);
        add(scene.data[curve].curve.Get().closed);
        vector(scene.world[curve].rot.up);
        for (const int node : scene.Children(curve))
        {
            if (scene.items[node].parent != curve || !scene.data[node].curveNode.HasValue())
            {
                continue;
            }
            const auto& data = scene.data[node].curveNode.Get();
            const auto& world = scene.world[node];
            add(scene.items[node].id);
            vector(world.pos);
            vector(world.rot.right);
            vector(world.rot.forward);
            vector(world.rot.up);
            add(static_cast<std::uint32_t>(data.mode));
            add(std::bit_cast<std::uint32_t>(data.tilt));
            for (const float value : data.incoming)
            {
                add(std::bit_cast<std::uint32_t>(value));
            }
            for (const float value : data.outgoing)
            {
                add(std::bit_cast<std::uint32_t>(value));
            }
        }
        return hash;
    }

    const CurveGeometry::Path* Resolve(const EditorScene& scene, int curve, std::string& error)
    {
        error.clear();
        if (curve < 0 || curve >= scene.count || !scene.data[curve].curve.HasValue())
        {
            error = "Choose an existing curve.";
            return nullptr;
        }
        auto& cache = g_caches[scene.items[curve].id];
        const auto signature = Signature(scene, curve);
        if (cache.signature != signature)
        {
            cache.error.clear();
            const auto nodes = Nodes(scene, curve);
            if (nodes.size() < 2)
            {
                cache.error = "A curve requires at least two nodes.";
            }
            else if (!cache.path.Build(
                         Knots(scene, curve, nodes), scene.data[curve].curve.Get().closed, Vector(scene.world[curve].rot.up)))
            {
                cache.error = "The curve is empty or exceeds the curve detail limit.";
            }
            cache.signature = signature;
        }
        error = cache.error;
        return error.empty() ? &cache.path : nullptr;
    }

    bool Handles(const EditorScene& scene, int node, CVector& incoming, CVector& outgoing)
    {
        if (node < 0 || node >= scene.count || !scene.data[node].curveNode.HasValue())
        {
            return false;
        }
        const int curve = scene.items[node].parent;
        if (curve < 0 || curve >= scene.count || !scene.data[curve].curve.HasValue())
        {
            return false;
        }
        const auto& handles = scene.data[node].curveNode.Get();
        if (handles.mode == CurveHandle::Aligned || handles.mode == CurveHandle::Free)
        {
            const auto& world = scene.world[node];
            incoming = world.pos
                + Utils::TransformDirection(world.rot, CVector(handles.incoming[0], handles.incoming[1], handles.incoming[2]));
            outgoing = world.pos
                + Utils::TransformDirection(world.rot, CVector(handles.outgoing[0], handles.outgoing[1], handles.outgoing[2]));
            return true;
        }
        const auto nodes = scene.Children(curve);
        const auto found = std::find(nodes.begin(), nodes.end(), node);
        if (found == nodes.end())
        {
            return false;
        }
        const auto isNode = [&](int index)
        {
            return scene.data[index].curveNode.HasValue();
        };
        const auto before = std::find_if(std::make_reverse_iterator(found), nodes.rend(), isNode);
        const auto after = std::find_if(found + 1, nodes.end(), isNode);
        const bool closed = scene.data[curve].curve.Get().closed;
        const int previous = before != nodes.rend() ? *before
            : closed                                ? *std::find_if(nodes.rbegin(), nodes.rend(), isNode)
                                                    : node;
        const int next = after != nodes.end() ? *after : closed ? *std::find_if(nodes.begin(), nodes.end(), isNode) : node;
        const auto knot = Knot(scene, node, previous, next, handles.mode);
        incoming = Vector(knot.incoming);
        outgoing = Vector(knot.outgoing);
        return true;
    }

    void SetNodeMode(const EditorScene& scene, int index, ItemData& data, CurveHandle mode)
    {
        if (data.curveNode.Get().mode == mode)
        {
            return;
        }
        const int curve = scene.items[index].parent;
        if (curve >= 0 && curve < scene.count && scene.data[curve].curve.HasValue())
        {
            const auto nodes = scene.Children(curve);
            const size_t node = std::find(nodes.begin(), nodes.end(), index) - nodes.begin();
            const bool closed = scene.data[curve].curve.Get().closed;
            const size_t previous = node > 0 ? node - 1 : closed ? nodes.size() - 1 : node;
            const size_t next = node + 1 < nodes.size() ? node + 1 : closed ? 0 : node;
            const auto effective = mode == CurveHandle::Auto || mode == CurveHandle::Vector ? mode : data.curveNode.Get().mode;
            const auto knot = Knot(scene, index, nodes[previous], nodes[next], effective);
            const auto rotation = NodeRotation(scene, curve, knot);
            Utils::BasisToEulerDegrees(
                Utils::ComposeRotationBases(Utils::TransposeRotationBasis(scene.world[curve].rot), rotation), data.transform.rot);
            const auto worldRotation
                = Utils::ComposeRotationBases(scene.world[curve].rot, Utils::EulerDegreesToBasis(data.transform.rot));
            const auto incoming = Utils::InverseTransformDirection(worldRotation, Vector(knot.incoming - knot.position));
            const auto outgoing = Utils::InverseTransformDirection(worldRotation, Vector(knot.outgoing - knot.position));
            auto& nodeData = data.curveNode.Edit();
            const float in[3] = { incoming.x, incoming.y, incoming.z };
            const float out[3] = { outgoing.x, outgoing.y, outgoing.z };
            std::copy_n(in, 3, nodeData.incoming);
            std::copy_n(out, 3, nodeData.outgoing);
        }
        auto& node = data.curveNode.Edit();
        node.mode = mode;
        node.Align(true);
    }

    void Synchronize(EditorScene& scene) noexcept
    {
        for (int curve = 0; curve < scene.count; ++curve)
        {
            if (!scene.data[curve].curve.HasValue())
            {
                continue;
            }
            const auto nodes = scene.Children(curve);
            const bool closed = scene.data[curve].curve.Get().closed;
            for (size_t node = 0; node < nodes.size(); ++node)
            {
                const int index = nodes[node];
                const auto& data = scene.data[index].curveNode.Get();
                if (data.mode != CurveHandle::Auto && data.mode != CurveHandle::Vector)
                {
                    continue;
                }
                const size_t previous = node > 0 ? node - 1 : closed ? nodes.size() - 1 : node;
                const size_t next = node + 1 < nodes.size() ? node + 1 : closed ? 0 : node;
                const auto rotation = NodeRotation(scene, curve, Knot(scene, index, nodes[previous], nodes[next], data.mode));
                float euler[3];
                Utils::BasisToEulerDegrees(
                    Utils::ComposeRotationBases(Utils::TransposeRotationBasis(scene.world[curve].rot), rotation), euler);
                auto& transform = scene.data[index].transform;
                if (memcmp(transform.rot, euler, sizeof(euler)) != 0)
                {
                    std::copy_n(euler, 3, transform.rot);
                    auto& world = scene.world[index];
                    world.rot = Utils::ComposeRotationBases(scene.world[curve].rot, Utils::EulerDegreesToBasis(euler));
                    Utils::BasisToEulerDegrees(world.rot, world.eulerDeg);
                    world.changedThisFrame = true;
                }
            }
        }
    }

    bool CanSubdivide(const EditorScene& scene)
    {
        for (int node = 0; node < scene.count; ++node)
        {
            if (!scene.items[node].selected || !scene.data[node].curveNode.HasValue())
            {
                continue;
            }
            const int curve = scene.items[node].parent;
            if (curve < 0 || !scene.data[curve].curve.HasValue())
            {
                continue;
            }
            if (scene.FindNextChild(curve, scene.items[node].order) >= 0
                || (scene.data[curve].curve.Get().closed && scene.FindNextChild(curve, INT_MIN) != node))
            {
                return true;
            }
        }
        return false;
    }

    bool SubdivideSelection()
    {
        if (!g_scene || !g_sceneActions)
        {
            return false;
        }
        try
        {
            auto& scene = *g_scene;
            if (!CanSubdivide(scene))
            {
                return false;
            }
            scene.UpdateWorldTransforms();
            std::vector<SubdivideAction::Entry> changes(0);
            std::vector<ItemSnapshot> created(0);
            for (int curve = 0; curve < scene.count; ++curve)
            {
                if (!scene.data[curve].curve.HasValue())
                {
                    continue;
                }
                const auto nodes = Nodes(scene, curve);
                if (nodes.size() < 2)
                {
                    continue;
                }
                const size_t segments = nodes.size() - (scene.data[curve].curve.Get().closed ? 0 : 1);
                if (std::none_of(nodes.begin(), nodes.begin() + segments,
                        [&](int node)
                        {
                            return scene.items[node].selected;
                        }))
                {
                    continue;
                }
                const auto knots = Knots(scene, curve, nodes);
                const auto base = changes.size();
                const auto localHandle = [&](int node, const CurveGeometry::Vec& point, float* out)
                {
                    const auto& world = scene.world[node];
                    const auto local = Utils::InverseTransformDirection(world.rot, Vector(point) - world.pos);
                    out[0] = local.x;
                    out[1] = local.y;
                    out[2] = local.z;
                };
                for (size_t i = 0; i < nodes.size(); ++i)
                {
                    const auto& item = scene.items[nodes[i]];
                    const auto& before = scene.data[nodes[i]].curveNode.Get();
                    changes.push_back({ item.id, item.order, static_cast<int>(i * 2), before, before });
                }
                for (size_t i = 0; i < segments; ++i)
                {
                    if (!scene.items[nodes[i]].selected)
                    {
                        continue;
                    }
                    if (created.size() >= static_cast<size_t>(EditorScene::MaxItems - scene.count) || scene.nextId <= 0
                        || scene.nextId >= INT_MAX - static_cast<int>(created.size()) - 1)
                    {
                        GuiShowToast("Curve subdivision exceeds the item limit.", true);
                        return false;
                    }
                    const size_t next = (i + 1) % nodes.size();
                    const auto& a = knots[i];
                    const auto& b = knots[next];
                    const auto ab = (a.position + a.outgoing) * .5;
                    const auto bc = (a.outgoing + b.incoming) * .5;
                    const auto cd = (b.incoming + b.position) * .5;
                    const auto incoming = (ab + bc) * .5;
                    const auto outgoing = (bc + cd) * .5;
                    const auto midpoint = (incoming + outgoing) * .5;
                    auto& first = changes[base + i].after;
                    auto& last = changes[base + next].after;
                    if (first.mode == CurveHandle::Free || first.mode == CurveHandle::Aligned)
                    {
                        localHandle(nodes[i], ab, first.outgoing);
                    }
                    if (last.mode == CurveHandle::Free || last.mode == CurveHandle::Aligned)
                    {
                        localHandle(nodes[next], cd, last.incoming);
                    }
                    ItemSnapshot item;
                    item.id = scene.nextId + static_cast<int>(created.size());
                    item.parentId = scene.items[curve].id;
                    item.order = static_cast<int>(i * 2 + 1);
                    item.type = SceneItemCurveNode;
                    strcpy_s(item.name, "Node");
                    const auto& world = scene.world[curve];
                    const auto position = Utils::InverseTransformDirection(world.rot, Vector(midpoint) - world.pos);
                    item.data.transform.pos[0] = position.x;
                    item.data.transform.pos[1] = position.y;
                    item.data.transform.pos[2] = position.z;
                    auto& node = item.data.curveNode.Edit();
                    node.mode = first.mode == last.mode ? first.mode : CurveHandle::Free;
                    const auto in = Utils::InverseTransformDirection(world.rot, Vector(incoming - midpoint));
                    const auto out = Utils::InverseTransformDirection(world.rot, Vector(outgoing - midpoint));
                    node.incoming[0] = in.x;
                    node.incoming[1] = in.y;
                    node.incoming[2] = in.z;
                    node.outgoing[0] = out.x;
                    node.outgoing[1] = out.y;
                    node.outgoing[2] = out.z;
                    const double turn = 2 * std::numbers::pi;
                    node.tilt = static_cast<float>(
                        std::remainder(a.tilt + std::remainder(b.tilt - a.tilt, turn) * .5, turn) * 180 / std::numbers::pi);
                    if (!node.IsValid())
                    {
                        GuiShowToast("Curve subdivision exceeds the handle limits.", true);
                        return false;
                    }
                    created.push_back(std::move(item));
                }
            }
            if (created.empty())
            {
                return false;
            }
            if (std::any_of(changes.begin(), changes.end(),
                    [](const auto& entry)
                    {
                        return !entry.after.IsValid();
                    }))
            {
                GuiShowToast("Curve subdivision exceeds the handle limits.", true);
                return false;
            }
            auto action = std::make_unique<SubdivideAction>(std::move(changes), std::move(created));
            if (action->MemoryUsage() > ActionManager::MaxHistoryBytes)
            {
                GuiShowToast("Curve subdivision exceeds the undo memory limit.", true);
                return false;
            }
            if (!g_sceneActions->Execute(std::move(action)))
            {
                return false;
            }
            SetHandle(-1, -1);
            return true;
        }
        catch (const std::bad_alloc&)
        {
            GuiShowToast("Not enough memory to subdivide the curve.", true);
            return false;
        }
    }

    bool ComputeBounds(const EditorScene& scene, int curve, CVector& minimum, CVector& maximum)
    {
        std::string error;
        const auto* path = Resolve(scene, curve, error);
        const auto& world = scene.world[curve];
        const auto local = [&](const CVector& point)
        {
            return Utils::TransformDirection(Utils::TransposeRotationBasis(world.rot), point - world.pos);
        };
        minimum = {};
        maximum = {};
        bool found = false;
        const auto add = [&](CVector point)
        {
            point = local(point);
            if (!found)
            {
                minimum = point;
                maximum = point;
                found = true;
            }
            else
            {
                minimum.x = (std::min)(minimum.x, point.x);
                minimum.y = (std::min)(minimum.y, point.y);
                minimum.z = (std::min)(minimum.z, point.z);
                maximum.x = (std::max)(maximum.x, point.x);
                maximum.y = (std::max)(maximum.y, point.y);
                maximum.z = (std::max)(maximum.z, point.z);
            }
        };
        if (path)
        {
            for (const auto& sample : path->Samples())
            {
                add(Vector(sample.position));
            }
        }
        else
        {
            for (const int node : Nodes(scene, curve))
            {
                add(scene.world[node].pos);
            }
        }
        return found;
    }

    void ResetCache()
    {
        g_caches.clear();
    }

    void Prune(const EditorScene& scene)
    {
        for (auto cache = g_caches.begin(); cache != g_caches.end();)
        {
            const int index = scene.FindIndexById(cache->first);
            if (index < 0 || !scene.data[index].curve.HasValue())
            {
                cache = g_caches.erase(cache);
            }
            else
            {
                ++cache;
            }
        }
    }
}
