// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "scene_limits.h"

enum SceneItemType
{
    SceneItemNormal = 0,
    SceneItemRemoveBuilding = 1,
    SceneItemRace = 2,
    SceneItemRaceCheckpoint = 3,
    SceneItemCurve = 4,
    SceneItemCurveNode = 5
};

enum SceneDropMode
{
    SceneDropNone = 0,
    SceneDropBefore,
    SceneDropAfter,
    SceneDropInto,
    SceneDropRoot
};

enum ComponentType
{
    ComponentTypeTransform = 0,
    ComponentTypeModel,
    ComponentTypeSlide,
    ComponentTypeMaterialTexture,
    ComponentTypeRemoveBuilding,
    ComponentTypeMaterialText,
    ComponentTypeVehicle,
    ComponentTypeBoxArea,
    ComponentTypePickup,
    ComponentTypeActor,
    ComponentTypeActorAnimation,
    ComponentTypeRace,
    ComponentTypeRaceCheckpoint,
    ComponentTypeLayout,
    ComponentTypeCurve,
    ComponentTypeCurveNode
};

struct SceneItem
{
    int id = 0;
    int parent = -1;
    int order = 0;
    SceneItemType type = SceneItemNormal;
    bool open = true;
    bool selected = false;
    char name[64] = {};

    int searchModelId = -2;
};

struct TransformData
{
    static constexpr float MinZ = -99.9f;

    float pos[3] = { 0.0f, 0.0f, 0.0f };
    float rot[3] = { 0.0f, 0.0f, 0.0f };
    bool operator==(const TransformData&) const = default;
};

struct ModelData
{
    int modelId = 0;
    float drawDistance = 0.0f;
    float streamDistance = 300.0f;
    bool operator==(const ModelData&) const = default;
};

struct VehicleData
{
    static constexpr float MinHealth = 0.0f;
    static constexpr float MaxHealth = 99999.9f;

    int modelId = 400;
    int colors[2] = { -1, -1 };
    float health = 1000.0f;
    bool operator==(const VehicleData&) const = default;
};

struct PickupData
{
    static constexpr float MinStreamDistance = 10.0f;
    static constexpr float MaxStreamDistance = 3000.0f;

    int modelId = 1239;
    float streamDistance = 200.0f;
    bool operator==(const PickupData&) const = default;
};

struct ActorData
{
    static constexpr float MinHealth = 0.0f;
    static constexpr float MaxHealth = 99999.9f;
    static constexpr float MinStreamDistance = 10.0f;
    static constexpr float MaxStreamDistance = 3000.0f;

    int skinId = 0;
    float health = 100.0f;
    bool invulnerable = true;
    float streamDistance = 200.0f;
    bool operator==(const ActorData&) const = default;
};

struct ActorAnimationData
{
    char library[16] = "DANCING";
    char name[24] = "dnce_M_a";
    float delta = 4.1f;
    bool loop = true;
    bool lockX = false;
    bool lockY = false;
    bool freeze = false;
    int time = 0;
    bool operator==(const ActorAnimationData& other) const
    {
        return std::strncmp(library, other.library, sizeof(library)) == 0 && std::strncmp(name, other.name, sizeof(name)) == 0
            && delta == other.delta && loop == other.loop && lockX == other.lockX && lockY == other.lockY
            && freeze == other.freeze && time == other.time;
    }
};

struct SlideData
{
    float targetPos[3] = { 0.0f, 0.0f, 0.0f };
    float targetRot[3] = { 0.0f, 0.0f, 0.0f };
    float speed = 5.0f;
    bool operator==(const SlideData&) const = default;
};

struct MaterialData
{
    int materialIndex = 0;
    unsigned int colorArgb = 0xFFFFFFFF;
    int textureModelId = -1;
    char txdName[24] = {};
    char textureName[128] = {};
    bool operator==(const MaterialData&) const = default;
};

struct MaterialTextData
{
    int materialIndex = 0;
    char text[2048] = "Text";
    int materialSize = 90;
    char fontFace[32] = "Arial";
    int fontSize = 24;
    bool bold = true;
    unsigned int fontColorArgb = 0xFFFFFFFF;
    unsigned int backgroundColorArgb = 0xFF000000;
    int alignment = 0;
    bool operator==(const MaterialTextData&) const = default;
};

