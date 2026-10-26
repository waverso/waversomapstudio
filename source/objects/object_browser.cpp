// Source Repository: https://github.com/waverso/waversomapstudio

#include "object_browser.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <numeric>

#include "../shared/model_checks.h"
#include "../utils/text_search.h"
#include "../catalog/database.h"
#include "../editor/user_data.h"

namespace
{
    static_assert(ObjectBrowser::BehaviourBreakable == CatalogDatabase::ObjectBehaviourBreakable);
    static_assert(ObjectBrowser::BehaviourCollidable == CatalogDatabase::ObjectBehaviourCollidable);
    static_assert(ObjectBrowser::BehaviourAnimation == CatalogDatabase::ObjectBehaviourAnimation);
    static_assert(ObjectBrowser::BehaviourBackface == CatalogDatabase::ObjectBehaviourBackface);
    static_assert(ObjectBrowser::BehaviourPhysics == CatalogDatabase::ObjectBehaviourPhysics);
    static_assert(ObjectBrowser::BehaviourExplosive == CatalogDatabase::ObjectBehaviourExplosive);

    bool g_built = false;
    std::vector<std::uint16_t> g_models;
}

void ObjectBrowser::EnsureIndex()
{
    if (g_built)
    {
        return;
    }
    g_built = true;
    const auto& view = CatalogDatabase::View();
    g_models.assign(view.modelIds.begin(), view.modelIds.end());
}

int ObjectBrowser::CategoryCount()
{
    return std::max(1, static_cast<int>(CatalogDatabase::View().objectCategoryNameOffsets.size()));
}

const char* ObjectBrowser::CategoryName(int category)
{
    return category >= 0 && category < CategoryCount() ? CatalogDatabase::ObjectCategoryName(static_cast<std::uint16_t>(category))
                                                       : "";
}

ObjectBrowser::FilterLimits ObjectBrowser::GetFilterLimits()
{
    FilterLimits limits;
    for (const CatalogDatabase::ObjectModelStats& stats : CatalogDatabase::View().objectModelStats)
    {
        if (!(stats.flags & CatalogDatabase::ObjectStatsIndexed))
        {
            continue;
        }
        limits.polyCountMax = std::max(limits.polyCountMax,
            stats.polyCount > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max()
                                                                                          : static_cast<int>(stats.polyCount));
    }
    return limits;
}

ObjectBrowser::ModelInfo ObjectBrowser::GetModel(int modelId)
{
    ModelInfo result {};
    if (modelId < 0 || modelId >= CatalogDatabase::MaxModelId
        || !CatalogDatabase::ModelExists(static_cast<std::uint16_t>(modelId)))
    {
        return result;
    }
    const auto& view = CatalogDatabase::View();
    const auto& model = view.models[modelId];
    result.modelId = modelId;
    result.name = model.modelNameIndex == CatalogDatabase::InvalidIndex ? "" : CatalogDatabase::ModelName(model.modelNameIndex);
    result.txdName = model.txdNameIndex == CatalogDatabase::InvalidIndex ? "" : CatalogDatabase::TxdName(model.txdNameIndex);
    result.description = CatalogDatabase::ModelDescription(static_cast<std::uint16_t>(modelId));
    result.collidable = (model.flags & CatalogDatabase::ModelFlagCollidable) != 0;
    result.lowLod = (model.flags & CatalogDatabase::ModelFlagLowLod) != 0;
    if (const CatalogDatabase::ObjectModelMeta* meta = CatalogDatabase::ObjectMeta(static_cast<std::uint16_t>(modelId)))
    {
        std::memcpy(result.boundsMin, meta->boundsMin, sizeof(result.boundsMin));
        std::memcpy(result.boundsMax, meta->boundsMax, sizeof(result.boundsMax));
        result.thumbnailReady = (meta->thumbnailFlags & CatalogDatabase::ObjectThumbnailReady) != 0
            && meta->thumbnailAtlas != CatalogDatabase::ObjectThumbnailNone;
    }
    if (const CatalogDatabase::ObjectModelStats* stats = CatalogDatabase::ObjectStats(static_cast<std::uint16_t>(modelId)))
    {
        result.colName = CatalogDatabase::StringAt(stats->colNameOffset);
        result.category = stats->category;
        result.sourceImg = stats->sourceArchive;
        result.behaviour = stats->behaviourFlags;
        result.collidable = (stats->behaviourFlags & CatalogDatabase::ObjectBehaviourCollidable) != 0;
        result.statsIndexed = (stats->flags & CatalogDatabase::ObjectStatsIndexed) != 0;
        result.manualCenter = (stats->flags & CatalogDatabase::ObjectStatsManualCenter) != 0;
        result.hasTime = stats->timeOn != CatalogDatabase::ObjectTimeNone && stats->timeOff != CatalogDatabase::ObjectTimeNone;
        result.timeOn = result.hasTime ? stats->timeOn : 0;
        result.timeOff = result.hasTime ? stats->timeOff : 0;
        result.dffSizeKb = stats->dffSizeKb;
        result.polyCount = stats->polyCount;
        result.vertexCount = stats->vertexCount;
        result.materialCount = stats->materialCount;
        result.radius = stats->radius;
        result.length = stats->length;
        result.width = stats->width;
        result.height = stats->height;
        result.drawDistance = stats->drawDistance;
    }
    else
    {
        result.width = std::max(0.0f, result.boundsMax[0] - result.boundsMin[0]);
        result.length = std::max(0.0f, result.boundsMax[1] - result.boundsMin[1]);
        result.height = std::max(0.0f, result.boundsMax[2] - result.boundsMin[2]);
        result.radius
            = std::sqrt(result.width * result.width + result.length * result.length + result.height * result.height) * 0.5f;
        if (result.collidable)
        {
            result.behaviour |= BehaviourCollidable;
        }
    }
    if (ModelChecks::IsUVAnim(static_cast<unsigned short>(modelId)))
    {
        result.behaviour |= BehaviourUvAnim;
    }
    return result;
}

