// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <bit>

#include "CCarCtrl.h"
#include "CPlane.h"
#include "CPools.h"
#include "CTrain.h"
#include "CVehicleModelInfo.h"
#include "CVisibilityPlugins.h"
#include "model_checks.h"
#include "vehicle_catalog.h"

namespace VehicleInstance
{
    inline CVehicle* Create(int modelId)
    {
        if (!VehicleCatalog::IsValid(modelId) || !CPools::ms_pVehiclePool || CPools::ms_pVehiclePool->GetNoOfFreeSpaces() == 0)
        {
            return nullptr;
        }
        auto* info = CModelInfo::GetModelInfo(modelId);
        if (!info || info->GetModelType() != MODEL_INFO_VEHICLE || !info->m_pRwObject)
        {
            return nullptr;
        }
        const auto* vehicleInfo = static_cast<CVehicleModelInfo*>(info);
        CVehicle* vehicle = nullptr;
        if (vehicleInfo->m_nVehicleType == VEHICLE_TRAIN)
        {
            auto* train = new CTrain(modelId, 2);
            train->m_nTrainFlags.bNotOnARailRoad = true;
            vehicle = train;
        }
        else
        {
            vehicle = CCarCtrl::GetNewVehicleDependingOnCarModel(modelId, 2);
        }
        if (vehicle)
        {
            vehicle->m_eDoorLock = DOORLOCK_LOCKED;
            vehicle->bCanBeDamaged = false;
            vehicle->bEngineOn = false;
            vehicle->m_nPrimaryColor = 2;
            vehicle->m_nSecondaryColor = 1;
            vehicle->m_nTertiaryColor = vehicle->m_nQuaternaryColor = 1;
        }
        return vehicle;
    }

    inline void UpdatePreview(CVehicle* vehicle)
    {
        if (!vehicle)
        {
            return;
        }
        if (VehicleCatalog::HasPropellerAnimation(vehicle->m_nModelIndex))
        {
            auto* plane = static_cast<CPlane*>(vehicle);
            plane->bEngineOn = true;
            plane->field_9C4 = std::bit_cast<int>(CPlane::PLANE_STD_PROP_SPEED);
            plane->ProcessControl();
        }
        vehicle->PreRender();
        if (vehicle->m_nVehicleClass == VEHICLE_BOAT)
        {
            vehicle->UpdateRwMatrix();
        }
    }

    class EditableMaterialsScope
    {
        public:
        explicit EditableMaterialsScope(CVehicle* vehicle) : vehicle(vehicle)
        {
            if (vehicle && vehicle->m_pRwClump)
            {
                info = static_cast<CVehicleModelInfo*>(CModelInfo::GetModelInfo(vehicle->m_nModelIndex));
                colors[0] = info->m_nCurrentPrimaryColor;
                colors[1] = info->m_nCurrentSecondaryColor;
                colors[2] = info->m_nCurrentTertiaryColor;
                colors[3] = info->m_nCurrentQuaternaryColor;
                info->SetVehicleColour(2, 1, 1, 1);
                info->SetEditableMaterials(vehicle->m_pRwClump);
            }
        }

        ~EditableMaterialsScope()
        {
            if (info)
            {
                info->ResetEditableMaterials(vehicle->m_pRwClump);
                info->SetVehicleColour(colors[0], colors[1], colors[2], colors[3]);
            }
        }

        private:
        CVehicle* vehicle = nullptr;
        CVehicleModelInfo* info = nullptr;
        unsigned char colors[4] = {};
    };
}
