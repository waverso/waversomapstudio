// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "CAnimBlendStaticAssociation.h"
#include "CAnimManager.h"
#include "CModelInfo.h"
#include "CPedModelInfo.h"
#include "CStreaming.h"
#include "common.h"
#include "skin_catalog.h"

namespace SkinInstance
{
    inline CPedModelInfo* Model(int modelId)
    {
        if (!SkinCatalog::IsValid(modelId) || !CModelInfo::ms_modelInfoPtrs)
        {
            return nullptr;
        }
        auto* info = CModelInfo::GetModelInfo(modelId);
        return info && info->GetModelType() == MODEL_INFO_PED ? static_cast<CPedModelInfo*>(info) : nullptr;
    }

    inline CAnimBlendAssocGroup* AnimationGroups()
    {
        return *reinterpret_cast<CAnimBlendAssocGroup**>(0xB4EA34);
    }

    inline bool AnimationReady(int modelId)
    {
        auto* info = Model(modelId);
        auto* groups = AnimationGroups();
        if (!info || !groups || info->m_nAnimType < 0 || info->m_nAnimType >= CAnimManager::ms_numAnimAssocDefinitions)
        {
            return false;
        }
        const auto& group = groups[info->m_nAnimType];
        const int idleIndex = 3 - group.m_nIdOffset;
        return group.m_pAnimBlock && group.m_pAnimBlock->bLoaded && group.m_pAssociations && idleIndex >= 0
            && static_cast<unsigned>(idleIndex) < group.m_nNumAnimations && group.m_pAssociations[idleIndex].m_pHeirarchy;
    }

    inline bool Request(int modelId, int flags)
    {
        auto* info = Model(modelId);
        if (!info)
        {
            return false;
        }
        if (modelId >= 290 && modelId <= 299 && CStreaming::ms_aInfoForModel[modelId].m_nLoadState == LOADSTATE_NOT_LOADED)
        {
            CStreaming::RequestSpecialChar(modelId - 290, SkinCatalog::Get(modelId)->modelName, flags);
        }
        else
        {
            CStreaming::RequestModel(modelId, flags);
        }
        if (!AnimationReady(modelId) && AnimationGroups() && info->m_nAnimType >= 0
            && info->m_nAnimType < CAnimManager::ms_numAnimAssocDefinitions)
        {
            const char* name = CAnimManager::GetAnimBlockName(info->m_nAnimType);
            const int block = name ? CAnimManager::GetAnimationBlockIndex(name) : -1;
            if (block >= 0)
            {
                CStreaming::RequestModel(25575 + block, flags);
            }
        }
        return true;
    }

    inline void UpdateHierarchy(RpClump* clump, int modelId)
    {
        if (!clump)
        {
            return;
        }
        if (auto* frame = RpClumpGetFrame(clump))
        {
            RwFrameUpdateObjects(frame);
        }
        auto* hierarchy = GetAnimHierarchyFromSkinClump(clump);
        if (!hierarchy)
        {
            return;
        }
        RpHAnimHierarchyUpdateMatrices(hierarchy);
        if (modelId != 0 || !hierarchy->pMatrixArray)
        {
            return;
        }
        for (int bone : { 21, 22, 31, 32, 301, 302 })
        {
            const int index = RpHAnimIDGetIndex(hierarchy, bone);
            if (index < 0 || index >= hierarchy->numNodes)
            {
                return;
            }
        }
        plugin::Call<0x5DF560>(clump);
    }

    inline bool Idle(RpClump* clump, int modelId)
    {
        if (!clump || !AnimationReady(modelId) || !GetAnimHierarchyFromSkinClump(clump))
        {
            return false;
        }
        if (!RpAnimBlendClumpIsInitialized(clump))
        {
            RpAnimBlendClumpInit(clump);
        }
        RpAnimBlendClumpRemoveAllAssociations(clump);
        auto* association = CAnimManager::AddAnimation(clump, Model(modelId)->m_nAnimType, 3);
        if (!association)
        {
            return false;
        }
        association->SetBlend(1.0f, 0.0f);
        association->SetCurrentTime(0.0f);
        association->m_bPlaying = true;
        association->m_bLooped = true;
        association->m_bFreezeTranslation = true;
        RpAnimBlendClumpUpdateAnimations(clump, 0.0f, true);
        UpdateHierarchy(clump, modelId);
        return true;
    }

    inline RpClump* Create(int modelId)
    {
        auto* info = Model(modelId);
        if (!info || !info->m_pRwClump || !AnimationReady(modelId))
        {
            return nullptr;
        }
        auto* clump = reinterpret_cast<RpClump*>(info->CreateInstance());
        if (clump && !Idle(clump, modelId))
        {
            RpClumpDestroy(clump);
            clump = nullptr;
        }
        return clump;
    }

