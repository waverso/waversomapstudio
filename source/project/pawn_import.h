// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <functional>
#include <string_view>

#include "project_document.h"

namespace PawnImport
{
    struct Result
    {
        ProjectDocument::Items items;
        std::string error;
        bool failed = false;
    };

    struct Environment
    {
        std::function<bool(int)> modelExists;
        std::function<bool(int)> pickupModelExists;
    };

    Result Parse(std::string_view source, const Environment& environment);
}
