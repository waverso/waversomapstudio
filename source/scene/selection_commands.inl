// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    bool CreateRemoveItemsForNatives(std::vector<ItemSnapshot>& outSnaps, size_t pendingBytes = 0)
    {
        if (!g_scenePicker || !g_scene)
        {
            return false;
        }
        const auto isLowLodModel = [](int modelId)
        {
            return modelId >= 0 && modelId < CatalogDatabase::MaxModelId
                && (CatalogDatabase::View().models[modelId].flags & CatalogDatabase::ModelFlagLowLod) != 0;
        };
        const auto formatModelName = [&](char* destination, std::size_t capacity, int modelId)
        {
            char modelName[48];
            TextureBrowser::GetModelDisplay(modelId, modelName, sizeof(modelName));
            snprintf(destination, capacity, isLowLodModel(modelId) ? "Remove %s (LOD)" : "Remove %s", modelName);
        };
        const int first = g_scene->count;
        size_t count = 0;
        for (const auto& se : g_scenePicker->GetSelection())
        {
            if (se.eMode == PickMode::Static)
            {
                count += se.nModelIdLOD >= 0 ? 2 : 1;
            }
        }
        count = (std::min)(count, static_cast<size_t>(EditorScene::MaxItems - first));
        if (count == 0)
        {
            return false;
        }
        if (g_sceneActions)
        {
            g_sceneActions->Prepare(pendingBytes + sizeof(CreateItemsAction)
                + (outSnaps.capacity() + outSnaps.size() + count) * sizeof(ItemSnapshot) + count * sizeof(RemoveBuildingData));
        }
        outSnaps.reserve(outSnaps.size() + count);

        for (const auto& se : g_scenePicker->GetSelection())
        {
            if (se.eMode != PickMode::Static)
            {
                continue;
            }

            char itemName[64];
            formatModelName(itemName, sizeof(itemName), se.nModelId);

            const CVector origin = (g_removalManager && se.lastEntity) ? g_removalManager->OriginOf(se.lastEntity) : se.posOrigin;

            const int idx = g_scene->AppendItem(-1, SceneItemRemoveBuilding, itemName);
            if (idx < 0)
            {
                break;
            }
            g_scene->InitItemRemoveBuilding(idx, se.nModelId, false);
            g_scene->data[idx].transform.pos[0] = origin.x;
            g_scene->data[idx].transform.pos[1] = origin.y;
            g_scene->data[idx].transform.pos[2] = origin.z;

            if (se.nModelIdLOD >= 0)
            {
                const CVector lodOrigin
                    = (g_removalManager && se.lastEntityLOD) ? g_removalManager->OriginOf(se.lastEntityLOD) : se.posOriginLOD;

                formatModelName(itemName, sizeof(itemName), se.nModelIdLOD);
                const int lodIdx = g_scene->AppendItem(-1, SceneItemRemoveBuilding, itemName);
                if (lodIdx < 0)
                {
                    break;
                }
                g_scene->InitItemRemoveBuilding(lodIdx, se.nModelIdLOD, false);
                g_scene->data[lodIdx].transform.pos[0] = lodOrigin.x;
                g_scene->data[lodIdx].transform.pos[1] = lodOrigin.y;
                g_scene->data[lodIdx].transform.pos[2] = lodOrigin.z;
            }
        }

        if (g_scene->count == first)
        {
            return false;
        }

        GuiOnItemsAppended(first, g_scene->count);
        for (int i = first; i < g_scene->count; ++i)
        {
            outSnaps.emplace_back();
            g_scene->CaptureItemSnapshot(i, outSnaps.back());
        }
        return true;
    }
}

