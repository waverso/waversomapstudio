// Source Repository: https://github.com/waverso/waversomapstudio

#include "model_thumbnails.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <d3d9.h>

#include "CTimer.h"
#include "../catalog/database.h"

namespace
{
    constexpr std::uint16_t ThumbnailSize = 64;
    constexpr std::uint16_t PageAtlasWidth = 1024;
    constexpr std::uint16_t PageAtlasHeight = 512;
    constexpr std::size_t PageAtlasColumns = PageAtlasWidth / ThumbnailSize;
    constexpr std::size_t PageAtlasRows = PageAtlasHeight / ThumbnailSize;
    constexpr std::size_t PageAtlasCapacity = PageAtlasColumns * PageAtlasRows;
    constexpr std::size_t MaximumVisibleThumbnails = 90;
    constexpr std::size_t PageAtlasCount = 3;
    static_assert(MaximumVisibleThumbnails <= PageAtlasCapacity);

    struct PageAtlas
    {
        IDirect3DTexture9* texture = nullptr;
        std::array<std::uint32_t, MaximumVisibleThumbnails> modelIds {};
        std::array<bool, MaximumVisibleThumbnails> ready {};
        std::array<std::uint16_t, MaximumVisibleThumbnails> frameIndex {};
        std::array<std::uint16_t, MaximumVisibleThumbnails> frameIntervalMs {};
        std::array<std::uint32_t, MaximumVisibleThumbnails> frameUploadedMs {};
        std::uint64_t hash = 0;
        std::size_t count = 0;
        bool valid = false;
        bool usedThisFrame = false;
    };
    IDirect3DDevice9* g_device = nullptr;
    std::array<PageAtlas, PageAtlasCount> g_pageAtlases;
    int g_activePageAtlas = -1;
    std::size_t g_nextPageAtlas = 0;
    bool g_pageAtlasCreationFailed = false;

    void ReleasePageAtlasesNow()
    {
        for (PageAtlas& page : g_pageAtlases)
        {
            if (page.texture)
            {
                page.texture->Release();
            }
            page = {};
        }
        g_activePageAtlas = -1;
        g_nextPageAtlas = 0;
        g_pageAtlasCreationFailed = false;
    }

    bool EnsurePageAtlases()
    {
        if (!g_device || g_pageAtlasCreationFailed)
        {
            return false;
        }
        bool complete = true;
        for (const PageAtlas& page : g_pageAtlases)
        {
            complete = complete && page.texture;
        }
        if (complete)
        {
            return true;
        }

        ReleasePageAtlasesNow();
        for (PageAtlas& page : g_pageAtlases)
        {
            if (FAILED(g_device->CreateTexture(
                    PageAtlasWidth, PageAtlasHeight, 1, 0, D3DFMT_DXT5, D3DPOOL_MANAGED, &page.texture, nullptr))
                || !page.texture)
            {
                ReleasePageAtlasesNow();
                g_pageAtlasCreationFailed = true;
                return false;
            }
        }
        return true;
    }

    std::uint64_t HashThumbnailPage(const std::uint32_t* modelIds, std::size_t count)
    {
        std::uint64_t hash = 14695981039346656037ull;
        const auto mixByte = [&](std::uint8_t value)
        {
            hash ^= value;
            hash *= 1099511628211ull;
        };
        for (std::size_t i = 0; i < count; ++i)
        {
            const std::uint32_t id = modelIds[i];
            mixByte(static_cast<std::uint8_t>(id));
            mixByte(static_cast<std::uint8_t>(id >> 8));
            mixByte(static_cast<std::uint8_t>(id >> 16));
            mixByte(static_cast<std::uint8_t>(id >> 24));
        }
        mixByte(static_cast<std::uint8_t>(count));
        mixByte(static_cast<std::uint8_t>(count >> 8));
        return hash;
    }

