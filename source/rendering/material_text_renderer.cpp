// Source Repository: https://github.com/waverso/waversomapstudio

#include "material_text_renderer.h"

#include <algorithm>
#include <array>
#include <dxsdk/d3dx9.h>
#include <string>
#include <wrl/client.h>

namespace
{
    using Microsoft::WRL::ComPtr;

    struct RenderResources
    {
        IDirect3DDevice9* device = nullptr;
        ComPtr<ID3DXFont> font;
        ComPtr<ID3DXSprite> sprite;
        ComPtr<IDirect3DSurface9> target, readback;
    } g_resources;

    struct RenderState
    {
        IDirect3DDevice9* device;
        ComPtr<IDirect3DStateBlock9> block;
        std::array<ComPtr<IDirect3DSurface9>, 4> targets;
        ComPtr<IDirect3DSurface9> depth;
        UINT count = 0;
        bool changed = false;

        explicit RenderState(IDirect3DDevice9* value) : device(value)
        {
        }

        bool Capture()
        {
            D3DCAPS9 caps {};
            if (FAILED(device->GetDeviceCaps(&caps)) || FAILED(device->CreateStateBlock(D3DSBT_ALL, &block)))
            {
                return false;
            }
            count = (std::min)(caps.NumSimultaneousRTs, static_cast<DWORD>(targets.size()));
            for (UINT i = 0; i < count; ++i)
            {
                const HRESULT result = device->GetRenderTarget(i, &targets[i]);
                if (FAILED(result) && result != D3DERR_NOTFOUND)
                {
                    return false;
                }
            }
            const HRESULT result = device->GetDepthStencilSurface(&depth);
            return targets[0] && (SUCCEEDED(result) || result == D3DERR_NOTFOUND);
        }

        ~RenderState()
        {
            if (!changed)
            {
                return;
            }
            device->SetDepthStencilSurface(nullptr);
            for (UINT i = 0; i < count; ++i)
            {
                device->SetRenderTarget(i, targets[i].Get());
            }
            device->SetDepthStencilSurface(depth.Get());
            block->Apply();
        }
    };

    bool ReadColor(std::wstring_view text, D3DCOLOR& color)
    {
        if (text.size() < 8 || text[0] != L'{' || text[7] != L'}')
        {
            return false;
        }
        unsigned int rgb = 0;
        for (int i = 1; i < 7; ++i)
        {
            const wchar_t c = text[i];
            const int digit = c >= L'0' && c <= L'9' ? c - L'0'
                : c >= L'A' && c <= L'F'             ? c - L'A' + 10
                : c >= L'a' && c <= L'f'             ? c - L'a' + 10
                                                     : -1;
            if (digit < 0)
            {
                return false;
            }
            rgb = (rgb << 4) | digit;
        }
        color = 0xFF000000 | rgb;
        return true;
    }

    class TextSprite final : public ID3DXSprite
    {
        ID3DXSprite* sprite;
        const std::vector<D3DCOLOR>& colors;
        size_t next = 0;

        public:
        TextSprite(ID3DXSprite* value, const std::vector<D3DCOLOR>& values) : sprite(value), colors(values)
        {
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** value) override
        {
            return sprite->QueryInterface(iid, value);
        }
        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return sprite->AddRef();
        }
        ULONG STDMETHODCALLTYPE Release() override
        {
            return sprite->Release();
        }
        HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** value) override
        {
            return sprite->GetDevice(value);
        }
        HRESULT STDMETHODCALLTYPE GetTransform(D3DXMATRIX* value) override
        {
            return sprite->GetTransform(value);
        }
        HRESULT STDMETHODCALLTYPE SetTransform(const D3DXMATRIX* value) override
        {
            return sprite->SetTransform(value);
        }
        HRESULT STDMETHODCALLTYPE SetWorldViewRH(const D3DXMATRIX* world, const D3DXMATRIX* view) override
        {
            return sprite->SetWorldViewRH(world, view);
        }
        HRESULT STDMETHODCALLTYPE SetWorldViewLH(const D3DXMATRIX* world, const D3DXMATRIX* view) override
        {
            return sprite->SetWorldViewLH(world, view);
        }
        HRESULT STDMETHODCALLTYPE Begin(DWORD flags) override
        {
            return sprite->Begin(flags);
        }
        HRESULT STDMETHODCALLTYPE Flush() override
        {
            return sprite->Flush();
        }
        HRESULT STDMETHODCALLTYPE End() override
        {
            return sprite->End();
        }
        HRESULT STDMETHODCALLTYPE OnLostDevice() override
        {
            return sprite->OnLostDevice();
        }
        HRESULT STDMETHODCALLTYPE OnResetDevice() override
        {
            return sprite->OnResetDevice();
        }
        HRESULT STDMETHODCALLTYPE Draw(IDirect3DTexture9* texture, const RECT* rect, const D3DXVECTOR3* center,
            const D3DXVECTOR3* position, D3DCOLOR color) override
        {
            if (next < colors.size())
            {
                color = colors[next++];
            }
            return sprite->Draw(texture, rect, center, position, color);
        }
    };
}