void Selection::DeleteUnifiedSelection()
{
    if (!g_scene || !g_sceneActions)
    {
        return;
    }
    const int first = g_scene->count;
    const int nextId = g_scene->nextId;
    std::unique_ptr<CompositeAction> composite;
    bool createdRb = false;
    try
    {
        composite = std::make_unique<CompositeAction>();
        std::vector<ItemSnapshot> deleted(0);
        g_scene->CaptureSelectionSnapshots(deleted);
        if (!deleted.empty())
        {
            composite->Add(std::make_unique<DeleteItemsAction>(std::move(deleted)));
        }
        std::vector<ItemSnapshot> removals(0);
        createdRb = CreateRemoveItemsForNatives(removals, composite->MemoryUsage());
        if (createdRb)
        {
            composite->Add(std::make_unique<CreateItemsAction>(std::move(removals), false));
        }
    }
    catch (const std::bad_alloc&)
    {
        g_scene->DiscardAppendedItems(first);
        g_scene->nextId = nextId;
        GuiShowToast("Deletion canceled: not enough memory to prepare undo.", true);
        return;
    }
    if (composite->Empty())
    {
        return;
    }
    if (!g_sceneActions->Execute(std::move(composite), true))
    {
        g_scene->DiscardAppendedItems(first);
        g_scene->nextId = nextId;
        return;
    }
    if (createdRb)
    {
        g_scenePicker->ClearSelection();
    }
}

void Selection::MoveToPoint(const CVector& point)
{
    if (!g_scene || !g_sceneActions || !g_gizmos)
    {
        return;
    }

    CVector handle;
    if (Curves::GetHandlePosition(handle))
    {
        Curves::MoveHandleTo(point);
        return;
    }

    int roots[EditorScene::MaxItems];
    const int movableCount = CollectMovableRoots(roots, EditorScene::MaxItems);
    if (movableCount == 0)
    {
        return;
    }

    const CVector delta = point - g_gizmos->GetGizmoPosition();
    if (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z <= 0.000001f)
    {
        return;
    }

    std::unique_ptr<TransformEditAction> action;
    try
    {
        g_sceneActions->Prepare(sizeof(TransformEditAction) + movableCount * sizeof(TransformEditAction::Entry));
        action = std::make_unique<TransformEditAction>(roots, movableCount);
    }
    catch (const std::bad_alloc&)
    {
        GuiShowToast("Not enough memory to start the transform.", true);
        return;
    }

    for (int i = 0; i < movableCount; ++i)
    {
        const int idx = roots[i];
        if (g_objectRuntime)
        {
            const ItemData& data = g_scene->data[idx];
            if (data.model.HasValue() && ModelChecks::IsEscalatorBaseModel(data.model.Get().modelId))
            {
                ObjectRuntimeState& runtime = g_objectRuntime->Runtime(idx);
                if (runtime.obj)
                {
                    runtime.obj->RemoveEscalatorsForEntity();
                }
            }
        }

        const EditorScene::WorldTransform& world = g_scene->world[idx];
        g_scene->SetWorldTransform(idx, world.pos + delta, world.eulerDeg);
    }

    g_scene->UpdateWorldTransforms();

    if (g_objectRuntime)
    {
        for (int i = 0; i < movableCount; ++i)
        {
            const int idx = roots[i];
            const ItemData& data = g_scene->data[idx];
            if (data.model.HasValue() && ModelChecks::IsEscalatorBaseModel(data.model.Get().modelId))
            {
                ObjectRuntimeState& runtime = g_objectRuntime->Runtime(idx);
                if (runtime.obj)
                {
                    runtime.obj->CreateEffects();
                }
            }
        }
    }

    if (action->CaptureAfter())
    {
        g_sceneActions->Execute(std::move(action));
    }
}

