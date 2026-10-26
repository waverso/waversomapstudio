// Source Repository: https://github.com/waverso/waversomapstudio

template <class Read, class Write>
struct InspectionField
{
    Read read;
    Write write;
};

template <class Read, class Write>
static auto InspectField(Read read, Write write)
{
    return InspectionField<Read, Write> { read, write };
}

template <class Component, class Value>
static auto InspectMember(ComponentValue<Component> ItemData::* component, Value Component::* member)
{
    return InspectField(
        [=](const ItemData& data, int)
        {
            return (data.*component).Get().*member;
        },
        [=](ItemData& data, int, const Value& value, std::string&)
        {
            (data.*component).Edit().*member = value;
            return true;
        });
}

template <class Component, class Value>
static auto InspectArray(ComponentValue<Component> ItemData::* component, Value (Component::*member)[3])
{
    return InspectField(
        [=](const ItemData& data, int)
        {
            const auto& values = (data.*component).Get().*member;
            return std::array { values[0], values[1], values[2] };
        },
        [=](ItemData& data, int, const std::array<Value, 3>& values, std::string&)
        {
            std::copy_n(values.data(), 3, (data.*component).Edit().*member);
            return true;
        });
}

struct InspectionTarget
{
    int index = -1;
    const ItemData* data = nullptr;
};

struct InspectionBuffer
{
    std::array<char, 2048> text {};
    Vec3State vector;
    bool focus = false;
    bool initialized = false;
    std::uint64_t signature = 0;
    double seed = 0;
};

struct InspectionPicker
{
    enum class Kind
    {
        None,
        Model,
        Vehicle,
        Skin,
        Texture,
        Animation,
        Curve
    };
    Kind kind = Kind::None;
    PropertyEdit::Component component;
    bool adding = false;
    std::vector<int> ids;
    std::vector<std::uint64_t> structures;
};

struct PropertyInspector
{
    struct ComponentLayout
    {
        float advance;
        float bottom;
    };
    std::vector<int> ids;
    std::vector<std::uintptr_t> nativeIds;
    std::vector<ItemData> nativeData;
    std::vector<std::array<char, 64>> nativeNames;
    std::vector<InspectionTarget> targets;
    std::vector<std::array<float, 3>> vectorValues;
    std::vector<PropertyEdit::Component> components;
    std::unordered_map<ImGuiID, InspectionBuffer> buffers;
    std::unordered_map<int, bool> open;
    std::function<void()> pending;
    std::vector<ComponentLayout> componentHeights;
    std::uint64_t layoutSignature = 0;
    float layoutWidth = 0;
    float layoutScale = 0;
    std::array<float, 2> noticeHeights {};
    PropertyEdit::Session session;
    InspectionPicker picker;
    ItemWidgetState widgets;
    bool readOnly = false;
    bool partial = false;
    bool drawing = false;
    bool interrupted = false;
    bool interacting = false;
    bool nameDirty = false;
    bool resetBuffers = false;
    ImGuiID nameOwner = 0;
    int interactionFrame = -1;
    int boxFace = -1;
    ImGuiID boxActiveId = 0;
    int drawFrame = -1;
    std::uint64_t structureSignature = 0;
    ComponentType interactionType = ComponentTypeTransform;
    PropertyEdit::Component currentComponent;
    float width = 0;

    float FieldWidth() const
    {
        return width - Theme::g_propertyLabelWidth;
    }
    float Gap() const
    {
        return Theme::g_padding * 0.5f;
    }
    int Primary() const
    {
        return ids.empty() ? -1 : g_scene->FindIndexById(ids.front());
    }

