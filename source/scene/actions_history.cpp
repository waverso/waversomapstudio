// Source Repository: https://github.com/waverso/waversomapstudio

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include "../utils/utils.h"
#include "../vendor/imgui/imgui.h"
#include "actions.h"
#include "free_camera.h"
#include "gizmos.h"
#include "../ui/gui_api.h"
#include "../layout/layout.h"
#include "../hooks/samp.h"

namespace
{
    std::uint64_t g_nextActionSequence = 0;

    std::uint64_t NextActionSequence()
    {
        return ++g_nextActionSequence;
    }

    bool FinishAutoApply()
    {
        if (g_gizmos && (g_gizmos->IsDragging() || g_gizmos->IsRadiusDragging() || g_gizmos->IsBoxAreaDragging()))
        {
            return false;
        }
        try
        {
            return GuiFinishPropertyEdit() && Layout::SettleAutoApply();
        }
        catch (const std::bad_alloc&)
        {
            GuiShowToast("Not enough memory to finish Auto Apply.", true);
            return false;
        }
    }

    ActionManager* LatestUndoManager()
    {
        ActionManager* manager = nullptr;
        if (g_sceneActions && g_sceneActions->CanUndo())
        {
            manager = g_sceneActions;
        }
        if (g_userActions && g_userActions->CanUndo() && (!manager || g_userActions->UndoSequence() > manager->UndoSequence()))
        {
            manager = g_userActions;
        }
        return manager;
    }

    ActionManager* LatestRedoManager()
    {
        ActionManager* manager = nullptr;
        if (g_sceneActions && g_sceneActions->CanRedo())
        {
            manager = g_sceneActions;
        }
        if (g_userActions && g_userActions->CanRedo() && (!manager || g_userActions->RedoSequence() > manager->RedoSequence()))
        {
            manager = g_userActions;
        }
        return manager;
    }
}

ActionManager::ActionManager()
{
    m_undoStack.reserve(MaxUndo);
    m_redoStack.reserve(MaxUndo);
}

void ActionManager::Prepare(size_t bytes)
{
    const size_t available = MaxHistoryBytes - (std::min)(bytes, MaxHistoryBytes);
    while (m_historyBytes > available)
    {
        auto& stack = m_undoStack.empty() ? m_redoStack : m_undoStack;
        m_historyBytes -= stack.front().action->MemoryUsage();
        stack.erase(stack.begin());
    }
}

void ActionManager::ClearRedo()
{
    for (const auto& redo : m_redoStack)
    {
        m_historyBytes -= redo.action->MemoryUsage();
    }
    m_redoStack.clear();
}

bool ActionManager::Execute(std::unique_ptr<IAction> action, bool requireUndo)
{
    if (!action)
    {
        return false;
    }
    const size_t bytes = action->MemoryUsage();
    if (requireUndo && bytes > MaxHistoryBytes)
    {
        GuiShowToast("Edit canceled: undo data exceeds the memory limit.", true);
        return false;
    }
    try
    {
        action->Apply();
    }
    catch (const std::bad_alloc&)
    {
        GuiShowToast("Not enough memory to apply the edit.", true);
        return false;
    }
    if (g_sceneActions && g_sceneActions != this)
    {
        g_sceneActions->ClearRedo();
    }
    if (g_userActions && g_userActions != this)
    {
        g_userActions->ClearRedo();
    }
    ClearRedo();
    Prepare(bytes);
    if (bytes > MaxHistoryBytes)
    {
        ++m_serial;
        GuiShowToast("Undo history cleared: the edit exceeds the memory limit.", true);
        return true;
    }
    if (m_undoStack.size() == MaxUndo)
    {
        m_historyBytes -= m_undoStack.front().action->MemoryUsage();
        m_undoStack.erase(m_undoStack.begin());
    }
    m_undoStack.push_back({ std::move(action), NextActionSequence() });
    m_historyBytes += bytes;
    ++m_serial;
    return true;
}

bool ActionManager::AppendApplied(std::unique_ptr<IAction>& action)
{
    if (!action || m_undoStack.empty())
    {
        return false;
    }
    auto& entry = m_undoStack.back();
    const size_t previous = entry.action->MemoryUsage();
    const size_t combined = previous + action->MemoryUsage() + sizeof(DependentAction);
    if (combined > MaxHistoryBytes)
    {
        return false;
    }
    auto group = std::make_unique<DependentAction>(entry.action, action);
    const size_t bytes = group->MemoryUsage();
    entry.action = std::move(group);
    m_historyBytes += bytes - previous;
    Prepare(0);
    ++m_serial;
    return true;
}

bool ActionManager::CanAppendApplied(size_t bytes) const noexcept
{
    return !m_undoStack.empty() && m_undoStack.back().action->MemoryUsage() + bytes + sizeof(DependentAction) <= MaxHistoryBytes;
}

