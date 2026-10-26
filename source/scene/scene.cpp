// Source Repository: https://github.com/waverso/waversomapstudio

#include "scene.h"

#include <cassert>
#include <climits>
#include <cstring>
#include <utility>

void EditorScene::CopyTo(EditorScene& target) const
{
    target.EnsureCapacity(count);
    for (int index = 0; index < count; ++index)
    {
        target.items[index] = items[index];
        target.data[index] = data[index];
        target.world[index] = world[index];
    }
    target.m_indexById = m_indexById;
    target.m_hierarchy = m_hierarchy;
    target.m_siblingOrder = m_siblingOrder;
    target.m_displayOrder = m_displayOrder;
    target.m_root = m_root;
    target.m_hierarchyDirty = m_hierarchyDirty;
    target.count = count;
    target.nextId = nextId;
}

void EditorScene::Swap(EditorScene& other) noexcept
{
    items.Swap(other.items);
    data.Swap(other.data);
    world.Swap(other.world);
    std::swap(count, other.count);
    std::swap(nextId, other.nextId);
    m_indexById.swap(other.m_indexById);
    m_hierarchy.swap(other.m_hierarchy);
    m_siblingOrder.swap(other.m_siblingOrder);
    m_displayOrder.swap(other.m_displayOrder);
    std::swap(m_root, other.m_root);
    std::swap(m_hierarchyDirty, other.m_hierarchyDirty);
}

void EditorScene::EnsureCapacity(int required)
{
    items.EnsureCapacity(required);
    data.EnsureCapacity(required);
    world.EnsureCapacity(required);
    m_hierarchy.resize(items.Capacity());
    m_siblingOrder.reserve(items.Capacity());
    m_displayOrder.reserve(items.Capacity());
}

void EditorScene::DiscardAppendedItems(int first) noexcept
{
    assert(first >= 0 && first <= count);
    for (int i = first; i < count; ++i)
    {
        m_indexById.erase(items[i].id);
        items[i] = SceneItem {};
        data[i] = ItemData {};
        world[i] = WorldTransform {};
    }
    count = first;
    InvalidateHierarchy();
    items.Trim(count);
    data.Trim(count);
    world.Trim(count);
}

int EditorScene::AppendItem(int parent, SceneItemType type, const char* name)
{
    if (count >= MaxItems || nextId <= 0 || nextId == INT_MAX || !CanParent(type, parent))
    {
        return -1;
    }

    EnsureCapacity(count + 1);
    const int idx = count;
    if (!m_indexById.emplace(nextId, idx).second)
    {
        return -1;
    }
    ++count;
    InvalidateHierarchy();
    SceneItem& it = items[idx];
    it.id = nextId++;
    it.parent = parent;
    int order = 10;
    for (int i = 0; i < idx; ++i)
    {
        if (items[i].parent == parent && items[i].order >= order)
        {
            order = items[i].order + 10;
        }
    }
    it.order = order;
    it.type = type;
    it.open = true;
    it.selected = false;
    it.searchModelId = -2;
    strcpy_s(it.name, name ? name : "Item");
    if (type == SceneItemRace)
    {
        data[idx].race.Edit();
        it.name[RaceData::MaxName] = '\0';
    }
    else if (type == SceneItemRaceCheckpoint)
    {
        data[idx].raceCheckpoint.Edit();
    }
    else if (type == SceneItemCurve)
    {
        data[idx].curve.Edit();
    }
    else if (type == SceneItemCurveNode)
    {
        data[idx].curveNode.Edit();
    }
    return idx;
}

int EditorScene::AppendItemWithId(const ItemSnapshot& snap, int parentIdx)
{
    if (count >= MaxItems || snap.id <= 0 || snap.id == INT_MAX || !CanParent(snap.type, parentIdx)
        || m_indexById.contains(snap.id))
    {
        return -1;
    }

    ItemData restored = snap.data;
    EnsureCapacity(count + 1);
    m_indexById.emplace(snap.id, count);

    const int idx = count++;
    InvalidateHierarchy();
    SceneItem& it = items[idx];
    it.id = snap.id;
    it.parent = parentIdx;
    it.order = snap.order;
    it.type = snap.type;
    it.open = snap.open;
    it.selected = false;
    it.searchModelId = snap.searchModelId;
    strcpy_s(it.name, snap.name);
    data[idx] = std::move(restored);

    if (snap.id >= nextId)
    {
        nextId = snap.id + 1;
    }
    return idx;
}

void EditorScene::CaptureItemSnapshot(int idx, ItemSnapshot& out) const
{
    out.id = items[idx].id;
    out.parentId = items[idx].parent >= 0 ? items[items[idx].parent].id : -1;
    out.order = items[idx].order;
    out.type = items[idx].type;
    out.open = items[idx].open;
    out.searchModelId = items[idx].searchModelId;
    strcpy_s(out.name, items[idx].name);
    out.data = data[idx];
}

void EditorScene::ResetItemData(int idx)
{
    data[idx] = ItemData {};
}

void EditorScene::InitItemNormal(int idx, bool withModel, int defaultModelId)
{
    ResetItemData(idx);
    items[idx].searchModelId = -2;
    if (withModel)
    {
        data[idx].model.Edit();
        data[idx].modelSeq = data[idx].nextSeq++;
        data[idx].model.Edit().modelId = defaultModelId;
        ApplyModelDistanceDefaults(data[idx].model.Edit());
        items[idx].searchModelId = defaultModelId;
    }
}

void EditorScene::InitItemRemoveBuilding(int idx, int modelId, bool custom)
{
    ResetItemData(idx);
    data[idx].removeBuilding.Edit();
    data[idx].removeBuilding.Edit().modelId = modelId;
    data[idx].removeBuilding.Edit().custom = custom;
    if (custom)
    {
        data[idx].removeBuilding.Edit().radius = 5.0f;
    }
    items[idx].searchModelId = modelId;
}

int EditorScene::FindIndexById(int id) const
{
    const auto found = m_indexById.find(id);
    return found == m_indexById.end() ? -1 : found->second;
}