    void Row(const char* label)
    {
        if (readOnly)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * ImGui::GetStyle().DisabledAlpha);
        }
        DrawLabel(ImGui::GetWindowDrawList(), label);
        if (readOnly)
        {
            ImGui::PopStyleVar();
        }
        ImGui::SetCursorPosX(Theme::g_propertyLabelWidth);
    }

    void ReadOnlyField(const char* key, const char* text, float fieldWidth)
    {
        const float padding = ImMax(Theme::g_padding, (fieldWidth - ImGui::CalcTextSize(text).x) * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * ImGui::GetStyle().DisabledAlpha);
        SelectableField(key, text, fieldWidth, Theme::g_frameHeight, ImGui::GetFont(), Theme::g_text, padding, true, true);
        ImGui::PopStyleVar();
    }

    template <class Read>
    auto Values(Read read, bool dominant = false) const
    {
        return PropertyEdit::Aggregate(
            targets.size(),
            [&](size_t i)
            {
                return read(*targets[i].data, targets[i].index);
            },
            dominant);
    }

    template <class Read>
    auto Scalar(ImGuiID id, Read read)
    {
        auto value = Values(read);
        auto& cache = buffers[id];
        std::uint64_t signature = 1469598103934665603ull;
        for (const auto& target : targets)
        {
            const auto scalar = read(*target.data, target.index);
            const auto* bytes = reinterpret_cast<const unsigned char*>(&scalar);
            for (size_t byte = 0; byte < sizeof(scalar); ++byte)
            {
                signature = (signature ^ bytes[byte]) * 1099511628211ull;
            }
        }
        if (!cache.initialized || (cache.signature != signature && session.Owner() != id))
        {
            cache.seed = static_cast<double>(value.mixed ? Values(read, true).value : value.value);
            cache.signature = signature;
            cache.initialized = true;
        }
        value.value = static_cast<decltype(value.value)>(cache.seed);
        return value;
    }

    bool Write(ImGuiID owner, const PropertyEdit::Session::Patch& patch, const PropertyWidgetResult& result);
    bool Finish(bool settle = true);
    void Bind();
    bool All(const std::function<bool(const ItemData&, int)>& predicate) const
    {
        return std::all_of(targets.begin(), targets.end(),
            [&](const auto& target)
            {
                return predicate(*target.data, target.index);
            });
    }

    template <class Field, class Value>
    void Set(ImGuiID owner, const Field& field, const Value& value, const PropertyWidgetResult& result)
    {
        if (result.changed)
        {
            Write(
                owner,
                [&](int index, ItemData& data, std::string& error)
                {
                    return field.write(data, index, value, error);
                },
                result);
        }
        Observe(result);
    }

    void Observe(const PropertyWidgetResult& result)
    {
        interacting |= result.active;
        if (result.active)
        {
            interactionType = currentComponent.type;
            interactionFrame = ImGui::GetFrameCount();
        }
    }

    template <class Field>
    void Int(const char* label, const char* key, const Field& field, int minimum, int maximum, int step = 1, float speed = 0.1f,
        const char* format = "%d", bool enabled = true);

    template <class Field>
    ImRect Identifier(const char* label, const char* key, const Field& field, int minimum, int maximum, bool enabled,
        const std::function<void()>& browse = {}, bool mixedIdentity = false, int (*step)(int, bool) = nullptr,
        const char* browseKey = "##browse");

    template <class Field>
    void Float(const char* label, const char* key, const Field& field, float minimum, float maximum, float step = 0.25f,
        float speed = 0.05f, const char* format = "%.3f", bool smart = true, bool enabled = true);

    template <class Field>
    void Check(const char* label, const char* key, const Field& field, bool enabled = true);

    template <class Field, class Value>
    void Combo(const char* label, const char* key, const Field& field, const char* const* labels, const Value* choices,
        size_t count, bool enabled = true);

    template <class Field, size_t Count>
    void Enum(const char* label, const char* key, const Field& field, const char* const (&labels)[Count], bool enabled = true);

    template <class Field>
    void Vector(const char* label, const char* key, const Field& field, bool rotation = false, bool compact = false,
        unsigned disabledAxes = 0, bool enabled = true, float minimum = -20000.0f, float maximum = 20000.0f);

    template <class Field>
    void Text(
        const char* label, const char* key, const Field& field, size_t capacity, bool multiline = false, bool enabled = true);

    void MixedCard(const char* key, int rows, bool image = true);

    void Model(ComponentType type, bool enabled = true);
    void Color(const char* label, const char* key, const std::function<unsigned(const ItemData&, int)>& read,
        const std::function<unsigned&(ItemData&, int)>& write, bool enabled = true, bool rgba = false);
    void LayoutFields();
    void ComponentFields(PropertyEdit::Component component);
    void HeaderMenu(const ComponentMenuCtx* component);
    void GlobalMenu(bool direct = false);
    void Add(ComponentType type, std::optional<int> model = std::nullopt);
    bool FreezePicker(InspectionPicker::Kind kind, PropertyEdit::Component component, bool adding = false);
    bool PickerValid();
};

