// Source Repository: https://github.com/waverso/waversomapstudio

#include "property_edit.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "../utils/transform_math.h"
#include "actions.h"
#include "scene.h"

namespace PropertyEdit
{
    std::uint64_t Structure(const ItemData& data)
    {
        std::uint64_t signature = 0;
        for (int type = ComponentTypeTransform; type <= ComponentTypeCurveNode; ++type)
        {
            signature = signature * 31 + ItemHasComponent(data, static_cast<ComponentType>(type));
        }
        for (int sequence : { data.modelSeq, data.vehicleSeq, data.pickupSeq, data.actorSeq, data.actorAnimationSeq,
                 data.slideSeq, data.boxAreaSeq, data.layoutSeq })
        {
            signature = signature * 31 + sequence;
        }
        for (int slot = 0; slot < data.MaterialCount(); ++slot)
        {
            signature = signature * 31 + data.materials[slot].sequence;
        }
        for (int slot = 0; slot < data.MaterialTextCount(); ++slot)
        {
            signature = signature * 31 + data.materialTexts[slot].sequence;
        }
        signature = signature * 31 + data.nextSeq;
        return signature * 31 + data.MaterialCount() * 17 + data.MaterialTextCount();
    }

    bool ParseVector(const char* text, float values[3], unsigned& parts)
    {
        parts = 0;
        if (!text)
        {
            return false;
        }
        unsigned axis = 0;
        while (*text && axis < 3)
        {
            if (std::strncmp(text, MixedText, 3) == 0)
            {
                text += 3;
                ++axis;
            }
            else if (*text == '-' && (text[1] == ',' || std::isspace(static_cast<unsigned char>(text[1])) || text[1] == 0))
            {
                ++text;
                ++axis;
            }
            else
            {
                char* parsed = nullptr;
                const float value = std::strtof(text, &parsed);
                if (parsed != text)
                {
                    if (!std::isfinite(value))
                    {
                        return false;
                    }
                    values[axis] = value;
                    parts |= 1u << axis++;
                    text = parsed;
                }
                else
                {
                    while (std::isalnum(static_cast<unsigned char>(*text)) || *text == '_')
                    {
                        ++text;
                    }
                    if (*text)
                    {
                        ++text;
                    }
                }
            }
        }
        return axis == 3;
    }

    namespace
    {
        int Count(const ItemData& data, ComponentType type)
        {
            if (type == ComponentTypeMaterialTexture)
            {
                return data.MaterialCount();
            }
            if (type == ComponentTypeMaterialText)
            {
                return data.MaterialTextCount();
            }
            return ItemHasComponent(data, type) ? 1 : 0;
        }

        int Sequence(const ItemData& data, Component component)
        {
            switch (component.type)
            {
                case ComponentTypeModel:
                    return data.modelSeq;
                case ComponentTypeVehicle:
                    return data.vehicleSeq;
                case ComponentTypePickup:
                    return data.pickupSeq;
                case ComponentTypeActor:
                    return data.actorSeq;
                case ComponentTypeActorAnimation:
                    return data.actorAnimationSeq;
                case ComponentTypeSlide:
                    return data.slideSeq;
                case ComponentTypeBoxArea:
                    return data.boxAreaSeq;
                case ComponentTypeLayout:
                    return data.layoutSeq;
                case ComponentTypeMaterialTexture:
                    return data.materials[component.slot].sequence;
                case ComponentTypeMaterialText:
                    return data.materialTexts[component.slot].sequence;
                default:
                    return 0;
            }
        }
    }

