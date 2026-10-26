// Source Repository: https://github.com/waverso/waversomapstudio

struct MaterialWidgets
{
    char colorBuf[12] = "FFFFFFFF";
    ImVec4 colorPickerCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    bool colorHasFocus = false;
    bool open = true;
    unsigned int lastColorArgb = 0xFFFFFFFF;
    int customTextureModelId = -1;
};

struct MaterialTextWidgets
{
    char fontColorBuf[12] = "FFFFFFFF";
    ImVec4 fontColorCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    bool fontColorHasFocus = false;
    unsigned int lastFontColorArgb = 0xFFFFFFFF;

    char backColorBuf[12] = "00000000";
    ImVec4 backColorCol = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    bool backColorHasFocus = false;
    unsigned int lastBackgroundColorArgb = 0x00000000;

    bool open = true;
};

struct BoxBoundsState
{
    float values[6] = {};
    TransformData transform;
    TransformData parentTransform;
    BoxAreaData area;
    int parentId = -1;
    bool valid = false;
};

struct ItemWidgetState
{
    bool transformOpen = true;
    bool lastTransformSpaceGlobal = false;
    Vec3State posState;
    Vec3State rotState;

    bool modelOpen = true;
    char modelBuf[8] = "0";
    bool modelHasFocus = false;

    bool slideOpen = true;
    bool actorAnimationOpen = true;
    char animationBuf[8] = {};
    bool animationHasFocus = false;
    Vec3State slidePosState;
    Vec3State slideRotState;
    bool boxAreaOpen = true;
    bool raceOpen = true;
    bool layoutOpen = true;
    bool curveOpen = true;
    char curveSearch[64] = {};
    Vec3State layoutRotState;
    char layoutRangeBuf[2][16] = {};
    bool layoutRangeFocus[2] = {};
    Vec3State curveInState;
    Vec3State curveOutState;
    Vec3State boxSizeState;
    BoxBoundsState boxBounds;
    unsigned int boxBoundsActiveId = 0;
    int boxBoundsActiveFace = -1;

    MaterialWidgets materials[16];
    MaterialTextWidgets materialTexts[16];

    bool rbOpen = true;
    char rbModelBuf[8] = "-1";
    bool rbModelHasFocus = false;
    unsigned int rbRadiusActiveId = 0;
};

struct ComponentClipboard
{
    bool valid = false;
    ComponentType type = ComponentTypeTransform;
    TransformData transform;
    TransformData worldTransform;
    VehicleData vehicle;
    PickupData pickup;
    ActorData actor;
    ActorAnimationData actorAnimation;
    ModelData model;
    SlideData slide;
    BoxAreaData boxArea;
    RaceData race;
    RaceCheckpointData raceCheckpoint;
    LayoutData layout;
    CurveData curve;
    CurveNodeData curveNode;
    MaterialData material;
    MaterialTextData materialText;
};

struct ComponentMenuCtx
{
    int itemIdx = -1;
    ComponentType type = ComponentTypeTransform;
    int materialSlot = -1;
};

static bool InspectionHeaderMenu(const ComponentMenuCtx* context);
static bool InspectionConfirmModel(int model, ComponentType type);
static bool InspectionConfirmTexture(int model, const char* library, const char* texture);
static bool InspectionConfirmAnimation(int id);
static void InspectionFreezeTexture(int item, int slot);
static bool InspectionUsesTransactions();
static int g_pendingCurveLocateId = -1;
class ItemWidgetStore
{
    struct Layout
    {
        bool transformOpen;
        bool modelOpen;
        bool slideOpen;
        bool actorAnimationOpen;
        bool rbOpen;
        bool boxAreaOpen;
        bool raceOpen;
        bool layoutOpen;
        bool curveOpen;
        bool materialOpen[16];
        bool textOpen[16];
        explicit Layout(const ItemWidgetState& state)
            : transformOpen(state.transformOpen), modelOpen(state.modelOpen), slideOpen(state.slideOpen),
              actorAnimationOpen(state.actorAnimationOpen), rbOpen(state.rbOpen), boxAreaOpen(state.boxAreaOpen),
              raceOpen(state.raceOpen), layoutOpen(state.layoutOpen), curveOpen(state.curveOpen)
        {
            for (int i = 0; i < 16; ++i)
            {
                materialOpen[i] = state.materials[i].open;
                textOpen[i] = state.materialTexts[i].open;
            }
        }
        void Restore(ItemWidgetState& state) const
        {
            state.transformOpen = transformOpen;
            state.modelOpen = modelOpen;
            state.slideOpen = slideOpen;
            state.actorAnimationOpen = actorAnimationOpen;
            state.rbOpen = rbOpen;
            state.boxAreaOpen = boxAreaOpen;
            state.raceOpen = raceOpen;
            state.layoutOpen = layoutOpen;
            state.curveOpen = curveOpen;
            for (int i = 0; i < 16; ++i)
            {
                state.materials[i].open = materialOpen[i];
                state.materialTexts[i].open = textOpen[i];
            }
        }
    };
    struct Entry
    {
        ItemWidgetState state;
        unsigned long long stamp = 0;
    };
    std::unordered_map<int, Entry> entries;
    std::unordered_map<int, Layout> layouts;
    unsigned long long stamp = 0;

