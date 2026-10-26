// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include "project_document.h"

namespace PawnExport
{
    inline constexpr int MaxNativeObjects = 1000;
    inline constexpr int MaxVehicles = 1999;
    inline constexpr int MaxRemovalInstructions = 1000;
    inline constexpr int MaxGangZones = 1024;
    inline constexpr int MaxNativePickups = 4096;
    inline constexpr int MaxNativeActors = 1000;

    enum Category : unsigned
    {
        Actors = 1u << 0,
        Objects = 1u << 1,
        Vehicles = 1u << 2,
        Pickups = 1u << 3,
        Removals = 1u << 4,
        BoxAreas = 1u << 5,
        Races = 1u << 6
    };

    enum class LoopStyle
    {
        For,
        Foreach,
        YIterate
    };

    struct Options
    {
        bool openMp = false;
        bool streamer = true;
        LoopStyle loopStyle = LoopStyle::For;
        bool yTimers = false;
        bool comments = true;
        unsigned categories = 0;

        bool Includes(Category category) const
        {
            return !categories || (categories & category) != 0;
        }
    };

    std::string Build(const ProjectDocument::Items& worldItems, const Options& options);
}
