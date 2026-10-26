// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "../shared/file_formats.h"

namespace CatalogDatabase
{
    inline constexpr auto Version = StudioFormat::DatabaseVersion;
    inline constexpr std::uint16_t MaxModelId = 20000;
    inline constexpr std::uint16_t InvalidIndex = 0xFFFF;

    enum SourceArchive : std::uint8_t
    {
        ArchiveGtaInt = 0,
        ArchiveGta3 = 1,
        ArchiveCutscene = 2,
        ArchiveSamp = 3,
        ArchiveCount = 4
    };

    inline constexpr std::uint8_t ModelFlagCollidable = 0x01;
    inline constexpr std::uint8_t ModelFlagLowLod = 0x02;
    inline constexpr std::uint8_t ObjectBehaviourBreakable = 0x01;
    inline constexpr std::uint8_t ObjectBehaviourCollidable = 0x02;
    inline constexpr std::uint8_t ObjectBehaviourAnimation = 0x04;
    inline constexpr std::uint8_t ObjectBehaviourBackface = 0x08;
    inline constexpr std::uint8_t ObjectBehaviourPhysics = 0x10;
    inline constexpr std::uint8_t ObjectBehaviourExplosive = 0x20;
    inline constexpr std::uint8_t ObjectBehaviourAll = ObjectBehaviourBreakable | ObjectBehaviourCollidable
        | ObjectBehaviourAnimation | ObjectBehaviourBackface | ObjectBehaviourPhysics | ObjectBehaviourExplosive;
    inline constexpr std::uint8_t ObjectStatsIndexed = 0x01;
    inline constexpr std::uint8_t ObjectStatsManualCenter = 0x02;
    inline constexpr std::uint8_t ObjectStatsMaterialColorUnsupported = 0x04;
    inline constexpr std::uint8_t ObjectTimeNone = 0xFF;
    inline constexpr std::uint16_t ObjectThumbnailNone = 0xFFFF;
    inline constexpr std::uint16_t ObjectThumbnailReady = 0x0001;
    inline constexpr std::uint16_t ObjectThumbnailAnimated = 0x0002;
    inline constexpr std::uint32_t ThumbnailCodecRawBc3 = 0;
    inline constexpr std::uint32_t ThumbnailCodecZstdBc3 = 1;
    inline constexpr std::uint16_t TextureFlagLowLod = 0x0001;
    inline constexpr std::uint16_t TextureFlagSeamlessV = 0x0002;
    inline constexpr std::uint16_t TextureFlagSeamlessH = 0x0004;

#pragma pack(push, 1)
    using DatabaseHeader = StudioFormat::DatabaseHeader;

    struct TextureRow
    {
        std::uint16_t textureNameIndex;
        std::uint16_t width;
        std::uint16_t height;
        std::uint16_t sizeKiB;
        std::uint8_t mips;
        std::uint8_t flags;
        std::uint16_t aspectIndex;
    };

    struct TxdInfo
    {
        std::uint32_t firstTexture;
        std::uint16_t textureCount;
        std::uint8_t sourceArchive;
        std::uint8_t _pad;
    };

    struct ModelEntry
    {
        std::uint16_t modelNameIndex;
        std::uint16_t txdNameIndex;
        std::uint8_t flags;
        std::uint8_t _pad;
    };

    struct TextureMeta
    {
        std::uint32_t nameOffset;
        std::uint32_t descriptionOffset;
        std::uint16_t category;
        std::uint16_t flags;
        std::uint16_t aspectIndex;
        std::uint16_t _pad;
    };

    struct AspectRatio
    {
        std::uint16_t w;
        std::uint16_t h;
    };

    struct ObjectModelMeta
    {
        std::uint32_t descriptionOffset;
        std::uint32_t firstTag;
        std::uint16_t tagCount;
        std::uint16_t thumbnailAtlas;
        std::uint16_t thumbnailTile;
        std::uint16_t thumbnailFlags;
        float target[3];
        float yaw;
        float pitch;
        float distance;
        float boundsMin[3];
        float boundsMax[3];
        std::uint16_t thumbnailFrameIntervalMs;
    };

