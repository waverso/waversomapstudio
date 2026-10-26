// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "../shared/file_formats.h"
#include "../scene/scene_data.h"
#include "../scene/scene_limits.h"

namespace ProjectDocument
{
    inline constexpr std::uint32_t Version = 1;
    inline constexpr std::size_t MaxItems = SceneLimits::MaxItems;
    inline constexpr std::size_t MaxFileBytes = 64 * 1024 * 1024;
    inline constexpr auto Magic = StudioFormat::ProjectMagic;
    inline constexpr std::size_t HeaderBytes = Magic.size() + 3 * sizeof(std::uint32_t);

    using Items = std::vector<ItemSnapshot>;

    struct Camera
    {
        float position[3] = {};
        float yaw = 0;
        float pitch = 0;
    };

    std::vector<std::uint8_t> Encode(const Items& items, const std::optional<Camera>& camera = std::nullopt);
    bool Decode(std::span<const std::uint8_t> bytes, Items& items, std::string& error, std::optional<Camera>* camera = nullptr);
    bool Validate(const Items& items, std::string& error);
}