bool RenderMaterialText(IDirect3DDevice9* device, const MaterialTextData& data, int width, int height, std::string_view text,
    std::vector<unsigned int>& pixels)
{
    if (!device || data.fontSize <= 0 || data.fontSize > 4096 || width <= 0 || height <= 0
        || pixels.size() != static_cast<size_t>(width) * height)
    {
        return false;
    }
    const HMODULE module = GetModuleHandleA("d3dx9_25.dll");
    if (!module)
    {
        return false;
    }
    const auto createFont = reinterpret_cast<decltype(&D3DXCreateFontA)>(GetProcAddress(module, "D3DXCreateFontA"));
    const auto createSprite = reinterpret_cast<decltype(&D3DXCreateSprite)>(GetProcAddress(module, "D3DXCreateSprite"));
    if (!createFont || !createSprite)
    {
        return false;
    }

    const int length = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring source(static_cast<size_t>(length), L'\0');
    if (!text.empty()
        && (!length || !MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), source.data(), length)))
    {
        return false;
    }
    std::wstring plain;
    std::vector<D3DCOLOR> colors;
    D3DCOLOR color = data.fontColorArgb;
    for (size_t i = 0; i < source.size();)
    {
        if (ReadColor(std::wstring_view(source).substr(i), color))
        {
            i += 8;
            continue;
        }
        const wchar_t c = source[i++];
        plain += c;
        if (c != L' ' && c != L'\t' && c != L'\r' && c != L'\n')
        {
            colors.push_back(color);
        }
    }

    RenderState state(device);
    if (!state.Capture())
    {
        return false;
    }
    if (g_resources.device != device)
    {
        ReleaseMaterialTextRenderer();
        g_resources.device = device;
    }
    auto& [rendererDevice, font, sprite, target, readback] = g_resources;
    const D3DFORMAT format = data.backgroundColorArgb ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8;
    char face[LF_FACESIZE] {};
    memcpy(face, data.fontFace, (std::min)(strnlen(data.fontFace, sizeof(data.fontFace)), sizeof(face) - 1));
    D3DXFONT_DESCA desc {};
    if (!font || FAILED(font->GetDescA(&desc)) || desc.Height != data.fontSize
        || desc.Weight != static_cast<UINT>(data.bold ? FW_BOLD : FW_NORMAL) || strcmp(desc.FaceName, face) != 0)
    {
        font.Reset();
        if (FAILED(createFont(device, data.fontSize, 0, data.bold ? FW_BOLD : FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, face, &font)))
        {
            return false;
        }
    }
    if (!sprite && FAILED(createSprite(device, &sprite)))
    {
        return false;
    }
    D3DSURFACE_DESC surface {};
    if (!target || FAILED(target->GetDesc(&surface)) || surface.Width != static_cast<UINT>(width)
        || surface.Height != static_cast<UINT>(height) || surface.Format != format)
    {
        target.Reset();
        readback.Reset();
        if (FAILED(device->CreateRenderTarget(width, height, format, D3DMULTISAMPLE_NONE, 0, FALSE, &target, nullptr)))
        {
            return false;
        }
    }
    if (!readback && FAILED(device->CreateOffscreenPlainSurface(width, height, format, D3DPOOL_SYSTEMMEM, &readback, nullptr)))
    {
        return false;
    }
    state.changed = true;
    if (FAILED(device->SetDepthStencilSurface(nullptr)))
    {
        return false;
    }
    for (UINT i = 1; i < state.count; ++i)
    {
        if (FAILED(device->SetRenderTarget(i, nullptr)))
        {
            return false;
        }
    }
    if (FAILED(device->SetRenderTarget(0, target.Get()))
        || FAILED(device->Clear(0, nullptr, D3DCLEAR_TARGET, data.backgroundColorArgb, 0, 0))
        || FAILED(sprite->Begin(D3DXSPRITE_ALPHABLEND)))
    {
        return false;
    }
    if (!data.backgroundColorArgb)
    {
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
    }
    TextSprite colored(sprite.Get(), colors);
    RECT rect { 0, 0, width, height };
    const DWORD alignment = data.alignment == 1 ? DT_CENTER | DT_VCENTER : data.alignment == 2 ? DT_RIGHT : DT_LEFT;
    const int drawn = font->DrawTextW(
        &colored, plain.c_str(), static_cast<int>(plain.size()), &rect, alignment | DT_NOCLIP, data.fontColorArgb);
    if (FAILED(sprite->End()) || (!drawn && !colors.empty()) || FAILED(device->GetRenderTargetData(target.Get(), readback.Get())))
    {
        return false;
    }
    D3DLOCKED_RECT locked {};
    if (FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
    {
        return false;
    }
    for (int y = 0; y < height; ++y)
    {
        const auto* row = reinterpret_cast<const unsigned int*>(
            static_cast<const char*>(locked.pBits) + static_cast<size_t>(y) * locked.Pitch);
        for (int x = 0; x < width; ++x)
        {
            pixels[static_cast<size_t>(y) * width + x] = row[x] | (data.backgroundColorArgb ? 0xFF000000 : 0);
        }
    }
    return SUCCEEDED(readback->UnlockRect());
}

void ReleaseMaterialTextRenderer()
{
    g_resources = {};
}
