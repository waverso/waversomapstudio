// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstddef>
#include <cstdint>

struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace ModelThumbnails
{
    struct View
    {
        IDirect3DTexture9* texture = nullptr;
        float uvMin[2] = {};
        float uvMax[2] = {};
    };

    void BeginFrame(IDirect3DDevice9* device);
    void PreparePage(const std::uint32_t* modelIds, std::size_t count);
    bool Get(int modelId, View& out);
    void Shutdown();
}