    bool PageMatches(const PageAtlas& page, std::uint64_t hash, const std::uint32_t* modelIds, std::size_t count)
    {
        return page.valid && page.hash == hash && page.count == count
            && (count == 0 || std::equal(modelIds, modelIds + count, page.modelIds.begin()));
    }

    bool CopyRawBc3Thumbnail(
        const CatalogDatabase::ThumbnailTilePayload& source, std::uint8_t* destination, std::size_t destinationPitch)
    {
        if (source.codec != CatalogDatabase::ThumbnailCodecRawBc3 || source.tileSize != ThumbnailSize || !source.width
            || !source.height || source.width % 4 || source.height % 4 || source.width % source.tileSize
            || source.height % source.tileSize || source.rawSize != static_cast<std::uint32_t>(source.width) * source.height
            || source.bytes.size() != source.rawSize || source.tileIndex >= source.tileCount)
        {
            return false;
        }

        const std::size_t columns = source.width / source.tileSize;
        const std::size_t rows = source.height / source.tileSize;
        if (!columns || source.tileIndex >= columns * rows)
        {
            return false;
        }

        constexpr std::size_t blockBytes = 16;
        constexpr std::size_t tileBlockRows = ThumbnailSize / 4;
        constexpr std::size_t tileRowBytes = (ThumbnailSize / 4) * blockBytes;
        const std::size_t sourcePitch = (source.width / 4) * blockBytes;
        const std::size_t tileX = source.tileIndex % columns;
        const std::size_t tileY = source.tileIndex / columns;
        const std::size_t sourceOffset = tileY * tileBlockRows * sourcePitch + tileX * tileRowBytes;
        const std::size_t requiredBytes = sourceOffset + (tileBlockRows - 1) * sourcePitch + tileRowBytes;
        if (requiredBytes > source.bytes.size())
        {
            return false;
        }

        for (std::size_t row = 0; row < tileBlockRows; ++row)
        {
            std::memcpy(
                destination + row * destinationPitch, source.bytes.data() + sourceOffset + row * sourcePitch, tileRowBytes);
        }
        return true;
    }

    bool CopyThumbnail(
        const CatalogDatabase::ThumbnailTilePayload& source, std::uint8_t* destination, std::size_t destinationPitch)
    {
        switch (source.codec)
        {
            case CatalogDatabase::ThumbnailCodecRawBc3:
                return CopyRawBc3Thumbnail(source, destination, destinationPitch);
            case CatalogDatabase::ThumbnailCodecZstdBc3:
                return false;
            default:
                return false;
        }
    }

    bool UploadThumbnailPage(PageAtlas& page, const std::uint32_t* modelIds, std::size_t count, std::uint64_t hash)
    {
        if (!page.texture)
        {
            return false;
        }
        D3DLOCKED_RECT lock {};
        if (FAILED(page.texture->LockRect(0, &lock, nullptr, 0)))
        {
            return false;
        }
        if (!lock.pBits || lock.Pitch < static_cast<INT>(PageAtlasWidth * 4u))
        {
            page.texture->UnlockRect(0);
            return false;
        }

        auto* pixels = static_cast<std::uint8_t*>(lock.pBits);
        constexpr std::size_t compressedRows = PageAtlasHeight / 4;
        constexpr std::size_t rowBytes = PageAtlasWidth * 4u;
        for (std::size_t row = 0; row < compressedRows; ++row)
        {
            std::memset(pixels + row * lock.Pitch, 0, rowBytes);
        }

        page.ready.fill(false);
        page.frameIntervalMs.fill(0);
        const unsigned uploadMs = static_cast<unsigned>(CTimer::m_snTimeInMilliseconds);
        for (std::size_t slot = 0; slot < page.frameIndex.size(); ++slot)
        {
            page.frameIndex[slot] = 0;
            page.frameUploadedMs[slot] = uploadMs;
        }
        for (std::size_t slot = 0; slot < count; ++slot)
        {
            CatalogDatabase::ThumbnailTilePayload source;
            if (!CatalogDatabase::ObjectThumbnail(static_cast<std::uint16_t>(modelIds[slot]), source))
            {
                continue;
            }

            constexpr std::size_t tileBlockRows = ThumbnailSize / 4;
            constexpr std::size_t tileRowBytes = ThumbnailSize * 4u;
            const std::size_t tileX = slot % PageAtlasColumns;
            const std::size_t tileY = slot / PageAtlasColumns;
            std::uint8_t* destination = pixels + tileY * tileBlockRows * lock.Pitch + tileX * tileRowBytes;
            page.ready[slot] = CopyThumbnail(source, destination, lock.Pitch);
            if (source.tileCount > 1)
            {
                page.frameIntervalMs[slot] = source.frameIntervalMs ? source.frameIntervalMs : 100;
            }
        }

        const HRESULT unlockResult = page.texture->UnlockRect(0);
        if (FAILED(unlockResult))
        {
            page.valid = false;
            return false;
        }

        page.count = count;
        page.hash = hash;
        page.valid = true;
        if (count)
        {
            std::copy_n(modelIds, count, page.modelIds.begin());
        }
        return true;
    }

