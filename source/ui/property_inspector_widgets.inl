// Source Repository: https://github.com/waverso/waversomapstudio

template <class Field>
inline void PropertyInspector::Int(const char* label, const char* key, const Field& field, int minimum, int maximum, int step,
    float speed, const char* format, bool enabled)
{
    const ImGuiID owner = ImGui::GetID(key);
    auto value = Scalar(owner, field.read);
    int number = static_cast<int>(value.value);
    Row(label);
    if (readOnly)
    {
        char text[64];
        snprintf(text, sizeof(text), format, number);
        ReadOnlyField(key, value.mixed ? PropertyEdit::MixedText : text, FieldWidth());
        return;
    }
    ImGui::BeginDisabled(!enabled);
    PropertyWidgetResult result;
    DrawDragIntWithButtons(key, &number, speed, minimum, maximum, format, step, step, FieldWidth(), Theme::g_frameHeight, Gap(),
        value.mixed, &result);
    ImGui::EndDisabled();
    if (result.changed)
    {
        buffers[owner].seed = number;
    }
    Set(owner, field, static_cast<decltype(value.value)>(number), result);
}

template <class Field>
inline ImRect PropertyInspector::Identifier(const char* label, const char* key, const Field& field, int minimum, int maximum,
    bool enabled, const std::function<void()>& browse, bool mixedIdentity, int (*step)(int, bool), const char* browseKey)
{
    const ImGuiID owner = ImGui::GetID(key);
    auto value = Scalar(owner, field.read);
    value.mixed |= mixedIdentity;
    auto& buffer = buffers[owner];
    if (!buffer.focus)
    {
        snprintf(buffer.text.data(), 32, "%d", value.value);
    }
    char display[32];
    snprintf(display, sizeof(display), value.value < 0 ? "-1 (Any)" : "%d", value.value);
    Row(label);
    if (readOnly)
    {
        ReadOnlyField(key, value.mixed ? PropertyEdit::MixedText : display, FieldWidth());
        return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    }
    ImGui::BeginDisabled(!enabled);
    PropertyWidgetResult result;
    const ImVec2 first = ImGui::GetCursorScreenPos();
    const bool edited
        = CenteredDisplayInput(key, buffer.text.data(), 32, FieldWidth() - (Theme::g_frameHeight + Gap()) * (browse ? 3 : 2),
            value.mixed ? PropertyEdit::MixedText : display, buffer.focus, ImGuiInputTextFlags_AutoSelectAll);
    result.active = buffer.focus;
    if (edited || ImGui::IsItemDeactivatedAfterEdit())
    {
        char* end = nullptr;
        const long parsed = strtol(buffer.text.data(), &end, 10);
        if (end != buffer.text.data() && *end == 0 && parsed >= minimum && parsed <= maximum)
        {
            value.value = static_cast<int>(parsed);
            result.changed = true;
        }
        else if (ImGui::IsItemDeactivatedAfterEdit())
        {
            g_toast.ShowError("Invalid ID.");
        }
    }
    if (browse)
    {
        ImGui::SameLine(0, Gap());
        if (ComponentIconButton(browseKey, g_icons->searchTexture, Theme::g_text))
        {
            browse();
        }
    }
    ImGui::PushID(key);
    ImGui::SameLine(0, Gap());
    ImGui::PushButtonRepeat(true);
    const bool decrease = IconButton("##dec", g_icons->subtractTexture, Theme::g_frameHeight);
    result.Capture(decrease);
    if (decrease)
    {
        value.value = step ? step(value.value, false) : ImMax(minimum, value.value - 1);
    }
    ImGui::SameLine(0, Gap());
    const bool increase = IconButton("##inc", g_icons->addTexture, Theme::g_frameHeight);
    result.Capture(increase);
    if (increase)
    {
        value.value = step ? step(value.value, true) : ImMin(maximum, value.value + 1);
    }
    ImGui::PopButtonRepeat();
    ImGui::PopID();
    const ImRect anchor(first, ImGui::GetItemRectMax());
    ImGui::EndDisabled();
    if (result.changed)
    {
        buffer.seed = value.value;
    }
    Set(owner, field, value.value, result);
    return anchor;
}

