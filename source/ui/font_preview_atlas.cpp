// Source Repository: https://github.com/waverso/waversomapstudio

#include "font_preview_atlas.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "../../resources/resource.h"
#include "../rendering/image_atlas.h"

namespace
{
    ImageAtlas::ComPtr<IDirect3DTexture9> g_texture;
    std::vector<FontPreviewAtlas::Entry> g_entries;
}

bool FontPreviewAtlas::Load(IDirect3DDevice9* device)
{
    Shutdown();
    ImageAtlas::Image atlas(IDR_FONT_LIST_ATLAS);
    if (device && atlas.bitmap)
    {
        ImageAtlas::CreateTexture(atlas.factory.Get(), device, atlas.bitmap.Get(), atlas.header.width, atlas.header.height, 1,
            g_texture.GetAddressOf());
    }
    g_entries.reserve(atlas.header.count);
    for (UINT i = 0; i < atlas.header.count; ++i)
    {
        const auto bounds = ImageAtlas::ReadEntry(atlas.entries, i);
        Entry entry {};
        std::memcpy(entry.name, bounds.name, sizeof(entry.name));
        entry.image.texture = g_texture.Get();
        entry.image.uvMin
            = ImVec2(static_cast<float>(bounds.x) / atlas.header.width, static_cast<float>(bounds.y) / atlas.header.height);
        entry.image.uvMax = ImVec2(static_cast<float>(bounds.x + bounds.width) / atlas.header.width,
            static_cast<float>(bounds.y + bounds.height) / atlas.header.height);
        entry.image.size = ImVec2(static_cast<float>(bounds.width), static_cast<float>(bounds.height));
        g_entries.push_back(entry);
    }
    return g_texture != nullptr;
}

void FontPreviewAtlas::Shutdown()
{
    g_entries.clear();
    g_texture.Reset();
}

std::span<const FontPreviewAtlas::Entry> FontPreviewAtlas::Entries()
{
    return g_entries;
}

const ImCustom::ComboImage* FontPreviewAtlas::Find(const char* name)
{
    if (name)
    {
        for (const auto& entry : g_entries)
        {
            if (_stricmp(entry.name, name) == 0)
            {
                return &entry.image;
            }
        }
    }
    return nullptr;
}

float FontPreviewAtlas::MaxWidth(float rowHeight)
{
    float width = 0.0f;
    for (const auto& entry : g_entries)
    {
        width = (std::max)(width, rowHeight * entry.image.size.x / entry.image.size.y);
    }
    return width;
}
