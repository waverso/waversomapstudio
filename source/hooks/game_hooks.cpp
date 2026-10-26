// Source Repository: https://github.com/waverso/waversomapstudio

#include "game_hooks.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../utils/utils.h"
#include "../vendor/detours/detours.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_impl_dx9.h"
#include "../vendor/imgui/imgui_impl_win32.h"
#include "CBaseModelInfo.h"
#include "CCamera.h"
#include "CMenuManager.h"
#include "CEntity.h"
#include "CFire.h"
#include "CIplStore.h"
#include "CLink.h"
#include "CObject.h"
#include "CPed.h"
#include "CPedIntelligence.h"
#include "CPhysical.h"
#include "CPickup.h"
#include "CPlayerPed.h"
#include "CRenderer.h"
#include "CStreaming.h"
#include "CTaskSimpleJetPack.h"
#include "CTimer.h"
#include "CVehicle.h"
#include "Fx_c.h"
#include "../rendering/collision_renderer.h"
#include "common.h"
#include "../catalog/database.h"
#include "../scene/free_camera.h"
#include "../scene/box_area.h"
#include "../scene/gizmos.h"
#include "../ui/gui.h"
#include "../rendering/model_preview.h"
#include "../objects/object_runtime.h"
#include "../rendering/outline_renderer.h"
#include "../rendering/particle_render_filter.h"
#include "../scene/picker.h"
#include "../pickups/pickup_runtime.h"
#include "plugin.h"
#include "../objects/removal_manager.h"
#include "samp.h"
#include "../scene/scene.h"
#include "../scene/selection.h"
#include "../vehicles/vehicle_runtime.h"

#include "game_hooks_player_visibility.inl"
#include "game_hooks_entities.inl"
#include "game_hooks_world.inl"
#include "game_hooks_render_lists.inl"
#include "game_hooks_render.inl"
#include "game_hooks_input.inl"

