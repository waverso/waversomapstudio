// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>
#include <d3d9.h>
#include <unordered_map>
#include <vector>

#include "CVector.h"
#include "CVector2D.h"
#include "IplDef.h"

class CBaseModelInfo;
class CCamera;
class CEntity;
class CObject;
template <typename T>
class CLink;

enum class GizmoHotkeyEvent : unsigned char
{
    Position,
    Rotation,
    Both,
    TogglePivot,
    ToggleSpace,
    None
};

class GameHooks
{
    public:
    GameHooks();
    ~GameHooks();

    bool m_initialized = false;
    IDirect3DDevice9* m_device = nullptr;
    HWND m_window = nullptr;
    std::unordered_map<CEntity*, std::vector<CEntity*>> m_lodHighMap;

    static void SquashCameraTransition();
    bool PopGizmoHotkeyEvent(GizmoHotkeyEvent& event);
    void NotifySyntheticCursorWarp(const POINT& clientPos);

    private:
    struct GizmoHotkeySequence
    {
        UINT vk = 0;
        unsigned long long downSerial = 0;
        bool contextAllowedAtDown = false;
    };

    struct Detour
    {
        PVOID* ppOriginal;
        PVOID pHook;
    };
    std::vector<Detour> m_detours;
    std::vector<GizmoHotkeyEvent> m_gizmoHotkeyEvents;
    GizmoHotkeySequence m_gizmoHotkeySequence;
    unsigned long long m_inputDownSerial = 0;
    std::vector<POINT> m_syntheticCursorWarps;

    void TrackGizmoHotkeyMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void ResetGizmoHotkeyInput();
    bool ConsumeSyntheticCursorWarp(UINT msg, WPARAM wParam, LPARAM lParam);

    using WndProcFn = LRESULT(__stdcall*)(HWND, UINT, WPARAM, LPARAM);
    using PresentFn = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
    using ResetFn = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
    using ProcessCamFn = void(__stdcall*)();
    using EntityRemoveFn = void(__thiscall*)(CEntity*);
    using EntityDtorFn = void(__thiscall*)(CEntity*);
    using EntityRenderFn = void(__thiscall*)(CEntity*);
    using EntityIsOccludedFn = bool(__thiscall*)(CEntity*);
    using VisibilityRenderEntityFn = void(__cdecl*)(void*, float);
    using FindPlayerCoorsFn = CVector(__cdecl*)(int);
    using WorldAddEntityFn = void(__cdecl*)(CEntity*);
    using StreamingAddEntityFn = CLink<CEntity*>*(__cdecl*)(CEntity*);
    using ProcessControlLogicFn = void(__thiscall*)(CObject*);
    using SetupMapEntityVisibilityFn = int32_t(__cdecl*)(CEntity*, CBaseModelInfo*, float, bool);
    using CalculateFadingAtomicAlphaFn = int32_t(__cdecl*)(CBaseModelInfo*, CEntity*, float);
    using ClockGetIsTimeInRangeFn = bool(__cdecl*)(unsigned char, unsigned char);
    using StartTransitionFn = void(__thiscall*)(CCamera*, short);
    using EnsureIplsAreInMemoryFn = void(__cdecl*)(const CVector&);
    using SetIfIplFn = void(__cdecl*)(const CVector2D&, IplDef*);
    using RwCameraBeginUpdateFn = RwCamera*(__cdecl*)(RwCamera*);
    using SpriteCalcScreenCoorsFn = bool(__cdecl*)(const RwV3d&, RwV3d*, float*, float*, bool, bool);
    using RenderListAppendFn = void(__cdecl*)();

