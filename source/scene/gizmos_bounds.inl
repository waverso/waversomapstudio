// Source Repository: https://github.com/waverso/waversomapstudio

D3DXVECTOR3 Gizmos::BoxAreaFacePosition(int face, bool rendered) const
{
    D3DXVECTOR3 position(m_boxAreaPosition.x, m_boxAreaPosition.y, m_boxAreaPosition.z);
    if (rendered && g_mInputDone && g_mAnchorLock)
    {
        position += D3DXVECTOR3(
            g_mAnchorPosition.x - m_position.x, g_mAnchorPosition.y - m_position.y, g_mAnchorPosition.z - m_position.z);
    }
    float offset[3] = {};
    offset[face % 3] = (face < 3 ? -0.5f : 0.5f) * m_boxAreaValue.GetSize(face % 3);
    return position + D3DXVECTOR3(offset[0], offset[1], offset[2]);
}

void Gizmos::RenderBoxArea()
{
    D3DXVECTOR3 origin(m_boxAreaPosition.x, m_boxAreaPosition.y, m_boxAreaPosition.z);
    if (g_mInputDone && g_mAnchorLock)
    {
        origin += D3DXVECTOR3(
            g_mAnchorPosition.x - m_position.x, g_mAnchorPosition.y - m_position.y, g_mAnchorPosition.z - m_position.z);
    }
    D3DXVECTOR3 corners[8];
    BuildBoxAreaCorners(origin, m_boxAreaValue, corners);
    const int activeFace = m_boxAreaDragging ? m_boxAreaFace
        : g_scene                            ? GuiGetBoxAreaEditingFace(g_scene->SingleSelectedIndex())
                                             : -1;
    D3DXMATRIX identity;
    D3DXMatrixIdentity(&identity);
    m_device->SetTransform(D3DTS_WORLD, &identity);
    m_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    using Vertex = LineRenderer::Vertex;
    Vertex edges[24];
    int count = 0;
    const D3DCOLOR color = m_boxAreaValue.GetColorARGB() | 0xFF000000u;
    for (int corner = 0; corner < 8; ++corner)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            const int end = corner ^ (1 << axis);
            if (corner >= end)
            {
                continue;
            }
            for (int index : { corner, end })
            {
                edges[count++] = { corners[index].x, corners[index].y, corners[index].z, color };
            }
        }
    }
    LineRenderer::Draw(m_device, D3DPT_LINELIST, 12, edges, sizeof(Vertex));
    if (m_boxAreaDragging || m_dragging || (g_scene && GuiIsAreaEditing(g_scene->SingleSelectedIndex())))
    {
        static constexpr int faces[6][6] = { { 0, 2, 6, 0, 6, 4 }, { 0, 4, 5, 0, 5, 1 }, { 0, 1, 3, 0, 3, 2 },
            { 1, 5, 7, 1, 7, 3 }, { 2, 3, 7, 2, 7, 6 }, { 4, 6, 7, 4, 7, 5 } };
        Vertex vertices[36];
        for (int face = 0; face < 6; ++face)
        {
            const D3DCOLOR fillColor = (color & 0x00FFFFFFu) | ((face == activeFace ? 80u : 26u) << 24);
            for (int vertex = 0; vertex < 6; ++vertex)
            {
                const auto& point = corners[faces[face][vertex]];
                vertices[face * 6 + vertex] = { point.x, point.y, point.z, fillColor };
            }
        }
        m_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 12, vertices, sizeof(Vertex));
    }
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
}

void Gizmos::RenderBoxAreaHandles(const Frame& f)
{
    const int activeFace = m_boxAreaDragging ? m_boxAreaFace
        : g_scene                            ? GuiGetBoxAreaEditingFace(g_scene->SingleSelectedIndex())
                                             : -1;
    m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    for (int face = 0; face < 6; ++face)
    {
        const D3DXVECTOR3 point = BoxAreaFacePosition(face, true);
        const D3DXVECTOR3 toCamera = f.camPos - point;
        const float scale = GetHandleWorldRadius({ point.x, point.y, point.z }) / 0.07f;
        D3DXMATRIX scaling, translation;
        D3DXMatrixScaling(&scaling, scale, scale, scale);
        D3DXMatrixTranslation(&translation, point.x, point.y, point.z);
        const float direction[3] = { toCamera.x, toCamera.y, toCamera.z };
        const bool front = direction[face % 3] * (face < 3 ? -1.0f : 1.0f) >= 0;
        const bool active = face == (activeFace >= 0 ? activeFace : m_boxAreaHover);
        const D3DCOLOR color
            = active ? 0xFFFFFFFFu : ((front ? 255u : 120u) << 24) | (m_boxAreaValue.GetColorARGB() & 0x00FFFFFFu);
        DrawMeshCol(m_sphereMesh, scaling * translation, color);
    }
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
}