    public:
    ItemWidgetState& operator[](int index)
    {
        const int id = g_scene->items[index].id;
        auto found = entries.find(id);
        if (found == entries.end())
        {
            if (entries.size() >= 32)
            {
                auto oldest = entries.end();
                for (auto it = entries.begin(); it != entries.end(); ++it)
                {
                    if (oldest == entries.end() || it->second.stamp < oldest->second.stamp)
                    {
                        oldest = it;
                    }
                }
                layouts.insert_or_assign(oldest->first, Layout(oldest->second.state));
                entries.erase(oldest);
            }
            found = entries.try_emplace(id).first;
            const auto layout = layouts.find(id);
            if (layout != layouts.end())
            {
                layout->second.Restore(found->second.state);
                layouts.erase(layout);
            }
        }
        found->second.stamp = ++stamp;
        return found->second.state;
    }
    ItemWidgetState* Find(int index)
    {
        const auto found = entries.find(g_scene->items[index].id);
        return found == entries.end() ? nullptr : &found->second.state;
    }
    void Reset(int index)
    {
        const int id = g_scene->items[index].id;
        entries.erase(id);
        layouts.erase(id);
    }
    void Prune()
    {
        std::erase_if(entries,
            [](const auto& entry)
            {
                return g_scene->FindIndexById(entry.first) < 0;
            });
        std::erase_if(layouts,
            [](const auto& entry)
            {
                return g_scene->FindIndexById(entry.first) < 0;
            });
    }
    void Clear()
    {
        entries.clear();
        layouts.clear();
    }
};
static ItemWidgetStore g_itemWidgets;
static ComponentClipboard g_componentClipboard;

static void ResetItemWidgetsRange(int first, int last)
{
    for (int i = first; i < last; ++i)
    {
        g_itemWidgets.Reset(i);
    }
}

static bool CanPasteNewComponent(const SceneItem& item, const ItemData& data, ComponentType type)
{
    return g_componentClipboard.valid && g_componentClipboard.type == type && CanAddComponentType(item, data, type);
}

static void CopyComponentToClipboard(
    const ItemData& data, ComponentType type, int materialSlot, const TransformData& worldTransform)
{
    g_componentClipboard.valid = true;
    g_componentClipboard.type = type;
    switch (type)
    {
        case ComponentTypeTransform:
            g_componentClipboard.transform = data.transform;
            g_componentClipboard.worldTransform = worldTransform;
            break;
        case ComponentTypeVehicle:
            g_componentClipboard.vehicle = data.vehicle.Get();
            break;
        case ComponentTypePickup:
            g_componentClipboard.pickup = data.pickup.Get();
            break;
        case ComponentTypeActor:
            g_componentClipboard.actor = data.actor.Get();
            break;
        case ComponentTypeActorAnimation:
            g_componentClipboard.actorAnimation = data.actorAnimation.Get();
            break;
        case ComponentTypeModel:
            g_componentClipboard.model = data.model.Get();
            break;
        case ComponentTypeSlide:
            g_componentClipboard.slide = data.slide.Get();
            break;
        case ComponentTypeBoxArea:
            g_componentClipboard.boxArea = data.boxArea.Get();
            break;
        case ComponentTypeRace:
            g_componentClipboard.race = data.race.Get();
            break;
        case ComponentTypeRaceCheckpoint:
            g_componentClipboard.raceCheckpoint = data.raceCheckpoint.Get();
            break;
        case ComponentTypeLayout:
            g_componentClipboard.layout = data.layout.Get();
            break;
        case ComponentTypeCurve:
            g_componentClipboard.curve = data.curve.Get();
            break;
        case ComponentTypeCurveNode:
            g_componentClipboard.curveNode = data.curveNode.Get();
            break;
        case ComponentTypeMaterialTexture:
            if (materialSlot >= 0 && materialSlot < data.MaterialCount())
            {
                g_componentClipboard.material = data.materials[materialSlot];
            }
            else
            {
                g_componentClipboard.valid = false;
            }
            break;
        case ComponentTypeMaterialText:
            if (materialSlot >= 0 && materialSlot < data.MaterialTextCount())
            {
                g_componentClipboard.materialText = data.materialTexts[materialSlot];
            }
            else
            {
                g_componentClipboard.valid = false;
            }
            break;
        default:
            g_componentClipboard.valid = false;
            break;
    }
}