    std::vector<Component> CommonComponents(const std::vector<const ItemData*>& targets, bool& partial)
    {
        std::vector<Component> result;
        partial = false;
        if (targets.empty())
        {
            return result;
        }
        for (int type = ComponentTypeTransform; type <= ComponentTypeCurveNode; ++type)
        {
            const auto componentType = static_cast<ComponentType>(type);
            int minimum = Count(*targets.front(), componentType), maximum = minimum;
            for (const auto* target : targets)
            {
                const int count = Count(*target, componentType);
                minimum = (std::min)(minimum, count);
                maximum = (std::max)(maximum, count);
            }
            partial |= minimum != maximum;
            for (int slot = 0; slot < minimum; ++slot)
            {
                Component component { componentType,
                    componentType == ComponentTypeMaterialTexture || componentType == ComponentTypeMaterialText ? slot : -1 };
                component.sequence = Sequence(*targets.front(), component);
                result.push_back(component);
            }
        }
        std::stable_sort(result.begin(), result.end(),
            [](Component first, Component other)
            {
                if (first.type == ComponentTypeTransform || other.type == ComponentTypeTransform)
                {
                    return first.type == ComponentTypeTransform && other.type != ComponentTypeTransform;
                }
                return first.sequence < other.sequence;
            });
        return result;
    }

    bool CanRemove(const ItemData& data, Component component)
    {
        switch (component.type)
        {
            case ComponentTypeTransform:
            case ComponentTypeRemoveBuilding:
            case ComponentTypeRace:
            case ComponentTypeRaceCheckpoint:
            case ComponentTypeCurve:
            case ComponentTypeCurveNode:
                return false;
            case ComponentTypeModel:
                return !data.slide.HasValue() && !data.MaterialCount() && !data.MaterialTextCount();
            case ComponentTypeActor:
                return !data.actorAnimation.HasValue();
            default:
                return Count(data, component.type) > (std::max)(0, component.slot);
        }
    }

    bool SameComponent(const ItemData& first, const ItemData& other, Component component)
    {
        switch (component.type)
        {
            case ComponentTypeTransform:
                return first.transform == other.transform;
            case ComponentTypeModel:
                return first.model == other.model;
            case ComponentTypeVehicle:
                return first.vehicle == other.vehicle;
            case ComponentTypePickup:
                return first.pickup == other.pickup;
            case ComponentTypeActor:
                return first.actor == other.actor;
            case ComponentTypeActorAnimation:
                return first.actorAnimation == other.actorAnimation;
            case ComponentTypeSlide:
                return first.slide == other.slide;
            case ComponentTypeBoxArea:
                return first.boxArea == other.boxArea;
            case ComponentTypeRemoveBuilding:
                return first.removeBuilding == other.removeBuilding;
            case ComponentTypeRace:
                return first.race == other.race;
            case ComponentTypeRaceCheckpoint:
                return first.raceCheckpoint == other.raceCheckpoint;
            case ComponentTypeCurve:
                return first.curve == other.curve;
            case ComponentTypeCurveNode:
                return first.curveNode == other.curveNode;
            case ComponentTypeLayout:
            {
                auto a = first.layout.Get(), b = other.layout.Get();
                a.modelPending = a.piecesPending = b.modelPending = b.piecesPending = false;
                return a == b;
            }
            case ComponentTypeMaterialTexture:
                return static_cast<const MaterialData&>(first.materials[component.slot])
                    == static_cast<const MaterialData&>(other.materials[component.slot]);
            case ComponentTypeMaterialText:
                return static_cast<const MaterialTextData&>(first.materialTexts[component.slot])
                    == static_cast<const MaterialTextData&>(other.materialTexts[component.slot]);
            default:
                return false;
        }
    }

    bool Session::Begin(EditorScene& scene, const std::vector<int>& ids, unsigned control, std::string& error)
    {
        if (!g_sceneActions || ids.empty())
        {
            error = "No editable items selected.";
            return false;
        }
        size_t bytes = sizeof(ItemDataEditAction) + ids.size() * sizeof(ItemDataEditAction::Entry);
        for (int id : ids)
        {
            const int index = scene.FindIndexById(id);
            if (index < 0)
            {
                error = "The selection changed.";
                return false;
            }
            bytes += scene.data[index].DynamicMemoryUsage() * 2;
        }
        if (bytes > ActionManager::MaxHistoryBytes)
        {
            error = "Edit exceeds the undo memory limit.";
            return false;
        }
        std::vector<ItemDataEditAction::Entry> entries(0);
        std::vector<ItemData> prepared(0);
        std::vector<Identity> names(0);
        entries.reserve(ids.size());
        prepared.reserve(ids.size());
        names.reserve(ids.size());
        for (int id : ids)
        {
            const int index = scene.FindIndexById(id);
            entries.push_back({ id, scene.data[index], scene.data[index] });
            prepared.push_back(scene.data[index]);
            Identity identity { id, scene.items[index].searchModelId };
            std::copy_n(scene.items[index].name, identity.name.size(), identity.name.begin());
            names.push_back(identity);
        }
        auto pending = std::make_unique<ItemDataEditAction>(std::move(entries));
        action = std::move(pending);
        working = std::move(prepared);
        identities = std::move(names);
        owner = control;
        return true;
    }

