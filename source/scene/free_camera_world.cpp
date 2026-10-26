// Source Repository: https://github.com/waverso/waversomapstudio

#include <cfloat>
#include <cmath>
#include <numbers>

#include "../utils/utils.h"
#include "CCam.h"
#include "CCamera.h"
#include "CColPoint.h"
#include "CEntity.h"
#include "CGame.h"
#include "CPed.h"
#include "CPhysical.h"
#include "CStreaming.h"
#include "CVehicle.h"
#include "CWorld.h"
#include "common.h"
#include "entity_raycast.h"
#include "free_camera.h"
#include "../hooks/game_hooks.h"

static int ResolveHitArea(CEntity* ent)
{
    if (!ent)
    {
        return -1;
    }
    const int area = ent->m_nAreaCode;
    return area == 13 ? 0 : area;
}

static void ApplyVisibleArea(int area, CPed* pPlayer, CVehicle* pVehicle)
{
    if (area < 0)
    {
        return;
    }
    if (CGame::currArea != area)
    {
        CGame::currArea = area;
        CStreaming::RemoveBuildingsNotInArea(area);
    }
    if (pPlayer)
    {
        pPlayer->m_nAreaCode = (unsigned char)area;
    }
    if (pVehicle)
    {
        pVehicle->m_nAreaCode = (unsigned char)area;
    }
}

void FreeCamera::UpdateGamePos()
{
    CPed* pPlayer = FindPlayerPed();
    CVehicle* pVehicle = FindPlayerVehicle(-1);
    CEntity* pTarget = pVehicle ? (CEntity*)pVehicle : (CEntity*)pPlayer;
    CPhysical* pPhysical = (CPhysical*)pTarget;

    if (m_active)
    {
        if (!m_gamePositionWasActive && CGame::currArea != 0)
        {
            g_renderInteriors = true;
            ApplyVisibleArea(0, pPlayer, pVehicle);
        }
        if (pTarget)
        {
            pTarget->SetPosn(m_position);
            pTarget->SetOrientation(0.0f, 0.0f, m_yawRadians);
            pTarget->bUsesCollision = false;
            pPhysical->m_vecMoveSpeed = { 0.0f, 0.0f, 0.0f };
            pPhysical->m_vecTurnSpeed = { 0.0f, 0.0f, 0.0f };

            if (pVehicle)
            {
                bool hideVeh
                    = !(pPlayer && (pPlayer->m_ePedState == PEDSTATE_DRIVING || pPlayer->m_ePedState == PEDSTATE_PASSENGER));
                pVehicle->bIsVisible = hideVeh;
                pVehicle->bEngineOn = hideVeh;
            }
        }
    }
    else if (m_gamePositionWasActive)
    {
        float camYaw = std::atan2(-m_cameraForward.x, m_cameraForward.y) - Utils::HalfPi;
        float normalizedYaw = (float)(std::fmod(camYaw + std::numbers::pi, Utils::TwoPi) - std::numbers::pi);
        TheCamera.m_aCams[TheCamera.m_nActiveCam].m_fVerticalAngle = m_pitchRadians;
        TheCamera.m_aCams[TheCamera.m_nActiveCam].m_fHorizontalAngle = normalizedYaw;

        if (pTarget)
        {
            if (pPlayer && !pPlayer->bInVehicle)
            {
                CColPoint hitPoint;
                CEntity* hitEnt = nullptr;
                CVector lookTarget = m_position + m_cameraForward * 180.0f;

                bool didHit = CWorld::ProcessLineOfSight(
                    m_position, lookTarget, hitPoint, hitEnt, true, false, false, true, false, false, false, false);

                float hitDist = didHit ? (hitPoint.m_vecPoint - m_position).Magnitude() : FLT_MAX;
                const EntityRay::RayResult fwdHit
                    = EntityRay::Cast(m_position, m_cameraForward, 180.0f, EntityRay::IgnoreHidden, EntityRay::GeometricOnly);
                if (fwdHit && fwdHit.fHitDistance < hitDist)
                {
                    didHit = true;
                    hitPoint.m_vecPoint = fwdHit.vecHitPos;
                    hitEnt = fwdHit.pEntity;
                }

                CVector spawnXY = didHit ? hitPoint.m_vecPoint - m_cameraForward * 0.8f : m_position;

                CEntity* groundEnt = nullptr;
                bool hasGround = false;
                float groundZ = CWorld::FindGroundZFor3DCoord(spawnXY.x, spawnXY.y, spawnXY.z + 1.0f, &hasGround, &groundEnt);

                const CVector probeStart(spawnXY.x, spawnXY.y, spawnXY.z + 1.0f);
                const EntityRay::RayResult downHit = EntityRay::Cast(
                    probeStart, CVector(0.0f, 0.0f, -1.0f), 300.0f, EntityRay::IgnoreHidden, EntityRay::GeometricOnly);
                if (downHit && (!hasGround || downHit.vecHitPos.z > groundZ))
                {
                    groundZ = downHit.vecHitPos.z;
                    groundEnt = downHit.pEntity;
                    hasGround = true;
                }
                if (!hasGround)
                {
                    groundZ = spawnXY.z;
                    groundEnt = nullptr;
                }

                pPlayer->SetPosn(spawnXY.x, spawnXY.y, groundZ + 1.0f);
                pPlayer->m_fCurrentRotation = m_yawRadians;
                pPlayer->m_fAimingRotation = m_yawRadians;
                pPlayer->m_fLookDirection = m_yawRadians;
                Utils::DisembarkInstantly();

                int area = ResolveHitArea(groundEnt);
                if (area < 0)
                {
                    area = ResolveHitArea(hitEnt);
                }
                ApplyVisibleArea(area, pPlayer, nullptr);
            }
            else
            {
                const EntityRay::RayResult downHit = EntityRay::Cast(m_position + CVector(0.0f, 0.0f, 1.0f),
                    CVector(0.0f, 0.0f, -1.0f), 300.0f, EntityRay::IgnoreHidden, EntityRay::GeometricOnly);
                ApplyVisibleArea(ResolveHitArea(downHit.pEntity), pPlayer, pVehicle);
            }

            pTarget->SetOrientation(0.0f, 0.0f, m_yawRadians);
            pTarget->bUsesCollision = true;
            pPhysical->m_vecMoveSpeed = { 0.0f, 0.0f, 0.0f };
            pPhysical->m_vecTurnSpeed = { 0.0f, 0.0f, 0.0f };

            if (pPlayer)
            {
                pPlayer->bIsVisible = true;
            }
            if (pVehicle)
            {
                pVehicle->bIsVisible = true;
                pVehicle->bEngineOn = true;
            }
        }
    }
    m_gamePositionWasActive = m_active;
}
