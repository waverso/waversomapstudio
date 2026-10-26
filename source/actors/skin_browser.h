// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>
#include <vector>

#include "../shared/skin_catalog.h"

namespace SkinBrowser
{
    enum SortKey
    {
        SortId,
        SortName,
        SortRadius,
        SortLength,
        SortWidth,
        SortHeight
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
        int gender = 0;
        float radius = 0;
        float length = 0;
        float width = 0;
        float height = 0;
        float boundsMin[3] = {};
        float boundsMax[3] = {};
        bool thumbnailReady = false;
    };

    struct Filter
    {
        char search[64] = {};
        unsigned searchFields = SearchDefault;
        unsigned categoryMask = 0;
        unsigned genderMask = 0;
        int idMin = 0, idMax = 311;
        float radiusMin = 0.8f, radiusMax = 1.1f;
        float heightMin = 1.5f, heightMax = 2.1f;
        bool favoritesOnly = false;
        int sortKey = SortId;
        bool sortDesc = false;
    };

    ModelInfo GetModel(int modelId);
    void RunFilter(const Filter& filter, std::vector<std::uint32_t>& out);
    bool IsFavorite(int modelId);
    void RestoreFavorites();
    void ToggleFavorite(int modelId);
}
