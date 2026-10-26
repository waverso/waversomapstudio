// Source Repository: https://github.com/waverso/waversomapstudio

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "../shared/animation_catalog.h"
#include "../shared/skin_catalog.h"
#include "project_document.h"

namespace ProjectDocument
{
    namespace
    {
        bool Finite(const float (&values)[3])
        {
            return std::all_of(std::begin(values), std::end(values),
                [](float v)
                {
                    return std::isfinite(v);
                });
        }
    }

    bool Validate(const Items& items, std::string& error)
    {
        error.clear();
        if (items.size() > MaxItems)
        {
            error = "Project exceeds the item limit.";
            return false;
        }
        std::unordered_set<int> ids;
        std::unordered_map<int, const ItemSnapshot*> parents;
        std::set<std::pair<int, int>> orders;
        for (const auto& item : items)
        {
            const auto& d = item.data;
            if (item.id <= 0 || item.id >= std::numeric_limits<int>::max() - static_cast<int>(MaxItems) || ids.contains(item.id)
                || (item.parentId != -1 && !ids.contains(item.parentId)) || item.order < 0
                || !orders.emplace(item.parentId, item.order).second)
            {
                error = "Invalid project hierarchy.";
                return false;
            }
            ids.insert(item.id);
            const auto parent = parents.find(item.parentId);
            const bool raceItem = item.type == SceneItemRace;
            const bool checkpointItem = item.type == SceneItemRaceCheckpoint;
            const bool curveItem = item.type == SceneItemCurve;
            const bool nodeItem = item.type == SceneItemCurveNode;
            if (curveItem != d.curve.HasValue() || nodeItem != d.curveNode.HasValue()
                || (nodeItem && (parent == parents.end() || parent->second->type != SceneItemCurve))
                || (!nodeItem && parent != parents.end()
                    && (parent->second->type == SceneItemCurve || parent->second->type == SceneItemCurveNode))
                || ((curveItem || nodeItem)
                    && (d.model.HasValue() || d.vehicle.HasValue() || d.pickup.HasValue() || d.actor.HasValue()
                        || d.actorAnimation.HasValue() || d.slide.HasValue() || d.removeBuilding.HasValue()
                        || d.boxArea.HasValue() || d.layout.HasValue() || d.race.HasValue() || d.raceCheckpoint.HasValue()
                        || d.MaterialCount() || d.MaterialTextCount()))
                || (nodeItem && !d.curveNode.Get().IsValid()))
            {
                error = "Invalid curve hierarchy or components.";
                return false;
            }
            if (curveItem)
            {
                for (auto ancestor = parent; ancestor != parents.end();)
                {
                    if (ancestor->second->data.layout.HasValue())
                    {
                        error = "The referenced curve must be outside this Layout's hierarchy.";
                        return false;
                    }
                    ancestor = parents.find(ancestor->second->parentId);
                }
            }
            if (raceItem != d.race.HasValue() || checkpointItem != d.raceCheckpoint.HasValue()
                || (checkpointItem && (parent == parents.end() || parent->second->type != SceneItemRace))
                || (!checkpointItem && parent != parents.end()
                    && (parent->second->type == SceneItemRace || parent->second->type == SceneItemRaceCheckpoint))
                || ((raceItem || checkpointItem)
                    && (d.model.HasValue() || d.vehicle.HasValue() || d.pickup.HasValue() || d.actor.HasValue()
                        || d.actorAnimation.HasValue() || d.slide.HasValue() || d.removeBuilding.HasValue()
                        || d.boxArea.HasValue() || d.layout.HasValue() || d.MaterialCount() || d.MaterialTextCount())))
            {
                error = "Invalid race hierarchy or components.";
                return false;
            }
            parents.emplace(item.id, &item);
            if (raceItem)
            {
                const auto& race = d.race.Get();
                const auto nameEnd = std::find(std::begin(item.name), std::end(item.name), '\0');
                if (nameEnd == std::end(item.name) || nameEnd == std::begin(item.name) || nameEnd - item.name > RaceData::MaxName
                    || race.mode < 0 || race.mode > 1 || race.laps < 1 || race.laps > RaceData::MaxLaps)
                {
                    error = "Invalid race settings.";
                    return false;
                }
            }
            if (checkpointItem)
            {
                const auto& checkpoint = d.raceCheckpoint.Get();
                if (!std::isfinite(checkpoint.radius) || checkpoint.radius < RaceCheckpointData::MinRadius
                    || checkpoint.radius > RaceCheckpointData::MaxRadius || checkpoint.type < 0
                    || checkpoint.type > RaceCheckpointData::AutoAir)
                {
                    error = "Invalid race checkpoint.";
                    return false;
                }
            }
            if ((item.type < SceneItemNormal || item.type > SceneItemCurveNode) || !Finite(d.transform.pos)
                || !Finite(d.transform.rot) || d.MaterialCount() < 0 || d.MaterialCount() > ItemData::MaxMaterials
                || d.MaterialTextCount() < 0 || d.MaterialTextCount() > ItemData::MaxMaterials || d.nextSeq < 1
                || d.nextSeq >= std::numeric_limits<int>::max() - 64
                || (d.model.HasValue()
                    && (d.model.Get().modelId < 0 || d.model.Get().modelId >= 20000 || !std::isfinite(d.model.Get().drawDistance)
                        || d.model.Get().drawDistance < 0 || !std::isfinite(d.model.Get().streamDistance)
                        || d.model.Get().streamDistance < 0))
                || (d.vehicle.HasValue()
                    && (item.type != SceneItemNormal || d.model.HasValue() || d.slide.HasValue() || d.removeBuilding.HasValue()
                        || d.pickup.HasValue() || d.actor.HasValue() || d.MaterialCount() || d.MaterialTextCount()
                        || d.vehicle.Get().modelId < 400 || d.vehicle.Get().modelId > 611 || d.vehicle.Get().colors[0] < -1
                        || d.vehicle.Get().colors[0] > 255 || d.vehicle.Get().colors[1] < -1 || d.vehicle.Get().colors[1] > 255
                        || !std::isfinite(d.vehicle.Get().health) || d.vehicle.Get().health < VehicleData::MinHealth
                        || d.vehicle.Get().health > VehicleData::MaxHealth || d.vehicleSeq < 1 || d.vehicleSeq >= d.nextSeq))
                || (d.slide.HasValue()
                    && (!Finite(d.slide.Get().targetPos) || !Finite(d.slide.Get().targetRot)
                        || !std::isfinite(d.slide.Get().speed) || d.slide.Get().speed <= 0))
                || (d.removeBuilding.HasValue()
                    && (d.removeBuilding.Get().modelId < -1 || d.removeBuilding.Get().modelId >= 20000
                        || !std::isfinite(d.removeBuilding.Get().radius) || d.removeBuilding.Get().radius < 0)))
            {
                error = "Invalid project component data.";
                return false;
            }
            if (d.layout.HasValue()
                && (item.type != SceneItemNormal || d.model.HasValue() || d.vehicle.HasValue() || d.pickup.HasValue()
                    || d.actor.HasValue() || d.actorAnimation.HasValue() || d.slide.HasValue() || d.removeBuilding.HasValue()
                    || d.boxArea.HasValue() || d.MaterialCount() || d.MaterialTextCount() || d.layoutSeq < 1
                    || d.layoutSeq >= d.nextSeq || !d.layout.Get().IsValid()))
            {
                error = "Invalid project layout.";
                return false;
            }
            if (d.layout.HasValue() && d.layout.Get().curveId >= std::numeric_limits<int>::max() - SceneLimits::MaxItems)
            {
                error = "Invalid curve reference.";
                return false;
            }
            if (d.boxArea.HasValue())
            {
                const auto& area = d.boxArea.Get();
                if (item.type != SceneItemNormal || d.model.HasValue() || d.vehicle.HasValue() || d.pickup.HasValue()
                    || d.actor.HasValue() || d.removeBuilding.HasValue() || d.boxAreaSeq < 1 || d.boxAreaSeq >= d.nextSeq)
                {
                    error = "Invalid project box area.";
                    return false;
                }
                for (int axis = 0; axis < 3; ++axis)
                {
                    const float size = area.GetSize(axis);
                    if (!std::isfinite(size) || size < BoxAreaData::MinSize || size > BoxAreaData::MaxSize)
                    {
                        error = "Box area size must be between 0.25 m and 20000 m.";
                        return false;
                    }
                }
            }
            if (d.pickup.HasValue())
            {
                const auto& pickup = d.pickup.Get();
                if (item.type != SceneItemNormal || d.model.HasValue() || d.vehicle.HasValue() || d.boxArea.HasValue()
                    || d.actor.HasValue() || d.removeBuilding.HasValue() || d.slide.HasValue() || d.MaterialCount()
                    || d.MaterialTextCount() || pickup.modelId < 321 || pickup.modelId >= 20000
                    || (pickup.modelId >= 400 && pickup.modelId <= 614) || !std::isfinite(pickup.streamDistance)
                    || pickup.streamDistance < PickupData::MinStreamDistance
                    || pickup.streamDistance > PickupData::MaxStreamDistance || d.pickupSeq < 1 || d.pickupSeq >= d.nextSeq)
                {
                    error = "Invalid project pickup.";
                    return false;
                }
            }
            if (d.actor.HasValue())
            {
                const auto& actor = d.actor.Get();
                if (item.type != SceneItemNormal || d.model.HasValue() || d.vehicle.HasValue() || d.pickup.HasValue()
                    || d.boxArea.HasValue() || d.removeBuilding.HasValue() || d.slide.HasValue() || d.MaterialCount()
                    || d.MaterialTextCount() || !SkinCatalog::IsValid(actor.skinId) || !std::isfinite(actor.health)
                    || actor.health < ActorData::MinHealth || actor.health > ActorData::MaxHealth
                    || !std::isfinite(actor.streamDistance) || actor.streamDistance < ActorData::MinStreamDistance
                    || actor.streamDistance > ActorData::MaxStreamDistance || d.actorSeq < 1 || d.actorSeq >= d.nextSeq)
                {
                    error = "Invalid project actor.";
                    return false;
                }
            }
            if (d.actorAnimation.HasValue())
            {
                const auto& animation = d.actorAnimation.Get();
                if (!d.actor.HasValue()
                    || std::find(std::begin(animation.library), std::end(animation.library), '\0') == std::end(animation.library)
                    || std::find(std::begin(animation.name), std::end(animation.name), '\0') == std::end(animation.name)
                    || !AnimationCatalog::Find(animation.library, animation.name) || !std::isfinite(animation.delta)
                    || animation.delta < 0.0f || animation.time < 0 || d.actorAnimationSeq < 1
                    || d.actorAnimationSeq >= d.nextSeq)
                {
                    error = "Invalid project actor animation.";
                    return false;
                }
            }
            for (int i = 0; i < d.MaterialCount(); ++i)
            {
                const auto& m = d.materials[i];
                if (m.materialIndex < 0 || m.materialIndex > 15 || m.textureModelId < -1 || m.textureModelId >= 20000)
                {
                    error = "Invalid project material.";
                    return false;
                }
            }
            for (int i = 0; i < d.MaterialTextCount(); ++i)
            {
                const auto& m = d.materialTexts[i];
                if (m.materialIndex < 0 || m.materialIndex > 15 || m.materialSize < 10 || m.materialSize > 140
                    || m.materialSize % 10 || m.fontSize < 1 || m.fontSize > 255 || m.alignment < 0 || m.alignment > 2)
                {
                    error = "Invalid project material text.";
                    return false;
                }
            }
        }
        for (const auto& item : items)
        {
            if (!item.data.layout.HasValue() || item.data.layout.Get().curveId < 0)
            {
                continue;
            }
            const auto curve = parents.find(item.data.layout.Get().curveId);
            if (curve != parents.end() && curve->second->type != SceneItemCurve)
            {
                error = "Layout references an invalid curve item.";
                return false;
            }
        }
        return true;
    }
}