    inline RwV3d Transform(const RwMatrix& matrix, const RwV3d& point)
    {
        return { matrix.right.x * point.x + matrix.up.x * point.y + matrix.at.x * point.z + matrix.pos.x,
            matrix.right.y * point.x + matrix.up.y * point.y + matrix.at.y * point.z + matrix.pos.y,
            matrix.right.z * point.x + matrix.up.z * point.y + matrix.at.z * point.z + matrix.pos.z };
    }

    template <class Vertex>
    inline bool Vertices(RpAtomic* atomic, std::vector<Vertex>& vertices)
    {
        auto* geometry = atomic ? RpAtomicGetGeometry(atomic) : nullptr;
        auto* morph = geometry && geometry->numMorphTargets ? &geometry->morphTarget[0] : nullptr;
        if (!morph || !morph->verts || geometry->numVertices <= 0)
        {
            return false;
        }
        auto* frame = RpAtomicGetFrame(atomic);
        const auto* ltm = frame ? RwFrameGetLTM(frame) : nullptr;
        auto* skin = RpSkinGeometryGetSkin(geometry);
        auto* hierarchy = skin ? RpSkinAtomicGetHAnimHierarchy(atomic) : nullptr;
        const auto* weights = skin ? RpSkinGetVertexBoneWeights(skin) : nullptr;
        const auto* indices = skin ? RpSkinGetVertexBoneIndices(skin) : nullptr;
        const auto* bind = skin ? RpSkinGetSkinToBoneMatrices(skin) : nullptr;
        const unsigned bones = skin ? RpSkinGetNumBones(skin) : 0;
        const bool skinned = hierarchy && hierarchy->pMatrixArray && weights && indices && bind;
        if (skin && (!skinned || !bones || hierarchy->numNodes <= 0))
        {
            return false;
        }
        vertices.resize(geometry->numVertices);
        for (int vertex = 0; vertex < geometry->numVertices; ++vertex)
        {
            const auto& source = morph->verts[vertex];
            RwV3d point {};
            if (skinned)
            {
                const float* weight = &weights[vertex].w0;
                for (int slot = 0; slot < 4; ++slot)
                {
                    const unsigned bone = (indices[vertex] >> (slot * 8)) & 0xFF;
                    if (weight[slot] <= 0 || bone >= bones || bone >= unsigned(hierarchy->numNodes))
                    {
                        continue;
                    }
                    const auto transformed = Transform(hierarchy->pMatrixArray[bone], Transform(bind[bone], source));
                    point.x += transformed.x * weight[slot];
                    point.y += transformed.y * weight[slot];
                    point.z += transformed.z * weight[slot];
                }
                if (ltm && (hierarchy->flags & rpHANIMHIERARCHYLOCALSPACEMATRICES))
                {
                    point = Transform(*ltm, point);
                }
            }
            else
            {
                point = ltm ? Transform(*ltm, source) : source;
            }
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
            {
                return false;
            }
            vertices[vertex] = { point.x, point.y, point.z };
        }
        return true;
    }

    struct Bounds
    {
        RwV3d minimum {};
        RwV3d maximum {};
        bool valid = false;
    };

    inline Bounds Measure(RpClump* clump)
    {
        Bounds bounds;
        if (!clump)
        {
            return bounds;
        }
        RpClumpForAllAtomics(
            clump,
            [](RpAtomic* atomic, void* data) -> RpAtomic*
            {
                if (!(RpAtomicGetFlags(atomic) & rpATOMICRENDER))
                {
                    return atomic;
                }
                std::vector<RwV3d> vertices;
                if (!Vertices(atomic, vertices))
                {
                    return atomic;
                }
                auto& bounds = *static_cast<Bounds*>(data);
                for (const auto& point : vertices)
                {
                    if (!bounds.valid)
                    {
                        bounds.minimum = bounds.maximum = point;
                        bounds.valid = true;
                    }
                    else
                    {
                        bounds.minimum.x = std::min(bounds.minimum.x, point.x);
                        bounds.minimum.y = std::min(bounds.minimum.y, point.y);
                        bounds.minimum.z = std::min(bounds.minimum.z, point.z);
                        bounds.maximum.x = std::max(bounds.maximum.x, point.x);
                        bounds.maximum.y = std::max(bounds.maximum.y, point.y);
                        bounds.maximum.z = std::max(bounds.maximum.z, point.z);
                    }
                }
                return atomic;
            },
            &bounds);
        return bounds;
    }

    inline Bounds ReferenceBounds(int modelId)
    {
        struct Cache
        {
            RpClump* source = nullptr;
            Bounds bounds;
        };
        static std::array<Cache, SkinCatalog::LastId + 1> cache;
        auto* info = Model(modelId);
        if (!info || !info->m_pRwClump)
        {
            return {};
        }
        auto& entry = cache[modelId];
        if (entry.source != info->m_pRwClump || !entry.bounds.valid)
        {
            auto* clump = Create(modelId);
            if (!clump)
            {
                return {};
            }
            entry.bounds = Measure(clump);
            entry.source = info->m_pRwClump;
            RpClumpDestroy(clump);
        }
        return entry.bounds;
    }
}
