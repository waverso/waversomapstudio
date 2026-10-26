// Source Repository: https://github.com/waverso/waversomapstudio

#include "races.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

#include "../utils/transform_math.h"
#include "C3dMarkers.h"
#include "CDraw.h"
#include "CPed.h"
#include "CRadar.h"
#include "CSpecialFX.h"
#include "CTimer.h"
#include "CWorld.h"
#include "common.h"
#include "../scene/free_camera.h"
#include "../hooks/game_hooks.h"
#include "race_geometry.h"
#include "race_simulation.h"
#include "race_visual_motion.h"
#include "../hooks/samp.h"
#include "../scene/scene.h"
#include "../ui/theme.h"

namespace Races
{
    namespace
    {
        struct Checkpoint
        {
            CVector position;
            RaceCheckpointData data;
        };

#include "races_markers.inl"

        Marker g_tube;
        Marker g_ring;
        Marker g_arrow;
        std::array<Visual, EditorScene::MaxItems> g_visuals;
        Simulation g_simulation;
        std::vector<Checkpoint> g_checkpoints;
        RaceData g_settings;
        char g_raceName[64] {};
        std::array<int, 5> g_blips { -1, -1, -1, -1, -1 };
        constexpr std::array<unsigned char, 5> iconAlpha { 255, 40, 30, 20, 10 };
        bool g_wasEditing = false;
        int g_startDelay = 0;
        DWORD g_startTick = 0;

        void ClearIcons()
        {
            for (int& handle : g_blips)
            {
                if (handle != -1 && CRadar::GetActualBlipArrayIndex(handle) >= 0)
                {
                    CRadar::ClearBlip(handle);
                }
                handle = -1;
            }
        }

        void ShowIcons()
        {
            ClearIcons();
            const auto targets = g_simulation.Icons();
            for (int i = 0; i < 5; ++i)
            {
                if (targets[i] < 0)
                {
                    break;
                }
                const int handle
                    = CRadar::SetCoordBlip(BLIP_COORD, g_checkpoints[targets[i]].position, 0, BLIP_DISPLAY_BLIP_ONLY, nullptr);
                if (handle == -1)
                {
                    break;
                }
                g_blips[i] = handle;
                CRadar::ChangeBlipColour(handle, 0xFF000000u | iconAlpha[i]);
                CRadar::ChangeBlipScale(handle, 3);
                CRadar::SetCoordBlipAppearance(handle, BLIP_FLAG_FRIEND);
                const int index = CRadar::GetActualBlipArrayIndex(handle);
                if (index >= 0)
                {
                    CRadar::ms_RadarTrace[index].m_bBlipFade = false;
                    CRadar::ms_RadarTrace[index].m_bShortRange = false;
                }
            }
        }

        void StartSelection()
        {
            Stop();
            if (!g_scene || !FindPlayerPed())
            {
                return;
            }
            int race = -1;
            for (const int index : g_scene->DisplayOrder())
            {
                const auto& item = g_scene->items[index];
                if (!item.selected)
                {
                    continue;
                }
                const int selectedRace = item.type == SceneItemRace ? index
                    : item.type == SceneItemRaceCheckpoint          ? item.parent
                                                                    : -1;
                if (selectedRace >= 0)
                {
                    if (race >= 0 && race != selectedRace)
                    {
                        return;
                    }
                    race = selectedRace;
                }
            }
            if (race < 0)
            {
                return;
            }
            g_settings = g_scene->data[race].race.Get();
            int first = -1;
            for (const int index : g_scene->DisplayOrder())
            {
                if (g_scene->items[index].parent != race)
                {
                    continue;
                }
                if (first < 0 && g_scene->items[index].selected)
                {
                    first = static_cast<int>(g_checkpoints.size());
                }
                g_checkpoints.push_back({ g_scene->world[index].pos, g_scene->data[index].raceCheckpoint.Get() });
            }
            if (first < 0 || g_scene->items[race].selected)
            {
                first = 0;
            }
            if (!g_simulation.Start(static_cast<int>(g_checkpoints.size()), first, g_settings))
            {
                Stop();
                SampAddChatMessage("The selected race needs more checkpoints.", 0xFFFF6060);
                return;
            }
            strcpy_s(g_raceName, g_scene->items[race].name);
            g_startTick = GetTickCount();
            g_startDelay = 2;
        }

    }

    void Stop()
    {
        ClearIcons();
        g_simulation = {};
        g_checkpoints.clear();
        g_visuals = {};
        g_startDelay = 0;
    }

    void Update()
    {
        const bool editing = g_freeCamera && g_freeCamera->m_active;
        if (editing != g_wasEditing)
        {
            g_wasEditing = editing;
            if (editing)
            {
                Stop();
            }
            else
            {
                StartSelection();
            }
        }
        if (editing || !g_simulation.Active())
        {
            return;
        }
        CPed* player = FindPlayerPed();
        if (!player || player->m_fHealth <= 0.0f || !SampIsActive())
        {
            Stop();
            return;
        }
        if (g_startDelay > 0)
        {
            if (--g_startDelay == 0)
            {
                g_startTick = GetTickCount();
                ShowIcons();
                const auto accent = Theme::Argb(Theme::g_accent), text = Theme::Argb(Theme::g_statusText);
                Samp_AddChatMessageF(accent, "[waversomapstudio] {%06X}Started race '%s' (CP %d).", text & 0xFFFFFFu, g_raceName,
                    g_simulation.target + 1);
            }
            return;
        }
        const auto& target = g_checkpoints[g_simulation.target];
        const CVector offset = player->GetPosition() - target.position;
        if (Utils::Dot3(offset, offset) <= target.data.radius * target.data.radius)
        {
            const auto accent = Theme::Argb(Theme::g_accent), text = Theme::Argb(Theme::g_statusText);
            Samp_AddChatMessageF(accent, "[waversomapstudio] {%06X}%s (CP %d): lap %d.", text & 0xFFFFFFu, g_raceName,
                g_simulation.target + 1, g_simulation.lap);
            g_simulation.Advance();
            if (!g_simulation.Active())
            {
                Samp_AddChatMessageF(accent, "[waversomapstudio] {%06X}Finished race '%s' in %.2f seconds.", text & 0xFFFFFFu,
                    g_raceName, (GetTickCount() - g_startTick) / 1000.0f);
                Stop();
            }
            else
            {
                ShowIcons();
            }
        }
    }

#include "races_render.inl"
}
