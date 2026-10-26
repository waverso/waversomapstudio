// Source Repository: https://github.com/waverso/waversomapstudio

void PropertyInspector::LayoutFields()
{
    static const char* const types[] = { "Loop", "Grid", "Curve" };
    const auto type = InspectField(INSPECT(layout, type).read,
        [](ItemData& data, int, LayoutType value, std::string&)
        {
            auto& layout = data.layout.Edit();
            if (layout.type != value)
            {
                layout.type = value;
                layout.axis = value == LayoutType::Loop ? 1 : 2;
                if (value == LayoutType::Loop && layout.UsesStep())
                {
                    layout.autoRadius = false;
                }
            }
            return true;
        });
    Enum("Type", "##layout_type", type, types);
    const auto distribution = Values(type.read);
    const auto source = INSPECT(layout, copyFromFirst);
    const auto sourceValue = Values(source.read);
    Row("Source");
    if (ImCustom::BeginCombo("##layout_source",
            sourceValue.mixed       ? PropertyEdit::MixedText
                : sourceValue.value ? "Copy From First Child"
                                    : "Create From Object Model",
            FieldWidth()))
    {
        const bool canCopy = All(
            [](const ItemData&, int index)
            {
                return g_scene->HasChildren(index);
            });
        ImGui::BeginDisabled(!canCopy);
        if (ImCustom::ComboItem("Copy From First Child", !sourceValue.mixed && sourceValue.value))
        {
            PropertyWidgetResult result;
            result.changed = true;
            Set(ImGui::GetID("##layout_source"), source, true, result);
        }
        ImGui::EndDisabled();
        if (ImCustom::ComboItem("Create From Object Model", !sourceValue.mixed && !sourceValue.value))
        {
            PropertyWidgetResult result;
            result.changed = true;
            Set(ImGui::GetID("##layout_source"), source, false, result);
        }
        ImCustom::EndCombo();
    }
    std::unordered_map<int, Layout::Children> children;
    for (const auto& target : targets)
    {
        children.emplace(target.index, Layout::Inspect(*g_scene, target.index));
    }
    const auto effective = [&](const ItemData& data, int index)
    {
        auto settings = Layout::EffectiveSettings(data.layout.Get(), children.at(index));
        std::string error;
        if (settings.UsesStep() || settings.type == LayoutType::Grid && settings.autoStep
            || settings.type == LayoutType::Loop && settings.autoRadius)
        {
            Layout::ResolveGeometry(*g_scene, children.at(index), settings, error);
        }
        return settings;
    };
    const bool canModel = All(
        [&](const ItemData& data, int index)
        {
            return !data.layout.Get().copyFromFirst && children.at(index).CanGenerate();
        });
    const bool canGenerate = All(
        [&](const ItemData& data, int index)
        {
            return children.at(index).CanGenerate(data.layout.Get().copyFromFirst);
        });
    Model(ComponentTypeLayout, canModel);
    const bool curve = !distribution.mixed && distribution.value == LayoutType::Curve;
    const bool grid = !distribution.mixed && distribution.value == LayoutType::Grid;
    const bool path = !distribution.mixed && !grid;
    if (path)
    {
        static const char* const modes[] = { "Copies", "Step" };
        const auto spacing = InspectField(INSPECT(layout, spacingMode).read,
            [&](ItemData& data, int index, LayoutSpacing value, std::string&)
            {
                auto& layout = data.layout.Edit();
                if (layout.spacingMode == value)
                {
                    return true;
                }
                const auto resolved = effective(data, index);
                if (value == LayoutSpacing::Step)
                {
                    const float step = Layout::PathStep(*g_scene, resolved);
                    if (step > 0)
                    {
                        layout.step = std::clamp(step, LayoutData::MinStep, LayoutData::MaxStep);
                    }
                    if (layout.type == LayoutType::Loop)
                    {
                        layout.radius = resolved.radius;
                        layout.autoRadius = false;
                    }
                }
                else
                {
                    layout.pieces = resolved.pieces;
                    layout.piecesPending = true;
                }
                layout.spacingMode = value;
                return true;
            });
        Enum("Space By", "##layout_spacing_mode", spacing, modes);
    }
    const bool canCopies = canGenerate
        && All(
            [](const ItemData& data, int)
            {
                return !data.layout.Get().UsesStep();
            });
    const auto copies = InspectField(
        [&](const ItemData& data, int index)
        {
            return effective(data, index).pieces;
        },
        [](ItemData& data, int, int value, std::string&)
        {
            data.layout.Edit().pieces = value;
            data.layout.Edit().piecesPending = true;
            return true;
        });
    Int("Copies", "##layout_pieces", copies, 0, EditorScene::MaxItems, 1, 0.1f, "%d", canCopies);
    if (path)
    {
        const auto step = InspectField(
            [&](const ItemData& data, int index)
            {
                return Layout::PathStep(*g_scene, effective(data, index));
            },
            INSPECT(layout, step).write);
        Float("Step", "##layout_step", step, LayoutData::MinStep, LayoutData::MaxStep, 0.25f, 0.05f, "%.3f", true,
            All(
                [](const ItemData& data, int)
                {
                    return data.layout.Get().UsesStep();
                }));
    }
    if (distribution.mixed)
    {
        Vector("Rotation", "##layout_rotation", INSPECT_VECTOR(layout, LayoutData, rotation), true);
        return;
    }
    if (!curve)
    {
        static const char* const axes[] = { "X", "Y", "Z" };
        static const char* const planes[] = { "YZ", "XZ", "XY" };
        if (grid)
        {
            Enum("Plane", "##layout_axis", INSPECT(layout, axis), planes);
        }
        else
        {
            Enum("Axis", "##layout_axis", INSPECT(layout, axis), axes);
        }
    }
    else
    {
        const auto field = InspectField(INSPECT(layout, curveId).read,
            [](ItemData& data, int index, int value, std::string& error)
            {
                if (value >= 0 && !Curves::CanUse(*g_scene, index, g_scene->FindIndexById(value)))
                {
                    error = "The Curve cannot be used by this selection.";
                    return false;
                }
                data.layout.Edit().curveId = value;
                return true;
            });
        const auto value = Values(field.read);
        const int selected = value.mixed ? -1 : g_scene->FindIndexById(value.value);
        const bool valid = selected >= 0
            && All(
                [&](const ItemData&, int index)
                {
                    return Curves::CanUse(*g_scene, index, selected);
                });
        const char* preview = value.mixed ? PropertyEdit::MixedText
            : valid                       ? g_scene->items[selected].name
            : value.value >= 0            ? "Missing Curve"
                                          : "Select Curve";
        Row("Curve");
        if (ImCustom::BeginCombo("##layout_curve", preview, FieldWidth() - Theme::g_frameHeight - Gap(), true))
        {
            const bool bound
                = !ImGui::IsWindowAppearing() || FreezePicker(InspectionPicker::Kind::Curve, { ComponentTypeLayout });
            if (!bound)
            {
                ImGui::CloseCurrentPopup();
            }
            else if (picker.kind != InspectionPicker::Kind::Curve || !PickerValid())
            {
                g_toast.ShowError("The selection or its components changed.");
                ImGui::CloseCurrentPopup();
            }
            else
            {
                const bool searchChanged = TextInputIcon("##curve_search", "Search", widgets.curveSearch,
                    sizeof(widgets.curveSearch), ImGui::GetContentRegionAvail().x, g_icons->searchTexture);
                ImCustom::BeginComboList(searchChanged);
                PropertyWidgetResult result;
                result.changed = true;
                if (ImCustom::ComboItem("None", !value.mixed && value.value < 0))
                {
                    Set(ImGui::GetID("##layout_curve"), field, -1, result);
                }
                for (int index : g_scene->DisplayOrder())
                {
                    if (!TextSearch::ContainsAsciiNoCase(g_scene->items[index].name, widgets.curveSearch)
                        || !All(
                            [&](const ItemData&, int layout)
                            {
                                return Curves::CanUse(*g_scene, layout, index);
                            }))
                    {
                        continue;
                    }
                    ImGui::PushID(g_scene->items[index].id);
                    if (ImCustom::ComboItem(g_scene->items[index].name, !value.mixed && value.value == g_scene->items[index].id))
                    {
                        Set(ImGui::GetID("##layout_curve"), field, g_scene->items[index].id, result);
                    }
                    ImGui::PopID();
                }
                ImCustom::EndComboList();
            }
            ImCustom::EndCombo();
        }
        ImGui::SameLine(0, Gap());
        ImGui::BeginDisabled(!valid);
        if (ComponentIconButton("##locate_curve", g_icons->selectTexture, Theme::g_text))
        {
            g_pendingCurveLocateId = value.value;
        }
        ImGui::EndDisabled();
    }
    if (grid)
    {
        Int("Columns", "##layout_columns", INSPECT(layout, columns), 1, EditorScene::MaxItems);
        const auto rows = InspectField(
            [&](const ItemData& data, int index)
            {
                const auto settings = effective(data, index);
                return (settings.pieces + settings.columns - 1) / settings.columns;
            },
            [](ItemData&, int, int, std::string&)
            {
                return true;
            });
        Int("Rows", "##layout_rows", rows, 0, EditorScene::MaxItems, 1, 0.1f, "%d", false);
        Check("Auto Step", "##layout_auto_step", INSPECT(layout, autoStep));
        const bool manual = All(
            [](const ItemData& data, int)
            {
                return !data.layout.Get().autoStep;
            });
        const bool automatic = All(
            [](const ItemData& data, int)
            {
                return data.layout.Get().autoStep;
            });
        for (int axis = 0; axis < 2; ++axis)
        {
            ImGui::PushID(axis);
            const auto step = InspectField(
                [&](const ItemData& data, int index)
                {
                    return effective(data, index).gridSpacing[axis];
                },
                [=](ItemData& data, int, float value, std::string&)
                {
                    data.layout.Edit().gridSpacing[axis] = value;
                    return true;
                });
            Float(axis ? "Step V" : "Step U", "##layout_spacing", step, -LayoutData::MaxOffset, LayoutData::MaxOffset, 0.25f,
                0.05f, "%.3f", true, manual);
            ImGui::PopID();
        }
        Float("Spacing", "##layout_auto_spacing", INSPECT(layout, spacing), 0.01f, 10, 0.05f, 0.01f, "%.3f", true, automatic);
    }
    else if (curve)
    {
        unsigned mixed = 0;
        float range[2];
        std::array<float, 4> limits { 0, 100, 0, 100 };
        for (int endpoint = 0; endpoint < 2; ++endpoint)
        {
            const auto value = Scalar(ImGui::GetID(endpoint ? "##range_max" : "##range_min"),
                [=](const ItemData& data, int)
                {
                    return data.layout.Get().curveRange[endpoint];
                });
            range[endpoint] = value.value * 100;
            if (value.mixed)
            {
                mixed |= 1u << endpoint;
            }
            for (const auto& target : targets)
            {
                const auto& layout = target.data->layout.Get();
                limits[1] = ImMin(limits[1], layout.curveRange[1] * 100);
                limits[2] = ImMax(limits[2], layout.curveRange[0] * 100);
            }
        }
        auto& minimum = buffers[ImGui::GetID("##range_min")];
        auto& maximum = buffers[ImGui::GetID("##range_max")];
        Row("Path Range");
        PropertyWidgetResult result;
        RangeSliderFloat("##layout_range", "%", &range[0], &range[1], 0, 100, FieldWidth(), minimum.text.data(),
            maximum.text.data(), minimum.focus, maximum.focus, RangeSliderCurve::Linear, Ui::Px(48.0f), Gap(), mixed, &result,
            &limits);
        if (result.changed)
        {
            for (int endpoint = 0; endpoint < 2; ++endpoint)
            {
                if (result.parts & (1u << endpoint))
                {
                    buffers[ImGui::GetID(endpoint ? "##range_max" : "##range_min")].seed = range[endpoint] * 0.01;
                }
            }
            Write(
                ImGui::GetID("##layout_range"),
                [&](int, ItemData& data, std::string& error)
                {
                    auto& layout = data.layout.Edit();
                    for (int endpoint = 0; endpoint < 2; ++endpoint)
                    {
                        if (result.parts & (1u << endpoint))
                        {
                            layout.curveRange[endpoint] = range[endpoint] * 0.01f;
                        }
                    }
                    if (!layout.IsValid())
                    {
                        error = "Range is incompatible with the selection.";
                        return false;
                    }
                    return true;
                },
                result);
        }
        Observe(result);
        static const char* const directions[] = { "Forward", "Reverse" };
        Enum("Direction", "##layout_direction", INSPECT(layout, reverse), directions);
        Float("Lateral", "##layout_lateral", INSPECT(layout, lateralOffset), -LayoutData::MaxOffset, LayoutData::MaxOffset);
        Float("Vertical", "##layout_vertical", INSPECT(layout, verticalOffset), -LayoutData::MaxOffset, LayoutData::MaxOffset);
        bool values[3];
        unsigned locksMixed = 0;
        for (int axis = 0; axis < 3; ++axis)
        {
            const auto value = Values(
                [=](const ItemData& data, int)
                {
                    return data.layout.Get().lockCurveRotation[axis];
                });
            values[axis] = value.value;
            if (value.mixed)
            {
                locksMixed |= 1u << axis;
            }
        }
        const char* labels[] = { "Lock X", "Lock Y", "Lock Z" };
        bool* pointers[] = { &values[0], &values[1], &values[2] };
        PropertyWidgetResult lockResult;
        DrawToggleButtonRow(
            ImGui::GetWindowDrawList(), "Lock Rot", labels, pointers, FieldWidth(), Gap(), locksMixed, &lockResult);
        if (lockResult.changed)
        {
            Write(
                ImGui::GetID("##layout_locks"),
                [&](int, ItemData& data, std::string&)
                {
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        if (lockResult.parts & (1u << axis))
                        {
                            data.layout.Edit().lockCurveRotation[axis] = values[axis];
                        }
                    }
                    return true;
                },
                lockResult);
        }
        Observe(lockResult);
    }
    else
    {
        static const char* const directions[] = { "Forward", "Reverse" };
        Enum("Direction", "##layout_direction", INSPECT(layout, reverse), directions);
        Check("Auto Radius", "##layout_auto_radius", INSPECT(layout, autoRadius),
            All(
                [](const ItemData& data, int)
                {
                    return !data.layout.Get().UsesStep();
                }));
        const bool manual = All(
            [](const ItemData& data, int)
            {
                const auto& layout = data.layout.Get();
                return !layout.autoRadius || layout.UsesStep();
            });
        const bool automatic = All(
            [](const ItemData& data, int)
            {
                const auto& layout = data.layout.Get();
                return layout.autoRadius && !layout.UsesStep();
            });
        const auto radius = InspectField(
            [&](const ItemData& data, int index)
            {
                return effective(data, index).radius;
            },
            INSPECT(layout, radius).write);
        Float(
            "Radius", "##layout_radius", radius, LayoutData::MinRadius, LayoutData::MaxRadius, 0.25f, 0.1f, "%.3f", true, manual);
        Float("Spacing", "##layout_auto_spacing", INSPECT(layout, spacing), 0.01f, 10, 0.05f, 0.01f, "%.3f", true, automatic);
        Float("Turns", "##layout_loops", INSPECT(layout, loops), LayoutData::MinLoops, LayoutData::MaxLoops, 0.25f);
        Float("Start Angle", "##layout_start", INSPECT(layout, startAngle), -360, 360, 5, 0.25f);
        Float("Axis Offset", "##layout_offset", INSPECT(layout, offset), -LayoutData::MaxOffset, LayoutData::MaxOffset);
    }
    Vector("Rotation", "##layout_rotation", INSPECT_VECTOR(layout, LayoutData, rotation), true);
}
