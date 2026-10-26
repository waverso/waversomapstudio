// Source Repository: https://github.com/waverso/waversomapstudio

#include "collision_renderer.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <unordered_set>
#include <vector>

#include "CCamera.h"
#include "CColDisk.h"
#include "CColLine.h"
#include "CColModel.h"
#include "CColStore.h"
#include "CColTriangle.h"
#include "CCollisionData.h"
#include "CEntity.h"
#include "CMatrix.h"
#include "CStreaming.h"
#include "CTimer.h"
#include "../scene/entity_raycast.h"
#include "../scene/free_camera.h"
#include "../hooks/game_hooks.h"
#include "line_renderer.h"
#include "../scene/selection.h"

namespace
{
    constexpr int CircleSegments = 24;
    constexpr int CollisionStreamingBase = 25000;
    constexpr D3DCOLOR GhostColor = D3DCOLOR_ARGB(52, 132, 255, 148);
    constexpr D3DCOLOR SolidColor = D3DCOLOR_ARGB(255, 132, 255, 148);

    DWORD FloatBits(float value)
    {
        DWORD bits;
        memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    CVector Cross(const CVector& a, const CVector& b)
    {
        return CVector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
    }

    float Length(const CVector& v)
    {
        return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    CVector Scale(const CVector& v, float scale)
    {
        return CVector(v.x * scale, v.y * scale, v.z * scale);
    }

    CVector Add(const CVector& a, const CVector& b)
    {
        return CVector(a.x + b.x, a.y + b.y, a.z + b.z);
    }

    CVector Subtract(const CVector& a, const CVector& b)
    {
        return CVector(a.x - b.x, a.y - b.y, a.z - b.z);
    }

    void AddLine(std::vector<CollisionRenderer::Vertex>& out, const CVector& a, const CVector& b)
    {
        out.push_back({ a.x, a.y, a.z });
        out.push_back({ b.x, b.y, b.z });
    }

    void AddCircle(std::vector<CollisionRenderer::Vertex>& out, const CVector& center, const CVector& axisU, const CVector& axisV,
        float radius)
    {
        if (radius <= 0.0f)
        {
            return;
        }
        for (int i = 0; i < CircleSegments; ++i)
        {
            const float a0 = D3DX_PI * 2.0f * static_cast<float>(i) / static_cast<float>(CircleSegments);
            const float a1 = D3DX_PI * 2.0f * static_cast<float>(i + 1) / static_cast<float>(CircleSegments);
            const CVector p0 = Add(center, Add(Scale(axisU, cosf(a0) * radius), Scale(axisV, sinf(a0) * radius)));
            const CVector p1 = Add(center, Add(Scale(axisU, cosf(a1) * radius), Scale(axisV, sinf(a1) * radius)));
            AddLine(out, p0, p1);
        }
    }

    void AddSphere(std::vector<CollisionRenderer::Vertex>& out, const CVector& center, float radius)
    {
        AddCircle(out, center, CVector(1.0f, 0.0f, 0.0f), CVector(0.0f, 1.0f, 0.0f), radius);
        AddCircle(out, center, CVector(1.0f, 0.0f, 0.0f), CVector(0.0f, 0.0f, 1.0f), radius);
        AddCircle(out, center, CVector(0.0f, 1.0f, 0.0f), CVector(0.0f, 0.0f, 1.0f), radius);
    }

    void AddBox(std::vector<CollisionRenderer::Vertex>& out, const CVector& minimum, const CVector& maximum)
    {
        const CVector p[8] = { { minimum.x, minimum.y, minimum.z }, { maximum.x, minimum.y, minimum.z },
            { maximum.x, maximum.y, minimum.z }, { minimum.x, maximum.y, minimum.z }, { minimum.x, minimum.y, maximum.z },
            { maximum.x, minimum.y, maximum.z }, { maximum.x, maximum.y, maximum.z }, { minimum.x, maximum.y, maximum.z } };
        constexpr unsigned char edges[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
            { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
        for (const auto& edge : edges)
        {
            AddLine(out, p[edge[0]], p[edge[1]]);
        }
    }

    void AddTaperedLine(
        std::vector<CollisionRenderer::Vertex>& out, const CVector& start, const CVector& end, float startRadius, float endRadius)
    {
        const CVector delta = Subtract(end, start);
        const float length = Length(delta);
        startRadius = std::max(startRadius, 0.0f);
        endRadius = std::max(endRadius, 0.0f);
        if (length <= 0.0001f)
        {
            const float radius = std::max(startRadius, endRadius);
            if (radius > 0.0f)
            {
                AddSphere(out, start, radius);
            }
            return;
        }
        if (startRadius <= 0.0f && endRadius <= 0.0f)
        {
            AddLine(out, start, end);
            return;
        }

        const CVector axis = Scale(delta, 1.0f / length);
        const CVector reference = fabsf(axis.z) < 0.9f ? CVector(0.0f, 0.0f, 1.0f) : CVector(1.0f, 0.0f, 0.0f);
        const CVector axisU = Scale(Cross(reference, axis), 1.0f / Length(Cross(reference, axis)));
        const CVector axisV = Cross(axis, axisU);
        AddCircle(out, start, axisU, axisV, startRadius);
        AddCircle(out, end, axisU, axisV, endRadius);
        for (int i = 0; i < 4; ++i)
        {
            const float angle = D3DX_PI * 0.5f * static_cast<float>(i);
            const CVector radial = Add(Scale(axisU, cosf(angle)), Scale(axisV, sinf(angle)));
            AddLine(out, Add(start, Scale(radial, startRadius)), Add(end, Scale(radial, endRadius)));
        }
    }

    D3DXMATRIX EntityWorldMatrix(const CEntity* entity)
    {
        const CVector position = entity->GetPosition();
        if (entity->m_matrix)
        {
            const CMatrix& matrix = *entity->m_matrix;
            return D3DXMATRIX(matrix.right.x, matrix.right.y, matrix.right.z, 0.0f, matrix.up.x, matrix.up.y, matrix.up.z, 0.0f,
                matrix.at.x, matrix.at.y, matrix.at.z, 0.0f, position.x, position.y, position.z, 1.0f);
        }

        const float heading = entity->m_placement.m_fHeading;
        const float c = cosf(heading);
        const float s = sinf(heading);
        return D3DXMATRIX(c, s, 0.0f, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, position.x, position.y, position.z, 1.0f);
    }
}

CollisionRenderer::CollisionRenderer()
{
    m_device = g_gameHooks ? g_gameHooks->m_device : nullptr;
    if (!m_device)
    {
        return;
    }
    D3DCAPS9 caps = {};
    if (SUCCEEDED(m_device->GetDeviceCaps(&caps)) && caps.MaxPrimitiveCount > 0)
    {
        m_maxPrimitiveCount = caps.MaxPrimitiveCount;
    }
}

void CollisionRenderer::SetEnabled(bool enabled)
{
    if (m_enabled == enabled)
    {
        return;
    }
    m_enabled = enabled;
    if (!m_enabled)
    {
        ReleaseRetainedCollisions();
        m_renderedEntities.clear();
        m_meshCache.clear();
        m_drawItems.clear();
    }
}

void CollisionRenderer::OnEntityRender(CEntity* entity)
{
    if (!m_enabled || !entity)
    {
        return;
    }
    RetainCollisionForEntity(entity);
    if (!EntityRay::IsLowLOD(entity) && Selection::IsEntitySelected(entity))
    {
        m_renderedEntities[entity] = CTimer::m_FrameCounter;
    }
}

void CollisionRenderer::OnEntityRemove(CEntity* entity)
{
    m_renderedEntities.erase(entity);
}

void CollisionRenderer::RetainCollisionForEntity(CEntity* entity)
{
    if (!g_freeCamera || !g_freeCamera->m_active || !g_renderInteriors || !entity->bUsesCollision)
    {
        return;
    }
    if (entity->m_nAreaCode == 0 || entity->m_nAreaCode == 13)
    {
        return;
    }

    CColModel* colModel = entity->GetColModel();
    if (!colModel || !colModel->m_bHasCollisionVolumes || colModel->m_nColSlot == 0)
    {
        return;
    }

    const unsigned char slot = colModel->m_nColSlot;
    if (!CColStore::ms_pColPool || !CColStore::ms_pColPool->GetAt(slot))
    {
        return;
    }

    auto [it, inserted] = m_retainedColSlots.emplace(slot, CTimer::m_FrameCounter);
    it->second = CTimer::m_FrameCounter;
    if (!inserted)
    {
        return;
    }

    CColStore::AddRef(slot);
    if (!CStreaming::ms_disableStreaming)
    {
        CStreaming::RequestModel(CollisionStreamingBase + slot, PRIORITY_REQUEST | KEEP_IN_MEMORY);
    }
}

void CollisionRenderer::PruneRetainedCollisions(unsigned int currentFrame)
{
    for (auto it = m_retainedColSlots.begin(); it != m_retainedColSlots.end();)
    {
        if (it->second == currentFrame)
        {
            ++it;
            continue;
        }

        if (CColStore::ms_pColPool && CColStore::ms_pColPool->GetAt(it->first))
        {
            CColStore::RemoveRef(it->first);
        }
        it = m_retainedColSlots.erase(it);
    }
}

void CollisionRenderer::ReleaseRetainedCollisions()
{
    if (CColStore::ms_pColPool)
    {
        for (const auto& retained : m_retainedColSlots)
        {
            if (CColStore::ms_pColPool->GetAt(retained.first))
            {
                CColStore::RemoveRef(retained.first);
            }
        }
    }
    m_retainedColSlots.clear();
}

const std::vector<CollisionRenderer::Vertex>* CollisionRenderer::GetMesh(CColModel* colModel)
{
    if (!colModel)
    {
        return nullptr;
    }
    if (!colModel->m_pColData)
    {
        m_meshCache.erase(colModel);
        return nullptr;
    }
    const CCollisionData* data = colModel->m_pColData;
    CachedMesh& mesh = m_meshCache[colModel];
    mesh.lastUsedFrame = CTimer::m_FrameCounter;
    const bool matches = mesh.data == data && mesh.spheres == data->m_pSpheres && mesh.boxes == data->m_pBoxes
        && mesh.lines == data->m_pLines && mesh.vertices == data->m_pVertices && mesh.triangles == data->m_pTriangles
        && mesh.numSpheres == data->m_nNumSpheres && mesh.numBoxes == data->m_nNumBoxes && mesh.numLines == data->m_nNumLines
        && mesh.numTriangles == data->m_nNumTriangles && mesh.usesDisks == static_cast<bool>(data->m_nFlags.bUsesDisks);
    if (!matches)
    {
        RebuildMesh(mesh, data);
    }
    return &mesh.lineVertices;
}

void CollisionRenderer::RebuildMesh(CachedMesh& mesh, const CCollisionData* data)
{
    mesh.data = data;
    mesh.spheres = data->m_pSpheres;
    mesh.boxes = data->m_pBoxes;
    mesh.lines = data->m_pLines;
    mesh.vertices = data->m_pVertices;
    mesh.triangles = data->m_pTriangles;
    mesh.numSpheres = data->m_nNumSpheres;
    mesh.numBoxes = data->m_nNumBoxes;
    mesh.numLines = data->m_nNumLines;
    mesh.numTriangles = data->m_nNumTriangles;
    mesh.usesDisks = data->m_nFlags.bUsesDisks;
    mesh.lineVertices.clear();

    mesh.lineVertices.reserve(static_cast<size_t>(data->m_nNumTriangles) * 4 + static_cast<size_t>(data->m_nNumBoxes) * 24
        + static_cast<size_t>(data->m_nNumSpheres) * CircleSegments * 6);

    if (data->m_pVertices && data->m_pTriangles)
    {
        std::unordered_set<unsigned int> edges;
        edges.reserve(static_cast<size_t>(data->m_nNumTriangles) * 3);
        auto getVertex = [&](unsigned short index) -> CVector
        {
            struct RawVec
            {
                short x, y, z;
            };
            RawVec raw;
            memcpy(&raw, reinterpret_cast<const char*>(data->m_pVertices) + (index * 6), 6);
            return CVector(
                static_cast<float>(raw.x) / 128.0f, static_cast<float>(raw.y) / 128.0f, static_cast<float>(raw.z) / 128.0f);
        };

        auto addEdge = [&](unsigned short a, unsigned short b)
        {
            if (a == b)
            {
                return;
            }
            const unsigned short minimum = std::min(a, b);
            const unsigned short maximum = std::max(a, b);
            const unsigned int key = (static_cast<unsigned int>(minimum) << 16) | maximum;
            if (!edges.insert(key).second)
            {
                return;
            }

            AddLine(mesh.lineVertices, getVertex(a), getVertex(b));
        };

        for (unsigned int i = 0; i < data->m_nNumTriangles; ++i)
        {
            const CColTriangle& triangle = data->m_pTriangles[i];
            addEdge(triangle.m_nVertA, triangle.m_nVertB);
            addEdge(triangle.m_nVertB, triangle.m_nVertC);
            addEdge(triangle.m_nVertC, triangle.m_nVertA);
        }
    }

    if (data->m_pBoxes)
    {
        for (unsigned int i = 0; i < data->m_nNumBoxes; ++i)
        {
            AddBox(mesh.lineVertices, data->m_pBoxes[i].m_vecMin, data->m_pBoxes[i].m_vecMax);
        }
    }

    if (data->m_pSpheres)
    {
        for (unsigned int i = 0; i < data->m_nNumSpheres; ++i)
        {
            AddSphere(mesh.lineVertices, data->m_pSpheres[i].m_vecCenter, data->m_pSpheres[i].m_fRadius);
        }
    }

    if (data->m_nNumLines > 0)
    {
        if (data->m_nFlags.bUsesDisks && data->m_pDisks)
        {
            for (unsigned int i = 0; i < data->m_nNumLines; ++i)
            {
                const CColDisk& disk = data->m_pDisks[i];
                AddTaperedLine(mesh.lineVertices, disk.m_vecStart, disk.m_vecEnd, disk.m_fStartRadius, disk.m_fEndRadius);
            }
        }
        else if (data->m_pLines)
        {
            for (unsigned int i = 0; i < data->m_nNumLines; ++i)
            {
                const CColLine& line = data->m_pLines[i];
                AddTaperedLine(mesh.lineVertices, line.m_vecStart, line.m_vecEnd, line.m_fStartSize, line.m_fEndSize);
            }
        }
    }
}

void CollisionRenderer::DrawLines(const std::vector<Vertex>& vertices)
{
    const unsigned int edgeCount = static_cast<unsigned int>(vertices.size() / 2);
    unsigned int firstEdge = 0;
    while (firstEdge < edgeCount)
    {
        const unsigned int count = std::min(m_maxPrimitiveCount, edgeCount - firstEdge);
        if (count == 0)
        {
            break;
        }
        m_device->DrawPrimitiveUP(D3DPT_LINELIST, count, vertices.data() + static_cast<size_t>(firstEdge) * 2, sizeof(Vertex));
        firstEdge += count;
    }
}

void CollisionRenderer::Render()
{
    if (!m_enabled || !m_device || !g_freeCamera || !g_freeCamera->m_active)
    {
        ReleaseRetainedCollisions();
        m_renderedEntities.clear();
        if (!g_freeCamera || !g_freeCamera->m_active)
        {
            m_meshCache.clear();
            m_drawItems.clear();
        }
        return;
    }

    const unsigned int currentFrame = CTimer::m_FrameCounter;
    if (g_renderInteriors)
    {
        PruneRetainedCollisions(currentFrame);
    }
    else
    {
        ReleaseRetainedCollisions();
    }
    if (currentFrame % 120 == 0)
    {
        for (auto it = m_meshCache.begin(); it != m_meshCache.end();)
        {
            if (currentFrame - it->second.lastUsedFrame > 600)
            {
                it = m_meshCache.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    auto& items = m_drawItems;
    if (items.size() < m_renderedEntities.size())
    {
        items.resize(m_renderedEntities.size());
    }
    size_t itemCount = 0;
    D3DXMATRIX identity;
    D3DXMatrixIdentity(&identity);
    const CVector cameraPosition = TheCamera.GetPosition();
    for (auto it = m_renderedEntities.begin(); it != m_renderedEntities.end();)
    {
        if (it->second != CTimer::m_FrameCounter)
        {
            it = m_renderedEntities.erase(it);
            continue;
        }
        CEntity* entity = it->first;
        const std::vector<Vertex>* vertices = entity ? GetMesh(entity->GetColModel()) : nullptr;
        if (vertices && !vertices->empty())
        {
            const D3DXMATRIX world = EntityWorldMatrix(entity);
            bool duplicate = false;
            for (size_t i = 0; i < itemCount; ++i)
            {
                const auto& item = items[i];
                if (item.vertices == vertices && memcmp(&item.sourceWorld, &world, sizeof(world)) == 0)
                {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
            {
                auto& item = items[itemCount++];
                item.sourceWorld = world;
                item.world = world;
                item.vertices = vertices;
                item.offsetVertices.clear();
                const float meshCameraOffset = 0.04f;
                if (meshCameraOffset > 0.0f)
                {
                    item.offsetVertices.reserve(vertices->size());
                    for (const Vertex& vertex : *vertices)
                    {
                        Vertex worldVertex = { vertex.x * world._11 + vertex.y * world._21 + vertex.z * world._31 + world._41,
                            vertex.x * world._12 + vertex.y * world._22 + vertex.z * world._32 + world._42,
                            vertex.x * world._13 + vertex.y * world._23 + vertex.z * world._33 + world._43 };
                        const float dx = cameraPosition.x - worldVertex.x;
                        const float dy = cameraPosition.y - worldVertex.y;
                        const float dz = cameraPosition.z - worldVertex.z;
                        const float distanceSq = dx * dx + dy * dy + dz * dz;
                        if (distanceSq > 0.00000001f)
                        {
                            const float distance = sqrtf(distanceSq);
                            const float offset = std::min(meshCameraOffset, distance * 0.25f);
                            const float scale = offset / distance;
                            worldVertex.x += dx * scale;
                            worldVertex.y += dy * scale;
                            worldVertex.z += dz * scale;
                        }
                        item.offsetVertices.push_back(worldVertex);
                    }
                    item.world = identity;
                }
            }
        }
        ++it;
    }
    if (itemCount == 0)
    {
        return;
    }

    IDirect3DStateBlock9* stateBlock = nullptr;
    if (FAILED(m_device->CreateStateBlock(D3DSBT_ALL, &stateBlock)) || !stateBlock)
    {
        return;
    }
    if (FAILED(stateBlock->Capture()))
    {
        stateBlock->Release();
        return;
    }

    D3DMATRIX oldProjection = {};
    m_device->GetTransform(D3DTS_PROJECTION, &oldProjection);
    D3DMATRIX visibleProjection = oldProjection;
    visibleProjection._33 -= 0.00001f;
    visibleProjection._43 -= 0.00001f;
    m_device->SetTransform(D3DTS_PROJECTION, &visibleProjection);

    m_device->SetFVF(D3DFVF_XYZ);

    m_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    m_device->SetRenderState(D3DRS_COLORWRITEENABLE,
        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    m_device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    m_device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
    m_device->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, TRUE);

    const DWORD zero = FloatBits(0.0f);
    m_device->SetRenderState(D3DRS_DEPTHBIAS, zero);
    m_device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, zero);

    LineRenderer::Render(m_device, { .vertexColors = false, .color = SolidColor, .hiddenAlpha = BYTE(GhostColor >> 24) },
        [&]
        {
            for (size_t i = 0; i < itemCount; ++i)
            {
                const auto& item = items[i];
                m_device->SetTransform(D3DTS_WORLD, &item.world);
                DrawLines(item.offsetVertices.empty() ? *item.vertices : item.offsetVertices);
            }
        });

    stateBlock->Apply();
    stateBlock->Release();
}
