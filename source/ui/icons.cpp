// Source Repository: https://github.com/waverso/waversomapstudio

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "icons.h"

#include <bit>
#include <cstring>

#include "../../resources/resource.h"
#include "../rendering/image_atlas.h"
#include "../rendering/editor_shaders.h"
#include "theme.h"

namespace
{
    bool HasAccent(IDirect3DTexture9* texture)
    {
        D3DSURFACE_DESC description {};
        D3DLOCKED_RECT locked {};
        if (FAILED(texture->GetLevelDesc(0, &description)) || FAILED(texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)))
        {
            return false;
        }
        bool found = false;
        for (UINT y = 0; y < description.Height && !found; ++y)
        {
            const auto* pixel = static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch;
            for (UINT x = 0; x < description.Width; ++x, pixel += 4)
            {
                if (pixel[3] && pixel[2] > std::max(pixel[0], pixel[1]))
                {
                    found = true;
                    break;
                }
            }
        }
        texture->UnlockRect(0);
        return found;
    }

    void LoadIcon(IWICImagingFactory* factory, IDirect3DDevice9* device, IWICBitmap* atlas, const ImageAtlas::AtlasEntry& entry,
        IDirect3DTexture9*& output) noexcept
    {
        ImageAtlas::ComPtr<IWICBitmapClipper> clipper;
        if (FAILED(factory->CreateBitmapClipper(clipper.GetAddressOf())))
        {
            return;
        }
        const WICRect rect { static_cast<INT>(entry.x), static_cast<INT>(entry.y), static_cast<INT>(entry.width),
            static_cast<INT>(entry.height) };
        if (FAILED(clipper->Initialize(atlas, &rect)))
        {
            return;
        }
        ImageAtlas::CreateTexture(
            factory, device, clipper.Get(), std::bit_ceil(entry.width), std::bit_ceil(entry.height), 0, &output);
    }
}

Icons::Icons(IDirect3DDevice9* pDevice) : m_device(pDevice)
{
    if (!pDevice)
    {
        return;
    }
    ImageAtlas::Image atlas(IDR_EDITOR_ATLAS);
    if (!atlas.bitmap)
    {
        return;
    }
    for (UINT i = 0; i < atlas.header.count; ++i)
    {
        const auto entry = ImageAtlas::ReadEntry(atlas.entries, i);
        for (const auto& slot : IconSlots)
        {
            if (std::strcmp(entry.name, slot.name) == 0)
            {
                LoadIcon(atlas.factory.Get(), pDevice, atlas.bitmap.Get(), entry, this->*slot.texture);
                if (auto* texture = this->*slot.texture; texture && HasAccent(texture))
                {
                    m_accentTextures.push_back(texture);
                }
                if (slot.texture == &Icons::logoTexture)
                {
                    logoAspectRatio = static_cast<float>(entry.height) / static_cast<float>(entry.width);
                }
                break;
            }
        }
    }
    if (!m_accentTextures.empty())
    {
        pDevice->CreatePixelShader(IconPixelShader, &m_accentShader);
    }
}

bool Icons::UsesAccent(IDirect3DTexture9* texture) const
{
    return m_accentShader && std::find(m_accentTextures.begin(), m_accentTextures.end(), texture) != m_accentTextures.end();
}

void Icons::BeginAccent(const ImDrawList*, const ImDrawCmd* command)
{
    const auto* icons = static_cast<const Icons*>(command->UserCallbackData);
    const ImU32 color = Theme::g_accent;
    const float accent[] = { ((color >> IM_COL32_R_SHIFT) & 255) / 255.0f, ((color >> IM_COL32_G_SHIFT) & 255) / 255.0f,
        ((color >> IM_COL32_B_SHIFT) & 255) / 255.0f, 1.0f };
    icons->m_device->SetPixelShader(icons->m_accentShader);
    icons->m_device->SetPixelShaderConstantF(0, accent, 1);
}

void Icons::EndAccent(const ImDrawList*, const ImDrawCmd* command)
{
    static_cast<const Icons*>(command->UserCallbackData)->m_device->SetPixelShader(nullptr);
}

Icons::~Icons()
{
    if (m_accentShader)
    {
        m_accentShader->Release();
    }
    for (const auto& slot : IconSlots)
    {
        if (auto* texture = this->*slot.texture)
        {
            texture->Release();
        }
    }
}
