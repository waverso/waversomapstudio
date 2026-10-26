// Source Repository: https://github.com/waverso/waversomapstudio

#include "game_patches.h"

#include <cstddef>
#include <cstdint>

#include "plugin.h"

static const uint8_t g_kHideHudDrawOn[1] = { 0xC3 };
static const uint8_t g_kHideHudDrawAfterFadeOn[1] = { 0xC3 };
static const uint8_t g_kHideHudStationNameOn[1] = { 0x05 };

static const uint8_t g_kNoRadio4Ea042On[2] = { 0x90, 0x90 };
static const uint8_t g_kNoRadio4Eb66COn[6] = { 0xE9, 0x12, 0x02, 0x00, 0x00, 0x90 };

static const uint8_t g_kNoTargetBlipOn[1] = { 0xEB };

static const uint8_t g_kNoGaragesCameraOn[5] = { 0xB0, 0x00, 0xC3, 0x90, 0x90 };

static const uint8_t g_kRenderInteriorsOn[1] = { 0xEB };

GamePatches::GamePatches()
{
    m_hideHud[0] = { 0x58FAE0, 1, g_kHideHudDrawOn, {}, false };
    m_hideHud[1] = { 0x58D490, 1, g_kHideHudDrawAfterFadeOn, {}, false };
    m_hideHud[2] = { 0x53E4FA, 1, g_kHideHudStationNameOn, {}, false };

    m_noRadio[0] = { 0x4EA042, 2, g_kNoRadio4Ea042On, {}, false };
    m_noRadio[1] = { 0x4EB66C, 6, g_kNoRadio4Eb66COn, {}, false };

    m_noTargetBlip[0] = { 0x53E1EC, 1, g_kNoTargetBlipOn, {}, false };

    m_noGaragesCamera[0] = { 0x448650, 5, g_kNoGaragesCameraOn, {}, false };

    m_renderInteriors[0] = { 0x553624, 1, g_kRenderInteriorsOn, {}, false };
    m_renderInteriors[1] = { 0x5536A3, 1, g_kRenderInteriorsOn, {}, false };
    m_renderInteriors[2] = { 0x554484, 1, g_kRenderInteriorsOn, {}, false };
    m_renderInteriors[3] = { 0x554677, 1, g_kRenderInteriorsOn, {}, false };
    m_renderInteriors[4] = { 0x554EC6, 1, g_kRenderInteriorsOn, {}, false };
}

GamePatches::~GamePatches()
{
    SetEditMode(PatchState::RestoreOriginal);
}

void GamePatches::Apply(PatchSite& s, PatchState st)
{
    if (st == PatchState::On)
    {
        if (!s.hasSaved)
        {
            plugin::patch::GetRaw(s.addr, s.saved, s.size);
            s.hasSaved = true;
        }
        plugin::patch::SetRaw(s.addr, (void*)s.onBytes, s.size);
    }
    else
    {
        if (!s.hasSaved)
        {
            return;
        }
        plugin::patch::SetRaw(s.addr, s.saved, s.size);
    }
    FlushInstructionCache(GetCurrentProcess(), (void*)s.addr, s.size);
}

void GamePatches::ApplyMany(PatchSite* sites, size_t n, PatchState s)
{
    for (size_t i = 0; i < n; ++i)
    {
        Apply(sites[i], s);
    }
}

void GamePatches::SetHideHud(PatchState s)
{
    ApplyMany(m_hideHud, _countof(m_hideHud), s);
}
void GamePatches::SetNoRadio(PatchState s)
{
    ApplyMany(m_noRadio, _countof(m_noRadio), s);
}
void GamePatches::SetNoTargetBlip(PatchState s)
{
    ApplyMany(m_noTargetBlip, _countof(m_noTargetBlip), s);
}
void GamePatches::SetNoGaragesCamera(PatchState s)
{
    ApplyMany(m_noGaragesCamera, _countof(m_noGaragesCamera), s);
}

void GamePatches::SetRenderInteriors(PatchState s)
{
    if (s == PatchState::On)
    {
        for (const PatchSite& site : m_renderInteriors)
        {
            if (!site.hasSaved && plugin::patch::GetUChar(site.addr, false) != 0x74)
            {
                return;
            }
        }
    }

    ApplyMany(m_renderInteriors, _countof(m_renderInteriors), s);
}

void GamePatches::SetEditMode(PatchState s)
{
    SetHideHud(s);
    SetNoRadio(s);
    SetNoTargetBlip(s);
    SetNoGaragesCamera(s);
}
