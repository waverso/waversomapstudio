// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "../shared/animation_catalog.h"
#include "../scene/scene_data.h"
#include "../editor/user_data.h"

namespace AnimationBrowser
{
    enum SortKey
    {
        SortName,
        SortLibrary,
        SortId,
        SortFrames,
        SortDuration
    };
    enum SearchField
    {
        SearchLibrary = 1,
        SearchId = 2,
        SearchName = 4,
        SearchDefault = 7
    };
    enum MissingMode
    {
        MissingAny,
        MissingOnly,
        MissingHide
    };
    enum BuggedMode
    {
        BuggedAny,
        BuggedOnly,
        BuggedHide
    };

    struct Filter
    {
        char search[96] = {};
        unsigned searchFields = SearchDefault;
        std::array<bool, AnimationCatalog::LibraryCount> libraries = {};
        int missing = MissingHide;
        int bugged = BuggedHide;
        int idMin = 1, idMax = AnimationCatalog::Count;
        int framesMin = 0, framesMax = AnimationCatalog::MaxFrames;
        float durationMin = 0.0f, durationMax = AnimationCatalog::MaxDuration;
        bool favoritesOnly = false;
        int sortKey = SortId;
        bool sortDesc = false;
        bool operator==(const Filter&) const = default;
    };

    inline std::unordered_set<int> g_favorites;

    inline bool IsFavorite(int id)
    {
        return g_favorites.contains(id);
    }

    inline void ToggleFavorite(int id)
    {
        const auto* animation = AnimationCatalog::Get(id);
        if (!animation)
        {
            return;
        }
        const bool enabled = !g_favorites.contains(id);
        UserData::SetFavorite(UserData::g_favoriteState.animations,
            UserData::NamedFavorite(AnimationCatalog::Libraries[animation->library], animation->name), enabled);
        if (enabled)
        {
            g_favorites.insert(id);
        }
        else
        {
            g_favorites.erase(id);
        }
    }

    inline void RestoreFavorites()
    {
        g_favorites.clear();
        if (UserData::g_favoriteState.animations.empty())
        {
            return;
        }
        for (const auto& animation : AnimationCatalog::Entries)
        {
            if (UserData::g_favoriteState.animations.contains(
                    UserData::NamedFavorite(AnimationCatalog::Libraries[animation.library], animation.name)))
            {
                g_favorites.insert(animation.id);
            }
        }
    }

    inline bool Contains(std::string_view text, std::string_view query)
    {
        return std::search(text.begin(), text.end(), query.begin(), query.end(),
                   [](char a, char b)
                   {
                       return (a >= 'A' && a <= 'Z' ? a + 'a' - 'A' : a) == (b >= 'A' && b <= 'Z' ? b + 'a' - 'A' : b);
                   })
            != text.end();
    }

    inline void RunFilter(const Filter& filter, std::vector<int>& result)
    {
        result.clear();
        const bool hasLibrary = std::any_of(filter.libraries.begin(), filter.libraries.end(),
            [](bool selected)
            {
                return selected;
            });
        for (const auto& animation : AnimationCatalog::Entries)
        {
            const bool missing = animation.missing != AnimationCatalog::Missing::None;
            if (animation.id < filter.idMin || animation.id > filter.idMax || animation.frames < filter.framesMin
                || animation.frames > filter.framesMax || animation.duration < filter.durationMin
                || animation.duration > filter.durationMax || (hasLibrary && !filter.libraries[animation.library])
                || (filter.missing == MissingOnly && !missing) || (filter.missing == MissingHide && missing)
                || (filter.bugged == BuggedOnly && !animation.bugged) || (filter.bugged == BuggedHide && animation.bugged)
                || (filter.favoritesOnly && !IsFavorite(animation.id)))
            {
                continue;
            }
            char id[16];
            snprintf(id, sizeof(id), "%d", animation.id);
            std::string_view query(filter.search);
            bool match = true;
            while (!query.empty())
            {
                const auto first = query.find_first_not_of(" \t>");
                if (first == std::string_view::npos)
                {
                    break;
                }
                query.remove_prefix(first);
                const auto end = query.find_first_of(" \t>");
                const auto token = query.substr(0, end);
                match
                    &= ((filter.searchFields & SearchLibrary) && Contains(AnimationCatalog::Libraries[animation.library], token))
                    || ((filter.searchFields & SearchId) && Contains(id, token))
                    || ((filter.searchFields & SearchName) && Contains(animation.name, token));
                if (!match || end == std::string_view::npos)
                {
                    break;
                }
                query.remove_prefix(end);
            }
            if (match)
            {
                result.push_back(animation.id);
            }
        }
        std::sort(result.begin(), result.end(),
            [&](int a, int b)
            {
                const auto& lhs = *AnimationCatalog::Get(a);
                const auto& rhs = *AnimationCatalog::Get(b);
                int compare = 0;
                switch (filter.sortKey)
                {
                    case SortLibrary:
                        compare = _stricmp(AnimationCatalog::Libraries[lhs.library], AnimationCatalog::Libraries[rhs.library]);
                        if (!compare)
                        {
                            compare = _stricmp(lhs.name, rhs.name);
                        }
                        break;
                    case SortId:
                        compare = (a > b) - (a < b);
                        break;
                    case SortFrames:
                        compare = (lhs.frames > rhs.frames) - (lhs.frames < rhs.frames);
                        break;
                    case SortDuration:
                        compare = (lhs.duration > rhs.duration) - (lhs.duration < rhs.duration);
                        break;
                    default:
                        compare = _stricmp(lhs.name, rhs.name);
                        break;
                }
                if (!compare)
                {
                    compare = (a > b) - (a < b);
                }
                return filter.sortDesc ? compare > 0 : compare < 0;
            });
    }

    inline void Select(ActorAnimationData& data, int id)
    {
        if (const auto* entry = AnimationCatalog::Get(id))
        {
            strcpy_s(data.library, AnimationCatalog::Libraries[entry->library]);
            strcpy_s(data.name, entry->name);
        }
    }

    inline void FormatDuration(char* buffer, size_t size, float duration)
    {
        snprintf(buffer, size, "%.2f", duration);
        size_t end = strlen(buffer);
        while (end && buffer[end - 1] == '0')
        {
            --end;
        }
        if (end && buffer[end - 1] == '.')
        {
            --end;
        }
        if (end + 1 < size)
        {
            buffer[end++] = 's';
            buffer[end] = '\0';
        }
    }
}