struct RemoveBuildingData
{
    int modelId = -1;
    float radius = 0.25f;
    bool custom = false;
    bool operator==(const RemoveBuildingData&) const = default;
};

struct RaceData
{
    static constexpr int MaxName = 32;
    static constexpr int MaxLaps = 999;

    int mode = 0;
    int laps = 1;
    bool operator==(const RaceData&) const = default;
};

struct RaceCheckpointData
{
    static constexpr float MinRadius = 0.25f;
    static constexpr float MaxRadius = 1000.0f;
    static constexpr int AutoGround = 9;
    static constexpr int AutoAir = 10;

    float radius = 5.0f;
    int type = AutoGround;
    bool operator==(const RaceCheckpointData&) const = default;
};

struct CurveData
{
    bool closed = false;
    bool operator==(const CurveData&) const = default;
};

enum class CurveHandle
{
    Auto,
    Aligned,
    Free,
    Vector
};

struct CurveNodeData
{
    CurveHandle mode = CurveHandle::Auto;
    float incoming[3] = { -3.0f, 0.0f, 0.0f };
    float outgoing[3] = { 3.0f, 0.0f, 0.0f };
    float tilt = 0.0f;

    bool IsValid() const noexcept
    {
        const auto valid = [](float value)
        {
            return std::isfinite(value) && std::abs(value) <= 20000.0f;
        };
        return mode >= CurveHandle::Auto && mode <= CurveHandle::Vector
            && std::all_of(std::begin(incoming), std::end(incoming), valid)
            && std::all_of(std::begin(outgoing), std::end(outgoing), valid) && std::isfinite(tilt) && std::abs(tilt) <= 360.0f;
    }
    void Align(bool outgoingChanged) noexcept
    {
        if (mode != CurveHandle::Aligned)
        {
            return;
        }
        const float* source = outgoingChanged ? outgoing : incoming;
        float* target = outgoingChanged ? incoming : outgoing;
        const float length = std::hypot(source[0], source[1], source[2]);
        const float opposite = std::hypot(target[0], target[1], target[2]);
        if (length > 0.000001f)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                target[axis] = -source[axis] * opposite / length;
            }
        }
    }
    bool operator==(const CurveNodeData&) const = default;
};

enum class LayoutType
{
    Loop,
    Grid,
    Curve
};
enum class LayoutSpacing
{
    Copies,
    Step
};

struct LayoutData
{
    static constexpr float MinRadius = 0.0f;
    static constexpr float MaxRadius = 6000.0f;
    static constexpr float MinLoops = 0.001f;
    static constexpr float MaxLoops = 32.0f;
    static constexpr float MaxOffset = 2000.0f;
    static constexpr float MinStep = 0.001f;
    static constexpr float MaxStep = 20000.0f;

    LayoutType type = LayoutType::Loop;
    int modelId = 8838;
    int pieces = 0;
    int axis = 1;
    bool reverse = false;
    float radius = 20.0f;
    float loops = 1.0f;
    float startAngle = 90.0f;
    float offset = 0.0f;
    float rotation[3] = { 0.0f, 0.0f, 90.0f };
    bool autoRadius = false;
    bool autoStep = false;
    float spacing = 1.0f;
    int columns = 4;
    float gridSpacing[2] = { 10.0f, 10.0f };
    bool copyFromFirst = false;
    bool modelPending = false;
    bool piecesPending = false;
    int curveId = -1;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    bool lockCurveRotation[3] = {};
    float curveRange[2] = { 0.0f, 1.0f };
    LayoutSpacing spacingMode = LayoutSpacing::Copies;
    float step = 5.0f;

    bool UsesStep() const noexcept
    {
        return type != LayoutType::Grid && spacingMode == LayoutSpacing::Step;
    }