    void UpdateAnimatedPageTiles(PageAtlas& page)
    {
        if (!page.valid || !page.texture || !page.count)
        {
            return;
        }
        const unsigned nowMs = static_cast<unsigned>(CTimer::m_snTimeInMilliseconds);

        struct Pending
        {
            std::size_t slot;
            std::uint16_t frame;
        };
        Pending pending[MaximumVisibleThumbnails];
        std::size_t pendingCount = 0;
        for (std::size_t slot = 0; slot < page.count; ++slot)
        {
            const std::uint16_t interval = page.frameIntervalMs[slot];
            if (!interval)
            {
                continue;
            }
            const unsigned elapsed = nowMs - page.frameUploadedMs[slot];
            if (elapsed < interval)
            {
                continue;
            }
            const auto modelId = static_cast<std::uint16_t>(page.modelIds[slot]);
            const CatalogDatabase::ObjectModelMeta* meta = CatalogDatabase::ObjectMeta(modelId);
            if (!meta || !(meta->thumbnailFlags & CatalogDatabase::ObjectThumbnailAnimated))
            {
                continue;
            }
            CatalogDatabase::ThumbnailTilePayload payload;
            if (!CatalogDatabase::ObjectThumbnail(modelId, payload) || payload.tileCount < 2)
            {
                continue;
            }
            const std::uint16_t next = static_cast<std::uint16_t>((page.frameIndex[slot] + 1) % payload.tileCount);
            pending[pendingCount++] = { slot, next };
        }
        if (!pendingCount)
        {
            return;
        }

        D3DLOCKED_RECT lock {};
        if (FAILED(page.texture->LockRect(0, &lock, nullptr, 0)))
        {
            return;
        }
        if (lock.pBits && lock.Pitch >= static_cast<INT>(PageAtlasWidth * 4u))
        {
            auto* pixels = static_cast<std::uint8_t*>(lock.pBits);
            constexpr std::size_t tileBlockRows = ThumbnailSize / 4;
            constexpr std::size_t tileRowBytes = ThumbnailSize * 4u;
            for (std::size_t i = 0; i < pendingCount; ++i)
            {
                const std::size_t slot = pending[i].slot;
                CatalogDatabase::ThumbnailTilePayload payload;
                if (!CatalogDatabase::ObjectThumbnail(static_cast<std::uint16_t>(page.modelIds[slot]), payload))
                {
                    continue;
                }
                payload.tileIndex = pending[i].frame;
                if (payload.tileIndex >= payload.tileCount)
                {
                    continue;
                }
                const std::size_t tileX = slot % PageAtlasColumns;
                const std::size_t tileY = slot / PageAtlasColumns;
                std::uint8_t* destination = pixels + tileY * tileBlockRows * lock.Pitch + tileX * tileRowBytes;
                if (!CopyThumbnail(payload, destination, lock.Pitch))
                {
                    continue;
                }
                page.frameIndex[slot] = pending[i].frame;
                page.frameUploadedMs[slot] = nowMs;
            }
        }
        page.texture->UnlockRect(0);
    }
}