void Gizmos::RenderSceneBounds()
{
    using Vertex = LineRenderer::Vertex;
    static std::vector<Vertex> edges;
    edges.clear();
    static std::vector<Curves::Marker> markers;
    markers.clear();
    auto addBox = [&](const D3DXVECTOR3(&corners)[8], D3DCOLOR color)
    {
        for (int corner = 0; corner < 8; ++corner)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                const int end = corner ^ (1 << axis);
                if (corner >= end)
                {
                    continue;
                }
                for (int vertex : { corner, end })
                {
                    const auto& point = corners[vertex];
                    edges.push_back({ point.x, point.y, point.z, color });
                }
            }
        }
    };
    const int selected = m_visible && m_mode != GizmoMode::None && g_scene ? g_scene->SingleSelectedIndex() : -1;
    const int active = m_boxAreaActive ? selected : -1;
    D3DXVECTOR3 radiusCamera;
    float radiusHalfHeight = 0.0f, radiusTangent = 0.0f;
    for (int index = 0; g_scene && index < g_scene->count; ++index)
    {
        const auto& data = g_scene->data[index];
        const auto& position = g_scene->world[index].pos;
        if (data.removeBuilding.HasValue() && data.removeBuilding.Get().custom && !(m_radiusActive && index == selected)
            && data.removeBuilding.Get().radius > 0.0f)
        {
            if (radiusHalfHeight == 0.0f)
            {
                D3DXMATRIX view, inverseView, projection;
                D3DVIEWPORT9 viewport;
                m_device->GetTransform(D3DTS_VIEW, &view);
                m_device->GetTransform(D3DTS_PROJECTION, &projection);
                m_device->GetViewport(&viewport);
                D3DXMatrixInverse(&inverseView, nullptr, &view);
                radiusCamera = { inverseView._41, inverseView._42, inverseView._43 };
                radiusHalfHeight = viewport.Height * 0.5f;
                radiusTangent = tanf(atanf(1.0f / projection._22));
            }
            const D3DXVECTOR3 center(position.x, position.y, position.z);
            const auto offsetToCamera = center - radiusCamera;
            const float distance = std::max(0.001f, D3DXVec3Length(&offsetToCamera));
            const float radius = data.removeBuilding.Get().radius;
            const int segments = RadiusSegments(radius * radiusHalfHeight / std::max(0.001f, distance * radiusTangent));
            const size_t offset = edges.size();
            edges.resize(offset + segments * 6);
            BuildRadiusEdges(
                edges.data() + offset, center, radius, segments, g_scene->items[index].selected ? RadiusColSolid : RadiusColBack);
        }
        if (!data.boxArea.HasValue() || index == active)
        {
            continue;
        }
        D3DXVECTOR3 corners[8];
        BuildBoxAreaCorners(D3DXVECTOR3(position.x, position.y, position.z), data.boxArea.Get(), corners);
        addBox(corners, data.boxArea.Get().GetColorARGB() | 0xFF000000u);
    }
    if (m_selectionBoundsVisible)
    {
        CVector minimum, maximum;
        if (Selection::ComputeBounds(minimum, maximum))
        {
            D3DXVECTOR3 corners[8];
            for (int corner = 0; corner < 8; ++corner)
            {
                corners[corner] = D3DXVECTOR3(
                    corner & 1 ? maximum.x : minimum.x, corner & 2 ? maximum.y : minimum.y, corner & 4 ? maximum.z : minimum.z);
            }
            addBox(corners, Theme::Argb(Theme::g_accent));
        }
    }
    if (g_scene)
    {
        static std::vector<std::array<float, 6>> lines;
        lines.clear();
        const int index = g_scene->SingleSelectedIndex();
        if (index >= 0 && g_scene->data[index].layout.HasValue())
        {
            Layout::BuildPreview(*g_scene, index, lines);
        }
        for (const auto& line : lines)
        {
            edges.push_back({ line[0], line[1], line[2], D3DCOLOR_XRGB(80, 215, 160) });
            edges.push_back({ line[3], line[4], line[5], D3DCOLOR_XRGB(80, 215, 160) });
        }
        static std::vector<std::array<float, 7>> curves;
        curves.clear();
        Curves::BuildDisplay(*g_scene, curves, markers);
        for (const auto& line : curves)
        {
            const D3DCOLOR color = 0xFF000000u | static_cast<unsigned>(line[6]);
            edges.push_back({ line[0], line[1], line[2], color });
            edges.push_back({ line[3], line[4], line[5], color });
        }
    }
    if (edges.empty() && markers.empty())
    {
        return;
    }
    D3DXMATRIX identity;
    D3DXMatrixIdentity(&identity);
    m_device->SetTransform(D3DTS_WORLD, &identity);
    m_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    const auto count = static_cast<UINT>(edges.size() / 2);
    LineRenderer::Draw(m_device, D3DPT_LINELIST, count, edges.data(), sizeof(Vertex));
    if (!markers.empty())
    {
        m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
        m_device->SetRenderState(D3DRS_AMBIENT, 0x404040);
        m_device->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
        m_device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
        m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
        D3DXMATRIX view;
        m_device->GetTransform(D3DTS_VIEW, &view);
        D3DLIGHT9 light {};
        light.Type = D3DLIGHT_DIRECTIONAL;
        light.Diffuse = { 0.85f, 0.85f, 0.85f, 1.0f };
        light.Direction = { view._13, view._23, view._33 };
        m_device->SetLight(7, &light);
        m_device->LightEnable(7, TRUE);
        for (const auto& marker : markers)
        {
            const float scale = marker.radius / 0.07f;
            D3DXMATRIX scaling, translation;
            D3DXMatrixScaling(&scaling, scale, scale, scale);
            D3DXMatrixTranslation(&translation, marker.position[0], marker.position[1], marker.position[2]);
            const auto world = scaling * translation;
            m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            DrawMeshCol(m_sphereMesh, world, (GhostSphereAlpha << 24) | marker.color);
            m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
            DrawMeshCol(m_sphereMesh, world, 0xFF000000u | marker.color);
        }
    }
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
}

