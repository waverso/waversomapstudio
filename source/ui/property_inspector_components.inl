// Source Repository: https://github.com/waverso/waversomapstudio

static const char* const g_kMaterialSizeLabels[14] = { "32x32", "64x32", "64x64", "128x32", "128x64", "128x128", "256x32",
    "256x64", "256x128", "256x256", "512x64", "512x128", "512x256", "512x512" };
static const int g_kMaterialSizeValues[14] = { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140 };
static const char* const g_kAlignLabels[3] = { "Left", "Center", "Right" };

void PropertyInspector::ComponentFields(PropertyEdit::Component component)
{
    const int slot = component.slot;
    switch (component.type)
    {
        case ComponentTypeTransform:
        {
            const bool global = readOnly || g_gizmos->GetGizmoSpace() == GizmoSpace::Global;
            const bool enabled = All(
                [](const ItemData& data, int)
                {
                    return !data.removeBuilding.HasValue() || data.removeBuilding.Get().custom;
                });
            unsigned disabledRotation = 0;
            for (const auto& target : targets)
            {
                const auto& data = *target.data;
                disabledRotation |= !data.SupportsRotation() ? 7u
                    : data.actor.HasValue() || data.vehicle.HasValue()
                        || (data.model.HasValue() && !ModelConstraints::SupportsRotationXY(data.model.Get().modelId))
                    ? 3u
                    : 0u;
            }
            for (int rotation = 0; rotation < 2; ++rotation)
            {
                if (rotation)
                {
                    ImGui::SetCursorScreenPos(
                        ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetItemRectMax().y + Theme::g_padding));
                }
                auto field = InspectField(
                    [=](const ItemData& data, int index)
                    {
                        if (rotation)
                        {
                            return InspectionRotation(data, index, global);
                        }
                        if (global)
                        {
                            return InspectionWorldPosition(data, index);
                        }
                        return std::array { data.transform.pos[0], data.transform.pos[1], data.transform.pos[2] };
                    },
                    [=](ItemData& data, int index, const std::array<float, 3>& value, std::string&)
                    {
                        std::copy_n(value.data(), 3, rotation ? data.transform.rot : data.transform.pos);
                        if (!rotation && g_scene->ComposesWithParent(index))
                        {
                            const auto& parent = g_scene->world[g_scene->items[index].parent];
                            auto position = parent.pos + Utils::TransformDirection(parent.rot, { value[0], value[1], value[2] });
                            if (position.z < TransformData::MinZ)
                            {
                                position.z = TransformData::MinZ;
                                const auto local = Utils::InverseTransformDirection(parent.rot, position - parent.pos);
                                data.transform.pos[0] = local.x;
                                data.transform.pos[1] = local.y;
                                data.transform.pos[2] = local.z;
                            }
                        }
                        return true;
                    });
                if (!global)
                {
                    Vector(rotation ? "Rotation" : "Position", rotation ? "##rotation" : "##position", field, rotation != 0,
                        false, rotation ? disabledRotation : 0, enabled);
                    continue;
                }
                const char* key = rotation ? "##rotation" : "##position";
                const ImGuiID owner = ImGui::GetID(key);
                float values[3];
                unsigned mixed = 0;
                for (int axis = 0; axis < 3; ++axis)
                {
                    const auto value = Scalar(owner + axis + 1,
                        [&](const ItemData& data, int index)
                        {
                            return field.read(data, index)[axis];
                        });
                    values[axis] = value.value;
                    if (value.mixed)
                    {
                        mixed |= 1u << axis;
                    }
                }
                Row(rotation ? "Rotation" : "Position");
                PropertyWidgetResult result;
                ImGui::BeginDisabled(!enabled);
                DrawVec3Field(key, buffers[owner].vector, values, width, false, readOnly, rotation ? disabledRotation : 0,
                    nullptr, rotation != 0, -20000, 20000, mixed, &result);
                ImGui::EndDisabled();
                if (result.changed && !readOnly)
                {
                    std::vector<TransformData> desired;
                    desired.reserve(ids.size());
                    for (int id : ids)
                    {
                        const int index = g_scene->FindIndexById(id);
                        TransformData pose;
                        const auto position = InspectionWorldPosition(g_scene->data[index], index);
                        std::copy_n(position.data(), 3, pose.pos);
                        std::copy_n(g_scene->world[index].eulerDeg, 3, pose.rot);
                        for (int axis = 0; axis < 3; ++axis)
                        {
                            if (result.parts & (1u << axis))
                            {
                                (rotation ? pose.rot : pose.pos)[axis] = values[axis];
                            }
                        }
                        desired.push_back(pose);
                    }
                    std::string error;
                    std::vector<TransformData> locals;
                    if (PropertyEdit::SetWorldPoses(*g_scene, ids, desired, locals, error))
                    {
                        Write(
                            owner,
                            [&](int index, ItemData& data, std::string&)
                            {
                                const auto target = std::find(ids.begin(), ids.end(), g_scene->items[index].id);
                                data.transform = locals[target - ids.begin()];
                                return true;
                            },
                            result);
                    }
                    else
                    {
                        g_toast.ShowError(error.c_str());
                    }
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        if (result.parts & (1u << axis))
                        {
                            buffers[owner + axis + 1].seed = values[axis];
                        }
                    }
                }
                Observe(result);
            }
            break;
        }
        case ComponentTypeModel:
            Model(component.type);
            Float("Draw Dist", "##draw_distance", INSPECT(model, drawDistance), 0, 3000, 10, 1, "%.0f m", false);
            Float("Stream Dist", "##stream_distance", INSPECT(model, streamDistance), 10, 3000, 10, 1, "%.0f m", false);
            if (const auto value = Values(INSPECT(model, modelId).read); !value.mixed)
            {
                DrawModelInfoCard(value.value, width);
            }
            else
            {
                MixedCard("##model_info", 3);
            }
            break;
        case ComponentTypeVehicle:
        {
            Model(component.type);
            Row("Colors");
            const float colorWidth = floorf((FieldWidth() - Gap()) * 0.5f);
            for (int colorSlot = 0; colorSlot < 2; ++colorSlot)
            {
                ImGui::PushID(colorSlot);
                if (colorSlot)
                {
                    ImGui::SameLine(0, Gap());
                }
                auto field = InspectField(
                    [=](const ItemData& data, int)
                    {
                        return data.vehicle.Get().colors[colorSlot];
                    },
                    [=](ItemData& data, int, int value, std::string&)
                    {
                        data.vehicle.Edit().colors[colorSlot] = value;
                        return true;
                    });
                const ImGuiID owner = ImGui::GetID("##vehicle_color");
                auto value = Scalar(owner, field.read);
                const bool enabled = std::any_of(targets.begin(), targets.end(),
                    [=](const auto& target)
                    {
                        const auto* model = VehicleCatalog::Get(target.data->vehicle.Get().modelId);
                        return model && model->colorSlots > colorSlot;
                    });
                ImGui::BeginDisabled(!enabled);
                PropertyWidgetResult result;
                DrawVehicleColorPicker(value.value, colorWidth, Theme::g_frameHeight, Gap(), value.mixed, &result);
                ImGui::EndDisabled();
                if (result.changed)
                {
                    buffers[owner].seed = value.value;
                }
                Set(owner, field, value.value, result);
                ImGui::PopID();
            }
            Float("Health", "##health", INSPECT(vehicle, health), 0, VehicleData::MaxHealth, 50, 1, "%.1f", false);
            if (const auto value = Values(INSPECT(vehicle, modelId).read); !value.mixed)
            {
                DrawVehicleComponentInfoCard(value.value, width);
            }
            else
            {
                MixedCard("##vehicle_info", 3);
            }
            break;
        }
        case ComponentTypePickup:
            Model(component.type);
            Float("Stream Dist", "##stream_distance", INSPECT(pickup, streamDistance), 10, 3000, 10, 1, "%.0f m", false);
            if (const auto value = Values(INSPECT(pickup, modelId).read); !value.mixed)
            {
                DrawModelInfoCard(value.value, width, false);
            }
            else
            {
                MixedCard("##model_info", 3);
            }
            break;
        case ComponentTypeActor:
            Model(component.type);
            Float("Health", "##health", INSPECT(actor, health), 0, ActorData::MaxHealth, 50, 1, "%.1f", false);
            Float("Stream Dist", "##stream_distance", INSPECT(actor, streamDistance), 10, 3000, 10, 1, "%.0f m", false);
            Check("Invulnerable", "##invulnerable", INSPECT(actor, invulnerable));
            if (const auto value = Values(INSPECT(actor, skinId).read); !value.mixed)
            {
                DrawActorComponentInfoCard(value.value, width);
            }
            else
            {
                MixedCard("##actor_info", 3);
            }
            break;
        case ComponentTypeActorAnimation:
        {
            auto field = InspectField(
                [](const ItemData& data, int)
                {
                    const auto& animation = data.actorAnimation.Get();
                    const auto* entry = AnimationCatalog::Find(animation.library, animation.name);
                    return entry ? entry->id : 0;
                },
                [](ItemData& data, int, int id, std::string& error)
                {
                    if (!AnimationCatalog::Get(id))
                    {
                        error = "Invalid animation ID.";
                        return false;
                    }
                    AnimationBrowser::Select(data.actorAnimation.Edit(), id);
                    return true;
                });
            const auto value = Values(field.read);
            const auto identity = Values(
                [](const ItemData& data, int)
                {
                    return std::pair(
                        std::string_view(data.actorAnimation.Get().library), std::string_view(data.actorAnimation.Get().name));
                });
            const ImRect anchor = Identifier(
                "Anim ID", "##actor_animation_id", field, 1, AnimationCatalog::Count, true,
                [&]
                {
                    if (!FreezePicker(InspectionPicker::Kind::Animation, component))
                    {
                        return;
                    }
                    auto& ui = g_animationBrowserUi;
                    AnimationBrowser::RunFilter(ui.filter, ui.results);
                    ui.dirty = false;
                    ui.focusSearch = true;
                    const auto found = std::find(ui.results.begin(), ui.results.end(), value.value);
                    ui.cursor = found == ui.results.end() ? 0 : static_cast<int>(found - ui.results.begin());
                    g_actorAnimationPopupId = ImGui::GetID("##actor_animation_popup");
                    ImGui::OpenPopup("##actor_animation_popup");
                },
                identity.mixed, nullptr, "##actor_animation_browse");
            auto animation = targets.front().data->actorAnimation.Get();
            DrawAnimationBrowserPopup(animation, anchor, Primary());
            Float("Blend Delta", "##actor_animation_delta", INSPECT(actorAnimation, delta), 0, FLT_MAX, 0.1f);
            Int("Time Limit", "##actor_animation_time", INSPECT(actorAnimation, time), 0, INT_MAX, 100, 1, "%d ms");
            const char* labels[] = { "Loop", "Lock X", "Lock Y", "Freeze" };
            bool values[4];
            unsigned mixed = 0;
            const auto fields = std::array { &ActorAnimationData::loop, &ActorAnimationData::lockX, &ActorAnimationData::lockY,
                &ActorAnimationData::freeze };
            for (int part = 0; part < 4; ++part)
            {
                const auto flag = Values(InspectMember(&ItemData::actorAnimation, fields[part]).read);
                values[part] = flag.value;
                if (flag.mixed)
                {
                    mixed |= 1u << part;
                }
            }
            bool* pointers[] = { &values[0], &values[1], &values[2], &values[3] };
            PropertyWidgetResult result;
            DrawToggleButtonRow(ImGui::GetWindowDrawList(), "Options", labels, pointers, FieldWidth(), Gap(), mixed, &result);
            if (result.changed)
            {
                Write(
                    ImGui::GetID("##animation_options"),
                    [&](int, ItemData& data, std::string&)
                    {
                        for (int part = 0; part < 4; ++part)
                        {
                            if (result.parts & (1u << part))
                            {
                                data.actorAnimation.Edit().*fields[part] = values[part];
                            }
                        }
                        return true;
                    },
                    result);
            }
            Observe(result);
            if (identity.mixed)
            {
                MixedCard("##animation_info", 3, false);
            }
            else
            {
                DrawAnimationComponentInfoCard(animation, width);
            }
            break;
        }
        case ComponentTypeSlide:
        {
            Vector("Target Pos", "##slide_position", INSPECT_VECTOR(slide, SlideData, targetPos), false, true);
            Vector("Target Rot", "##slide_rotation", INSPECT_VECTOR(slide, SlideData, targetRot), true, true);
            Float("Speed", "##slide_speed", INSPECT(slide, speed), 0.01f, 200, 1, 0.05f, "%.2f m/s", false);
            bool anyPlaying = false, canPlay = g_objectRuntime != nullptr;
            for (const auto& target : targets)
            {
                if (target.index < 0 || !g_objectRuntime)
                {
                    canPlay = false;
                    continue;
                }
                const auto& runtime = g_objectRuntime->Runtime(target.index);
                anyPlaying |= runtime.slide.playing;
                canPlay &= runtime.state == ObjectStreamState::Spawned || runtime.slide.playing;
            }
            Row("");
            ImGui::BeginDisabled(!canPlay && !anyPlaying);
            if (IconLabelButton("##slide_play", anyPlaying ? "Stop" : "Play", g_icons->playTexture, FieldWidth()) && Finish())
            {
                for (int id : ids)
                {
                    const int index = g_scene->FindIndexById(id);
                    if (index < 0 || !g_scene->data[index].slide.HasValue())
                    {
                        continue;
                    }
                    if (anyPlaying)
                    {
                        g_objectRuntime->StopSlide(index);
                    }
                    else if (!g_objectRuntime->Runtime(index).slide.playing)
                    {
                        g_objectRuntime->ToggleSlide(index);
                    }
                }
            }
            ImGui::EndDisabled();
            break;
        }
        case ComponentTypeRace:
        {
            static const char* const modes[] = { "Point-To-Point", "Circuit" };
            Enum("Mode", "##race_mode", INSPECT(race, mode), modes);
            Int("Laps", "##race_laps", INSPECT(race, laps), 1, RaceData::MaxLaps, 1, 0.05f, "%d",
                All(
                    [](const ItemData& data, int)
                    {
                        return data.race.Get().mode == 1;
                    }));
            break;
        }
        case ComponentTypeRaceCheckpoint:
        {
            static const char* const labels[]
                = { "Air (Auto)", "Ground (Auto)", "Ground (Normal)", "Ground (Finish)", "Ground (No Arrow)", "Air (Normal)",
                      "Air (Finish)", "Air (Rotating)", "Air (Strobing)", "Air (Swinging)", "Air (Descending)" };
            static const int values[] = { 10, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8 };
            Combo("Type", "##checkpoint_type", INSPECT(raceCheckpoint, type), labels, values, std::size(values));
            Float("Radius", "##checkpoint_radius", INSPECT(raceCheckpoint, radius), RaceCheckpointData::MinRadius,
                RaceCheckpointData::MaxRadius, 0.25f, 0.1f, "%.2f m", false);
            break;
        }
        case ComponentTypeCurve:
            Check("Closed", "##curve_closed", INSPECT(curve, closed));
            break;
        case ComponentTypeCurveNode:
        {
            static const char* const modes[] = { "Auto", "Aligned", "Free", "Linear" };
            auto mode = InspectField(INSPECT(curveNode, mode).read,
                [](ItemData& data, int index, CurveHandle value, std::string&)
                {
                    Curves::SetNodeMode(*g_scene, index, data, value);
                    return true;
                });
            Enum("Mode", "##curve_handle_mode", mode, modes);
            Float("Tilt Angle", "##curve_tilt", INSPECT(curveNode, tilt), -360, 360, 5, 0.25f);
            const bool manual = All(
                [](const ItemData& data, int)
                {
                    return data.curveNode.Get().mode == CurveHandle::Free || data.curveNode.Get().mode == CurveHandle::Aligned;
                });
            for (int handle = 0; handle < 2; ++handle)
            {
                auto field = InspectField(
                    [=](const ItemData& data, int index)
                    {
                        const auto& node = data.curveNode.Get();
                        if (node.mode == CurveHandle::Auto || node.mode == CurveHandle::Vector)
                        {
                            CVector incoming, outgoing;
                            if (Curves::Handles(*g_scene, index, incoming, outgoing))
                            {
                                const auto& world = g_scene->world[index];
                                const auto local
                                    = Utils::InverseTransformDirection(world.rot, (handle ? outgoing : incoming) - world.pos);
                                return std::array { local.x, local.y, local.z };
                            }
                        }
                        const float* values = handle ? node.outgoing : node.incoming;
                        return std::array { values[0], values[1], values[2] };
                    },
                    [=](ItemData& data, int, const std::array<float, 3>& value, std::string&)
                    {
                        auto& node = data.curveNode.Edit();
                        std::copy_n(value.data(), 3, handle ? node.outgoing : node.incoming);
                        node.Align(handle != 0);
                        return true;
                    });
                Vector(handle ? "Outgoing" : "Incoming", handle ? "##curve_outgoing" : "##curve_incoming", field, false, false, 0,
                    manual);
            }
            break;
        }
        case ComponentTypeLayout:
            LayoutFields();
            break;
        case ComponentTypeBoxArea:
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                static const char* const axes[] = { "Bounds X", "Bounds Y", "Bounds Z" };
                Row(axes[axis]);
                for (int side = 0; side < 2; ++side)
                {
                    const int face = axis + side * 3;
                    ImGui::PushID(face);
                    const auto read = [=](const ItemData& data, int index)
                    {
                        return InspectionWorldPosition(data, index)[axis]
                            + (side ? 0.5f : -0.5f) * data.boxArea.Get().GetSize(axis);
                    };
                    const ImGuiID owner = ImGui::GetID("##box_bound");
                    auto& cache = buffers[owner];
                    const auto previous = cache.seed;
                    const bool initialized = cache.initialized;
                    auto value = Scalar(owner, read);
                    if (initialized && !value.mixed && std::abs(value.value - previous) < 0.0005f)
                    {
                        cache.seed = value.value = static_cast<float>(previous);
                    }
                    if (side)
                    {
                        ImGui::SameLine(0, Gap());
                    }
                    ImGui::SetNextItemWidth((FieldWidth() - Gap()) * 0.5f);
                    float minimum = -20000.0f, maximum = 20000.0f;
                    for (const auto& target : targets)
                    {
                        const float opposite = InspectionWorldPosition(*target.data, target.index)[axis]
                            + (side ? -0.5f : 0.5f) * target.data->boxArea.Get().GetSize(axis);
                        minimum = ImMax(minimum, opposite + (side ? BoxAreaData::MinSize : -BoxAreaData::MaxSize));
                        maximum = ImMin(maximum, opposite + (side ? BoxAreaData::MaxSize : -BoxAreaData::MinSize));
                        if (axis == 2)
                        {
                            minimum = ImMax(minimum, TransformData::MinZ * 2 - opposite);
                        }
                    }
                    ImGui::BeginDisabled(minimum > maximum);
                    ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, value.mixed);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, FrameTextPadY()));
                    PropertyWidgetResult result;
                    result.Capture(
                        ImGui::DragFloat("##box_bound", &value.value, 0.1f, minimum, maximum, "", ImGuiSliderFlags_AlwaysClamp));
                    ImGui::PopStyleVar();
                    if (result.active)
                    {
                        boxFace = face;
                        boxActiveId = ImGui::GetItemID();
                    }
                    LockMouseCursorForActiveDrag();
                    if (!ImGui::TempInputIsActive(ImGui::GetItemID()))
                    {
                        char number[48];
                        SmartFormat(number, sizeof(number), value.value);
                        const char* text = value.mixed && !result.changed ? PropertyEdit::MixedText : number;
                        const char* prefix = side ? "Max" : "Min";
                        const float prefixWidth = ImGui::CalcTextSize(prefix).x;
                        const float textWidth = ImGui::CalcTextSize(text).x;
                        const auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
                        const float x = (min.x + max.x - prefixWidth - Gap() - textWidth) * 0.5f;
                        const float y = (min.y + max.y - ImGui::GetFontSize()) * 0.5f;
                        auto* draw = ImGui::GetWindowDrawList();
                        draw->AddText(ImVec2(x, y), ImGui::GetColorU32(Theme::g_textDim), prefix);
                        draw->AddText(ImVec2(x + prefixWidth + Gap(), y), ImGui::GetColorU32(Theme::g_text), text);
                    }
                    ImGui::PopItemFlag();
                    ImGui::EndDisabled();
                    if (result.changed)
                    {
                        std::vector<TransformData> desired;
                        std::vector<float> sizes;
                        for (int id : ids)
                        {
                            const int index = g_scene->FindIndexById(id);
                            const auto& data = g_scene->data[index];
                            const auto position = InspectionWorldPosition(data, index);
                            const float opposite = position[axis] + (side ? -0.5f : 0.5f) * data.boxArea.Get().GetSize(axis);
                            TransformData pose;
                            std::copy_n(position.data(), 3, pose.pos);
                            std::copy_n(g_scene->world[index].eulerDeg, 3, pose.rot);
                            pose.pos[axis] = (opposite + value.value) * 0.5f;
                            desired.push_back(pose);
                            sizes.push_back(side ? value.value - opposite : opposite - value.value);
                        }
                        std::vector<TransformData> locals;
                        std::string error;
                        if (PropertyEdit::SetWorldPoses(*g_scene, ids, desired, locals, error))
                        {
                            Write(
                                owner,
                                [&](int index, ItemData& data, std::string& error)
                                {
                                    const auto i = std::find(ids.begin(), ids.end(), g_scene->items[index].id) - ids.begin();
                                    if (sizes[i] < BoxAreaData::MinSize || sizes[i] > BoxAreaData::MaxSize)
                                    {
                                        error = "Bounds are incompatible with the selection.";
                                        return false;
                                    }
                                    data.transform = locals[i];
                                    data.boxArea.Edit().SetSize(axis, sizes[i]);
                                    return true;
                                },
                                result);
                        }
                        buffers[owner].seed = value.value;
                    }
                    Observe(result);
                    ImGui::PopID();
                }
            }
            auto size = InspectField(
                [](const ItemData& data, int)
                {
                    const auto& box = data.boxArea.Get();
                    return std::array { box.GetSize(0), box.GetSize(1), box.GetSize(2) };
                },
                [](ItemData& data, int, const std::array<float, 3>& value, std::string&)
                {
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        data.boxArea.Edit().SetSize(axis, value[axis]);
                    }
                    return true;
                });
            Vector("Size", "##box_size", size, false, false, 0, true, BoxAreaData::MinSize, BoxAreaData::MaxSize);
            Color(
                "Color", "##box_color", INSPECT(boxArea, colorRGBA).read,
                [](ItemData& data, int) -> unsigned&
                {
                    return data.boxArea.Edit().colorRGBA;
                },
                true, true);
            break;
        }
        case ComponentTypeRemoveBuilding:
        {
            const bool custom = All(
                [](const ItemData& data, int)
                {
                    return data.removeBuilding.Get().custom;
                });
            const auto model = InspectField(INSPECT(removeBuilding, modelId).read,
                [](ItemData& data, int, int value, std::string& error)
                {
                    if (value != -1 && !((value >= 321 && value <= 373) || (value >= 615 && value <= 19999)))
                    {
                        error = "Invalid removal model ID.";
                        return false;
                    }
                    data.removeBuilding.Edit().modelId = value;
                    return true;
                });
            Identifier("Model ID", "##remove_model_id", model, -1, 19999, custom, {}, false, StepRemoveModelId);
            Float(
                "Radius", "##remove_radius", INSPECT(removeBuilding, radius), 0.25f, 10000, 0.25f, 0.1f, "%.2f m", false, custom);
            break;
        }
        case ComponentTypeMaterialTexture:
        {
            static const char* const modes[] = { "Custom Texture", "Original Texture", "SA-MP Blank Texture" };
            const auto mode = InspectField(
                [=](const ItemData& data, int)
                {
                    const auto& material = data.materials[slot];
                    return material.textureModelId > 0 && material.textureName[0] ? 0 : material.textureModelId == 0 ? 2 : 1;
                },
                [=](ItemData& data, int, int value, std::string&)
                {
                    data.materials[slot].textureModelId = value == 2 ? 0 : -1;
                    return true;
                });
            const auto value = Values(mode.read);
            Row("Mode");
            if (ImCustom::BeginCombo("##texture_mode", value.mixed ? PropertyEdit::MixedText : modes[value.value],
                    FieldWidth() - Theme::g_frameHeight - Gap()))
            {
                for (int choice = 0; choice < 3; ++choice)
                {
                    if (!ImCustom::ComboItem(modes[choice], !value.mixed && choice == value.value))
                    {
                        continue;
                    }
                    if (choice == 0)
                    {
                        if (FreezePicker(InspectionPicker::Kind::Texture, component))
                        {
                            OpenTextureBrowser(Primary(), slot);
                        }
                    }
                    else
                    {
                        PropertyWidgetResult result;
                        result.changed = true;
                        Set(ImGui::GetID("##texture_mode"), mode, choice, result);
                    }
                }
                ImCustom::EndCombo();
            }
            ImGui::SameLine(0, Gap());
            if (ComponentIconButton("##texture_browse", g_icons->searchTexture, Theme::g_text))
            {
                if (FreezePicker(InspectionPicker::Kind::Texture, component))
                {
                    OpenTextureBrowser(Primary(), slot);
                }
            }
            auto index = InspectField(
                [=](const ItemData& data, int)
                {
                    return data.materials[slot].materialIndex;
                },
                [=](ItemData& data, int, int value, std::string&)
                {
                    data.materials[slot].materialIndex = value;
                    return true;
                });
            Int("Mat Slot", "##material_index", index, 0, 15, 1, 0.05f);
            Color(
                "Color", "##material_color",
                [=](const ItemData& data, int)
                {
                    return data.materials[slot].colorArgb;
                },
                [=](ItemData& data, int) -> unsigned&
                {
                    return data.materials[slot].colorArgb;
                },
                All(
                    [](const ItemData& data, int)
                    {
                        return CatalogDatabase::ModelSupportsMaterialColor(static_cast<std::uint16_t>(data.model.Get().modelId));
                    }));
            const auto texture = Values(
                [=](const ItemData& data, int)
                {
                    const auto& material = data.materials[slot];
                    return std::tuple { material.textureModelId, std::string_view(material.txdName),
                        std::string_view(material.textureName) };
                });
            if (texture.mixed)
            {
                MixedCard("##texture_info", 3);
            }
            else if (std::get<0>(texture.value) > 0)
            {
                DrawMaterialTextureInfoCard(targets.front().data->materials[slot], width, slot, Primary());
            }
            break;
        }
        case ComponentTypeMaterialText:
        {
            auto intField = [=](int MaterialTextData::* member)
            {
                return InspectField(
                    [=](const ItemData& data, int)
                    {
                        return data.materialTexts[slot].*member;
                    },
                    [=](ItemData& data, int, int value, std::string&)
                    {
                        data.materialTexts[slot].*member = value;
                        return true;
                    });
            };
            Int("Mat Slot", "##text_index", intField(&MaterialTextData::materialIndex), 0, 15, 1, 0.05f);
            Color(
                "Font Color", "##text_font_color",
                [=](const ItemData& data, int)
                {
                    return data.materialTexts[slot].fontColorArgb;
                },
                [=](ItemData& data, int) -> unsigned&
                {
                    return data.materialTexts[slot].fontColorArgb;
                });
            Color(
                "BG Color", "##text_back_color",
                [=](const ItemData& data, int)
                {
                    return data.materialTexts[slot].backgroundColorArgb;
                },
                [=](ItemData& data, int) -> unsigned&
                {
                    return data.materialTexts[slot].backgroundColorArgb;
                });
            Combo("Mat Size", "##text_size", intField(&MaterialTextData::materialSize), g_kMaterialSizeLabels,
                g_kMaterialSizeValues, std::size(g_kMaterialSizeValues));
            Int("Font Size", "##text_font_size", intField(&MaterialTextData::fontSize), 1, 255, 1, 0.2f);
            auto font = InspectField(
                [=](const ItemData& data, int)
                {
                    return std::string_view(data.materialTexts[slot].fontFace);
                },
                [=](ItemData& data, int, std::string_view value, std::string&)
                {
                    strncpy_s(data.materialTexts[slot].fontFace, value.data(), _TRUNCATE);
                    return true;
                });
            const auto face = Values(font.read);
            Row("Font Family");
            const auto* preview = face.mixed ? nullptr : FontPreviewAtlas::Find(face.value.data());
            const auto& comboStyle = ImCustom::GetStyle();
            const float popupWidth = FontPreviewAtlas::MaxWidth(Theme::g_frameHeight) + comboStyle.popupPad * 2.0f
                + comboStyle.itemPadX * 2.0f + ImGui::GetStyle().ScrollbarSize;
            if (ImCustom::BeginCombo("##text_font_face", face.mixed ? PropertyEdit::MixedText : face.value.data(), FieldWidth(),
                    true, preview, popupWidth))
            {
                static ComboSearch search;
                ImCustom::BeginComboList(search.Draw());
                for (const auto& entry : FontPreviewAtlas::Entries())
                {
                    const char* choice = entry.name;
                    if (!search.Matches(choice))
                    {
                        continue;
                    }
                    if (ImCustom::ComboItem(choice, !face.mixed && _stricmp(choice, face.value.data()) == 0, &entry.image))
                    {
                        PropertyWidgetResult result;
                        result.changed = true;
                        Set(ImGui::GetID("##text_font_face"), font, std::string_view(choice), result);
                    }
                }
                ImCustom::EndComboList();
                ImCustom::EndCombo();
            }
            Enum("Alignment", "##text_align", intField(&MaterialTextData::alignment), g_kAlignLabels);
            const auto bold = InspectField(
                [=](const ItemData& data, int)
                {
                    return data.materialTexts[slot].bold;
                },
                [=](ItemData& data, int, bool value, std::string&)
                {
                    data.materialTexts[slot].bold = value;
                    return true;
                });
            Check("Bold", "##text_bold", bold);
            auto text = InspectField(
                [=](const ItemData& data, int)
                {
                    return std::string_view(data.materialTexts[slot].text);
                },
                [=](ItemData& data, int, std::string_view value, std::string&)
                {
                    strncpy_s(data.materialTexts[slot].text, value.data(), _TRUNCATE);
                    return true;
                });
            Text("Text", "##material_text", text, 2048, true);
            break;
        }
    }
}
