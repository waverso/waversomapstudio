// Source Repository: https://github.com/waverso/waversomapstudio

#include "model_constraints.h"

#include "CBaseModelInfo.h"
#include "CModelInfo.h"

bool ModelConstraints::SupportsRotationXY(int modelId)
{
    if (modelId < 0 || modelId >= 20000 || !CModelInfo::ms_modelInfoPtrs)
    {
        return true;
    }
    CBaseModelInfo* model = CModelInfo::GetModelInfo(modelId);
    return !model || !model->AsAtomicModelInfoPtr() || !model->SwaysInWind();
}
