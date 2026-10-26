// Source Repository: https://github.com/waverso/waversomapstudio

#include "actions.h"

#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

#include "../ui/gui_api.h"
#include "scene.h"

static bool g_sceneEditGuardActive = false;
static EditorScene g_sceneEditBackup;

SceneEditGuard::SceneEditGuard(bool enabled)
{
    if (enabled && g_scene && !g_sceneEditGuardActive)
    {
        try
        {
            g_scene->CopyTo(g_sceneEditBackup);
        }
        catch (...)
        {
            g_sceneEditBackup.DiscardAppendedItems(0);
            throw;
        }
        active = true;
        g_sceneEditGuardActive = true;
    }
}

SceneEditGuard::~SceneEditGuard()
{
    if (!active)
    {
        return;
    }
    g_sceneEditGuardActive = false;
    if (!committed)
    {
        const int oldCount = g_scene->count;
        g_scene->Swap(g_sceneEditBackup);
        GuiOnSceneRestored(oldCount);
    }
    g_sceneEditBackup.DiscardAppendedItems(0);
}

namespace
{
    void RestoreItemSnapshots(
        const std::vector<ItemSnapshot>& items, bool select = true, int selectedItemId = -1, bool scroll = true)
    {
        if (!g_scene)
        {
            return;
        }

        const int first = g_scene->count;
        const int nextId = g_scene->nextId;
        try
        {
            for (const auto& s : items)
            {
                if (g_scene->FindIndexById(s.id) >= 0)
                {
                    continue;
                }
                const int parentIdx = s.parentId >= 0 ? g_scene->FindIndexById(s.parentId) : -1;
                g_scene->AppendItemWithId(s, parentIdx);
            }
        }
        catch (...)
        {
            g_scene->DiscardAppendedItems(first);
            g_scene->nextId = nextId;
            throw;
        }
        GuiOnItemsAppended(first, g_scene->count);

        if (!select)
        {
            if (scroll && !items.empty())
            {
                GuiRequestOutlinerScrollTo(items.front().id);
            }
            return;
        }

        g_scene->ClearSelection();
        for (const auto& s : items)
        {
            if (selectedItemId >= 0 && s.id != selectedItemId)
            {
                continue;
            }
            const int idx = g_scene->FindIndexById(s.id);
            if (idx >= 0)
            {
                g_scene->items[idx].selected = true;
            }
        }
    }

    void DeleteItemSnapshots(const std::vector<ItemSnapshot>& items)
    {
        if (!g_scene)
        {
            return;
        }

        g_scene->ClearSelection();
        bool any = false;
        for (const auto& s : items)
        {
            const int idx = g_scene->FindIndexById(s.id);
            if (idx >= 0)
            {
                g_scene->items[idx].selected = true;
                any = true;
            }
        }
        if (any)
        {
            GuiDeleteSceneSelection();
        }
    }
}

void CompositeAction::Apply()
{
    SceneEditGuard guard(m_actions.size() > 1);
    for (auto& a : m_actions)
    {
        a->Apply();
    }
    guard.Commit();
}

void CompositeAction::Undo()
{
    SceneEditGuard guard(m_actions.size() > 1);
    for (auto it = m_actions.rbegin(); it != m_actions.rend(); ++it)
    {
        (*it)->Undo();
    }
    guard.Commit();
}

void DependentAction::Apply()
{
    SceneEditGuard guard;
    source->Apply();
    dependent->Apply();
    guard.Commit();
}

void DependentAction::Undo()
{
    SceneEditGuard guard;
    dependent->Undo();
    source->Undo();
    guard.Commit();
}

DeleteItemsAction::DeleteItemsAction(std::vector<ItemSnapshot>&& items) : m_items(0)
{
    m_items.swap(items);
}

void DeleteItemsAction::Apply()
{
    DeleteItemSnapshots(m_items);
}

void DeleteItemsAction::Undo()
{
    RestoreItemSnapshots(m_items);
}

CreateItemsAction::CreateItemsAction(
    std::vector<ItemSnapshot>&& items, bool selectOnApply, int selectedItemId, bool scrollOnApply)
    : m_items(0), m_selectOnApply(selectOnApply), m_selectedItemId(selectedItemId), m_scrollOnApply(scrollOnApply)
{
    m_items.swap(items);
}

void CreateItemsAction::Apply()
{
    RestoreItemSnapshots(m_items, m_selectOnApply, m_selectedItemId, m_scrollOnApply);
}

void CreateItemsAction::Undo()
{
    DeleteItemSnapshots(m_items);
}

ItemDataEditAction::ItemDataEditAction(std::vector<Entry>&& entries) : m_entries(0)
{
    m_entries.swap(entries);
}

void ItemDataEditAction::ApplyEntries(bool useAfter)
{
    if (!g_scene)
    {
        return;
    }
    std::vector<std::pair<int, ItemData>> changes(0);
    for (const auto& entry : m_entries)
    {
        const int idx = g_scene->FindIndexById(entry.itemId);
        const auto& value = useAfter ? entry.after : entry.before;
        if (idx >= 0 && g_scene->data[idx] != value)
        {
            changes.emplace_back(idx, value);
        }
    }
    if (changes.empty())
    {
        return;
    }
    for (auto& [idx, value] : changes)
    {
        g_scene->data[idx] = std::move(value);
    }
    g_scene->UpdateWorldTransforms();
}

