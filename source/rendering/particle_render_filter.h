// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "FxManager_c.h"
#include "FxPrimBP_c.h"

namespace PlayerVisibility
{
    template <typename Visitor>
    void VisitParticleLists(FxManager_c& manager, Visitor&& visit)
    {
        for (auto* blueprint = manager.m_fxSystemBPList.GetHead(); blueprint;
            blueprint = manager.m_fxSystemBPList.GetNext(blueprint))
        {
            for (unsigned i = 0; i < blueprint->m_nNumEmitters; ++i)
            {
                auto* emitter = static_cast<FxPrimBP_c*>(blueprint->m_emittersList[i]);
                if (emitter)
                {
                    visit(emitter->m_emitterPrts);
                }
            }
        }
    }

    class ParticleRenderFilter
    {
        public:
        ParticleRenderFilter(FxManager_c& manager, const std::vector<FxSystem_c*>& hidden)
            : removed(Buffer()), start(removed.size())
        {
            if (hidden.empty())
            {
                return;
            }
            std::size_t capacity = start;
            VisitParticleLists(manager,
                [&](List_c& list)
                {
                    capacity += list.count;
                });
            if (capacity > removed.capacity())
            {
                removed.reserve(std::max(capacity, removed.capacity() * 2));
            }
            const auto* hiddenBegin = hidden.data();
            const auto* hiddenEnd = hiddenBegin + hidden.size();
            VisitParticleLists(manager,
                [&](List_c& list)
                {
                    for (auto* particle = list.GetHead(); particle;)
                    {
                        auto* next = list.GetNext(particle);
                        auto* system = *reinterpret_cast<FxSystem_c**>(reinterpret_cast<std::uintptr_t>(particle) + 0x28);
                        if (std::find(hiddenBegin, hiddenEnd, system) != hiddenEnd)
                        {
                            removed.push_back({ &list, particle });
                            list.RemoveItem(particle);
                        }
                        particle = next;
                    }
                });
        }

        ~ParticleRenderFilter()
        {
            if (removed.size() == start)
            {
                return;
            }
            auto* end = removed.data() + start;
            for (auto* it = removed.data() + removed.size(); it != end;)
            {
                --it;
                ListItem_c* links[2];
                std::memcpy(links, it->particle, sizeof(links));
                if (links[0])
                {
                    std::memcpy(reinterpret_cast<char*>(links[0]) + sizeof(void*), &it->particle, sizeof(void*));
                }
                else
                {
                    it->list->last = it->particle;
                }
                if (links[1])
                {
                    std::memcpy(links[1], &it->particle, sizeof(void*));
                }
                else
                {
                    it->list->first = it->particle;
                }
                ++it->list->count;
            }
            removed.resize(start);
        }

        ParticleRenderFilter(const ParticleRenderFilter&) = delete;
        ParticleRenderFilter& operator=(const ParticleRenderFilter&) = delete;

        private:
        struct RemovedParticle
        {
            List_c* list;
            ListItem_c* particle;
        };
        static std::vector<RemovedParticle>& Buffer()
        {
            static thread_local std::vector<RemovedParticle> buffer;
            return buffer;
        }
        std::vector<RemovedParticle>& removed;
        std::size_t start;
    };
}
