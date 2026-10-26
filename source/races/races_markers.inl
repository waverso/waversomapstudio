// Source Repository: https://github.com/waverso/waversomapstudio

CVector Direction(const CVector& from, const CVector& to)
{
    CVector result = to - from;
    const float length = result.Magnitude();
    return length > 0.0001f ? result * (1.0f / length) : CVector(0, 1, 0);
}

struct Visual
{
    int id = -1;
    int type = -1;
    CVector origin;
    CVector direction;
    VisualMotion motion;
    float rotation = 0.0f;
    unsigned int frame = UINT_MAX;

    void Update(
        int itemId, int checkpointType, const CVector& position, const CVector& normal, float radius, const CVector& player)
    {
        if (id != itemId || type != checkpointType || position.x != origin.x || position.y != origin.y || position.z != origin.z)
        {
            id = itemId;
            type = checkpointType;
            origin = position;
            direction = normal;
            motion.Reset(type, position.z);
            rotation = 0.0f;
            frame = UINT_MAX;
        }
        if (frame == CTimer::m_FrameCounter)
        {
            return;
        }
        if (type <= 2 && frame != UINT_MAX)
        {
            rotation = std::fmod(rotation + D3DXToRadian(CTimer::ms_fTimeStep), 2.0f * D3DX_PI);
        }
        frame = CTimer::m_FrameCounter;
        const float x = player.x - position.x;
        const float y = player.y - position.y;
        motion.Update(x * x + y * y, player.z, radius, CTimer::m_snTimeInMilliseconds);
        if (motion.type == 5)
        {
            const float rotation = motion.amount;
            direction = Direction(CVector(), direction + CVector(rotation, rotation, rotation));
        }
    }
};

struct Vertex
{
    D3DXVECTOR3 position;
    D3DXVECTOR3 normal;
    D3DCOLOR color;
    float u, v;
};

struct MeshPart
{
    RwTexture* texture = nullptr;
    bool lit = false;
    float ambient = 1.0f, diffuse = 1.0f;
    std::vector<Vertex> vertices;
    std::vector<Vertex> colored;
};

struct Marker
{
    RpClump* source = nullptr;
    std::vector<MeshPart> parts;
    D3DXVECTOR3 minimum, maximum;
    bool bright = true;
    bool doubleSided = false;
    bool depthWrite = false;

    bool Ensure(unsigned short type)
    {
        auto* clump = C3dMarkers::m_pRpClumpArray[type == MARKER3D_TUBE ? 1 : type];
        if (!clump)
        {
            return false;
        }
        if (source == clump && !parts.empty())
        {
            return true;
        }
        RpAtomic* atomic = nullptr;
        RpClumpForAllAtomics(
            clump,
            [](RpAtomic* value, void* data) -> RpAtomic*
            {
                auto** first = static_cast<RpAtomic**>(data);
                if (!*first)
                {
                    *first = value;
                }
                return value;
            },
            &atomic);
        auto* geometry = atomic ? RpAtomicGetGeometry(atomic) : nullptr;
        auto* target = geometry && RpGeometryGetNumMorphTargets(geometry) > 0 ? RpGeometryGetMorphTarget(geometry, 0) : nullptr;
        if (!target || !target->verts)
        {
            return false;
        }
        source = nullptr;
        bright = type != MARKER3D_CONE;
        doubleSided = type == MARKER3D_CONE;
        parts.clear();
        parts.resize(RpGeometryGetNumMaterials(geometry));
        minimum = D3DXVECTOR3(FLT_MAX, FLT_MAX, FLT_MAX);
        maximum = D3DXVECTOR3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        const auto flags = RpGeometryGetFlags(geometry);
        const auto* colors = flags & rpGEOMETRYPRELIT ? RpGeometryGetPreLightColors(geometry) : nullptr;
        const auto* uv = RpGeometryGetNumTexCoordSets(geometry) > 0 ? RpGeometryGetVertexTexCoords(geometry, 1) : nullptr;
        const auto* triangles = RpGeometryGetTriangles(geometry);
        for (int material = 0; material < static_cast<int>(parts.size()); ++material)
        {
            const auto* value = RpGeometryGetMaterial(geometry, material);
            auto& part = parts[material];
            part.texture = value->texture;
            part.ambient = value->surfaceProps.ambient;
            part.diffuse = value->surfaceProps.diffuse;
            part.lit = (flags & rpGEOMETRYLIGHT) && target->normals;
        }
        for (int triangle = 0; triangle < RpGeometryGetNumTriangles(geometry); ++triangle)
        {
            const auto& face = triangles[triangle];
            if (face.matIndex >= parts.size())
            {
                continue;
            }
            for (const unsigned short index : face.vertIndex)
            {
                if (index >= RpGeometryGetNumVertices(geometry))
                {
                    return false;
                }
                const auto& position = target->verts[index];
                const auto& part = parts[face.matIndex];
                const auto color = colors ? colors[index] : part.lit ? RwRGBA { 0, 0, 0, 255 } : RwRGBA { 255, 255, 255, 255 };
                const auto normal = target->normals ? target->normals[index] : RwV3d {};
                parts[face.matIndex].vertices.push_back({ D3DXVECTOR3(position.x, position.y, position.z),
                    D3DXVECTOR3(normal.x, normal.y, normal.z), D3DCOLOR_ARGB(color.alpha, color.red, color.green, color.blue),
                    uv ? uv[index].u : 0.0f, uv ? uv[index].v : 0.0f });
                minimum.x = std::min(minimum.x, position.x);
                minimum.y = std::min(minimum.y, position.y);
                minimum.z = std::min(minimum.z, position.z);
                maximum.x = std::max(maximum.x, position.x);
                maximum.y = std::max(maximum.y, position.y);
                maximum.z = std::max(maximum.z, position.z);
            }
        }
        if (minimum.x > maximum.x)
        {
            return false;
        }
        source = clump;
        return true;
    }
};

