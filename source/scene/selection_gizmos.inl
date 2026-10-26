// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    bool IsItemMovable(int idx)
    {
        const SceneItem& it = g_scene->items[idx];
        if (it.type == SceneItemNormal || it.type == SceneItemRace || it.type == SceneItemRaceCheckpoint
            || it.type == SceneItemCurve || it.type == SceneItemCurveNode)
        {
            return true;
        }
        return it.type == SceneItemRemoveBuilding && g_scene->data[idx].removeBuilding.HasValue()
            && g_scene->data[idx].removeBuilding.Get().custom;
    }

    int CollectMovableRoots(int* out, int maxOut)
    {
        std::array<unsigned char, EditorScene::MaxItems> inherited;
        int firstParent = -1;
        bool firstSelected = false;
        bool prepared = false;
        int n = 0;
        for (int i = 0; i < g_scene->count && n < maxOut; ++i)
        {
            if (!g_scene->items[i].selected)
            {
                continue;
            }
            const int parent = g_scene->items[i].parent;
            if (parent >= 0 && parent < g_scene->count)
            {
                if (g_scene->items[parent].selected)
                {
                    continue;
                }
                if (!prepared)
                {
                    if (firstParent < 0)
                    {
                        firstParent = parent;
                        firstSelected = g_scene->HasSelectedAncestor(i);
                    }
                    if (firstParent == parent)
                    {
                        if (firstSelected)
                        {
                            continue;
                        }
                        if (IsItemMovable(i))
                        {
                            out[n++] = i;
                        }
                        continue;
                    }
                    inherited.fill(0);
                    inherited[firstParent] = firstSelected ? 2 : 1;
                    prepared = true;
                }
                if (!inherited[parent])
                {
                    int ancestor = parent;
                    while (
                        ancestor >= 0 && ancestor < g_scene->count && !inherited[ancestor] && !g_scene->items[ancestor].selected)
                    {
                        ancestor = g_scene->items[ancestor].parent;
                    }
                    const unsigned char selected = ancestor >= 0 && ancestor < g_scene->count
                            && (inherited[ancestor] == 2 || g_scene->items[ancestor].selected)
                        ? 2
                        : 1;
                    ancestor = parent;
                    while (ancestor >= 0 && ancestor < g_scene->count && !inherited[ancestor])
                    {
                        inherited[ancestor] = selected;
                        if (g_scene->items[ancestor].selected)
                        {
                            break;
                        }
                        ancestor = g_scene->items[ancestor].parent;
                    }
                }
                if (inherited[parent] == 2)
                {
                    continue;
                }
            }
            if (IsItemMovable(i))
            {
                out[n++] = i;
            }
        }
        return n;
    }

    CVector ItemPivotPos(int idx)
    {
        if (g_pickupRuntime && g_scene->data[idx].pickup.HasValue())
        {
            CVector position;
            if (!g_pickupRuntime->GetPosition(idx, position))
            {
                return g_scene->world[idx].pos;
            }
            if (g_pivotMode == GizmoPivot::Center)
            {
                if (auto* object = g_pickupRuntime->GetObject(idx))
                {
                    return position + (object->GetBoundCentre() - object->GetPosition());
                }
            }
            return position;
        }
        if (g_pivotMode == GizmoPivot::Center)
        {
            const ItemData& data = g_scene->data[idx];
            if (data.actor.HasValue())
            {
                const auto model = SkinBrowser::GetModel(data.actor.Get().skinId);
                const CVector center((model.boundsMin[0] + model.boundsMax[0]) * .5f,
                    (model.boundsMin[1] + model.boundsMax[1]) * .5f, (model.boundsMin[2] + model.boundsMax[2]) * .5f);
                return g_scene->world[idx].pos + Utils::TransformDirection(g_scene->world[idx].rot, center);
            }
            if (data.vehicle.HasValue())
            {
                ModelPreview::Profile profile;
                if (ModelPreview::GetProfile(data.vehicle.Get().modelId, profile))
                {
                    const CVector offset(profile.target[0], profile.target[1], profile.target[2]);
                    return g_scene->world[idx].pos + Utils::TransformDirection(g_scene->world[idx].rot, offset);
                }
            }
            if (data.model.HasValue())
            {
                const ObjectBrowser::ModelInfo model = ObjectBrowser::GetModel(data.model.Get().modelId);
                ModelPreview::Profile profile;
                if (model.manualCenter && ModelPreview::GetProfile(data.model.Get().modelId, profile))
                {
                    const CVector localOffset(profile.target[0], profile.target[1], profile.target[2]);
                    const auto rotation
                        = ModelConstraints::AppliedRotation(data.model.Get().modelId, g_scene->world[idx].eulerDeg);
                    return g_scene->world[idx].pos
                        + Utils::TransformDirection(Utils::EulerDegreesToBasis(rotation.data()), localOffset);
                }
            }
            if (g_objectRuntime)
            {
                ObjectRuntimeState& rt = g_objectRuntime->Runtime(idx);
                if (rt.state == ObjectStreamState::Spawned && rt.obj)
                {
                    return rt.obj->GetBoundCentre();
                }
            }
        }
        return g_scene->world[idx].pos;
    }

    Utils::RotationBasis QuatToRot(const D3DXQUATERNION& q)
    {
        D3DXMATRIX m;
        D3DXMatrixRotationQuaternion(&m, &q);
        Utils::RotationBasis r;
        r.right = CVector(m._11, m._12, m._13);
        r.forward = CVector(m._21, m._22, m._23);
        r.up = CVector(m._31, m._32, m._33);
        return r;
    }

    D3DXQUATERNION RotToQuat(const Utils::RotationBasis& r)
    {
        D3DXMATRIX m;
        D3DXMatrixIdentity(&m);
        m._11 = r.right.x;
        m._12 = r.right.y;
        m._13 = r.right.z;
        m._21 = r.forward.x;
        m._22 = r.forward.y;
        m._23 = r.forward.z;
        m._31 = r.up.x;
        m._32 = r.up.y;
        m._33 = r.up.z;
        D3DXQUATERNION q;
        D3DXQuaternionRotationMatrix(&q, &m);
        return q;
    }
}

