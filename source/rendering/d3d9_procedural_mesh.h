// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>
#include <d3d9.h>
#include <vector>

class D3D9ProceduralMesh final
{
    public:
    D3D9ProceduralMesh() noexcept = default;
    ~D3D9ProceduralMesh() noexcept;

    D3D9ProceduralMesh(const D3D9ProceduralMesh&) = delete;
    D3D9ProceduralMesh& operator=(const D3D9ProceduralMesh&) = delete;

    D3D9ProceduralMesh(D3D9ProceduralMesh&& other) noexcept;
    D3D9ProceduralMesh& operator=(D3D9ProceduralMesh&& other) noexcept;

    HRESULT CreateCylinder(
        IDirect3DDevice9* device, float radius1, float radius2, float length, UINT slices, UINT stacks) noexcept;
    HRESULT CreateSphere(IDirect3DDevice9* device, float radius, UINT slices, UINT stacks) noexcept;
    HRESULT CreateBox(IDirect3DDevice9* device, float width, float height, float depth) noexcept;
    HRESULT CreateTorus(IDirect3DDevice9* device, float innerRadius, float outerRadius, UINT sides, UINT rings) noexcept;

    HRESULT Draw() const noexcept;
    bool Intersect(
        const D3DVECTOR& rayOrigin, const D3DVECTOR& rayDirection, float& distance, DWORD* faceIndex = nullptr) const noexcept;

    void Reset() noexcept;
    bool IsValid() const noexcept;

    private:
    struct Vertex
    {
        float x;
        float y;
        float z;
        float nx;
        float ny;
        float nz;
    };

    HRESULT Upload(IDirect3DDevice9* device, std::vector<Vertex>&& vertices, std::vector<std::uint16_t>&& indices) noexcept;
    void MoveFrom(D3D9ProceduralMesh& other) noexcept;
    bool IntersectsBounds(const D3DVECTOR& rayOrigin, const D3DVECTOR& rayDirection) const noexcept;

    IDirect3DDevice9* m_device = nullptr;
    IDirect3DVertexBuffer9* m_vertexBuffer = nullptr;
    IDirect3DIndexBuffer9* m_indexBuffer = nullptr;
    std::vector<Vertex> m_vertices;
    std::vector<std::uint16_t> m_indices;
    D3DVECTOR m_boundsMin {};
    D3DVECTOR m_boundsMax {};
};
