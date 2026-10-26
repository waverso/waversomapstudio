// Source Repository: https://github.com/waverso/waversomapstudio

static D3DCOLOR ScaleAlpha(D3DCOLOR color, float alpha)
{
    const DWORD sourceAlpha = (color >> 24) & 0xFF;
    const DWORD scaledAlpha = (DWORD)std::clamp(sourceAlpha * alpha, 0.0f, 255.0f);
    return (color & 0x00FFFFFF) | (scaledAlpha << 24);
}

void Gizmos::DrawMeshCol(const D3D9ProceduralMesh& mesh, const D3DXMATRIX& world, D3DCOLOR col, bool ambient)
{
    D3DMATERIAL9 mtrl;
    ZeroMemory(&mtrl, sizeof(mtrl));
    D3DXCOLOR c(col);
    c.a *= m_renderAlpha;
    if (!m_enabled || m_renderAlpha < 1.0f)
    {
        if (!m_enabled)
        {
            c.a *= GizmoDisabledAlpha;
        }
        m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }
    mtrl.Diffuse = c;
    mtrl.Ambient = c;
    mtrl.Emissive = ambient ? D3DXCOLOR(0, 0, 0, 0) : c * 0.85f;
    mtrl.Specular = D3DXCOLOR(0.8f, 0.8f, 0.8f, 1.f);
    mtrl.Power = 25.f;
    m_device->SetMaterial(&mtrl);
    m_device->SetTransform(D3DTS_WORLD, &world);
    mesh.Draw();
}

D3DCOLOR Gizmos::Hi(Part p, D3DCOLOR base) const
{
    return (p == m_hover || p == m_dragPart) ? ColYellow : base;
}

D3DCOLOR Gizmos::HiA(Part p, D3DCOLOR base) const
{
    if (p == m_hover || p == m_dragPart)
    {
        return D3DCOLOR_ARGB((BYTE)(base >> 24), 255, 220, 0);
    }
    return base;
}

void Gizmos::Render()
{
    if (!m_device || !g_freeCamera->m_active)
    {
        return;
    }
    if (!m_visible || m_mode == GizmoMode::None)
    {
        bool hasBounds = m_selectionBoundsVisible && Selection::TotalCount() > 0;
        if (g_scene)
        {
            for (int index = 0; index < g_scene->count; ++index)
            {
                const auto& data = g_scene->data[index];
                hasBounds |= data.boxArea.HasValue() || data.curve.HasValue()
                    || (data.removeBuilding.HasValue() && data.removeBuilding.Get().custom)
                    || (g_scene->items[index].selected && data.layout.HasValue());
            }
        }
        if (!hasBounds)
        {
            return;
        }
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

    RenderSceneBounds();
    if (!m_visible || m_mode == GizmoMode::None)
    {
        sb->Apply();
        sb->Release();
        g_mInputDone = false;
        return;
    }

    const Frame f = (g_mInputDone && g_mAnchorLock) ? BuildFrame(g_mAnchorPosition, g_mAnchorQuat)
                                                    : BuildFrame(m_position, GetVisualQuat());
    if (!g_mInputDone)
    {
        DoInput(f);
    }

    if (f.fadeAlpha <= 0.0f && !m_radiusActive && !m_boxAreaActive)
    {
        sb->Apply();
        sb->Release();
        g_mInputDone = false;
        return;
    }

    m_renderAlpha = f.fadeAlpha;
    DrawDragGuides(f);
    m_renderAlpha = 1.0f;

    m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
    m_device->SetRenderState(D3DRS_AMBIENT, 0x404040);
    m_device->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
    m_device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
    m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

    if (m_radiusActive)
    {
        RenderRadius(f);
    }
    if (m_boxAreaActive)
    {
        RenderBoxArea();
    }

    m_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0xFFFFFFFF, 1.f, 0);
    m_device->SetRenderState(D3DRS_SPECULARENABLE, TRUE);

    D3DLIGHT9 lt;
    ZeroMemory(&lt, sizeof(lt));
    lt.Type = D3DLIGHT_DIRECTIONAL;
    lt.Diffuse = { 0.85f, 0.85f, 0.85f, 1.f };
    lt.Direction = -f.toCam;
    m_device->SetLight(7, &lt);
    m_device->LightEnable(7, TRUE);

    D3DXMATRIX gameProj;
    m_device->GetTransform(D3DTS_PROJECTION, &gameProj);

    float projNear = std::max(0.04f, f.nearestViewDepth), projFar = f.camDist;
    if (m_dragging
        && (m_dragPart == Part::SphereCenter || m_dragPart == Part::AxisX || m_dragPart == Part::AxisY
            || m_dragPart == Part::AxisZ || m_dragPart == Part::QuadXY || m_dragPart == Part::QuadXZ
            || m_dragPart == Part::QuadYZ))
    {
        D3DXVECTOR3 gv = D3DXVECTOR3(g_mDragPositionStart.x, g_mDragPositionStart.y, g_mDragPositionStart.z) - f.camPos;
        float gd = std::max(0.001f, D3DXVec3Length(&gv));
        projNear = std::min(projNear, gd);
        projFar = std::max(projFar, gd);
    }
    if (m_radiusActive)
    {
        D3DXVECTOR3 rcv = RadiusDrawCenter() - f.camPos;
        float rcd = D3DXVec3Length(&rcv);
        projFar = std::max(projFar, rcd + m_radiusValue);
        projNear = std::min(projNear, std::max(0.05f, rcd - m_radiusValue));
    }
    if (m_boxAreaActive)
    {
        for (int corner = 0; corner < 8; ++corner)
        {
            const auto& box = m_boxAreaValue;
            D3DXVECTOR3 point(m_boxAreaPosition.x + (corner & 1 ? 0.5f : -0.5f) * box.GetSize(0),
                m_boxAreaPosition.y + (corner & 2 ? 0.5f : -0.5f) * box.GetSize(1),
                m_boxAreaPosition.z + (corner & 4 ? 0.5f : -0.5f) * box.GetSize(2));
            point -= f.camPos;
            projFar = std::max(projFar, D3DXVec3Length(&point));
            projNear = std::min(projNear, std::max(0.04f, D3DXVec3Dot(&point, &f.camForward)));
        }
    }
    D3DXMATRIX gizmoProj = MakeGizmoProj(gameProj, projNear, projFar);
    m_device->SetTransform(D3DTS_PROJECTION, &gizmoProj);

    m_renderAlpha = f.fadeAlpha;
    DoRender(f);
    m_renderAlpha = 1.0f;

    if (m_radiusActive)
    {
        RenderRadiusHandles(f);
    }
    if (m_boxAreaActive)
    {
        RenderBoxAreaHandles(f);
    }

    sb->Apply();
    m_device->SetTransform(D3DTS_PROJECTION, &gameProj);
    sb->Release();
    g_mInputDone = false;
}