void Selection::UpdateBoxAreaGizmoBinding()
{
    if (!g_gizmos || !g_scene || !g_scenePicker)
    {
        return;
    }
    const int index = g_scene->SelectionCount() == 1 ? g_scene->SingleSelectedIndex() : -1;
    const bool active = index >= 0 && g_scenePicker->GetSelection().empty() && g_scene->data[index].boxArea.HasValue();
    if (g_wasBoxAreaDragging && (!active || g_scene->items[index].id != g_boxAreaItemId))
    {
        g_gizmos->SetBoxAreaGizmo(false, CVector(), BoxAreaData());
    }
    if (active && !g_gizmos->IsDragging() && !g_gizmos->IsBoxAreaDragging())
    {
        g_gizmos->SetGizmoPosition(g_scene->world[index].pos);
    }
    g_gizmos->SetBoxAreaGizmo(
        active, active ? g_scene->world[index].pos : CVector(), active ? g_scene->data[index].boxArea.Get() : BoxAreaData());
}

void Selection::UpdateGizmoBinding()
{
    if (!g_gizmos || !g_scene || !g_scenePicker || !g_objectRuntime)
    {
        return;
    }
    if (!g_gizmos->IsDragging())
    {
        g_gizmos->SetRotationEnabled(true);
    }

    UpdateSelectionStamps();
    if (Curves::UpdateHandleGizmoBinding())
    {
        return;
    }
    UpdateBoxAreaGizmoBinding();
    if (g_gizmos->IsBoxAreaDragging())
    {
        const int index = g_scene->SingleSelectedIndex();
        if (!g_wasBoxAreaDragging)
        {
            if (!g_sceneActions)
            {
                g_gizmos->SetBoxAreaGizmo(false, CVector(), BoxAreaData());
                return;
            }
            try
            {
                g_sceneActions->Prepare(sizeof(ItemDataEditAction) + sizeof(ItemDataEditAction::Entry)
                    + 2 * g_scene->data[index].DynamicMemoryUsage());
                g_boxAreaBefore = g_scene->data[index];
            }
            catch (const std::bad_alloc&)
            {
                g_gizmos->SetBoxAreaGizmo(false, CVector(), BoxAreaData());
                GuiShowToast("Not enough memory to resize the box area.", true);
                return;
            }
            g_boxAreaItemId = g_scene->items[index].id;
            g_wasBoxAreaDragging = true;
        }
        g_scene->data[index].boxArea.Edit() = g_gizmos->GetBoxAreaValue();
        const CVector center = g_gizmos->GetBoxAreaPosition();
        const CVector gizmo = g_gizmos->GetGizmoPosition() + center - g_scene->world[index].pos;
        float rotation[3];
        std::copy_n(g_scene->world[index].eulerDeg, 3, rotation);
        g_scene->SetWorldTransform(index, center, rotation);
        g_gizmos->SetGizmoPosition(gizmo);
        return;
    }
    if (g_wasBoxAreaDragging)
    {
        g_wasBoxAreaDragging = false;
        const int index = g_scene->FindIndexById(g_boxAreaItemId);
        if (index >= 0 && g_sceneActions && g_boxAreaBefore != g_scene->data[index])
        {
            std::vector<ItemDataEditAction::Entry> entries(1);
            entries[0].itemId = g_boxAreaItemId;
            entries[0].before = std::move(g_boxAreaBefore);
            entries[0].after = g_scene->data[index];
            g_sceneActions->Execute(std::make_unique<ItemDataEditAction>(std::move(entries)));
        }
        g_boxAreaBefore = {};
        g_boxAreaItemId = -1;
    }

    {
        if (g_gizmos->IsRadiusDragging())
        {
            const int selIdx = (g_scene->SelectionCount() == 1) ? g_scene->SingleSelectedIndex() : -1;
            if (selIdx >= 0
                && (g_scene->items[selIdx].type == SceneItemRemoveBuilding || g_scene->data[selIdx].raceCheckpoint.HasValue()))
            {
                if (!g_wasRadiusDragging)
                {
                    g_wasRadiusDragging = true;
                    g_radiusItemId = g_scene->items[selIdx].id;
                    g_radiusBefore = g_scene->data[selIdx];
                }
                if (g_scene->data[selIdx].raceCheckpoint.HasValue())
                {
                    g_scene->data[selIdx].raceCheckpoint.Edit().radius
                        = std::clamp(g_gizmos->GetRadiusValue(), RaceCheckpointData::MinRadius, RaceCheckpointData::MaxRadius);
                }
                else
                {
                    g_scene->data[selIdx].removeBuilding.Edit().radius = g_gizmos->GetRadiusValue();
                }
            }
        }
        else if (g_wasRadiusDragging)
        {
            g_wasRadiusDragging = false;
            const int idx = g_scene->FindIndexById(g_radiusItemId);
            if (idx >= 0 && g_sceneActions && g_radiusBefore != g_scene->data[idx])
            {
                std::vector<ItemDataEditAction::Entry> entries(1);
                entries[0].itemId = g_radiusItemId;
                entries[0].before = g_radiusBefore;
                entries[0].after = g_scene->data[idx];
                g_sceneActions->Execute(std::make_unique<ItemDataEditAction>(std::move(entries)));
            }
            g_radiusItemId = -1;
        }
    }

    const bool dragging = g_gizmos->IsDragging();

    if (dragging)
    {
        if (!g_wasDragging)
        {
            g_dragApplied = false;
            g_dragGizmoStart = g_gizmos->GetGizmoPosition();
            g_dragItems.clear();

            int roots[EditorScene::MaxItems];
            const int n = CollectMovableRoots(roots, EditorScene::MaxItems);
            if (!g_sceneActions)
            {
                g_gizmos->SetGizmoEnabled(false);
                return;
            }
            try
            {
                g_sceneActions->Prepare(
                    sizeof(TransformEditAction) + n * (sizeof(TransformEditAction::Entry) + sizeof(DragItem)));
                g_dragItems.reserve(n);
                g_dragAction = std::make_unique<TransformEditAction>(roots, n);
            }
            catch (const std::bad_alloc&)
            {
                g_dragAction.reset();
                g_gizmos->SetGizmoEnabled(false);
                GuiShowToast("Not enough memory to start the transform.", true);
                return;
            }
            g_scene->UpdateWorldTransforms();
            g_wasDragging = true;
            for (int i = 0; i < n; ++i)
            {
                DragItem di;
                di.id = g_scene->items[roots[i]].id;
                di.startPos = g_scene->world[roots[i]].pos;
                di.startRot = g_scene->world[roots[i]].rot;
                g_dragItems.push_back(di);

                const ItemData& dd = g_scene->data[roots[i]];
                if (dd.model.HasValue() && ModelChecks::IsEscalatorBaseModel(dd.model.Get().modelId) && g_objectRuntime)
                {
                    ObjectRuntimeState& rt = g_objectRuntime->Runtime(roots[i]);
                    if (rt.obj)
                    {
                        rt.obj->RemoveEscalatorsForEntity();
                    }
                }
            }
        }

        const CVector gizmoPos = g_gizmos->GetGizmoPosition();
        const CVector posDelta = gizmoPos - g_dragGizmoStart;
        const D3DXQUATERNION dragQuat = g_gizmos->GetDragRotationDelta();

        const float posDelta2 = posDelta.x * posDelta.x + posDelta.y * posDelta.y + posDelta.z * posDelta.z;
        const bool rotIsIdentity = fabsf(dragQuat.x) < 1e-7f && fabsf(dragQuat.y) < 1e-7f && fabsf(dragQuat.z) < 1e-7f;
        if (!g_dragApplied && posDelta2 < 1e-12f && rotIsIdentity)
        {
            return;
        }
        g_dragApplied = true;

        const Utils::RotationBasis rotDelta = QuatToRot(dragQuat);

        for (const auto& di : g_dragItems)
        {
            const int idx = g_scene->FindIndexById(di.id);
            if (idx < 0)
            {
                continue;
            }

            const CVector rel = di.startPos - g_dragGizmoStart;
            const Utils::RotationBasis newRot = Utils::ComposeRotationBases(rotDelta, di.startRot);

            float newEuler[3];
            Utils::BasisToEulerDegrees(newRot, newEuler);
            const ItemData& item = g_scene->data[idx];
            if (item.model.HasValue() && !ModelConstraints::SupportsRotationXY(item.model.Get().modelId))
            {
                float startEuler[3];
                Utils::BasisToEulerDegrees(di.startRot, startEuler);
                newEuler[0] = startEuler[0];
                newEuler[1] = startEuler[1];
            }
            const CVector newPos = g_dragGizmoStart + posDelta + Utils::TransformDirection(rotDelta, rel);
            g_scene->SetWorldTransform(idx, newPos, newEuler);
        }

        if (g_dragItems.size() == 1)
        {
            const int index = g_scene->FindIndexById(g_dragItems.front().id);
            if (index >= 0 && g_scene->data[index].pickup.HasValue())
            {
                g_gizmos->SetGizmoPosition(ItemPivotPos(index));
            }
        }

        const int rbIdxDrag = (g_scene->SelectionCount() == 1) ? g_scene->SingleSelectedIndex() : -1;
        UpdateBoxAreaGizmoBinding();
        if (rbIdxDrag >= 0 && g_scene->items[rbIdxDrag].type == SceneItemRemoveBuilding
            && g_scene->data[rbIdxDrag].removeBuilding.Get().custom)
        {
            g_gizmos->SetRadiusGizmo(true, gizmoPos, g_scene->data[rbIdxDrag].removeBuilding.Get().radius);
        }
        else if (rbIdxDrag >= 0 && g_scene->data[rbIdxDrag].raceCheckpoint.HasValue())
        {
            g_gizmos->SetRadiusGizmo(
                true, gizmoPos, g_scene->data[rbIdxDrag].raceCheckpoint.Get().radius, RaceCheckpointData::MaxRadius);
        }

        return;
    }

    if (g_wasDragging)
    {
        g_wasDragging = false;
        if (g_sceneActions && !g_dragItems.empty())
        {
            for (const auto& di : g_dragItems)
            {
                const int idx = g_scene->FindIndexById(di.id);
                if (idx < 0)
                {
                    continue;
                }

                const ItemData& dd = g_scene->data[idx];
                if (dd.model.HasValue() && ModelChecks::IsEscalatorBaseModel(dd.model.Get().modelId) && g_objectRuntime)
                {
                    ObjectRuntimeState& rt = g_objectRuntime->Runtime(idx);
                    if (rt.obj)
                    {
                        rt.obj->CreateEffects();
                    }
                }
            }
            if (g_dragAction && g_dragAction->CaptureAfter())
            {
                g_sceneActions->Execute(std::move(g_dragAction));
            }
        }
        g_dragAction.reset();
        g_dragItems.clear();
    }

    int roots[EditorScene::MaxItems];
    const int movable = CollectMovableRoots(roots, EditorScene::MaxItems);
    const auto& natives = g_scenePicker->GetSelection();

    if (movable > 0)
    {
        CVector center = CVector(0.0f, 0.0f, 0.0f);
        for (int i = 0; i < movable; ++i)
        {
            center += ItemPivotPos(roots[i]);
        }
        const float inv = 1.0f / (float)movable;
        center.x *= inv;
        center.y *= inv;
        center.z *= inv;

        g_gizmos->SetGizmoVisible(true);
        g_gizmos->SetGizmoEnabled(true);
        g_gizmos->SetGizmoPosition(center);
        const bool multipleSelected = TotalCount() > 1;
        const ItemData& item = g_scene->data[roots[0]];
        const bool rotation = multipleSelected || item.SupportsRotation();
        g_gizmos->SetRotationEnabled(rotation);
        g_gizmos->SetRotationXYEnabled(multipleSelected
            || (rotation && !item.vehicle.HasValue() && !item.actor.HasValue()
                && (!item.model.HasValue() || ModelConstraints::SupportsRotationXY(item.model.Get().modelId))));

        int oldest = roots[0];
        unsigned int bestStamp = UINT_MAX;
        for (int i = 0; i < movable; ++i)
        {
            const unsigned int stamp = SelStampForId(g_scene->items[roots[i]].id);
            if (stamp != 0 && stamp < bestStamp)
            {
                bestStamp = stamp;
                oldest = roots[i];
            }
        }
        g_gizmos->SetGizmoQuat(RotToQuat(g_scene->world[oldest].rot));

        const int rbIdx = (g_scene->SelectionCount() == 1) ? g_scene->SingleSelectedIndex() : -1;
        if (rbIdx >= 0 && g_scene->items[rbIdx].type == SceneItemRemoveBuilding
            && g_scene->data[rbIdx].removeBuilding.Get().custom)
        {
            g_gizmos->SetRadiusGizmo(true, center, g_scene->data[rbIdx].removeBuilding.Get().radius);
        }
        else if (rbIdx >= 0 && g_scene->data[rbIdx].raceCheckpoint.HasValue())
        {
            g_gizmos->SetRadiusGizmo(
                true, center, g_scene->data[rbIdx].raceCheckpoint.Get().radius, RaceCheckpointData::MaxRadius);
        }
        else
        {
            g_gizmos->SetRadiusGizmo(false, CVector(0.0f, 0.0f, 0.0f), 0.0f);
        }
    }
    else if (!natives.empty())
    {
        CVector center = CVector(0.0f, 0.0f, 0.0f);
        int n = 0;
        for (const auto& e : natives)
        {
            center += (g_pivotMode == GizmoPivot::Center) ? e.posCentre : e.posPivot;
            ++n;
        }
        if (n > 0)
        {
            const float inv = 1.0f / (float)n;
            center.x *= inv;
            center.y *= inv;
            center.z *= inv;
        }

        g_gizmos->SetGizmoVisible(true);
        g_gizmos->SetGizmoEnabled(false);
        g_gizmos->SetRotationXYEnabled(true);
        g_gizmos->SetGizmoPosition(center);

        D3DXQUATERNION quat = RotToQuat(natives.front().rot);
        g_gizmos->SetGizmoQuat(quat);
        g_gizmos->SetRadiusGizmo(false, CVector(0.0f, 0.0f, 0.0f), 0.0f);
    }
    else
    {
        g_gizmos->SetGizmoVisible(false);
        g_gizmos->SetRadiusGizmo(false, CVector(0.0f, 0.0f, 0.0f), 0.0f);
    }
    UpdateBoxAreaGizmoBinding();
}
