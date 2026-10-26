// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <memory>

class CEntity;
class CObject;
class CPickup;
struct CVector;

class PickupRuntime
{
    public:
    PickupRuntime();
    ~PickupRuntime();
    void Update();
    void Clear();
    int LookupItemIndex(CEntity* entity) const;
    CObject* GetObject(int itemIndex) const;
    bool GetPosition(int itemIndex, CVector& position) const;
    bool OwnsPickup(CPickup* pickup) const;
    bool IsSpawning() const;
    int PreviewCapacity() const;
    static bool IsValidModel(int modelId);
    static float GetVisualScale(int modelId);

    private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

extern PickupRuntime* g_pickupRuntime;

bool PickupIsPickupObject(CObject* obj);
