// Source Repository: https://github.com/waverso/waversomapstudio

static void GuiDeleteSelectionNoUndo()
{
    static int remap[EditorScene::MaxItems];
    int oldCount = 0;
    g_scene->DeleteSelection(remap, oldCount);
    g_itemWidgets.Prune();
    if (g_objectRuntime)
    {
        g_objectRuntime->OnSceneRemap(remap, oldCount);
    }
    if (g_vehicleRuntime)
    {
        g_vehicleRuntime->Update();
    }
    if (g_pickupRuntime)
    {
        g_pickupRuntime->Update();
    }
    if (g_actorRuntime)
    {
        g_actorRuntime->Update();
    }

    g_outlinerUi.renameIndex = -1;
    g_outlinerUi.focusRename = false;
    g_outlinerUi.popupIndex = -1;
    g_outlinerUi.dragIndex = -1;
    g_outlinerUi.dragActive = false;
}

static void CommitCreatedSelection(bool selectOnApply = true, int selectedItemId = -1)
{
    if (!g_sceneActions || !g_scene)
    {
        return;
    }
    std::vector<ItemSnapshot> snaps(0);
    g_scene->CaptureSelectionSnapshots(snaps);
    if (!snaps.empty())
    {
        g_sceneActions->Execute(std::make_unique<CreateItemsAction>(std::move(snaps), selectOnApply, selectedItemId));
    }
}

struct SpawnGroundHit
{
    CVector point;
    bool found = false;
};

static CVector SpawnGroundAnchor(int modelId, float fallbackRadius = 0.0f)
{
    if (modelId < 0 || modelId >= CatalogDatabase::MaxModelId)
    {
        return CVector(0.0f, 0.0f, -fallbackRadius);
    }

    const CatalogDatabase::ObjectModelMeta* meta = CatalogDatabase::ObjectMeta(static_cast<std::uint16_t>(modelId));
    if (!meta)
    {
        return CVector(0.0f, 0.0f, -fallbackRadius);
    }

    const bool zeroBounds = meta->boundsMin[0] == 0.0f && meta->boundsMin[1] == 0.0f && meta->boundsMin[2] == 0.0f
        && meta->boundsMax[0] == 0.0f && meta->boundsMax[1] == 0.0f && meta->boundsMax[2] == 0.0f;
    if (zeroBounds)
    {
        return CVector(0.0f, 0.0f, -fallbackRadius);
    }

    return CVector(
        (meta->boundsMin[0] + meta->boundsMax[0]) * 0.5f, (meta->boundsMin[1] + meta->boundsMax[1]) * 0.5f, meta->boundsMin[2]);
}

static CVector ComputeSpawnPosFromRay(const CVector& rayOrigin, const CVector& rayDir, float boundRadius,
    const CVector& groundAnchor, SpawnGroundHit* groundHit = nullptr, float minRayDistance = 0.0f)
{
    const float maxDist = std::max(40.0f + boundRadius * 4.0f, minRayDistance);

    CVector spawnPos = rayOrigin + rayDir * ImMin(15.0f + boundRadius, maxDist);
    float bestDist = FLT_MAX;
    bool hasHit = false;
    CColPoint colPoint;
    CEntity* hitEntity = nullptr;
    if (CWorld::ProcessLineOfSight(
            rayOrigin, rayOrigin + rayDir * maxDist, colPoint, hitEntity, true, false, false, true, true, false, false, false))
    {
        spawnPos = colPoint.m_vecPoint;
        bestDist = (colPoint.m_vecPoint - rayOrigin).Magnitude();
        hasHit = true;
    }
    const EntityRay::RayResult rayHit
        = EntityRay::Cast(rayOrigin, rayDir, maxDist, EntityRay::IgnoreHidden, EntityRay::GeometricOnly);
    if (rayHit && rayHit.fHitDistance < bestDist)
    {
        spawnPos = rayHit.vecHitPos;
        hasHit = true;
    }
    if (groundHit)
    {
        groundHit->point = spawnPos;
        groundHit->found = hasHit;
    }
    return hasHit ? spawnPos - groundAnchor : spawnPos;
}

