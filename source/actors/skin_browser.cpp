// Source Repository: https://github.com/waverso/waversomapstudio

#include "skin_browser.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../shared/skin_catalog.h"
#include "../shared/skin_instance.h"
#include "../utils/text_search.h"
#include "../catalog/database.h"
#include "../editor/user_data.h"

namespace
{
    std::array<bool, SkinCatalog::LastId + 1> g_favorites {};
}

SkinBrowser::ModelInfo SkinBrowser::GetModel(int modelId)
{
    ModelInfo model;
    const auto* entry = SkinCatalog::Get(modelId);
    if (!entry)
    {
        return model;
    }
    model.modelId = modelId;
    model.name = entry->name;
    model.modelName = entry->modelName;
    model.category = entry->category;
    model.gender = entry->gender;
    const auto* stats = CatalogDatabase::ObjectStats(static_cast<std::uint16_t>(modelId));
    const auto* meta = CatalogDatabase::ObjectMeta(static_cast<std::uint16_t>(modelId));
    if (stats && meta && (stats->flags & CatalogDatabase::ObjectStatsIndexed))
    {
        std::copy_n(meta->boundsMin, 3, model.boundsMin);
        std::copy_n(meta->boundsMax, 3, model.boundsMax);
        model.radius = stats->radius;
    }
    else
    {
        const auto bounds = SkinInstance::ReferenceBounds(modelId);
        if (bounds.valid)
        {
            model.boundsMin[0] = bounds.minimum.x;
            model.boundsMin[1] = bounds.minimum.y;
            model.boundsMin[2] = bounds.minimum.z;
            model.boundsMax[0] = bounds.maximum.x;
            model.boundsMax[1] = bounds.maximum.y;
            model.boundsMax[2] = bounds.maximum.z;
        }
    }
    model.length = model.boundsMax[1] - model.boundsMin[1];
    model.width = model.boundsMax[0] - model.boundsMin[0];
    model.height = model.boundsMax[2] - model.boundsMin[2];
    if (model.radius <= 0)
    {
        model.radius = std::sqrt(model.length * model.length + model.width * model.width + model.height * model.height) * 0.5f;
    }
    model.thumbnailReady = meta && (meta->thumbnailFlags & CatalogDatabase::ObjectThumbnailReady);
    return model;
}

bool SkinBrowser::IsFavorite(int modelId)
{
    return SkinCatalog::IsValid(modelId) && g_favorites[modelId];
}

void SkinBrowser::ToggleFavorite(int modelId)
{
    if (SkinCatalog::IsValid(modelId))
    {
        UserData::SetFavorite(UserData::g_favoriteState.skins, modelId, !g_favorites[modelId]);
        g_favorites[modelId] = !g_favorites[modelId];
    }
}

void SkinBrowser::RestoreFavorites()
{
    g_favorites.fill(false);
    for (int id : UserData::g_favoriteState.skins)
    {
        if (SkinCatalog::IsValid(id))
        {
            g_favorites[id] = true;
        }
    }
}

void SkinBrowser::RunFilter(const Filter& filter, std::vector<std::uint32_t>& out)
{
    out.clear();
    const Filter defaults;
    const bool filterRadius = filter.radiusMin != defaults.radiusMin || filter.radiusMax != defaults.radiusMax;
    const bool filterHeight = filter.heightMin != defaults.heightMin || filter.heightMax != defaults.heightMax;
    for (int id = SkinCatalog::FirstId; id <= SkinCatalog::LastId; ++id)
    {
        const auto* model = SkinCatalog::Get(id);
        if (!model || id < filter.idMin || id > filter.idMax || (filter.favoritesOnly && !IsFavorite(id))
            || (filter.categoryMask && !(filter.categoryMask & (1u << model->category)))
            || (filter.genderMask && !(filter.genderMask & (1u << model->gender))))
        {
            continue;
        }
        char number[16];
        snprintf(number, sizeof(number), "%d", id);
        if (*filter.search && !((filter.searchFields & SearchName) && TextSearch::ContainsNoCase(model->name, filter.search))
            && !((filter.searchFields & SearchId) && TextSearch::ContainsNoCase(number, filter.search))
            && !((filter.searchFields & SearchModelName) && TextSearch::ContainsNoCase(model->modelName, filter.search)))
        {
            continue;
        }
        if (filterRadius || filterHeight)
        {
            const auto dimensions = GetModel(id);
            if ((filterRadius && (dimensions.radius < filter.radiusMin || dimensions.radius > filter.radiusMax))
                || (filterHeight && (dimensions.height < filter.heightMin || dimensions.height > filter.heightMax)))
            {
                continue;
            }
        }
        out.push_back(id);
    }
    std::sort(out.begin(), out.end(),
        [&](std::uint32_t a, std::uint32_t b)
        {
            int cmp = 0;
            if (filter.sortKey == SortName)
            {
                cmp = _stricmp(SkinCatalog::Get(a)->name, SkinCatalog::Get(b)->name);
            }
            else if (filter.sortKey >= SortRadius)
            {
                const auto x = GetModel(a), y = GetModel(b);
                const float xv[] = { x.radius, x.length, x.width, x.height };
                const float yv[] = { y.radius, y.length, y.width, y.height };
                const int key = std::clamp(filter.sortKey - int(SortRadius), 0, 3);
                cmp = (xv[key] > yv[key]) - (xv[key] < yv[key]);
            }
            if (!cmp)
            {
                cmp = (a > b) - (a < b);
            }
            return filter.sortDesc ? cmp > 0 : cmp < 0;
        });
}
