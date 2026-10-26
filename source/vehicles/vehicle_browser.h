// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>
#include <vector>

#include "../shared/vehicle_catalog.h"
#include "../catalog/browser_filter.h"

namespace VehicleBrowser
{
    enum SortKey
    {
        SortId,
        SortName,
        SortMaxSpeed,
        SortRadius,
        SortLength,
        SortWidth,
        SortHeight,
        SortSeats
    };
    enum SearchField
    {
        SearchName = 1,
        SearchId = 2,
        SearchModelName = 4,
        SearchDefault = 7
    };

    struct ModelInfo
    {
        int modelId = -1;
        const char* name = "";
        const char* modelName = "";
        int category = 0;
        int seats = 0;
        int maxSpeed = 0;
        int colorSlots = 0;
        int modificationShop = 0;
        float radius = 0;
        float length = 0;
        float width = 0;
        float height = 0;
        bool animation = false;
        bool thumbnailReady = false;
    };

    struct Filter
    {
        char search[64] = {};
        unsigned searchFields = SearchDefault;
        unsigned categoryMask = 0;
        unsigned shopMask = 0;
        int idMin = 400, idMax = 611;
        int seatsMin = 0, seatsMax = 9;
        int speedMin = 43, speedMax = 270;
        std::uint8_t colorSlotsMask = 0;
        float radiusMin = 0.3f, radiusMax = 45.0f;
        float lengthMin = 0.3f, lengthMax = 60.0f;
        float widthMin = 0.3f, widthMax = 65.0f;
        float heightMin = 0.3f, heightMax = 30.0f;
        std::uint8_t animationFilter = BrowserFilter::Any;
        bool favoritesOnly = false;
        int sortKey = SortId;
        bool sortDesc = false;
    };

    ModelInfo GetModel(int modelId);
    int CategoryCount();
    const char* CategoryName(int category);
    void RunFilter(const Filter& filter, std::vector<std::uint32_t>& out);
    bool IsFavorite(int modelId);
    void RestoreFavorites();
    void ToggleFavorite(int modelId);
}
