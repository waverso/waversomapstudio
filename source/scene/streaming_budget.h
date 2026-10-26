// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cmath>

namespace StreamingBudget
{
    inline constexpr int Objects = 1000;
    inline constexpr int Vehicles = 200;
    inline constexpr int Pickups = 1000;
    inline constexpr int Actors = 1000;
    inline constexpr int PedPoolReserve = 16;
    inline constexpr int ObjectPoolReserve = 64;
    inline constexpr int VehiclePoolReserve = 16;
    inline constexpr int PickupPoolReserve = 16;

    inline int Capacity(int maximum, int owned, int freeSlots, int reserve)
    {
        return std::clamp(owned + freeSlots - reserve, 0, maximum);
    }

    template <int MaxItems>
    class Selection
    {
        public:
        template <class Scene>
        void Begin(const Scene& scene)
        {
            count = 0;
            std::fill_n(allowed, scene.count, false);
            for (int index : scene.DisplayOrder())
            {
                const auto& item = scene.items[index];
                priority[index] = item.selected || (item.parent >= 0 && priority[item.parent]);
            }
        }

        void Add(int index, int id, float distanceSquared, bool retained)
        {
            candidates[count++] = { index, id, distanceSquared, priority[index], retained };
        }

        void Select(int capacity)
        {
            const int keep = std::clamp(capacity, 0, count);
            if (keep < count)
            {
                if (keep > 0)
                {
                    for (int i = 0; i < count; ++i)
                    {
                        auto& candidate = candidates[i];
                        candidate.distance = std::max(0.0f, std::sqrt(candidate.distance) - (candidate.retained ? 25.0f : 0.0f));
                    }
                    std::nth_element(candidates, candidates + keep, candidates + count, Less);
                    std::sort(candidates, candidates + keep, Less);
                }
                count = keep;
            }
            else
            {
                std::partition(candidates, candidates + count,
                    [](const Candidate& candidate)
                    {
                        return candidate.priority;
                    });
            }
            for (int i = 0; i < count; ++i)
            {
                allowed[candidates[i].index] = true;
            }
        }

        bool Contains(int index) const
        {
            return allowed[index];
        }

        int Count() const
        {
            return count;
        }

        int Index(int position) const
        {
            return candidates[position].index;
        }

        private:
        struct Candidate
        {
            int index;
            int id;
            float distance;
            bool priority;
            bool retained;
        };

        static bool Less(const Candidate& a, const Candidate& b)
        {
            if (a.priority != b.priority)
            {
                return a.priority;
            }
            if (a.distance != b.distance)
            {
                return a.distance < b.distance;
            }
            if (a.retained != b.retained)
            {
                return a.retained;
            }
            return a.id < b.id;
        }

        Candidate candidates[MaxItems];
        bool allowed[MaxItems] = {};
        bool priority[MaxItems] = {};
        int count = 0;
    };
}
