// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>
#include <d3d9.h>
#include <unordered_set>
#include <utility>

class CObject;
class CEntity;
struct CVector;

enum class SampVersion
{
    None,
    V037R1,
    V037R5,
    V03DLR1
};

struct SampUi
{
    uint32_t chat, scoreboard, input, dialog, death;
};
struct SampFunctions
{
    uint32_t refGame, setCursorMode, inputClose, dialogHide, scoreboardEnable, f4Disable, cursorMode, addChatMessage,
        prepClassSel, processLobby, helpDialog;
};
struct SampPools
{
    uint16_t pObjectOff, objArrayOff, maxObjects;
};
struct SampOverlay
{
    uint32_t labelDraw, playerTagLabel, playerTagHealth, projection;
};

class SampIntegration
{
    public:
    ~SampIntegration();
    void Update();

    bool IsActive() const
    {
        return m_initialized;
    }
    SampVersion Version() const
    {
        return m_ver;
    }

    bool IsChatInputActive() const;
    bool IsDialogActive() const;
    bool IsCursorVisible() const;

    void AddChatMessage(const char* text, D3DCOLOR color = 0xFFFFFFFF) const;
    void AddChatMessageF(D3DCOLOR color, const char* fmt, ...) const;

    void OnObjectAdded(CObject* obj);
    void OnEntityRemove(CEntity* ent);
    bool IsServerObject(CObject* obj) const;
    bool IsLocalPlayerAccessory(const CEntity* entity) const;

    private:
    bool ResolveVersion();
    bool InstallOverlayHooks();
    void RemoveOverlayHooks();
    bool BeginOverlayProjectionShift();
    void EndOverlayProjectionShift();
    void EnterEdit();
    bool ExitEdit();
    void ReconcileObjects() const;

    uintptr_t m_base = 0;
    SampVersion m_ver = SampVersion::None;
    uint32_t m_infoOff = 0;
    uint32_t m_poolsOff = 0;
    SampUi m_ui = {};
    SampFunctions m_fn = {};
    SampPools m_pools = {};
    SampOverlay m_overlay = {};
    int m_savedChatMode = -1;
    int m_savedKill = -1;
    uint8_t m_sbEnableSaved = 0;
    bool m_sbEnablePatched = false;
    uint8_t m_helpSaved = 0;
    bool m_helpPatched = false;
    uint8_t m_prepClassSaved = 0;
    bool m_prepClassPatched = false;
    uint8_t m_lobbySaved = 0;
    bool m_lobbyPatched = false;
    uint8_t m_r1WeatherSaved[3] = { 0 };
    bool m_r1WeatherPatched = false;
    bool m_versionResolved = false;
    bool m_initialized = false;
    bool m_editActive = false;
    bool m_overlayHooksInstalled = false;
    unsigned int m_overlayShiftDepth = 0;
    D3DMATRIX m_overlayProjectionSaved = {};

    using LabelDrawFn = void(__thiscall*)(void*, CVector*, const char*, D3DCOLOR, BOOL, bool);
    using PlayerTagLabelFn = void(__thiscall*)(void*, CVector*, const char*, D3DCOLOR, float, bool, int);
    using PlayerTagHealthFn = void(__thiscall*)(void*, CVector*, float, float, float);
    LabelDrawFn m_originalLabelDraw = nullptr;
    PlayerTagLabelFn m_originalPlayerTagLabel = nullptr;
    PlayerTagHealthFn m_originalPlayerTagHealth = nullptr;

    static void __fastcall HookLabelDraw(
        void* self, void* edx, CVector* position, const char* text, D3DCOLOR color, BOOL shadow, bool noObstacles);
    static void __fastcall HookPlayerTagLabel(
        void* self, void* edx, CVector* position, const char* text, D3DCOLOR color, float distance, bool drawStatus, int status);
    static void __fastcall HookPlayerTagHealth(
        void* self, void* edx, CVector* position, float health, float armour, float distance);

    unsigned int m_frame = 0;
    mutable std::unordered_set<CObject*> m_serverObjs;
    mutable bool m_objsDirty = true;
    mutable unsigned int m_lastReconcileFrame = 0xFFFFFFFFu;
};

extern SampIntegration* g_sampIntegration;

inline bool SampIsActive()
{
    return g_sampIntegration && g_sampIntegration->IsActive();
}

inline bool SampTextInputActive()
{
    return g_sampIntegration && (g_sampIntegration->IsChatInputActive() || g_sampIntegration->IsDialogActive());
}

inline bool SampCursorVisible()
{
    return g_sampIntegration && g_sampIntegration->IsCursorVisible();
}

inline bool SampIsServerObject(CObject* obj)
{
    return g_sampIntegration && g_sampIntegration->IsActive() && g_sampIntegration->IsServerObject(obj);
}

inline void SampAddChatMessage(const char* text, D3DCOLOR color = 0xFFFFFFFF)
{
    if (g_sampIntegration && g_sampIntegration->IsActive())
    {
        g_sampIntegration->AddChatMessage(text, color);
    }
}

template <typename... Args>
inline void Samp_AddChatMessageF(D3DCOLOR color, const char* fmt, Args&&... args)
{
    if (g_sampIntegration && g_sampIntegration->IsActive())
    {
        g_sampIntegration->AddChatMessageF(color, fmt, std::forward<Args>(args)...);
    }
}