D3DXVECTOR3 Gizmos::RadiusDrawCenter() const
{
    D3DXVECTOR3 c(m_radiusCenter.x, m_radiusCenter.y, m_radiusCenter.z);
    if (g_mInputDone && g_mAnchorLock)
    {
        c += D3DXVECTOR3(
            g_mAnchorPosition.x - m_position.x, g_mAnchorPosition.y - m_position.y, g_mAnchorPosition.z - m_position.z);
    }
    return c;
}

float Gizmos::RadiusScreenPx(const Frame& f)
{
    D3DXVECTOR3 rcv = RadiusDrawCenter() - f.camPos;
    float camDistR = std::max(0.001f, D3DXVec3Length(&rcv));
    D3DVIEWPORT9 vp;
    m_device->GetViewport(&vp);
    float pdenom = std::max(0.001f, camDistR * tanf(f.vFovHalf));
    return m_radiusValue * ((float)vp.Height * 0.5f) / pdenom;
}

void Gizmos::RenderRadius(const Frame& f)
{
    const float r = m_radiusValue;
    if (r <= 0.f)
    {
        return;
    }

    const auto c = RadiusDrawCenter();
    D3DXVECTOR3 toCamR = f.camPos - c;
    D3DXVec3Normalize(&toCamR, &toCamR);
    const int segs = RadiusSegments(RadiusScreenPx(f));

    m_device->SetVertexShader(nullptr);
    m_device->SetPixelShader(nullptr);
    m_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    D3DXMATRIX id;
    D3DXMatrixIdentity(&id);
    m_device->SetTransform(D3DTS_WORLD, &id);

    m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

    const bool bFill = m_radiusDragging || m_dragging || (g_scene && GuiIsAreaEditing(g_scene->SingleSelectedIndex()));
    if (bFill)
    {
        D3DLIGHT9 lt;
        ZeroMemory(&lt, sizeof(lt));
        lt.Type = D3DLIGHT_DIRECTIONAL;
        lt.Diffuse = { 0.85f, 0.85f, 0.85f, 1.f };
        lt.Direction = -toCamR;
        m_device->SetLight(7, &lt);
        m_device->LightEnable(7, TRUE);

        float fs = r / TorusOuter;
        D3DXMATRIX msF, mtF;
        D3DXMatrixScaling(&msF, fs, fs, fs);
        D3DXMatrixTranslation(&mtF, c.x, c.y, c.z);

        m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
        DrawMeshCol(m_rotationSphereMesh, msF * mtF, RadiusColFill);
        m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    }

    m_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    m_device->SetTransform(D3DTS_WORLD, &id);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

    LineRenderer::Vertex edges[RadiusMaxSegs * 6];
    BuildRadiusEdges(edges, c, r, segs, RadiusColSolid);
    LineRenderer::Draw(m_device, D3DPT_LINELIST, segs * 3, edges, sizeof(LineRenderer::Vertex));

    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
}

void Gizmos::RenderRadiusHandles(const Frame& f)
{
    const float r = m_radiusValue;
    if (r <= 0.f)
    {
        return;
    }

    const auto c = RadiusDrawCenter();
    D3DXVECTOR3 toCamR = f.camPos - c;
    D3DXVec3Normalize(&toCamR, &toCamR);
    m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

    if (RadiusScreenPx(f) >= RadiusMinPxHandles)
    {
        m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
        m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);

        for (int i = 0; i < 6; ++i)
        {
            D3DXVECTOR3 hp = c + g_kRadiusAxis[i] * r;
            const float hs = GetHandleWorldRadius({ hp.x, hp.y, hp.z }) / 0.07f;
            D3DXMATRIX ms, mt;
            D3DXMatrixScaling(&ms, hs, hs, hs);
            D3DXMatrixTranslation(&mt, hp.x, hp.y, hp.z);
            bool active = (i == m_radiusHover || i == m_radiusDragHandle);
            float facing = D3DXVec3Dot(&g_kRadiusAxis[i], &toCamR);
            D3DCOLOR col = active ? RadiusColHover : (facing >= 0.f ? RadiusColSolid : RadiusColBack);
            DrawMeshCol(m_sphereMesh, ms * mt, col);
        }
    }

    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_LIGHTING, TRUE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
}