static bool ResolveSceneContextPoint(const ImVec2& screenPos, CVector& outPoint)
{
    if (!g_freeCamera)
    {
        return false;
    }

    CVector rayOrigin, rayDir;
    if (!g_freeCamera->ScreenToWorldRayForPicking(screenPos.x, screenPos.y, rayOrigin, rayDir))
    {
        return false;
    }

    float bestDistance = FLT_MAX;
    bool hit = false;
    CColPoint colPoint;
    CEntity* hitEntity = nullptr;
    if (CWorld::ProcessLineOfSight(
            rayOrigin, rayOrigin + rayDir * 500.0f, colPoint, hitEntity, true, false, false, true, true, false, false, false))
    {
        outPoint = colPoint.m_vecPoint;
        bestDistance = (outPoint - rayOrigin).Magnitude();
        hit = true;
    }

    const EntityRay::RayResult rayHit
        = EntityRay::Cast(rayOrigin, rayDir, 500.0f, EntityRay::IgnoreHidden, EntityRay::GeometricOnly);
    if (rayHit && rayHit.fHitDistance < bestDistance)
    {
        outPoint = rayHit.vecHitPos;
        hit = true;
    }
    return hit;
}

static CVector ComputeCameraSpawnPos(
    float boundRadius, const CVector& groundAnchor, SpawnGroundHit* groundHit = nullptr, float minRayDistance = 0.0f)
{
    CVector fwd, right, up;
    g_freeCamera->GetCameraVectors(fwd, right, up);
    return ComputeSpawnPosFromRay(g_freeCamera->m_position, fwd, boundRadius, groundAnchor, groundHit, minRayDistance);
}

static size_t SceneCloneItemCount()
{
    size_t needed = 0;
    for (int i = 0; i < g_scene->count; ++i)
    {
        if (g_scene->items[i].type != SceneItemRemoveBuilding && (g_scene->items[i].selected || g_scene->HasSelectedAncestor(i)))
        {
            ++needed;
        }
    }
    return needed;
}

static bool CheckCloneCapacity(size_t needed)
{
    if (needed == 0)
    {
        return false;
    }
    const int available = EditorScene::MaxItems - g_scene->count;
    if (needed <= static_cast<size_t>(available))
    {
        return true;
    }
    g_toast.ShowError("Cannot clone: the scene item limit would be exceeded.");
    return false;
}

struct CloneState
{
    int count = g_scene->count;
    int nextId = g_scene->nextId;
    int orders[EditorScene::MaxItems];
    bool selected[EditorScene::MaxItems];
    bool committed = false;
    CVector pivot = g_gizmos ? g_gizmos->GetGizmoPosition() : CVector(0.0f, 0.0f, 0.0f);
    Utils::RotationBasis basis;
    bool preserveSelection
        = g_gizmos && (g_gizmos->IsDragging() || g_gizmos->IsRadiusDragging() || g_gizmos->IsBoxAreaDragging());

    CloneState()
    {
        if (g_gizmos && g_utilityPanelState.cloneOffset.space == SceneTools::CloneOffsetSpace::Local)
        {
            basis = g_gizmos->GetGizmoRotation();
        }
        for (int i = 0; i < count; ++i)
        {
            orders[i] = g_scene->items[i].order;
            selected[i] = g_scene->items[i].selected;
        }
    }
    void Commit()
    {
        const auto offset = g_utilityPanelState.cloneOffset.Sample(g_utilityPanelState.cloneOffsetRandom);
        SceneTools::ApplyCloneOffset(*g_scene, count, offset, pivot, basis);
        CommitCreatedSelection(!preserveSelection);
        if (preserveSelection)
        {
            for (int i = 0; i < g_scene->count; ++i)
            {
                g_scene->items[i].selected = i < count && selected[i];
            }
        }
        committed = true;
    }
    ~CloneState()
    {
        if (committed)
        {
            return;
        }
        g_scene->DiscardAppendedItems(count);
        g_scene->nextId = nextId;
        for (int i = 0; i < count; ++i)
        {
            g_scene->items[i].order = orders[i];
            g_scene->items[i].selected = selected[i];
        }
    }
};

static void GuiCloneSelection()
{
    if (!CheckCloneCapacity(SceneCloneItemCount()))
    {
        return;
    }
    CloneState state;
    try
    {
        static int clones[EditorScene::MaxItems];
        g_scene->CloneSelection(clones, state.preserveSelection);
        ResetItemWidgetsRange(state.count, g_scene->count);
        state.Commit();
    }
    catch (const std::bad_alloc&)
    {
        g_toast.ShowError("Not enough memory to clone the selection.");
    }
}

