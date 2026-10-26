// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <memory>

class CEntity;
class CVehicle;

class VehicleRuntime
{
    public:
    VehicleRuntime();
    ~VehicleRuntime();
    void Update();
    int LookupItemIndex(CEntity* entity) const;
    CVehicle* GetVehicle(int itemIndex) const;
    int PreviewCapacity() const;
    void Clear();

    private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

extern VehicleRuntime* g_vehicleRuntime;
int LookupEditorItemIndex(CEntity* entity);
