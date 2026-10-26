// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cfloat>

#include "../shared/model_checks.h"
#include "../utils/utils.h"
#include "CGame.h"
#include "CModelInfo.h"
#include "CObject.h"
#include "CPools.h"
#include "../actors/actor_runtime.h"
#include "../hooks/game_hooks.h"
#include "../objects/removal_manager.h"
#include "../vehicles/vehicle_runtime.h"

class EntityRay
{
    public:
    struct RayResult
    {
        CEntity* pEntity = nullptr;
        float fHitDistance = FLT_MAX;
        CVector vecHitPos = {};
        CVector vecHitNormal = {};
        explicit operator bool() const
        {
            return pEntity != nullptr;
        }
    };

    enum LodCastMode
    {
        IgnoreHidden,
        IgnoreAll,
        CastAll
    };
    enum CullMode
    {
        ScreenCulls,
        GeometricOnly,
        GeometricFrontFaces
    };

    static RayResult Cast(const CVector& origin, const CVector& dir, float maxDistance = 500.0f,
        LodCastMode castType = IgnoreHidden, CullMode cullMode = ScreenCulls, bool (*skipEntity)(CEntity*) = nullptr)
    {
        RayResult result;
        CastFromPool(CPools::ms_pBuildingPool, origin, dir, maxDistance, result, true, castType, cullMode, skipEntity);
        CastFromPool(CPools::ms_pObjectPool, origin, dir, maxDistance, result, false, castType, cullMode, skipEntity);
        CastFromPool(CPools::ms_pDummyPool, origin, dir, maxDistance, result, false, castType, cullMode, skipEntity);
        CastFromPool(CPools::ms_pVehiclePool, origin, dir, maxDistance, result, false, castType, cullMode, skipEntity);
        CastFromPool(CPools::ms_pPedPool, origin, dir, maxDistance, result, false, castType, cullMode, skipEntity);
        return result;
    }

    static bool IsLowLOD(CEntity* ent)
    {
        if (!ent || ent->m_nModelIndex < 0 || ent->m_nModelIndex >= 20000)
        {
            return false;
        }
        CBaseModelInfo* mi = CModelInfo::GetModelInfo(ent->m_nModelIndex);
        return mi && !mi->bIsLod && ent->bIsBIGBuilding;
    }

    static bool IsObjectInWorld(CObject* object)
    {
        return object && (object->bIsBIGBuilding || object->m_pCollisionList != nullptr);
    }

    static bool IsLowLodHidden(CEntity* entity)
    {
        if (!g_gameHooks)
        {
            return false;
        }
        auto it = g_gameHooks->m_lodHighMap.find(entity);
        if (it == g_gameHooks->m_lodHighMap.end() || it->second.empty())
        {
            return false;
        }
        for (CEntity* hd : it->second)
        {
            if (!hd || hd->bDistanceFade || (g_removalManager && g_removalManager->IsRemoved(hd)))
            {
                return false;
            }
        }
        return true;
    }

    private:
    struct AtomicRayContext
    {
        CVector origin, dir;
        float maxD, bestT;
        bool hit, cull;
        CVector bestNormal = {};
    };