struct ItemEditTracker
{
    int itemId = -1;
    bool wasActive = false;
    int lastSerial = 0;
    ItemData preEdit;
    ItemData lastFrame;
};
static ItemEditTracker g_editTracker;

static void CommitItemEdit(int itemIdx, const ItemData& before)
{
    if (!g_sceneActions)
    {
        return;
    }
    std::vector<ItemDataEditAction::Entry> entries(1);
    entries[0].itemId = g_scene->items[itemIdx].id;
    entries[0].before = before;
    entries[0].after = g_scene->data[itemIdx];
    g_sceneActions->Execute(std::make_unique<ItemDataEditAction>(std::move(entries)));
}

static void UpdateEditTracker()
{
    if (!g_scene || !g_sceneActions)
    {
        return;
    }
    if (InspectionUsesTransactions())
    {
        g_editTracker.itemId = -1;
        g_editTracker.wasActive = false;
        g_editTracker.lastSerial = g_sceneActions->GetSerial();
        return;
    }
    ItemEditTracker& t = g_editTracker;

    if (g_sceneActions->GetSerial() != t.lastSerial)
    {
        t.lastSerial = g_sceneActions->GetSerial();
        const int idx0 = t.itemId >= 0 ? g_scene->FindIndexById(t.itemId) : -1;
        if (idx0 >= 0)
        {
            t.lastFrame = g_scene->data[idx0];
            t.preEdit = t.lastFrame;
        }
        else
        {
            t.itemId = -1;
        }
        t.wasActive = false;
        return;
    }

    const int selIdx = (g_scene->SelectionCount() == 1) ? g_scene->SingleSelectedIndex() : -1;
    const int selId = selIdx >= 0 ? g_scene->items[selIdx].id : -1;

    if (t.itemId != -1 && t.itemId != selId)
    {
        const int oldIdx = g_scene->FindIndexById(t.itemId);
        if (oldIdx >= 0)
        {
            const ItemData& base = t.wasActive ? t.preEdit : t.lastFrame;
            if (base != g_scene->data[oldIdx])
            {
                CommitItemEdit(oldIdx, base);
            }
        }
        t.itemId = -1;
        t.wasActive = false;
        t.lastSerial = g_sceneActions->GetSerial();
    }

    if (selId < 0)
    {
        t.itemId = -1;
        return;
    }

    if (t.itemId != selId)
    {
        t.itemId = selId;
        t.wasActive = false;
        t.lastFrame = g_scene->data[selIdx];
        t.preEdit = t.lastFrame;
        return;
    }

    if (g_gizmos && (g_gizmos->IsDragging() || g_gizmos->IsRadiusDragging() || g_gizmos->IsBoxAreaDragging()))
    {
        t.wasActive = false;
        t.preEdit = g_scene->data[selIdx];
        t.lastFrame = t.preEdit;
        return;
    }

    const bool activeNow = ImGui::IsAnyItemActive();
    const ItemData& cur = g_scene->data[selIdx];

    if (activeNow && !t.wasActive)
    {
        t.preEdit = t.lastFrame;
    }

    if (!activeNow)
    {
        const ItemData& base = t.wasActive ? t.preEdit : t.lastFrame;
        if (base != cur)
        {
            CommitItemEdit(selIdx, base);
            t.lastSerial = g_sceneActions->GetSerial();
        }
        t.preEdit = cur;
    }

    t.wasActive = activeNow;
    t.lastFrame = cur;
}

static void CaptureHierarchyBefore(std::vector<HierarchyEditAction::Entry>& out)
{
    out.clear();
    for (int i = 0; i < g_scene->count; ++i)
    {
        HierarchyEditAction::Entry e;
        e.itemId = g_scene->items[i].id;
        const int p = g_scene->items[i].parent;
        e.beforeParentId = (p >= 0) ? g_scene->items[p].id : -1;
        e.beforeOrder = g_scene->items[i].order;
        e.beforeLocal = g_scene->data[i].transform;
        out.push_back(e);
    }
}