void ModelThumbnails::BeginFrame(IDirect3DDevice9* device)
{
    for (PageAtlas& page : g_pageAtlases)
    {
        page.usedThisFrame = false;
    }
    if (device != g_device)
    {
        ReleasePageAtlasesNow();
        g_device = device;
    }
    if (g_device && g_device->TestCooperativeLevel() == D3D_OK)
    {
        EnsurePageAtlases();
        if (g_activePageAtlas >= 0 && g_activePageAtlas < static_cast<int>(g_pageAtlases.size()))
        {
            UpdateAnimatedPageTiles(g_pageAtlases[g_activePageAtlas]);
        }
    }
}

void ModelThumbnails::PreparePage(const std::uint32_t* modelIds, std::size_t count)
{
    count = std::min(count, MaximumVisibleThumbnails);
    if ((count && !modelIds) || !EnsurePageAtlases())
    {
        g_activePageAtlas = -1;
        return;
    }

    const std::uint64_t hash = HashThumbnailPage(modelIds, count);
    for (std::size_t i = 0; i < g_pageAtlases.size(); ++i)
    {
        if (PageMatches(g_pageAtlases[i], hash, modelIds, count))
        {
            g_activePageAtlas = static_cast<int>(i);
            return;
        }
    }

    std::size_t destination = g_nextPageAtlas % g_pageAtlases.size();
    for (std::size_t attempt = 0; attempt < g_pageAtlases.size(); ++attempt)
    {
        const PageAtlas& candidate = g_pageAtlases[destination];
        if (!candidate.usedThisFrame)
        {
            break;
        }
        destination = (destination + 1) % g_pageAtlases.size();
    }
    if (g_pageAtlases[destination].usedThisFrame)
    {
        g_activePageAtlas = -1;
        return;
    }
    g_nextPageAtlas = (destination + 1) % g_pageAtlases.size();

    PageAtlas& page = g_pageAtlases[destination];
    page.valid = false;
    if (UploadThumbnailPage(page, modelIds, count, hash))
    {
        g_activePageAtlas = static_cast<int>(destination);
    }
    else
    {
        g_activePageAtlas = -1;
    }
}

bool ModelThumbnails::Get(int modelId, View& out)
{
    out = {};
    if (modelId < 0 || modelId >= CatalogDatabase::MaxModelId || g_activePageAtlas < 0
        || g_activePageAtlas >= static_cast<int>(g_pageAtlases.size()))
    {
        return false;
    }

    PageAtlas& page = g_pageAtlases[g_activePageAtlas];
    if (!page.valid || !page.texture)
    {
        return false;
    }
    for (std::size_t slot = 0; slot < page.count; ++slot)
    {
        if (page.modelIds[slot] != static_cast<std::uint32_t>(modelId))
        {
            continue;
        }
        if (!page.ready[slot])
        {
            return false;
        }

        page.usedThisFrame = true;
        const float x = static_cast<float>((slot % PageAtlasColumns) * ThumbnailSize);
        const float y = static_cast<float>((slot / PageAtlasColumns) * ThumbnailSize);
        out.texture = page.texture;
        out.uvMin[0] = (x + 0.5f) / static_cast<float>(PageAtlasWidth);
        out.uvMin[1] = (y + 0.5f) / static_cast<float>(PageAtlasHeight);
        out.uvMax[0] = (x + ThumbnailSize - 0.5f) / static_cast<float>(PageAtlasWidth);
        out.uvMax[1] = (y + ThumbnailSize - 0.5f) / static_cast<float>(PageAtlasHeight);
        return true;
    }
    return false;
}

void ModelThumbnails::Shutdown()
{
    ReleasePageAtlasesNow();
    g_device = nullptr;
}