template <class Field>
inline void PropertyInspector::Float(const char* label, const char* key, const Field& field, float minimum, float maximum,
    float step, float speed, const char* format, bool smart, bool enabled)
{
    const ImGuiID owner = ImGui::GetID(key);
    auto value = Scalar(owner, field.read);
    float number = static_cast<float>(value.value);
    Row(label);
    if (readOnly)
    {
        char text[64];
        if (smart)
        {
            SmartFormat(text, sizeof(text), number);
        }
        else
        {
            snprintf(text, sizeof(text), format, number);
        }
        ReadOnlyField(key, value.mixed ? PropertyEdit::MixedText : text, FieldWidth());
        return;
    }
    ImGui::BeginDisabled(!enabled);
    PropertyWidgetResult result;
    DrawDragFloatWithButtons(key, &number, speed, minimum, maximum, format, step, step, FieldWidth(), Theme::g_frameHeight, Gap(),
        smart, value.mixed, &result);
    ImGui::EndDisabled();
    if (result.changed && !std::isfinite(number))
    {
        result.changed = false;
        g_toast.ShowError("Invalid numeric value.");
    }
    if (result.changed)
    {
        buffers[owner].seed = number;
    }
    Set(owner, field, number, result);
}

template <class Field>
inline void PropertyInspector::Check(const char* label, const char* key, const Field& field, bool enabled)
{
    auto value = Values(field.read);
    if (value.mixed)
    {
        value.value = false;
    }
    Row(label);
    ImGui::BeginDisabled(!enabled);
    ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, value.mixed);
    PropertyWidgetResult result;
    result.Capture(ImGui::Checkbox(key, &value.value));
    ImGui::PopItemFlag();
    ImGui::EndDisabled();
    Set(ImGui::GetID(key), field, value.value, result);
}

template <class Field, class Value>
inline void PropertyInspector::Combo(const char* label, const char* key, const Field& field, const char* const* labels,
    const Value* choices, size_t count, bool enabled)
{
    auto value = Values(field.read);
    const char* preview = PropertyEdit::MixedText;
    if (!value.mixed)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (value.value == choices[i])
            {
                preview = labels[i];
            }
        }
    }
    Row(label);
    ImGui::BeginDisabled(!enabled);
    if (ImCustom::BeginCombo(key, preview, FieldWidth()))
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (ImCustom::ComboItem(labels[i], !value.mixed && value.value == choices[i]))
            {
                PropertyWidgetResult result;
                result.changed = true;
                Set(ImGui::GetID(key), field, choices[i], result);
            }
        }
        ImCustom::EndCombo();
    }
    ImGui::EndDisabled();
}

template <class Field, size_t Count>
inline void PropertyInspector::Enum(
    const char* label, const char* key, const Field& field, const char* const (&labels)[Count], bool enabled)
{
    using Value = decltype(field.read(*targets.front().data, targets.front().index));
    std::array<Value, Count> choices;
    for (size_t i = 0; i < Count; ++i)
    {
        choices[i] = static_cast<Value>(i);
    }
    Combo(label, key, field, labels, choices.data(), Count, enabled);
}

