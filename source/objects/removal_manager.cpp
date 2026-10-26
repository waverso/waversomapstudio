// Source Repository: https://github.com/waverso/waversomapstudio

#include "removal_manager.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "../shared/model_checks.h"
#include "../scene/scene_limits.h"
#include "CDummyObject.h"
#include "CEntity.h"
#include "CObject.h"
#include "CPools.h"
#include "object_runtime.h"
#include "../pickups/pickup_runtime.h"
#include "../hooks/samp.h"

#pragma pack(push, 1)
struct GameOccluder
{
    short midX, midY, midZ;
    short widthX, widthY, height;
    char rotX, rotY, rotZ;
    char pad;
    unsigned short nFlags;
};
#pragma pack(pop)

static constexpr int MaxMapOccluders = 1000;
static size_t* const pNumOccluders = (size_t*)0xC73F98;
static GameOccluder* const aOccluders = (GameOccluder*)0xC73FA0;

void RemovalManager::RemoveOccluders(int instId, float fPosX, float fPosY, float fPosZ, float fRadius)
{
    if (*pNumOccluders == 0)
    {
        return;
    }

    const float fSqRadius = fRadius * fRadius;
    std::vector<int>& instIndices = m_instanceOccluderMap[instId];

    const int count = static_cast<int>(*pNumOccluders);
    const int limit = (count < MaxMapOccluders) ? count : MaxMapOccluders;

    for (int i = 0; i < limit; ++i)
    {
        float occX, occY, occZ;
        auto snapIt = m_occluderSnapshots.find(i);
        if (snapIt != m_occluderSnapshots.end())
        {
            occX = static_cast<float>(snapIt->second.midX) * 0.25f;
            occY = static_cast<float>(snapIt->second.midY) * 0.25f;
            occZ = static_cast<float>(snapIt->second.midZ) * 0.25f;
        }
        else
        {
            GameOccluder& occ = aOccluders[i];
            occX = static_cast<float>(occ.midX) * 0.25f;
            occY = static_cast<float>(occ.midY) * 0.25f;
            occZ = static_cast<float>(occ.midZ) * 0.25f;
        }

        float dx = occX - fPosX;
        float dy = occY - fPosY;
        float dz = occZ - fPosZ;
        if ((dx * dx + dy * dy + dz * dz) > fSqRadius)
        {
            continue;
        }

        bool alreadyInThisInst = false;
        for (int idx : instIndices)
        {
            if (idx == i)
            {
                alreadyInThisInst = true;
                break;
            }
        }
        if (alreadyInThisInst)
        {
            continue;
        }

        if (snapIt == m_occluderSnapshots.end())
        {
            GameOccluder& occ = aOccluders[i];

            OccluderSnapshot snap;
            snap.midX = occ.midX, snap.midY = occ.midY, snap.midZ = occ.midZ;
            snap.widthX = occ.widthX, snap.widthY = occ.widthY, snap.height = occ.height;
            m_occluderSnapshots[i] = snap;

            occ.midX = occ.midY = occ.midZ = 0;
            occ.widthX = occ.widthY = occ.height = 0;
        }

        instIndices.push_back(i);
        m_occluderRefCount[i]++;
    }
}

void RemovalManager::RestoreOccluders(int instId)
{
    auto mapIt = m_instanceOccluderMap.find(instId);
    if (mapIt == m_instanceOccluderMap.end())
    {
        return;
    }

    for (int i : mapIt->second)
    {
        if (i < 0 || i >= MaxMapOccluders)
        {
            continue;
        }

        auto refIt = m_occluderRefCount.find(i);
        if (refIt == m_occluderRefCount.end())
        {
            continue;
        }

        refIt->second--;

        if (refIt->second <= 0)
        {
            auto snapIt = m_occluderSnapshots.find(i);
            if (snapIt != m_occluderSnapshots.end())
            {
                GameOccluder& occ = aOccluders[i];
                const OccluderSnapshot& s = snapIt->second;

                occ.midX = s.midX;
                occ.midY = s.midY;
                occ.midZ = s.midZ;
                occ.widthX = s.widthX;
                occ.widthY = s.widthY;
                occ.height = s.height;

                m_occluderSnapshots.erase(snapIt);
            }
            m_occluderRefCount.erase(refIt);
        }
    }

    m_instanceOccluderMap.erase(mapIt);
}

int RemovalManager::AddInstruction(int modelId, const CVector& pos, float radius)
{
    RemovalInstruction inst;
    inst.nInstId = m_nextInstanceId++;
    inst.nModelId = modelId;
    inst.posXYZ = pos;
    inst.fRadius = radius;
    m_removeInstructions.push_back(inst);

    RemoveOccluders(inst.nInstId, pos.x, pos.y, pos.z, std::max(250.0f, radius + 80.0f));

    ScanPoolsForInstruction(inst.nInstId);
    return inst.nInstId;
}

