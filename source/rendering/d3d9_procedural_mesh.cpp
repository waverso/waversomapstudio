// Source Repository: https://github.com/waverso/waversomapstudio

#include "d3d9_procedural_mesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <numbers>
#include <utility>

namespace
{
    using std::numbers::pi_v;
    constexpr DWORD MeshFvf = D3DFVF_XYZ | D3DFVF_NORMAL;

    bool FitsIndex16(std::uint64_t vertexCount) noexcept
    {
        return vertexCount > 0 && vertexCount <= static_cast<std::uint64_t>((std::numeric_limits<std::uint16_t>::max)()) + 1u;
    }

    bool FitsVectorSize(std::uint64_t count) noexcept
    {
        return count <= static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)());
    }
}

D3D9ProceduralMesh::~D3D9ProceduralMesh() noexcept
{
    Reset();
}

D3D9ProceduralMesh::D3D9ProceduralMesh(D3D9ProceduralMesh&& other) noexcept
{
    MoveFrom(other);
}

D3D9ProceduralMesh& D3D9ProceduralMesh::operator=(D3D9ProceduralMesh&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        MoveFrom(other);
    }
    return *this;
}

HRESULT D3D9ProceduralMesh::CreateCylinder(
    IDirect3DDevice9* device, float radius1, float radius2, float length, UINT slices, UINT stacks) noexcept
{
    if (!device || !std::isfinite(radius1) || !std::isfinite(radius2) || !std::isfinite(length) || radius1 < 0.0f
        || radius2 < 0.0f || (radius1 == 0.0f && radius2 == 0.0f) || length <= 0.0f || slices < 3 || stacks < 1)
    {
        return D3DERR_INVALIDCALL;
    }

    const bool hasBottomCap = radius1 > 0.0f;
    const bool hasTopCap = radius2 > 0.0f;
    const std::uint64_t bodyVertices = (static_cast<std::uint64_t>(stacks) + 1u) * (static_cast<std::uint64_t>(slices) + 1u);
    const std::uint64_t capVertices
        = static_cast<std::uint64_t>(hasBottomCap + hasTopCap) * (static_cast<std::uint64_t>(slices) + 1u);
    const std::uint64_t vertexCount = bodyVertices + capVertices;
    const std::uint64_t bodyFaces = static_cast<std::uint64_t>(stacks) * slices * 2u;
    const std::uint64_t capFaces = static_cast<std::uint64_t>(hasBottomCap + hasTopCap) * slices;
    const std::uint64_t maxIndexCount = (bodyFaces + capFaces) * 3u;
    if (!FitsIndex16(vertexCount) || !FitsVectorSize(maxIndexCount))
    {
        return D3DERR_INVALIDCALL;
    }

    try
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint16_t> indices;
        vertices.reserve(static_cast<std::size_t>(vertexCount));
        indices.reserve(static_cast<std::size_t>(maxIndexCount));

        const float halfLength = length * 0.5f;
        const float normalZ = (radius1 - radius2) / length;
        const float normalScale = 1.0f / std::sqrt(1.0f + normalZ * normalZ);

        for (UINT stack = 0; stack <= stacks; ++stack)
        {
            const float v = static_cast<float>(stack) / static_cast<float>(stacks);
            const float radius = radius1 + (radius2 - radius1) * v;
            const float z = -halfLength + length * v;
            for (UINT slice = 0; slice <= slices; ++slice)
            {
                const float angle = (2.0f * pi_v<float> * static_cast<float>(slice)) / static_cast<float>(slices);
                const float c = std::cos(angle);
                const float s = std::sin(angle);
                vertices.push_back({ radius * c, radius * s, z, c * normalScale, s * normalScale, normalZ * normalScale });
            }
        }

        const UINT bodyStride = slices + 1u;
        for (UINT stack = 0; stack < stacks; ++stack)
        {
            const float lowerV = static_cast<float>(stack) / static_cast<float>(stacks);
            const float upperV = static_cast<float>(stack + 1u) / static_cast<float>(stacks);
            const float lowerRadius = radius1 + (radius2 - radius1) * lowerV;
            const float upperRadius = radius1 + (radius2 - radius1) * upperV;
            for (UINT slice = 0; slice < slices; ++slice)
            {
                const std::uint16_t a = static_cast<std::uint16_t>(stack * bodyStride + slice);
                const std::uint16_t b = static_cast<std::uint16_t>(a + 1u);
                const std::uint16_t c = static_cast<std::uint16_t>((stack + 1u) * bodyStride + slice);
                const std::uint16_t d = static_cast<std::uint16_t>(c + 1u);
                if (lowerRadius > 0.0f)
                {
                    indices.push_back(a);
                    indices.push_back(b);
                    indices.push_back(c);
                }
                if (upperRadius > 0.0f)
                {
                    indices.push_back(b);
                    indices.push_back(d);
                    indices.push_back(c);
                }
            }
        }

        if (hasBottomCap)
        {
            const std::uint16_t center = static_cast<std::uint16_t>(vertices.size());
            vertices.push_back({ 0.0f, 0.0f, -halfLength, 0.0f, 0.0f, -1.0f });
            const std::uint16_t first = static_cast<std::uint16_t>(vertices.size());
            for (UINT slice = 0; slice < slices; ++slice)
            {
                const float angle = (2.0f * pi_v<float> * static_cast<float>(slice)) / static_cast<float>(slices);
                vertices.push_back({ radius1 * std::cos(angle), radius1 * std::sin(angle), -halfLength, 0.0f, 0.0f, -1.0f });
            }
            for (UINT slice = 0; slice < slices; ++slice)
            {
                const std::uint16_t current = static_cast<std::uint16_t>(first + slice);
                const std::uint16_t next = static_cast<std::uint16_t>(first + ((slice + 1u) % slices));
                indices.push_back(center);
                indices.push_back(next);
                indices.push_back(current);
            }
        }

        if (hasTopCap)
        {
            const std::uint16_t center = static_cast<std::uint16_t>(vertices.size());
            vertices.push_back({ 0.0f, 0.0f, halfLength, 0.0f, 0.0f, 1.0f });
            const std::uint16_t first = static_cast<std::uint16_t>(vertices.size());
            for (UINT slice = 0; slice < slices; ++slice)
            {
                const float angle = (2.0f * pi_v<float> * static_cast<float>(slice)) / static_cast<float>(slices);
                vertices.push_back({ radius2 * std::cos(angle), radius2 * std::sin(angle), halfLength, 0.0f, 0.0f, 1.0f });
            }
            for (UINT slice = 0; slice < slices; ++slice)
            {
                const std::uint16_t current = static_cast<std::uint16_t>(first + slice);
                const std::uint16_t next = static_cast<std::uint16_t>(first + ((slice + 1u) % slices));
                indices.push_back(center);
                indices.push_back(current);
                indices.push_back(next);
            }
        }

        return Upload(device, std::move(vertices), std::move(indices));
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

HRESULT D3D9ProceduralMesh::CreateSphere(IDirect3DDevice9* device, float radius, UINT slices, UINT stacks) noexcept
{
    if (!device || !std::isfinite(radius) || radius <= 0.0f || slices < 3 || stacks < 2)
    {
        return D3DERR_INVALIDCALL;
    }

    const std::uint64_t vertexCount = 2u + (static_cast<std::uint64_t>(stacks) - 1u) * (static_cast<std::uint64_t>(slices) + 1u);
    const std::uint64_t faceCount = 2u * static_cast<std::uint64_t>(slices) * (static_cast<std::uint64_t>(stacks) - 1u);
    const std::uint64_t indexCount = faceCount * 3u;
    if (!FitsIndex16(vertexCount) || !FitsVectorSize(indexCount))
    {
        return D3DERR_INVALIDCALL;
    }

    try
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint16_t> indices;
        vertices.reserve(static_cast<std::size_t>(vertexCount));
        indices.reserve(static_cast<std::size_t>(indexCount));

        vertices.push_back({ 0.0f, 0.0f, radius, 0.0f, 0.0f, 1.0f });
        for (UINT stack = 1; stack < stacks; ++stack)
        {
            const float phi = (pi_v<float> * static_cast<float>(stack)) / static_cast<float>(stacks);
            const float ringRadius = std::sin(phi);
            const float nz = std::cos(phi);
            for (UINT slice = 0; slice <= slices; ++slice)
            {
                const float theta = (2.0f * pi_v<float> * static_cast<float>(slice)) / static_cast<float>(slices);
                const float nx = ringRadius * std::cos(theta);
                const float ny = ringRadius * std::sin(theta);
                vertices.push_back({ radius * nx, radius * ny, radius * nz, nx, ny, nz });
            }
        }
        const std::uint16_t bottom = static_cast<std::uint16_t>(vertices.size());
        vertices.push_back({ 0.0f, 0.0f, -radius, 0.0f, 0.0f, -1.0f });

        const UINT stride = slices + 1u;
        const std::uint16_t firstRing = 1u;
        for (UINT slice = 0; slice < slices; ++slice)
        {
            const std::uint16_t a = static_cast<std::uint16_t>(firstRing + slice);
            const std::uint16_t b = static_cast<std::uint16_t>(a + 1u);
            indices.push_back(0u);
            indices.push_back(a);
            indices.push_back(b);
        }

        for (UINT stack = 0; stack + 2u < stacks; ++stack)
        {
            const UINT upperStart = 1u + stack * stride;
            const UINT lowerStart = upperStart + stride;
            for (UINT slice = 0; slice < slices; ++slice)
            {
                const std::uint16_t a = static_cast<std::uint16_t>(upperStart + slice);
                const std::uint16_t b = static_cast<std::uint16_t>(a + 1u);
                const std::uint16_t c = static_cast<std::uint16_t>(lowerStart + slice);
                const std::uint16_t d = static_cast<std::uint16_t>(c + 1u);
                indices.push_back(a);
                indices.push_back(c);
                indices.push_back(b);
                indices.push_back(b);
                indices.push_back(c);
                indices.push_back(d);
            }
        }

        const UINT lastRingStart = 1u + (stacks - 2u) * stride;
        for (UINT slice = 0; slice < slices; ++slice)
        {
            const std::uint16_t a = static_cast<std::uint16_t>(lastRingStart + slice);
            const std::uint16_t b = static_cast<std::uint16_t>(a + 1u);
            indices.push_back(a);
            indices.push_back(bottom);
            indices.push_back(b);
        }

        return Upload(device, std::move(vertices), std::move(indices));
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

HRESULT D3D9ProceduralMesh::CreateBox(IDirect3DDevice9* device, float width, float height, float depth) noexcept
{
    if (!device || !std::isfinite(width) || !std::isfinite(height) || !std::isfinite(depth) || width <= 0.0f || height <= 0.0f
        || depth <= 0.0f)
    {
        return D3DERR_INVALIDCALL;
    }

    try
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint16_t> indices;
        vertices.reserve(24u);
        indices.reserve(36u);

        const float x = width * 0.5f;
        const float y = height * 0.5f;
        const float z = depth * 0.5f;

        const auto addFace
            = [&](const D3DVECTOR& a, const D3DVECTOR& b, const D3DVECTOR& c, const D3DVECTOR& d, const D3DVECTOR& normal)
        {
            const std::uint16_t base = static_cast<std::uint16_t>(vertices.size());
            vertices.push_back({ a.x, a.y, a.z, normal.x, normal.y, normal.z });
            vertices.push_back({ b.x, b.y, b.z, normal.x, normal.y, normal.z });
            vertices.push_back({ c.x, c.y, c.z, normal.x, normal.y, normal.z });
            vertices.push_back({ d.x, d.y, d.z, normal.x, normal.y, normal.z });
            indices.push_back(base);
            indices.push_back(static_cast<std::uint16_t>(base + 1u));
            indices.push_back(static_cast<std::uint16_t>(base + 2u));
            indices.push_back(base);
            indices.push_back(static_cast<std::uint16_t>(base + 2u));
            indices.push_back(static_cast<std::uint16_t>(base + 3u));
        };

        addFace({ x, -y, -z }, { x, y, -z }, { x, y, z }, { x, -y, z }, { 1.0f, 0.0f, 0.0f });
        addFace({ -x, y, -z }, { -x, -y, -z }, { -x, -y, z }, { -x, y, z }, { -1.0f, 0.0f, 0.0f });
        addFace({ x, y, -z }, { -x, y, -z }, { -x, y, z }, { x, y, z }, { 0.0f, 1.0f, 0.0f });
        addFace({ -x, -y, -z }, { x, -y, -z }, { x, -y, z }, { -x, -y, z }, { 0.0f, -1.0f, 0.0f });
        addFace({ -x, -y, z }, { x, -y, z }, { x, y, z }, { -x, y, z }, { 0.0f, 0.0f, 1.0f });
        addFace({ -x, y, -z }, { x, y, -z }, { x, -y, -z }, { -x, -y, -z }, { 0.0f, 0.0f, -1.0f });

        return Upload(device, std::move(vertices), std::move(indices));
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

HRESULT D3D9ProceduralMesh::CreateTorus(
    IDirect3DDevice9* device, float innerRadius, float outerRadius, UINT sides, UINT rings) noexcept
{
    if (!device || !std::isfinite(innerRadius) || !std::isfinite(outerRadius) || innerRadius <= 0.0f || outerRadius <= 0.0f
        || sides < 3 || rings < 3)
    {
        return D3DERR_INVALIDCALL;
    }

    const std::uint64_t vertexCount = (static_cast<std::uint64_t>(rings) + 1u) * (static_cast<std::uint64_t>(sides) + 1u);
    const std::uint64_t faceCount = static_cast<std::uint64_t>(rings) * sides * 2u;
    const std::uint64_t indexCount = faceCount * 3u;
    if (!FitsIndex16(vertexCount) || !FitsVectorSize(indexCount))
    {
        return D3DERR_INVALIDCALL;
    }

    try
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint16_t> indices;
        vertices.reserve(static_cast<std::size_t>(vertexCount));
        indices.reserve(static_cast<std::size_t>(indexCount));

        for (UINT ring = 0; ring <= rings; ++ring)
        {
            const float theta = (2.0f * pi_v<float> * static_cast<float>(ring)) / static_cast<float>(rings);
            const float ringCos = std::cos(theta);
            const float ringSin = std::sin(theta);
            for (UINT side = 0; side <= sides; ++side)
            {
                const float phi = (2.0f * pi_v<float> * static_cast<float>(side)) / static_cast<float>(sides);
                const float sideCos = std::cos(phi);
                const float sideSin = std::sin(phi);
                const float radial = outerRadius + innerRadius * sideCos;
                vertices.push_back(
                    { radial * ringCos, radial * ringSin, innerRadius * sideSin, sideCos * ringCos, sideCos * ringSin, sideSin });
            }
        }

        const UINT stride = sides + 1u;
        for (UINT ring = 0; ring < rings; ++ring)
        {
            for (UINT side = 0; side < sides; ++side)
            {
                const std::uint16_t a = static_cast<std::uint16_t>(ring * stride + side);
                const std::uint16_t b = static_cast<std::uint16_t>((ring + 1u) * stride + side);
                const std::uint16_t c = static_cast<std::uint16_t>(a + 1u);
                const std::uint16_t d = static_cast<std::uint16_t>(b + 1u);
                indices.push_back(a);
                indices.push_back(b);
                indices.push_back(c);
                indices.push_back(b);
                indices.push_back(d);
                indices.push_back(c);
            }
        }

        return Upload(device, std::move(vertices), std::move(indices));
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

HRESULT D3D9ProceduralMesh::Draw() const noexcept
{
    if (!IsValid())
    {
        return D3DERR_INVALIDCALL;
    }

    HRESULT result = m_device->SetStreamSource(0, m_vertexBuffer, 0, sizeof(Vertex));
    if (FAILED(result))
    {
        return result;
    }
    result = m_device->SetIndices(m_indexBuffer);
    if (FAILED(result))
    {
        return result;
    }
    result = m_device->SetFVF(MeshFvf);
    if (FAILED(result))
    {
        return result;
    }
    return m_device->DrawIndexedPrimitive(
        D3DPT_TRIANGLELIST, 0, 0, static_cast<UINT>(m_vertices.size()), 0, static_cast<UINT>(m_indices.size() / 3u));
}

bool D3D9ProceduralMesh::Intersect(
    const D3DVECTOR& rayOrigin, const D3DVECTOR& rayDirection, float& distance, DWORD* faceIndex) const noexcept
{
    if (m_vertices.empty() || m_indices.empty() || !std::isfinite(rayOrigin.x) || !std::isfinite(rayOrigin.y)
        || !std::isfinite(rayOrigin.z) || !std::isfinite(rayDirection.x) || !std::isfinite(rayDirection.y)
        || !std::isfinite(rayDirection.z))
    {
        return false;
    }

    const double dx = rayDirection.x;
    const double dy = rayDirection.y;
    const double dz = rayDirection.z;
    const double directionLengthSq = dx * dx + dy * dy + dz * dz;
    if (directionLengthSq <= 1.0e-20 || !IntersectsBounds(rayOrigin, rayDirection))
    {
        return false;
    }

    double nearest = std::numeric_limits<double>::infinity();
    DWORD nearestFace = 0;
    const std::size_t faceCount = m_indices.size() / 3u;
    for (std::size_t face = 0; face < faceCount; ++face)
    {
        const Vertex& v0 = m_vertices[m_indices[face * 3u]];
        const Vertex& v1 = m_vertices[m_indices[face * 3u + 1u]];
        const Vertex& v2 = m_vertices[m_indices[face * 3u + 2u]];

        const double e1x = static_cast<double>(v1.x) - v0.x;
        const double e1y = static_cast<double>(v1.y) - v0.y;
        const double e1z = static_cast<double>(v1.z) - v0.z;
        const double e2x = static_cast<double>(v2.x) - v0.x;
        const double e2y = static_cast<double>(v2.y) - v0.y;
        const double e2z = static_cast<double>(v2.z) - v0.z;

        const double px = dy * e2z - dz * e2y;
        const double py = dz * e2x - dx * e2z;
        const double pz = dx * e2y - dy * e2x;
        const double determinant = e1x * px + e1y * py + e1z * pz;
        if (std::abs(determinant) <= 1.0e-12)
        {
            continue;
        }

        const double inverseDeterminant = 1.0 / determinant;
        const double tx = static_cast<double>(rayOrigin.x) - v0.x;
        const double ty = static_cast<double>(rayOrigin.y) - v0.y;
        const double tz = static_cast<double>(rayOrigin.z) - v0.z;
        const double u = (tx * px + ty * py + tz * pz) * inverseDeterminant;
        if (u < -1.0e-7 || u > 1.0000001)
        {
            continue;
        }

        const double qx = ty * e1z - tz * e1y;
        const double qy = tz * e1x - tx * e1z;
        const double qz = tx * e1y - ty * e1x;
        const double v = (dx * qx + dy * qy + dz * qz) * inverseDeterminant;
        if (v < -1.0e-7 || u + v > 1.0000001)
        {
            continue;
        }

        const double hitDistance = (e2x * qx + e2y * qy + e2z * qz) * inverseDeterminant;
        if (hitDistance >= 0.0 && hitDistance < nearest)
        {
            nearest = hitDistance;
            nearestFace = static_cast<DWORD>(face);
        }
    }

    if (!std::isfinite(nearest) || nearest > (std::numeric_limits<float>::max)())
    {
        return false;
    }

    distance = static_cast<float>(nearest);
    if (faceIndex)
    {
        *faceIndex = nearestFace;
    }
    return true;
}

void D3D9ProceduralMesh::Reset() noexcept
{
    if (m_indexBuffer)
    {
        m_indexBuffer->Release();
        m_indexBuffer = nullptr;
    }
    if (m_vertexBuffer)
    {
        m_vertexBuffer->Release();
        m_vertexBuffer = nullptr;
    }
    if (m_device)
    {
        m_device->Release();
        m_device = nullptr;
    }
    m_vertices.clear();
    m_indices.clear();
    m_boundsMin = {};
    m_boundsMax = {};
}

bool D3D9ProceduralMesh::IsValid() const noexcept
{
    return m_device && m_vertexBuffer && m_indexBuffer && !m_vertices.empty() && !m_indices.empty();
}

HRESULT D3D9ProceduralMesh::Upload(
    IDirect3DDevice9* device, std::vector<Vertex>&& vertices, std::vector<std::uint16_t>&& indices) noexcept
{
    if (!device || vertices.empty() || indices.empty() || indices.size() % 3u != 0u || !FitsIndex16(vertices.size()))
    {
        return D3DERR_INVALIDCALL;
    }

    const std::size_t vertexBytesSize = vertices.size() * sizeof(Vertex);
    const std::size_t indexBytesSize = indices.size() * sizeof(std::uint16_t);
    if (vertexBytesSize > (std::numeric_limits<UINT>::max)() || indexBytesSize > (std::numeric_limits<UINT>::max)())
    {
        return D3DERR_INVALIDCALL;
    }

    D3DVECTOR boundsMin = { vertices[0].x, vertices[0].y, vertices[0].z };
    D3DVECTOR boundsMax = boundsMin;
    for (const Vertex& vertex : vertices)
    {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z) || !std::isfinite(vertex.nx)
            || !std::isfinite(vertex.ny) || !std::isfinite(vertex.nz))
        {
            return D3DERR_INVALIDCALL;
        }
        boundsMin.x = (std::min)(boundsMin.x, vertex.x);
        boundsMin.y = (std::min)(boundsMin.y, vertex.y);
        boundsMin.z = (std::min)(boundsMin.z, vertex.z);
        boundsMax.x = (std::max)(boundsMax.x, vertex.x);
        boundsMax.y = (std::max)(boundsMax.y, vertex.y);
        boundsMax.z = (std::max)(boundsMax.z, vertex.z);
    }
    for (std::uint16_t index : indices)
    {
        if (index >= vertices.size())
        {
            return D3DERR_INVALIDCALL;
        }
    }

    D3DCAPS9 caps {};
    HRESULT result = device->GetDeviceCaps(&caps);
    if (FAILED(result))
    {
        return result;
    }
    if (vertices.size() - 1u > caps.MaxVertexIndex || indices.size() / 3u > caps.MaxPrimitiveCount)
    {
        return D3DERR_INVALIDCALL;
    }

    IDirect3DVertexBuffer9* vertexBuffer = nullptr;
    IDirect3DIndexBuffer9* indexBuffer = nullptr;
    result = device->CreateVertexBuffer(
        static_cast<UINT>(vertexBytesSize), D3DUSAGE_WRITEONLY, MeshFvf, D3DPOOL_MANAGED, &vertexBuffer, nullptr);
    if (FAILED(result))
    {
        return result;
    }

    result = device->CreateIndexBuffer(
        static_cast<UINT>(indexBytesSize), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &indexBuffer, nullptr);
    if (FAILED(result))
    {
        vertexBuffer->Release();
        return result;
    }

    void* vertexData = nullptr;
    result = vertexBuffer->Lock(0, static_cast<UINT>(vertexBytesSize), &vertexData, 0);
    if (SUCCEEDED(result))
    {
        std::memcpy(vertexData, vertices.data(), vertexBytesSize);
        result = vertexBuffer->Unlock();
    }
    if (FAILED(result))
    {
        indexBuffer->Release();
        vertexBuffer->Release();
        return result;
    }

    void* indexData = nullptr;
    result = indexBuffer->Lock(0, static_cast<UINT>(indexBytesSize), &indexData, 0);
    if (SUCCEEDED(result))
    {
        std::memcpy(indexData, indices.data(), indexBytesSize);
        result = indexBuffer->Unlock();
    }
    if (FAILED(result))
    {
        indexBuffer->Release();
        vertexBuffer->Release();
        return result;
    }

    device->AddRef();
    Reset();
    m_device = device;
    m_vertexBuffer = vertexBuffer;
    m_indexBuffer = indexBuffer;
    m_vertices = std::move(vertices);
    m_indices = std::move(indices);
    m_boundsMin = boundsMin;
    m_boundsMax = boundsMax;
    return S_OK;
}

void D3D9ProceduralMesh::MoveFrom(D3D9ProceduralMesh& other) noexcept
{
    m_device = other.m_device;
    m_vertexBuffer = other.m_vertexBuffer;
    m_indexBuffer = other.m_indexBuffer;
    m_vertices = std::move(other.m_vertices);
    m_indices = std::move(other.m_indices);
    m_boundsMin = other.m_boundsMin;
    m_boundsMax = other.m_boundsMax;

    other.m_device = nullptr;
    other.m_vertexBuffer = nullptr;
    other.m_indexBuffer = nullptr;
    other.m_boundsMin = {};
    other.m_boundsMax = {};
}

bool D3D9ProceduralMesh::IntersectsBounds(const D3DVECTOR& rayOrigin, const D3DVECTOR& rayDirection) const noexcept
{
    double minimumDistance = 0.0;
    double maximumDistance = std::numeric_limits<double>::infinity();

    const double origins[3] = { rayOrigin.x, rayOrigin.y, rayOrigin.z };
    const double directions[3] = { rayDirection.x, rayDirection.y, rayDirection.z };
    const double boundsMin[3] = { m_boundsMin.x, m_boundsMin.y, m_boundsMin.z };
    const double boundsMax[3] = { m_boundsMax.x, m_boundsMax.y, m_boundsMax.z };
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::abs(directions[axis]) <= 1.0e-15)
        {
            if (origins[axis] < boundsMin[axis] || origins[axis] > boundsMax[axis])
            {
                return false;
            }
            continue;
        }

        double nearDistance = (boundsMin[axis] - origins[axis]) / directions[axis];
        double farDistance = (boundsMax[axis] - origins[axis]) / directions[axis];
        if (nearDistance > farDistance)
        {
            std::swap(nearDistance, farDistance);
        }
        minimumDistance = (std::max)(minimumDistance, nearDistance);
        maximumDistance = (std::min)(maximumDistance, farDistance);
        if (maximumDistance < minimumDistance)
        {
            return false;
        }
    }
    return true;
}
