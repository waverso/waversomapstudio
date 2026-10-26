// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <d3d9.h>
#include <unordered_map>

class CEntity;

class OutlineRenderer
{
    public:
    OutlineRenderer();
    ~OutlineRenderer();

    void Render();
    void OnDeviceLost();
    void OnSelectedEntityRender(CEntity* pEntity);
    void OnEntityRemove(CEntity* pEntity);
    bool IsInMaskPass() const
    {
        return m_inMaskPass;
    }

    float m_widthPx = 0.5f;
    float m_occludedAlpha = 0.3f;
    float m_slopeBias = -0.4f;

    private:
    void ReleaseResources();
    bool EnsureResources();
    bool EnsureShader();
    bool RenderMask(IDirect3DSurface9* pWorldDS, const D3DVIEWPORT9& viewport);
    bool RenderComposite(const D3DVIEWPORT9& viewport);

    IDirect3DDevice9* m_device = nullptr;
    IDirect3DTexture9* m_maskTexture = nullptr;
    IDirect3DSurface9* m_maskTextureSurf = nullptr;
    IDirect3DSurface9* m_maskSurfMsaa = nullptr;
    IDirect3DSurface9* m_maskDs = nullptr;
    IDirect3DPixelShader9* m_pixelShader = nullptr;
    IDirect3DPixelShader9* m_maskPixelShader = nullptr;
    UINT m_width = 0;
    UINT m_height = 0;
    D3DMULTISAMPLE_TYPE m_msType = D3DMULTISAMPLE_NONE;
    DWORD m_msQuality = 0;
    D3DFORMAT m_targetFormat = D3DFMT_UNKNOWN;
    D3DFORMAT m_maskFormat = D3DFMT_UNKNOWN;
    D3DFORMAT m_depthFormat = D3DFMT_UNKNOWN;
    ULONGLONG m_shaderRetryAtMs = 0;
    bool m_shaderUnsupported = false;
    bool m_inMaskPass = false;
    std::unordered_map<CEntity*, unsigned int> m_selectedEntities;
};

extern OutlineRenderer* g_outlineRenderer;
