// Source Repository: https://github.com/waverso/waversomapstudio

float Gizmos::GetAxisIndicatorLeft() const
{
    const auto display = ImGui::GetIO().DisplaySize;
    D3DVIEWPORT9 viewport {};
    const float ratio = g_gui ? g_gui->GetSidebarVisualWidthRatio() : 0.15f;
    if (!m_device || FAILED(m_device->GetViewport(&viewport)) || !viewport.Width || !viewport.Height)
    {
        return display.x * (1.0f - ratio) - Ui::RoundedPx(IndMarginNative) - Ui::RoundedPx(IndViewNative * m_axisIndicatorScale);
    }
    const float left = static_cast<float>(viewport.X);
    const float right = left + viewport.Width;
    const float size = std::clamp(Ui::RoundedPx(IndViewNative * m_axisIndicatorScale), 1.0f,
        static_cast<float>((std::min)(viewport.Width, viewport.Height)));
    return Ui::Round(
        std::clamp(ImMin(display.x * (1.0f - ratio), right) - Ui::RoundedPx(IndMarginNative) - size, left, right - size));
}

void Gizmos::RenderAxisIndicator()
{
    m_indLabelValid = false;
    if (!m_device || !g_freeCamera || !g_freeCamera->m_active)
    {
        return;
    }

    D3DXMATRIX gameView = {};
    D3DVIEWPORT9 gameVp = {};
    if (FAILED(m_device->GetTransform(D3DTS_VIEW, &gameView)) || FAILED(m_device->GetViewport(&gameVp)))
    {
        return;
    }

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (gameVp.Width == 0 || gameVp.Height == 0 || displaySize.x <= 0.0f || displaySize.y <= 0.0f)
    {
        return;
    }

    IDirect3DStateBlock9* sb = nullptr;
    if (FAILED(m_device->CreateStateBlock(D3DSBT_ALL, &sb)) || !sb)
    {
        return;
    }
    if (FAILED(sb->Capture()))
    {
        sb->Release();
        return;
    }

    m_device->SetVertexShader(nullptr);
    m_device->SetPixelShader(nullptr);
    for (int i = 0; i < 8; i++)
    {
        m_device->SetTexture(i, nullptr);
        m_device->SetTextureStageState(i, D3DTSS_COLOROP, D3DTOP_DISABLE);
        m_device->SetTextureStageState(i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }
    m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    m_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    m_device->SetPixelShader(nullptr);
    m_device->SetVertexShader(nullptr);
    m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
    m_device->SetRenderState(D3DRS_AMBIENT, 0x707070);
    m_device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    m_device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
    m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);

    const float viewportX = static_cast<float>(gameVp.X);
    const float viewportY = static_cast<float>(gameVp.Y);
    const float viewportW = static_cast<float>(gameVp.Width);
    const float viewportH = static_cast<float>(gameVp.Height);

    const float indicatorView
        = std::clamp(Ui::RoundedPx(IndViewNative * m_axisIndicatorScale), 1.0f, std::min(viewportW, viewportH));
    const float margin = std::max(0.0f, Ui::RoundedPx(IndMarginNative));
    const float sidebarRatio = std::clamp(g_gui ? g_gui->GetSidebarVisualWidthRatio() : 0.15f, 0.0f, 1.0f);

    const float sidebarBoundaryX = displaySize.x * (1.0f - sidebarRatio);
    const float indicatorRight = ImMin(sidebarBoundaryX, viewportX + viewportW) - margin;
    const float maxX = viewportX + viewportW - indicatorView;
    const float maxY = viewportY + viewportH - indicatorView;
    const float vx = Ui::Round(std::clamp(indicatorRight - indicatorView, viewportX, maxX));
    const float vy = Ui::Round(std::clamp(viewportY + margin, viewportY, maxY));

    D3DRECT rect = { static_cast<LONG>(vx), static_cast<LONG>(vy), static_cast<LONG>(vx + indicatorView),
        static_cast<LONG>(vy + indicatorView) };
    m_device->Clear(1, &rect, D3DCLEAR_ZBUFFER, 0xFFFFFFFF, 1.f, 0);

    D3DXMATRIX invView;
    D3DXMatrixInverse(&invView, nullptr, &gameView);
    D3DXVECTOR3 camFwd(invView._31, invView._32, invView._33);
    D3DXVec3Normalize(&camFwd, &camFwd);

    D3DLIGHT9 lt;
    ZeroMemory(&lt, sizeof(lt));
    lt.Type = D3DLIGHT_DIRECTIONAL;
    lt.Diffuse = { 0.9f, 0.9f, 0.9f, 1.f };
    lt.Direction = camFwd;
    m_device->SetLight(7, &lt);
    m_device->LightEnable(7, TRUE);

    D3DXMATRIX indView = gameView;
    indView._41 = 0.f;
    indView._42 = 0.f;
    indView._43 = IndCameraDistance;
    m_device->SetTransform(D3DTS_VIEW, &indView);

    const float wScale = viewportW / indicatorView;
    const float hScale = viewportH / indicatorView;
    D3DXMATRIX proj;
    D3DXMatrixOrthoLH(&proj, 2.f * IndOrthoExt * wScale, 2.f * IndOrthoExt * hScale, IndNear, IndFar);

    const float targetX = vx + indicatorView * 0.5f;
    const float targetY = vy + indicatorView * 0.5f;
    proj._41 = ((targetX - viewportX) / viewportW) * 2.0f - 1.0f;
    proj._42 = 1.0f - ((targetY - viewportY) / viewportH) * 2.0f;
    m_device->SetTransform(D3DTS_PROJECTION, &proj);

    auto DrawInd = [&](const D3D9ProceduralMesh& mesh, const D3DXMATRIX& world, D3DCOLOR col)
    {
        D3DMATERIAL9 mtrl;
        ZeroMemory(&mtrl, sizeof(mtrl));
        D3DXCOLOR c(col);
        mtrl.Diffuse = c;
        mtrl.Ambient = c;
        m_device->SetMaterial(&mtrl);
        m_device->SetTransform(D3DTS_WORLD, &world);
        mesh.Draw();
    };

    D3DXMATRIX rotX, rotY, rotZ;
    D3DXMatrixRotationY(&rotX, D3DXToRadian(90.f));
    D3DXMatrixRotationX(&rotY, D3DXToRadian(-90.f));
    D3DXMatrixIdentity(&rotZ);

    const float barSX = IndBarR / TorusTube;
    const float barSZ = (2.f * IndBarHalf) / ShaftLength;
    const float coneSX = IndConeR / ConeBaseR;
    const float coneSZ = IndConeLength / ConeLength;

    D3DXMATRIX barS, coneS, coneT, mw;
    D3DXMatrixScaling(&barS, barSX, barSX, barSZ);
    D3DXMatrixScaling(&coneS, coneSX, coneSX, coneSZ);
    D3DXMatrixTranslation(&coneT, 0.f, 0.f, IndBarHalf + IndConeLength * 0.5f);

    auto DrawArrow = [&](const D3DXMATRIX& rot, D3DCOLOR col)
    {
        mw = barS * rot;
        DrawInd(m_cylinderMesh, mw, col);
        mw = coneS * coneT * rot;
        DrawInd(m_coneMesh, mw, col);
    };

    DrawArrow(rotX, ColRed);
    DrawArrow(rotY, ColGreen);
    DrawArrow(rotZ, ColBlue);

    D3DXMATRIX ident;
    D3DXMatrixIdentity(&ident);
    const D3DXVECTOR3 axisDir[3] = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f } };
    for (int i = 0; i < 3; i++)
    {
        D3DXVECTOR3 tip = axisDir[i] * (IndBarHalf + IndConeLength + IndLabelGap);
        D3DXVECTOR3 scr;
        D3DXVec3Project(&scr, &tip, &gameVp, &proj, &indView, &ident);
        m_indLabelPosition[i] = ImVec2(scr.x, scr.y);
    }

    const HRESULT restoreResult = sb->Apply();
    sb->Release();
    if (FAILED(restoreResult))
    {
        return;
    }
    m_indLabelValid = true;
}

void Gizmos::DrawAxisIndicatorLabels()
{
    if (!m_indLabelValid)
    {
        return;
    }

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize() * m_axisIndicatorScale;
    const char* lbl[3] = { "X", "Y", "Z" };
    const ImU32 cols[3] = {
        IM_COL32(255, 70, 70, 255),
        IM_COL32(90, 255, 90, 255),
        IM_COL32(90, 120, 255, 255),
    };
    for (int i = 0; i < 3; i++)
    {
        ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, lbl[i]);
        ImVec2 p(m_indLabelPosition[i].x - ts.x * 0.5f, m_indLabelPosition[i].y - ts.y * 0.5f);
        const float shadowOffset = Ui::Hairline();
        dl->AddText(font, fontSize, ImVec2(p.x + shadowOffset, p.y + shadowOffset), IM_COL32(0, 0, 0, 160), lbl[i]);
        dl->AddText(font, fontSize, p, cols[i], lbl[i]);
    }
}
