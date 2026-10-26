// Source Repository: https://github.com/waverso/waversomapstudio

#include <algorithm>
#include <cstddef>
#include <new>
#include <utility>
#include <vector>

#include "actions.h"
#include "scene.h"

void EditorScene::CaptureSelectionSnapshots(std::vector<ItemSnapshot>& out) const
{
    int ordered[MaxItems];
    int countOrdered = 0;
    CollectDisplayOrder(-1, ordered, countOrdered);
    int selected = 0;
    for (int k = 0; k < countOrdered; ++k)
    {
        const int i = ordered[k];
        if (items[i].selected || HasSelectedAncestor(i))
        {
            ordered[selected++] = i;
        }
    }
    if (selected == 0)
    {
        return;
    }
    const size_t capacity = (std::max)(out.capacity(), out.size() + selected);
    size_t dynamicBytes = 0;
    for (int k = 0; k < selected; ++k)
    {
        dynamicBytes += data[ordered[k]].DynamicMemoryUsage();
    }
    if (g_sceneActions)
    {
        g_sceneActions->Prepare(sizeof(CreateItemsAction) + dynamicBytes + (capacity + out.capacity()) * sizeof(ItemSnapshot));
    }
    out.reserve(out.size() + selected);
    for (int k = 0; k < selected; ++k)
    {
        out.emplace_back();
        CaptureItemSnapshot(ordered[k], out.back());
    }
}

int EditorScene::SingleSelectedIndex() const
{
    int found = -1;
    for (int i = 0; i < count; ++i)
    {
        if (!items[i].selected || HasSelectedAncestor(i))
        {
            continue;
        }
        if (found >= 0)
        {
            return -1;
        }
        found = i;
    }
    return found;
}

void EditorScene::ClearSelection()
{
    for (int i = 0; i < count; ++i)
    {
        items[i].selected = false;
    }
}

int EditorScene::SelectionCount() const
{
    int n = 0;
    for (int i = 0; i < count; ++i)
    {
        if (items[i].selected)
        {
            ++n;
        }
    }
    return n;
}

void EditorScene::DeleteSelection(int* outRemap, int& outOldCount)
{
    InvalidateHierarchy();
    outOldCount = count;
    int newCount = 0;
    for (int i = 0; i < count; ++i)
    {
        const bool remove = items[i].selected || HasSelectedAncestor(i);
        outRemap[i] = remove ? -1 : newCount++;
    }
    for (int i = 0; i < count; ++i)
    {
        const int target = outRemap[i];
        if (target < 0)
        {
            m_indexById.erase(items[i].id);
            continue;
        }
        if (target != i)
        {
            items[target] = items[i];
            data[target] = std::move(data[i]);
            world[target] = world[i];
        }
        const int parent = items[target].parent;
        items[target].parent = parent >= 0 ? outRemap[parent] : -1;
        m_indexById.find(items[target].id)->second = target;
    }
    for (int i = newCount; i < count; ++i)
    {
        items[i] = SceneItem {};
        data[i] = ItemData {};
        world[i] = WorldTransform {};
    }
    count = newCount;
    items.Trim(count);
    data.Trim(count);
    world.Trim(count);
}

bool EditorScene::CanCloneSelection() const
{
    for (int i = 0; i < count; ++i)
    {
        if (!items[i].selected || HasSelectedAncestor(i))
        {
            continue;
        }
        if (items[i].type != SceneItemRemoveBuilding)
        {
            return true;
        }
    }
    return false;
}

int EditorScene::CloneSelection(int* outClones, bool insertBefore)
{
    int ordered[MaxItems];
    int orderedCount = 0;
    CollectDisplayOrder(-1, ordered, orderedCount);
    const int first = count;
    const int savedNextId = nextId;
    int savedOrders[MaxItems];
    int cloned[MaxItems];
    bool planned[MaxItems] {};
    bool reorder[MaxItems + 1] {};
    int needed = 0;
    for (int i = 0; i < first; ++i)
    {
        savedOrders[i] = items[i].order;
        cloned[i] = -1;
    }
    for (int k = 0; k < orderedCount; ++k)
    {
        const int index = ordered[k];
        const int parent = items[index].parent;
        planned[index] = items[index].type != SceneItemRemoveBuilding
            && ((parent >= 0 && planned[parent]) || (items[index].selected && !HasSelectedAncestor(index)));
        needed += planned[index];
    }
    if (needed == 0 || needed > MaxItems - first)
    {
        return 0;
    }
    int cloneCount = 0;
    try
    {
        for (int k = 0; k < orderedCount; ++k)
        {
            const int source = ordered[k];
            if (!planned[source])
            {
                continue;
            }
            const int parent = items[source].parent;
            const bool root = parent < 0 || cloned[parent] < 0;
            ItemData copied = data[source];
            const int clone = AppendItem(root ? parent : cloned[parent], items[source].type, items[source].name);
            if (clone < 0)
            {
                throw std::bad_alloc();
            }
            cloned[source] = clone;
            items[clone].open = items[source].open;
            items[clone].searchModelId = items[source].searchModelId;
            items[clone].order = items[source].order;
            data[clone] = std::move(copied);
            if (root)
            {
                outClones[cloneCount++] = clone;
                reorder[parent + 1] = true;
            }
        }
        for (int source = 0; source < first; ++source)
        {
            if (cloned[source] < 0 || !data[cloned[source]].layout.HasValue())
            {
                continue;
            }
            auto& layout = data[cloned[source]].layout.Edit();
            const int curve = FindIndexById(layout.curveId);
            if (curve >= 0 && curve < first && cloned[curve] >= 0)
            {
                layout.curveId = items[cloned[curve]].id;
            }
        }
        int sequence[MaxItems + 1] {};
        for (int k = 0; k < orderedCount; ++k)
        {
            const int source = ordered[k];
            const int group = items[source].parent + 1;
            if (!reorder[group])
            {
                continue;
            }
            if (insertBefore && cloned[source] >= 0)
            {
                items[cloned[source]].order = (++sequence[group]) * 10;
            }
            items[source].order = (++sequence[group]) * 10;
            if (!insertBefore && cloned[source] >= 0)
            {
                items[cloned[source]].order = (++sequence[group]) * 10;
            }
        }
    }
    catch (...)
    {
        DiscardAppendedItems(first);
        nextId = savedNextId;
        for (int i = 0; i < first; ++i)
        {
            items[i].order = savedOrders[i];
        }
        throw;
    }
    InvalidateHierarchy();
    UpdateWorldTransforms();
    ClearSelection();
    for (int i = 0; i < cloneCount; ++i)
    {
        items[outClones[i]].selected = true;
    }
    return cloneCount;
}