    struct ObjectModelStats
    {
        std::uint32_t colNameOffset;
        std::uint32_t dffSizeKb;
        std::uint32_t polyCount;
        std::uint32_t vertexCount;
        float radius;
        float length;
        float width;
        float height;
        float drawDistance;
        std::uint16_t materialCount;
        std::uint16_t category;
        std::uint8_t sourceArchive;
        std::uint8_t timeOn;
        std::uint8_t timeOff;
        std::uint8_t behaviourFlags;
        std::uint8_t flags;
        std::uint8_t _pad[3];
    };

    struct ThumbnailAtlas
    {
        std::uint32_t dataOffset;
        std::uint32_t compressedSize;
        std::uint32_t rawSize;
        std::uint32_t crc32;
        std::uint32_t codec;
        std::uint16_t width;
        std::uint16_t height;
        std::uint16_t tileSize;
        std::uint16_t tileCount;
    };
#pragma pack(pop)

    static_assert(sizeof(ObjectModelMeta) == 66);
    static_assert(sizeof(ObjectModelStats) == 48);

    struct DatabaseView
    {
        const DatabaseHeader& header;
        std::span<const std::uint32_t> categoryOffsets;
        std::span<const std::uint32_t> txdNameOffsets;
        std::span<const TextureMeta> textureMetas;
        std::span<const std::uint32_t> modelNameOffsets;
        std::span<const TextureRow> textureRows;
        std::span<const TxdInfo> txdInfos;
        std::span<const ModelEntry> models;
        std::span<const std::uint16_t> modelIds;
        std::span<const AspectRatio> aspectRatios;
        std::span<const std::uint32_t> objectTagNameOffsets;
        std::span<const std::uint16_t> objectModelTags;
        std::span<const ObjectModelMeta> objectModelMetas;
        std::span<const ThumbnailAtlas> thumbnailAtlases;
        std::span<const std::uint32_t> objectCategoryNameOffsets;
        std::span<const ObjectModelStats> objectModelStats;
        std::string_view stringPool;
    };

    struct ThumbnailTilePayload
    {
        std::span<const std::uint8_t> bytes;
        std::uint32_t codec = ThumbnailCodecRawBc3;
        std::uint32_t rawSize = 0;
        std::uint32_t crc32 = 0;
        std::uint16_t width = 0;
        std::uint16_t height = 0;
        std::uint16_t tileSize = 0;
        std::uint16_t tileIndex = 0;
        std::uint16_t tileCount = 1;
        std::uint16_t frameIntervalMs = 0;
    };

    [[nodiscard]] const DatabaseView& View() noexcept;

    [[nodiscard]] const char* StringAt(std::uint32_t offset) noexcept;
    [[nodiscard]] const char* CategoryName(std::uint16_t category) noexcept;
    [[nodiscard]] const char* TxdName(std::uint16_t index) noexcept;
    [[nodiscard]] const char* TexName(std::uint16_t index) noexcept;
    [[nodiscard]] const char* TextureDescription(std::uint16_t texNameIdx) noexcept;
    [[nodiscard]] const char* ModelName(std::uint16_t index) noexcept;
    [[nodiscard]] const char* ModelDescription(std::uint16_t model_id) noexcept;
    [[nodiscard]] const char* ObjectTagName(std::uint16_t tag) noexcept;
    [[nodiscard]] const ObjectModelMeta* ObjectMeta(std::uint16_t model_id) noexcept;
    [[nodiscard]] std::span<const std::uint16_t> ModelTags(std::uint16_t model_id) noexcept;
    [[nodiscard]] const char* ObjectCategoryName(std::uint16_t category) noexcept;
    [[nodiscard]] const ObjectModelStats* ObjectStats(std::uint16_t model_id) noexcept;
    [[nodiscard]] bool ObjectThumbnail(std::uint16_t model_id, ThumbnailTilePayload& out) noexcept;

    [[nodiscard]] bool ModelExists(std::uint16_t model_id) noexcept;
    [[nodiscard]] bool ModelCollidable(std::uint16_t model_id) noexcept;
    [[nodiscard]] bool ModelSupportsMaterialColor(std::uint16_t model_id) noexcept;
    [[nodiscard]] bool TextureIsLowLod(std::uint16_t texNameIdx) noexcept;
    [[nodiscard]] bool TextureIsSeamless(std::uint16_t texNameIdx, int seamless) noexcept;
}