    bool IsValid() const noexcept
    {
        return type >= LayoutType::Loop && type <= LayoutType::Curve && modelId >= 0 && modelId < 20000 && pieces >= 0
            && pieces <= SceneLimits::MaxItems && axis >= 0 && axis < 3 && std::isfinite(radius) && radius >= MinRadius
            && radius <= MaxRadius && std::isfinite(loops) && loops >= MinLoops && loops <= MaxLoops && std::isfinite(startAngle)
            && std::abs(startAngle) <= 360.0f && std::isfinite(offset) && std::abs(offset) <= MaxOffset
            && std::all_of(
                std::begin(rotation), std::end(rotation),
                [](float value)
                {
                    return std::isfinite(value) && std::abs(value) <= 360.0f;
                })
            && std::isfinite(spacing) && spacing >= 0.01f && spacing <= 10.0f && columns >= 1 && columns <= SceneLimits::MaxItems
            && curveId >= -1 && std::isfinite(lateralOffset) && std::abs(lateralOffset) <= MaxOffset
            && std::isfinite(verticalOffset) && std::abs(verticalOffset) <= MaxOffset && std::isfinite(curveRange[0])
            && std::isfinite(curveRange[1]) && curveRange[0] >= 0.0f && curveRange[1] <= 1.0f && curveRange[0] <= curveRange[1]
            && spacingMode >= LayoutSpacing::Copies && spacingMode <= LayoutSpacing::Step && std::isfinite(step)
            && step >= MinStep && step <= MaxStep
            && std::all_of(
                std::begin(gridSpacing), std::end(gridSpacing),
                [](float value)
                {
                    return std::isfinite(value) && std::abs(value) <= MaxOffset;
                });
    }
    bool operator==(const LayoutData&) const = default;
};

struct BoxAreaData
{
    static constexpr float MinSize = 0.25f;
    static constexpr float MaxSize = 20000.0f;
    static constexpr unsigned int DefaultColor = 0x3AAEFFB3;

    unsigned int colorRGBA = DefaultColor;

    unsigned int GetColorARGB() const noexcept
    {
        return (colorRGBA >> 8) | (colorRGBA << 24);
    }

    float GetSize(int axis) const noexcept
    {
        return size[axis];
    }
    void SetSize(int axis, float value) noexcept
    {
        if (axis >= 0 && axis < 3 && std::isfinite(value))
        {
            size[axis] = std::clamp(value, MinSize, MaxSize);
        }
    }
    bool operator==(const BoxAreaData&) const = default;

    private:
    float size[3] = { 5.0f, 5.0f, 5.0f };
};

template <class T>
class ComponentValue
{
    public:
    ComponentValue() = default;
    ComponentValue(const ComponentValue& other)
    {
        if (other.value)
        {
            value = std::make_unique<T>(*other.value);
        }
    }
    ComponentValue(ComponentValue&&) noexcept = default;
    ComponentValue& operator=(ComponentValue&&) noexcept = default;
    ComponentValue& operator=(const ComponentValue& other)
    {
        if (other.value)
        {
            Edit() = *other.value;
        }
        else
        {
            Reset();
        }
        return *this;
    }
    ComponentValue& operator=(const T& data)
    {
        Edit() = data;
        return *this;
    }
    bool HasValue() const noexcept
    {
        return value != nullptr;
    }
    const T& Get() const noexcept
    {
        static const T empty;
        return value ? *value : empty;
    }
    T& Edit()
    {
        if (!value)
        {
            value = std::make_unique<T>();
        }
        return *value;
    }
    void Reset() noexcept
    {
        value.reset();
    }
    size_t MemoryUsage() const noexcept
    {
        return value ? sizeof(T) : 0;
    }
    bool operator==(const ComponentValue& other) const
    {
        return HasValue() == other.HasValue() && (!value || *value == *other.value);
    }

    private:
    std::unique_ptr<T> value;
};

template <class T>
struct ComponentEntry : T
{
    int sequence = 0;
    ComponentEntry& operator=(const T& data)
    {
        static_cast<T&>(*this) = data;
        return *this;
    }
    bool operator==(const ComponentEntry&) const = default;
};

