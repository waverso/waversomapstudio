// Source Repository: https://github.com/waverso/waversomapstudio

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "CBaseModelInfo.h"
#include "CModelInfo.h"
#include "scene.h"
#include "../textures/texture_browser.h"

bool ItemHasComponent(const ItemData& data, ComponentType type)
{
    switch (type)
    {
        case ComponentTypeTransform:
            return true;
        case ComponentTypeVehicle:
            return data.vehicle.HasValue();
        case ComponentTypePickup:
            return data.pickup.HasValue();
        case ComponentTypeActor:
            return data.actor.HasValue();
        case ComponentTypeActorAnimation:
            return data.actorAnimation.HasValue();
        case ComponentTypeModel:
            return data.model.HasValue();
        case ComponentTypeSlide:
            return data.slide.HasValue();
        case ComponentTypeMaterialTexture:
            return data.MaterialCount() > 0;
        case ComponentTypeMaterialText:
            return data.MaterialTextCount() > 0;
        case ComponentTypeRemoveBuilding:
            return data.removeBuilding.HasValue();
        case ComponentTypeBoxArea:
            return data.boxArea.HasValue();
        case ComponentTypeRace:
            return data.race.HasValue();
        case ComponentTypeRaceCheckpoint:
            return data.raceCheckpoint.HasValue();
        case ComponentTypeLayout:
            return data.layout.HasValue();
        case ComponentTypeCurve:
            return data.curve.HasValue();
        case ComponentTypeCurveNode:
            return data.curveNode.HasValue();
    }
    return false;
}

bool CanAddComponentType(const SceneItem& item, const ItemData& data, ComponentType type)
{
    if (item.type != SceneItemNormal || data.layout.HasValue())
    {
        return false;
    }
    if (type == ComponentTypeLayout && g_scene)
    {
        const int target = g_scene->FindIndexById(item.id);
        if (target >= 0 && &g_scene->data[target] == &data)
        {
            for (int index = 0; index < g_scene->count; ++index)
            {
                if (g_scene->data[index].curve.HasValue() && g_scene->IsDescendantOf(index, target))
                {
                    return false;
                }
            }
        }
    }
    const bool hasExclusiveComponent = data.model.HasValue() || data.vehicle.HasValue() || data.boxArea.HasValue()
        || data.pickup.HasValue() || data.actor.HasValue();
    switch (type)
    {
        case ComponentTypeLayout:
            return !hasExclusiveComponent && !data.slide.HasValue() && !data.actorAnimation.HasValue()
                && !data.removeBuilding.HasValue() && !data.race.HasValue() && !data.raceCheckpoint.HasValue()
                && !data.MaterialCount() && !data.MaterialTextCount();
        case ComponentTypeVehicle:
        case ComponentTypePickup:
        case ComponentTypeActor:
            return !hasExclusiveComponent && !data.slide.HasValue() && !data.MaterialCount() && !data.MaterialTextCount()
                && !data.removeBuilding.HasValue();
        case ComponentTypeModel:
            return !hasExclusiveComponent;
        case ComponentTypeSlide:
            return !data.slide.HasValue() && data.model.HasValue();
        case ComponentTypeActorAnimation:
            return data.actor.HasValue() && !data.actorAnimation.HasValue();
        case ComponentTypeBoxArea:
            return !hasExclusiveComponent && !data.removeBuilding.HasValue();
        case ComponentTypeMaterialTexture:
            return data.MaterialCount() < 16 && data.model.HasValue();
        case ComponentTypeMaterialText:
            return data.MaterialTextCount() < 16 && data.model.HasValue();
        default:
            return false;
    }
}

bool SetDefaultMaterialTexture(MaterialData& material)
{
    const int row = TextureBrowser::FindRow("LSOffice1Floors", "ceilingtiles3-128x128");
    if (row < 0)
    {
        return false;
    }
    const auto texture = TextureBrowser::GetRow(static_cast<std::uint32_t>(row));
    if (texture.modelId <= 0)
    {
        return false;
    }
    material.textureModelId = texture.modelId;
    strncpy_s(material.txdName, texture.txdName, _TRUNCATE);
    strncpy_s(material.textureName, texture.textureName, _TRUNCATE);
    return true;
}