void ItemDataEditAction::Apply()
{
    ApplyEntries(true);
}

void ItemDataEditAction::Undo()
{
    ApplyEntries(false);
}

HierarchyEditAction::HierarchyEditAction(std::vector<Entry> entries) : m_entries(std::move(entries))
{
}

void HierarchyEditAction::ApplyEntries(bool useAfter)
{
    if (!g_scene)
    {
        return;
    }
    g_scene->InvalidateHierarchy();
    for (const auto& e : m_entries)
    {
        const int idx = g_scene->FindIndexById(e.itemId);
        if (idx < 0)
        {
            continue;
        }

        const int parentId = useAfter ? e.afterParentId : e.beforeParentId;
        g_scene->items[idx].parent = (parentId >= 0) ? g_scene->FindIndexById(parentId) : -1;
        g_scene->items[idx].order = useAfter ? e.afterOrder : e.beforeOrder;
        g_scene->data[idx].transform = useAfter ? e.afterLocal : e.beforeLocal;
    }
}

void HierarchyEditAction::Apply()
{
    ApplyEntries(true);
}

void HierarchyEditAction::Undo()
{
    ApplyEntries(false);
}

RenameItemAction::RenameItemAction(int itemId, const char* before, const char* after) : m_itemId(itemId)
{
    strcpy_s(m_before, before ? before : "");
    strcpy_s(m_after, after ? after : "");
}

void RenameItemAction::Apply()
{
    if (!g_scene)
    {
        return;
    }
    const int idx = g_scene->FindIndexById(m_itemId);
    if (idx >= 0 && g_scene->items[idx].type != SceneItemRaceCheckpoint)
    {
        strcpy_s(g_scene->items[idx].name, m_after);
        g_scene->UpdateRaceNames();
    }
}

void RenameItemAction::Undo()
{
    if (!g_scene)
    {
        return;
    }
    const int idx = g_scene->FindIndexById(m_itemId);
    if (idx >= 0 && g_scene->items[idx].type != SceneItemRaceCheckpoint)
    {
        strcpy_s(g_scene->items[idx].name, m_before);
        g_scene->UpdateRaceNames();
    }
}

size_t CompositeAction::MemoryUsage() const noexcept
{
    size_t bytes = sizeof(*this) + m_actions.capacity() * sizeof(m_actions[0]);
    for (const auto& action : m_actions)
    {
        bytes += action->MemoryUsage();
    }
    return bytes;
}

size_t DeleteItemsAction::MemoryUsage() const noexcept
{
    size_t bytes = sizeof(*this) + m_items.capacity() * sizeof(ItemSnapshot);
    for (const auto& item : m_items)
    {
        bytes += item.data.DynamicMemoryUsage();
    }
    return bytes;
}

size_t CreateItemsAction::MemoryUsage() const noexcept
{
    size_t bytes = sizeof(*this) + m_items.capacity() * sizeof(ItemSnapshot);
    for (const auto& item : m_items)
    {
        bytes += item.data.DynamicMemoryUsage();
    }
    return bytes;
}

size_t ItemDataEditAction::MemoryUsage() const noexcept
{
    size_t bytes = sizeof(*this) + m_entries.capacity() * sizeof(Entry);
    for (const auto& entry : m_entries)
    {
        bytes += entry.before.DynamicMemoryUsage() + entry.after.DynamicMemoryUsage();
    }
    return bytes;
}

size_t HierarchyEditAction::MemoryUsage() const noexcept
{
    return sizeof(*this) + m_entries.capacity() * sizeof(Entry);
}

size_t RenameItemAction::MemoryUsage() const noexcept
{
    return sizeof(*this);
}

TransformEditAction::TransformEditAction(const int* indices, size_t count) : m_entries(count)
{
    for (size_t i = 0; i < count; ++i)
    {
        m_entries[i].itemId = g_scene->items[indices[i]].id;
        m_entries[i].before = g_scene->data[indices[i]].transform;
    }
}

bool TransformEditAction::CaptureAfter()
{
    size_t changed = 0;
    if (g_scene)
    {
        for (auto& entry : m_entries)
        {
            const int idx = g_scene->FindIndexById(entry.itemId);
            if (idx < 0)
            {
                continue;
            }
            entry.after = g_scene->data[idx].transform;
            if (memcmp(&entry.before, &entry.after, sizeof(TransformData)) != 0)
            {
                m_entries[changed++] = entry;
            }
        }
    }
    m_entries.resize(changed);
    return changed != 0;
}

void TransformEditAction::ApplyEntries(bool useAfter)
{
    if (!g_scene)
    {
        return;
    }
    for (const auto& entry : m_entries)
    {
        const int idx = g_scene->FindIndexById(entry.itemId);
        if (idx >= 0)
        {
            g_scene->data[idx].transform = useAfter ? entry.after : entry.before;
        }
    }
}

void TransformEditAction::Apply()
{
    ApplyEntries(true);
}

void TransformEditAction::Undo()
{
    ApplyEntries(false);
}

size_t TransformEditAction::MemoryUsage() const noexcept
{
    return sizeof(*this) + m_entries.capacity() * sizeof(Entry);
}