void Selection::SnapToGround(bool alignToNormal)
{
    if (!g_scene || !g_sceneActions || !g_objectRuntime)
    {
        return;
    }

    g_scene->UpdateWorldTransforms();

    CVector handle;
    if (Curves::GetHandlePosition(handle))
    {
        const auto skip = [](CEntity* entity)
        {
            const int index = entity ? LookupEditorItemIndex(entity) : -1;
            return index >= 0 && (g_scene->items[index].selected || g_scene->HasSelectedAncestor(index));
        };
        const auto ceiling
            = EntityRay::Cast(handle, { 0, 0, 1 }, 1.0f, EntityRay::IgnoreHidden, EntityRay::GeometricFrontFaces, skip);
        const float lift = (std::max)(0.0f, (ceiling ? ceiling.fHitDistance : 1.0f) - 0.01f);
        const auto hit = EntityRay::Cast(
            handle + CVector(0, 0, lift), { 0, 0, -1 }, 500.0f, EntityRay::IgnoreHidden, EntityRay::GeometricOnly, skip);
        if (!hit)
        {
            GuiShowToast("No ground surface found.", true);
        }
        else if (alignToNormal)
        {
            Curves::AlignHandleTo(hit.vecHitNormal);
        }
        else
        {
            Curves::MoveHandleTo(hit.vecHitPos);
        }
        return;
    }

    int roots[EditorScene::MaxItems];
    const int movableCount = CollectMovableRoots(roots, EditorScene::MaxItems);
    if (movableCount == 0)
    {
        return;
    }

    bool affected[EditorScene::MaxItems] {};
    for (int root = 0; root < movableCount; ++root)
    {
        affected[roots[root]] = true;
    }
    int indices[EditorScene::MaxItems];
    int affectedCount = 0;
    for (int index : g_scene->DisplayOrder())
    {
        if (g_scene->ComposesWithParent(index) && affected[g_scene->items[index].parent])
        {
            affected[index] = true;
        }
        if (affected[index])
        {
            indices[affectedCount++] = index;
        }
    }

    for (int entry = 0; entry < affectedCount; ++entry)
    {
        const auto& actor = g_scene->data[indices[entry]].actor;
        if (actor.HasValue() && SkinBrowser::GetModel(actor.Get().skinId).height <= 0.0f)
        {
            GuiShowToast("Wait for the actor skin to load before snapping to ground.", true);
            return;
        }
        const auto& pickup = g_scene->data[indices[entry]].pickup;
        if (pickup.HasValue() && PickupRuntime::GetVisualScale(pickup.Get().modelId) <= 0.0f)
        {
            GuiShowToast("Wait for the pickup model to load before snapping to ground.", true);
            return;
        }
    }

    int oldest = roots[0];
    unsigned int bestStamp = UINT_MAX;
    for (int r = 0; r < movableCount; ++r)
    {
        const unsigned int stamp = SelStampForId(g_scene->items[roots[r]].id);
        if (stamp != 0 && stamp < bestStamp)
        {
            bestStamp = stamp;
            oldest = roots[r];
        }
    }

    auto forEachSupportPoint = [&](int index, auto&& callback, float clearance = PickupPosition::MinGroundClearance)
    {
        const auto& data = g_scene->data[index];
        const auto& world = g_scene->world[index];
        if (data.raceCheckpoint.HasValue() || (data.removeBuilding.HasValue() && data.removeBuilding.Get().custom))
        {
            callback(world.pos);
        }
        else
        {
            Bounds::ForEachItemCorner(*g_scene, index, world, false, callback, clearance, true);
        }
    };

    CVector mn(FLT_MAX, FLT_MAX, FLT_MAX), mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int r = 0; r < affectedCount; ++r)
    {
        forEachSupportPoint(indices[r],
            [&](const CVector& p)
            {
                mn.x = std::min(mn.x, p.x);
                mn.y = std::min(mn.y, p.y);
                mn.z = std::min(mn.z, p.z);
                mx.x = std::max(mx.x, p.x);
                mx.y = std::max(mx.y, p.y);
                mx.z = std::max(mx.z, p.z);
            });
    }
    if (mn.x > mx.x)
    {
        return;
    }

    const CVector bottom(mn.x + (mx.x - mn.x) * 0.5f, mn.y + (mx.y - mn.y) * 0.5f, mn.z);

    CVector origin(0.0f, 0.0f, 0.0f);
    for (int r = 0; r < movableCount; ++r)
    {
        origin += g_scene->world[roots[r]].pos;
    }
    origin *= 1.0f / static_cast<float>(movableCount);
    if (affectedCount > movableCount)
    {
        origin = mn + (mx - mn) * 0.5f;
    }

    CVector dir(0.0f, 0.0f, -1.0f);

    auto IsSkippedEntity = [](CEntity* ent) -> bool
    {
        if (!ent || !g_objectRuntime || !g_scene)
        {
            return false;
        }
        const int itemIdx = LookupEditorItemIndex(ent);
        return itemIdx >= 0 && (g_scene->items[itemIdx].selected || g_scene->HasSelectedAncestor(itemIdx));
    };

    const CVector inverseDir = CVector(-dir.x, -dir.y, -dir.z);
    const float inverseDistance = std::max(1.0f, mx.z - origin.z + 0.01f);
    const EntityRay::RayResult inverseHit = EntityRay::Cast(
        origin, inverseDir, inverseDistance, EntityRay::IgnoreHidden, EntityRay::GeometricFrontFaces, IsSkippedEntity);
    const float inverseTravel = inverseHit ? inverseHit.fHitDistance : inverseDistance;
    const float safeTravel = inverseTravel - 0.01f;
    const CVector safeOrigin = safeTravel > 0.0f ? origin + inverseDir * safeTravel : origin;

    const float groundDistance = std::max(500.0f, safeOrigin.z - mn.z + 500.0f);
    const EntityRay::RayResult hit
        = EntityRay::Cast(safeOrigin, dir, groundDistance, EntityRay::IgnoreHidden, EntityRay::GeometricOnly, IsSkippedEntity);
    if (!hit || hit.fHitDistance >= FLT_MAX)
    {
        GuiShowToast("No ground surface found.", true);
        return;
    }

    std::vector<DragItem> movedItems;
    std::unique_ptr<TransformEditAction> action;
    try
    {
        g_sceneActions->Prepare(
            sizeof(TransformEditAction) + movableCount * (sizeof(TransformEditAction::Entry) + sizeof(DragItem)));
        movedItems.reserve(movableCount);
        action = std::make_unique<TransformEditAction>(roots, movableCount);
    }
    catch (const std::bad_alloc&)
    {
        GuiShowToast("Not enough memory to start the transform.", true);
        return;
    }
    for (int r = 0; r < movableCount; ++r)
    {
        const int idx = roots[r];

        const ItemData& dd = g_scene->data[idx];
        if (dd.model.HasValue() && ModelChecks::IsEscalatorBaseModel(dd.model.Get().modelId))
        {
            ObjectRuntimeState& rt = g_objectRuntime->Runtime(idx);
            if (rt.obj)
            {
                rt.obj->RemoveEscalatorsForEntity();
            }
        }

        DragItem di;
        di.id = g_scene->items[idx].id;
        di.startPos = g_scene->world[idx].pos;
        di.startRot = g_scene->world[idx].rot;
        movedItems.push_back(di);
    }

    D3DXQUATERNION qDelta(0.0f, 0.0f, 0.0f, 1.0f);
    Utils::RotationBasis rotDelta;
    rotDelta.right = CVector(1.0f, 0.0f, 0.0f);
    rotDelta.forward = CVector(0.0f, 1.0f, 0.0f);
    rotDelta.up = CVector(0.0f, 0.0f, 1.0f);

    if (alignToNormal)
    {
        const CVector upW = Utils::TransformDirection(g_scene->world[oldest].rot, CVector(0.0f, 0.0f, 1.0f));
        D3DXVECTOR3 up(upW.x, upW.y, upW.z);
        D3DXVECTOR3 n(hit.vecHitNormal.x, hit.vecHitNormal.y, hit.vecHitNormal.z);
        float dotV = D3DXVec3Dot(&up, &n);
        dotV = std::max(-1.0f, std::min(1.0f, dotV));

        if (dotV < 0.99995f)
        {
            D3DXVECTOR3 axis;
            if (dotV > -0.99995f)
            {
                D3DXVec3Cross(&axis, &up, &n);
            }
            else
            {
                const D3DXVECTOR3 ref = (fabsf(up.z) < 0.9f) ? D3DXVECTOR3(0.f, 0.f, 1.f) : D3DXVECTOR3(1.f, 0.f, 0.f);
                D3DXVec3Cross(&axis, &up, &ref);
            }
            const float axisLen = D3DXVec3Length(&axis);
            if (axisLen > 1e-8f)
            {
                axis /= axisLen;
                D3DXQuaternionRotationAxis(&qDelta, &axis, acosf(dotV));
                rotDelta = QuatToRot(qDelta);
            }
        }
    }

    for (const auto& di : movedItems)
    {
        const int idx = g_scene->FindIndexById(di.id);
        if (idx < 0)
        {
            continue;
        }

        CVector newPos;
        float newEuler[3];
        if (alignToNormal)
        {
            newPos = bottom + Utils::TransformDirection(rotDelta, di.startPos - bottom);
            newEuler[0] = newEuler[1] = newEuler[2] = 0.0f;
            const Utils::RotationBasis newRot = Utils::ComposeRotationBases(rotDelta, di.startRot);
            Utils::BasisToEulerDegrees(newRot, newEuler);
        }
        else
        {
            newPos = di.startPos + (hit.vecHitPos - bottom);
            for (int a = 0; a < 3; ++a)
            {
                newEuler[a] = g_scene->world[idx].eulerDeg[a];
            }
        }
        g_scene->SetWorldTransform(idx, newPos, newEuler);
    }

    g_scene->UpdateWorldTransforms();

    if (alignToNormal)
    {
        const CVector n = hit.vecHitNormal;

        float minS = FLT_MAX;
        for (int entry = 0; entry < affectedCount; ++entry)
        {
            forEachSupportPoint(indices[entry],
                [&](const CVector& p)
                {
                    minS = std::min(minS, Utils::Dot3(p - hit.vecHitPos, n));
                });
        }

        if (minS != FLT_MAX && fabsf(minS) > 0.001f)
        {
            const CVector push(n.x * -minS, n.y * -minS, n.z * -minS);
            for (const auto& di : movedItems)
            {
                const int idx = g_scene->FindIndexById(di.id);
                if (idx < 0)
                {
                    continue;
                }
                g_scene->SetWorldTransform(idx, g_scene->world[idx].pos + push, g_scene->world[idx].eulerDeg);
            }
        }
        g_scene->UpdateWorldTransforms();
    }

    for (const auto& item : movedItems)
    {
        const int index = g_scene->FindIndexById(item.id);
        if (index < 0 || !g_scene->data[index].pickup.HasValue())
        {
            continue;
        }
        float bottomOffset = FLT_MAX;
        forEachSupportPoint(
            index,
            [&](const CVector& corner)
            {
                bottomOffset = std::min(bottomOffset, corner.z);
            },
            0.0f);
        CVector position = g_scene->world[index].pos;
        bottomOffset -= position.z;
        if (!PickupPosition::SnapHeight(position.z, hit.vecHitPos.z, bottomOffset, position.z))
        {
            action->Undo();
            g_scene->UpdateWorldTransforms();
            GuiShowToast("The pickup cannot be placed above this ground within the GTA coordinate range.", true);
            return;
        }
        g_scene->SetWorldTransform(index, position, g_scene->world[index].eulerDeg);
    }
    for (const auto& di : movedItems)
    {
        const int idx = g_scene->FindIndexById(di.id);
        if (idx < 0)
        {
            continue;
        }
        const ItemData& dd = g_scene->data[idx];
        if (dd.model.HasValue() && ModelChecks::IsEscalatorBaseModel(dd.model.Get().modelId))
        {
            ObjectRuntimeState& rt = g_objectRuntime->Runtime(idx);
            if (rt.obj)
            {
                rt.obj->CreateEffects();
            }
        }
    }

    g_scene->UpdateWorldTransforms();
    if (action->CaptureAfter())
    {
        g_sceneActions->Execute(std::move(action));
    }
}
