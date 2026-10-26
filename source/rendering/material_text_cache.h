// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <chrono>
#include <cstring>
#include <memory>
#include <rw/rwcore.h>
#include <unordered_map>

#include "../scene/scene_data.h"

class MaterialTextCache
{
    public:
    struct Entry
    {
        MaterialTextData params;
        RwTexture* texture = nullptr;
        unsigned int hash = 0;
        size_t bytes = 0;
        int refs = 0;
        Entry* previous = nullptr;
        Entry* next = nullptr;

        ~Entry()
        {
            if (texture)
            {
                RwTextureDestroy(texture);
            }
        }
    };

    static bool Matches(const Entry& entry, const MaterialTextData& params)
    {
        const auto& saved = entry.params;
        return saved.materialSize == params.materialSize && saved.fontSize == params.fontSize && saved.bold == params.bold
            && saved.fontColorArgb == params.fontColorArgb && saved.backgroundColorArgb == params.backgroundColorArgb
            && saved.alignment == params.alignment && std::strncmp(saved.fontFace, params.fontFace, sizeof(saved.fontFace)) == 0
            && std::strncmp(saved.text, params.text, sizeof(saved.text)) == 0;
    }

    void BeginFrame()
    {
        m_builds = 0;
        m_buildTime = Clock::duration::zero();
    }

    template <class Build>
    Entry* Acquire(const MaterialTextData& params, unsigned int hash, size_t bytes, Build&& build, bool& failed)
    {
        failed = false;
        const auto range = m_entries.equal_range(hash);
        for (auto it = range.first; it != range.second; ++it)
        {
            Entry* entry = it->second.get();
            if (Matches(*entry, params))
            {
                if (!entry->refs)
                {
                    UnlinkIdle(entry);
                }
                ++entry->refs;
                return entry;
            }
        }
        if (m_builds >= 4 || m_buildTime >= std::chrono::milliseconds(2))
        {
            return nullptr;
        }
        ++m_builds;
        const auto start = Clock::now();
        Entry* result = nullptr;
        try
        {
            auto entry = std::make_unique<Entry>();
            entry->params = params;
            entry->hash = hash;
            entry->bytes = bytes;
            const auto it = m_entries.emplace(hash, std::move(entry));
            try
            {
                it->second->texture = build();
            }
            catch (const std::bad_alloc&)
            {
            }
            if (it->second->texture)
            {
                result = it->second.get();
                result->refs = 1;
            }
            else
            {
                m_entries.erase(it);
            }
        }
        catch (const std::bad_alloc&)
        {
        }
        m_buildTime += Clock::now() - start;
        failed = !result;
        return result;
    }

    void Release(Entry* entry)
    {
        if (!entry || --entry->refs)
        {
            return;
        }
        entry->previous = m_idleLast;
        if (m_idleLast)
        {
            m_idleLast->next = entry;
        }
        else
        {
            m_idleFirst = entry;
        }
        m_idleLast = entry;
        m_idleBytes += entry->bytes;
        ++m_idleCount;
        while (m_idleBytes > 16 * 1024 * 1024 || m_idleCount > 256)
        {
            Entry* oldest = m_idleFirst;
            UnlinkIdle(oldest);
            const auto range = m_entries.equal_range(oldest->hash);
            for (auto it = range.first; it != range.second; ++it)
            {
                if (it->second.get() == oldest)
                {
                    m_entries.erase(it);
                    break;
                }
            }
        }
    }

    private:
    using Clock = std::chrono::steady_clock;
    std::unordered_multimap<unsigned int, std::unique_ptr<Entry>> m_entries;
    Entry* m_idleFirst = nullptr;
    Entry* m_idleLast = nullptr;
    size_t m_idleBytes = 0;
    int m_idleCount = 0;
    int m_builds = 0;
    Clock::duration m_buildTime {};

    void UnlinkIdle(Entry* entry)
    {
        if (entry->previous)
        {
            entry->previous->next = entry->next;
        }
        else
        {
            m_idleFirst = entry->next;
        }
        if (entry->next)
        {
            entry->next->previous = entry->previous;
        }
        else
        {
            m_idleLast = entry->previous;
        }
        entry->previous = entry->next = nullptr;
        m_idleBytes -= entry->bytes;
        --m_idleCount;
    }
};