void RemovalManager::RemoveInstruction(int instId)
{
    std::vector<CEntity*> restored;
    for (auto& re : m_removedEntities)
    {
        if (re.nInstId != instId)
        {
            continue;
        }
        if (re.lastEntity)
        {
            restored.push_back(re.lastEntity);
        }
        UndoRemoval(re);
    }
    m_removedEntities.erase(std::remove_if(m_removedEntities.begin(), m_removedEntities.end(),
                                [instId](const RemovedEntity& re)
                                {
                                    return re.nInstId == instId;
                                }),
        m_removedEntities.end());
    m_removeInstructions.erase(std::remove_if(m_removeInstructions.begin(), m_removeInstructions.end(),
                                   [instId](const RemovalInstruction& i)
                                   {
                                       return i.nInstId == instId;
                                   }),
        m_removeInstructions.end());
    RestoreOccluders(instId);

    for (CEntity* ent : restored)
    {
        if (IsRemoved(ent))
        {
            continue;
        }
        for (const auto& inst : m_removeInstructions)
        {
            if (!MatchesInstruction(ent, inst))
            {
                continue;
            }
            ApplyRemoval(ent, inst.nInstId);
            break;
        }
    }
}

CVector RemovalManager::OriginOf(CEntity* ent) const
{
    if (!ent)
    {
        return CVector(0.0f, 0.0f, 0.0f);
    }
    if (ent->m_nType == ENTITY_TYPE_OBJECT)
    {
        auto it = m_doorOrigin.find(ent);
        if (it != m_doorOrigin.end())
        {
            return it->second;
        }
        CDummyObject* d = static_cast<CObject*>(ent)->m_pDummyObject;
        if (d)
        {
            return d->m_placement.m_vPosn.IsZero() ? d->GetPosition() : d->m_placement.m_vPosn;
        }
    }
    return ent->GetPosition();
}

bool RemovalManager::MatchesInstruction(CEntity* ent, const RemovalInstruction& inst) const
{
    if (!ent)
    {
        return false;
    }
    if (g_pickupRuntime && g_pickupRuntime->LookupItemIndex(ent) >= 0)
    {
        return false;
    }
    if (g_objectRuntime && g_objectRuntime->LookupItemIndex(ent) >= 0)
    {
        return false;
    }
    if (inst.nModelId != -1 && ent->m_nModelIndex != inst.nModelId)
    {
        return false;
    }

    const CVector pos = OriginOf(ent);
    float dx = pos.x - inst.posXYZ.x;
    float dy = pos.y - inst.posXYZ.y;
    float dz = pos.z - inst.posXYZ.z;
    return (dx * dx + dy * dy + dz * dz) <= (inst.fRadius * inst.fRadius);
}

bool RemovalManager::MatchesRemovedEntity(CEntity* ent, const RemovedEntity& re) const
{
    if (!ent)
    {
        return false;
    }
    if (ent->m_nModelIndex != re.nModelId)
    {
        return false;
    }
    if (ent->m_nType != re.nType && re.nType != ENTITY_TYPE_OBJECT && re.nType != ENTITY_TYPE_DUMMY)
    {
        return false;
    }
    if (ent->m_nAreaCode != re.nAreaCode)
    {
        return false;
    }

    const CVector pos = OriginOf(ent);
    float dx = pos.x - re.posPivot.x;
    float dy = pos.y - re.posPivot.y;
    float dz = pos.z - re.posPivot.z;
    return (dx * dx + dy * dy + dz * dz) <= (SceneLimits::PositionMatchEpsilon * SceneLimits::PositionMatchEpsilon);
}

bool RemovalManager::IsAlreadyRemoved(CEntity* ent, int instId) const
{
    for (const auto& re : m_removedEntities)
    {
        if (re.nInstId != instId)
        {
            continue;
        }
        if (MatchesRemovedEntity(ent, re))
        {
            return true;
        }
    }
    return false;
}

void RemovalManager::HideRemoved(CEntity* entity, const RemovedEntity& re)
{
    entity->bUsesCollision = false;

    if (ModelChecks::IsEscalatorBaseModel(re.nModelId))
    {
        entity->RemoveEscalatorsForEntity();
    }

    if (re.nType == ENTITY_TYPE_OBJECT)
    {
        entity->bIsVisible = false;
    }
    else
    {
        entity->GetPosition().z = re.fPosZOriginal - 2000.0f;
        entity->m_nAreaCode = 1;
        entity->bRemoveFromWorld = true;
    }
}

void RemovalManager::ApplyRemoval(CEntity* entity, int instId)
{
    RemovedEntity re;
    re.nModelId = static_cast<short>(entity->m_nModelIndex);
    re.nType = static_cast<eEntityType>(entity->m_nType);
    re.nAreaCode = entity->m_nAreaCode;
    re.posPivot = OriginOf(entity);
    re.fPosZOriginal = entity->GetPosition().z;
    re.bHadCollision = entity->bUsesCollision != 0;
    re.bWasVisible = entity->bIsVisible != 0;
    re.nInstId = instId;
    re.lastEntity = entity;

    HideRemoved(entity, re);

    m_removedEntities.push_back(re);
}

