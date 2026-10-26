// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>
#include <string_view>

namespace StudioFormat
{
    inline constexpr std::string_view ProjectMagic = "\xF0\xBA\xC4\xD0\xB7\x02";
    inline constexpr std::string_view DatabaseMagic = "\xF0\xBA\xC4\xD0\xB7\x01";
    inline constexpr std::string_view SettingsMagic = "\xF0\xBA\xC4\xD0\xB7\x03";
    inline constexpr std::string_view BookmarksMagic = "\xF0\xBA\xC4\xD0\xB7\x04";
    inline constexpr std::string_view FavoritesMagic = "\xF0\xBA\xC4\xD0\xB7\x05";
    inline constexpr std::string_view AtlasMagic = "\xF0\xBA\xC4\xD0\xB7\x06";
    inline constexpr std::string_view ThemesMagic = "\xF0\xBA\xC4\xD0\xB7\x07";
    inline constexpr std::uint32_t DatabaseVersion = 1;
    inline constexpr std::uint32_t AtlasVersion = 1;

#pragma pack(push, 1)
    struct AtlasHeader
    {
        char magic[AtlasMagic.size()];
        std::uint32_t version;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t count;
        std::uint32_t imageSize;
    };

    struct AtlasEntry
    {
        char name[32];
        std::uint32_t x;
        std::uint32_t y;
        std::uint32_t width;
        std::uint32_t height;
    };

    struct DatabaseHeader
    {
        char magic[DatabaseMagic.size()];
        std::uint32_t version;
        std::uint32_t headerSize;
        std::uint32_t flags;
        std::uint32_t txdNameCount;
        std::uint32_t textureNameCount;
        std::uint32_t modelNameCount;
        std::uint32_t textureRowCount;
        std::uint32_t txdInfoCount;
        std::uint32_t modelCount;
        std::uint32_t validModelCount;
        std::uint32_t categoryCount;
        std::uint32_t aspectRatioCount;
        std::uint32_t categoryOffsets;
        std::uint32_t txdNameOffsets;
        std::uint32_t textureMetas;
        std::uint32_t modelNameOffsets;
        std::uint32_t textureRows;
        std::uint32_t txdInfos;
        std::uint32_t models;
        std::uint32_t modelIds;
        std::uint32_t aspectRatios;
        std::uint32_t stringPool;
        std::uint32_t stringPoolSize;
        std::uint32_t objectTagCount;
        std::uint32_t objectModelTagCount;
        std::uint32_t objectTagNameOffsets;
        std::uint32_t objectModelTags;
        std::uint32_t objectModelMetas;
        std::uint32_t thumbnailAtlasCount;
        std::uint32_t thumbnailAtlases;
        std::uint32_t objectCategoryCount;
        std::uint32_t objectCategoryNameOffsets;
        std::uint32_t objectModelStats;
        std::uint32_t reservedObjectFilterLimits;
    };
#pragma pack(pop)

    static_assert(sizeof(AtlasHeader) == AtlasMagic.size() + 5 * sizeof(std::uint32_t));
    static_assert(sizeof(AtlasEntry) == 48);
    static_assert(sizeof(DatabaseHeader) == DatabaseMagic.size() + 34 * sizeof(std::uint32_t));
}
