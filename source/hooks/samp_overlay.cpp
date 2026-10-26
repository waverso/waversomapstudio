// Source Repository: https://github.com/waverso/waversomapstudio

#include <Windows.h>

#include "../vendor/detours/detours.h"
#include "../scene/free_camera.h"
#include "game_hooks.h"
#include "samp.h"

bool SampIntegration::InstallOverlayHooks()
{
    if (m_overlayHooksInstalled)
    {
        return true;
    }
    if (!m_base || !m_overlay.labelDraw || !m_overlay.playerTagLabel || !m_overlay.playerTagHealth || !m_overlay.projection)
    {
        return false;
    }

    m_originalLabelDraw = reinterpret_cast<LabelDrawFn>(m_base + m_overlay.labelDraw);
    m_originalPlayerTagLabel = reinterpret_cast<PlayerTagLabelFn>(m_base + m_overlay.playerTagLabel);
    m_originalPlayerTagHealth = reinterpret_cast<PlayerTagHealthFn>(m_base + m_overlay.playerTagHealth);

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<PVOID*>(&m_originalLabelDraw), HookLabelDraw);
    DetourAttach(reinterpret_cast<PVOID*>(&m_originalPlayerTagLabel), HookPlayerTagLabel);
    DetourAttach(reinterpret_cast<PVOID*>(&m_originalPlayerTagHealth), HookPlayerTagHealth);
    if (DetourTransactionCommit() != NO_ERROR)
    {
        m_originalLabelDraw = nullptr;
        m_originalPlayerTagLabel = nullptr;
        m_originalPlayerTagHealth = nullptr;
        return false;
    }

    m_overlayHooksInstalled = true;
    return true;
}

void SampIntegration::RemoveOverlayHooks()
{
    if (!m_overlayHooksInstalled)
    {
        return;
    }
    if (m_overlayShiftDepth != 0)
    {
        *reinterpret_cast<D3DMATRIX*>(m_base + m_overlay.projection) = m_overlayProjectionSaved;
        m_overlayShiftDepth = 0;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(reinterpret_cast<PVOID*>(&m_originalLabelDraw), HookLabelDraw);
    DetourDetach(reinterpret_cast<PVOID*>(&m_originalPlayerTagLabel), HookPlayerTagLabel);
    DetourDetach(reinterpret_cast<PVOID*>(&m_originalPlayerTagHealth), HookPlayerTagHealth);
    DetourTransactionCommit();
    m_overlayHooksInstalled = false;
}

bool SampIntegration::BeginOverlayProjectionShift()
{
    if (!m_overlayHooksInstalled || !g_freeCamera || !g_gameHooks || !g_gameHooks->m_device)
    {
        return false;
    }

    float offsetX = 0.0f;
    if (!g_freeCamera->GetVisualOverlayOffsetX(offsetX))
    {
        return false;
    }

    if (m_overlayShiftDepth++ != 0)
    {
        return true;
    }

    D3DVIEWPORT9 viewport = {};
    if (FAILED(g_gameHooks->m_device->GetViewport(&viewport)) || viewport.Width == 0)
    {
        m_overlayShiftDepth = 0;
        return false;
    }

    auto* projection = reinterpret_cast<D3DMATRIX*>(m_base + m_overlay.projection);
    m_overlayProjectionSaved = *projection;
    const float clipOffset = 2.0f * offsetX / static_cast<float>(viewport.Width);
    projection->_11 += clipOffset * projection->_14;
    projection->_21 += clipOffset * projection->_24;
    projection->_31 += clipOffset * projection->_34;
    projection->_41 += clipOffset * projection->_44;
    return true;
}

void SampIntegration::EndOverlayProjectionShift()
{
    if (m_overlayShiftDepth == 0)
    {
        return;
    }
    if (--m_overlayShiftDepth != 0)
    {
        return;
    }
    *reinterpret_cast<D3DMATRIX*>(m_base + m_overlay.projection) = m_overlayProjectionSaved;
}

void __fastcall SampIntegration::HookLabelDraw(
    void* self, void*, CVector* position, const char* text, D3DCOLOR color, BOOL shadow, bool noObstacles)
{
    SampIntegration* samp = g_sampIntegration;
    const bool shifted = samp && samp->BeginOverlayProjectionShift();
    if (samp && samp->m_originalLabelDraw)
    {
        samp->m_originalLabelDraw(self, position, text, color, shadow, noObstacles);
    }
    if (shifted)
    {
        samp->EndOverlayProjectionShift();
    }
}

void __fastcall SampIntegration::HookPlayerTagLabel(
    void* self, void*, CVector* position, const char* text, D3DCOLOR color, float distance, bool drawStatus, int status)
{
    SampIntegration* samp = g_sampIntegration;
    const bool shifted = samp && samp->BeginOverlayProjectionShift();
    if (samp && samp->m_originalPlayerTagLabel)
    {
        samp->m_originalPlayerTagLabel(self, position, text, color, distance, drawStatus, status);
    }
    if (shifted)
    {
        samp->EndOverlayProjectionShift();
    }
}

void __fastcall SampIntegration::HookPlayerTagHealth(
    void* self, void*, CVector* position, float health, float armour, float distance)
{
    SampIntegration* samp = g_sampIntegration;
    const bool shifted = samp && samp->BeginOverlayProjectionShift();
    if (samp && samp->m_originalPlayerTagHealth)
    {
        samp->m_originalPlayerTagHealth(self, position, health, armour, distance);
    }
    if (shifted)
    {
        samp->EndOverlayProjectionShift();
    }
}
