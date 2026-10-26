// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <unordered_map>
#include <vector>

#include "CEntity.h"
#include "CVector.h"

struct RemovalInstruction
{
    int nInstId = -1;
    int nModelId = -1;
    CVector posXYZ = {};
    float fRadius = 0.25f;
};

struct RemovedEntity
{
    short nModelId = -1;
    eEntityType nType = ENTITY_TYPE_NOTHING;
    unsigned char nAreaCode = (unsigned char)-1;
    CVector posPivot = {};
    float fPosZOriginal = 0.0f;
    bool bHadCollision = true;
    bool bWasVisible = true;

    int nInstId = -1;

    CEntity* lastEntity = nullptr;
};

class RemovalManager
{
    public:
    int AddInstruction(int modelId, const CVector& pos, float radius = 0.25f);
    void RemoveInstruction(int instId);
    void OnEntityAdd(CEntity* entity);
    void OnEntityRemove(CEntity* entity);
    void OnControlLogicProcess(CEntity* entity);
    bool IsRemoved(CEntity* entity) const;

    CVector OriginOf(CEntity* ent) const;

    private:
    int m_nextInstanceId = 0;
    std::vector<RemovalInstruction> m_removeInstructions;
    std::vector<RemovedEntity> m_removedEntities;

    struct OccluderSnapshot
    {
        short midX, midY, midZ;
        short widthX, widthY, height;
    };

    std::unordered_map<int, OccluderSnapshot> m_occluderSnapshots;
    std::unordered_map<int, std::vector<int>> m_instanceOccluderMap;
    std::unordered_map<int, int> m_occluderRefCount;

    std::unordered_map<CEntity*, CVector> m_doorOrigin;

    bool MatchesInstruction(CEntity* ent, const RemovalInstruction& inst) const;
    bool MatchesRemovedEntity(CEntity* ent, const RemovedEntity& re) const;
    bool IsAlreadyRemoved(CEntity* ent, int instId) const;
    void ApplyRemoval(CEntity* entity, int instId);
    void UndoRemoval(RemovedEntity& re);
    void HideRemoved(CEntity* entity, const RemovedEntity& re);
    void ScanPoolsForInstruction(int instId);

    void RemoveOccluders(int instId, float fPosX, float fPosY, float fPosZ, float fRadius);
    void RestoreOccluders(int instId);

    template <typename TPool>
    void ScanPool(TPool* pool, int instId);
};

extern RemovalManager* g_removalManager;