struct RenderState
{
    IDirect3DStateBlock9* block = nullptr;
    explicit RenderState(IDirect3DDevice9* device)
    {
        device->CreateStateBlock(D3DSBT_ALL, &block);
    }
    ~RenderState()
    {
        if (block)
        {
            block->Apply();
            block->Release();
        }
    }
};

struct Pass
{
    IDirect3DDevice9* device;
    D3DXMATRIX view, projection;
    D3DVIEWPORT9 viewport {};
    MarkerLighting lighting;
    bool picking = false;
    D3DXVECTOR3 origin, direction;
    float distance = 0.0f;
    int item = -1;
    int hit = -1;

    bool Initialize()
    {
        if (!device || FAILED(device->GetTransform(D3DTS_VIEW, &view))
            || FAILED(device->GetTransform(D3DTS_PROJECTION, &projection)) || FAILED(device->GetViewport(&viewport)))
        {
            return false;
        }
        const auto& ambient = *reinterpret_cast<const RwRGBAReal*>(0xC886D4);
        const auto& direct = *reinterpret_cast<const RwRGBAReal*>(0xC886B4);
        lighting.ambient = D3DXVECTOR3(ambient.red, ambient.green, ambient.blue);
        lighting.diffuse = D3DXVECTOR3(direct.red, direct.green, direct.blue);
        lighting.direction = D3DXVECTOR3(0, 0, 0);
        auto* sun = *reinterpret_cast<RpLight**>(0xC886EC);
        if (auto* frame = sun ? RpLightGetFrame(sun) : nullptr)
        {
            const auto& direction = RwFrameGetLTM(frame)->at;
            lighting.direction = D3DXVECTOR3(direction.x, direction.y, direction.z);
            D3DXVec3Normalize(&lighting.direction, &lighting.direction);
        }
        return true;
    }
    void Begin()
    {
        device->SetVertexShader(nullptr);
        device->SetPixelShader(nullptr);
        device->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
        device->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_CLIPPING, TRUE);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_DEPTHBIAS, 0);
        device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 15);
        device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        for (DWORD stage = 1; stage < 8; ++stage)
        {
            device->SetTexture(stage, nullptr);
        }
    }
    void Submit(Marker& marker, const D3DXMATRIX& matrix, CRGBA color)
    {
        D3DXMATRIX inverse, normalMatrix;
        D3DXVECTOR3 localOrigin, localDirection;
        if (!D3DXMatrixInverse(&inverse, nullptr, &matrix))
        {
            return;
        }
        if (picking)
        {
            D3DXVec3TransformCoord(&localOrigin, &origin, &inverse);
            D3DXVec3TransformNormal(&localDirection, &direction, &inverse);
        }
        else
        {
            if (!BoundsVisible(marker.minimum, marker.maximum, matrix * view * projection))
            {
                return;
            }
            device->SetTransform(D3DTS_WORLD, &matrix);
            device->SetRenderState(D3DRS_CULLMODE, marker.doubleSided ? D3DCULL_NONE : D3DCULL_CW);
            device->SetRenderState(D3DRS_ZWRITEENABLE, marker.depthWrite);
            D3DXMatrixTranspose(&normalMatrix, &inverse);
        }
        const MarkerLighting markerLighting = marker.bright
            ? MarkerLighting { D3DXVECTOR3(0.6f, 0.6f, 0.6f), D3DXVECTOR3(1, 1, 1), lighting.direction }
            : lighting;
        const D3DCOLOR tint = D3DCOLOR_ARGB(color.a, color.r, color.g, color.b);
        for (auto& part : marker.parts)
        {
            if (part.vertices.empty())
            {
                continue;
            }
            if (picking)
            {
                for (std::size_t index = 0; index + 2 < part.vertices.size(); index += 3)
                {
                    if (RayTriangle(localOrigin, localDirection, part.vertices[index].position, part.vertices[index + 1].position,
                            part.vertices[index + 2].position, distance))
                    {
                        hit = item;
                    }
                }
                continue;
            }
            part.colored = part.vertices;
            for (auto& vertex : part.colored)
            {
                if (part.lit)
                {
                    D3DXVECTOR3 normal;
                    D3DXVec3TransformNormal(&normal, &vertex.normal, &normalMatrix);
                    D3DXVec3Normalize(&normal, &normal);
                    vertex.color = LitMarkerColor(vertex.color, tint, normal, markerLighting, part.ambient, part.diffuse);
                    continue;
                }
                const DWORD base = vertex.color;
                vertex.color = D3DCOLOR_ARGB(((base >> 24) & 255) * color.a / 255, ((base >> 16) & 255) * color.r / 255,
                    ((base >> 8) & 255) * color.g / 255, (base & 255) * color.b / 255);
            }
            auto* texture = part.texture && part.texture->raster
                ? *reinterpret_cast<IDirect3DTexture9**>(part.texture->raster + 1)
                : nullptr;
            device->SetTexture(0, texture);
            device->SetTextureStageState(0, D3DTSS_COLOROP, texture ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
            device->SetTextureStageState(0, D3DTSS_ALPHAOP, texture ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
            device->DrawPrimitiveUP(
                D3DPT_TRIANGLELIST, static_cast<UINT>(part.colored.size() / 3), part.colored.data(), sizeof(Vertex));
        }
    }
};

void DrawMarker(Pass& pass, Marker& owner, unsigned short type, const CVector& position, const CVector& normal, float size,
    CRGBA color, float rotation = 0.0f)
{
    if (!owner.Ensure(type))
    {
        return;
    }
    auto matrix = MarkerTransform(
        D3DXVECTOR3(position.x, position.y, position.z), D3DXVECTOR3(normal.x, normal.y, normal.z), size, type == MARKER3D_TUBE);
    if (rotation != 0.0f)
    {
        D3DXMATRIX spin;
        D3DXMatrixRotationZ(&spin, rotation);
        matrix = spin * matrix;
    }
    if (type == MARKER3D_TUBE || type == MARKER3D_TORUS)
    {
        if (auto* player = FindPlayerPed())
        {
            const CVector offset = player->GetPosition() - position;
            const float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y) * (type == MARKER3D_TUBE ? 0.25f : 1.0f);
            color.a = MarkerAlpha(color.a, size, distance);
        }
    }
    pass.Submit(owner, matrix, color);
}

