// Source Repository: https://github.com/waverso/waversomapstudio

#include <algorithm>
#include <cstring>
#include <span>
#include <vector>

#include "scene.h"

void EditorScene::InvalidateHierarchy() noexcept
{
    m_hierarchyDirty = true;
}

void EditorScene::EnsureHierarchy() const
{
    if (!m_hierarchyDirty)
    {
        return;
    }
    m_siblingOrder.resize(count);
    m_displayOrder.clear();
    m_root = {};
    for (int i = 0; i < count; ++i)
    {
        m_hierarchy[i] = {};
        m_siblingOrder[i] = i;
    }
    int* sorted = m_siblingOrder.data();
    if (count > 1)
    {
        std::sort(sorted, sorted + count,
            [&](int a, int b)
            {
                const auto& left = items[a];
                const auto& right = items[b];
                if (left.parent != right.parent)
                {
                    return left.parent < right.parent;
                }
                return left.order != right.order ? left.order < right.order : a < b;
            });
    }
    for (int begin = 0; begin < count;)
    {
        const int parent = items[sorted[begin]].parent;
        int end = begin + 1;
        while (end < count && items[sorted[end]].parent == parent)
        {
            ++end;
        }
        if (parent >= -1 && parent < count)
        {
            auto& node = parent < 0 ? m_root : m_hierarchy[parent];
            node.begin = begin;
            node.end = end;
        }
        begin = end;
    }
    struct Frame
    {
        int cursor;
        int end;
        int parent;
    };
    Frame stack[MaxItems + 1];
    int depth = 0;
    stack[0] = { m_root.begin, m_root.end, -1 };
    while (depth >= 0)
    {
        auto& frame = stack[depth];
        if (frame.cursor == frame.end)
        {
            if (frame.parent >= 0)
            {
                m_hierarchy[frame.parent].displayEnd = static_cast<int>(m_displayOrder.size());
            }
            --depth;
            continue;
        }
        const int index = sorted[frame.cursor++];
        auto& node = m_hierarchy[index];
        node.depth = depth;
        node.displayBegin = static_cast<int>(m_displayOrder.size());
        m_displayOrder.push_back(index);
        node.displayEnd = static_cast<int>(m_displayOrder.size());
        if (node.begin != node.end)
        {
            stack[++depth] = { node.begin, node.end, index };
        }
    }
    m_hierarchyDirty = false;
}

const std::vector<int>& EditorScene::DisplayOrder() const
{
    EnsureHierarchy();
    return m_displayOrder;
}

int EditorScene::HierarchyDepth(int index) const
{
    EnsureHierarchy();
    return m_hierarchy[index].depth;
}

bool EditorScene::HasChildren(int idx) const
{
    EnsureHierarchy();
    const auto& node = idx < 0 ? m_root : m_hierarchy[idx];
    return node.begin != node.end;
}

std::span<const int> EditorScene::Children(int parent) const
{
    if (parent < -1 || parent >= count)
    {
        return {};
    }
    EnsureHierarchy();
    const auto& node = parent < 0 ? m_root : m_hierarchy[parent];
    return std::span<const int>(m_siblingOrder).subspan(node.begin, node.end - node.begin);
}

bool EditorScene::IsDescendantOf(int child, int possibleParent) const
{
    int p = (child >= 0 && child < count) ? items[child].parent : -1;
    while (p >= 0 && p < count)
    {
        if (p == possibleParent)
        {
            return true;
        }
        p = items[p].parent;
    }
    return false;
}

bool EditorScene::HasSelectedAncestor(int idx) const
{
    int p = items[idx].parent;
    while (p >= 0 && p < count)
    {
        if (items[p].selected)
        {
            return true;
        }
        p = items[p].parent;
    }
    return false;
}

int EditorScene::FindNextChild(int parent, int previousOrder) const
{
    EnsureHierarchy();
    const auto& node = parent < 0 ? m_root : m_hierarchy[parent];
    if (node.begin == node.end)
    {
        return -1;
    }
    const int* begin = m_siblingOrder.data() + node.begin;
    const int* end = m_siblingOrder.data() + node.end;
    const int* found = std::upper_bound(begin, end, previousOrder,
        [&](int order, int index)
        {
            return order < items[index].order;
        });
    return found == end ? -1 : *found;
}