    bool Session::Write(EditorScene& scene, const std::vector<int>& ids, unsigned control, const Patch& patch, std::string& error)
    {
        error.clear();
        try
        {
            if (action && owner != control && !Finish(error))
            {
                return false;
            }
            if (!action && !Begin(scene, ids, control, error))
            {
                return false;
            }
            auto& entries = action->Entries();
            if (entries.size() != ids.size())
            {
                error = "The selection changed.";
                Cancel(scene);
                return false;
            }
            for (size_t i = 0; i < entries.size(); ++i)
            {
                const int index = scene.FindIndexById(entries[i].itemId);
                if (index < 0 || entries[i].itemId != ids[i] || Structure(scene.data[index]) != Structure(entries[i].after))
                {
                    error = "The selection changed.";
                    Cancel(scene);
                    return false;
                }
                working[i] = scene.data[index];
                if (!patch(index, working[i], error))
                {
                    Cancel(scene);
                    return false;
                }
            }
            for (size_t i = 0; i < entries.size(); ++i)
            {
                entries[i].after = working[i];
            }
            if (action->MemoryUsage() > ActionManager::MaxHistoryBytes)
            {
                error = "Edit exceeds the undo memory limit.";
                Cancel(scene);
                return false;
            }
            for (size_t i = 0; i < entries.size(); ++i)
            {
                std::swap(scene.data[scene.FindIndexById(entries[i].itemId)], working[i]);
            }
            return true;
        }
        catch (const std::bad_alloc&)
        {
            error = "Out of memory.";
            Cancel(scene);
            return false;
        }
    }

    bool Session::Finish(std::string& error)
    {
        if (!action)
        {
            return true;
        }
        auto& entries = action->Entries();
        std::erase_if(entries,
            [](const auto& entry)
            {
                return entry.before == entry.after;
            });
        if (!entries.empty())
        {
            std::unique_ptr<IAction> completed = std::move(action);
            if (!g_sceneActions || !g_sceneActions->RecordApplied(completed))
            {
                action.reset(static_cast<ItemDataEditAction*>(completed.release()));
                error = "Unable to record the edit in the undo history.";
                return false;
            }
        }
        Reset();
        return true;
    }

    bool Session::CaptureAfter(EditorScene& scene, std::string& error)
    {
        if (!action)
        {
            return true;
        }
        try
        {
            for (auto& entry : action->Entries())
            {
                const int index = scene.FindIndexById(entry.itemId);
                if (index < 0)
                {
                    error = "The selection changed.";
                    Cancel(scene);
                    return false;
                }
                entry.after = scene.data[index];
            }
            if (action->MemoryUsage() > ActionManager::MaxHistoryBytes)
            {
                error = "Edit exceeds the undo memory limit.";
                Cancel(scene);
                return false;
            }
            return true;
        }
        catch (const std::bad_alloc&)
        {
            error = "Out of memory.";
            Cancel(scene);
            return false;
        }
    }

    void Session::Cancel(EditorScene& scene) noexcept
    {
        if (action)
        {
            for (auto& entry : action->Entries())
            {
                const int index = scene.FindIndexById(entry.itemId);
                if (index >= 0)
                {
                    scene.data[index] = std::move(entry.before);
                }
            }
            for (const auto& identity : identities)
            {
                const int index = scene.FindIndexById(identity.id);
                if (index >= 0)
                {
                    scene.items[index].searchModelId = identity.model;
                    std::copy(identity.name.begin(), identity.name.end(), scene.items[index].name);
                }
            }
        }
        Reset();
    }