void DrawFinish(Pass& pass, const CVector& position, float radius, CRGBA color)
{
    if (!gpFinishFlagTex)
    {
        return;
    }
    static Marker flag;
    if (flag.parts.empty())
    {
        flag.minimum = D3DXVECTOR3(-1, -1, 0);
        flag.maximum = D3DXVECTOR3(1, 1, 0);
        flag.parts.resize(1);
        flag.doubleSided = true;
        flag.depthWrite = true;
        flag.parts[0].vertices = { { D3DXVECTOR3(-1, -1, 0), D3DXVECTOR3(0, 0, 1), 0xFFFFFFFF, 0, 1 },
            { D3DXVECTOR3(1, -1, 0), D3DXVECTOR3(0, 0, 1), 0xFFFFFFFF, 1, 1 },
            { D3DXVECTOR3(1, 1, 0), D3DXVECTOR3(0, 0, 1), 0xFFFFFFFF, 1, 0 },
            { D3DXVECTOR3(-1, -1, 0), D3DXVECTOR3(0, 0, 1), 0xFFFFFFFF, 0, 1 },
            { D3DXVECTOR3(1, 1, 0), D3DXVECTOR3(0, 0, 1), 0xFFFFFFFF, 1, 0 },
            { D3DXVECTOR3(-1, 1, 0), D3DXVECTOR3(0, 0, 1), 0xFFFFFFFF, 0, 0 } };
    }
    flag.parts[0].texture = gpFinishFlagTex;
    pass.Submit(flag,
        FinishTransform(D3DXVECTOR3(position.x, position.y, position.z + 3.0f), pass.view, pass.projection, pass.viewport,
            static_cast<float>(RsGlobal.maximumWidth), static_cast<float>(RsGlobal.maximumHeight), radius, CDraw::ms_fFOV),
        color);
}