template <class Field>
inline void PropertyInspector::Vector(const char* label, const char* key, const Field& field, bool rotation, bool compact,
    unsigned disabledAxes, bool enabled, float minimum, float maximum)
{
    const ImGuiID owner = ImGui::GetID(key);
    float values[3];
    unsigned mixed = 0;
    const bool cacheValues = currentComponent.type == ComponentTypeCurveNode && !enabled && !readOnly;
    if (cacheValues)
    {
        vectorValues.resize(g_scene->count);
        for (const auto& target : targets)
        {
            vectorValues[target.index] = field.read(*target.data, target.index);
        }
    }
    for (int axis = 0; axis < 3; ++axis)
    {
        const auto value = cacheValues ? Scalar(owner + axis + 1,
                                             [&](const ItemData&, int index)
                                             {
                                                 return vectorValues[index][axis];
                                             })
                                       : Scalar(owner + axis + 1,
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
    Row(label);
    PropertyWidgetResult result;
    ImGui::BeginDisabled(!enabled);
    DrawVec3Field(key, buffers[owner].vector, values, width, compact, readOnly, disabledAxes, nullptr, rotation, minimum, maximum,
        mixed, &result);
    ImGui::EndDisabled();
    if (result.changed)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            if (result.parts & (1u << axis))
            {
                buffers[owner + axis + 1].seed = values[axis];
            }
        }
        Write(
            owner,
            [&](int index, ItemData& data, std::string& error)
            {
                auto individual = field.read(data, index);
                for (int axis = 0; axis < 3; ++axis)
                {
                    if (result.parts & (1u << axis))
                    {
                        individual[axis] = values[axis];
                    }
                }
                return field.write(data, index, individual, error);
            },
            result);
    }
    Observe(result);
}

template <class Field>
inline void PropertyInspector::Text(
    const char* label, const char* key, const Field& field, size_t capacity, bool multiline, bool enabled)
{
    auto value = Values(field.read);
    auto& buffer = buffers[ImGui::GetID(key)];
    if (!buffer.focus)
    {
        snprintf(buffer.text.data(), capacity, "%s", value.mixed ? "" : value.value.data());
    }
    Row(label);
    ImGui::BeginDisabled(!enabled);
    PropertyWidgetResult result;
    if (multiline)
    {
        result.Capture(ImGui::InputTextMultiline(key, buffer.text.data(), capacity,
            ImVec2(FieldWidth(), Theme::g_frameHeight * 4), ImGuiInputTextFlags_AllowTabInput));
        buffer.focus = ImGui::IsItemActive();
        if (value.mixed && !buffer.focus)
        {
            const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
            const auto size = ImGui::CalcTextSize(PropertyEdit::MixedText);
            ImGui::GetWindowDrawList()->AddText(
                ImVec2((minimum.x + maximum.x - size.x) * 0.5f, (minimum.y + maximum.y - size.y) * 0.5f),
                ImGui::GetColorU32(Theme::g_text), PropertyEdit::MixedText);
        }
    }
    else
    {
        result.changed = CenteredDisplayInput(key, buffer.text.data(), capacity, FieldWidth(),
            value.mixed ? PropertyEdit::MixedText : value.value.data(), buffer.focus);
        result.active = buffer.focus;
    }
    ImGui::EndDisabled();
    if (result.changed)
    {
        Set(ImGui::GetID(key), field, std::string_view(buffer.text.data()), result);
    }
    else
    {
        Observe(result);
    }
}

inline void PropertyInspector::MixedCard(const char* key, int rows, bool image)
{
    const float rowHeight = ScaledFontSize(g_gui->m_fontSmall) + Ui::Px(8);
    const float imageSize = image ? floorf(Theme::g_frameHeight * 2.8f) : 0;
    const float height
        = Theme::g_padding * 3 + ImMax(ImGui::GetFontSize(), Theme::g_iconSize) + ImMax(imageSize, rows * rowHeight);
    ImGui::SetCursorPosX(0);
    const auto minimum = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(key, ImVec2(width, height));
    const auto maximum = ImGui::GetItemRectMax();
    const auto size = ImGui::CalcTextSize(PropertyEdit::MixedText);
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(minimum, maximum, ImGui::GetColorU32(Theme::g_infoCardBackground));
    draw->AddRect(minimum, maximum, ImGui::GetColorU32(Theme::g_border));
    draw->AddText(ImVec2((minimum.x + maximum.x - size.x) * 0.5f, (minimum.y + maximum.y - size.y) * 0.5f),
        ImGui::GetColorU32(Theme::g_text), PropertyEdit::MixedText);
}

