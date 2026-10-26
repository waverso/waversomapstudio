// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../vendor/imgui/imgui.h"
#include "theme_catalog.h"
#include "../editor/user_data.h"

namespace Theme
{
    struct BaseColors
    {
        std::uint32_t themeId;
        ImU32 window, panel, control, border, text, secondaryText, accent;
        bool operator==(const BaseColors&) const = default;
    };

    inline constexpr ImU32 BaseColors::* ColorFields[] = { &BaseColors::window, &BaseColors::panel, &BaseColors::control,
        &BaseColors::border, &BaseColors::text, &BaseColors::secondaryText, &BaseColors::accent };

    inline constexpr ImU32 Rgb(unsigned color)
    {
        return IM_COL32(color >> 16, (color >> 8) & 255, color & 255, 255);
    }

    inline constexpr std::uint32_t Argb(ImU32 color)
    {
        return (((color >> IM_COL32_A_SHIFT) & 255u) << 24) | (((color >> IM_COL32_R_SHIFT) & 255u) << 16)
            | (((color >> IM_COL32_G_SHIFT) & 255u) << 8) | ((color >> IM_COL32_B_SHIFT) & 255u);
    }

    inline constexpr auto PackedPresets = []
    {
        struct
        {
            BaseColors values[std::size(Catalog::Presets)];
        } result {};
        for (std::size_t i = 0; i < std::size(Catalog::Presets); ++i)
        {
            const auto& base = Catalog::Presets[i];
            result.values[i] = { base.themeId, Rgb(base.window), Rgb(base.panel), Rgb(base.control), Rgb(base.border),
                Rgb(base.text), Rgb(base.secondaryText), Rgb(base.accent) };
        }
        return result;
    }();
    inline constexpr const auto& Presets = PackedPresets.values;

    inline Index g_selectedIndex = Index::Dark;
    inline std::vector<unsigned> g_displayOrder = []
    {
        std::vector<unsigned> order(std::size(Presets));
        for (unsigned i = 0; i < order.size(); ++i)
        {
            order[i] = i;
        }
        return order;
    }();
    inline std::vector<bool> g_hidden(std::size(Presets));
    struct CustomTheme
    {
        std::string name;
        BaseColors colors;
        Index sourcePresetIndex;
    };
    inline std::vector<CustomTheme> g_customThemes;
    inline std::uint32_t g_nextCustomId = UserData::CustomThemeIdBase;
    inline std::uint64_t g_revision = 0;

    inline const char* Name(unsigned index)
    {
        return index < std::size(Presets) ? Names[index] : g_customThemes[index - std::size(Presets)].name.c_str();
    }

    inline const BaseColors& Colors(Index themeIndex)
    {
        const auto index = static_cast<unsigned>(themeIndex);
        return index < std::size(Presets) ? Presets[index] : g_customThemes[index - std::size(Presets)].colors;
    }

    inline int FindIndexById(std::uint32_t themeId)
    {
        const auto it = std::ranges::find(g_displayOrder, themeId,
            [](unsigned index)
            {
                return Colors(static_cast<Index>(index)).themeId;
            });
        return it == g_displayOrder.end() ? -1 : static_cast<int>(*it);
    }

    inline Index SourcePresetIndex(Index themeIndex)
    {
        const auto index = static_cast<unsigned>(themeIndex);
        return index < std::size(Presets) ? themeIndex : g_customThemes[index - std::size(Presets)].sourcePresetIndex;
    }

