// Source Repository: https://github.com/waverso/waversomapstudio

#include "database.h"

#include <Windows.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>

#include "../../resources/resource.h"
#include "../shared/crc32.h"

namespace
{
    const unsigned char* g_catalogBytes = nullptr;
    std::size_t g_catalogSizeBytes = 0;

    void LoadDatabaseResource() noexcept
    {
        if (g_catalogBytes)
        {
            return;
        }

        HMODULE module = nullptr;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&LoadDatabaseResource), &module))
        {
            std::abort();
        }

        HRSRC resource = FindResourceA(module, MAKEINTRESOURCEA(IDR_EDITOR_DATABASE), RT_RCDATA);
        if (!resource)
        {
            std::abort();
        }
        HGLOBAL loaded = LoadResource(module, resource);
        if (!loaded)
        {
            std::abort();
        }
        const DWORD size = SizeofResource(module, resource);
        const void* data = LockResource(loaded);
        if (!data || !size)
        {
            std::abort();
        }

        g_catalogBytes = static_cast<const unsigned char*>(data);
        g_catalogSizeBytes = size;
    }

    template <class T>
    [[nodiscard]] std::span<const T> TableAt(
        const CatalogDatabase::DatabaseHeader& h, std::uint32_t offset, std::uint32_t count) noexcept
    {
        const std::size_t bytes = g_catalogSizeBytes;
        const std::size_t begin = offset;
        if (count > bytes / sizeof(T))
        {
            std::abort();
        }
        const std::size_t size = (std::size_t)count * sizeof(T);
        if (begin > bytes || size > bytes - begin)
        {
            std::abort();
        }
        if ((reinterpret_cast<std::uintptr_t>(g_catalogBytes + begin) % alignof(T)) != 0)
        {
            std::abort();
        }
        return { reinterpret_cast<const T*>(g_catalogBytes + begin), (std::size_t)count };
    }

    [[noreturn]] void FailInvalidDatabase() noexcept
    {
        std::abort();
    }

    [[nodiscard]] bool IsFiniteNonNegative(float value) noexcept
    {
        return std::isfinite(value) && value >= 0.0f;
    }

    [[nodiscard]] const CatalogDatabase::DatabaseView& BuildView() noexcept
    {
        using namespace CatalogDatabase;
        static_assert(std::is_trivially_copyable_v<DatabaseHeader>);
        static_assert(std::is_trivially_copyable_v<TextureRow>);
        static_assert(std::is_trivially_copyable_v<TxdInfo>);
        static_assert(std::is_trivially_copyable_v<ModelEntry>);
        static_assert(std::is_trivially_copyable_v<TextureMeta>);
        static_assert(std::is_trivially_copyable_v<AspectRatio>);
        static_assert(std::is_trivially_copyable_v<ObjectModelMeta>);
        static_assert(std::is_trivially_copyable_v<ObjectModelStats>);
        static_assert(std::is_trivially_copyable_v<ThumbnailAtlas>);

        LoadDatabaseResource();
        if (g_catalogSizeBytes < sizeof(DatabaseHeader))
        {
            FailInvalidDatabase();
        }

        const auto& h = *reinterpret_cast<const DatabaseHeader*>(g_catalogBytes);
        if (std::memcmp(h.magic, StudioFormat::DatabaseMagic.data(), sizeof(h.magic)) != 0)
        {
            FailInvalidDatabase();
        }
        if (h.version != Version || h.headerSize != sizeof(DatabaseHeader))
        {
            FailInvalidDatabase();
        }
        if (h.modelCount != MaxModelId)
        {
            FailInvalidDatabase();
        }
        if (h.validModelCount > h.modelCount)
        {
            FailInvalidDatabase();
        }
        if (!h.txdNameCount || !h.textureNameCount || !h.textureRowCount)
        {
            FailInvalidDatabase();
        }
        if (h.txdInfoCount != h.txdNameCount)
        {
            FailInvalidDatabase();
        }
        if (!h.objectCategoryCount || h.objectCategoryCount > 32)
        {
            FailInvalidDatabase();
        }

        const auto categoryOffsets = TableAt<std::uint32_t>(h, h.categoryOffsets, h.categoryCount);
        const auto txdNameOffsets = TableAt<std::uint32_t>(h, h.txdNameOffsets, h.txdNameCount);
        const auto texMetas = TableAt<TextureMeta>(h, h.textureMetas, h.textureNameCount);
        const auto modelNameOffsets = TableAt<std::uint32_t>(h, h.modelNameOffsets, h.modelNameCount);
        const auto textureRows = TableAt<TextureRow>(h, h.textureRows, h.textureRowCount);
        const auto txdInfos = TableAt<TxdInfo>(h, h.txdInfos, h.txdInfoCount);
        const auto models = TableAt<ModelEntry>(h, h.models, h.modelCount);
        const auto modelIds = TableAt<std::uint16_t>(h, h.modelIds, h.validModelCount);
        const auto aspectRatios = TableAt<AspectRatio>(h, h.aspectRatios, h.aspectRatioCount);
        const auto objectTagNameOffsets = TableAt<std::uint32_t>(h, h.objectTagNameOffsets, h.objectTagCount);
        const auto objectModelTags = TableAt<std::uint16_t>(h, h.objectModelTags, h.objectModelTagCount);
        const auto objectModelMetas = TableAt<ObjectModelMeta>(h, h.objectModelMetas, h.modelCount);
        const auto thumbAtlases = TableAt<ThumbnailAtlas>(h, h.thumbnailAtlases, h.thumbnailAtlasCount);
        const auto objectCategoryNameOffsets = TableAt<std::uint32_t>(h, h.objectCategoryNameOffsets, h.objectCategoryCount);
        const auto objectModelStats = TableAt<ObjectModelStats>(h, h.objectModelStats, h.modelCount);

        if (h.stringPool > g_catalogSizeBytes || h.stringPoolSize > g_catalogSizeBytes - h.stringPool)
        {
            FailInvalidDatabase();
        }
        const auto pool = std::string_view(reinterpret_cast<const char*>(g_catalogBytes + h.stringPool), h.stringPoolSize);
        if (pool.empty() || pool.back() != '\0')
        {
            FailInvalidDatabase();
        }

        const auto validateOffset = [&](std::uint32_t off) noexcept
        {
            if (off >= pool.size())
            {
                FailInvalidDatabase();
            }
            const void* nul = std::memchr(pool.data() + off, '\0', pool.size() - off);
            if (!nul)
            {
                FailInvalidDatabase();
            }
        };
        for (std::uint32_t off : categoryOffsets)
        {
            validateOffset(off);
        }
        for (std::uint32_t off : txdNameOffsets)
        {
            validateOffset(off);
        }
        for (const TextureMeta& meta : texMetas)
        {
            validateOffset(meta.nameOffset);
            validateOffset(meta.descriptionOffset);
            if (meta.category >= h.categoryCount)
            {
                FailInvalidDatabase();
            }
            if (meta.aspectIndex != InvalidIndex && meta.aspectIndex >= h.aspectRatioCount)
            {
                FailInvalidDatabase();
            }
        }
        for (std::uint32_t off : modelNameOffsets)
        {
            validateOffset(off);
        }
        for (std::uint32_t off : objectTagNameOffsets)
        {
            validateOffset(off);
        }
        for (std::uint32_t off : objectCategoryNameOffsets)
        {
            validateOffset(off);
        }
        for (const TextureRow& row : textureRows)
        {
            if (row.textureNameIndex >= h.textureNameCount)
            {
                FailInvalidDatabase();
            }
            if (row.aspectIndex != InvalidIndex && row.aspectIndex >= h.aspectRatioCount)
            {
                FailInvalidDatabase();
            }
        }
        for (const TxdInfo& txd : txdInfos)
        {
            if (txd.sourceArchive >= ArchiveCount)
            {
                FailInvalidDatabase();
            }
            if (txd.firstTexture > h.textureRowCount || txd.textureCount > h.textureRowCount - txd.firstTexture)
            {
                FailInvalidDatabase();
            }
        }
        for (const ModelEntry& model : models)
        {
            if (model.modelNameIndex != InvalidIndex && model.modelNameIndex >= h.modelNameCount)
            {
                FailInvalidDatabase();
            }
            if (model.txdNameIndex != InvalidIndex && model.txdNameIndex >= h.txdInfoCount)
            {
                FailInvalidDatabase();
            }
        }
        std::uint16_t previousModelId = 0;
        bool hasPreviousModelId = false;
        for (std::uint16_t modelId : modelIds)
        {
            if (modelId >= h.modelCount || models[modelId].txdNameIndex == InvalidIndex)
            {
                FailInvalidDatabase();
            }
            if (hasPreviousModelId && modelId <= previousModelId)
            {
                FailInvalidDatabase();
            }
            previousModelId = modelId;
            hasPreviousModelId = true;
        }
        for (const ObjectModelMeta& meta : objectModelMetas)
        {
            validateOffset(meta.descriptionOffset);
            if (meta.firstTag > objectModelTags.size() || meta.tagCount > objectModelTags.size() - meta.firstTag)
            {
                FailInvalidDatabase();
            }
            if (meta.thumbnailAtlas != ObjectThumbnailNone && meta.thumbnailAtlas >= thumbAtlases.size())
            {
                FailInvalidDatabase();
            }
            if (!std::isfinite(meta.target[0]) || !std::isfinite(meta.target[1]) || !std::isfinite(meta.target[2])
                || !std::isfinite(meta.yaw) || !std::isfinite(meta.pitch) || !std::isfinite(meta.distance)
                || meta.distance <= 0.0f)
            {
                FailInvalidDatabase();
            }
            for (int axis = 0; axis < 3; ++axis)
            {
                if (!std::isfinite(meta.boundsMin[axis]) || !std::isfinite(meta.boundsMax[axis])
                    || meta.boundsMin[axis] > meta.boundsMax[axis])
                {
                    FailInvalidDatabase();
                }
            }
        }
        for (std::uint16_t tag : objectModelTags)
        {
            if (tag >= objectTagNameOffsets.size())
            {
                FailInvalidDatabase();
            }
        }
        for (const ObjectModelStats& stats : objectModelStats)
        {
            validateOffset(stats.colNameOffset);
            if (stats.category >= objectCategoryNameOffsets.size())
            {
                FailInvalidDatabase();
            }
            if (stats.sourceArchive >= ArchiveCount)
            {
                FailInvalidDatabase();
            }
            const bool hasTimeOn = stats.timeOn != ObjectTimeNone;
            const bool hasTimeOff = stats.timeOff != ObjectTimeNone;
            if (hasTimeOn != hasTimeOff)
            {
                FailInvalidDatabase();
            }
            if ((hasTimeOn && stats.timeOn > 23) || (hasTimeOff && stats.timeOff > 23))
            {
                FailInvalidDatabase();
            }
            if (stats.behaviourFlags & ~ObjectBehaviourAll)
            {
                FailInvalidDatabase();
            }
            if (stats.flags & ~(ObjectStatsIndexed | ObjectStatsManualCenter | ObjectStatsMaterialColorUnsupported))
            {
                FailInvalidDatabase();
            }
            if (!IsFiniteNonNegative(stats.radius) || !IsFiniteNonNegative(stats.length) || !IsFiniteNonNegative(stats.width)
                || !IsFiniteNonNegative(stats.height) || !IsFiniteNonNegative(stats.drawDistance))
            {
                FailInvalidDatabase();
            }
        }
        if (objectCategoryNameOffsets.empty())
        {
            FailInvalidDatabase();
        }
        for (const ThumbnailAtlas& atlas : thumbAtlases)
        {
            if (!atlas.width || !atlas.height || !atlas.tileSize || !atlas.tileCount || !atlas.rawSize)
            {
                FailInvalidDatabase();
            }
            if (atlas.dataOffset > g_catalogSizeBytes || atlas.compressedSize > g_catalogSizeBytes - atlas.dataOffset)
            {
                FailInvalidDatabase();
            }
            if (atlas.codec != ThumbnailCodecRawBc3 && atlas.codec != ThumbnailCodecZstdBc3)
            {
                FailInvalidDatabase();
            }
            if (atlas.width % 4 || atlas.height % 4 || atlas.tileSize % 4 || atlas.width % atlas.tileSize
                || atlas.height % atlas.tileSize)
            {
                FailInvalidDatabase();
            }
            const std::uint32_t tileCapacity = (static_cast<std::uint32_t>(atlas.width) / atlas.tileSize)
                * (static_cast<std::uint32_t>(atlas.height) / atlas.tileSize);
            if (atlas.tileCount > tileCapacity || atlas.rawSize != static_cast<std::uint32_t>(atlas.width) * atlas.height)
            {
                FailInvalidDatabase();
            }
            if (atlas.codec == ThumbnailCodecRawBc3 && atlas.compressedSize != atlas.rawSize)
            {
                FailInvalidDatabase();
            }
        }
        for (const ObjectModelMeta& meta : objectModelMetas)
        {
            if (meta.thumbnailAtlas != ObjectThumbnailNone && meta.thumbnailTile >= thumbAtlases[meta.thumbnailAtlas].tileCount)
            {
                FailInvalidDatabase();
            }
            if (meta.thumbnailFlags & ~(ObjectThumbnailReady | ObjectThumbnailAnimated))
            {
                FailInvalidDatabase();
            }
            if ((meta.thumbnailFlags & ObjectThumbnailAnimated) && meta.thumbnailFrameIntervalMs == 0)
            {
                FailInvalidDatabase();
            }
        }

        static const DatabaseView view { h, categoryOffsets, txdNameOffsets, texMetas, modelNameOffsets, textureRows, txdInfos,
            models, modelIds, aspectRatios, objectTagNameOffsets, objectModelTags, objectModelMetas, thumbAtlases,
            objectCategoryNameOffsets, objectModelStats, pool };
        return view;
    }
}