static bool InspectionModelValid(ComponentType type, int model)
{
    switch (type)
    {
        case ComponentTypeActor:
            return SkinCatalog::IsValid(model);
        case ComponentTypeVehicle:
            return VehicleCatalog::IsValid(model);
        case ComponentTypePickup:
            return PickupRuntime::IsValidModel(model);
        default:
            return IsObjectModelInputValid(model);
    }
}

void PropertyInspector::Color(const char* label, const char* key, const std::function<unsigned(const ItemData&, int)>& read,
    const std::function<unsigned&(ItemData&, int)>& write, bool enabled, bool rgba)
{
    const ImGuiID owner = ImGui::GetID(key);
    const std::array shifts = rgba ? std::array { 24, 16, 8, 0 } : std::array { 16, 8, 0, 24 };
    unsigned mixed = 0, color = 0;
    for (int channel = 0; channel < 4; ++channel)
    {
        const auto value = Scalar(owner + channel + 1,
            [&](const ItemData& data, int index)
            {
                return (read(data, index) >> shifts[channel]) & 255u;
            });
        color |= value.value << shifts[channel];
        if (value.mixed)
        {
            mixed |= 1u << channel;
        }
    }
    auto& buffer = buffers[owner];
    unsigned argb = rgba ? (color >> 8) | (color << 24) : color;
    unsigned previous = argb;
    ImVec4 pickerColor = ARGBToVec4(argb);
    if (!buffer.focus)
    {
        snprintf(buffer.text.data(), 12, "%08X", color);
    }
    PropertyWidgetResult result;
    ImGui::BeginDisabled(!enabled);
    DrawArgbColorRow(label, key, argb, buffer.text.data(), pickerColor, buffer.focus, previous, Theme::g_propertyLabelWidth,
        FieldWidth(), Theme::g_frameHeight, Gap(), mixed, &result, rgba);
    ImGui::EndDisabled();
    color = rgba ? (argb << 8) | (argb >> 24) : argb;
    if (result.changed)
    {
        unsigned edited = 0;
        for (int channel = 0; channel < 4; ++channel)
        {
            if (result.parts & (1u << channel))
            {
                edited |= 255u << shifts[channel];
                buffers[owner + channel + 1].seed = (color >> shifts[channel]) & 255u;
            }
        }
        Write(
            owner,
            [&](int index, ItemData& data, std::string&)
            {
                auto& target = write(data, index);
                target = (target & ~edited) | (color & edited);
                return true;
            },
            result);
    }
    Observe(result);
}

static bool InspectionSetModel(ItemData& data, int index, ComponentType type, int model, std::string& error)
{
    if (!InspectionModelValid(type, model))
    {
        error = "Invalid model ID.";
        return false;
    }
    switch (type)
    {
        case ComponentTypeModel:
            if (data.model.Get().modelId != model)
            {
                data.model.Edit().modelId = model;
                ApplyModelDistanceDefaults(data.model.Edit());
            }
            break;
        case ComponentTypeVehicle:
            data.vehicle.Edit().modelId = model;
            break;
        case ComponentTypePickup:
            data.pickup.Edit().modelId = model;
            break;
        case ComponentTypeActor:
            data.actor.Edit().skinId = model;
            break;
        case ComponentTypeLayout:
            if (data.layout.Get().copyFromFirst || !Layout::Inspect(*g_scene, index).CanGenerate())
            {
                error = "The Layout source cannot be changed.";
                return false;
            }
            data.layout.Edit().modelId = model;
            data.layout.Edit().modelPending = true;
            break;
        default:
            return false;
    }
    return true;
}

