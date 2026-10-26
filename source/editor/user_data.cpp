// Source Repository: https://github.com/waverso/waversomapstudio

#include "user_data.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "../shared/crc32.h"
#include "../shared/file_formats.h"
#include "../ui/theme_catalog.h"

namespace UserData
{
    namespace
    {
        constexpr std::array Magics = { StudioFormat::SettingsMagic, StudioFormat::BookmarksMagic, StudioFormat::FavoritesMagic,
            StudioFormat::ThemesMagic };
        constexpr std::array Members
            = { &Settings::uiScale, &Settings::gizmoScale, &Settings::indicatorScale, &Settings::fov, &Settings::mouseSensitivity,
                  &Settings::baseSpeed, &Settings::scrollSpeed, &Settings::fastMultiplier, &Settings::slowMultiplier,
                  &Settings::sidebarWidth, &Settings::sidebarSplit, &Settings::positionGrid, &Settings::rotationGrid };
        constexpr std::array<std::array<float, 2>, Members.size()> Limits = { { { 0.75f, 1.5f }, { 0.7f, 1.3f }, { 0.7f, 1.3f },
            { 20.0f, 110.0f }, { 0.5f, 10.0f }, { 10.0f, 150.0f }, { 20.0f, 130.0f }, { 2.0f, 5.0f }, { 0.05f, 0.5f },
            { 0.0f, 1.0f }, { 0.3f, 0.7f }, { 0.0f, 1000.0f }, { 0.0f, 180.0f } } };

        void Require(bool condition, const char* error)
        {
            if (!condition)
            {
                throw std::runtime_error(error);
            }
        }

        bool ValidText(std::string_view text)
        {
            return text.find('\0') == std::string_view::npos
                && (text.empty()
                    || (text.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max())
                        && MultiByteToWideChar(
                               CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0)
                            > 0));
        }

        struct Writer
        {
            Bytes bytes;
            void U32(std::uint32_t value)
            {
                for (int i = 0; i < 4; ++i)
                {
                    bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
                }
            }
            void Float(float value)
            {
                Require(std::isfinite(value), "Invalid user data number.");
                U32(std::bit_cast<std::uint32_t>(value));
            }
            void String(std::string_view value, std::size_t limit)
            {
                Require(!value.empty() && value.size() <= limit && ValidText(value), "Invalid user data string.");
                U32(static_cast<std::uint32_t>(value.size()));
                bytes.insert(bytes.end(), value.begin(), value.end());
            }
        };

        struct Reader
        {
            std::span<const std::uint8_t> bytes;
            std::size_t offset = 0;
            std::uint32_t U32()
            {
                Require(bytes.size() - offset >= 4, "Truncated user data file.");
                std::uint32_t value = 0;
                for (int i = 0; i < 4; ++i)
                {
                    value |= std::uint32_t(bytes[offset++]) << (i * 8);
                }
                return value;
            }
            float Float()
            {
                const float value = std::bit_cast<float>(U32());
                Require(std::isfinite(value), "Invalid user data number.");
                return value;
            }
            std::uint32_t Count(std::size_t maximum, std::size_t minimumBytes)
            {
                const auto count = U32();
                Require(count <= maximum && count <= (bytes.size() - offset) / minimumBytes, "Invalid user data count.");
                return count;
            }
            std::string String(std::size_t maximum)
            {
                const auto count = Count(maximum, 1);
                Require(count != 0, "Empty user data string.");
                std::string value(reinterpret_cast<const char*>(bytes.data() + offset), count);
                offset += count;
                Require(ValidText(value), "Invalid user data string.");
                return value;
            }
        };

        void Validate(const Settings& settings)
        {
            for (std::size_t i = 0; i < Members.size(); ++i)
            {
                const float value = settings.*Members[i];
                Require(std::isfinite(value) && value >= Limits[i][0] && value <= Limits[i][1], "Invalid editor setting.");
            }
            Require(settings.vehicleColorStyle <= 1, "Invalid vehicle color picker style.");
            Require(settings.themeIndex < std::size(Theme::Catalog::Presets), "Invalid interface theme.");
            const auto days = settings.updateCheckDays;
            Require(days == 0 || days == 1 || days == 7 || days == 30, "Invalid update check interval.");
        }

        void Validate(const Bookmark& bookmark)
        {
            Require(bookmark.id && bookmark.id < std::numeric_limits<std::uint64_t>::max(), "Invalid bookmark ID.");
            for (float coordinate : bookmark.position)
            {
                Require(std::isfinite(coordinate) && std::abs(coordinate) <= 10000000.0f, "Invalid bookmark position.");
            }
            Require(std::isfinite(bookmark.yaw) && std::isfinite(bookmark.pitch) && std::abs(bookmark.pitch) <= 1.571f,
                "Invalid bookmark direction.");
        }