const CatalogDatabase::DatabaseView& CatalogDatabase::View() noexcept
{
    static const DatabaseView& view = BuildView();
    return view;
}

const char* CatalogDatabase::StringAt(std::uint32_t offset) noexcept
{
    const auto& view = View();
    if (offset >= view.stringPool.size())
    {
        FailInvalidDatabase();
    }
    return view.stringPool.data() + offset;
}

const char* CatalogDatabase::CategoryName(std::uint16_t category) noexcept
{
    const auto& view = View();
    if (category >= view.categoryOffsets.size())
    {
        return "?";
    }
    return StringAt(view.categoryOffsets[category]);
}

const char* CatalogDatabase::TxdName(std::uint16_t index) noexcept
{
    const auto& view = View();
    if (index >= view.txdNameOffsets.size())
    {
        return "";
    }
    return StringAt(view.txdNameOffsets[index]);
}

const char* CatalogDatabase::TexName(std::uint16_t index) noexcept
{
    const auto& view = View();
    if (index >= view.textureMetas.size())
    {
        return "";
    }
    return StringAt(view.textureMetas[index].nameOffset);
}

const char* CatalogDatabase::TextureDescription(std::uint16_t texNameIdx) noexcept
{
    const auto& view = View();
    if (texNameIdx >= view.textureMetas.size())
    {
        return "";
    }
    return StringAt(view.textureMetas[texNameIdx].descriptionOffset);
}

