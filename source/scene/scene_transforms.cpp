// Source Repository: https://github.com/waverso/waversomapstudio

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../shared/skin_catalog.h"
#include "../shared/vehicle_catalog.h"
#include "../utils/transform_math.h"
#include "../layout/curves.h"
#include "../layout/layout.h"
#include "scene.h"
#include "../textures/texture_browser.h"

void EditorScene::UpdateRaceNames()
{
    int number = 0;
    int parent = -1;
    for (const int index : DisplayOrder())
    {
        auto& item = items[index];
        if (item.type == SceneItemRace)
        {
            item.name[RaceData::MaxName] = '\0';
        }
        else if (item.type == SceneItemRaceCheckpoint && item.parent >= 0)
        {
            if (parent != item.parent)
            {
                parent = item.parent;
                number = 0;
            }
            snprintf(item.name, sizeof(item.name), "%s (CP %d)", items[parent].name, ++number);
        }
    }
}

bool EditorScene::ComposesWithParent(int idx) const
{
    return items[idx].parent >= 0 && items[idx].type != SceneItemRemoveBuilding;
}

void EditorScene::UpdateWorldTransforms() noexcept
{
    UpdateRaceNames();
    Layout::Synchronize(*this);
    int ordered[MaxItems];
    int n = 0;
    CollectDisplayOrder(-1, ordered, n);

    for (int k = 0; k < n; ++k)
    {
        const int i = ordered[k];
        auto& item = items[i];
        const auto& components = data[i];
        if (components.vehicle.HasValue() || components.actor.HasValue() || components.model.HasValue()
            || components.pickup.HasValue())
        {
            const int modelId = components.vehicle.HasValue() ? components.vehicle.Get().modelId
                : components.actor.HasValue()                 ? components.actor.Get().skinId
                : components.pickup.HasValue()                ? components.pickup.Get().modelId
                                                              : components.model.Get().modelId;
            if (item.searchModelId != modelId)
            {
                const auto name = [&](int id, char* output, size_t size)
                {
                    if (components.vehicle.HasValue())
                    {
                        const auto* model = VehicleCatalog::Get(id);
                        snprintf(output, size, "%s", model ? model->name : "Vehicle");
                    }
                    else if (components.actor.HasValue())
                    {
                        const auto* model = SkinCatalog::Get(id);
                        snprintf(output, size, "%s", model ? model->name : "Actor");
                    }
                    else
                    {
                        TextureBrowser::GetModelDisplay(id, output, size);
                    }
                };
                char previousName[sizeof(item.name)];
                name(item.searchModelId, previousName, sizeof(previousName));
                if (item.searchModelId >= 0 && strcmp(item.name, previousName) == 0)
                {
                    name(modelId, item.name, sizeof(item.name));
                }
                item.searchModelId = modelId;
            }
        }
        TransformData& tr = data[i].transform;
        WorldTransform& w = world[i];
        const CVector prevPos = w.pos;
        float prevEuler[3];
        memcpy(prevEuler, w.eulerDeg, sizeof(prevEuler));

        if (!ComposesWithParent(i))
        {
            tr.pos[2] = (std::max)(tr.pos[2], TransformData::MinZ);
            w.pos = CVector(tr.pos[0], tr.pos[1], tr.pos[2]);
            memcpy(w.eulerDeg, tr.rot, sizeof(w.eulerDeg));
            w.rot = Utils::EulerDegreesToBasis(tr.rot);
        }
        else
        {
            const WorldTransform& pw = world[items[i].parent];
            w.rot = Utils::ComposeRotationBases(pw.rot, Utils::EulerDegreesToBasis(tr.rot));
            w.pos = pw.pos + Utils::TransformDirection(pw.rot, CVector(tr.pos[0], tr.pos[1], tr.pos[2]));
            Utils::BasisToEulerDegrees(w.rot, w.eulerDeg);
        }

        w.pos.z = (std::max)(w.pos.z, TransformData::MinZ);

        if (data[i].vehicle.HasValue() || data[i].actor.HasValue())
        {
            if (data[i].actor.HasValue())
            {
                tr.rot[0] = tr.rot[1] = 0.0f;
            }
            w.eulerDeg[0] = w.eulerDeg[1] = 0.0f;
            w.eulerDeg[2] = tr.rot[2] + (ComposesWithParent(i) ? world[items[i].parent].eulerDeg[2] : 0.0f);
            w.rot = Utils::EulerDegreesToBasis(w.eulerDeg);
        }
        if (data[i].HasFixedRotation())
        {
            std::fill_n(tr.rot, 3, 0.0f);
            std::fill_n(w.eulerDeg, 3, 0.0f);
            w.rot = Utils::EulerDegreesToBasis(w.eulerDeg);
        }
        w.changedThisFrame = (prevPos.x != w.pos.x || prevPos.y != w.pos.y || prevPos.z != w.pos.z
            || memcmp(prevEuler, w.eulerDeg, sizeof(prevEuler)) != 0);
    }
    Curves::Synchronize(*this);
}

void EditorScene::SetWorldTransform(int idx, const CVector& position, const float eulerDeg[3])
{
    CVector pos = position;
    pos.z = (std::max)(pos.z, TransformData::MinZ);
    TransformData& tr = data[idx].transform;
    WorldTransform& w = world[idx];

    if (!ComposesWithParent(idx))
    {
        tr.pos[0] = pos.x;
        tr.pos[1] = pos.y;
        tr.pos[2] = pos.z;
        memcpy(tr.rot, eulerDeg, sizeof(tr.rot));
    }
    else
    {
        const WorldTransform& pw = world[items[idx].parent];
        const CVector local = Utils::InverseTransformDirection(pw.rot, pos - pw.pos);
        tr.pos[0] = local.x;
        tr.pos[1] = local.y;
        tr.pos[2] = local.z;

        const Utils::RotationBasis localRot
            = Utils::ComposeRotationBases(Utils::TransposeRotationBasis(pw.rot), Utils::EulerDegreesToBasis(eulerDeg));
        Utils::BasisToEulerDegrees(localRot, tr.rot);
    }

    w.pos = pos;
    memcpy(w.eulerDeg, eulerDeg, sizeof(w.eulerDeg));
    w.rot = Utils::EulerDegreesToBasis(eulerDeg);
    if (data[idx].vehicle.HasValue() || data[idx].actor.HasValue())
    {
        tr.rot[0] = tr.rot[1] = 0.0f;
        tr.rot[2] = eulerDeg[2] - (ComposesWithParent(idx) ? world[items[idx].parent].eulerDeg[2] : 0.0f);
        w.eulerDeg[0] = w.eulerDeg[1] = 0.0f;
        w.rot = Utils::EulerDegreesToBasis(w.eulerDeg);
    }
    if (data[idx].HasFixedRotation())
    {
        std::fill_n(tr.rot, 3, 0.0f);
        std::fill_n(w.eulerDeg, 3, 0.0f);
        w.rot = Utils::EulerDegreesToBasis(w.eulerDeg);
    }
    w.changedThisFrame = true;
}