static PropertyInspector g_propertyInspector;

static std::array<float, 3> InspectionWorldPosition(const ItemData& data, int index)
{
    if (index < 0)
    {
        return { data.transform.pos[0], data.transform.pos[1], data.transform.pos[2] };
    }
    const auto& world = g_scene->world[index];
    return { world.pos.x, world.pos.y, world.pos.z };
}

static std::array<float, 3> InspectionRotation(const ItemData& data, int index, bool global)
{
    const float* rotation = index >= 0 && global ? g_scene->world[index].eulerDeg : data.transform.rot;
    return { rotation[0], rotation[1], rotation[2] };
}

#include "property_inspector_widgets.inl"

#define INSPECT(component, member) \
    InspectMember(&ItemData::component, &std::remove_cvref_t<decltype(ItemData {}.component.Get())>::member)

#define INSPECT_VECTOR(component, type, member) InspectArray(&ItemData::component, &type::member)

#include "property_inspector_components.inl"
#include "property_inspector_layout.inl"

#undef INSPECT_VECTOR
#undef INSPECT

#include "property_inspector_menus.inl"
#include "property_inspector_pickers.inl"
#include "property_inspector_session.inl"

static void DrawInspectionNotice(const char* key, const char* text, float width)
{
    const float textWidth = ImMax(1.0f, width - Theme::g_padding * 2.0f);
    auto* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    const float scale = fontSize / font->FontSize;
    const char* textEnd = text + strlen(text);
    auto lines = [&](auto draw)
    {
        int count = 0;
        for (const char* line = text; *line; ++count)
        {
            const char* end = font->CalcWordWrapPositionA(scale, line, textEnd, textWidth);
            if (end == line)
            {
                ++end;
            }
            draw(count, line, end);
            line = end;
            while (*line == ' ' || *line == '\t' || *line == '\n')
            {
                ++line;
            }
        }
        return count;
    };
    const int count = lines(
        [](int, const char*, const char*)
        {
        });
    ImGui::SetCursorPosX(0);
    const auto minimum = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(key, ImVec2(width, count * fontSize + Theme::g_popupPadding * 2.0f));
    const auto maximum = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(minimum, maximum, ImGui::GetColorU32(Theme::g_infoCardBackground));
    draw->AddRect(minimum, maximum, ImGui::GetColorU32(Theme::g_border), 0, 0, Ui::Hairline());
    draw->PushClipRect(minimum + ImVec2(Theme::g_padding, Theme::g_popupPadding),
        maximum - ImVec2(Theme::g_padding, Theme::g_popupPadding), true);
    lines(
        [&](int index, const char* line, const char* end)
        {
            const auto size = ImGui::CalcTextSize(line, end);
            draw->AddText(ImVec2(minimum.x + (width - size.x) * 0.5f, minimum.y + Theme::g_popupPadding + index * fontSize),
                ImGui::GetColorU32(Theme::g_textDim), line, end);
        });
    draw->PopClipRect();
}