const char* CatalogDatabase::ModelName(std::uint16_t index) noexcept
{
    const auto& view = View();
    if (index >= view.modelNameOffsets.size())
    {
        return "";
    }
    return StringAt(view.modelNameOffsets[index]);
}

const char* CatalogDatabase::ModelDescription(std::uint16_t modelId) noexcept
{
    const ObjectModelMeta* meta = ObjectMeta(modelId);
    return meta ? StringAt(meta->descriptionOffset) : "";
}

const char* CatalogDatabase::ObjectTagName(std::uint16_t tag) noexcept
{
    const auto& view = View();
    return tag < view.objectTagNameOffsets.size() ? StringAt(view.objectTagNameOffsets[tag]) : "";
}

const CatalogDatabase::ObjectModelMeta* CatalogDatabase::ObjectMeta(std::uint16_t modelId) noexcept
{
    const auto& view = View();
    return modelId < view.objectModelMetas.size() ? &view.objectModelMetas[modelId] : nullptr;
}

std::span<const std::uint16_t> CatalogDatabase::ModelTags(std::uint16_t modelId) noexcept
{
    const auto& view = View();
    const ObjectModelMeta* meta = ObjectMeta(modelId);
    if (!meta || meta->firstTag > view.objectModelTags.size() || meta->tagCount > view.objectModelTags.size() - meta->firstTag)
    {
        return {};
    }
    return view.objectModelTags.subspan(meta->firstTag, meta->tagCount);
}

