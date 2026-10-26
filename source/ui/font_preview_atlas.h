// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <span>

#include "imgui_custom.h"

struct IDirect3DDevice9;

namespace FontPreviewAtlas
{
    struct Entry
    {
        char name[32];
        ImCustom::ComboImage image;
    };

    bool Load(IDirect3DDevice9* device);
    void Shutdown();
    std::span<const Entry> Entries();
    const ImCustom::ComboImage* Find(const char* name);
    float MaxWidth(float rowHeight);
}