void PropertyInspector::Model(ComponentType type, bool enabled)
{
    const auto field = InspectField(
        [type](const ItemData& data, int index)
        {
            switch (type)
            {
                case ComponentTypeVehicle:
                    return data.vehicle.Get().modelId;
                case ComponentTypePickup:
                    return data.pickup.Get().modelId;
                case ComponentTypeActor:
                    return data.actor.Get().skinId;
                case ComponentTypeLayout:
                {
                    const auto children = Layout::Inspect(*g_scene, index);
                    return Layout::EffectiveSettings(data.layout.Get(), children).modelId;
                }
                default:
                    return data.model.Get().modelId;
            }
        },
        [type](ItemData& data, int index, int value, std::string& error)
        {
            return InspectionSetModel(data, index, type, value, error);
        });
    const ImGuiID owner = ImGui::GetID("##model_id");
    auto value = Scalar(owner, field.read);
    auto& buffer = buffers[owner];
    if (!buffer.focus)
    {
        snprintf(buffer.text.data(), 32, "%d", value.value);
    }
    char display[32];
    snprintf(display, sizeof(display), "%d", value.value);
    Row(type == ComponentTypeActor ? "Skin ID" : "Model ID");
    if (readOnly)
    {
        ReadOnlyField("##model_id", value.mixed ? PropertyEdit::MixedText : display, FieldWidth());
        return;
    }
    ImGui::BeginDisabled(!enabled);
    PropertyWidgetResult result;
    const bool edited = CenteredDisplayInput("##model_id", buffer.text.data(), 32,
        FieldWidth() - (Theme::g_frameHeight + Gap()) * 3, value.mixed ? PropertyEdit::MixedText : display, buffer.focus,
        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CallbackCharFilter, FilterIntegerModelInput);
    result.active = buffer.focus;
    if (edited || ImGui::IsItemDeactivatedAfterEdit())
    {
        int typed = 0;
        if (ParseModelIdInput(buffer.text.data(), typed) && InspectionModelValid(type, typed))
        {
            value.value = typed;
            result.changed = true;
        }
        else if (ImGui::IsItemDeactivatedAfterEdit())
        {
            g_toast.ShowError("Invalid model ID.");
        }
    }
    ImGui::SameLine(0, Gap());
    if (ComponentIconButton("##model_browse", g_icons->searchTexture, Theme::g_text))
    {
        const auto kind = type == ComponentTypeVehicle ? InspectionPicker::Kind::Vehicle
            : type == ComponentTypeActor               ? InspectionPicker::Kind::Skin
                                                       : InspectionPicker::Kind::Model;
        if (FreezePicker(kind, { type }))
        {
            const int id = ids.front();
            if (type == ComponentTypeVehicle)
            {
                OpenVehicleBrowser(id);
            }
            else if (type == ComponentTypeActor)
            {
                OpenSkinBrowser(id);
            }
            else
            {
                OpenObjectBrowser(id);
            }
        }
    }
    ImGui::PushButtonRepeat(true);
    for (int direction : { -1, 1 })
    {
        ImGui::SameLine(0, Gap());
        const bool clicked = ComponentIconButton(direction < 0 ? "##model_dec" : "##model_inc",
            direction < 0 ? g_icons->subtractTexture : g_icons->addTexture, Theme::g_text);
        result.Capture(clicked);
        if (!clicked)
        {
            continue;
        }
        int next = value.value;
        if (type == ComponentTypeVehicle)
        {
            next = ImClamp(next + direction, VehicleCatalog::FirstId, VehicleCatalog::LastId);
        }
        else if (type == ComponentTypeActor)
        {
            next = ImClamp(next + direction, 0, 311);
            if (next == 74)
            {
                next += direction;
            }
        }
        else
        {
            do
            {
                const int previous = next;
                next = direction < 0 ? TextureBrowser::PrevValidModel(next) : TextureBrowser::NextValidModel(next);
                if (next == previous)
                {
                    break;
                }
            } while (!InspectionModelValid(type, next));
        }
        if (InspectionModelValid(type, next))
        {
            value.value = next;
        }
    }
    ImGui::PopButtonRepeat();
    ImGui::EndDisabled();
    if (result.changed)
    {
        buffer.seed = value.value;
    }
    Set(owner, field, value.value, result);
}