const char* CatalogDatabase::ObjectCategoryName(std::uint16_t category) noexcept
{
    const auto& view = View();
    if (view.objectCategoryNameOffsets.empty())
    {
        return category == 0 ? "None" : "";
    }
    return category < view.objectCategoryNameOffsets.size() ? StringAt(view.objectCategoryNameOffsets[category]) : "";
}

const CatalogDatabase::ObjectModelStats* CatalogDatabase::ObjectStats(std::uint16_t modelId) noexcept
{
    const auto& view = View();
    return modelId < view.objectModelStats.size() ? &view.objectModelStats[modelId] : nullptr;
}

bool CatalogDatabase::ObjectThumbnail(std::uint16_t modelId, ThumbnailTilePayload& out) noexcept
{
    out = {};
    const auto& view = View();
    if (modelId >= view.objectModelMetas.size())
    {
        return false;
    }

    const ObjectModelMeta& meta = view.objectModelMetas[modelId];
    if (!(meta.thumbnailFlags & ObjectThumbnailReady) || meta.thumbnailAtlas == ObjectThumbnailNone
        || meta.thumbnailAtlas >= view.thumbnailAtlases.size())
    {
        return false;
    }

    const ThumbnailAtlas& atlas = view.thumbnailAtlases[meta.thumbnailAtlas];
    if (meta.thumbnailTile >= atlas.tileCount || atlas.dataOffset > g_catalogSizeBytes
        || atlas.compressedSize > g_catalogSizeBytes - atlas.dataOffset)
    {
        return false;
    }

    struct Validation
    {
        std::once_flag once;
        bool valid = false;
    };
    try
    {
        static const auto validations = std::make_unique<Validation[]>(view.thumbnailAtlases.size());
        auto& validation = validations[meta.thumbnailAtlas];
        const auto bytes
            = std::span<const std::uint8_t>(g_catalogBytes + atlas.dataOffset, static_cast<std::size_t>(atlas.compressedSize));
        std::call_once(validation.once,
            [&]() noexcept
            {
                validation.valid = atlas.codec != ThumbnailCodecRawBc3 || Utils::Crc32(bytes) == atlas.crc32;
            });
        if (!validation.valid)
        {
            return false;
        }

        out.bytes = bytes;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    out.codec = atlas.codec;
    out.rawSize = atlas.rawSize;
    out.crc32 = atlas.crc32;
    out.width = atlas.width;
    out.height = atlas.height;
    out.tileSize = atlas.tileSize;
    out.tileIndex = meta.thumbnailTile;
    out.tileCount = atlas.tileCount;
    out.frameIntervalMs = (meta.thumbnailFlags & ObjectThumbnailAnimated) ? meta.thumbnailFrameIntervalMs : 0;
    return true;
}

bool CatalogDatabase::ModelExists(std::uint16_t model_id) noexcept
{
    const auto& view = View();
    return model_id < view.models.size() && view.models[model_id].txdNameIndex != InvalidIndex;
}

bool CatalogDatabase::ModelCollidable(std::uint16_t model_id) noexcept
{
    const auto& view = View();
    return ModelExists(model_id) && (view.models[model_id].flags & ModelFlagCollidable);
}

bool CatalogDatabase::ModelSupportsMaterialColor(std::uint16_t model_id) noexcept
{
    const ObjectModelStats* stats = ObjectStats(model_id);
    return !stats || !(stats->flags & ObjectStatsMaterialColorUnsupported);
}

bool CatalogDatabase::TextureIsLowLod(std::uint16_t texNameIdx) noexcept
{
    const auto& view = View();
    return texNameIdx < view.textureMetas.size() && (view.textureMetas[texNameIdx].flags & TextureFlagLowLod);
}

bool CatalogDatabase::TextureIsSeamless(std::uint16_t texNameIdx, int seamless) noexcept
{
    const auto& view = View();
    if (texNameIdx >= view.textureMetas.size())
    {
        return false;
    }
    const std::uint16_t flags = view.textureMetas[texNameIdx].flags;
    if (seamless == 0)
    {
        return (flags & TextureFlagSeamlessV) != 0;
    }
    if (seamless == 1)
    {
        return (flags & TextureFlagSeamlessH) != 0;
    }
    return (flags & (TextureFlagSeamlessV | TextureFlagSeamlessH)) == (TextureFlagSeamlessV | TextureFlagSeamlessH);
}