GameHooks::GameHooks()
{
    IDirect3DDevice9* device = *(IDirect3DDevice9**)0xC97C28;
    if (!device || !RsGlobal.ps || !RsGlobal.ps->window)
    {
        return;
    }

    if (!ValidateRenderLists())
    {
        Utils::FatalExit("Cannot expand renderer lists.");
    }

    m_device = device;
    m_window = RsGlobal.ps->window;

    void** vTable = *(void***)device;
    m_originalPresent = (PresentFn)vTable[17];
    m_originalReset = (ResetFn)vTable[16];
    m_originalProcessCamera = (ProcessCamFn)0x52BA06;
    m_originalEntityRemove = (EntityRemoveFn)0x534AE0;
    m_originalEntityDtor = (EntityDtorFn)0x535E90;
    m_originalEntityRender = (EntityRenderFn)0x534310;
    m_originalEntityIsOccluded = (EntityIsOccludedFn)0x71FAE0;
    m_originalVisibilityRenderEntity = (VisibilityRenderEntityFn)0x732B40;
    m_originalFindPlayerCoors = (FindPlayerCoorsFn)0x56E010;
    m_originalWorldAddEntity = (WorldAddEntityFn)0x563220;
    m_originalStreamingAddEntity = (StreamingAddEntityFn)0x409650;
    m_originalProcessControlLogic = (ProcessControlLogicFn)0x5A29A0;
    m_originalSetupMapEntityVisibility = (SetupMapEntityVisibilityFn)0x553F60;
    m_originalCalculateFadingAtomicAlpha = (CalculateFadingAtomicAlphaFn)0x732500;
    m_originalClockGetIsTimeInRange = (ClockGetIsTimeInRangeFn)0x52CEE0;
    m_originalStartTransition = (StartTransitionFn)0x515200;
    m_originalEnsureIplsAreInMemory = (EnsureIplsAreInMemoryFn)0x4053F0;
    m_originalSetIfIplIsRequired = (SetIfIplFn)0x404660;
    m_originalSetIfIplIsRequiredReducedBb = (SetIfIplFn)0x404690;
    m_originalSetIfInteriorIplIsRequired = (SetIfIplFn)0x4045F0;
    m_originalRwCameraBeginUpdate = (RwCameraBeginUpdateFn)0x7EE190;
    m_originalSpriteCalcScreenCoors = (SpriteCalcScreenCoorsFn)0x70CE30;

    m_detours = {
        { &(PVOID&)g_originalFxRender, (PVOID)EditorFxRender },
        { &(PVOID&)g_originalFxSystemUpdate, (PVOID)EditorFxSystemUpdate },
        { &(PVOID&)g_originalDrawRadarMap, (PVOID)EditorDrawRadarMap },
        { &(PVOID&)g_originalDrawRadarBlips, (PVOID)EditorDrawRadarBlips },
        { &(PVOID&)g_originalJetPackRender, (PVOID)EditorJetPackRender },
        { &(PVOID&)g_originalPickupUpdate, (PVOID)EditorPickupUpdate },
        { &(PVOID&)g_originalVehicleHealthy, (PVOID)EditorVehicleHealthy },
        { &(PVOID&)g_originalCameraCheckPeds, (PVOID)EditorCameraCheckPeds },
        { &(PVOID&)m_originalPresent, (PVOID)HookPresent },
        { &(PVOID&)m_originalReset, (PVOID)HookReset },
        { &(PVOID&)m_originalProcessCamera, (PVOID)HookProcessCamera },
        { &(PVOID&)m_originalEntityRemove, (PVOID)HookEntityRemove },
        { &(PVOID&)m_originalEntityDtor, (PVOID)HookEntityDtor },
        { &(PVOID&)m_originalEntityRender, (PVOID)HookEntityRender },
        { &(PVOID&)m_originalEntityIsOccluded, (PVOID)HookEntityIsOccluded },
        { &(PVOID&)m_originalVisibilityRenderEntity, (PVOID)HookVisibilityRenderEntity },
        { &(PVOID&)m_originalFindPlayerCoors, (PVOID)HookFindPlayerCoors },
        { &(PVOID&)m_originalWorldAddEntity, (PVOID)HookWorldAddEntity },
        { &(PVOID&)m_originalStreamingAddEntity, (PVOID)HookStreamingAddEntity },
        { &(PVOID&)m_originalProcessControlLogic, (PVOID)HookProcessControlLogic },
        { &(PVOID&)m_originalSetupMapEntityVisibility, (PVOID)HookSetupMapEntityVisibility },
        { &(PVOID&)m_originalCalculateFadingAtomicAlpha, (PVOID)HookCalculateFadingAtomicAlpha },
        { &(PVOID&)m_originalClockGetIsTimeInRange, (PVOID)HookClockGetIsTimeInRange },
        { &(PVOID&)m_originalStartTransition, (PVOID)HookStartTransition },
        { &(PVOID&)m_originalEnsureIplsAreInMemory, (PVOID)HookEnsureIplsAreInMemory },
        { &(PVOID&)m_originalSetIfIplIsRequired, (PVOID)HookSetIfIplIsRequired },
        { &(PVOID&)m_originalSetIfIplIsRequiredReducedBb, (PVOID)HookSetIfIplIsRequiredReducedBb },
        { &(PVOID&)m_originalSetIfInteriorIplIsRequired, (PVOID)HookSetIfInteriorIplIsRequired },
        { &(PVOID&)m_originalRwCameraBeginUpdate, (PVOID)HookRwCameraBeginUpdate },
        { &(PVOID&)m_originalSpriteCalcScreenCoors, (PVOID)HookSpriteCalcScreenCoors },
        { &(PVOID&)m_originalAppendVisibleEntity, (PVOID)HookAppendVisibleEntity },
        { &(PVOID&)m_originalAppendVisibleLod, (PVOID)HookAppendVisibleLod },
    };

    m_originalWndProc = (WndProcFn)SetWindowLongPtr(m_window, GWLP_WNDPROC, (LONG_PTR)HookWndProc);

    LONG result = DetourTransactionBegin();
    if (result == NO_ERROR)
    {
        result = DetourUpdateThread(GetCurrentThread());
    }
    for (const Detour& d : m_detours)
    {
        if (result == NO_ERROR)
        {
            result = DetourAttach(d.ppOriginal, d.pHook);
        }
    }
    if (result == NO_ERROR)
    {
        result = DetourTransactionCommit();
    }
    else
    {
        DetourTransactionAbort();
    }

    if (result != NO_ERROR)
    {
        SetWindowLongPtr(m_window, GWLP_WNDPROC, (LONG_PTR)m_originalWndProc);
        m_window = nullptr;
        m_originalWndProc = nullptr;
        m_detours.clear();
        Utils::FatalExit("Failed to install hooks.");
    }

    SetRenderLists(true);
    m_initialized = true;
}

GameHooks::~GameHooks()
{
    if (!m_initialized)
    {
        return;
    }
    m_initialized = false;

    if (m_originalWndProc && m_window)
    {
        SetWindowLongPtr(m_window, GWLP_WNDPROC, (LONG_PTR)m_originalWndProc);
    }

    LONG result = DetourTransactionBegin();
    if (result == NO_ERROR)
    {
        result = DetourUpdateThread(GetCurrentThread());
    }
    for (const Detour& d : m_detours)
    {
        if (result == NO_ERROR)
        {
            result = DetourDetach(d.ppOriginal, d.pHook);
        }
    }
    if (result == NO_ERROR)
    {
        result = DetourTransactionCommit();
    }
    else
    {
        DetourTransactionAbort();
    }
    if (result != NO_ERROR)
    {
        Utils::FatalExit("Failed to remove hooks.");
    }
    SetRenderLists(false);
}
