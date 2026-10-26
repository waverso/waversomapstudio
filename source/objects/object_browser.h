// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>
#include <vector>

#include "../catalog/browser_filter.h"

namespace ObjectBrowser
{
    enum SortKey
    {
        SortId = 0,
        SortName,
        SortLibrary,
        SortRadius,
        SortLength,
        SortWidth,
        SortHeight,
        SortPolyCount,
        SortDffSize
    };

    enum SearchField : std::uint8_t
    {
        SearchName = 1 << 0,
        SearchId = 1 << 1,
        SearchDescription = 1 << 2,
        SearchDefault = SearchName | SearchId | SearchDescription
    };

    enum Behaviour : std::uint16_t
    {
        BehaviourBreakable = 1 << 0,
        BehaviourCollidable = 1 << 1,
        BehaviourAnimation = 1 << 2,
        BehaviourBackface = 1 << 3,
        BehaviourPhysics = 1 << 4,
        BehaviourExplosive = 1 << 5,
        BehaviourUvAnim = 1 << 6
    };

    struct ModelInfo
    {
        int modelId = -1;
        const char* name = "";
        const char* txdName = "";
        const char* description = "";
        const char* colName = "";
        std::uint16_t category = 0;
        std::uint8_t sourceImg = 1;
        std::uint16_t behaviour = 0;
        bool collidable = false;
        bool lowLod = false;
        bool statsIndexed = false;
        bool manualCenter = false;
        bool hasTime = false;
        std::uint8_t timeOn = 0;
        std::uint8_t timeOff = 0;
        bool thumbnailReady = false;
        std::uint32_t dffSizeKb = 0;
        std::uint32_t polyCount = 0;
        std::uint32_t vertexCount = 0;
        std::uint16_t materialCount = 0;
        float radius = 0.0f;
        float length = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float drawDistance = 0.0f;
        float boundsMin[3] = {};
        float boundsMax[3] = {};
    };

    struct FilterLimits
    {
        int idMin = 321, idMax = 19999;
        int dffSizeMinKb = 2, dffSizeMaxKb = 800;
        float radiusMin = 0.0f, radiusMax = 340.0f;
        float lengthMin = 0.0f, lengthMax = 515.0f;
        float widthMin = 0.0f, widthMax = 500.0f;
        float heightMin = 0.0f, heightMax = 470.0f;
        int drawDistanceMin = 5, drawDistanceMax = 1500;
        int polyCountMin = 0, polyCountMax = 0;
    };

    struct Filter
    {
        char search[64] = {};
        std::uint8_t searchFields = SearchDefault;
        std::uint8_t sourceMask = 0;
        std::uint32_t categoryMask = 0;
        int idMin = -1, idMax = -1;
        int dffSizeMinKb = -1, dffSizeMaxKb = -1;
        float radiusMin = -1.0f, radiusMax = -1.0f;
        float lengthMin = -1.0f, lengthMax = -1.0f;
        float widthMin = -1.0f, widthMax = -1.0f;
        float heightMin = -1.0f, heightMax = -1.0f;
        int drawDistanceMin = -1, drawDistanceMax = -1;
        int polyCountMin = -1, polyCountMax = -1;
        std::uint8_t lodFilter = BrowserFilter::Hide;
        std::uint8_t breakableFilter = BrowserFilter::Any;
        std::uint8_t collidableFilter = BrowserFilter::Any;
        std::uint8_t animationFilter = BrowserFilter::Any;
        std::uint8_t uvAnimFilter = BrowserFilter::Any;
        std::uint8_t backfaceFilter = BrowserFilter::Any;
        std::uint8_t timeFilter = BrowserFilter::Any;
        std::uint8_t physicsFilter = BrowserFilter::Any;
        std::uint8_t explosiveFilter = BrowserFilter::Any;
        bool favoritesOnly = false;
        int sortKey = SortId;
        bool sortDesc = false;
    };

    void EnsureIndex();
    int CategoryCount();
    const char* CategoryName(int category);
    FilterLimits GetFilterLimits();
    ModelInfo GetModel(int modelId);
    void RunFilter(const Filter& filter, std::vector<std::uint32_t>& out);

    bool IsFavorite(int modelId);
    void ToggleFavorite(int modelId);
}