struct PropScrollRequest
{
    int itemId = -1;
    ComponentType type = ComponentTypeTransform;
    int matSlot = -1;
};
static PropScrollRequest g_propScroll;

static void RequestComponentScroll(int itemIdx, ComponentType type, int matSlot = -1)
{
    if (!g_scene || itemIdx < 0 || itemIdx >= g_scene->count || !ItemHasComponent(g_scene->data[itemIdx], type))
    {
        return;
    }
    auto& widgets = g_itemWidgets[itemIdx];
    switch (type)
    {
        case ComponentTypeModel:
        case ComponentTypeVehicle:
        case ComponentTypePickup:
        case ComponentTypeActor:
            widgets.modelOpen = true;
            break;
        case ComponentTypeActorAnimation:
            widgets.actorAnimationOpen = true;
            break;
        case ComponentTypeSlide:
            widgets.slideOpen = true;
            break;
        case ComponentTypeBoxArea:
            widgets.boxAreaOpen = true;
            break;
        case ComponentTypeLayout:
            widgets.layoutOpen = true;
            break;
        case ComponentTypeCurve:
        case ComponentTypeCurveNode:
            widgets.curveOpen = true;
            break;
        case ComponentTypeMaterialTexture:
            if (matSlot < 0 || matSlot >= g_scene->data[itemIdx].MaterialCount())
            {
                return;
            }
            widgets.materials[matSlot].open = true;
            break;
        case ComponentTypeMaterialText:
            if (matSlot < 0 || matSlot >= g_scene->data[itemIdx].MaterialTextCount())
            {
                return;
            }
            widgets.materialTexts[matSlot].open = true;
            break;
        default:
            return;
    }
    g_propScroll.itemId = g_scene->items[itemIdx].id;
    g_propScroll.type = type;
    g_propScroll.matSlot = matSlot;
}

static void GuiAddComponent(int itemIdx, ComponentType type)
{
    if (!g_scene || itemIdx < 0 || itemIdx >= g_scene->count
        || !CanAddComponentType(g_scene->items[itemIdx], g_scene->data[itemIdx], type))
    {
        return;
    }
    const int previousMaterialCount = g_scene->data[itemIdx].MaterialCount();
    AddComponentToItem(g_scene->data[itemIdx], type);
    if (type == ComponentTypeMaterialTexture && g_scene->data[itemIdx].MaterialCount() == previousMaterialCount)
    {
        g_toast.ShowError("Could not add material texture. Check the catalog and slot limit.");
        return;
    }
    int slot = -1;
    if (type == ComponentTypeMaterialTexture)
    {
        slot = g_scene->data[itemIdx].MaterialCount() - 1;
        if (slot >= 0)
        {
            g_itemWidgets[itemIdx].materials[slot] = MaterialWidgets {};
        }
    }
    else if (type == ComponentTypeMaterialText)
    {
        slot = g_scene->data[itemIdx].MaterialTextCount() - 1;
        if (slot >= 0)
        {
            g_itemWidgets[itemIdx].materialTexts[slot] = MaterialTextWidgets {};
        }
    }
    RequestComponentScroll(itemIdx, type, slot);
}