static void CommitHierarchyDiff(std::vector<HierarchyEditAction::Entry>& captured)
{
    if (!g_sceneActions)
    {
        return;
    }
    g_sceneActions->Prepare(sizeof(HierarchyEditAction) + captured.size() * sizeof(HierarchyEditAction::Entry));
    std::vector<HierarchyEditAction::Entry> changed;
    changed.reserve(captured.size());
    for (auto& e : captured)
    {
        const int idx = g_scene->FindIndexById(e.itemId);
        if (idx < 0)
        {
            continue;
        }
        const int p = g_scene->items[idx].parent;
        e.afterParentId = (p >= 0) ? g_scene->items[p].id : -1;
        e.afterOrder = g_scene->items[idx].order;
        e.afterLocal = g_scene->data[idx].transform;

        if (e.afterParentId != e.beforeParentId || e.afterOrder != e.beforeOrder
            || memcmp(&e.afterLocal, &e.beforeLocal, sizeof(TransformData)) != 0)
        {
            changed.push_back(e);
        }
    }
    if (!changed.empty())
    {
        g_sceneActions->Execute(std::make_unique<HierarchyEditAction>(std::move(changed)));
    }
}

static bool IsObjectModelInputValid(int modelId)
{
    return modelId >= 321 && modelId <= 19999 && !VehicleCatalog::IsValid(modelId) && TextureBrowser::ModelExists(modelId);
}

static CVector ComputeAddSpawnPos(const AddPopupContext& context, float boundRadius, const CVector& groundAnchor,
    SpawnGroundHit* groundHit = nullptr, float minRayDistance = 0.0f)
{
    return context.useMouseRay
        ? ComputeSpawnPosFromRay(context.rayOrigin, context.rayDir, boundRadius, groundAnchor, groundHit, minRayDistance)
        : ComputeCameraSpawnPos(boundRadius, groundAnchor, groundHit, minRayDistance);
}

static void FinalizeAddedItem(int idx, int parent)
{
    g_itemWidgets.Reset(idx);
    g_scene->ClearSelection();
    if (g_scenePicker)
    {
        g_scenePicker->ClearSelection();
    }
    g_scene->items[idx].selected = true;
    if (parent >= 0)
    {
        g_scene->items[parent].open = true;
    }
    CommitCreatedSelection(true, g_scene->items[idx].id);
}

static bool AddEmptyToScene(int parent, const AddPopupContext& context)
{
    const int idx = g_scene->AppendItem(parent, SceneItemNormal, "Empty Item");
    if (idx < 0)
    {
        g_toast.ShowError("Item limit reached.");
        return false;
    }

    g_scene->InitItemNormal(idx, false);
    const float zeroRot[3] = { 0.0f, 0.0f, 0.0f };
    g_scene->SetWorldTransform(idx, ComputeAddSpawnPos(context, 50.0f, CVector(0.0f, 0.0f, 0.0f)), zeroRot);
    FinalizeAddedItem(idx, parent);
    return true;
}

static bool AddBoxAreaToScene(int parent, const AddPopupContext& context)
{
    const int idx = g_scene->AppendItem(parent, SceneItemNormal, "Box Area");
    if (idx < 0)
    {
        g_toast.ShowError("Item limit reached.");
        return false;
    }
    g_scene->InitItemNormal(idx, false);
    AddComponentToItem(g_scene->data[idx], ComponentTypeBoxArea);
    const float zeroRot[3] = {};
    g_scene->SetWorldTransform(idx, ComputeAddSpawnPos(context, 50.0f, CVector()), zeroRot);
    FinalizeAddedItem(idx, parent);
    return true;
}

static bool AddLayoutToScene(int parent, const AddPopupContext& context)
{
    const int index = g_scene->AppendItem(parent, SceneItemNormal, "Layout");
    if (index < 0)
    {
        g_toast.ShowError("Item limit reached.");
        return false;
    }
    g_scene->InitItemNormal(index, false);
    AddComponentToItem(g_scene->data[index], ComponentTypeLayout);
    const float rotation[3] = {};
    g_scene->SetWorldTransform(index, ComputeAddSpawnPos(context, 50.0f, CVector()), rotation);
    FinalizeAddedItem(index, parent);
    return true;
}