    inline unsigned Clone(unsigned sourceIndex)
    {
        if (g_nextCustomId == UINT32_MAX)
        {
            throw std::overflow_error("No more custom theme IDs are available.");
        }
        const std::string base = std::string(Name(sourceIndex)) + " Copy";
        std::string name = base;
        for (unsigned suffix = 2; std::ranges::any_of(g_displayOrder,
                 [&](unsigned index)
                 {
                     return name == Name(index);
                 });
            ++suffix)
        {
            name = base + " " + std::to_string(suffix);
        }
        auto colors = Colors(static_cast<Index>(sourceIndex));
        colors.themeId = g_nextCustomId++;
        const auto index = static_cast<unsigned>(std::size(Presets) + g_customThemes.size());
        g_customThemes.push_back({ std::move(name), colors, SourcePresetIndex(static_cast<Index>(sourceIndex)) });
        g_displayOrder.insert(std::ranges::find(g_displayOrder, sourceIndex) + 1, index);
        g_hidden.push_back(false);
        ++g_revision;
        return index;
    }

    inline void Delete(unsigned deletedIndex)
    {
        if (deletedIndex < std::size(Presets))
        {
            return;
        }
        if (static_cast<unsigned>(g_selectedIndex) == deletedIndex)
        {
            g_selectedIndex = SourcePresetIndex(g_selectedIndex);
        }
        g_customThemes.erase(g_customThemes.begin() + (deletedIndex - std::size(Presets)));
        g_hidden.erase(g_hidden.begin() + deletedIndex);
        std::erase(g_displayOrder, deletedIndex);
        for (auto& index : g_displayOrder)
        {
            index -= index > deletedIndex;
        }
        if (static_cast<unsigned>(g_selectedIndex) > deletedIndex)
        {
            g_selectedIndex = static_cast<Index>(static_cast<unsigned>(g_selectedIndex) - 1);
        }
        ++g_revision;
    }

    inline bool Rename(unsigned index, const char* name)
    {
        if (index < std::size(Presets) || std::string_view(name).find_first_not_of(" \t\r\n\f\v") == std::string_view::npos
            || std::ranges::any_of(
                g_displayOrder,
                [&](unsigned other)
                {
                    return other != index && name == std::string_view(Name(other));
                }))
        {
            return false;
        }
        auto& current = g_customThemes[index - std::size(Presets)].name;
        if (current != name)
        {
            current = name;
            ++g_revision;
        }
        return true;
    }

    enum class SortOrder
    {
        Alphabetical,
        Original,
        Visible
    };

    inline void Sort(SortOrder order)
    {
        if (order == SortOrder::Alphabetical)
        {
            std::stable_sort(g_displayOrder.begin(), g_displayOrder.end(),
                [](unsigned a, unsigned b)
                {
                    return _stricmp(Name(a), Name(b)) < 0;
                });
        }
        else if (order == SortOrder::Original)
        {
            std::ranges::sort(g_displayOrder);
        }
        else
        {
            std::stable_partition(g_displayOrder.begin(), g_displayOrder.end(),
                [](unsigned index)
                {
                    return !g_hidden[index];
                });
        }
        ++g_revision;
    }

    inline UserData::Themes Capture()
    {
        UserData::Themes saved;
        saved.selectedThemeId = Colors(g_selectedIndex).themeId;
        saved.nextThemeId = g_nextCustomId;
        saved.customThemes.reserve(g_customThemes.size());
        saved.orderedThemeIds.reserve(g_displayOrder.size());
        for (const auto& value : g_customThemes)
        {
            UserData::CustomTheme theme;
            theme.themeId = value.colors.themeId;
            theme.sourcePresetId = Presets[static_cast<unsigned>(value.sourcePresetIndex)].themeId;
            theme.name = value.name;
            for (std::size_t i = 0; i < std::size(ColorFields); ++i)
            {
                theme.colors[i] = Argb(value.colors.*ColorFields[i]);
            }
            saved.customThemes.push_back(std::move(theme));
        }
        for (unsigned index : g_displayOrder)
        {
            saved.orderedThemeIds.push_back(Colors(static_cast<Index>(index)).themeId);
        }
        for (unsigned index = 0; index < g_hidden.size(); ++index)
        {
            if (g_hidden[index])
            {
                saved.hiddenThemeIds.push_back(Colors(static_cast<Index>(index)).themeId);
            }
        }
        return saved;
    }

