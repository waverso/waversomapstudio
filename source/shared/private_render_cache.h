// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>

namespace SharedPrivateRender
{
    inline void InvalidateGeometryBindings()
    {
        for (std::uintptr_t stream = 0; stream < 2; ++stream)
        {
            *reinterpret_cast<std::uint32_t*>(0xC97BD8 + stream * 16) = 0xFFFFFFFF;
        }
        *reinterpret_cast<std::uint32_t*>(0x8E2440) = 0xFFFFFFFF;
        *reinterpret_cast<std::uint32_t*>(0x8E2444) = 0xFFFFFFFF;
        *reinterpret_cast<std::uint32_t*>(0x8E2448) = 0xFFFFFFFF;
        *reinterpret_cast<std::uint32_t*>(0x8E244C) = 0xFFFFFFFF;
        *reinterpret_cast<std::uint32_t*>(0x8E2450) = 0xFFFFFFFF;
        *reinterpret_cast<std::uint32_t*>(0xC9A5F4) = 0xFFFFFFFF;
        *reinterpret_cast<std::uint32_t*>(0xC98AF8) = 0xFFFFFFFF;
    }

    class GeometryBindingGuard
    {
        public:
        GeometryBindingGuard()
        {
            InvalidateGeometryBindings();
        }
        ~GeometryBindingGuard()
        {
            InvalidateGeometryBindings();
        }
        GeometryBindingGuard(const GeometryBindingGuard&) = delete;
        GeometryBindingGuard& operator=(const GeometryBindingGuard&) = delete;
    };
}