void Gizmos::DoRender(const Frame& f)
{
    if (f.fadeAlpha <= 0.0f)
    {
        return;
    }
    m_device->SetVertexShader(nullptr);
    m_device->SetPixelShader(nullptr);

    bool dragT = m_dragging && m_dragPart >= Part::AxisX && m_dragPart <= Part::SphereCenter;
    bool dragR = m_dragging && m_dragPart >= Part::RingX && m_dragPart <= Part::RingOuter;

    D3DXMATRIX mv, mf;
    if ((m_mode == GizmoMode::Position || m_mode == GizmoMode::Both) && !dragR)
    {
        auto DrawAxis = [&](Part p, const D3DXMATRIX& rot, D3DCOLOR col)
        {
            if (dragT && m_dragPart != p)
            {
                return;
            }
            D3DXMatrixTranslation(&mv, 0.f, 0.f, ShaftLength * 0.5f);
            mf = mv * rot * f.matST;
            DrawMeshCol(m_cylinderMesh, mf, Hi(p, col));
            D3DXMatrixTranslation(&mv, 0.f, 0.f, ConeCenter);
            mf = mv * rot * f.matST;
            DrawMeshCol(m_coneMesh, mf, Hi(p, col));
        };
        DrawAxis(Part::AxisX, f.rotX, ColRed);
        DrawAxis(Part::AxisY, f.rotY, ColGreen);
        DrawAxis(Part::AxisZ, f.rotZ, ColBlue);

        const float qa = D3DXToRadian(90.f);
        D3DXMATRIX rXY, rXZ, rYZ;
        if (!dragT || m_dragPart == Part::QuadXY)
        {
            D3DXMatrixRotationZ(&rXY, qa);
            D3DXMatrixTranslation(&mv, QuadOffset, QuadOffset, 0.f);
            DrawMeshCol(m_quadMesh, rXY * mv * f.matST, Hi(Part::QuadXY, ColBlue));
        }
        if (!dragT || m_dragPart == Part::QuadXZ)
        {
            D3DXMatrixRotationX(&rXZ, qa);
            D3DXMatrixTranslation(&mv, QuadOffset, 0.f, QuadOffset);
            DrawMeshCol(m_quadMesh, rXZ * mv * f.matST, Hi(Part::QuadXZ, ColGreen));
        }
        if (!dragT || m_dragPart == Part::QuadYZ)
        {
            D3DXMatrixRotationY(&rYZ, qa);
            D3DXMatrixTranslation(&mv, 0.f, QuadOffset, QuadOffset);
            DrawMeshCol(m_quadMesh, rYZ * mv * f.matST, Hi(Part::QuadYZ, ColRed));
        }

        DrawMeshCol(m_sphereMesh, f.matST, Hi(Part::SphereCenter, ColGray));

        if (m_dragging
            && (m_dragPart == Part::SphereCenter || m_dragPart == Part::AxisX || m_dragPart == Part::AxisY
                || m_dragPart == Part::AxisZ || m_dragPart == Part::QuadXY || m_dragPart == Part::QuadXZ
                || m_dragPart == Part::QuadYZ))
        {
            D3DXVECTOR3 gp(g_mDragPositionStart.x, g_mDragPositionStart.y, g_mDragPositionStart.z);
            D3DXVECTOR3 toGhost = gp - f.camPos;
            float gd = std::max(0.001f, D3DXVec3Length(&toGhost));
            float gs = f.scale * gd / f.camDist;

            D3DXMATRIX matGS, matGT;
            D3DXMatrixScaling(&matGS, gs, gs, gs);
            D3DXMatrixTranslation(&matGT, gp.x, gp.y, gp.z);

            D3DXVECTOR3 gn = -toGhost;
            D3DXVec3Normalize(&gn, &gn);
            float gcp[4] = { gn.x, gn.y, gn.z, -D3DXVec3Dot(&gn, &gp) };
            m_device->SetClipPlane(0, gcp);
            m_device->SetRenderState(D3DRS_CLIPPLANEENABLE, D3DCLIPPLANE0);

            m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

            DrawMeshCol(m_sphereMesh, matGS * matGT, D3DCOLOR_ARGB(GhostSphereAlpha, 210, 210, 210));

            m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
            m_device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        }
    }

    if (m_rotationEnabled && (m_mode == GizmoMode::Rotation || m_mode == GizmoMode::Both) && !dragT)
    {
        if (!dragR)
        {
            float cp[4] = { f.toCam.x, f.toCam.y, f.toCam.z, -D3DXVec3Dot(&f.toCam, &f.renderPos) };
            m_device->SetClipPlane(0, cp);
            m_device->SetRenderState(D3DRS_CLIPPLANEENABLE, D3DCLIPPLANE0);
        }

        if (!m_rotationXyEnabled && !dragR)
        {
            DWORD savedBlend;
            m_device->GetRenderState(D3DRS_ALPHABLENDENABLE, &savedBlend);
            const float savedAlpha = m_renderAlpha;
            m_renderAlpha *= GizmoDisabledAlpha;
            DrawMeshCol(m_torusMesh, f.rotX * f.matST, ColRed);
            DrawMeshCol(m_torusMesh, f.rotY * f.matST, ColGreen);
            m_renderAlpha = savedAlpha;
            m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, savedBlend);
        }
        else if (m_rotationXyEnabled)
        {
            if (!dragR || m_dragPart == Part::RingX)
            {
                DrawMeshCol(m_torusMesh, f.rotX * f.matST, Hi(Part::RingX, ColRed));
            }
            if (!dragR || m_dragPart == Part::RingY)
            {
                DrawMeshCol(m_torusMesh, f.rotY * f.matST, Hi(Part::RingY, ColGreen));
            }
        }
        if (!dragR || m_dragPart == Part::RingZ)
        {
            DrawMeshCol(m_torusMesh, f.rotZ * f.matST, Hi(Part::RingZ, ColBlue));
        }

        m_device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);

        if (!dragR || m_dragPart == Part::RingInner)
        {
            m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

            DrawMeshCol(m_rotationSphereMesh, f.matSTNoRot, HiA(Part::RingInner, D3DCOLOR_ARGB(45, 210, 210, 210)));
            DrawMeshCol(m_torusMesh, f.billRot * f.matSTNoRot, HiA(Part::RingInner, D3DCOLOR_ARGB(120, 210, 210, 210)), false);

            m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        }

        if (!dragR || m_dragPart == Part::RingOuter)
        {
            DrawMeshCol(m_torusMesh, f.billRot * f.matBigST, Hi(Part::RingOuter, ColGray), false);
        }

        const float arcAng = (g_mInputDone && g_mAnchorLock) ? g_mAnchorArcAngle : g_mDragArcAngle;
        if (m_dragging && m_dragPart >= Part::RingX && m_dragPart <= Part::RingOuter && fabsf(arcAng) > 1e-4f
            && D3DXVec3LengthSq(&g_mDragRotationStart) > 1e-6f)
        {
            D3DCOLOR base = ColYellow;

            float radius = f.scale * TorusOuter * (m_dragPart == Part::RingOuter ? OuterScale : 1.f);
            int segs = std::min(ArcMaxSegs, std::max(1, (int)ceilf(fabsf(arcAng) / D3DXToRadian(3.f))));

            struct ArcVtx
            {
                float x, y, z;
                D3DCOLOR c;
            };
            ArcVtx fan[ArcMaxSegs + 2];
            ArcVtx edge[ArcMaxSegs + 3];

            D3DCOLOR fillCol = ScaleAlpha((base & 0x00FFFFFF) | (ArcFillAlpha << 24), f.fadeAlpha);
            D3DCOLOR edgeCol = ScaleAlpha((base & 0x00FFFFFF) | (ArcEdgeAlpha << 24), f.fadeAlpha);

            fan[0] = { f.renderPos.x, f.renderPos.y, f.renderPos.z, fillCol };
            edge[0] = { f.renderPos.x, f.renderPos.y, f.renderPos.z, edgeCol };
            for (int i = 0; i <= segs; i++)
            {
                float t = arcAng * (float)i / (float)segs;
                D3DXMATRIX r;
                D3DXMatrixRotationAxis(&r, &g_mDragAxis, t);
                D3DXVECTOR3 d;
                D3DXVec3TransformNormal(&d, &g_mDragRotationStart, &r);
                D3DXVECTOR3 p = f.renderPos + d * radius;
                fan[i + 1] = { p.x, p.y, p.z, fillCol };
                edge[i + 1] = { p.x, p.y, p.z, edgeCol };
            }
            edge[segs + 2] = edge[0];

            m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
            m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

            D3DXMATRIX id;
            D3DXMatrixIdentity(&id);
            m_device->SetTransform(D3DTS_WORLD, &id);
            m_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);

            m_device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, segs, fan, sizeof(ArcVtx));
            LineRenderer::Draw(m_device, D3DPT_LINESTRIP, segs + 2, edge, sizeof(ArcVtx), LineRenderer::Depth::Visible);

            m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
            m_device->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
            m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
            m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
        }
    }
}

