// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstddef>
#include <cstdint>

enum class PatchState
{
    On,
    RestoreOriginal
};

class GamePatches
{
    public:
    GamePatches();
    ~GamePatches();

    void SetEditMode(PatchState s);

    void SetHideHud(PatchState s);
    void SetNoRadio(PatchState s);
    void SetNoTargetBlip(PatchState s);
    void SetNoGaragesCamera(PatchState s);
    void SetRenderInteriors(PatchState s);

    private:
    struct PatchSite
    {
        uintptr_t addr;
        size_t size;
        const uint8_t* onBytes;
        uint8_t saved[8];
        bool hasSaved;
    };

    static void Apply(PatchSite& site, PatchState s);
    static void ApplyMany(PatchSite* sites, size_t n, PatchState s);

    PatchSite m_hideHud[3];
    PatchSite m_noRadio[2];
    PatchSite m_noTargetBlip[1];
    PatchSite m_noGaragesCamera[1];
    PatchSite m_renderInteriors[5];
};

extern GamePatches* g_gamePatches;