static void MeasureInspectionComponents()
{
    auto& inspector = g_propertyInspector;
    if (inspector.layoutSignature == inspector.structureSignature && inspector.layoutWidth == inspector.width
        && inspector.layoutScale == Ui::Scale())
    {
        return;
    }
    PropertyInspector measurement;
    measurement.targets = inspector.targets;
    measurement.ids = inspector.ids;
    measurement.width = inspector.width;
    measurement.readOnly = inspector.readOnly;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(inspector.width, ImGui::GetIO().DisplaySize.y));
    ImGui::Begin("##PropertiesMeasure", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground);
    ImGui::GetCurrentWindow()->Hidden = true;
    ImGui::GetCurrentWindow()->DC.LayoutFloorY = true;
    ImGui::SetScrollY(0);
    ImGui::PushClipRect(ImVec2(0, 0), ImVec2(0, 0), false);
    ImGui::BeginDisabled();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, inspector.Gap()));
    inspector.componentHeights.clear();
    for (const auto entry : inspector.components)
    {
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::PushID(entry.type * 32 + entry.slot + 1);
        measurement.currentComponent = entry;
        measurement.ComponentFields(entry);
        inspector.componentHeights.push_back({ ImGui::GetCursorPosY() + inspector.Gap(),
            ImGui::GetItemRectMax().y - ImGui::GetWindowPos().y + ImGui::GetScrollY() });
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, Theme::g_padding));
    ImGui::SetCursorPos(ImVec2(0, 0));
    if (inspector.partial)
    {
        DrawInspectionNotice("##partial_components", "Some components are not shared by all selected items.", inspector.width);
        inspector.noticeHeights[0] = ImGui::GetItemRectSize().y;
    }
    if (inspector.readOnly)
    {
        DrawInspectionNotice("##native_readonly", "Native entities are read-only.", inspector.width);
        inspector.noticeHeights[1] = ImGui::GetItemRectSize().y;
    }
    ImGui::PopStyleVar();
    ImGui::EndDisabled();
    ImGui::PopClipRect();
    ImGui::End();
    inspector.layoutSignature = inspector.structureSignature;
    inspector.layoutWidth = inspector.width;
    inspector.layoutScale = Ui::Scale();
}