void Gizmos::DrawDragGuides(const Frame& f)
{
    if (!m_enabled || !m_dragging || f.fadeAlpha <= 0.0f)
    {
        return;
    }

    D3DXMATRIX vr;
    D3DXQUATERNION qv = GetVisualQuat();
    D3DXMatrixRotationQuaternion(&vr, &qv);
    auto AxW = [&](float x, float y, float z)
    {
        D3DXVECTOR3 l(x, y, z), w;
        D3DXVec3TransformNormal(&w, &l, &vr);
        D3DXVec3Normalize(&w, &w);
        return w;
    };

    D3DXVECTOR3 dirs[2];
    int n = 0;
    switch (m_dragPart)
    {
        case Part::AxisX:
            dirs[0] = g_mDragAxis;
            n = 1;
            break;
        case Part::AxisY:
            dirs[0] = g_mDragAxis;
            n = 1;
            break;
        case Part::AxisZ:
            dirs[0] = g_mDragAxis;
            n = 1;
            break;
        case Part::RingX:
            dirs[0] = g_mDragAxis;
            n = 1;
            break;
        case Part::RingY:
            dirs[0] = g_mDragAxis;
            n = 1;
            break;
        case Part::RingZ:
            dirs[0] = g_mDragAxis;
            n = 1;
            break;
        case Part::QuadXY:
            dirs[0] = AxW(1.f, 0.f, 0.f);
            dirs[1] = AxW(0.f, 1.f, 0.f);
            n = 2;
            break;
        case Part::QuadXZ:
            dirs[0] = AxW(1.f, 0.f, 0.f);
            dirs[1] = AxW(0.f, 0.f, 1.f);
            n = 2;
            break;
        case Part::QuadYZ:
            dirs[0] = AxW(0.f, 1.f, 0.f);
            dirs[1] = AxW(0.f, 0.f, 1.f);
            n = 2;
            break;
        default:
            return;
    }

    m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    D3DXMATRIX id;
    D3DXMatrixIdentity(&id);
    m_device->SetTransform(D3DTS_WORLD, &id);
    m_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);

    struct GuideVtx
    {
        float x, y, z;
        D3DCOLOR c;
    };
    D3DXVECTOR3 center = f.renderPos;
    D3DCOLOR col = ScaleAlpha((ColYellow & 0x00FFFFFF) | (GuideAlpha << 24), f.fadeAlpha);
    for (int i = 0; i < n; i++)
    {
        D3DXVECTOR3 a = center - dirs[i] * GuideHalfLength;
        D3DXVECTOR3 b = center + dirs[i] * GuideHalfLength;
        GuideVtx verts[2] = { { a.x, a.y, a.z, col }, { b.x, b.y, b.z, col } };
        LineRenderer::Draw(m_device, D3DPT_LINELIST, 1, verts, sizeof(GuideVtx));
    }
}