void EditorScene::CollectDisplayOrder(int parent, int* out, int& outCount) const
{
    EnsureHierarchy();
    const int begin = parent < 0 ? 0 : m_hierarchy[parent].displayBegin + 1;
    const int end = parent < 0 ? static_cast<int>(m_displayOrder.size()) : m_hierarchy[parent].displayEnd;
    for (int i = begin; i < end; ++i)
    {
        out[outCount++] = m_displayOrder[i];
    }
}

bool EditorScene::CanParent(SceneItemType type, int parent) const
{
    if (parent < -1 || parent >= count)
    {
        return false;
    }
    if (type == SceneItemRaceCheckpoint)
    {
        return parent >= 0 && items[parent].type == SceneItemRace;
    }
    if (type == SceneItemCurveNode)
    {
        return parent >= 0 && items[parent].type == SceneItemCurve;
    }
    if (type == SceneItemCurve)
    {
        for (int ancestor = parent; ancestor >= 0; ancestor = items[ancestor].parent)
        {
            if (data[ancestor].layout.HasValue())
            {
                return false;
            }
        }
    }
    return parent == -1
        || (items[parent].type != SceneItemRace && items[parent].type != SceneItemRaceCheckpoint
            && items[parent].type != SceneItemCurve && items[parent].type != SceneItemCurveNode);
}

bool EditorScene::CanDropSelection(int target, SceneDropMode mode) const
{
    if (mode == SceneDropNone)
    {
        return false;
    }
    if (mode != SceneDropRoot && (target < 0 || target >= count || items[target].selected))
    {
        return false;
    }
    if (mode == SceneDropInto && items[target].type == SceneItemRemoveBuilding)
    {
        return false;
    }

    const int newParent = mode == SceneDropRoot ? -1 : (mode == SceneDropInto ? target : items[target].parent);

    for (int i = 0; i < count; ++i)
    {
        if (!items[i].selected || HasSelectedAncestor(i))
        {
            continue;
        }
        if (!CanParent(items[i].type, newParent))
        {
            return false;
        }
        if (!CanParent(SceneItemCurve, newParent))
        {
            for (int child = 0; child < count; ++child)
            {
                if (data[child].curve.HasValue() && IsDescendantOf(child, i))
                {
                    return false;
                }
            }
        }
        if (newParent == i || (newParent >= 0 && IsDescendantOf(newParent, i)))
        {
            return false;
        }
    }
    return true;
}

void EditorScene::ApplyDrop(int dropTarget, SceneDropMode dropMode)
{
    if (!CanDropSelection(dropTarget, dropMode))
    {
        return;
    }
    UpdateWorldTransforms();
    const auto& ordered = DisplayOrder();
    int roots[MaxItems];
    bool selectedBranch[MaxItems] {};
    bool moving[MaxItems] {};
    int rootCount = 0;
    for (const int index : ordered)
    {
        const int parent = items[index].parent;
        const bool inherited = parent >= 0 && selectedBranch[parent];
        selectedBranch[index] = inherited || items[index].selected;
        if (items[index].selected && !inherited)
        {
            roots[rootCount++] = index;
            moving[index] = true;
        }
    }
    if (!rootCount)
    {
        return;
    }
    const int newParent = dropMode == SceneDropRoot ? -1 : dropMode == SceneDropInto ? dropTarget : items[dropTarget].parent;
    int siblings[MaxItems];
    int siblingCount = 0;
    int insertAt = -1;
    for (const int index : ordered)
    {
        if (items[index].parent != newParent || moving[index])
        {
            continue;
        }
        if (index == dropTarget && dropMode == SceneDropBefore)
        {
            insertAt = siblingCount;
        }
        siblings[siblingCount++] = index;
        if (index == dropTarget && dropMode == SceneDropAfter)
        {
            insertAt = siblingCount;
        }
    }
    if (insertAt < 0)
    {
        insertAt = siblingCount;
    }
    int order = 0;
    for (int i = 0; i <= siblingCount; ++i)
    {
        if (i == insertAt)
        {
            for (int r = 0; r < rootCount; ++r)
            {
                const int index = roots[r];
                const CVector position = world[index].pos;
                float rotation[3];
                memcpy(rotation, world[index].eulerDeg, sizeof(rotation));
                items[index].parent = newParent;
                items[index].order = (++order) * 10;
                SetWorldTransform(index, position, rotation);
            }
        }
        if (i < siblingCount)
        {
            items[siblings[i]].order = (++order) * 10;
        }
    }
    if (dropMode == SceneDropInto)
    {
        items[dropTarget].open = true;
    }
    InvalidateHierarchy();
}
