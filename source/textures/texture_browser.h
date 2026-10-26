// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../catalog/browser_filter.h"

struct RwTexture;

namespace TextureBrowser
{
    struct RowInfo
    {
        const char* textureName = "";
        const char* txdName = "";
        const char* description = "";
        std::uint16_t width = 0;
        std::uint16_t height = 0;
        std::uint16_t sizeKb = 0;
        std::uint8_t mips = 0;
        std::uint8_t sourceImg = 0;
        std::uint16_t texNameIdx = 0;
        std::uint16_t txdIdx = 0;
        std::uint16_t category = 0;
        bool isLowLod = false;
        int modelId = -1;
        std::uint16_t dupCount = 1;
    };

    enum SortKey
    {
        SortTextureName = 0,
        SortLibName,
        SortSize,
        SortResolution,
        SortMipmaps
    };
    enum SearchField : std::uint8_t
    {
        SearchTexture = 1 << 0,
        SearchLibrary = 1 << 1,
        SearchDescription = 1 << 2,
        SearchDefault = SearchTexture | SearchLibrary | SearchDescription
    };

    struct Filter
    {
        char search[64] = {};
        std::uint8_t sourceMask = 0;
        int tag = -1;
        std::uint32_t tagMask = 0;
        std::uint16_t dimensionMask = 0;
        std::uint8_t searchFields = SearchDefault;
        int seamless = -1;
        bool hideDuplicates = true;
        std::uint8_t lodFilter = BrowserFilter::Hide;

        int mipMin = -1, mipMax = -1;
        int sizeMinKb = -1, sizeMaxKb = -1;
        int wMin = -1, wMax = -1;
        int hMin = -1, hMax = -1;
        int aspectW = 0, aspectH = 0;

        bool favoritesOnly = false;

        int sortKey = SortTextureName;
        bool sortDesc = false;
    };

    void EnsureIndex();
    RowInfo GetRow(std::uint32_t row);
    int FindRow(const char* txdName, const char* textureName);
    void RunFilter(const Filter& f, std::vector<std::uint32_t>& out);

    const char* SourceImgName(int img);
    int TagCount();
    const char* TagName(int tag);
    void GetTagsSummary(std::uint16_t texNameIdx, char* out, size_t outSz);

    int DimensionRatioCount();
    void DimensionRatio(int i, int& w, int& h);
    const char* DimensionRatioName(int i);

    bool ModelExists(int modelId);
    int NextValidModel(int modelId);
    int PrevValidModel(int modelId);

    bool IsFavorite(std::uint32_t row);
    void RestoreFavorites();
    void ToggleFavorite(std::uint32_t row);

    void BeginFrame();
    RwTexture* GetThumb(std::uint32_t row);
    void ReleaseAll();

    void GetModelDisplay(int modelId, char* out, size_t outSz);
}
