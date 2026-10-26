// Source Repository: https://github.com/waverso/waversomapstudio

static D3DMATRIX& RenderWareProjectionMatrix()
{
    return *reinterpret_cast<D3DMATRIX*>(0x8E2458);
}

static D3DMATRIX*& RenderWareActiveViewProjection()
{
    return *reinterpret_cast<D3DMATRIX**>(0xC97C64);
}

void FreeCamera::BeginVisualNavigationRender()
{
    if (m_visualViewportActive || !m_active || !g_gameHooks || !g_gameHooks->m_device || !g_gui)
    {
        return;
    }

    if (m_sidebarIntroElapsed >= SidebarIntroDurationSeconds)
    {
        g_gui->m_sidebarOffsetRatio = g_gui->GetSidebarWidthRatio();
    }

    m_visualRayStateValid = false;
    IDirect3DDevice9* device = g_gameHooks->m_device;
    if (FAILED(device->GetViewport(&m_visualViewportSaved))
        || FAILED(device->GetTransform(D3DTS_PROJECTION, &m_visualProjectionSaved)) || m_visualViewportSaved.Width == 0)
    {
        return;
    }
    m_visualRwProjectionSaved = RenderWareProjectionMatrix();

    const float visibleRatio = std::clamp(1.0f - g_gui->m_sidebarOffsetRatio, 0.1f, 1.0f);
    m_visualViewportApplied = m_visualViewportSaved;
    m_visualViewportApplied.Width
        = std::max<DWORD>(1, static_cast<DWORD>(std::lround(static_cast<float>(m_visualViewportSaved.Width) * visibleRatio)));
    const float appliedRatio
        = static_cast<float>(m_visualViewportApplied.Width) / static_cast<float>(m_visualViewportSaved.Width);

    m_visualProjectionApplied = m_visualProjectionSaved;
    m_visualProjectionApplied._11 /= appliedRatio;

    if (FAILED(device->SetViewport(&m_visualViewportApplied)))
    {
        return;
    }
    RenderWareProjectionMatrix() = m_visualProjectionApplied;
    RenderWareActiveViewProjection() = nullptr;
    RwD3D9SetTransform(D3DTS_PROJECTION, &m_visualProjectionApplied);
    if (FAILED(device->SetTransform(D3DTS_PROJECTION, &m_visualProjectionApplied)))
    {
        RenderWareProjectionMatrix() = m_visualRwProjectionSaved;
        RenderWareActiveViewProjection() = nullptr;
        RwD3D9SetTransform(D3DTS_PROJECTION, &m_visualProjectionSaved);
        device->SetTransform(D3DTS_PROJECTION, &m_visualProjectionSaved);
        device->SetViewport(&m_visualViewportSaved);
        return;
    }
    m_visualViewportActive = true;
    m_visualRayStateValid = true;
}

void FreeCamera::ApplyVisualNavigationRender()
{
    if (!m_visualViewportActive || !g_gameHooks || !g_gameHooks->m_device)
    {
        return;
    }
    IDirect3DDevice9* device = g_gameHooks->m_device;
    RenderWareProjectionMatrix() = m_visualProjectionApplied;
    RenderWareActiveViewProjection() = nullptr;
    device->SetViewport(&m_visualViewportApplied);
    RwD3D9SetTransform(D3DTS_PROJECTION, &m_visualProjectionApplied);
    device->SetTransform(D3DTS_PROJECTION, &m_visualProjectionApplied);
}

void FreeCamera::SetVisualWorldProjectionPass(bool active)
{
    m_visualWorldProjectionPass = active && m_visualViewportActive;
}

bool FreeCamera::RemapVisualScreenCoordinates(RwV3d& screen) const
{
    if (!m_visualViewportActive || !m_visualWorldProjectionPass)
    {
        return false;
    }

    const float nativeCenterX
        = static_cast<float>(m_visualViewportSaved.X) + static_cast<float>(m_visualViewportSaved.Width) * 0.5f;
    const float visualCenterX
        = static_cast<float>(m_visualViewportApplied.X) + static_cast<float>(m_visualViewportApplied.Width) * 0.5f;
    screen.x += visualCenterX - nativeCenterX;
    return true;
}

bool FreeCamera::GetVisualOverlayOffsetX(float& outOffset) const
{
    outOffset = 0.0f;
    if (!m_active || !m_visualRayStateValid)
    {
        return false;
    }

    const float nativeCenterX
        = static_cast<float>(m_visualViewportSaved.X) + static_cast<float>(m_visualViewportSaved.Width) * 0.5f;
    const float visualCenterX
        = static_cast<float>(m_visualViewportApplied.X) + static_cast<float>(m_visualViewportApplied.Width) * 0.5f;
    outOffset = visualCenterX - nativeCenterX;
    return fabsf(outOffset) > 0.001f;
}

bool FreeCamera::ScreenToWorldRayForPicking(float sx, float sy, CVector& outOrigin, CVector& outDir) const
{
    if (!g_gameHooks || !g_gameHooks->m_device)
    {
        return false;
    }

    IDirect3DDevice9* device = g_gameHooks->m_device;
    if (!m_active || !m_visualRayStateValid)
    {
        return Utils::ScreenToWorldRay(device, sx, sy, outOrigin, outDir);
    }

    D3DXMATRIX view = {};
    if (FAILED(device->GetTransform(D3DTS_VIEW, &view)))
    {
        return false;
    }
    return Utils::ScreenToWorldRayWithMatrices(
        sx, sy, m_visualViewportApplied, m_visualProjectionApplied, view, outOrigin, outDir);
}

void FreeCamera::EndVisualNavigationRender()
{
    if (!m_visualViewportActive)
    {
        return;
    }
    m_visualWorldProjectionPass = false;
    m_visualViewportActive = false;
    if (!g_gameHooks || !g_gameHooks->m_device)
    {
        return;
    }

    IDirect3DDevice9* device = g_gameHooks->m_device;
    RenderWareProjectionMatrix() = m_visualRwProjectionSaved;
    RenderWareActiveViewProjection() = nullptr;
    RwD3D9SetTransform(D3DTS_PROJECTION, &m_visualProjectionSaved);
    device->SetTransform(D3DTS_PROJECTION, &m_visualProjectionSaved);
    device->SetViewport(&m_visualViewportSaved);
}