    inline void Restore(const UserData::Themes& saved)
    {
        std::vector<CustomTheme> custom;
        custom.reserve(saved.customThemes.size());
        for (const auto& value : saved.customThemes)
        {
            BaseColors colors { value.themeId };
            for (std::size_t i = 0; i < std::size(ColorFields); ++i)
            {
                const auto color = value.colors[i];
                colors.*ColorFields[i] = IM_COL32((color >> 16) & 255, (color >> 8) & 255, color & 255, color >> 24);
            }
            const auto* sourcePreset = std::ranges::find(Presets, value.sourcePresetId, &BaseColors::themeId);
            custom.push_back({ value.name, colors,
                sourcePreset == std::end(Presets) ? Index::Dark : static_cast<Index>(sourcePreset - Presets) });
        }
        std::unordered_map<std::uint32_t, unsigned> indices;
        indices.reserve(std::size(Presets) + custom.size());
        for (unsigned i = 0; i < std::size(Presets); ++i)
        {
            indices.emplace(Presets[i].themeId, i);
        }
        for (unsigned i = 0; i < custom.size(); ++i)
        {
            indices.emplace(custom[i].colors.themeId, static_cast<unsigned>(std::size(Presets)) + i);
        }
        const auto find = [&](std::uint32_t themeId)
        {
            const auto it = indices.find(themeId);
            return it == indices.end() ? -1 : static_cast<int>(it->second);
        };
        const auto count = std::size(Presets) + custom.size();
        std::vector<unsigned> order;
        order.reserve(count);
        std::vector<bool> used(count), hidden(count);
        for (auto themeId : saved.orderedThemeIds)
        {
            const int index = find(themeId);
            if (index >= 0 && !used[index])
            {
                order.push_back(static_cast<unsigned>(index));
                used[index] = true;
            }
        }
        for (unsigned index = 0; index < count; ++index)
        {
            if (!used[index])
            {
                order.push_back(index);
            }
        }
        for (auto themeId : saved.hiddenThemeIds)
        {
            const int index = find(themeId);
            if (index >= 0)
            {
                hidden[index] = true;
            }
        }
        const int selected = find(saved.selectedThemeId);
        g_customThemes.swap(custom);
        g_displayOrder.swap(order);
        g_hidden.swap(hidden);
        g_selectedIndex = selected >= 0 ? static_cast<Index>(selected) : Index::Dark;
        g_nextCustomId = saved.nextThemeId;
        ++g_revision;
    }
    inline bool g_isLight = false;
    inline ImU32 g_windowBackground, g_panelBackground, g_alternatePanelBackground, g_infoCardBackground, g_controlBackground,
        g_hoveredControlBackground, g_activeControlBackground, g_headerBackground, g_hoveredHeaderBackground,
        g_alternateListRowBackground;
    inline ImU32 g_accent, g_accentSoft, g_accentMuted, g_accentDrag, g_accentHover, g_accentSoftHover;
    inline ImU32 g_text, g_textHover, g_textDim, g_textRemove, g_textRace, g_textRaceHover, g_textRaceCheckpoint,
        g_textRaceCheckpointHover;
    inline ImU32 g_border, g_borderStrong, g_borderShadow, g_shadow, g_highlight;
    inline ImU32 g_danger, g_dangerHover, g_dangerActive, g_success, g_successHover, g_successActive, g_statusText;
    inline ImU32 g_axisX, g_axisY, g_axisZ;
    inline ImU32 g_chipLowLod, g_chipCollidable, g_chipBreakable, g_chipAnimation, g_chipUvAnim, g_chipBackface, g_chipPhysics,
        g_chipExplosive;
    inline ImU32 g_chipFavorited, g_chipTime, g_chipSeamlessHor, g_chipSeamlessVer, g_chipSeamlessBoth;
    inline ImU32 g_toastBackground, g_toastText, g_errorBackground, g_errorText;

