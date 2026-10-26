// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cctype>

namespace TextSearch
{
    inline bool ContainsAsciiNoCase(const char* text, const char* needle) noexcept
    {
        if (!needle || !needle[0])
        {
            return true;
        }
        if (!text)
        {
            return false;
        }
        const auto lower = [](char value)
        {
            return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
        };
        for (const char* start = text; *start; ++start)
        {
            const char* a = start;
            const char* b = needle;
            while (*a && *b && lower(*a) == lower(*b))
            {
                ++a;
                ++b;
            }
            if (!*b)
            {
                return true;
            }
        }
        return false;
    }

    inline bool ContainsNoCase(const char* text, const char* query)
    {
        if (!*query)
        {
            return true;
        }
        for (; *text; ++text)
        {
            const char* a = text;
            const char* b = query;
            while (*a && *b && std::tolower(static_cast<unsigned char>(*a)) == std::tolower(static_cast<unsigned char>(*b)))
            {
                ++a;
                ++b;
            }
            if (!*b)
            {
                return true;
            }
        }
        return false;
    }
}
