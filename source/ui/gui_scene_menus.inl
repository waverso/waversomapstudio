// Source Repository: https://github.com/waverso/waversomapstudio

static bool IsValidAddParent(int index)
{
    return g_scene && index >= 0 && index < g_scene->count
        && (g_scene->items[index].type == SceneItemNormal || g_scene->items[index].type == SceneItemRace
            || g_scene->items[index].type == SceneItemCurve);
}

static int FindSingleSelectedAddParent()
{
    if (!g_scene)
    {
        return -1;
    }

    int selectedCount = 0;
    int parent = -1;
    for (int i = 0; i < g_scene->count; ++i)
    {
        if (!g_scene->items[i].selected)
        {
            continue;
        }
        ++selectedCount;
        if (IsValidAddParent(i))
        {
            parent = i;
        }
        else if (g_scene->items[i].type == SceneItemRaceCheckpoint || g_scene->items[i].type == SceneItemCurveNode)
        {
            parent = g_scene->items[i].parent;
        }
    }
    return selectedCount == 1 && IsValidAddParent(parent) ? parent : -1;
}

static void DrawSceneToolsMenu(int target, SceneTools::DuplicateScope scope)
{
    if (!g_scene || !ImGui::BeginMenu("Tools"))
    {
        return;
    }
    const bool canDetect = target >= -1 && target < g_scene->count && SceneTools::CanDetectDuplicates(*g_scene, scope);
    if (ImGui::MenuItem("Detect Duplicates", nullptr, false, canDetect))
    {
        try
        {
            const auto duplicates = SceneTools::FindDuplicates(*g_scene, scope);
            if (!duplicates.empty())
            {
                Selection::ClearAll();
                for (const int index : duplicates)
                {
                    g_scene->items[index].selected = true;
                    for (int parent = g_scene->items[index].parent; parent >= 0; parent = g_scene->items[parent].parent)
                    {
                        g_scene->items[parent].open = true;
                    }
                }
                GuiRequestOutlinerScrollTo(g_scene->items[duplicates.front()].id);
            }
            char message[64];
            snprintf(message, sizeof(message), "Selected duplicate objects: %zu.", duplicates.size());
            g_toast.ShowInfo(duplicates.empty() ? "No duplicate objects found." : message);
        }
        catch (const std::bad_alloc&)
        {
            g_toast.ShowError("Not enough memory to detect duplicates.");
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        if (!canDetect)
        {
            SetStyledTooltip(scope == SceneTools::DuplicateScope::Scene ? "Requires at least three root items."
                                                                        : "Select multiple items or one parent with children.");
        }
        else
        {
            SetStyledTooltip(scope == SceneTools::DuplicateScope::Scene
                    ? "Select duplicate objects in the scene."
                    : "Select duplicate objects in the selection and its descendants.");
        }
    }
    const bool canCenter = g_sceneActions && Selection::TotalCount() == 1 && SceneTools::CanCenterParent(*g_scene, target);
    for (const auto mode : { SceneTools::CenterParentMode::Bounds, SceneTools::CenterParentMode::Pivot })
    {
        const char* label = mode == SceneTools::CenterParentMode::Bounds ? "Center Parent (Bounds)" : "Center Parent (Pivot)";
        if (ImGui::MenuItem(label, nullptr, false, canCenter))
        {
            try
            {
                if (!SceneTools::CenterParent(target, mode))
                {
                    g_toast.ShowInfo("Parent is already centered.");
                }
            }
            catch (const std::bad_alloc&)
            {
                g_toast.ShowError("Not enough memory to center the parent.");
            }
        }
        if (!canCenter && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            SetStyledTooltip("Select one parent with children.");
        }
    }
    ImGui::EndMenu();
}

static void SetAddModalContext(int parent, const AddPopupContext& context)
{
    g_modal.contextParent = parent;
    g_modal.useMouseRay = context.useMouseRay;
    g_modal.rayOrigin = context.rayOrigin;
    g_modal.rayDir = context.rayDir;
}

static void OutlinerAddPopupItems(int parent, const AddPopupContext& context)
{
    if (parent >= 0 && g_scene->items[parent].type == SceneItemCurve)
    {
        if (ImGui::MenuItem("Curve Node"))
        {
            AddCurveItemToScene(parent, context, true);
        }
        return;
    }
    if (parent >= 0 && g_scene->items[parent].type == SceneItemRace)
    {
        if (ImGui::MenuItem("Race Checkpoint"))
        {
            AddRaceItemToScene(parent, context, true);
        }
        return;
    }
    if (ImGui::MenuItem("Empty Item"))
    {
        AddEmptyToScene(parent, context);
    }

    if (ImGui::MenuItem("Browse Objects"))
    {
        SetAddModalContext(parent, context);
        g_modal.activeType = PendingModalNone;
        OpenObjectBrowser();
    }

    if (ImGui::MenuItem("Browse Vehicles"))
    {
        SetAddModalContext(parent, context);
        g_modal.activeType = PendingModalNone;
        OpenVehicleBrowser();
    }

    if (ImGui::MenuItem("Browse Actor Skins"))
    {
        SetAddModalContext(parent, context);
        g_modal.activeType = PendingModalNone;
        OpenSkinBrowser();
    }
    if (ImGui::MenuItem("Pickup"))
    {
        AddPickupToScene(parent, context);
    }
    if (ImGui::MenuItem("Custom Removal"))
    {
        AddCustomRemoveToScene(parent, context);
    }
    if (ImGui::MenuItem("Box Area"))
    {
        AddBoxAreaToScene(parent, context);
    }
    if (ImGui::MenuItem("Race"))
    {
        AddRaceItemToScene(parent, context, false);
    }
    if (ImGui::MenuItem("Layout"))
    {
        AddLayoutToScene(parent, context);
    }
    if (ImGui::MenuItem("Curve", nullptr, false, g_scene->CanParent(SceneItemCurve, parent)))
    {
        AddCurveItemToScene(parent, context, false);
    }
}

static void DrawAddMenus(int childParent, const AddPopupContext& context)
{
    const bool canAdd = g_scene && g_scene->count < EditorScene::MaxItems;
    if (ImGui::BeginMenu("Add", canAdd))
    {
        OutlinerAddPopupItems(-1, context);
        if (IsValidAddParent(childParent) && g_scene->items[childParent].type == SceneItemRace
            && ImGui::MenuItem("Race Checkpoint"))
        {
            AddRaceItemToScene(childParent, context, true);
        }
        if (IsValidAddParent(childParent) && g_scene->items[childParent].type == SceneItemCurve && ImGui::MenuItem("Curve Node"))
        {
            AddCurveItemToScene(childParent, context, true);
        }
        ImGui::EndMenu();
    }

    const bool canAddChild = canAdd && IsValidAddParent(childParent);
    if (canAddChild && ImGui::BeginMenu("Add As Child", true))
    {
        OutlinerAddPopupItems(childParent, context);
        ImGui::EndMenu();
    }
}

static void DrawOutlinerGlobalMenuItems()
{
    const bool canSelectAll = g_scene && g_scene->count > 0;
    const bool canCollapse = OutlinerHasExpandedParents();
    const bool canExpand = OutlinerHasCollapsedParents();
    const bool hasSelection = Selection::TotalCount() > 0;

    if (ContextMenuItem("Select All", "Ctrl+A", canSelectAll))
    {
        GuiSelectAllOutliner();
    }
    if (ContextMenuItem("Select Parent", "P", GuiCanSelectParentOutliner()))
    {
        GuiSelectParentOutliner();
    }
    if (ImGui::MenuItem("Collapse All", nullptr, false, canCollapse))
    {
        OutlinerCollapseAll();
    }
    if (ImGui::MenuItem("Expand All", nullptr, false, canExpand))
    {
        OutlinerExpandAll();
    }
    if (ImGui::MenuItem("Clear Selection", nullptr, false, hasSelection))
    {
        Selection::ClearAll();
    }
}

static void DrawCurveNodeMenu()
{
    if (!g_scene)
    {
        return;
    }
    int nodes = 0;
    for (int index = 0; index < g_scene->count; ++index)
    {
        nodes += g_scene->items[index].selected && g_scene->data[index].curveNode.HasValue();
    }
    if (nodes > 0
        && ImGui::MenuItem(nodes == 1 ? "Subdivide Node" : "Subdivide Nodes", nullptr, false, Curves::CanSubdivide(*g_scene)))
    {
        Curves::SubdivideSelection();
    }
}

static void DrawSelectionContextMenuItems(int target, SceneTools::DuplicateScope scope, const std::optional<CVector>* scenePoint)
{
    const bool outliner = scenePoint == nullptr;
    const bool validItem = g_scene && target >= 0 && target < g_scene->count;
    const bool hasSelection = Selection::TotalCount() > 0;
    const bool showItemActions = !outliner || target != -1;

    ImGui::Separator();

    if (outliner && showItemActions)
    {
        const bool canRename
            = validItem && g_scene->SelectionCount() <= 1 && g_scene->items[target].type != SceneItemRaceCheckpoint;
        if (ContextMenuItem("Rename", "F2", canRename))
        {
            BeginOutlinerRename(target);
        }
    }
    if (showItemActions)
    {
        const bool canClone = outliner ? validItem && g_scene->CanCloneSelection() : GuiCanCloneUnifiedSelection();
        if (ContextMenuItem("Clone", "Ctrl+C", canClone))
        {
            if (outliner)
            {
                GuiCloneSelection();
            }
            else
            {
                GuiCloneUnifiedSelection();
            }
        }
    }
    DrawSceneToolsMenu(target, scope);
    DrawCurveNodeMenu();

    ImGui::Separator();

    if (outliner)
    {
        DrawOutlinerGlobalMenuItems();
    }
    else
    {
        const bool canSnap = Selection::HasMovableRoots() && !(g_scenePicker && !g_scenePicker->GetSelection().empty());
        if (ImGui::MenuItem("Move To Here", nullptr, false, canSnap && scenePoint->has_value()))
        {
            Selection::MoveToPoint(**scenePoint);
        }
        if (ImGui::MenuItem("Snap To Ground", nullptr, false, canSnap))
        {
            Selection::SnapToGround(false);
        }
        if (ImGui::MenuItem("Align To Normal", nullptr, false, canSnap))
        {
            Selection::SnapToGround(true);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Clear Selection", nullptr, false, hasSelection))
        {
            Selection::ClearAll();
        }
    }

    if (showItemActions)
    {
        ImGui::Separator();
        if (ContextMenuItem("Delete", "Del", outliner ? validItem : hasSelection))
        {
            Selection::DeleteUnifiedSelection();
        }
    }
}
