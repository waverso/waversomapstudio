// Source Repository: https://github.com/waverso/waversomapstudio

bool PropertyInspector::FreezePicker(InspectionPicker::Kind kind, PropertyEdit::Component component, bool adding)
{
    if (!Finish() || ids.empty() || readOnly)
    {
        return false;
    }
    picker.kind = kind;
    picker.component = component;
    picker.adding = adding;
    picker.ids = ids;
    picker.structures.clear();
    for (int id : ids)
    {
        const int index = g_scene->FindIndexById(id);
        if (index < 0)
        {
            picker.kind = InspectionPicker::Kind::None;
            return false;
        }
        picker.structures.push_back(PropertyEdit::Structure(g_scene->data[index]));
    }
    return true;
}

bool PropertyInspector::PickerValid()
{
    size_t selected = 0;
    for (int index : g_scene->DisplayOrder())
    {
        if (!g_scene->items[index].selected)
        {
            continue;
        }
        if (selected >= picker.ids.size() || g_scene->items[index].id != picker.ids[selected]
            || selected >= picker.structures.size()
            || PropertyEdit::Structure(g_scene->data[index]) != picker.structures[selected])
        {
            return false;
        }
        ++selected;
    }
    return selected == picker.ids.size() && (!g_scenePicker || g_scenePicker->GetSelection().empty());
}

static bool InspectionConfirmModel(int model, ComponentType type)
{
    auto& inspector = g_propertyInspector;
    auto& picker = inspector.picker;
    const auto kind = type == ComponentTypeVehicle ? InspectionPicker::Kind::Vehicle
        : type == ComponentTypeActor               ? InspectionPicker::Kind::Skin
                                                   : InspectionPicker::Kind::Model;
    if (picker.kind != kind)
    {
        return false;
    }
    if (!inspector.PickerValid())
    {
        g_toast.ShowError("The selection or its components changed.");
        picker.kind = InspectionPicker::Kind::None;
        return true;
    }
    inspector.ids = picker.ids;
    if (picker.adding)
    {
        inspector.Add(picker.component.type, model);
    }
    else
    {
        const auto component = picker.component.type;
        PropertyWidgetResult result;
        result.changed = true;
        inspector.Write(
            ImHashStr("##model_confirm"),
            [=](int index, ItemData& data, std::string& error)
            {
                return InspectionSetModel(data, index, component, model, error);
            },
            result);
        inspector.Finish();
    }
    picker.kind = InspectionPicker::Kind::None;
    return true;
}

static bool InspectionConfirmTexture(int model, const char* library, const char* texture)
{
    auto& inspector = g_propertyInspector;
    if (inspector.picker.kind != InspectionPicker::Kind::Texture)
    {
        return false;
    }
    if (!inspector.PickerValid())
    {
        g_toast.ShowError("The selection or its components changed.");
    }
    else
    {
        const int slot = inspector.picker.component.slot;
        PropertyWidgetResult result;
        result.changed = true;
        inspector.Write(
            ImHashStr("##texture_confirm"),
            [=](int, ItemData& data, std::string&)
            {
                auto& material = data.materials[slot];
                material.textureModelId = model;
                strncpy_s(material.txdName, library, _TRUNCATE);
                strncpy_s(material.textureName, texture, _TRUNCATE);
                return true;
            },
            result);
        inspector.Finish();
    }
    inspector.picker.kind = InspectionPicker::Kind::None;
    return true;
}

static bool InspectionConfirmAnimation(int id)
{
    auto& inspector = g_propertyInspector;
    if (inspector.picker.kind != InspectionPicker::Kind::Animation)
    {
        return false;
    }
    if (!inspector.PickerValid())
    {
        g_toast.ShowError("The selection or its components changed.");
    }
    else
    {
        PropertyWidgetResult result;
        result.changed = true;
        const bool written = inspector.Write(
            ImHashStr("##animation_confirm"),
            [=](int, ItemData& data, std::string&)
            {
                AnimationBrowser::Select(data.actorAnimation.Edit(), id);
                return true;
            },
            result);
        if (inspector.Finish() && written && g_actorRuntime)
        {
            for (int selected : inspector.ids)
            {
                const int index = g_scene->FindIndexById(selected);
                if (index >= 0)
                {
                    g_actorRuntime->ReplayAnimation(index);
                }
            }
        }
    }
    return true;
}

static void InspectionFreezeTexture(int item, int slot)
{
    if (g_propertyInspector.drawing && item == g_propertyInspector.Primary())
    {
        g_propertyInspector.FreezePicker(InspectionPicker::Kind::Texture, { ComponentTypeMaterialTexture, slot });
    }
}