static bool AddRaceItemToScene(int parent, const AddPopupContext& context, bool checkpoint)
{
    const auto type = checkpoint ? SceneItemRaceCheckpoint : SceneItemRace;
    if (!g_scene->CanParent(type, parent))
    {
        return false;
    }
    const int index = g_scene->AppendItem(parent, type, checkpoint ? "Checkpoint" : "Race");
    if (index < 0)
    {
        g_toast.ShowError("Item limit reached.");
        return false;
    }
    const float rotation[3] = {};
    const CVector position = ComputeAddSpawnPos(context, 50.0f, CVector());
    g_scene->SetWorldTransform(index, position, rotation);
    if (!checkpoint)
    {
        const int child = g_scene->AppendItem(index, SceneItemRaceCheckpoint, "Checkpoint");
        if (child < 0)
        {
            g_scene->DiscardAppendedItems(index);
            g_toast.ShowError("Item limit reached.");
            return false;
        }
        g_scene->SetWorldTransform(child, position, rotation);
        g_itemWidgets.Reset(child);
    }
    g_scene->UpdateRaceNames();
    FinalizeAddedItem(index, parent);
    return true;
}

static bool AddCurveItemToScene(int parent, const AddPopupContext& context, bool node)
{
    const auto type = node ? SceneItemCurveNode : SceneItemCurve;
    if (!g_scene->CanParent(type, parent))
    {
        return false;
    }
    if (EditorScene::MaxItems - g_scene->count < (node ? 1 : 3))
    {
        g_toast.ShowError("Item limit reached.");
        return false;
    }
    g_scene->UpdateWorldTransforms();
    const int first = g_scene->count;
    const int nextId = g_scene->nextId;
    std::array<bool, EditorScene::MaxItems> selected {};
    const bool parentOpen = parent >= 0 && g_scene->items[parent].open;
    for (int index = 0; index < first; ++index)
    {
        selected[index] = g_scene->items[index].selected;
    }
    try
    {
        const int index = g_scene->AppendItem(parent, type, node ? "Node" : "Curve");
        if (index < 0)
        {
            g_toast.ShowError("Item limit reached.");
            return false;
        }
        const float rotation[3] = {};
        CVector position = ComputeAddSpawnPos(context, 50.0f, CVector());
        if (node && !context.useMouseRay)
        {
            int last = -1, previous = -1;
            for (const int child : g_scene->DisplayOrder())
            {
                if (child == index || g_scene->items[child].parent != parent)
                {
                    continue;
                }
                previous = last;
                last = child;
            }
            if (last >= 0)
            {
                position = g_scene->world[last].pos
                    + (previous >= 0 ? g_scene->world[last].pos - g_scene->world[previous].pos
                                     : Utils::TransformDirection(g_scene->world[parent].rot, { 10, 0, 0 }));
            }
        }
        g_scene->SetWorldTransform(index, position, rotation);
        if (!node)
        {
            for (int point = 0; point < 2; ++point)
            {
                const int child = g_scene->AppendItem(index, SceneItemCurveNode, "Node");
                if (child < 0)
                {
                    throw std::bad_alloc();
                }
                g_scene->data[child].transform.pos[0] = point == 0 ? -10.0f : 10.0f;
                g_itemWidgets.Reset(child);
            }
        }
        g_scene->UpdateWorldTransforms();
        FinalizeAddedItem(index, parent);
        return true;
    }
    catch (const std::bad_alloc&)
    {
        g_scene->DiscardAppendedItems(first);
        g_scene->nextId = nextId;
        for (int index = 0; index < first; ++index)
        {
            g_scene->items[index].selected = selected[index];
        }
        if (parent >= 0)
        {
            g_scene->items[parent].open = parentOpen;
        }
        g_toast.ShowError("Not enough memory to create the curve.");
        return false;
    }
}

