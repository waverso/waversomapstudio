// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include "CEntity.h"
#include "../rendering/material_text_cache.h"
#include "../scene/streaming_budget.h"
#include "../scene/scene.h"

class CObject;
struct RwTexture;

bool IsBreakableModel(unsigned short modelId);

enum class ObjectStreamState
{
    Unspawned = 0,
    WantSpawn,
    Loading,
    Spawned
};

struct MaterialTextureState
{
    RwTexture* tex = nullptr;
    int textureModelId = -1;
    char txdName[24] = {};
    char textureName[128] = {};
    ULONGLONG retryAtMs = 0;
};

struct MaterialTextState
{
    RwTexture* tex = nullptr;
    MaterialTextCache::Entry* entry = nullptr;
    ULONGLONG retryAtMs = 0;
};

struct MaterialOrderState
{
    unsigned char entries[ItemData::MaxMaterials * 2] = {};
    int materialCount = -1;
    int textCount = -1;

    int Update(const ItemData& data);
};

struct SlideRuntimeState
{
    bool playing = false;
    CVector curPos = CVector(0.0f, 0.0f, 0.0f);
    float curRot[3] = { 0.0f, 0.0f, 0.0f };
};

struct RemovalRuntimeState
{
    int instanceId = -1;
    int issuedModelId = -1;
    float issuedPosition[3] = { 0.0f, 0.0f, 0.0f };
    float issuedRadius = -1.0f;

    int pendingModelId = -1;
    float pendingPosition[3] = { 0.0f, 0.0f, 0.0f };
    float pendingRadius = -1.0f;
    ULONGLONG stableAtMs = 0;
};

struct ObjectRuntimeState
{
    ObjectStreamState state = ObjectStreamState::Unspawned;
    CObject* obj = nullptr;
    int scriptHandle = -1;
    int poolSlot = -1;
    int spawnedModelId = -1;
    int loadingModelId = -1;
    float boundRadius = 0.0f;
    ULONGLONG retryAtMs = 0;
    float lastPos[3] = { 0.0f, 0.0f, 0.0f };
    float lastRot[3] = { 0.0f, 0.0f, 0.0f };
    bool escalatorDirty = false;
    bool materialsActive = false;

    MaterialTextureState mats[16];
    MaterialTextState matTexts[16];
    MaterialOrderState materialOrder;
    SlideRuntimeState slide;
    RemovalRuntimeState rb;
};

class ObjectRuntime
{
    public:
    ObjectRuntime();
    ~ObjectRuntime();

    void Update();
    int PreviewCapacity() const
    {
        return m_previewCapacity;
    }

    inline int LookupItemIndex(CEntity* ent) const
    {
        if (!ent || ent->m_nType != ENTITY_TYPE_OBJECT)
        {
            return -1;
        }
        const uintptr_t off = (uintptr_t)ent - m_poolBase;
        if (off >= m_poolSpan)
        {
            return -1;
        }
        const int slot = (int)(off / m_poolStride);
        return (m_slotOwner[slot] == (CObject*)ent) ? m_slotToItem[slot] : -1;
    }

    bool IsSpawning() const
    {
        return m_spawning;
    }

    void OnEntityRemove(CEntity* ent);
    void OnSceneRemap(const int* remap, int oldCount);

    void DespawnItem(int idx);
    ObjectRuntimeState& Runtime(int idx)
    {
        return m_itemStates[idx];
    }

    RwTexture* ResolveTexture(int texModelId, const char* txdLib, const char* texName);
    void ReleaseTexture(RwTexture* tex);

    void ToggleSlide(int idx);
    void StopSlide(int idx);

    private:
    bool SpawnNow(int idx);
    bool RequestModel(int idx, int modelId);
    void ReleaseModelRequest(int idx);
    void ClearSlot(int slot);
    void PushTransformIfChanged(int idx);
    void UpdateSlides(float dtSeconds);
    void SyncItemTextures(int idx);
    void ReleaseItemTextures(int idx);
    void SyncItemMaterialTexts(int idx);
    void ReleaseItemMaterialTexts(int idx);
    RwTexture* BuildMaterialTextTexture(const MaterialTextData& d);
    void SyncRemoveBuilding(int idx, ULONGLONG now);
    void RevokeRemoveBuilding(int idx);

    ObjectRuntimeState m_itemStates[EditorScene::MaxItems];
    MaterialTextCache m_textCache;
    StreamingBudget::Selection<EditorScene::MaxItems> m_preview;
    int m_previewCapacity = 0;
    struct ModelRequest
    {
        int modelId = -1;
        int users = 0;
        bool owned = false;
    };
    static constexpr int MaxLoadingRequests = 8;
    ModelRequest m_modelRequests[MaxLoadingRequests];
    int* m_slotToItem = nullptr;
    CObject** m_slotOwner = nullptr;
    int m_slotCount = 0;
    uintptr_t m_poolBase = 0;
    uintptr_t m_poolStride = 0;
    uintptr_t m_poolSpan = 0;

    int m_knownCount = 0;
    int m_blankTxdSlot = -1;
    bool m_despawning = false;
    bool m_spawning = false;
};

extern ObjectRuntime* g_objectRuntime;