    bool Session::Contains(int id) const noexcept
    {
        return action
            && std::any_of(identities.begin(), identities.end(),
                [id](const auto& entry)
                {
                    return entry.id == id;
                });
    }

    void Session::Reset() noexcept
    {
        action.reset();
        working.clear();
        identities.clear();
        owner = 0;
    }

    bool SetWorldPoses(EditorScene& scene, const std::vector<int>& ids, const std::vector<TransformData>& poses,
        std::vector<TransformData>& locals, std::string& error)
    {
        if (ids.size() != poses.size())
        {
            return false;
        }
        std::vector<int> targets(scene.count, -1);
        std::vector<EditorScene::WorldTransform> final(scene.count);
        locals.resize(ids.size());
        for (size_t i = 0; i < ids.size(); ++i)
        {
            const int index = scene.FindIndexById(ids[i]);
            if (index < 0)
            {
                error = "The selection changed.";
                return false;
            }
            targets[index] = static_cast<int>(i);
        }
        for (int index : scene.DisplayOrder())
        {
            const int target = targets[index];
            const bool composed = scene.ComposesWithParent(index);
            auto& world = final[index];
            if (target >= 0)
            {
                auto pose = poses[target];
                pose.pos[2] = (std::max)(pose.pos[2], TransformData::MinZ);
                auto& local = locals[target];
                world.pos = { pose.pos[0], pose.pos[1], pose.pos[2] };
                world.rot = Utils::EulerDegreesToBasis(pose.rot);
                if (composed)
                {
                    const auto& parent = final[scene.items[index].parent];
                    const auto inverse = Utils::TransposeRotationBasis(parent.rot);
                    const auto position = Utils::TransformDirection(inverse, world.pos - parent.pos);
                    local.pos[0] = position.x;
                    local.pos[1] = position.y;
                    local.pos[2] = position.z;
                    Utils::BasisToEulerDegrees(Utils::ComposeRotationBases(inverse, world.rot), local.rot);
                    if (scene.data[index].vehicle.HasValue() || scene.data[index].actor.HasValue())
                    {
                        local.rot[0] = local.rot[1] = 0;
                        local.rot[2] = pose.rot[2] - parent.eulerDeg[2];
                    }
                }
                else
                {
                    local = pose;
                }
                std::copy_n(pose.rot, 3, world.eulerDeg);
            }
            else
            {
                const auto& local = scene.data[index].transform;
                world.pos = { local.pos[0], local.pos[1], local.pos[2] };
                world.rot = Utils::EulerDegreesToBasis(local.rot);
                if (composed)
                {
                    const auto& parent = final[scene.items[index].parent];
                    world.pos = parent.pos + Utils::TransformDirection(parent.rot, world.pos);
                    world.rot = Utils::ComposeRotationBases(parent.rot, world.rot);
                }
                Utils::BasisToEulerDegrees(world.rot, world.eulerDeg);
            }
            world.pos.z = (std::max)(world.pos.z, TransformData::MinZ);
            const auto& data = scene.data[index];
            if (data.vehicle.HasValue() || data.actor.HasValue())
            {
                const auto& local = target >= 0 ? locals[target] : data.transform;
                world.eulerDeg[0] = world.eulerDeg[1] = 0;
                world.eulerDeg[2] = local.rot[2] + (composed ? final[scene.items[index].parent].eulerDeg[2] : 0);
                world.rot = Utils::EulerDegreesToBasis(world.eulerDeg);
            }
            if (data.HasFixedRotation())
            {
                if (target >= 0)
                {
                    std::fill_n(locals[target].rot, 3, 0.0f);
                }
                std::fill_n(world.eulerDeg, 3, 0.0f);
                world.rot = Utils::EulerDegreesToBasis(world.eulerDeg);
            }
        }
        return true;
    }
}
