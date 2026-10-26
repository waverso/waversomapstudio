// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "scene_data.h"

class EditorScene;

class SceneEditGuard
{
    public:
    explicit SceneEditGuard(bool enabled = true);
    ~SceneEditGuard();
    void Commit() noexcept
    {
        committed = true;
    }

    private:
    bool active = false;
    bool committed = false;
};

struct IAction
{
    virtual void Apply() = 0;
    virtual void Undo() = 0;
    virtual void Focus()
    {
    }
    virtual size_t MemoryUsage() const noexcept = 0;
    virtual ~IAction() = default;
};

class ActionManager
{
    public:
    static constexpr size_t MaxUndo = 128;

    static constexpr size_t MaxHistoryBytes = 128 * 1024 * 1024;

    ActionManager();
    void Prepare(size_t bytes);
    size_t HistoryBytes() const noexcept
    {
        return m_historyBytes;
    }
    bool Execute(std::unique_ptr<IAction> action, bool requireUndo = false);
    bool AppendApplied(std::unique_ptr<IAction>& action);
    bool CanAppendApplied(size_t bytes) const noexcept;
    bool RecordApplied(std::unique_ptr<IAction>& action);

    void Undo();
    void Redo();

    bool CanUndo() const
    {
        return !m_undoStack.empty();
    }
    bool CanRedo() const
    {
        return !m_redoStack.empty();
    }
    std::uint64_t UndoSequence() const noexcept;
    std::uint64_t RedoSequence() const noexcept;
    std::uint64_t LatestSequence() const noexcept;

    int GetSerial() const
    {
        return m_serial;
    }

    void Clear();
    void ClearRedo();
    void ClearBefore(std::uint64_t sequence);

    private:
    struct HistoryEntry
    {
        std::unique_ptr<IAction> action;
        std::uint64_t sequence = 0;
    };

    std::vector<HistoryEntry> m_undoStack;
    std::vector<HistoryEntry> m_redoStack;
    size_t m_historyBytes = 0;
    int m_serial = 0;
};

class CompositeAction : public IAction
{
    public:
    CompositeAction() : m_actions(0)
    {
    }
    void Add(std::unique_ptr<IAction> action)
    {
        m_actions.push_back(std::move(action));
    }
    void Reserve(size_t count)
    {
        m_actions.reserve(count);
    }
    bool Empty() const
    {
        return m_actions.empty();
    }

    void Apply() override;
    void Undo() override;
    size_t MemoryUsage() const noexcept override;

    private:
    std::vector<std::unique_ptr<IAction>> m_actions;
};

class DependentAction final : public IAction
{
    public:
    DependentAction(std::unique_ptr<IAction>& source, std::unique_ptr<IAction>& dependent)
        : source(std::move(source)), dependent(std::move(dependent))
    {
    }
    void Apply() override;
    void Undo() override;
    size_t MemoryUsage() const noexcept override
    {
        return sizeof(*this) + source->MemoryUsage() + dependent->MemoryUsage();
    }

    private:
    std::unique_ptr<IAction> source;
    std::unique_ptr<IAction> dependent;
};

class DeleteItemsAction : public IAction
{
    public:
    explicit DeleteItemsAction(std::vector<ItemSnapshot>&& items);

    void Apply() override;
    void Undo() override;
    size_t MemoryUsage() const noexcept override;

    private:
    std::vector<ItemSnapshot> m_items;
};

class CreateItemsAction : public IAction
{
    public:
    explicit CreateItemsAction(
        std::vector<ItemSnapshot>&& items, bool selectOnApply = true, int selectedItemId = -1, bool scrollOnApply = true);

    void Apply() override;
    void Undo() override;
    size_t MemoryUsage() const noexcept override;

    private:
    std::vector<ItemSnapshot> m_items;
    bool m_selectOnApply = true;
    int m_selectedItemId = -1;
    bool m_scrollOnApply = true;
};

class ItemDataEditAction : public IAction
{
    public:
    struct Entry
    {
        int itemId = -1;
        ItemData before;
        ItemData after;
    };

    explicit ItemDataEditAction(std::vector<Entry>&& entries);

    std::vector<Entry>& Entries() noexcept
    {
        return m_entries;
    }

    void Apply() override;
    void Undo() override;
    size_t MemoryUsage() const noexcept override;

    private:
    void ApplyEntries(bool useAfter);
    std::vector<Entry> m_entries;
};

class TransformEditAction : public IAction
{
    public:
    struct Entry
    {
        int itemId = -1;
        TransformData before;
        TransformData after;
    };

    TransformEditAction(const int* indices, size_t count);
    bool CaptureAfter();
    void Apply() override;
    void Undo() override;
    size_t MemoryUsage() const noexcept override;

    private:
    void ApplyEntries(bool useAfter);
    std::vector<Entry> m_entries;
};

class HierarchyEditAction : public IAction
{
    public:
    struct Entry
    {
        int itemId = -1;
        int beforeParentId = -1, afterParentId = -1;
        int beforeOrder = 0, afterOrder = 0;
        TransformData beforeLocal, afterLocal;
    };

    explicit HierarchyEditAction(std::vector<Entry> entries);

    void Apply() override;
    void Undo() override;
    size_t MemoryUsage() const noexcept override;

    private:
    void ApplyEntries(bool useAfter);
    std::vector<Entry> m_entries;
};

class RenameItemAction : public IAction
{
    public:
    RenameItemAction(int itemId, const char* before, const char* after);

    void Apply() override;
    void Undo() override;
    size_t MemoryUsage() const noexcept override;

    private:
    int m_itemId;
    char m_before[64];
    char m_after[64];
};

extern ActionManager* g_sceneActions;
extern ActionManager* g_userActions;

bool ActionHistoryCanUndo();
bool ActionHistoryCanRedo();
void ActionHistoryUndo();
void ActionHistoryRedo();
void ActionHistoryHandleInput();
void ActionHistoryClearForNewProject();
