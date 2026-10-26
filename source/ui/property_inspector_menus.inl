// Source Repository: https://github.com/waverso/waversomapstudio

void PropertyInspector::Add(ComponentType type, std::optional<int> model)
{
    if (drawing)
    {
        pending = [this, type, model]
        {
            Add(type, model);
        };
        return;
    }
    if (!Finish())
    {
        return;
    }
    PropertyWidgetResult result;
    result.changed = true;
    if (!Write(
            ImGui::GetID("##add_component"),
            [&](int index, ItemData& data, std::string& error)
            {
                if (!CanAddComponentType(g_scene->items[index], g_scene->data[index], type))
                {
                    error = "The selection cannot accept this component.";
                    return false;
                }
                const int materials = data.MaterialCount();
                AddComponentToItem(data, type);
                if (!ItemHasComponent(data, type) || (type == ComponentTypeMaterialTexture && data.MaterialCount() == materials))
                {
                    error = "Unable to create the component.";
                    return false;
                }
                if (model && !InspectionSetModel(data, index, type, *model, error))
                {
                    return false;
                }
                return true;
            },
            result))
    {
        return;
    }
    Finish();
    interrupted = true;
    int slot = -1;
    for (const auto& target : targets)
    {
        const auto& data = *target.data;
        const int addedSlot = type == ComponentTypeMaterialTexture ? data.MaterialCount() - 1
            : type == ComponentTypeMaterialText                    ? data.MaterialTextCount() - 1
                                                                   : -1;
        slot = slot < 0 ? addedSlot : ImMin(slot, addedSlot);
    }
    RequestComponentScroll(Primary(), type, slot);
}

void PropertyInspector::GlobalMenu(bool direct)
{
    static const std::pair<ComponentType, const char*> addable[]
        = { { ComponentTypeModel, "Object" }, { ComponentTypeVehicle, "Vehicle" }, { ComponentTypeActor, "Actor" },
              { ComponentTypePickup, "Pickup" }, { ComponentTypeBoxArea, "Box Area" }, { ComponentTypeLayout, "Layout" },
              { ComponentTypeActorAnimation, "Actor Animation" }, { ComponentTypeMaterialTexture, "Material Texture" },
              { ComponentTypeMaterialText, "Material Text" }, { ComponentTypeSlide, "Slide" } };
    const auto canAdd = [&](ComponentType type)
    {
        return !readOnly && !ids.empty()
            && All(
                [&](const ItemData& data, int index)
                {
                    return index >= 0 && CanAddComponentType(g_scene->items[index], data, type);
                });
    };
    bool anyAddable = false;
    for (const auto& [type, label] : addable)
    {
        anyAddable |= canAdd(type);
    }
    if (direct || ImGui::BeginMenu("Add Component", anyAddable))
    {
        for (const auto& [type, label] : addable)
        {
            if (type == ComponentTypeActorAnimation)
            {
                ImGui::Separator();
            }
            if (!ImGui::MenuItem(label, nullptr, false, canAdd(type)))
            {
                continue;
            }
            if (type == ComponentTypeModel || type == ComponentTypeVehicle || type == ComponentTypeActor)
            {
                const auto kind = type == ComponentTypeModel ? InspectionPicker::Kind::Model
                    : type == ComponentTypeVehicle           ? InspectionPicker::Kind::Vehicle
                                                             : InspectionPicker::Kind::Skin;
                if (!FreezePicker(kind, { type }, true))
                {
                    continue;
                }
                if (type == ComponentTypeModel)
                {
                    OpenObjectBrowser(ids.front());
                }
                else if (type == ComponentTypeVehicle)
                {
                    OpenVehicleBrowser(ids.front());
                }
                else
                {
                    OpenSkinBrowser(ids.front());
                }
            }
            else
            {
                Add(type);
            }
        }
        if (!direct)
        {
            ImGui::EndMenu();
        }
    }
    if (direct)
    {
        return;
    }
    ImGui::Separator();
    bool anyOpen = false, anyClosed = false;
    for (const auto component : components)
    {
        const int key = component.type * 32 + component.slot + 1;
        const auto found = open.find(key);
        const bool expanded = found == open.end() || found->second;
        anyOpen |= expanded;
        anyClosed |= !expanded;
    }
    if (ImGui::MenuItem("Collapse All", nullptr, false, anyOpen))
    {
        for (const auto component : components)
        {
            open[component.type * 32 + component.slot + 1] = false;
        }
    }
    if (ImGui::MenuItem("Expand All", nullptr, false, anyClosed))
    {
        for (const auto component : components)
        {
            open[component.type * 32 + component.slot + 1] = true;
        }
    }
}