bool ActionManager::RecordApplied(std::unique_ptr<IAction>& action)
{
    if (!action || action->MemoryUsage() > MaxHistoryBytes)
    {
        return false;
    }
    const size_t bytes = action->MemoryUsage();
    if (g_sceneActions && g_sceneActions != this)
    {
        g_sceneActions->ClearRedo();
    }
    if (g_userActions && g_userActions != this)
    {
        g_userActions->ClearRedo();
    }
    ClearRedo();
    Prepare(bytes);
    if (m_undoStack.size() == MaxUndo)
    {
        m_historyBytes -= m_undoStack.front().action->MemoryUsage();
        m_undoStack.erase(m_undoStack.begin());
    }
    m_undoStack.push_back({ std::move(action), NextActionSequence() });
    m_historyBytes += bytes;
    ++m_serial;
    return true;
}

void ActionManager::Undo()
{
    if (m_undoStack.empty()
        || (g_gizmos && (g_gizmos->IsDragging() || g_gizmos->IsRadiusDragging() || g_gizmos->IsBoxAreaDragging())))
    {
        return;
    }
    if (!FinishAutoApply())
    {
        return;
    }
    HistoryEntry entry = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    try
    {
        entry.action->Undo();
    }
    catch (const std::bad_alloc&)
    {
        m_undoStack.push_back(std::move(entry));
        GuiShowToast("Not enough memory to undo the edit.", true);
        return;
    }
    if (this == g_userActions)
    {
        entry.action->Focus();
    }
    entry.sequence = NextActionSequence();
    m_redoStack.push_back(std::move(entry));
    Layout::ResetAutoApply(false, true);
    ++m_serial;
}

void ActionManager::Redo()
{
    if (m_redoStack.empty()
        || (g_gizmos && (g_gizmos->IsDragging() || g_gizmos->IsRadiusDragging() || g_gizmos->IsBoxAreaDragging())))
    {
        return;
    }
    if (!FinishAutoApply())
    {
        return;
    }
    if (m_redoStack.empty())
    {
        return;
    }
    HistoryEntry entry = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    try
    {
        entry.action->Apply();
    }
    catch (const std::bad_alloc&)
    {
        m_redoStack.push_back(std::move(entry));
        GuiShowToast("Not enough memory to redo the edit.", true);
        return;
    }
    if (this == g_userActions)
    {
        entry.action->Focus();
    }
    entry.sequence = NextActionSequence();
    m_undoStack.push_back(std::move(entry));
    Layout::ResetAutoApply(true, true);
    ++m_serial;
}

std::uint64_t ActionManager::UndoSequence() const noexcept
{
    return m_undoStack.empty() ? 0 : m_undoStack.back().sequence;
}

std::uint64_t ActionManager::RedoSequence() const noexcept
{
    return m_redoStack.empty() ? 0 : m_redoStack.back().sequence;
}

std::uint64_t ActionManager::LatestSequence() const noexcept
{
    std::uint64_t latest = 0;
    for (const auto& entry : m_undoStack)
    {
        latest = (std::max)(latest, entry.sequence);
    }
    for (const auto& entry : m_redoStack)
    {
        latest = (std::max)(latest, entry.sequence);
    }
    return latest;
}

void ActionManager::Clear()
{
    if (this == g_sceneActions)
    {
        Layout::ResetAutoApply();
    }
    m_undoStack.clear();
    m_redoStack.clear();
    m_historyBytes = 0;
    ++m_serial;
}

void ActionManager::ClearBefore(std::uint64_t sequence)
{
    auto remove = [sequence, this](std::vector<HistoryEntry>& stack)
    {
        for (auto it = stack.begin(); it != stack.end();)
        {
            if (it->sequence < sequence)
            {
                m_historyBytes -= it->action->MemoryUsage();
                it = stack.erase(it);
            }
            else
            {
                ++it;
            }
        }
    };

    remove(m_undoStack);
    remove(m_redoStack);
    ++m_serial;
}

bool ActionHistoryCanUndo()
{
    return LatestUndoManager() != nullptr;
}

bool ActionHistoryCanRedo()
{
    return LatestRedoManager() != nullptr;
}

void ActionHistoryUndo()
{
    if (!FinishAutoApply())
    {
        return;
    }
    if (ActionManager* manager = LatestUndoManager())
    {
        manager->Undo();
    }
}

void ActionHistoryRedo()
{
    if (!FinishAutoApply())
    {
        return;
    }
    if (ActionManager* manager = LatestRedoManager())
    {
        manager->Redo();
    }
}

void ActionHistoryHandleInput()
{
    if (!g_freeCamera || !g_freeCamera->m_active || ImGui::GetIO().WantCaptureKeyboard || ImGui::IsAnyItemActive()
        || SampTextInputActive() || !(GetAsyncKeyState(VK_CONTROL) & 0x8000))
    {
        return;
    }

    if (Utils::IsKeyPressedOnce('Z'))
    {
        ActionHistoryUndo();
    }
    else if (Utils::IsKeyPressedOnce('Y'))
    {
        ActionHistoryRedo();
    }
}

void ActionHistoryClearForNewProject()
{
    const std::uint64_t lastWorkspaceAction = g_sceneActions ? g_sceneActions->LatestSequence() : 0;
    if (g_sceneActions)
    {
        g_sceneActions->Clear();
    }
    if (g_userActions && lastWorkspaceAction != 0)
    {
        g_userActions->ClearBefore(lastWorkspaceAction);
    }
}