    static RpAtomic* CB_AtomicRay(RpAtomic* atomic, void* data)
    {
        AtomicRayContext* ctx = (AtomicRayContext*)data;
        if (ModelChecks::IsGunflashAtomic(atomic))
        {
            return atomic;
        }
        RpGeometry* geom = RpAtomicGetGeometry(atomic);
        if (!geom || geom->numTriangles == 0)
        {
            return atomic;
        }
        RpMorphTarget* morph = &geom->morphTarget[0];
        if (!morph || !morph->verts)
        {
            return atomic;
        }

        RwFrame* frame = RpAtomicGetFrame(atomic);
        const RwMatrix* ltm = frame ? RwFrameGetLTM(frame) : nullptr;
        if (ltm)
        {
            float sc = std::max({ Utils::RwLen(ltm->right), Utils::RwLen(ltm->up), Utils::RwLen(ltm->at) });
            const RwSphere& bs = morph->boundingSphere;
            if (!Utils::IntersectsRaySphere(ctx->origin, ctx->dir,
                    Utils::ToWorld(*ltm, { bs.center.x, bs.center.y, bs.center.z }), bs.radius * sc + 0.1f, ctx->maxD))
            {
                return atomic;
            }
        }

        const RwV3d* verts = morph->verts;
        const RpTriangle* tris = geom->triangles;
        for (int i = 0; i < geom->numTriangles; ++i)
        {
            const RpTriangle& tri = tris[i];
            CVector w0, w1, w2;
            if (ltm)
            {
                w0 = Utils::ToWorld(*ltm, verts[tri.vertIndex[0]]);
                w1 = Utils::ToWorld(*ltm, verts[tri.vertIndex[1]]);
                w2 = Utils::ToWorld(*ltm, verts[tri.vertIndex[2]]);
            }
            else
            {
                RwV3d p0 = verts[tri.vertIndex[0]];
                w0 = { p0.x, p0.y, p0.z };
                RwV3d p1 = verts[tri.vertIndex[1]];
                w1 = { p1.x, p1.y, p1.z };
                RwV3d p2 = verts[tri.vertIndex[2]];
                w2 = { p2.x, p2.y, p2.z };
            }
            float t;
            if (Utils::IntersectRayTriangle(ctx->origin, ctx->dir, w0, w1, w2, ctx->cull, t) && t <= ctx->maxD && t < ctx->bestT)
            {
                ctx->bestT = t;
                ctx->hit = true;

                CVector n = Utils::CrossProduct(w1 - w0, w2 - w0);
                const float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
                if (len > 1e-8f)
                {
                    n.x /= len;
                    n.y /= len;
                    n.z /= len;
                    if (Utils::Dot3(n, ctx->dir) > 0.0f)
                    {
                        n.x = -n.x;
                        n.y = -n.y;
                        n.z = -n.z;
                    }
                    ctx->bestNormal = n;
                }
            }
        }
        return atomic;
    }

    static float TestEntityRay(CEntity* ent, const CVector& origin, const CVector& dir, float maxD, CVector* outNormal = nullptr,
        bool frontFacesOnly = false)
    {
        if (ent->m_nType == ENTITY_TYPE_PED)
        {
            const auto* meshes = g_actorRuntime ? g_actorRuntime->Meshes(ent) : nullptr;
            float best = FLT_MAX;
            if (!meshes)
            {
                return best;
            }
            for (const auto& mesh : *meshes)
            {
                for (size_t index = 0; index + 2 < mesh.indices.size(); index += 3)
                {
                    const auto& a = mesh.vertices[mesh.indices[index]];
                    const auto& b = mesh.vertices[mesh.indices[index + 1]];
                    const auto& c = mesh.vertices[mesh.indices[index + 2]];
                    float distance;
                    if (!Utils::IntersectRayTriangle(origin, dir, a, b, c, frontFacesOnly, distance) || distance < 0
                        || distance >= std::min(best, maxD))
                    {
                        continue;
                    }
                    best = distance;
                    if (outNormal)
                    {
                        CVector normal = Utils::CrossProduct(b - a, c - a);
                        const float length = normal.Magnitude();
                        if (length > 1e-8f)
                        {
                            normal *= 1.0f / length;
                            if (Utils::Dot3(normal, dir) > 0)
                            {
                                normal *= -1.0f;
                            }
                        }
                        *outNormal = normal;
                    }
                }
            }
            return best;
        }
        if (!ent->m_pRwObject)
        {
            return FLT_MAX;
        }
        AtomicRayContext ctx { origin, dir, maxD, FLT_MAX, false, frontFacesOnly || ent->bBackfaceCulled };
        const RwUInt8 type = ent->m_pRwObject->type;
        if (type == rpCLUMP)
        {
            RpClumpForAllAtomics(ent->m_pRwClump, CB_AtomicRay, &ctx);
        }
        else if (type == rpATOMIC)
        {
            CB_AtomicRay(ent->m_pRwAtomic, &ctx);
        }
        if (ctx.hit && outNormal)
        {
            *outNormal = ctx.bestNormal;
        }
        return ctx.hit ? ctx.bestT : FLT_MAX;
    }

