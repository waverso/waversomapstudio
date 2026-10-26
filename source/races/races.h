// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

struct CVector;

namespace Races
{
    void Update();
    void Render();
    void Stop();
    int Pick(const CVector& origin, const CVector& direction, float& maxDistance);
}