void RemovalManager::UndoRemoval(RemovedEntity& re)
{
    CEntity* ent = re.lastEntity;
    if (!ent)
    {
        return;
    }

    if (ModelChecks::IsEscalatorStepModel(re.nModelId))
    {
        return;
    }

    ent->bUsesCollision = re.bHadCollision;

    if (re.nType == ENTITY_TYPE_OBJECT)
    {
        ent->bIsVisible = re.bWasVisible;
    }
    else
    {
        ent->GetPosition().z = re.fPosZOriginal;
        ent->m_nAreaCode = re.nAreaCode;
        ent->bRemoveFromWorld = false;
    }

    if (ModelChecks::IsEscalatorBaseModel(re.nModelId))
    {
        ent->RemoveEscalatorsForEntity();
        ent->CreateEffects();
    }
}

template <typename TPool>
void RemovalManager::ScanPool(TPool* pool, int instId)
{
    if (!pool)
    {
        return;
    }

    const RemovalInstruction* inst = nullptr;
    for (const auto& i : m_removeInstructions)
    {
        if (i.nInstId == instId)
        {
            inst = &i;
            break;
        }
    }
    if (!inst)
    {
        return;
    }

    for (int i = 0; i < pool->m_nSize; ++i)
    {
        if (pool->m_byteMap[i].bEmpty)
        {
            continue;
        }
        CEntity* ent = reinterpret_cast<CEntity*>(pool->GetAt(i));
        if (!ent || !ent->m_pRwObject)
        {
            continue;
        }
        if (ent->m_nModelIndex < 0 || ent->m_nModelIndex >= 20000)
        {
            continue;
        }
        if (ent->m_nType == ENTITY_TYPE_OBJECT && (SampIsServerObject((CObject*)ent) || PickupIsPickupObject((CObject*)ent)))
        {
            continue;
        }

        if (!MatchesInstruction(ent, *inst))
        {
            continue;
        }
        if (IsRemoved(ent))
        {
            continue;
        }
        if (IsAlreadyRemoved(ent, instId))
        {
            continue;
        }
        ApplyRemoval(ent, instId);
    }
}

void RemovalManager::ScanPoolsForInstruction(int instId)
{
    ScanPool(CPools::ms_pBuildingPool, instId);
    ScanPool(CPools::ms_pObjectPool, instId);
    ScanPool(CPools::ms_pDummyPool, instId);
}

void RemovalManager::OnControlLogicProcess(CEntity* entity)
{
    if (!entity)
    {
        return;
    }
    if (m_doorOrigin.find(entity) != m_doorOrigin.end())
    {
        return;
    }
    CDummyObject* d = static_cast<CObject*>(entity)->m_pDummyObject;
    if (d)
    {
        m_doorOrigin[entity] = d->GetPosition();
    }
}

void RemovalManager::OnEntityAdd(CEntity* entity)
{
    if (!entity)
    {
        return;
    }
    m_doorOrigin.erase(entity);

    if (entity->m_nType == ENTITY_TYPE_DUMMY)
    {
        m_doorOrigin[entity] = entity->GetPosition();
    }
    else if (entity->m_nType == ENTITY_TYPE_OBJECT)
    {
        CDummyObject* d = static_cast<CObject*>(entity)->m_pDummyObject;
        if (d)
        {
            auto it = m_doorOrigin.find(reinterpret_cast<CEntity*>(d));
            m_doorOrigin[entity] = (it != m_doorOrigin.end()) ? it->second : d->GetPosition();
        }
    }

    if (entity->m_nType == ENTITY_TYPE_OBJECT && (SampIsServerObject((CObject*)entity) || PickupIsPickupObject((CObject*)entity)))
    {
        return;
    }

    if (g_objectRuntime && g_objectRuntime->IsSpawning())
    {
        return;
    }
    if (g_pickupRuntime && g_pickupRuntime->IsSpawning())
    {
        return;
    }

    for (auto& re : m_removedEntities)
    {
        if (!MatchesRemovedEntity(entity, re))
        {
            continue;
        }

        re.lastEntity = entity;
        HideRemoved(entity, re);
        return;
    }

    if (IsRemoved(entity))
    {
        return;
    }

    for (const auto& inst : m_removeInstructions)
    {
        if (!MatchesInstruction(entity, inst))
        {
            continue;
        }
        if (IsAlreadyRemoved(entity, inst.nInstId))
        {
            continue;
        }
        ApplyRemoval(entity, inst.nInstId);
        return;
    }
}

void RemovalManager::OnEntityRemove(CEntity* entity)
{
    if (!entity)
    {
        return;
    }
    m_doorOrigin.erase(entity);
    for (auto& re : m_removedEntities)
    {
        if (re.lastEntity == entity)
        {
            re.lastEntity = nullptr;
        }
    }
}

bool RemovalManager::IsRemoved(CEntity* entity) const
{
    if (!entity)
    {
        return false;
    }
    for (const auto& re : m_removedEntities)
    {
        if (re.lastEntity == entity)
        {
            return true;
        }
    }
    return false;
}
