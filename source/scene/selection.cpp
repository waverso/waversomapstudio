// Source Repository: https://github.com/waverso/waversomapstudio

#include "selection.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../shared/model_checks.h"
#include "../utils/utils.h"
#include "CEntity.h"
#include "CModelInfo.h"
#include "CObject.h"
#include "actions.h"
#include "../actors/actor_runtime.h"
#include "../layout/curves.h"
#include "../catalog/database.h"
#include "entity_raycast.h"
#include "free_camera.h"
#include "gizmos.h"
#include "../ui/gui_api.h"
#include "../layout/layout.h"
#include "../objects/model_constraints.h"
#include "../rendering/model_preview.h"
#include "../objects/object_browser.h"
#include "../objects/object_runtime.h"
#include "picker.h"
#include "../pickups/pickup_position.h"
#include "../pickups/pickup_runtime.h"
#include "../objects/removal_manager.h"
#include "../hooks/samp.h"
#include "scene.h"
#include "../actors/skin_browser.h"
#include "../textures/texture_browser.h"
#include "../vehicles/vehicle_runtime.h"

namespace
{
    GizmoPivot g_pivotMode = GizmoPivot::Pivot;
    bool g_drawHighlight = true;
    const EditorScene* g_renderSelectionScene = nullptr;
    int g_renderSelectionCount = 0;
    std::array<bool, EditorScene::MaxItems> g_renderSelection;

    struct DragItem
    {
        int id = 0;
        CVector startPos = CVector(0.0f, 0.0f, 0.0f);
        Utils::RotationBasis startRot;
    };

    bool g_wasDragging = false;
    bool g_dragApplied = false;
    CVector g_dragGizmoStart = CVector(0.0f, 0.0f, 0.0f);
    std::vector<DragItem> g_dragItems;
    std::unique_ptr<TransformEditAction> g_dragAction;

    bool g_wasRadiusDragging = false;
    int g_radiusItemId = -1;
    ItemData g_radiusBefore;
    bool g_wasBoxAreaDragging = false;
    int g_boxAreaItemId = -1;
    ItemData g_boxAreaBefore;

    unsigned int g_selStampNext = 1;
    std::unordered_map<int, unsigned int> g_selStamps;

    unsigned int SelStampForId(int id)
    {
        const auto found = g_selStamps.find(id);
        return found == g_selStamps.end() ? 0 : found->second;
    }

    void UpdateSelectionStamps()
    {
        for (auto stamp = g_selStamps.begin(); stamp != g_selStamps.end();)
        {
            const int idx = g_scene->FindIndexById(stamp->first);
            if (idx < 0 || !g_scene->items[idx].selected)
            {
                stamp = g_selStamps.erase(stamp);
            }
            else
            {
                ++stamp;
            }
        }
        for (int i = 0; i < g_scene->count; ++i)
        {
            if (g_scene->items[i].selected && g_selStamps.try_emplace(g_scene->items[i].id, g_selStampNext).second)
            {
                ++g_selStampNext;
            }
        }
    }
}

#include "selection_bounds.inl"
#include "selection_gizmos.inl"
#include "selection_commands.inl"

int Selection::TotalCount()
{
    int n = g_scene ? g_scene->SelectionCount() : 0;
    if (g_scenePicker)
    {
        n += (int)g_scenePicker->GetSelection().size();
    }
    return n;
}

bool Selection::HasMovableRoots()
{
    if (!g_scene)
    {
        return false;
    }
    int probe[1];
    return CollectMovableRoots(probe, 1) > 0;
}

void Selection::ClearAll()
{
    if (g_scenePicker)
    {
        g_scenePicker->ClearSelection();
    }
    GuiClearSceneSelection();
}

GizmoPivot Selection::GetPivotMode()
{
    return g_pivotMode;
}
void Selection::TogglePivotMode()
{
    g_pivotMode = (g_pivotMode == GizmoPivot::Pivot) ? GizmoPivot::Center : GizmoPivot::Pivot;
}

bool Selection::GetDrawHighlight()
{
    return g_drawHighlight;
}
void Selection::SetDrawHighlight(bool draw)
{
    g_drawHighlight = draw;
}

void Selection::BeginRenderSelection()
{
    g_renderSelectionScene = nullptr;
    if (!g_scene || !g_freeCamera || !g_freeCamera->m_active || !g_drawHighlight)
    {
        return;
    }
    const auto& order = g_scene->DisplayOrder();
    if (order.size() != static_cast<std::size_t>(g_scene->count))
    {
        return;
    }
    for (int index : order)
    {
        const auto& item = g_scene->items[index];
        g_renderSelection[index] = item.selected || (item.parent >= 0 && g_renderSelection[item.parent]);
    }
    g_renderSelectionCount = g_scene->count;
    g_renderSelectionScene = g_scene;
}

void Selection::EndRenderSelection()
{
    g_renderSelectionScene = nullptr;
}

bool Selection::IsEntitySelected(CEntity* pEntity)
{
    if (g_scenePicker && g_scenePicker->IsSelected(pEntity))
    {
        return true;
    }
    if (g_objectRuntime && g_scene)
    {
        const int idx = LookupEditorItemIndex(pEntity);
        if (idx >= 0 && idx < g_scene->count)
        {
            if (g_renderSelectionScene == g_scene && g_renderSelectionCount == g_scene->count)
            {
                return g_renderSelection[idx];
            }
            return g_scene->items[idx].selected || g_scene->HasSelectedAncestor(idx);
        }
    }
    return false;
}

bool Selection::IsEntityHighlighted(CEntity* pEntity)
{
    if (!g_drawHighlight)
    {
        return false;
    }
    if (!g_freeCamera || !g_freeCamera->m_active)
    {
        return false;
    }
    return IsEntitySelected(pEntity);
}

void Selection::HandleInput(ImGuiIO& io)
{
    if (!g_freeCamera || !g_freeCamera->m_active)
    {
        return;
    }
    if (io.WantCaptureKeyboard || SampTextInputActive())
    {
        return;
    }

    if (io.KeyCtrl)
    {
        if (Utils::IsKeyPressedOnce('C'))
        {
            GuiCloneUnifiedSelection();
            return;
        }
        if (Utils::IsKeyPressedOnce('A'))
        {
            GuiSelectAllOutliner();
            return;
        }
    }

    if (!Utils::IsKeyPressedOnce(VK_DELETE))
    {
        return;
    }

    DeleteUnifiedSelection();
}