static bool InspectionPaste(ItemData& data, int index, PropertyEdit::Component component, std::string& error)
{
    const auto& clipboard = g_componentClipboard;
    switch (component.type)
    {
        case ComponentTypeModel:
            if (!InspectionModelValid(component.type, clipboard.model.modelId))
            {
                error = "Invalid model ID.";
                return false;
            }
            data.model.Edit() = clipboard.model;
            break;
        case ComponentTypeVehicle:
            data.vehicle.Edit() = clipboard.vehicle;
            break;
        case ComponentTypePickup:
            data.pickup.Edit() = clipboard.pickup;
            break;
        case ComponentTypeActor:
            data.actor.Edit() = clipboard.actor;
            break;
        case ComponentTypeActorAnimation:
            data.actorAnimation.Edit() = clipboard.actorAnimation;
            break;
        case ComponentTypeSlide:
            data.slide.Edit() = clipboard.slide;
            break;
        case ComponentTypeBoxArea:
            data.boxArea.Edit() = clipboard.boxArea;
            break;
        case ComponentTypeRace:
            data.race.Edit() = clipboard.race;
            break;
        case ComponentTypeRaceCheckpoint:
            data.raceCheckpoint.Edit() = clipboard.raceCheckpoint;
            break;
        case ComponentTypeCurve:
            data.curve.Edit() = clipboard.curve;
            break;
        case ComponentTypeCurveNode:
            data.curveNode.Edit() = clipboard.curveNode;
            break;
        case ComponentTypeLayout:
            if (!clipboard.layout.IsValid()
                || (clipboard.layout.curveId >= 0
                    && !Curves::CanUse(*g_scene, index, g_scene->FindIndexById(clipboard.layout.curveId))))
            {
                error = "The Layout cannot use this Curve.";
                return false;
            }
            data.layout.Edit() = clipboard.layout;
            data.layout.Edit().modelPending = data.layout.Edit().piecesPending = true;
            break;
        case ComponentTypeMaterialTexture:
            data.materials[component.slot] = clipboard.material;
            break;
        case ComponentTypeMaterialText:
            data.materialTexts[component.slot] = clipboard.materialText;
            break;
        case ComponentTypeTransform:
            if (data.removeBuilding.HasValue() && !data.removeBuilding.Get().custom)
            {
                error = "This transform is read-only.";
                return false;
            }
            data.transform = clipboard.transform;
            break;
        default:
            return false;
    }
    return true;
}