static void DrawInspectionProperties(float width)
{
    auto& inspector = g_propertyInspector;
    inspector.width = width;
    inspector.drawFrame = ImGui::GetFrameCount();
    inspector.Bind();
    if (auto command = std::exchange(inspector.pending, {}))
    {
        command();
        inspector.Bind();
    }
    if (inspector.targets.empty())
    {
        DrawCenteredMessage("Nothing selected.");
        return;
    }
    inspector.drawing = true;
    ImGui::PushID("##inspection");
    inspector.nameOwner = ImGui::GetID("##item_name");
    auto& buffer = inspector.buffers[inspector.nameOwner];
    const auto name = PropertyEdit::Aggregate(inspector.targets.size(),
        [&](size_t i)
        {
            const auto& target = inspector.targets[i];
            return std::string_view(target.index >= 0 ? g_scene->items[target.index].name
                                                      : inspector.nativeNames[target.data - inspector.nativeData.data()].data());
        });
    if (!buffer.focus)
    {
        snprintf(
            buffer.text.data(), 64, "%s", name.mixed ? (inspector.readOnly ? PropertyEdit::MixedText : "") : name.value.data());
    }
    const bool canName = !inspector.readOnly
        && inspector.All(
            [](const ItemData&, int index)
            {
                return g_scene->items[index].type != SceneItemRaceCheckpoint;
            });
    bool canAdd = !inspector.readOnly && !inspector.ids.empty();
    if (canAdd)
    {
        canAdd = false;
        for (int type = ComponentTypeModel; type <= ComponentTypeCurveNode; ++type)
        {
            canAdd |= inspector.All(
                [&](const ItemData& data, int index)
                {
                    return CanAddComponentType(g_scene->items[index], data, static_cast<ComponentType>(type));
                });
        }
    }
    const float inputWidth = width - Theme::g_frameHeight * (canAdd ? 2 : 1) - Theme::g_padding * (canAdd ? 2 : 1);
    ImGui::SetCursorPos(ImVec2(Theme::g_padding, ImGui::GetCursorPosY() + Theme::g_padding));
    ImGui::BeginDisabled(!canName && !inspector.readOnly);
    const size_t nameCapacity = inspector.All(
                                    [](const ItemData&, int index)
                                    {
                                        return index < 0 || g_scene->items[index].type != SceneItemRace;
                                    })
        ? 64
        : RaceData::MaxName + 1;
    if (inspector.readOnly)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * ImGui::GetStyle().DisabledAlpha);
    }
    inspector.nameDirty |= TextInput("##item_name", name.mixed ? PropertyEdit::MixedText : "Name", buffer.text.data(),
        nameCapacity, inputWidth, inspector.readOnly ? ImGuiInputTextFlags_ReadOnly : 0);
    buffer.focus = ImGui::IsItemActive();
    if (inspector.readOnly)
    {
        ImGui::PopStyleVar();
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && !ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        inspector.Finish();
    }
    ImGui::EndDisabled();
    if (canAdd)
    {
        ImGui::SameLine(0, Theme::g_padding);
        if (IconSquareButton("##PropAddComponent", g_icons->addTexture, Theme::g_frameHeight))
        {
            ImGui::OpenPopup("##add_component");
        }
    }
    ImGui::SameLine(0, Theme::g_padding);
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::g_danger);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::g_dangerHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::g_dangerActive);
    const bool remove = IconSquareButton("##PropDel", g_icons->deleteTexture, Theme::g_frameHeight, IM_COL32_WHITE);
    ImGui::PopStyleColor(3);
    if (remove && inspector.Finish())
    {
        Selection::DeleteUnifiedSelection();
        ImGui::PopID();
        inspector.drawing = false;
        return;
    }
    if (ImCustom::BeginPopup("##add_component"))
    {
        inspector.GlobalMenu(true);
        ImCustom::EndPopup();
    }
    ImGui::SetCursorPos(ImVec2(Theme::g_padding, ImGui::GetCursorPosY()));
    MeasureInspectionComponents();
    const float viewHeight = floorf(ImMax(4.0f, ImGui::GetContentRegionAvail().y));
    const auto* previousWindow = FindItemListWindow("##PropComponents");
    const float previousScroll = previousWindow ? previousWindow->Scroll.y : 0;
    float contentHeight = 0;
    float contentBottom = 0;
    ImVec2 scrollRange(0, 0);
    for (size_t index = 0; index < inspector.components.size(); ++index)
    {
        const auto component = inspector.components[index];
        auto [entry, inserted] = inspector.open.try_emplace(component.type * 32 + component.slot + 1, true);
        const bool focus = g_propScroll.type == component.type && g_propScroll.matSlot == component.slot
            && std::find(inspector.ids.begin(), inspector.ids.end(), g_propScroll.itemId) != inspector.ids.end();
        entry->second |= focus;
        const float top = contentHeight;
        contentBottom = contentHeight + Theme::g_headerHeight;
        contentHeight = floorf(contentHeight + Theme::g_headerHeight + Theme::g_padding);
        if (entry->second)
        {
            contentBottom = contentHeight + inspector.componentHeights[index].bottom;
            contentHeight += inspector.componentHeights[index].advance;
        }
        if (focus)
        {
            scrollRange = ImVec2(top, contentHeight);
            g_propScroll.itemId = -1;
        }
    }
    for (int notice = 0; notice < 2; ++notice)
    {
        if (notice == 0 ? inspector.partial : inspector.readOnly)
        {
            contentBottom = contentHeight + inspector.noticeHeights[notice];
            contentHeight = floorf(contentBottom + Theme::g_padding);
        }
    }
    ImGui::SetNextWindowContentSize(ImVec2(0, ImMax(1.0f, floorf(contentBottom + Theme::g_padding))));
    if (scrollRange.y > scrollRange.x && (scrollRange.x < previousScroll || scrollRange.y > previousScroll + viewHeight))
    {
        const float target = ImMin(scrollRange.x, scrollRange.y - viewHeight);
        ImGui::SetNextWindowScroll(ImVec2(-1, ImMax(0.0f, target)));
    }
    ImGui::BeginChild("##PropComponents", ImVec2(width + Theme::g_padding, 0), false);
    ImGui::GetCurrentWindow()->DC.LayoutFloorY = true;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, Theme::g_padding));
    static const char* const titles[] = { "Transform", "Object", "Slide", "Material Texture", "Remove Building", "Material Text",
        "Vehicle", "Box Area", "Pickup", "Actor", "Actor Animation", "Race", "Race Checkpoint", "Layout", "Curve", "Curve Node" };
    for (size_t componentIndex = 0; componentIndex < inspector.components.size(); ++componentIndex)
    {
        const auto component = inspector.components[componentIndex];
        if (inspector.interrupted)
        {
            break;
        }
        const int key = component.type * 32 + component.slot + 1;
        auto [entry, inserted] = inspector.open.try_emplace(key, true);
        char id[32], title[64];
        snprintf(id, sizeof(id), "hdr_%d_%d", component.type, component.slot);
        snprintf(title, sizeof(title), "%s", titles[component.type]);
        if (component.type == ComponentTypeTransform)
        {
            snprintf(title, sizeof(title), "Transform (%s)",
                inspector.readOnly || g_gizmos->GetGizmoSpace() == GizmoSpace::Global ? "Global" : "Local");
        }
        if (component.slot >= 0)
        {
            const int count = component.type == ComponentTypeMaterialTexture
                ? inspector.targets.front().data->MaterialCount()
                : inspector.targets.front().data->MaterialTextCount();
            if (count > 1)
            {
                snprintf(title, sizeof(title), "%s %d", titles[component.type], component.slot + 1);
            }
        }
        const int primary = inspector.Primary();
        const ComponentMenuCtx menu { primary, component.type, component.slot };
        const auto blockOrigin = ImGui::GetCursorScreenPos();
        const bool expanded = DrawComponentHeader(id, title, &entry->second, width, &menu, inspector.readOnly);
        const float bodyTop = ImGui::GetCursorPosY();
        inspector.currentComponent = component;
        if (expanded && !inspector.interrupted)
        {
            ImGui::BeginDisabled(
                inspector.readOnly && component.type != ComponentTypeTransform && component.type != ComponentTypeModel);
            ImGui::PushID(key);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, inspector.Gap()));
            inspector.ComponentFields(component);
            ImGui::PopStyleVar();
            ImGui::PopID();
            ImGui::EndDisabled();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + inspector.Gap());
            inspector.componentHeights[componentIndex] = { ImGui::GetCursorPosY() - bodyTop,
                ImGui::GetItemRectMax().y - ImGui::GetWindowPos().y + ImGui::GetScrollY() - bodyTop };
        }
        const ImVec2 blockMin(blockOrigin.x, blockOrigin.y + Theme::g_headerHeight);
        const ImVec2 blockMax(blockMin.x + width, ImGui::GetCursorScreenPos().y);
        ImGui::PushID(key);
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
            && ImGui::IsMouseHoveringRect(blockMin, blockMax) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("##component_context");
        }
        if (ImCustom::BeginPopup("##component_context"))
        {
            inspector.HeaderMenu(&menu);
            ImCustom::EndPopup();
        }
        ImGui::PopID();
    }
    if (inspector.partial)
    {
        DrawInspectionNotice("##partial_components", "Some components are not shared by all selected items.", width);
    }
    if (inspector.readOnly)
    {
        DrawInspectionNotice("##native_readonly", "Native entities are read-only.", width);
    }
    const float free = ImGui::GetCurrentWindow()->InnerRect.Max.y - ImGui::GetCursorScreenPos().y - Theme::g_padding;
    if (free > Ui::Px(2) && ImGui::InvisibleButton("##prop_freespace", ImVec2(width, free), ImGuiButtonFlags_MouseButtonRight))
    {
        ImGui::OpenPopup("##properties_menu");
    }
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetItemRectMax().y));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::Dummy(ImVec2(0, Theme::g_padding));
    ImGui::PopStyleVar();
    if (ImCustom::BeginPopup("##properties_menu"))
    {
        inspector.GlobalMenu();
        ImCustom::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopID();
    inspector.drawing = false;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && inspector.session.Active())
    {
        inspector.session.Cancel(*g_scene);
        g_scene->UpdateWorldTransforms();
        inspector.buffers.clear();
        ImGui::ClearActiveID();
    }
    else if (inspector.session.Active() && !inspector.interacting)
    {
        inspector.Finish();
    }
}
