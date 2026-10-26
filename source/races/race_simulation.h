// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <array>

#include "../scene/scene_data.h"

namespace Races
{
    struct Simulation
    {
        int count = 0;
        int target = -1;
        int mode = 0;
        int laps = 1;
        int lap = 1;
        bool started = false;

        bool Start(int checkpoints, int first, const RaceData& settings)
        {
            *this = {};
            if (checkpoints < 1 || first < 0 || first >= checkpoints || settings.mode < 0 || settings.mode > 1
                || settings.laps < 1 || settings.laps > RaceData::MaxLaps || (settings.mode == 1 && checkpoints < 2))
            {
                return false;
            }
            count = checkpoints;
            target = first;
            mode = settings.mode;
            laps = settings.laps;
            return true;
        }

        bool Active() const
        {
            return target >= 0;
        }

        bool FinalTarget() const
        {
            return Active() && (mode == 0 ? target == count - 1 : target == 0 && started && lap == laps);
        }

        int Type(int type) const
        {
            if (type == RaceCheckpointData::AutoGround)
            {
                return FinalTarget() ? 1 : 0;
            }
            if (type == RaceCheckpointData::AutoAir)
            {
                return FinalTarget() ? 4 : 3;
            }
            return type;
        }

        void Advance()
        {
            if (!Active())
            {
                return;
            }
            if (FinalTarget())
            {
                target = -1;
                return;
            }
            if (mode == 1 && target == 0)
            {
                if (started)
                {
                    ++lap;
                }
                started = true;
            }
            target = (target + 1) % count;
        }

        std::array<int, 5> Icons() const
        {
            std::array<int, 5> result { -1, -1, -1, -1, -1 };
            Simulation cursor = *this;
            int filled = 0;
            for (int step = 0; cursor.Active() && step <= count && filled < 5; ++step)
            {
                if (std::find(result.begin(), result.end(), cursor.target) == result.end())
                {
                    result[filled++] = cursor.target;
                }
                cursor.Advance();
            }
            return result;
        }
    };
}