void PropertyInspector::HeaderMenu(const ComponentMenuCtx* context)
{
    if (!context)
    {
        return;
    }
    const PropertyEdit::Component component { context->type, context->materialSlot };
    bool same = component.type != ComponentTypeRemoveBuilding;
    const auto& first = *targets.front().data;
    for (const auto& target : targets)
    {
        same &= PropertyEdit::SameComponent(first, *target.data, component);
        if (component.type == ComponentTypeTransform)
        {
            const auto position = InspectionWorldPosition(*target.data, target.index);
            const auto firstPosition = InspectionWorldPosition(first, targets.front().index);
            const auto rotation = InspectionRotation(*target.data, target.index, true);
            const auto firstRotation = InspectionRotation(first, targets.front().index, true);
            for (int axis = 0; axis < 3; ++axis)
            {
                same &= std::abs(position[axis] - firstPosition[axis]) < 0.00001f
                    && std::abs(std::remainder(rotation[axis] - firstRotation[axis], 360.0f)) < 0.00001f;
            }
        }
    }
    if (ImGui::MenuItem("Copy", nullptr, false, same))
    {
        TransformData world;
        const auto position = InspectionWorldPosition(first, targets.front().index);
        const auto rotation = InspectionRotation(first, targets.front().index, true);
        std::copy_n(position.data(), 3, world.pos);
        std::copy_n(rotation.data(), 3, world.rot);
        CopyComponentToClipboard(first, component.type, component.slot, world);
    }
    const bool canPaste = !readOnly && g_componentClipboard.valid && g_componentClipboard.type == component.type;
    if (ImGui::MenuItem("Paste Values", nullptr, false, canPaste))
    {
        if (!Finish())
        {
            return;
        }
        PropertyWidgetResult result;
        result.changed = true;
        std::vector<TransformData> locals;
        std::string error;
        bool valid = true;
        if (component.type == ComponentTypeTransform && g_gizmos->GetGizmoSpace() == GizmoSpace::Global)
        {
            std::vector<TransformData> poses(ids.size(), g_componentClipboard.worldTransform);
            valid = PropertyEdit::SetWorldPoses(*g_scene, ids, poses, locals, error);
        }
        if (valid)
        {
            Write(
                ImGui::GetID("##paste_values"),
                [&](int index, ItemData& data, std::string& error)
                {
                    if (component.type == ComponentTypeTransform)
                    {
                        const auto& pose = g_gizmos->GetGizmoSpace() == GizmoSpace::Global ? g_componentClipboard.worldTransform
                                                                                           : g_componentClipboard.transform;
                        const bool flat = data.actor.HasValue() || data.vehicle.HasValue()
                            || (data.model.HasValue() && !ModelConstraints::SupportsRotationXY(data.model.Get().modelId));
                        const bool fixed = !data.SupportsRotation();
                        if ((data.removeBuilding.HasValue() && !data.removeBuilding.Get().custom)
                            || ((flat || fixed) && (pose.rot[0] != 0 || pose.rot[1] != 0)) || (fixed && pose.rot[2] != 0))
                        {
                            error = "The selection cannot use this transform.";
                            return false;
                        }
                    }
                    if (component.type == ComponentTypeTransform && !locals.empty())
                    {
                        const auto target = std::find(ids.begin(), ids.end(), g_scene->items[index].id);
                        data.transform = locals[target - ids.begin()];
                        return true;
                    }
                    return InspectionPaste(data, index, component, error);
                },
                result);
        }
        else
        {
            g_toast.ShowError(error.c_str());
        }
        Finish();
    }
    const bool canPasteNew = !readOnly && g_componentClipboard.valid
        && All(
            [&](const ItemData& data, int index)
            {
                return index >= 0 && CanPasteNewComponent(g_scene->items[index], data, g_componentClipboard.type);
            });
    if (ImGui::MenuItem("Paste As New", nullptr, false, canPasteNew))
    {
        pending = [this]
        {
            if (!Finish())
            {
                return;
            }
            const auto type = g_componentClipboard.type;
            int slot = -1;
            PropertyWidgetResult result;
            result.changed = true;
            if (Write(
                    ImGui::GetID("##paste_new"),
                    [&](int index, ItemData& data, std::string& error)
                    {
                        if (!CanPasteNewComponent(g_scene->items[index], g_scene->data[index], type))
                        {
                            error = "The selection cannot accept this component.";
                            return false;
                        }
                        const int textureCount = data.MaterialCount(), textCount = data.MaterialTextCount();
                        AddComponentToItem(data, type);
                        PropertyEdit::Component added { type,
                            type == ComponentTypeMaterialTexture    ? textureCount
                                : type == ComponentTypeMaterialText ? textCount
                                                                    : -1 };
                        if (!ItemHasComponent(data, type)
                            || (type == ComponentTypeMaterialTexture && data.MaterialCount() == textureCount))
                        {
                            error = "Unable to create the component.";
                            return false;
                        }
                        slot = slot < 0 ? added.slot : ImMin(slot, added.slot);
                        return InspectionPaste(data, index, added, error);
                    },
                    result))
            {
                Finish();
                interrupted = true;
                RequestComponentScroll(Primary(), type, slot);
            }
        };
    }
    const bool canRemove = !readOnly
        && All(
            [&](const ItemData& data, int)
            {
                return PropertyEdit::CanRemove(data, component);
            });
    if (ImGui::MenuItem("Remove", nullptr, false, canRemove))
    {
        pending = [this, component]
        {
            if (!Finish())
            {
                return;
            }
            PropertyWidgetResult result;
            result.changed = true;
            Write(
                ImGui::GetID("##remove_component"),
                [&](int, ItemData& data, std::string& error)
                {
                    if (!PropertyEdit::CanRemove(data, component))
                    {
                        error = "The selection cannot remove this component.";
                        return false;
                    }
                    RemoveComponentFromItem(data, component.type, component.slot);
                    return true;
                },
                result);
            Finish();
            interrupted = true;
            if (component.type == ComponentTypeSlide && g_objectRuntime)
            {
                for (int id : ids)
                {
                    const int index = g_scene->FindIndexById(id);
                    if (index >= 0)
                    {
                        g_objectRuntime->StopSlide(index);
                    }
                }
            }
        };
    }
    ImGui::Separator();
    GlobalMenu();
}

static bool InspectionHeaderMenu(const ComponentMenuCtx* context)
{
    if (!g_propertyInspector.drawing)
    {
        return false;
    }
    g_propertyInspector.HeaderMenu(context);
    return true;
}