static bool AddPickupToScene(int parent, const AddPopupContext& context)
{
    const int modelId = PickupData().modelId;
    if (!PickupRuntime::IsValidModel(modelId))
    {
        g_toast.ShowError("Pickup model is unavailable.");
        return false;
    }
    if (!plugin::Command<plugin::Commands::HAS_MODEL_LOADED>(modelId))
    {
        plugin::Command<plugin::Commands::REQUEST_MODEL>(modelId);
        plugin::Command<plugin::Commands::LOAD_ALL_MODELS_NOW>();
    }
    const float scale = PickupRuntime::GetVisualScale(modelId);
    if (!plugin::Command<plugin::Commands::HAS_MODEL_LOADED>(modelId) || !std::isfinite(scale) || scale <= 0.0f)
    {
        g_toast.ShowError("Pickup model is unavailable.");
        return false;
    }
    const auto model = ObjectBrowser::GetModel(modelId);
    CVector groundAnchor = SpawnGroundAnchor(modelId, model.radius) * scale;
    const float bottomOffset = groundAnchor.z;
    groundAnchor.z -= PickupPosition::MinGroundClearance;
    SpawnGroundHit groundHit;
    CVector position = ComputeAddSpawnPos(context, std::max(0.1f, model.radius * scale), groundAnchor, &groundHit, 500.0f);
    if (groundHit.found && !PickupPosition::SnapHeight(position.z, groundHit.point.z, bottomOffset, position.z))
    {
        g_toast.ShowError("The pickup cannot be placed above this ground.");
        return false;
    }
    const float coordinates[3] = { position.x, position.y, position.z };
    if (!PickupPosition::ValidInput(coordinates))
    {
        g_toast.ShowError("The pickup position is outside the GTA coordinate range.");
        return false;
    }
    char name[64];
    TextureBrowser::GetModelDisplay(modelId, name, sizeof(name));
    const int index = g_scene->AppendItem(parent, SceneItemNormal, name);
    if (index < 0)
    {
        g_toast.ShowError("Item limit reached.");
        return false;
    }
    g_scene->InitItemNormal(index, false);
    AddComponentToItem(g_scene->data[index], ComponentTypePickup);
    g_scene->items[index].searchModelId = modelId;
    const float rotation[3] = {};
    g_scene->SetWorldTransform(index, position, rotation);
    FinalizeAddedItem(index, parent);
    return true;
}

static bool AddCustomRemoveToScene(int parent, const AddPopupContext& context)
{
    const int idx = g_scene->AppendItem(parent, SceneItemRemoveBuilding, "Custom Removal");
    if (idx < 0)
    {
        g_toast.ShowError("Item limit reached.");
        return false;
    }

    g_scene->InitItemRemoveBuilding(idx, -1, true);
    const float zeroRot[3] = { 0.0f, 0.0f, 0.0f };
    g_scene->SetWorldTransform(idx, ComputeAddSpawnPos(context, 50.0f, CVector(0.0f, 0.0f, 0.0f)), zeroRot);
    FinalizeAddedItem(idx, parent);
    return true;
}

static bool AddObjectToScene(int modelId)
{
    if (!IsObjectModelInputValid(modelId))
    {
        g_toast.ShowError("Invalid object model ID.");
        return false;
    }

    if (!plugin::Command<plugin::Commands::HAS_MODEL_LOADED>(modelId))
    {
        plugin::Command<plugin::Commands::REQUEST_MODEL>(modelId);
        plugin::Command<plugin::Commands::LOAD_ALL_MODELS_NOW>();
    }

    float boundRadius = 2.0f;
    const ObjectBrowser::ModelInfo browserModel = ObjectBrowser::GetModel(modelId);
    if (browserModel.modelId >= 0 && browserModel.radius > 0.0f)
    {
        boundRadius = browserModel.radius;
    }
    else if (CBaseModelInfo* mi = CModelInfo::GetModelInfo(modelId); mi && mi->m_pColModel)
    {
        boundRadius = mi->m_pColModel->m_boundSphere.m_fRadius;
    }

    char itemName[64];
    TextureBrowser::GetModelDisplay(modelId, itemName, sizeof(itemName));
    EditorScene& sc = *g_scene;
    const int idx = sc.AppendItem(g_modal.contextParent, SceneItemNormal, itemName);
    if (idx < 0)
    {
        g_toast.ShowError("Item limit reached.");
        return false;
    }

    sc.InitItemNormal(idx, true, modelId);
    const float zeroRot[3] = { 0.0f, 0.0f, 0.0f };
    const CVector groundAnchor = SpawnGroundAnchor(modelId);
    const CVector spawnPos = g_modal.useMouseRay
        ? ComputeSpawnPosFromRay(g_modal.rayOrigin, g_modal.rayDir, boundRadius, groundAnchor)
        : ComputeCameraSpawnPos(boundRadius, groundAnchor);
    sc.SetWorldTransform(idx, spawnPos, zeroRot);
    FinalizeAddedItem(idx, g_modal.contextParent);
    return true;
}
