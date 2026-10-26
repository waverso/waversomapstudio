// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace ProjectFilename
{
    inline bool Normalize(std::string_view input, std::string& name, std::string& error)
    {
        error.clear();
        const auto first = input.find_first_not_of(' ');
        name = first == std::string_view::npos ? "" : std::string(input.substr(first));
        if (name.empty())
        {
            error = "Enter a file name.";
            return false;
        }
        if (name.size() > 80)
        {
            error = "The editor supports names up to 80 UTF-8 bytes.";
            return false;
        }
        if (name.back() == ' ' || name.back() == '.')
        {
            error = "Windows does not allow file names ending in a space or period.";
            return false;
        }
        if (name.find_first_of("<>:\"/\\|?*") != std::string::npos
            || std::any_of(name.begin(), name.end(),
                [](unsigned char c)
                {
                    return c < 32;
                }))
        {
            error = "Windows does not allow < > : \" / \\ | ? * or control characters in file names.";
            return false;
        }
        std::string base = name.substr(0, name.find('.'));
        while (!base.empty() && base.back() == ' ')
        {
            base.pop_back();
        }
        std::transform(base.begin(), base.end(), base.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::toupper(c));
            });
        const bool port = base.starts_with("COM") || base.starts_with("LPT");
        const auto number = port ? base.substr(3) : "";
        if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL"
            || (port
                && ((number.size() == 1 && number[0] >= '1' && number[0] <= '9') || number == "\xC2\xB9" || number == "\xC2\xB2"
                    || number == "\xC2\xB3")))
        {
            error = "This file name is reserved by Windows. Choose a different name.";
            return false;
        }
        return true;
    }
}
