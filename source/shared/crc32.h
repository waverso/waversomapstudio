// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace Utils
{
    inline std::uint32_t Crc32(std::span<const std::uint8_t> bytes, std::uint32_t previousCrc = 0) noexcept
    {
        static constexpr auto table = []
        {
            std::array<std::uint32_t, 256> values {};
            for (std::uint32_t i = 0; i < values.size(); ++i)
            {
                std::uint32_t crc = i;
                for (int bit = 0; bit < 8; ++bit)
                {
                    crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
                }
                values[i] = crc;
            }
            return values;
        }();
        std::uint32_t crc = ~previousCrc;
        for (auto byte : bytes)
        {
            crc = (crc >> 8) ^ table[(crc ^ byte) & 255];
        }
        return ~crc;
    }
}