        void Validate(const Themes& themes)
        {
            Require(themes.customThemes.size() <= MaxFileBytes / 41 && themes.nextThemeId >= CustomThemeIdBase,
                "Invalid custom theme count or next ID.");
            std::set<std::uint32_t> ids;
            std::set<std::string_view> names;
            for (const auto& theme : themes.customThemes)
            {
                Require(
                    theme.themeId >= CustomThemeIdBase && theme.themeId < themes.nextThemeId && ids.insert(theme.themeId).second,
                    "Invalid or duplicate custom theme ID.");
                Require(theme.sourcePresetId && theme.sourcePresetId < CustomThemeIdBase, "Invalid custom theme source.");
                Require(theme.name.size() <= MaxFileBytes && ValidText(theme.name)
                        && theme.name.find_first_not_of(" \t\r\n\f\v") != std::string::npos && names.insert(theme.name).second,
                    "Invalid or duplicate custom theme name.");
            }
            const auto validId = [&](std::uint32_t id)
            {
                return id && (id < CustomThemeIdBase || ids.contains(id));
            };
            Require(validId(themes.selectedThemeId), "Invalid selected theme ID.");
            for (const auto* values : { &themes.orderedThemeIds, &themes.hiddenThemeIds })
            {
                Require(values->size() <= MaxFileBytes / 4, "Too many theme references.");
                std::set<std::uint32_t> seen;
                for (auto id : *values)
                {
                    Require(validId(id) && seen.insert(id).second, "Invalid or duplicate theme reference.");
                }
            }
        }

        void WriteIds(Writer& writer, const std::set<int>& ids, int minimum, int maximum)
        {
            Require(ids.size() <= static_cast<std::size_t>(maximum - minimum + 1), "Too many favorite models.");
            writer.U32(static_cast<std::uint32_t>(ids.size()));
            for (int id : ids)
            {
                Require(id >= minimum && id <= maximum, "Invalid favorite model ID.");
                writer.U32(static_cast<std::uint32_t>(id));
            }
        }

