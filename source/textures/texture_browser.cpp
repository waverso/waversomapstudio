// Source Repository: https://github.com/waverso/waversomapstudio

#include "texture_browser.h"

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>

#include "../utils/text_search.h"
#include "CModelInfo.h"
#include "CStreaming.h"
#include "CTxdStore.h"
#include "../catalog/database.h"
#include "../editor/user_data.h"

namespace
{
    bool g_built = false;
    std::vector<std::uint16_t> g_rowToTxd;
    std::vector<int> g_txdToModel;
    std::vector<std::uint16_t> g_dupCount;
    std::vector<std::uint32_t> g_dedupRows;
    std::set<std::uint32_t> g_favorites;
}

#include "texture_browser_cache.inl"
#include "texture_browser_filter.inl"

void TextureBrowser::EnsureIndex()
{
    if (g_built)
    {
        return;
    }
    g_built = true;

    const auto& view = CatalogDatabase::View();

    g_rowToTxd.resize(view.textureRows.size());
    for (std::uint16_t t = 0; t < (std::uint16_t)view.txdInfos.size(); ++t)
    {
        const auto& info = view.txdInfos[t];
        for (std::uint32_t r = 0; r < info.textureCount; ++r)
        {
            g_rowToTxd[info.firstTexture + r] = t;
        }
    }

    g_txdToModel.assign(view.txdInfos.size(), -1);
    for (std::uint16_t id : view.modelIds)
    {
        const auto& model = view.models[id];
        if (model.txdNameIndex != CatalogDatabase::InvalidIndex && g_txdToModel[model.txdNameIndex] < 0)
        {
            g_txdToModel[model.txdNameIndex] = id;
        }
    }

    std::vector<std::uint32_t> best(view.textureMetas.size(), 0xFFFFFFFFu);
    g_dupCount.assign(view.textureMetas.size(), 0);
    for (std::uint32_t r = 0; r < (std::uint32_t)view.textureRows.size(); ++r)
    {
        const auto& row = view.textureRows[r];
        g_dupCount[row.textureNameIndex]++;
        std::uint32_t& b = best[row.textureNameIndex];
        if (b == 0xFFFFFFFFu)
        {
            b = r;
            continue;
        }
        const auto& cur = view.textureRows[b];
        if ((unsigned)row.width * row.height > (unsigned)cur.width * cur.height)
        {
            b = r;
        }
    }

    g_dedupRows.reserve(view.textureMetas.size());
    for (std::uint32_t r = 0; r < (std::uint32_t)view.textureRows.size(); ++r)
    {
        if (best[view.textureRows[r].textureNameIndex] == r)
        {
            g_dedupRows.push_back(r);
        }
    }

    g_thumbnailTexture.assign(view.textureRows.size(), nullptr);
    g_thumbnailRetryAt.assign(view.textureRows.size(), 0);
    RestoreFavorites();
}

int TextureBrowser::FindRow(const char* txdName, const char* textureName)
{
    if (!txdName || !*txdName || !textureName || !*textureName)
    {
        return -1;
    }
    const auto& view = CatalogDatabase::View();
    for (std::size_t t = 0; t < view.txdInfos.size(); ++t)
    {
        if (_stricmp(CatalogDatabase::TxdName(static_cast<std::uint16_t>(t)), txdName) != 0)
        {
            continue;
        }
        const auto& info = view.txdInfos[t];
        const std::size_t end
            = std::min(view.textureRows.size(), static_cast<std::size_t>(info.firstTexture) + info.textureCount);
        for (std::size_t row = info.firstTexture; row < end; ++row)
        {
            if (_stricmp(CatalogDatabase::TexName(view.textureRows[row].textureNameIndex), textureName) == 0)
            {
                return static_cast<int>(row);
            }
        }
    }
    return -1;
}

TextureBrowser::RowInfo TextureBrowser::GetRow(std::uint32_t row)
{
    RowInfo out {};
    EnsureIndex();
    const auto& view = CatalogDatabase::View();
    if (row >= view.textureRows.size())
    {
        return out;
    }
    const auto& r = view.textureRows[row];
    const std::uint16_t txdIdx = g_rowToTxd[row];
    const auto& meta = view.textureMetas[r.textureNameIndex];
    out.textureName = CatalogDatabase::TexName(r.textureNameIndex);
    out.txdName = CatalogDatabase::TxdName(txdIdx);
    out.description = CatalogDatabase::TextureDescription(r.textureNameIndex);
    out.width = r.width;
    out.height = r.height;
    out.sizeKb = r.sizeKiB;
    out.mips = r.mips;
    out.sourceImg = view.txdInfos[txdIdx].sourceArchive;
    out.texNameIdx = r.textureNameIndex;
    out.txdIdx = txdIdx;
    out.category = meta.category;
    out.isLowLod = CatalogDatabase::TextureIsLowLod(r.textureNameIndex);
    out.modelId = g_txdToModel[txdIdx];
    out.dupCount = g_dupCount[r.textureNameIndex];
    return out;
}

const char* TextureBrowser::SourceImgName(int img)
{
    static constexpr const char* Names[CatalogDatabase::ArchiveCount] = { "gta_int.img", "gta3.img", "cutscene.img", "samp.img" };
    return (img >= 0 && img < CatalogDatabase::ArchiveCount) ? Names[img] : "Unknown";
}

