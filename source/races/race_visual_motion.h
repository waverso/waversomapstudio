// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

namespace Races
{
    struct VisualMotion
    {
        int type = -1;
        float amount = 0.0f;
        float height = 0.0f;
        bool rising = true;
        unsigned int finishedAt = 0;
        bool waiting = false;

        void Reset(int checkpointType, float z)
        {
            *this = {};
            type = checkpointType;
            height = z;
        }

        void Update(float playerDistanceSquared, float playerZ, float radius, unsigned int tick)
        {
            if (waiting)
            {
                if (tick - finishedAt < 1000u)
                {
                    return;
                }
                Reset(6, height);
            }
            const auto reverse = [this](float maximum, float minimum)
            {
                if (amount > maximum)
                {
                    rising = false;
                }
                else if (amount < minimum)
                {
                    rising = true;
                }
            };
            if (type == 5)
            {
                reverse(0.5f, -0.5f);
                amount = rising ? 0.1f : -0.1f;
            }
            else if (type == 6)
            {
                reverse(0.75f, 0.0f);
                amount += rising ? 0.01f : -0.01f;
                if (amount < 0.0f)
                {
                    type = -1;
                    finishedAt = tick;
                    waiting = true;
                }
            }
            else if (type == 7)
            {
                if (playerDistanceSquared < radius * radius)
                {
                    type = 8;
                    amount = -0.1f;
                }
                else
                {
                    reverse(0.2f, -0.2f);
                    amount += rising ? 0.02f : -0.02f;
                    height += amount;
                }
            }
            if (type == 8)
            {
                if (playerDistanceSquared > radius * radius)
                {
                    type = 7;
                    rising = true;
                    amount = 0.02f;
                }
                if (playerZ - 0.75f < height)
                {
                    height += amount;
                }
            }
        }
    };
}
