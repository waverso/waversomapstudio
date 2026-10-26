// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <d3d9.h>
#include <unordered_map>
#include <vector>

class CEntity;
class CColModel;
class CCollisionData;

class CollisionRenderer
{
    public:
    CollisionRenderer();

    void Render();
    void OnEntityRender(CEntity* entity);
    void OnEntityRemove(CEntity* entity);
    void SetEnabled(bool enabled);
    bool IsEnabled() const
    {
        return m_enabled;
    }

    struct Vertex
    {
        float x;
        float y;
        float z;
    };

    private:
    struct DrawItem
    {
        D3DMATRIX sourceWorld;
        D3DMATRIX world;
        const std::vector<Vertex>* vertices = nullptr;
        std::vector<Vertex> offsetVertices;
    };

    struct CachedMesh
    {
        const CCollisionData* data = nullptr;
        const void* spheres = nullptr;
        const void* boxes = nullptr;
        const void* lines = nullptr;
        const void* vertices = nullptr;
        const void* triangles = nullptr;
        unsigned int numSpheres = 0;
        unsigned int numBoxes = 0;
        unsigned int numLines = 0;
        unsigned int numTriangles = 0;
        unsigned int lastUsedFrame = 0;
        bool usesDisks = false;
        std::vector<Vertex> lineVertices;
    };

    const std::vector<Vertex>* GetMesh(CColModel* colModel);
    void RebuildMesh(CachedMesh& mesh, const CCollisionData* data);
    void DrawLines(const std::vector<Vertex>& vertices);
    void RetainCollisionForEntity(CEntity* entity);
    void PruneRetainedCollisions(unsigned int currentFrame);
    void ReleaseRetainedCollisions();

    IDirect3DDevice9* m_device = nullptr;
    unsigned int m_maxPrimitiveCount = 65535;
    bool m_enabled = false;
    std::vector<DrawItem> m_drawItems;
    std::unordered_map<CEntity*, unsigned int> m_renderedEntities;
    std::unordered_map<const CColModel*, CachedMesh> m_meshCache;
    std::unordered_map<unsigned char, unsigned int> m_retainedColSlots;
};

extern CollisionRenderer* g_collisionRenderer;
