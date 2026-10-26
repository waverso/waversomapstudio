// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    struct Bounds
    {
        CVector& minimum;
        CVector& maximum;
        Utils::RotationBasis space;
        bool found = false;

        Bounds(CVector& minimum, CVector& maximum, Utils::RotationBasis space = {})
            : minimum(minimum), maximum(maximum), space(space)
        {
            minimum = CVector(FLT_MAX, FLT_MAX, FLT_MAX);
            maximum = CVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        }

        template <class Callback>
        static void ForEachBoxCorner(const CVector& boxMin, const CVector& boxMax, CVector position,
            Utils::RotationBasis rotation, Callback&& callback, CEntity* entity = nullptr, float scale = 1.0f)
        {
            if (entity)
            {
                position = entity->GetPosition();
                if (entity->m_matrix)
                {
                    rotation.right = entity->m_matrix->right;
                    rotation.forward = entity->m_matrix->up;
                    rotation.up = entity->m_matrix->at;
                }
                else
                {
                    const float heading[3] = { 0.0f, 0.0f, D3DXToDegree(entity->m_placement.m_fHeading) };
                    rotation = Utils::EulerDegreesToBasis(heading);
                }
                if (entity->m_nType == ENTITY_TYPE_OBJECT)
                {
                    scale = static_cast<CObject*>(entity)->m_fScale;
                }
            }
            for (int corner = 0; corner < 8; ++corner)
            {
                const CVector local(
                    corner & 1 ? boxMax.x : boxMin.x, corner & 2 ? boxMax.y : boxMin.y, corner & 4 ? boxMax.z : boxMin.z);
                const CVector point = position + Utils::TransformDirection(rotation, local * scale);
                if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
                {
                    continue;
                }
                callback(point);
            }
        }

        template <class Callback>
        static void ForEachModelCorner(int modelId, const CVector& position, const Utils::RotationBasis& rotation,
            Callback&& callback, CEntity* entity = nullptr, float scale = 1.0f, float clearance = 0.0f)
        {
            const auto model = ObjectBrowser::GetModel(modelId);
            CVector boxMin(model.boundsMin[0], model.boundsMin[1], model.boundsMin[2]);
            CVector boxMax(model.boundsMax[0], model.boundsMax[1], model.boundsMax[2]);
            if (boxMin.IsZero() && boxMax.IsZero())
            {
                const auto* info
                    = modelId >= 0 && modelId < CModelInfo::ms_modelInfoCount ? CModelInfo::GetModelInfo(modelId) : nullptr;
                if (info && info->m_pColModel)
                {
                    boxMin = info->m_pColModel->m_boundBox.m_vecMin;
                    boxMax = info->m_pColModel->m_boundBox.m_vecMax;
                }
                else
                {
                    boxMin = CVector(-model.radius, -model.radius, -model.radius);
                    boxMax = CVector(model.radius, model.radius, model.radius);
                }
            }
            if (scale > 0.0f)
            {
                boxMin.z -= clearance / scale;
            }
            ForEachBoxCorner(boxMin, boxMax, position, rotation, callback, entity, scale);
        }

        template <class Callback>
        static void ForEachItemCorner(const EditorScene& scene, int index, const EditorScene::WorldTransform& world, bool live,
            Callback&& callback, float clearance = 0.0f, bool includeEmpty = false)
        {
            const auto& data = scene.data[index];
            const Utils::RotationBasis identity;
            if (data.model.HasValue())
            {
                const int modelId = data.model.Get().modelId;
                const auto applied = ModelConstraints::AppliedRotation(modelId, world.eulerDeg);
                ForEachModelCorner(modelId, world.pos, Utils::EulerDegreesToBasis(applied.data()), callback,
                    live && g_objectRuntime ? g_objectRuntime->Runtime(index).obj : nullptr);
            }
            else if (data.vehicle.HasValue())
            {
                const float yaw[3] = { 0.0f, 0.0f, world.eulerDeg[2] };
                ForEachModelCorner(data.vehicle.Get().modelId, world.pos, Utils::EulerDegreesToBasis(yaw), callback,
                    live && g_vehicleRuntime ? g_vehicleRuntime->GetVehicle(index) : nullptr);
            }
            else if (data.actor.HasValue())
            {
                const auto model = SkinBrowser::GetModel(data.actor.Get().skinId);
                const float yaw[3] = { 0.0f, 0.0f, world.eulerDeg[2] };
                ForEachBoxCorner(CVector(model.boundsMin[0], model.boundsMin[1], model.boundsMin[2]),
                    CVector(model.boundsMax[0], model.boundsMax[1], model.boundsMax[2]), world.pos,
                    Utils::EulerDegreesToBasis(yaw), callback, live && g_actorRuntime ? g_actorRuntime->GetPed(index) : nullptr);
            }
            else if (data.pickup.HasValue())
            {
                CVector position = world.pos;
                if (live && g_pickupRuntime && !g_pickupRuntime->GetPosition(index, position))
                {
                    return;
                }
                const int modelId = data.pickup.Get().modelId;
                const float scale = PickupRuntime::GetVisualScale(modelId);
                ForEachModelCorner(modelId, position, identity, callback,
                    live && g_pickupRuntime ? g_pickupRuntime->GetObject(index) : nullptr, scale > 0.0f ? scale : 1.0f,
                    clearance);
            }
            else if (data.boxArea.HasValue())
            {
                const auto& area = data.boxArea.Get();
                const CVector extent(area.GetSize(0) * 0.5f, area.GetSize(1) * 0.5f, area.GetSize(2) * 0.5f);
                ForEachBoxCorner(extent * -1.0f, extent, world.pos, identity, callback);
            }
            else if (data.raceCheckpoint.HasValue() || data.removeBuilding.HasValue())
            {
                const float radius
                    = data.raceCheckpoint.HasValue() ? data.raceCheckpoint.Get().radius : data.removeBuilding.Get().radius;
                const CVector extent(radius, radius, radius);
                ForEachBoxCorner(extent * -1.0f, extent, world.pos, identity, callback);
            }
            else if (data.curve.HasValue())
            {
                CVector minimum, maximum;
                if (Curves::ComputeBounds(scene, index, minimum, maximum))
                {
                    ForEachBoxCorner(minimum, maximum, world.pos, world.rot, callback);
                }
                else
                {
                    ForEachBoxCorner(CVector(), CVector(), world.pos, identity, callback);
                }
            }
            else if (data.layout.HasValue())
            {
                CVector minimum, maximum;
                if (Layout::ComputeBounds(scene, index, minimum, maximum))
                {
                    ForEachBoxCorner(minimum, maximum, world.pos, world.rot, callback);
                }
                else
                {
                    ForEachBoxCorner(CVector(), CVector(), world.pos, identity, callback);
                }
            }
            else if (includeEmpty || !scene.HasChildren(index))
            {
                ForEachBoxCorner(CVector(), CVector(), world.pos, identity, callback);
            }
        }

        void IncludePoint(const CVector& worldPoint)
        {
            const CVector point = Utils::TransformDirection(space, worldPoint);
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
            {
                return;
            }
            minimum.x = std::min(minimum.x, point.x);
            minimum.y = std::min(minimum.y, point.y);
            minimum.z = std::min(minimum.z, point.z);
            maximum.x = std::max(maximum.x, point.x);
            maximum.y = std::max(maximum.y, point.y);
            maximum.z = std::max(maximum.z, point.z);
            found = true;
        }

        void IncludeModel(int modelId, const CVector& position, const Utils::RotationBasis& rotation, CEntity* entity = nullptr)
        {
            ForEachModelCorner(
                modelId, position, rotation,
                [&](const CVector& point)
                {
                    IncludePoint(point);
                },
                entity);
        }

        void IncludeItem(
            const EditorScene& scene, int index, const EditorScene::WorldTransform& world, bool live, bool includeEmpty = false)
        {
            ForEachItemCorner(
                scene, index, world, live,
                [&](const CVector& point)
                {
                    IncludePoint(point);
                },
                0.0f, includeEmpty);
        }
    };

    bool ComputeSelectionBounds(CVector& outMinimum, CVector& outMaximum, bool includeEmpty)
    {
        Bounds bounds(outMinimum, outMaximum);
        if (g_scene)
        {
            int selectedDepth = -1;
            for (int index : g_scene->DisplayOrder())
            {
                const int depth = g_scene->HierarchyDepth(index);
                if (depth <= selectedDepth)
                {
                    selectedDepth = -1;
                }
                if (selectedDepth < 0)
                {
                    if (!g_scene->items[index].selected)
                    {
                        continue;
                    }
                    selectedDepth = depth;
                }
                bounds.IncludeItem(*g_scene, index, g_scene->world[index], true, includeEmpty);
            }
        }
        if (g_scenePicker)
        {
            for (const auto& entry : g_scenePicker->GetSelection())
            {
                bounds.IncludeModel(entry.nModelId, entry.posPivot, entry.rot, entry.lastEntity);
            }
        }
        return bounds.found;
    }
}