    template <typename TPool>
    static void CastFromPool(TPool* pool, const CVector& origin, const CVector& dir, float maxDistance, RayResult& best,
        bool isBuildingPool = false, LodCastMode castType = IgnoreHidden, CullMode cullMode = ScreenCulls,
        bool (*skipEntity)(CEntity*) = nullptr)
    {
        if (!pool)
        {
            return;
        }
        const int area = CGame::currArea;
        const int numPasses = isBuildingPool && castType == CastAll ? 2 : 1;

        for (int pass = 0; pass < numPasses; ++pass)
        {
            if (pass == 1)
            {
                if (!best.pEntity)
                {
                    break;
                }
                if (!IsLowLOD(best.pEntity))
                {
                    break;
                }
                if (best.pEntity->m_nNumLodChildren == 0)
                {
                    break;
                }
            }
            CEntity* firstPassLodEnt = (pass == 1) ? best.pEntity : nullptr;

            for (int i = 0; i < pool->m_nSize; ++i)
            {
                if (pool->m_byteMap[i].bEmpty)
                {
                    continue;
                }
                CEntity* pEnt = (CEntity*)(pool->GetAt(i));
                if (!pEnt)
                {
                    continue;
                }
                if (pEnt->m_nType == ENTITY_TYPE_VEHICLE && (!g_vehicleRuntime || g_vehicleRuntime->LookupItemIndex(pEnt) < 0))
                {
                    continue;
                }
                if (pEnt->m_nType == ENTITY_TYPE_PED && (!g_actorRuntime || g_actorRuntime->LookupItemIndex(pEnt) < 0))
                {
                    continue;
                }
                if (pEnt->m_nType == ENTITY_TYPE_OBJECT && !IsObjectInWorld((CObject*)pEnt))
                {
                    continue;
                }
                if (!pEnt->m_pRwObject)
                {
                    continue;
                }

                if (pEnt->m_nModelIndex < 0 || pEnt->m_nModelIndex >= 20000)
                {
                    continue;
                }
                if (!g_renderInteriors && area != pEnt->m_nAreaCode && pEnt->m_nAreaCode != 13)
                {
                    continue;
                }
                if (!pEnt->IsVisible())
                {
                    continue;
                }
                if (ModelChecks::IsEscalatorStepModel(pEnt->m_nModelIndex))
                {
                    continue;
                }
                if (skipEntity && skipEntity(pEnt))
                {
                    continue;
                }

                bool isLowLod = IsLowLOD(pEnt);
                if (isLowLod)
                {
                    if (castType == IgnoreAll)
                    {
                        continue;
                    }
                    else if (castType == IgnoreHidden && pEnt->m_nNumLodChildren > 0 && IsLowLodHidden(pEnt))
                    {
                        continue;
                    }
                }

                const bool interiorEnt = pEnt->m_nAreaCode != 0 && pEnt->m_nAreaCode != 13;
                const bool relaxedCulls = cullMode != ScreenCulls || (g_renderInteriors && interiorEnt);

                if (pEnt->m_nType != ENTITY_TYPE_PED
                    && !Utils::IntersectsRaySphere(
                        origin, dir, pEnt->GetBoundCentre(), pEnt->GetBoundRadius() + 0.5f, maxDistance))
                {
                    continue;
                }

                if (cullMode == ScreenCulls)
                {
                    if (!pEnt->GetIsOnScreen())
                    {
                        continue;
                    }
                    if (!pEnt->GetIsBoundingBoxOnScreen())
                    {
                        continue;
                    }
                    if (!relaxedCulls && pEnt->IsEntityOccluded())
                    {
                        continue;
                    }
                }

                if (pass == 0)
                {
                    CVector normal;
                    float dist = TestEntityRay(pEnt, origin, dir, maxDistance, &normal, cullMode == GeometricFrontFaces);
                    if (dist < best.fHitDistance)
                    {
                        best.fHitDistance = dist;
                        best.vecHitPos = origin + dir * dist;
                        best.vecHitNormal = normal;
                        best.pEntity = pEnt;
                    }
                }
                else
                {
                    if (isLowLod)
                    {
                        continue;
                    }
                    if (pEnt->m_pLod == firstPassLodEnt)
                    {
                        CVector normal;
                        float dist = TestEntityRay(pEnt, origin, dir, maxDistance, &normal, cullMode == GeometricFrontFaces);
                        if (dist < best.fHitDistance)
                        {
                            best.fHitDistance = dist;
                            best.vecHitPos = origin + dir * dist;
                            best.vecHitNormal = normal;
                        }
                        best.pEntity = pEnt;
                        break;
                    }
                }
            }
        }
    }
};
