// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <memory>
#include <vector>

#include "CVector.h"

class CEntity;
class CPed;

struct ActorMesh
{
    std::vector<CVector> vertices;
    std::vector<unsigned short> indices;
};

class ActorRuntime
{
    public:
    ActorRuntime();
    ~ActorRuntime();
    void Update();
    void Clear();
    int LookupItemIndex(CEntity* entity) const;
    CPed* GetPed(int itemIndex) const;
    void ReplayAnimation(int itemIndex);
    const std::vector<ActorMesh>* Meshes(CEntity* entity);
    int PreviewCapacity() const;

    private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

extern ActorRuntime* g_actorRuntime;