        std::set<int> ReadIds(Reader& reader, int minimum, int maximum)
        {
            const auto count = reader.Count(maximum - minimum + 1, 4);
            std::set<int> ids;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto id = reader.U32();
                Require(id >= static_cast<unsigned>(minimum) && id <= static_cast<unsigned>(maximum)
                        && ids.insert(static_cast<int>(id)).second,
                    "Invalid or duplicate favorite model ID.");
            }
            return ids;
        }

        void WriteNames(Writer& writer, const std::set<NamedFavorite>& names)
        {
            Require(names.size() <= MaxFavorites, "Too many named favorites.");
            writer.U32(static_cast<std::uint32_t>(names.size()));
            for (const auto& key : names)
            {
                writer.String(key.library, 255);
                writer.String(key.name, 255);
            }
        }

        std::set<NamedFavorite> ReadNames(Reader& reader)
        {
            const auto count = reader.Count(MaxFavorites, 10);
            std::set<NamedFavorite> names;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto library = reader.String(255);
                const auto name = reader.String(255);
                Require(names.emplace(library, name).second, "Duplicate named favorite.");
            }
            return names;
        }
    }

    Bytes Encode(const Document& document)
    {
        Writer writer;
        if (const auto* settings = std::get_if<Settings>(&document))
        {
            Validate(*settings);
            for (auto member : Members)
            {
                writer.Float(settings->*member);
            }
            writer.U32(settings->vehicleColorStyle);
            writer.U32(settings->outline ? 1 : 0);
            writer.U32(Theme::Catalog::Presets[settings->themeIndex].themeId);
            writer.U32(settings->updateCheckDays);
            writer.U32(static_cast<std::uint32_t>(settings->lastUpdateCheck));
            writer.U32(static_cast<std::uint32_t>(settings->lastUpdateCheck >> 32));
        }
        else if (const auto* bookmarks = std::get_if<Bookmarks>(&document))
        {
            Require(bookmarks->size() <= MaxBookmarks, "Too many bookmarks.");
            writer.U32(static_cast<std::uint32_t>(bookmarks->size()));
            std::set<std::uint64_t> ids;
            for (const auto& bookmark : *bookmarks)
            {
                Validate(bookmark);
                Require(ids.insert(bookmark.id).second, "Duplicate bookmark ID.");
                writer.U32(static_cast<std::uint32_t>(bookmark.id));
                writer.U32(static_cast<std::uint32_t>(bookmark.id >> 32));
                writer.String(bookmark.name, 63);
                for (float coordinate : bookmark.position)
                {
                    writer.Float(coordinate);
                }
                writer.Float(bookmark.yaw);
                writer.Float(bookmark.pitch);
            }
        }
        else if (const auto* favorites = std::get_if<Favorites>(&document))
        {
            WriteIds(writer, favorites->objects, 0, 19999);
            WriteIds(writer, favorites->vehicles, 400, 611);
            WriteIds(writer, favorites->skins, 0, 311);
            WriteNames(writer, favorites->textures);
            WriteNames(writer, favorites->animations);
        }
        else
        {
            const auto& themes = std::get<Themes>(document);
            Validate(themes);
            writer.U32(themes.selectedThemeId);
            writer.U32(themes.nextThemeId);
            writer.U32(static_cast<std::uint32_t>(themes.customThemes.size()));
            for (const auto& theme : themes.customThemes)
            {
                writer.U32(theme.themeId);
                writer.U32(theme.sourcePresetId);
                for (auto color : theme.colors)
                {
                    writer.U32(color);
                }
                writer.String(theme.name, MaxFileBytes);
            }
            for (const auto* values : { &themes.orderedThemeIds, &themes.hiddenThemeIds })
            {
                writer.U32(static_cast<std::uint32_t>(values->size()));
                for (auto id : *values)
                {
                    writer.U32(id);
                }
            }
        }
        const auto magic = Magics[document.index()];
        Require(writer.bytes.size() <= MaxFileBytes - magic.size() - 12, "User data file is too large.");
        Writer header;
        header.bytes.insert(header.bytes.end(), magic.begin(), magic.end());
        header.U32(Version);
        header.U32(static_cast<std::uint32_t>(writer.bytes.size()));
        header.U32(Utils::Crc32(writer.bytes));
        header.bytes.insert(header.bytes.end(), writer.bytes.begin(), writer.bytes.end());
        return std::move(header.bytes);
    }

    Document Decode(Group group, std::span<const std::uint8_t> bytes)
    {
        const auto index = static_cast<std::size_t>(group);
        Require(index < Magics.size(), "Invalid user data group.");
        const auto magic = Magics[index];
        Require(bytes.size() >= magic.size() + 12 && bytes.size() <= MaxFileBytes, "Invalid user data file size.");
        Require(
            std::equal(magic.begin(), magic.end(), reinterpret_cast<const char*>(bytes.data())), "Invalid user data signature.");
        Reader reader { bytes, magic.size() };
        const auto version = reader.U32();
        Require(version == Version, "Unsupported user data version.");
        const auto size = reader.U32();
        const auto checksum = reader.U32();
        Require(size == bytes.size() - reader.offset && checksum == Utils::Crc32(bytes.subspan(reader.offset)),
            "User data size or checksum mismatch.");
        Document document;
        if (group == Group::Settings)
        {
            Settings settings;
            for (auto member : Members)
            {
                settings.*member = reader.Float();
            }
            settings.vehicleColorStyle = reader.U32();
            const auto outline = reader.U32();
            Require(outline <= 1, "Invalid outline setting.");
            settings.outline = outline != 0;
            const auto theme = reader.U32();
            const auto* selected = std::ranges::find(Theme::Catalog::Presets, theme, &Theme::Catalog::Palette::themeId);
            settings.themeIndex = selected == std::end(Theme::Catalog::Presets)
                ? 0
                : static_cast<std::uint32_t>(selected - Theme::Catalog::Presets);
            settings.updateCheckDays = reader.U32();
            settings.lastUpdateCheck = reader.U32();
            settings.lastUpdateCheck |= std::uint64_t(reader.U32()) << 32;
            Validate(settings);
            document = settings;
        }
        else if (group == Group::Bookmarks)
        {
            const auto count = reader.Count(MaxBookmarks, 33);
            Bookmarks bookmarks;
            bookmarks.reserve(count);
            std::set<std::uint64_t> ids;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                Bookmark bookmark;
                bookmark.id = reader.U32();
                bookmark.id |= std::uint64_t(reader.U32()) << 32;
                bookmark.name = reader.String(63);
                for (float& coordinate : bookmark.position)
                {
                    coordinate = reader.Float();
                }
                bookmark.yaw = reader.Float();
                bookmark.pitch = reader.Float();
                Validate(bookmark);
                Require(ids.insert(bookmark.id).second, "Duplicate bookmark ID.");
                bookmarks.push_back(std::move(bookmark));
            }
            document = std::move(bookmarks);
        }
        else if (group == Group::Favorites)
        {
            Favorites favorites;
            favorites.objects = ReadIds(reader, 0, 19999);
            favorites.vehicles = ReadIds(reader, 400, 611);
            favorites.skins = ReadIds(reader, 0, 311);
            favorites.textures = ReadNames(reader);
            favorites.animations = ReadNames(reader);
            document = std::move(favorites);
        }
        else
        {
            Themes themes;
            themes.selectedThemeId = reader.U32();
            themes.nextThemeId = reader.U32();
            const auto count = reader.Count(MaxFileBytes / 41, 41);
            themes.customThemes.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                CustomTheme theme;
                theme.themeId = reader.U32();
                theme.sourcePresetId = reader.U32();
                for (auto& color : theme.colors)
                {
                    color = reader.U32();
                }
                theme.name = reader.String(MaxFileBytes);
                themes.customThemes.push_back(std::move(theme));
            }
            for (auto* values : { &themes.orderedThemeIds, &themes.hiddenThemeIds })
            {
                const auto entries = reader.Count(MaxFileBytes / 4, 4);
                values->reserve(entries);
                for (std::uint32_t i = 0; i < entries; ++i)
                {
                    values->push_back(reader.U32());
                }
            }
            Validate(themes);
            document = std::move(themes);
        }
        Require(reader.offset == bytes.size(), "Unexpected trailing user data.");
        return document;
    }
}
