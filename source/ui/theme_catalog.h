// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace Theme
{
    enum class Index : unsigned
    {
        Dark,
        Light,
        Basalt,
        Carbon,
        CatppuccinLatte,
        CatppuccinMocha,
        Cocoa,
        Dracula,
        Evergreen,
        Graphite,
        GruvboxDark,
        GruvboxLight,
        Ironwood,
        Kanagawa,
        Midnight,
        Monokai,
        Moonstone,
        NightOwl,
        Nord,
        OneDark,
        Orchid,
        Rose,
        RosePine,
        Sand,
        SoftGray,
        SolarizedDark,
        SolarizedLight,
        Synthwave,
        TokyoNight,
        Violet,
        Walnut,
        PresetCount
    };

    inline constexpr const char* Names[] = {
        "Default Dark",
        "Default Light",
        "Basalt",
        "Carbon",
        "Catppuccin Latte",
        "Catppuccin Mocha",
        "Cocoa",
        "Dracula",
        "Evergreen",
        "Graphite",
        "Gruvbox Dark",
        "Gruvbox Light",
        "Ironwood",
        "Kanagawa",
        "Midnight",
        "Monokai",
        "Moonstone",
        "Night Owl",
        "Nord",
        "One Dark",
        "Orchid",
        "Rose",
        "Rose Pine",
        "Sand",
        "Soft Gray",
        "Solarized Dark",
        "Solarized Light",
        "Synthwave",
        "Tokyo Night",
        "Violet",
        "Walnut",
    };

    namespace Catalog
    {
        struct Palette
        {
            std::uint32_t themeId;
            std::uint32_t window, panel, control, border, text, secondaryText, accent;
        };

        inline constexpr Palette Presets[] = {
            { 0x01, 0x0D1016, 0x151A21, 0x20252C, 0x2F353C, 0xE6EDF3, 0x8B949E, 0x3AAEFF },
            { 0x02, 0xF6F6FA, 0xEDEDED, 0xF5F5F5, 0xCFCFD8, 0x1F1F1F, 0x767676, 0x1064A0 },
            { 0x03, 0x353D3F, 0x2A3235, 0x2E383B, 0x50595C, 0xE5E8EA, 0xA8ABAE, 0x8DBF5F },
            { 0x04, 0x222222, 0x111111, 0x323232, 0x595959, 0xF9F9F9, 0xB3B3B3, 0xF56200 },
            { 0x05, 0xEFF1F5, 0xE6E9EF, 0xF7F8FB, 0xBCC0CC, 0x4C4F69, 0x6C6F85, 0x8839EF },
            { 0x06, 0x181825, 0x1E1E2E, 0x313244, 0x45475A, 0xCDD6F4, 0xA6ADC8, 0xCBA6F7 },
            { 0x07, 0x170B07, 0x1B0F0B, 0x271610, 0x45372B, 0xC8C39C, 0x989174, 0xF6EB97 },
            { 0x08, 0x21222C, 0x282A36, 0x343746, 0x44475A, 0xF8F8F2, 0x8F9BC9, 0xBD93F9 },
            { 0x09, 0x212924, 0x252D28, 0x3B3B3B, 0x5C675F, 0xF3F3F3, 0xDEDEDE, 0x8BD69B },
            { 0x0A, 0x151515, 0x222222, 0x101010, 0x3A3A3A, 0xC8C8C8, 0x818181, 0x86B8F9 },
            { 0x0B, 0x1D2021, 0x282828, 0x3C3836, 0x504945, 0xEBDBB2, 0xA89984, 0xFABD2F },
            { 0x0C, 0xFBF1C7, 0xEBDBB2, 0xF2E5BC, 0xD5C4A1, 0x3C3836, 0x665C54, 0x076678 },
            { 0x0D, 0x303638, 0x212A27, 0x2A3531, 0x59635F, 0xF5F5F5, 0xC5C9C9, 0xBE3A00 },
            { 0x0E, 0x16161D, 0x1F1F28, 0x2A2A37, 0x54546D, 0xDCD7BA, 0xA09C85, 0x7E9CD8 },
            { 0x0F, 0x060916, 0x0D1123, 0x131931, 0x3E4355, 0xECECEC, 0xAEAFB3, 0xF2D62B },
            { 0x10, 0x1E1F1C, 0x272822, 0x3E3D32, 0x49483E, 0xF8F8F2, 0xA6A28C, 0xA6E22E },
            { 0x11, 0x222529, 0x26292D, 0x363636, 0x565B62, 0xEFEFEF, 0xD0D0D0, 0x86B2E1 },
            { 0x12, 0x010E1A, 0x011627, 0x0B2942, 0x1D3B53, 0xD6DEEB, 0x7791A7, 0x7FDBCA },
            { 0x13, 0x242933, 0x2E3440, 0x3B4252, 0x4C566A, 0xECEFF4, 0x9AA5B8, 0x88C0D0 },
            { 0x14, 0x21252B, 0x282C34, 0x2C313A, 0x3E4451, 0xABB2BF, 0x7F848E, 0x61AFEF },
            { 0x15, 0x383838, 0x2D2D2D, 0x444444, 0x5C5C5C, 0xD1D1D1, 0x969696, 0x9D5BB1 },
            { 0x16, 0x261F20, 0x2A2324, 0x352D2E, 0x58494B, 0xF1F1F1, 0xD0D0D0, 0xFAA1A8 },
            { 0x17, 0x191724, 0x1F1D2E, 0x26233A, 0x403D52, 0xE0DEF4, 0x908CAA, 0xEBBCBA },
            { 0x18, 0x222425, 0x151515, 0x2B2B2B, 0x4E4E4E, 0xEDEDED, 0xC1C1C1, 0xF1E693 },
            { 0x19, 0xA8A8A8, 0xBFBFBF, 0xCBCBCB, 0x979797, 0x272727, 0x4B4B4B, 0x126FDA },
            { 0x1A, 0x00212B, 0x002B36, 0x073642, 0x124A58, 0xEEE8D5, 0x93A1A1, 0x268BD2 },
            { 0x1B, 0xFDF6E3, 0xEEE8D5, 0xF7F1DE, 0xD6CFB8, 0x073642, 0x657B83, 0x268BD2 },
            { 0x1C, 0x241B2F, 0x262335, 0x34294F, 0x495495, 0xFFFFFF, 0xB6B1D1, 0xFF7EDB },
            { 0x1D, 0x16161E, 0x1A1B26, 0x24283B, 0x3B4261, 0xC0CAF5, 0x9AA5CE, 0x7AA2F7 },
            { 0x1E, 0x272727, 0x202020, 0x3A3A3A, 0x5A5A5A, 0xF5F5F5, 0xDEDEDE, 0x6C5CDD },
            { 0x1F, 0x41240B, 0x2D1001, 0x462E14, 0x5C4323, 0xD4B682, 0xA89570, 0x4CD933 },
        };

        static_assert(std::size(Names) == static_cast<std::size_t>(Index::PresetCount));
        static_assert(std::size(Presets) == static_cast<std::size_t>(Index::PresetCount));
        static_assert(Presets[0].themeId == 1 && Presets[1].themeId == 2);
        static_assert(std::ranges::all_of(Presets,
                          [](const Palette& preset)
                          {
                              return preset.themeId && std::ranges::count(Presets, preset.themeId, &Palette::themeId) == 1;
                          }),
            "Theme IDs must be unique and nonzero.");
    }
}