bool Selection::ComputeBounds(CVector& outMinimum, CVector& outMaximum)
{
    return ComputeSelectionBounds(outMinimum, outMaximum, false);
}

bool Selection::ComputeChildrenBounds(const EditorScene& scene, int root, CVector& outMinimum, CVector& outMaximum)
{
    Bounds bounds(outMinimum, outMaximum);
    if (root < 0 || root >= scene.count)
    {
        return false;
    }
    const int rootDepth = scene.HierarchyDepth(root);
    bool started = false;
    for (const int index : scene.DisplayOrder())
    {
        if (index == root)
        {
            started = true;
            continue;
        }
        if (!started)
        {
            continue;
        }
        if (scene.HierarchyDepth(index) <= rootDepth)
        {
            break;
        }
        bounds.IncludeItem(scene, index, scene.world[index], true);
    }
    return bounds.found;
}

bool Selection::ComputeItemBounds(
    const EditorScene& scene, int root, const float rotation[3], CVector& outMinimum, CVector& outMaximum)
{
    Bounds bounds(outMinimum, outMaximum, Utils::EulerDegreesToBasis(rotation));
    if (root < 0 || root >= scene.count)
    {
        return false;
    }
    const int rootDepth = scene.HierarchyDepth(root);
    std::vector<EditorScene::WorldTransform> parents(0);
    bool started = false;
    for (const int index : scene.DisplayOrder())
    {
        if (index == root)
        {
            started = true;
        }
        if (!started)
        {
            continue;
        }
        const int depth = scene.HierarchyDepth(index) - rootDepth;
        if (index != root && depth <= 0)
        {
            break;
        }
        EditorScene::WorldTransform pose;
        if (depth > 0)
        {
            const auto& transform = scene.data[index].transform;
            const auto& parent = parents[depth - 1];
            pose.rot = Utils::ComposeRotationBases(parent.rot, Utils::EulerDegreesToBasis(transform.rot));
            pose.pos
                = parent.pos + Utils::TransformDirection(parent.rot, { transform.pos[0], transform.pos[1], transform.pos[2] });
            Utils::BasisToEulerDegrees(pose.rot, pose.eulerDeg);
            const auto& data = scene.data[index];
            if (data.removeBuilding.HasValue())
            {
                const auto& world = scene.world[root];
                pose.pos = Utils::TransformDirection(Utils::TransposeRotationBasis(Utils::EulerDegreesToBasis(world.eulerDeg)),
                    CVector(transform.pos[0], transform.pos[1], transform.pos[2]) - world.pos);
            }
            if (data.vehicle.HasValue() || data.actor.HasValue())
            {
                pose.eulerDeg[0] = pose.eulerDeg[1] = 0.0f;
                pose.eulerDeg[2] = parent.eulerDeg[2] + transform.rot[2];
                pose.rot = Utils::EulerDegreesToBasis(pose.eulerDeg);
            }
            if (data.pickup.HasValue() || data.raceCheckpoint.HasValue())
            {
                std::fill_n(pose.eulerDeg, 3, 0.0f);
                pose.rot = {};
            }
        }
        parents.resize(depth);
        parents.push_back(pose);
        bounds.IncludeItem(scene, index, pose, false);
    }
    return bounds.found;
}

bool Selection::ComputeFocusTarget(CVector& outCenter, float& outStandoff)
{
    CVector minimum, maximum;
    if (!ComputeSelectionBounds(minimum, maximum, true))
    {
        return false;
    }
    outCenter = minimum + (maximum - minimum) * 0.5f;
    const CVector extent = (maximum - minimum) * 0.5f;
    outStandoff = std::max(3.0f, sqrtf(Utils::Dot3(extent, extent)) * 2.5f);
    if (!g_scene || (g_scenePicker && !g_scenePicker->GetSelection().empty()))
    {
        return true;
    }
    int root = -1;
    for (int index = 0; index < g_scene->count; ++index)
    {
        if (!g_scene->items[index].selected || g_scene->HasSelectedAncestor(index))
        {
            continue;
        }
        if (root >= 0)
        {
            return true;
        }
        root = index;
    }
    if (root >= 0 && g_scene->data[root].layout.HasValue())
    {
        outStandoff = std::min(400.0f, outStandoff);
    }
    return true;
}