void AddComponentToItem(ItemData& data, ComponentType type)
{
    if (data.race.HasValue() || data.raceCheckpoint.HasValue() || data.layout.HasValue() || data.curve.HasValue()
        || data.curveNode.HasValue())
    {
        return;
    }
    const bool hasExclusiveComponent = data.model.HasValue() || data.vehicle.HasValue() || data.boxArea.HasValue()
        || data.pickup.HasValue() || data.actor.HasValue();
    switch (type)
    {
        case ComponentTypeLayout:
            if (!hasExclusiveComponent && !data.slide.HasValue() && !data.actorAnimation.HasValue()
                && !data.removeBuilding.HasValue() && !data.MaterialCount() && !data.MaterialTextCount())
            {
                data.layout.Edit();
                data.layoutSeq = data.nextSeq++;
            }
            break;
        case ComponentTypeActorAnimation:
            if (data.actor.HasValue() && !data.actorAnimation.HasValue())
            {
                data.actorAnimation.Edit();
                data.actorAnimationSeq = data.nextSeq++;
            }
            break;
        case ComponentTypeActor:
            if (!hasExclusiveComponent && !data.slide.HasValue() && !data.MaterialCount() && !data.MaterialTextCount()
                && !data.removeBuilding.HasValue())
            {
                data.actor.Edit();
                data.actorSeq = data.nextSeq++;
                data.transform.rot[0] = data.transform.rot[1] = 0.0f;
            }
            break;
        case ComponentTypePickup:
            if (!hasExclusiveComponent && !data.slide.HasValue() && !data.MaterialCount() && !data.MaterialTextCount()
                && !data.removeBuilding.HasValue())
            {
                data.pickup.Edit();
                data.pickupSeq = data.nextSeq++;
                std::fill_n(data.transform.rot, 3, 0.0f);
            }
            break;
        case ComponentTypeVehicle:
            if (!hasExclusiveComponent && !data.slide.HasValue() && !data.MaterialCount() && !data.MaterialTextCount()
                && !data.removeBuilding.HasValue())
            {
                data.vehicle.Edit();
                data.vehicleSeq = data.nextSeq++;
                data.transform.rot[0] = data.transform.rot[1] = 0.0f;
            }
            break;
        case ComponentTypeModel:
            if (!hasExclusiveComponent)
            {
                data.model.Edit();
                data.modelSeq = data.nextSeq++;
            }
            break;
        case ComponentTypeSlide:
            if (!data.slide.HasValue())
            {
                data.slide.Edit();
                data.slideSeq = data.nextSeq++;
            }
            break;
        case ComponentTypeMaterialTexture:
            if (data.MaterialCount() < 16)
            {
                MaterialData material;
                if (!SetDefaultMaterialTexture(material))
                {
                    break;
                }
                data.materials.emplace_back();
                data.materials.back() = material;
                data.materials.back().sequence = data.nextSeq++;
            }
            break;
        case ComponentTypeBoxArea:
            if (!hasExclusiveComponent && !data.removeBuilding.HasValue())
            {
                data.boxArea.Edit();
                data.boxAreaSeq = data.nextSeq++;
            }
            break;
        case ComponentTypeMaterialText:
            if (data.MaterialTextCount() < 16)
            {
                data.materialTexts.emplace_back();
                data.materialTexts.back().sequence = data.nextSeq++;
            }
            break;
        default:
            break;
    }
}

void ApplyModelDistanceDefaults(ModelData& m)
{
    float dd = 300.0f;
    CBaseModelInfo* mi = (m.modelId > 0 && m.modelId < 20000) ? CModelInfo::GetModelInfo(m.modelId) : nullptr;
    if (mi && mi->m_fDrawDistance > 0.0f)
    {
        dd = mi->m_fDrawDistance;
    }
    m.drawDistance = std::min(std::max(dd, 10.0f), 3000.0f);
    m.streamDistance = std::min(std::max(dd + 100.0f, 300.0f), 3000.0f);
}

void RemoveComponentFromItem(ItemData& data, ComponentType type, int materialSlot)
{
    switch (type)
    {
        case ComponentTypeVehicle:
            data.vehicle.Reset();
            break;
        case ComponentTypePickup:
            data.pickup.Reset();
            break;
        case ComponentTypeActor:
            data.actorAnimation.Reset();
            data.actor.Reset();
            break;
        case ComponentTypeActorAnimation:
            data.actorAnimation.Reset();
            break;
        case ComponentTypeModel:
            data.model.Reset();
            break;
        case ComponentTypeSlide:
            data.slide.Reset();
            break;
        case ComponentTypeBoxArea:
            data.boxArea.Reset();
            break;
        case ComponentTypeLayout:
            data.layout.Reset();
            data.layoutSeq = 0;
            break;
        case ComponentTypeMaterialTexture:
            if (materialSlot >= 0 && materialSlot < data.MaterialCount())
            {
                for (int i = materialSlot; i < data.MaterialCount() - 1; ++i)
                {
                    data.materials[i] = data.materials[i + 1];
                }
                data.materials.pop_back();
            }
            break;
        case ComponentTypeMaterialText:
            if (materialSlot >= 0 && materialSlot < data.MaterialTextCount())
            {
                for (int i = materialSlot; i < data.MaterialTextCount() - 1; ++i)
                {
                    data.materialTexts[i] = data.materialTexts[i + 1];
                }
                data.materialTexts.pop_back();
            }
            break;
        default:
            break;
    }
}