int TextureBrowser::TagCount()
{
    return (int)CatalogDatabase::View().categoryOffsets.size();
}

const char* TextureBrowser::TagName(int tag)
{
    return tag >= 0 && tag < TagCount() ? CatalogDatabase::CategoryName((std::uint16_t)tag) : "?";
}

void TextureBrowser::GetTagsSummary(std::uint16_t texNameIdx, char* out, size_t outSz)
{
    if (!outSz)
    {
        return;
    }
    const auto& view = CatalogDatabase::View();
    if (texNameIdx >= view.textureMetas.size() || view.textureMetas[texNameIdx].category == 0)
    {
        snprintf(out, outSz, "None");
    }
    else
    {
        snprintf(out, outSz, "%s", CatalogDatabase::CategoryName(view.textureMetas[texNameIdx].category));
    }
}

void TextureBrowser::GetModelDisplay(int modelId, char* out, size_t outSz)
{
    const auto& view = CatalogDatabase::View();
    if (modelId >= 0 && modelId < (int)view.models.size())
    {
        const auto& model = view.models[modelId];
        if (model.txdNameIndex != CatalogDatabase::InvalidIndex && model.modelNameIndex < view.modelNameOffsets.size())
        {
            snprintf(out, outSz, "%s", CatalogDatabase::ModelName(model.modelNameIndex));
            return;
        }
    }
    snprintf(out, outSz, "Unknown");
}

int TextureBrowser::DimensionRatioCount()
{
    return std::min((int)CatalogDatabase::View().aspectRatios.size(), 16);
}

void TextureBrowser::DimensionRatio(int i, int& w, int& h)
{
    const auto& aspects = CatalogDatabase::View().aspectRatios;
    if (i >= 0 && i < (int)aspects.size())
    {
        w = aspects[i].w;
        h = aspects[i].h;
    }
    else
    {
        w = 0;
        h = 0;
    }
}

const char* TextureBrowser::DimensionRatioName(int i)
{
    static char label[32];
    int w = 0, h = 0;
    DimensionRatio(i, w, h);
    if (w <= 0 || h <= 0)
    {
        return "?";
    }
    snprintf(label, sizeof(label), w == 1 && h == 1 ? "%d:%d (Square)" : "%d:%d", w, h);
    return label;
}

bool TextureBrowser::ModelExists(int modelId)
{
    return modelId >= 0 && modelId < (int)CatalogDatabase::View().models.size()
        && CatalogDatabase::ModelExists((std::uint16_t)modelId);
}

int TextureBrowser::NextValidModel(int modelId)
{
    const auto& ids = CatalogDatabase::View().modelIds;
    if (modelId >= 0xFFFF)
    {
        return modelId;
    }
    auto it = std::upper_bound(ids.begin(), ids.end(), (std::uint16_t)(modelId < 0 ? 0 : modelId));
    return (it != ids.end()) ? (int)*it : modelId;
}

int TextureBrowser::PrevValidModel(int modelId)
{
    const auto& ids = CatalogDatabase::View().modelIds;
    if (modelId <= 0)
    {
        return modelId;
    }
    auto it = std::lower_bound(ids.begin(), ids.end(), (std::uint16_t)modelId);
    if (it == ids.begin())
    {
        return modelId;
    }
    --it;
    return (int)*it;
}

bool TextureBrowser::IsFavorite(std::uint32_t row)
{
    return g_favorites.find(row) != g_favorites.end();
}

void TextureBrowser::RestoreFavorites()
{
    g_favorites.clear();
    if (!g_built || UserData::g_favoriteState.textures.empty())
    {
        return;
    }
    const auto& view = CatalogDatabase::View();
    for (std::size_t t = 0; t < view.txdInfos.size(); ++t)
    {
        const auto& info = view.txdInfos[t];
        const char* library = CatalogDatabase::TxdName(static_cast<std::uint16_t>(t));
        for (std::uint32_t row = info.firstTexture; row < info.firstTexture + info.textureCount; ++row)
        {
            if (UserData::g_favoriteState.textures.contains(
                    UserData::NamedFavorite(library, CatalogDatabase::TexName(view.textureRows[row].textureNameIndex))))
            {
                g_favorites.insert(row);
            }
        }
    }
}

void TextureBrowser::ToggleFavorite(std::uint32_t row)
{
    if (row >= CatalogDatabase::View().textureRows.size())
    {
        return;
    }
    const auto model = GetRow(row);
    const bool enabled
        = UserData::ToggleFavorite(UserData::g_favoriteState.textures, UserData::NamedFavorite(model.txdName, model.textureName));
    const auto& view = CatalogDatabase::View();
    const auto& info = view.txdInfos[g_rowToTxd[row]];
    for (std::uint32_t candidate = info.firstTexture; candidate < info.firstTexture + info.textureCount; ++candidate)
    {
        if (_stricmp(CatalogDatabase::TexName(view.textureRows[candidate].textureNameIndex), model.textureName) != 0)
        {
            continue;
        }
        if (enabled)
        {
            g_favorites.insert(candidate);
        }
        else
        {
            g_favorites.erase(candidate);
        }
    }
}
