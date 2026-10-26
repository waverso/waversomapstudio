// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <d3d9.h>

namespace LineRenderer
{
    enum class Depth
    {
        Scene,
        Visible,
        Overlay
    };

    struct Vertex
    {
        float x, y, z;
        D3DCOLOR color;
    };

    struct Style
    {
        Depth depth = Depth::Scene;
        bool vertexColors = true;
        D3DCOLOR color = 0xFFFFFFFFu;
        BYTE hiddenAlpha = 80;
        BYTE inversionAlpha = 100;
    };

    template <class Device, class Draw>
    void Render(Device* device, const Style& style, const Draw& draw)
    {
        device->SetVertexShader(nullptr);
        device->SetPixelShader(nullptr);
        for (DWORD stage = 0; stage < 8; ++stage)
        {
            device->SetTexture(stage, nullptr);
            device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_DISABLE);
            device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        }
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        const DWORD colorSource = style.vertexColors ? D3DTA_DIFFUSE : D3DTA_TFACTOR;
        device->SetTextureStageState(0, D3DTSS_COLORARG1, colorSource);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, colorSource);
        device->SetRenderState(D3DRS_TEXTUREFACTOR, style.color);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_ZENABLE, style.depth == Depth::Overlay ? D3DZB_FALSE : D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        draw();

        if (style.depth == Depth::Scene)
        {
            const DWORD tintAlpha = (DWORD(style.hiddenAlpha) * (255 - style.inversionAlpha) + 127) / 255;
            device->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATER);
            if (style.inversionAlpha)
            {
                const DWORD remaining = 255 - tintAlpha;
                const DWORD contrastAlpha = (DWORD(style.inversionAlpha) * 255 + remaining / 2) / remaining;
                device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_INVDESTCOLOR);
                device->SetRenderState(
                    D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(contrastAlpha, contrastAlpha, contrastAlpha, contrastAlpha));
                device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
                device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
                draw();
            }

            device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            if (tintAlpha)
            {
                device->SetRenderState(D3DRS_TEXTUREFACTOR, (style.color & 0x00FFFFFFu) | (tintAlpha << 24));
                device->SetTextureStageState(0, D3DTSS_COLORARG1, colorSource);
                device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
                draw();
            }
        }

        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    }

    template <class Device>
    void Draw(Device* device, D3DPRIMITIVETYPE type, UINT count, const void* vertices, UINT stride, Depth depth = Depth::Scene)
    {
        if (count)
        {
            Render(device, { .depth = depth },
                [&]
                {
                    device->DrawPrimitiveUP(type, count, vertices, stride);
                });
        }
    }
}
