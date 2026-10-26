// Source Repository: https://github.com/waverso/waversomapstudio

#include "outline_renderer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../vendor/imgui/imgui.h"
#include "CEntity.h"
#include "CTimer.h"
#include "CVisibilityPlugins.h"
#include "../actors/actor_runtime.h"
#include "../scene/free_camera.h"
#include "../hooks/game_hooks.h"
#include "../scene/selection.h"
#include "../ui/theme.h"

#include "outline_renderer_helpers.inl"

OutlineRenderer::OutlineRenderer()
{
    if (!g_gameHooks)
    {
        return;
    }
    m_device = g_gameHooks->m_device;
}

OutlineRenderer::~OutlineRenderer()
{
    ReleaseResources();
    if (m_pixelShader)
    {
        m_pixelShader->Release();
        m_pixelShader = nullptr;
    }
    if (m_maskPixelShader)
    {
        m_maskPixelShader->Release();
        m_maskPixelShader = nullptr;
    }
}

void OutlineRenderer::Render()
{
    if (!m_device || !g_gameHooks || !g_freeCamera || !g_freeCamera->m_active || !Selection::GetDrawHighlight()
        || m_selectedEntities.empty())
    {
        return;
    }
    if (!EnsureShader() || !EnsureResources())
    {
        return;
    }

    DeviceStateGuard state(m_device);
    if (!state.IsValid())
    {
        return;
    }

    const bool rendered = RenderMask(state.DepthStencil(), state.Viewport());
    const bool targetsRestored = state.RestoreTargets();

    bool maskReady = rendered && targetsRestored;
    if (maskReady && m_maskSurfMsaa)
    {
        maskReady = SUCCEEDED(m_device->StretchRect(m_maskSurfMsaa, nullptr, m_maskTextureSurf, nullptr, D3DTEXF_NONE));
    }

    if (maskReady)
    {
        RenderComposite(state.Viewport());
    }
    state.Restore();
}

void OutlineRenderer::OnDeviceLost()
{
    m_inMaskPass = false;
    m_shaderRetryAtMs = 0;
    m_shaderUnsupported = false;
    ReleaseResources();
}

void OutlineRenderer::OnSelectedEntityRender(CEntity* pEntity)
{
    if (pEntity)
    {
        m_selectedEntities[pEntity] = CTimer::m_FrameCounter;
    }
}

void OutlineRenderer::OnEntityRemove(CEntity* pEntity)
{
    m_selectedEntities.erase(pEntity);
}

bool OutlineRenderer::RenderMask(IDirect3DSurface9* pWorldDS, const D3DVIEWPORT9& viewport)
{
    std::vector<CEntity*> entities;
    entities.reserve(m_selectedEntities.size());
    for (auto it = m_selectedEntities.begin(); it != m_selectedEntities.end();)
    {
        if (it->second != CTimer::m_FrameCounter)
        {
            it = m_selectedEntities.erase(it);
            continue;
        }
        if (it->first && it->first->m_pRwObject)
        {
            entities.push_back(it->first);
        }
        ++it;
    }
    if (entities.empty())
    {
        return false;
    }

    IDirect3DSurface9* maskTarget = m_maskSurfMsaa ? m_maskSurfMsaa : m_maskTextureSurf;
    if (!maskTarget)
    {
        return false;
    }
    for (DWORD target = 1; target < 4; ++target)
    {
        m_device->SetRenderTarget(target, nullptr);
    }
    m_device->SetDepthStencilSurface(nullptr);
    if (FAILED(m_device->SetRenderTarget(0, maskTarget)))
    {
        return false;
    }
    if (FAILED(m_device->SetViewport(&viewport)))
    {
        return false;
    }
    if (FAILED(m_device->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0)))
    {
        return false;
    }

    m_device->SetVertexShader(nullptr);
    m_device->SetPixelShader(m_maskPixelShader);
    for (DWORD stage = 0; stage < 8; ++stage)
    {
        m_device->SetTexture(stage, nullptr);
    }

    m_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    m_device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_device->SetRenderState(D3DRS_COLORWRITEENABLE,
        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    m_device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    m_device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    m_device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, m_msType != D3DMULTISAMPLE_NONE);
    m_device->SetRenderState(D3DRS_DEPTHBIAS, FloatBits(0.0f));
    m_device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, FloatBits(0.0f));

    MaskPassGuard maskPass(m_inMaskPass);

    auto renderPass = [&](DWORD colorWriteEnable, DWORD zEnable, D3DCMPFUNC zFunc, BOOL zWrite)
    {
        m_device->SetRenderState(D3DRS_ZENABLE, zEnable);
        m_device->SetRenderState(D3DRS_ZFUNC, zFunc);
        m_device->SetRenderState(D3DRS_ZWRITEENABLE, zWrite);
        m_device->SetRenderState(D3DRS_COLORWRITEENABLE, colorWriteEnable);

        bool rendered = false;
        for (size_t index = 0; index < entities.size(); ++index)
        {
            const float normalizedId = static_cast<float>((index % 254) + 1) / 255.0f;
            const float shaderConstant[4] = { normalizedId, normalizedId, normalizedId, 1.0f };
            m_device->SetPixelShaderConstantF(0, shaderConstant, 1);
            rendered = (entities[index]->m_nType == ENTITY_TYPE_PED ? DrawActorMask(m_device, entities[index])
                                                                    : DrawEntityMaskInstanced(m_device, entities[index]))
                || rendered;
        }
        return rendered;
    };

    bool rendered = false;
    if (m_maskDs && SUCCEEDED(m_device->SetDepthStencilSurface(m_maskDs))
        && SUCCEEDED(m_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 1.0f, 0)))
    {
        rendered = renderPass(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_BLUE, D3DZB_TRUE, D3DCMP_LESSEQUAL, TRUE);
    }
    else
    {
        m_device->SetDepthStencilSurface(nullptr);
        rendered = renderPass(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_BLUE, D3DZB_FALSE, D3DCMP_ALWAYS, FALSE);
    }

    if (pWorldDS && SUCCEEDED(m_device->SetDepthStencilSurface(pWorldDS)))
    {
        m_device->SetRenderState(D3DRS_DEPTHBIAS, FloatBits(0.0f));
        m_device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, FloatBits(m_slopeBias));
        rendered = renderPass(D3DCOLORWRITEENABLE_GREEN, D3DZB_TRUE, D3DCMP_LESSEQUAL, FALSE) || rendered;
    }
    else
    {
        m_device->SetDepthStencilSurface(nullptr);
        rendered = renderPass(D3DCOLORWRITEENABLE_GREEN, D3DZB_FALSE, D3DCMP_ALWAYS, FALSE) || rendered;
    }

    return rendered;
}