    inline ImU32 Alpha(ImU32 color, unsigned alpha)
    {
        return (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
    }

    inline ImU32 Mix(ImU32 from, ImU32 to, float amount)
    {
        const auto channel = [&](unsigned shift)
        {
            return static_cast<int>(std::lround(((from >> shift) & 255) * (1.0f - amount) + ((to >> shift) & 255) * amount));
        };
        return IM_COL32(channel(IM_COL32_R_SHIFT), channel(IM_COL32_G_SHIFT), channel(IM_COL32_B_SHIFT), 255);
    }

    inline ImU32 Shade(ImU32 color, int red, int green, int blue)
    {
        const int r = (color >> IM_COL32_R_SHIFT) & 255;
        const int g = (color >> IM_COL32_G_SHIFT) & 255;
        const int b = (color >> IM_COL32_B_SHIFT) & 255;
        return IM_COL32(std::clamp(r + (g_isLight ? -blue : red), 0, 255), std::clamp(g + (g_isLight ? -green : green), 0, 255),
            std::clamp(b + (g_isLight ? -red : blue), 0, 255), 255);
    }

    inline float Luminance(ImU32 color)
    {
        const auto channel = [&](unsigned shift)
        {
            const float value = ((color >> shift) & 255) / 255.0f;
            return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
        };
        return channel(IM_COL32_R_SHIFT) * 0.2126f + channel(IM_COL32_G_SHIFT) * 0.7152f + channel(IM_COL32_B_SHIFT) * 0.0722f;
    }

    inline float Contrast(ImU32 foreground, ImU32 background)
    {
        const float a = Luminance(foreground), b = Luminance(background);
        return (std::max(a, b) + 0.05f) / (std::min(a, b) + 0.05f);
    }

    inline ImU32 ControlFill(ImU32 color, float amount)
    {
        const float minimum = std::min(4.5f, Contrast(g_text, g_controlBackground));
        for (int step = 0; step < 16; ++step)
        {
            const ImU32 fill = Mix(g_controlBackground, color, amount * (16 - step) / 16.0f);
            if (Contrast(g_text, fill) >= minimum)
            {
                return fill;
            }
        }
        return g_controlBackground;
    }

    inline ImU32 Semantic(ImU32 color)
    {
        if (g_isLight)
        {
            const ImU32 original = color;
            const float background = Luminance(g_panelBackground);
            for (int step = 0; step < 20; ++step)
            {
                const float foreground = Luminance(color);
                if ((std::max(background, foreground) + 0.05f) / (std::min(background, foreground) + 0.05f) >= 4.5f)
                {
                    break;
                }
                color = Mix(original, g_text, (step + 1) / 20.0f);
            }
        }
        return color;
    }

    inline void Apply(const BaseColors& base)
    {
        g_windowBackground = base.window;
        g_panelBackground = base.panel;
        g_controlBackground = base.control;
        g_border = base.border;
        g_text = base.text;
        g_textDim = base.secondaryText;
        g_accent = base.accent;
        g_isLight = Luminance(g_panelBackground) > Luminance(g_text);
        g_alternatePanelBackground = Shade(g_panelBackground, 4, 5, 6);
        g_infoCardBackground = IM_COL32(0, 0, 0, g_isLight ? 12 : 38);
        g_hoveredControlBackground = Shade(g_controlBackground, 9, 11, 13);
        g_activeControlBackground = Shade(g_controlBackground, 17, 20, 24);
        g_headerBackground = Shade(g_panelBackground, 6, 7, 8);
        g_hoveredHeaderBackground = Shade(g_panelBackground, 15, 18, 22);
        g_alternateListRowBackground = Shade(g_panelBackground, 2, 3, 4);
        g_accentSoft = Alpha(g_accent, 48);
        g_accentMuted = Shade(g_accent, -20, -68, -101);
        g_accentDrag = Alpha(g_accent, 190);
        g_accentHover = Shade(g_accent, 44, 22, 0);
        g_accentSoftHover = Alpha(g_accent, 72);
        g_textHover = Shade(g_text, 26, 22, 14);
        g_textRemove = Semantic(IM_COL32(230, 92, 100, 255));
        g_textRace = Semantic(IM_COL32(255, 228, 110, 255));
        g_textRaceHover = g_isLight ? Shade(g_textRace, 20, 20, 20) : IM_COL32(255, 247, 170, 255);
        g_textRaceCheckpoint = Semantic(IM_COL32(210, 155, 35, 255));
        g_textRaceCheckpointHover = g_isLight ? Shade(g_textRaceCheckpoint, 20, 20, 20) : IM_COL32(242, 190, 55, 255);
        g_borderStrong = Shade(g_border, 23, 26, 31);
        g_shadow = IM_COL32(0, 0, 0, g_isLight ? 32 : 145);
        const int borderLow = std::min(
            { (g_border >> IM_COL32_R_SHIFT) & 255, (g_border >> IM_COL32_G_SHIFT) & 255, (g_border >> IM_COL32_B_SHIFT) & 255 });
        const int panelHigh = std::max({ (g_panelBackground >> IM_COL32_R_SHIFT) & 255,
            (g_panelBackground >> IM_COL32_G_SHIFT) & 255, (g_panelBackground >> IM_COL32_B_SHIFT) & 255 });
        g_borderShadow
            = g_isLight ? Alpha(g_shadow, std::clamp(255 - borderLow * 255 / std::max(1, panelHigh) + 16, 64, 192)) : g_shadow;
        g_highlight = g_isLight ? IM_COL32(0, 0, 0, 255) : IM_COL32_WHITE;
        g_danger = IM_COL32(184, 54, 62, 255);
        g_dangerHover = IM_COL32(215, 70, 78, 255);
        g_dangerActive = IM_COL32(148, 42, 49, 255);
        g_success = IM_COL32(45, 120, 70, 255);
        g_successHover = IM_COL32(60, 150, 90, 255);
        g_successActive = IM_COL32(40, 110, 64, 255);
        g_statusText = g_isLight ? IM_COL32_WHITE : g_text;
        g_axisX = Semantic(IM_COL32(242, 94, 94, 255));
        g_axisY = Semantic(IM_COL32(97, 210, 132, 255));
        g_axisZ = Semantic(IM_COL32(83, 157, 255, 255));
        g_chipLowLod = Semantic(IM_COL32(170, 170, 170, 255));
        g_chipCollidable = Semantic(IM_COL32(21, 206, 66, 255));
        g_chipBreakable = Semantic(IM_COL32(247, 103, 20, 255));
        g_chipAnimation = Semantic(IM_COL32(37, 106, 245, 255));
        g_chipUvAnim = Semantic(IM_COL32(255, 90, 200, 255));
        g_chipBackface = Semantic(IM_COL32(230, 200, 90, 255));
        g_chipPhysics = Semantic(IM_COL32(48, 200, 190, 255));
        g_chipExplosive = Semantic(IM_COL32(225, 75, 105, 255));
        g_chipFavorited = Semantic(IM_COL32(248, 46, 48, 255));
        g_chipTime = Semantic(IM_COL32(178, 138, 255, 255));
        g_chipSeamlessHor = Semantic(IM_COL32(160, 230, 0, 255));
        g_chipSeamlessVer = Semantic(IM_COL32(0, 190, 255, 255));
        g_chipSeamlessBoth = Semantic(IM_COL32(0, 255, 170, 255));
        g_toastBackground = Alpha(
            g_isLight ? Mix(g_panelBackground, g_accent, 0.08f) : Shade(g_windowBackground, 0, 8, 13), g_isLight ? 245 : 230);
        g_toastText = g_isLight ? g_text : Shade(g_text, -29, -7, 4);
        g_errorBackground = Alpha(
            g_isLight ? Mix(g_panelBackground, g_danger, 0.08f) : Shade(g_windowBackground, 19, 0, -1), g_isLight ? 245 : 230);
        g_errorText = g_isLight ? g_textRemove : Shade(g_text, 11, -33, -41);
    }
}
