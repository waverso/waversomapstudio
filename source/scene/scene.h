// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <span>
#include <unordered_map>
#include <vector>

#include "../utils/transform_math.h"
#include "scene_data.h"
#include "scene_limits.h"
#include "scene_storage.h"

bool ItemHasComponent(const ItemData& data, ComponentType type);
bool CanAddComponentType(const SceneItem& item, const ItemData& data, ComponentType type);
void AddComponentToItem(ItemData& data, ComponentType type);
void RemoveComponentFromItem(ItemData& data, ComponentType type, int materialSlot);
void ApplyModelDistanceDefaults(ModelData& m);
bool SetDefaultMaterialTexture(MaterialData& material);

class EditorScene
{
    public:
    static constexpr int MaxItems = SceneLimits::MaxItems;

    struct WorldTransform
    {
        CVector pos = CVector(0.0f, 0.0f, 0.0f);
        float eulerDeg[3] = { 0.0f, 0.0f, 0.0f };
        Utils::RotationBasis rot;
        bool changedThisFrame = false;
    };

    SceneStorage<SceneItem> items;
    SceneStorage<ItemData> data;
    SceneStorage<WorldTransform> world;
    int count = 0;
    int nextId = 1;

    EditorScene() : m_indexById(0), m_hierarchy(0), m_siblingOrder(0), m_displayOrder(0)
    {
    }
    void CopyTo(EditorScene& target) const;
    void Swap(EditorScene& other) noexcept;

    void InvalidateHierarchy() noexcept;
    const std::vector<int>& DisplayOrder() const;
    int HierarchyDepth(int index) const;
    bool CanParent(SceneItemType type, int parent) const;
    void UpdateRaceNames();
    void DiscardAppendedItems(int first) noexcept;
    int AppendItem(int parent, SceneItemType type, const char* name);
    int AppendItemWithId(const ItemSnapshot& snap, int parentIdx);
    void CaptureItemSnapshot(int idx, ItemSnapshot& out) const;
    void CaptureSelectionSnapshots(std::vector<ItemSnapshot>& out) const;
    void ResetItemData(int idx);
    void InitItemNormal(int idx, bool withModel, int defaultModelId = 0);
    void InitItemRemoveBuilding(int idx, int modelId, bool custom);

    int FindIndexById(int id) const;
    bool HasChildren(int idx) const;
    std::span<const int> Children(int parent) const;
    bool IsDescendantOf(int child, int possibleParent) const;
    bool HasSelectedAncestor(int idx) const;
    int SingleSelectedIndex() const;
    int FindNextChild(int parent, int previousOrder) const;
    void CollectDisplayOrder(int parent, int* out, int& outCount) const;

    void ClearSelection();
    int SelectionCount() const;

    void DeleteSelection(int* outRemap, int& outOldCount);

    bool CanCloneSelection() const;
    int CloneSelection(int* outClones, bool insertBefore = false);

    bool CanDropSelection(int target, SceneDropMode mode) const;
    void ApplyDrop(int dropTarget, SceneDropMode dropMode);

    void UpdateWorldTransforms() noexcept;
    bool ComposesWithParent(int idx) const;
    void SetWorldTransform(int idx, const CVector& pos, const float eulerDeg[3]);

    private:
    void EnsureCapacity(int required);
    std::unordered_map<int, int> m_indexById;
    struct HierarchyNode
    {
        int begin = 0;
        int end = 0;
        int displayBegin = 0;
        int displayEnd = 0;
        int depth = 0;
    };
    void EnsureHierarchy() const;
    mutable bool m_hierarchyDirty = true;
    mutable std::vector<HierarchyNode> m_hierarchy;
    mutable std::vector<int> m_siblingOrder;
    mutable std::vector<int> m_displayOrder;
    mutable HierarchyNode m_root;
};

extern EditorScene* g_scene;
