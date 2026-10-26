// Source Repository: https://github.com/waverso/waversomapstudio

#include "vehicle_browser.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "../shared/vehicle_catalog.h"
#include "../utils/text_search.h"
#include "../catalog/database.h"
#include "../editor/user_data.h"

namespace
{
    std::array<bool, VehicleCatalog::Entries.size()> g_favorites {};

    bool VisibilityMatches(bool value, std::uint8_t filter)
    {
        return filter == BrowserFilter::Any || (filter == BrowserFilter::Only ? value : !value);
    }
}

VehicleBrowser::ModelInfo VehicleBrowser::GetModel(int modelId)
{
    ModelInfo model;
    const auto* entry = VehicleCatalog::Get(modelId);
    if (!entry)
    {
        return model;
    }
    model.modelId = modelId;
    model.name = entry->name;
    model.modelName = entry->modelName;
    model.category = entry->category;
    model.seats = entry->seats;
    model.maxSpeed = entry->maxSpeed;
    model.colorSlots = entry->colorSlots;
    model.modificationShop = entry->modificationShop;
    if (const auto* stats = CatalogDatabase::ObjectStats(static_cast<std::uint16_t>(modelId)))
    {
        model.radius = stats->radius;
        model.length = stats->length;
        model.width = stats->width;
        model.height = stats->height;
        model.animation = (stats->behaviourFlags & CatalogDatabase::ObjectBehaviourAnimation) != 0;
    }
    if (const auto* meta = CatalogDatabase::ObjectMeta(static_cast<std::uint16_t>(modelId)))
    {
        model.thumbnailReady = (meta->thumbnailFlags & CatalogDatabase::ObjectThumbnailReady) != 0;
    }
    return model;
}

int VehicleBrowser::CategoryCount()
{
    return static_cast<int>(std::size(VehicleCatalog::Categories));
}

const char* VehicleBrowser::CategoryName(int category)
{
    return category >= 0 && category < CategoryCount() ? VehicleCatalog::Categories[category] : "";
}

bool VehicleBrowser::IsFavorite(int modelId)
{
    return VehicleCatalog::IsValid(modelId) && g_favorites[modelId - VehicleCatalog::FirstId];
}

void VehicleBrowser::ToggleFavorite(int modelId)
{
    if (VehicleCatalog::IsValid(modelId))
    {
        auto& favorite = g_favorites[modelId - VehicleCatalog::FirstId];
        UserData::SetFavorite(UserData::g_favoriteState.vehicles, modelId, !favorite);
        favorite = !favorite;
    }
}

void VehicleBrowser::RestoreFavorites()
{
    g_favorites.fill(false);
    for (int id : UserData::g_favoriteState.vehicles)
    {
        if (VehicleCatalog::IsValid(id))
        {
            g_favorites[id - VehicleCatalog::FirstId] = true;
        }
    }
}

void VehicleBrowser::RunFilter(const Filter& filter, std::vector<std::uint32_t>& out)
{
    out.clear();
    for (int id = VehicleCatalog::FirstId; id <= VehicleCatalog::LastId; ++id)
    {
        const auto model = GetModel(id);
        const unsigned colorSlotBit
            = model.colorSlots >= 0 && model.colorSlots < 8 ? 1u << static_cast<unsigned>(model.colorSlots) : 0;
        if (id < filter.idMin || id > filter.idMax || (filter.favoritesOnly && !IsFavorite(id))
            || (filter.categoryMask && !(filter.categoryMask & (1u << model.category)))
            || (filter.shopMask && !(filter.shopMask & (1u << model.modificationShop))) || model.seats < filter.seatsMin
            || model.seats > filter.seatsMax || model.maxSpeed < filter.speedMin || model.maxSpeed > filter.speedMax
            || (filter.colorSlotsMask && !(filter.colorSlotsMask & colorSlotBit)) || model.radius < filter.radiusMin
            || model.radius > filter.radiusMax || model.length < filter.lengthMin || model.length > filter.lengthMax
            || model.width < filter.widthMin || model.width > filter.widthMax || model.height < filter.heightMin
            || model.height > filter.heightMax || !VisibilityMatches(model.animation, filter.animationFilter))
        {
            continue;
        }
        char number[16];
        snprintf(number, sizeof(number), "%d", id);
        if (*filter.search && !((filter.searchFields & SearchName) && TextSearch::ContainsNoCase(model.name, filter.search))
            && !((filter.searchFields & SearchId) && TextSearch::ContainsNoCase(number, filter.search))
            && !((filter.searchFields & SearchModelName) && TextSearch::ContainsNoCase(model.modelName, filter.search)))
        {
            continue;
        }
        out.push_back(id);
    }
    std::sort(out.begin(), out.end(),
        [&](std::uint32_t a, std::uint32_t b)
        {
            const auto x = GetModel(a), y = GetModel(b);
            int cmp = 0;
            const auto compare = [](auto a, auto b)
            {
                return (a > b) - (a < b);
            };
            switch (filter.sortKey)
            {
                case SortName:
                    cmp = _stricmp(x.name, y.name);
                    break;
                case SortMaxSpeed:
                    cmp = compare(x.maxSpeed, y.maxSpeed);
                    break;
                case SortRadius:
                    cmp = compare(x.radius, y.radius);
                    break;
                case SortLength:
                    cmp = compare(x.length, y.length);
                    break;
                case SortWidth:
                    cmp = compare(x.width, y.width);
                    break;
                case SortHeight:
                    cmp = compare(x.height, y.height);
                    break;
                case SortSeats:
                    cmp = compare(x.seats, y.seats);
                    break;
                default:
                    break;
            }
            if (!cmp)
            {
                cmp = compare(a, b);
            }
            return filter.sortDesc ? cmp > 0 : cmp < 0;
        });
}