void ObjectBrowser::RunFilter(const Filter& filter, std::vector<std::uint32_t>& out)
{
    EnsureIndex();
    out.clear();
    const auto visibilityMatches = [](bool value, std::uint8_t mode)
    {
        if (mode == BrowserFilter::Only)
        {
            return value;
        }
        if (mode == BrowserFilter::Hide)
        {
            return !value;
        }
        return true;
    };
    for (std::uint16_t id : g_models)
    {
        const ModelInfo model = GetModel(id);
        if (filter.idMin >= 0 && id < filter.idMin)
        {
            continue;
        }
        if (filter.idMax >= 0 && id > filter.idMax)
        {
            continue;
        }
        if (!visibilityMatches(model.lowLod, filter.lodFilter))
        {
            continue;
        }
        if (!visibilityMatches((model.behaviour & BehaviourBreakable) != 0, filter.breakableFilter))
        {
            continue;
        }
        if (!visibilityMatches((model.behaviour & BehaviourCollidable) != 0, filter.collidableFilter))
        {
            continue;
        }
        if (!visibilityMatches((model.behaviour & BehaviourAnimation) != 0, filter.animationFilter))
        {
            continue;
        }
        if (!visibilityMatches((model.behaviour & BehaviourUvAnim) != 0, filter.uvAnimFilter))
        {
            continue;
        }
        if (!visibilityMatches((model.behaviour & BehaviourBackface) != 0, filter.backfaceFilter))
        {
            continue;
        }
        if (!visibilityMatches(model.hasTime, filter.timeFilter))
        {
            continue;
        }
        if (!visibilityMatches((model.behaviour & BehaviourPhysics) != 0, filter.physicsFilter))
        {
            continue;
        }
        if (!visibilityMatches((model.behaviour & BehaviourExplosive) != 0, filter.explosiveFilter))
        {
            continue;
        }
        if (filter.favoritesOnly && !IsFavorite(id))
        {
            continue;
        }
        if (filter.sourceMask
            && (model.sourceImg >= CatalogDatabase::ArchiveCount || !(filter.sourceMask & (1u << model.sourceImg))))
        {
            continue;
        }
        if (filter.categoryMask)
        {
            if (model.category >= 32 || !(filter.categoryMask & (1u << model.category)))
            {
                continue;
            }
        }
        if (filter.dffSizeMinKb >= 0 && model.dffSizeKb < static_cast<std::uint32_t>(filter.dffSizeMinKb))
        {
            continue;
        }
        if (filter.dffSizeMaxKb >= 0 && model.dffSizeKb > static_cast<std::uint32_t>(filter.dffSizeMaxKb))
        {
            continue;
        }
        if (filter.radiusMin >= 0.0f && model.radius < filter.radiusMin)
        {
            continue;
        }
        if (filter.radiusMax >= 0.0f && model.radius > filter.radiusMax)
        {
            continue;
        }
        if (filter.lengthMin >= 0.0f && model.length < filter.lengthMin)
        {
            continue;
        }
        if (filter.lengthMax >= 0.0f && model.length > filter.lengthMax)
        {
            continue;
        }
        if (filter.widthMin >= 0.0f && model.width < filter.widthMin)
        {
            continue;
        }
        if (filter.widthMax >= 0.0f && model.width > filter.widthMax)
        {
            continue;
        }
        if (filter.heightMin >= 0.0f && model.height < filter.heightMin)
        {
            continue;
        }
        if (filter.heightMax >= 0.0f && model.height > filter.heightMax)
        {
            continue;
        }
        if (filter.drawDistanceMin >= 0 && model.drawDistance < filter.drawDistanceMin)
        {
            continue;
        }
        if (filter.drawDistanceMax >= 0 && model.drawDistance > filter.drawDistanceMax)
        {
            continue;
        }
        const bool filtersPolyCount = filter.polyCountMin >= 0 || filter.polyCountMax >= 0;
        if (filtersPolyCount && !model.statsIndexed)
        {
            continue;
        }
        if (filter.polyCountMin >= 0 && model.polyCount < static_cast<std::uint32_t>(filter.polyCountMin))
        {
            continue;
        }
        if (filter.polyCountMax >= 0 && model.polyCount > static_cast<std::uint32_t>(filter.polyCountMax))
        {
            continue;
        }
        if (filter.search[0])
        {
            char idText[12];
            snprintf(idText, sizeof(idText), "%u", id);
            std::uint8_t fields = filter.searchFields & SearchDefault;
            if (!fields)
            {
                fields = SearchName;
            }
            bool found = false;
            if (fields & SearchName)
            {
                found = found || TextSearch::ContainsAsciiNoCase(model.name, filter.search);
            }
            if (fields & SearchId)
            {
                found = found || TextSearch::ContainsAsciiNoCase(idText, filter.search);
            }
            if (fields & SearchDescription)
            {
                found = found || TextSearch::ContainsAsciiNoCase(model.description, filter.search);
            }
            if (!found)
            {
                continue;
            }
        }
        out.push_back(id);
    }
    if (out.size() > 1)
    {
        std::vector<ModelInfo> infos;
        infos.reserve(out.size());
        for (std::uint32_t id : out)
        {
            infos.push_back(GetModel(static_cast<int>(id)));
        }
        std::vector<std::uint32_t> order(out.size());
        std::iota(order.begin(), order.end(), 0u);
        std::sort(order.begin(), order.end(),
            [&](std::uint32_t i, std::uint32_t j)
            {
                const ModelInfo& ma = infos[i];
                const ModelInfo& mb = infos[j];
                int result = 0;
                auto compareFloat = [](float lhs, float rhs)
                {
                    return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
                };
                auto compareUInt = [](std::uint32_t lhs, std::uint32_t rhs)
                {
                    return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
                };
                switch (filter.sortKey)
                {
                    case SortName:
                        result = std::strcmp(ma.name, mb.name);
                        break;
                    case SortLibrary:
                        result = std::strcmp(ma.txdName, mb.txdName);
                        break;
                    case SortRadius:
                        result = compareFloat(ma.radius, mb.radius);
                        break;
                    case SortLength:
                        result = compareFloat(ma.length, mb.length);
                        break;
                    case SortWidth:
                        result = compareFloat(ma.width, mb.width);
                        break;
                    case SortHeight:
                        result = compareFloat(ma.height, mb.height);
                        break;
                    case SortPolyCount:
                        result = compareUInt(ma.polyCount, mb.polyCount);
                        break;
                    case SortDffSize:
                        result = compareUInt(ma.dffSizeKb, mb.dffSizeKb);
                        break;
                    case SortId:
                    default:
                        result = compareUInt(out[i], out[j]);
                        break;
                }
                if (!result)
                {
                    result = out[i] < out[j] ? -1 : (out[i] > out[j] ? 1 : 0);
                }
                return filter.sortDesc ? result > 0 : result < 0;
            });
        std::vector<std::uint32_t> sorted(out.size());
        for (std::size_t k = 0; k < order.size(); ++k)
        {
            sorted[k] = out[order[k]];
        }
        out.swap(sorted);
    }
}

bool ObjectBrowser::IsFavorite(int modelId)
{
    return UserData::g_favoriteState.objects.contains(modelId);
}

void ObjectBrowser::ToggleFavorite(int modelId)
{
    if (modelId < 0 || modelId >= CatalogDatabase::MaxModelId
        || !CatalogDatabase::ModelExists(static_cast<std::uint16_t>(modelId)))
    {
        return;
    }
    UserData::ToggleFavorite(UserData::g_favoriteState.objects, modelId);
}
