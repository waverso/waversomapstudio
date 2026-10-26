// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace UserData
{
    inline constexpr std::uint32_t Version = 1;
    inline constexpr std::size_t MaxFileBytes = 32 * 1024 * 1024;
    inline constexpr std::size_t MaxBookmarks = 65536;
    inline constexpr std::size_t MaxFavorites = 262144;
    inline constexpr std::uint32_t CustomThemeIdBase = 0x80000000u;

    enum class Group
    {
        Settings,
        Bookmarks,
        Favorites,
        Themes
    };

    struct Settings
    {
        float uiScale = 1.0f;
        float gizmoScale = 1.0f;
        float indicatorScale = 1.0f;
        float fov = 70.0f;
        float mouseSensitivity = 2.5f;
        float baseSpeed = 60.0f;
        float scrollSpeed = 70.0f;
        float fastMultiplier = 4.0f;
        float slowMultiplier = 0.2f;
        float sidebarWidth = 0.0f;
        float sidebarSplit = 0.38f;
        float positionGrid = 5.0f;
        float rotationGrid = 45.0f;
        std::uint32_t vehicleColorStyle = 1;
        std::uint32_t themeIndex = 0;
        bool outline = true;
        std::uint32_t updateCheckDays = 1;
        std::uint64_t lastUpdateCheck = 0;
        bool operator==(const Settings&) const = default;
    };

    struct Bookmark
    {
        std::uint64_t id = 0;
        std::string name;
        float position[3] = {};
        float yaw = 0.0f;
        float pitch = 0.0f;
        bool operator==(const Bookmark&) const = default;
    };
    using Bookmarks = std::vector<Bookmark>;

    struct NamedFavorite
    {
        std::string library;
        std::string name;
        NamedFavorite(std::string_view library, std::string_view name) : library(library), name(name)
        {
            for (std::string* text : { &this->library, &this->name })
            {
                for (char& c : *text)
                {
                    if (c >= 'A' && c <= 'Z')
                    {
                        c += 'a' - 'A';
                    }
                }
            }
        }
        auto operator<=>(const NamedFavorite&) const = default;
    };

    struct Favorites
    {
        std::set<int> objects, vehicles, skins;
        std::set<NamedFavorite> textures, animations;
        bool operator==(const Favorites&) const = default;
    };

    struct CustomTheme
    {
        std::uint32_t themeId = 0, sourcePresetId = 0;
        std::string name;
        std::array<std::uint32_t, 7> colors {};
        bool operator==(const CustomTheme&) const = default;
    };

    struct Themes
    {
        std::uint32_t selectedThemeId = 1, nextThemeId = CustomThemeIdBase;
        std::vector<CustomTheme> customThemes;
        std::vector<std::uint32_t> orderedThemeIds, hiddenThemeIds;
        bool operator==(const Themes&) const = default;
    };

    inline Favorites g_favoriteState;
    inline std::uint64_t g_favoritesRevision = 0;
    inline std::uint64_t g_bookmarksRevision = 0;

    template <class Set, class Key>
    void SetFavorite(Set& set, const Key& key, bool enabled)
    {
        const bool changed = enabled ? set.insert(key).second : set.erase(key) != 0;
        if (changed)
        {
            ++g_favoritesRevision;
        }
    }

    template <class Set, class Key>
    bool ToggleFavorite(Set& set, const Key& key)
    {
        const bool enabled = !set.contains(key);
        SetFavorite(set, key, enabled);
        return enabled;
    }

    using Bytes = std::vector<std::uint8_t>;
    using Document = std::variant<Settings, Bookmarks, Favorites, Themes>;

    Bytes Encode(const Document& document);
    Document Decode(Group group, std::span<const std::uint8_t> bytes);
}