bool OutlineRenderer::RenderComposite(const D3DVIEWPORT9& viewport)
{
    if (!m_maskTexture || !m_width || !m_height)
    {
        return false;
    }

    struct Vertex
    {
        float x, y, z, rhw, u, v;
    };
    const float width = static_cast<float>(m_width);
    const float height = static_cast<float>(m_height);
    const Vertex quad[4] = { { -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f }, { width - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
        { -0.5f, height - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f }, { width - 0.5f, height - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f } };

    m_device->SetVertexShader(nullptr);
    if (FAILED(m_device->SetPixelShader(m_pixelShader)))
    {
        return false;
    }
    if (FAILED(m_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1)))
    {
        return false;
    }
    for (DWORD target = 1; target < 4; ++target)
    {
        m_device->SetRenderTarget(target, nullptr);
    }

    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    m_device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_device->SetRenderState(D3DRS_COLORWRITEENABLE,
        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    LONG vpx = static_cast<LONG>(viewport.X), vpy = static_cast<LONG>(viewport.Y);
    const float radius = std::max(m_widthPx, 0.65f);
    const LONG border = static_cast<LONG>(ceilf(std::max(radius * 1.35f, radius + 1.5f)));
    RECT compositeClip = { vpx, vpy, vpx + static_cast<LONG>(viewport.Width) - border, vpy + static_cast<LONG>(viewport.Height) };
    if (FAILED(m_device->SetScissorRect(&compositeClip)))
    {
        return false;
    }
    m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    m_device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    m_device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    m_device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, m_msType != D3DMULTISAMPLE_NONE);

    if (FAILED(m_device->SetTexture(3, m_maskTexture)) || FAILED(m_device->SetTexture(4, m_maskTexture)))
    {
        return false;
    }
    m_device->SetSamplerState(3, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    m_device->SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    m_device->SetSamplerState(3, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    m_device->SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_device->SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    m_device->SetSamplerState(4, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_device->SetSamplerState(4, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    m_device->SetSamplerState(4, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    m_device->SetSamplerState(4, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_device->SetSamplerState(4, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    m_device->SetSamplerState(4, D3DSAMP_SRGBTEXTURE, FALSE);

    const float parameters[4] = { 1.0f / width, 1.0f / height, m_widthPx, m_occludedAlpha };
    const ImVec4 color = ImGui::ColorConvertU32ToFloat4(Theme::g_accent);

    if (FAILED(m_device->SetPixelShaderConstantF(0, parameters, 1)))
    {
        return false;
    }
    if (FAILED(m_device->SetPixelShaderConstantF(1, &color.x, 1)))
    {
        return false;
    }
    return SUCCEEDED(m_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex)));
}
