// Source Repository: https://github.com/waverso/waversomapstudio

#include <Windows.h>
#include <cstddef>

#include "editor_shaders.h"
#include "outline_renderer.h"

void OutlineRenderer::ReleaseResources()
{
    if (m_maskDs)
    {
        m_maskDs->Release();
        m_maskDs = nullptr;
    }
    if (m_maskSurfMsaa)
    {
        m_maskSurfMsaa->Release();
        m_maskSurfMsaa = nullptr;
    }
    if (m_maskTextureSurf)
    {
        m_maskTextureSurf->Release();
        m_maskTextureSurf = nullptr;
    }
    if (m_maskTexture)
    {
        m_maskTexture->Release();
        m_maskTexture = nullptr;
    }
    m_width = 0;
    m_height = 0;
    m_msType = D3DMULTISAMPLE_NONE;
    m_msQuality = 0;
    m_targetFormat = D3DFMT_UNKNOWN;
    m_maskFormat = D3DFMT_UNKNOWN;
    m_depthFormat = D3DFMT_UNKNOWN;
}

bool OutlineRenderer::EnsureResources()
{
    IDirect3DSurface9* renderTarget = nullptr;
    if (FAILED(m_device->GetRenderTarget(0, &renderTarget)) || !renderTarget)
    {
        return false;
    }

    D3DSURFACE_DESC targetDesc = {};
    const HRESULT targetDescResult = renderTarget->GetDesc(&targetDesc);
    renderTarget->Release();
    if (FAILED(targetDescResult) || !targetDesc.Width || !targetDesc.Height)
    {
        return false;
    }

    D3DFORMAT depthFormat = D3DFMT_UNKNOWN;
    IDirect3DSurface9* depthStencil = nullptr;
    D3DSURFACE_DESC depthDesc = {};
    if (SUCCEEDED(m_device->GetDepthStencilSurface(&depthStencil)) && depthStencil)
    {
        if (SUCCEEDED(depthStencil->GetDesc(&depthDesc)))
        {
            depthFormat = depthDesc.Format;
        }
        depthStencil->Release();
    }

    if (m_maskTexture && m_maskTextureSurf && targetDesc.Width == m_width && targetDesc.Height == m_height
        && targetDesc.MultiSampleType == m_msType && targetDesc.MultiSampleQuality == m_msQuality
        && targetDesc.Format == m_targetFormat && depthFormat == m_depthFormat)
    {
        return true;
    }

    ReleaseResources();

    const D3DFORMAT candidates[3] = { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, targetDesc.Format };
    for (size_t candidate = 0; candidate < 3; ++candidate)
    {
        const D3DFORMAT format = candidates[candidate];
        bool duplicate = false;
        for (size_t previous = 0; previous < candidate; ++previous)
        {
            if (candidates[previous] == format)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate || format == D3DFMT_UNKNOWN)
        {
            continue;
        }

        IDirect3DTexture9* texture = nullptr;
        IDirect3DSurface9* textureSurface = nullptr;
        IDirect3DSurface9* multisampledSurface = nullptr;

        HRESULT result = m_device->CreateTexture(
            targetDesc.Width, targetDesc.Height, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &texture, nullptr);

        if (SUCCEEDED(result) && texture)
        {
            result = texture->GetSurfaceLevel(0, &textureSurface);
        }

        if (SUCCEEDED(result) && targetDesc.MultiSampleType != D3DMULTISAMPLE_NONE)
        {
            result = m_device->CreateRenderTarget(targetDesc.Width, targetDesc.Height, format, targetDesc.MultiSampleType,
                targetDesc.MultiSampleQuality, FALSE, &multisampledSurface, nullptr);
        }

        if (SUCCEEDED(result) && texture && textureSurface)
        {
            m_maskTexture = texture;
            m_maskTextureSurf = textureSurface;
            m_maskSurfMsaa = multisampledSurface;
            m_maskFormat = format;
            break;
        }

        if (multisampledSurface)
        {
            multisampledSurface->Release();
        }
        if (textureSurface)
        {
            textureSurface->Release();
        }
        if (texture)
        {
            texture->Release();
        }
    }

    if (!m_maskTexture || !m_maskTextureSurf)
    {
        return false;
    }

    if (depthFormat != D3DFMT_UNKNOWN && depthDesc.Width == targetDesc.Width && depthDesc.Height == targetDesc.Height
        && depthDesc.MultiSampleType == targetDesc.MultiSampleType
        && depthDesc.MultiSampleQuality == targetDesc.MultiSampleQuality)
    {
        m_device->CreateDepthStencilSurface(targetDesc.Width, targetDesc.Height, depthFormat, targetDesc.MultiSampleType,
            targetDesc.MultiSampleQuality, TRUE, &m_maskDs, nullptr);
    }

    m_width = targetDesc.Width;
    m_height = targetDesc.Height;
    m_msType = targetDesc.MultiSampleType;
    m_msQuality = targetDesc.MultiSampleQuality;
    m_targetFormat = targetDesc.Format;
    m_depthFormat = depthFormat;
    return true;
}

bool OutlineRenderer::EnsureShader()
{
    if (m_pixelShader && m_maskPixelShader)
    {
        return true;
    }
    const auto now = GetTickCount64();
    if (m_shaderUnsupported || now < m_shaderRetryAtMs)
    {
        return false;
    }
    m_shaderRetryAtMs = now + 1000;

    D3DCAPS9 caps = {};
    if (FAILED(m_device->GetDeviceCaps(&caps)))
    {
        return false;
    }
    if (caps.PixelShaderVersion < D3DPS_VERSION(3, 0))
    {
        m_shaderUnsupported = true;
        return false;
    }

    if (FAILED(m_device->CreatePixelShader(g_kMaskPs, &m_maskPixelShader)) || !m_maskPixelShader)
    {
        return false;
    }
    if (FAILED(m_device->CreatePixelShader(g_kOutlinePs, &m_pixelShader)) || !m_pixelShader)
    {
        m_maskPixelShader->Release();
        m_maskPixelShader = nullptr;
        return false;
    }

    return true;
}