template <class T>
class ComponentList
{
    public:
    ComponentList() = default;
    ComponentList(const ComponentList& other)
    {
        if (other.values)
        {
            values = std::make_unique<std::vector<T>>(*other.values);
        }
    }
    ComponentList(ComponentList&&) noexcept = default;
    ComponentList& operator=(ComponentList&&) noexcept = default;
    ComponentList& operator=(const ComponentList& other)
    {
        if (other.values)
        {
            if (values)
            {
                *values = *other.values;
            }
            else
            {
                values = std::make_unique<std::vector<T>>(*other.values);
            }
        }
        else
        {
            values.reset();
        }
        return *this;
    }
    size_t size() const noexcept
    {
        return values ? values->size() : 0;
    }
    size_t capacity() const noexcept
    {
        return values ? values->capacity() : 0;
    }
    bool empty() const noexcept
    {
        return size() == 0;
    }
    void resize(size_t count)
    {
        if (count)
        {
            Edit().resize(count);
        }
        else
        {
            values.reset();
        }
    }
    T& emplace_back()
    {
        return Edit().emplace_back();
    }
    void pop_back() noexcept
    {
        values->pop_back();
        if (values->empty())
        {
            values.reset();
        }
    }
    T& back() noexcept
    {
        return values->back();
    }
    const T& back() const noexcept
    {
        return values->back();
    }
    T& operator[](size_t index) noexcept
    {
        return (*values)[index];
    }
    const T& operator[](size_t index) const noexcept
    {
        return (*values)[index];
    }
    size_t MemoryUsage() const noexcept
    {
        return values ? sizeof(*values) + values->capacity() * sizeof(T) : 0;
    }
    bool operator==(const ComponentList& other) const
    {
        return size() == other.size() && (empty() || *values == *other.values);
    }

    private:
    std::vector<T>& Edit()
    {
        if (!values)
        {
            values = std::make_unique<std::vector<T>>(0);
        }
        return *values;
    }
    std::unique_ptr<std::vector<T>> values;
};

struct ItemData
{
    static constexpr int MaxMaterials = 16;

    TransformData transform;
    ComponentValue<VehicleData> vehicle;
    ComponentValue<PickupData> pickup;
    ComponentValue<ActorData> actor;
    ComponentValue<ActorAnimationData> actorAnimation;
    ComponentValue<ModelData> model;
    ComponentValue<SlideData> slide;
    ComponentValue<RemoveBuildingData> removeBuilding;
    ComponentValue<BoxAreaData> boxArea;
    ComponentValue<RaceData> race;
    ComponentValue<RaceCheckpointData> raceCheckpoint;
    ComponentValue<LayoutData> layout;
    ComponentValue<CurveData> curve;
    ComponentValue<CurveNodeData> curveNode;
    ComponentList<ComponentEntry<MaterialData>> materials;
    ComponentList<ComponentEntry<MaterialTextData>> materialTexts;
    int vehicleSeq = 0;
    int pickupSeq = 0;
    int actorSeq = 0;
    int actorAnimationSeq = 0;
    int modelSeq = 0;
    int slideSeq = 0;
    int boxAreaSeq = 0;
    int layoutSeq = 0;
    int nextSeq = 1;

    bool HasFixedRotation() const noexcept
    {
        return pickup.HasValue() || raceCheckpoint.HasValue() || boxArea.HasValue() || removeBuilding.HasValue();
    }
    bool SupportsRotation() const noexcept
    {
        return !HasFixedRotation()
            && (!curveNode.HasValue() || curveNode.Get().mode == CurveHandle::Aligned
                || curveNode.Get().mode == CurveHandle::Free);
    }
    int MaterialCount() const noexcept
    {
        return static_cast<int>(materials.size());
    }
    int MaterialTextCount() const noexcept
    {
        return static_cast<int>(materialTexts.size());
    }
    size_t DynamicMemoryUsage() const noexcept
    {
        return vehicle.MemoryUsage() + pickup.MemoryUsage() + actor.MemoryUsage() + actorAnimation.MemoryUsage()
            + model.MemoryUsage() + slide.MemoryUsage() + removeBuilding.MemoryUsage() + boxArea.MemoryUsage()
            + race.MemoryUsage() + raceCheckpoint.MemoryUsage() + layout.MemoryUsage() + curve.MemoryUsage()
            + curveNode.MemoryUsage() + materials.MemoryUsage() + materialTexts.MemoryUsage();
    }
    bool operator==(const ItemData&) const = default;
};

struct ItemSnapshot
{
    int id = 0;
    int parentId = -1;
    int order = 0;
    SceneItemType type = SceneItemNormal;
    bool open = true;
    int searchModelId = -2;
    char name[64] = {};
    ItemData data;
};