    static LRESULT __stdcall HookWndProc(HWND, UINT, WPARAM, LPARAM);
    static HRESULT __stdcall HookPresent(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
    static HRESULT __stdcall HookReset(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
    static void __fastcall HookEntityRemove(CEntity* pEntity, void* edx);
    static void __fastcall HookEntityDtor(CEntity* pEntity, void* edx);
    static void __fastcall HookEntityRender(CEntity* pEntity, void* edx);
    static bool __fastcall HookEntityIsOccluded(CEntity* pEntity, void* edx);
    static void __cdecl HookVisibilityRenderEntity(void* obj, float dist);
    static CVector __cdecl HookFindPlayerCoors(int playerId);
    static void __cdecl HookWorldAddEntity(CEntity* entity);
    static CLink<CEntity*>* __cdecl HookStreamingAddEntity(CEntity* entity);
    static void __fastcall HookProcessControlLogic(CObject* obj, void* edx);
    static int32_t __cdecl HookSetupMapEntityVisibility(
        CEntity* entity, CBaseModelInfo* baseModelInfo, float fDistance, bool bIsTimeInRange);
    static int32_t __cdecl HookCalculateFadingAtomicAlpha(CBaseModelInfo* modelInfo, CEntity* entity, float distance);
    static bool __cdecl HookClockGetIsTimeInRange(unsigned char from, unsigned char to);
    static void __fastcall HookStartTransition(CCamera* cam, void* edx, short newMode);
    static void __cdecl HookEnsureIplsAreInMemory(const CVector& posn);
    static void __cdecl HookSetIfIplIsRequired(const CVector2D& posn, IplDef* def);
    static void __cdecl HookSetIfIplIsRequiredReducedBb(const CVector2D& posn, IplDef* def);
    static void __cdecl HookSetIfInteriorIplIsRequired(const CVector2D& posn, IplDef* def);
    static RwCamera* __cdecl HookRwCameraBeginUpdate(RwCamera* camera);
    static bool __cdecl HookSpriteCalcScreenCoors(
        const RwV3d& posn, RwV3d* out, float* width, float* height, bool checkMaxVisible, bool checkMinVisible);

    WndProcFn m_originalWndProc = nullptr;
    PresentFn m_originalPresent = nullptr;
    ResetFn m_originalReset = nullptr;
    ProcessCamFn m_originalProcessCamera = nullptr;
    EntityRemoveFn m_originalEntityRemove = nullptr;
    EntityDtorFn m_originalEntityDtor = nullptr;
    EntityRenderFn m_originalEntityRender = nullptr;
    EntityIsOccludedFn m_originalEntityIsOccluded = nullptr;
    VisibilityRenderEntityFn m_originalVisibilityRenderEntity = nullptr;
    FindPlayerCoorsFn m_originalFindPlayerCoors = nullptr;
    WorldAddEntityFn m_originalWorldAddEntity = nullptr;
    StreamingAddEntityFn m_originalStreamingAddEntity = nullptr;
    ProcessControlLogicFn m_originalProcessControlLogic = nullptr;
    SetupMapEntityVisibilityFn m_originalSetupMapEntityVisibility = nullptr;
    CalculateFadingAtomicAlphaFn m_originalCalculateFadingAtomicAlpha = nullptr;
    ClockGetIsTimeInRangeFn m_originalClockGetIsTimeInRange = nullptr;
    StartTransitionFn m_originalStartTransition = nullptr;
    EnsureIplsAreInMemoryFn m_originalEnsureIplsAreInMemory = nullptr;
    SetIfIplFn m_originalSetIfIplIsRequired = nullptr;
    SetIfIplFn m_originalSetIfIplIsRequiredReducedBb = nullptr;
    SetIfIplFn m_originalSetIfInteriorIplIsRequired = nullptr;
    RwCameraBeginUpdateFn m_originalRwCameraBeginUpdate = nullptr;
    SpriteCalcScreenCoorsFn m_originalSpriteCalcScreenCoors = nullptr;
    RenderListAppendFn m_originalAppendVisibleEntity = (RenderListAppendFn)0x553521;
    RenderListAppendFn m_originalAppendVisibleLod = (RenderListAppendFn)0x5534ED;
};

extern GameHooks* g_gameHooks;
extern bool g_renderInteriors;
