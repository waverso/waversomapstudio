// Source Repository: https://github.com/waverso/waversomapstudio

#include "samp.h"

#include <Windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>

#include "../utils/utils.h"
#include "CEntity.h"
#include "CObject.h"
#include "common.h"
#include "../scene/free_camera.h"
#include "plugin.h"
#include "../ui/theme.h"

static bool SafePtrChain(uintptr_t base, uint32_t infoOff, uint32_t poolsOff)
{
    __try
    {
        void* info = *(void**)(base + infoOff);
        if (!info)
        {
            return false;
        }
        void* pools = *(void**)((uintptr_t)info + poolsOff);
        return pools != nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void* SampInst(uintptr_t base, uint32_t rva)
{
    __try
    {
        return *(void**)(base + rva);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static bool SampReadI(void* inst, uint32_t off, int& out)
{
    if (!inst)
    {
        return false;
    }
    __try
    {
        out = *(int*)((char*)inst + off);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void SampWriteI(void* inst, uint32_t off, int v)
{
    if (!inst)
    {
        return;
    }
    __try
    {
        *(int*)((char*)inst + off) = v;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

static void SampCallThis(uintptr_t fnAddr, void* obj)
{
    if (!obj)
    {
        return;
    }
    __try
    {
        ((void(__thiscall*)(void*))fnAddr)(obj);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

static void SampCursorOff(uintptr_t base, const SampFunctions& fn)
{
    void* game = SampInst(base, fn.refGame);
    if (!game)
    {
        return;
    }
    __try
    {
        ((void(__thiscall*)(void*, int, int))(base + fn.setCursorMode))(game, 0, 1);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

SampIntegration::~SampIntegration()
{
    RemoveOverlayHooks();
}

bool SampIntegration::IsChatInputActive() const
{
    if (!m_initialized)
    {
        return false;
    }
    int v;
    return SampReadI(SampInst(m_base, m_ui.input), 0x14E0, v) && v == 1;
}

bool SampIntegration::IsDialogActive() const
{
    if (!m_initialized)
    {
        return false;
    }
    int v;
    return SampReadI(SampInst(m_base, m_ui.dialog), 0x28, v) && v == 1;
}

bool SampIntegration::IsCursorVisible() const
{
    if (!m_initialized)
    {
        return false;
    }
    int v;
    return SampReadI(SampInst(m_base, m_fn.refGame), m_fn.cursorMode, v) && v != 0;
}

void SampIntegration::AddChatMessage(const char* text, D3DCOLOR color) const
{
    if (!m_initialized || !text || !m_fn.addChatMessage)
    {
        return;
    }
    void* chat = SampInst(m_base, m_ui.chat);
    if (!chat)
    {
        return;
    }
    __try
    {
        ((void(__thiscall*)(void*, D3DCOLOR, const char*))(m_base + m_fn.addChatMessage))(
            chat, (color << 8) | (color >> 24), text);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void SampIntegration::AddChatMessageF(D3DCOLOR color, const char* fmt, ...) const
{
    if (!m_initialized || !fmt || !m_fn.addChatMessage)
    {
        return;
    }

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    buf[sizeof(buf) - 1] = '\0';

    AddChatMessage(buf, color);
}

static int SampWalkObjectPool(uintptr_t base, uint32_t infoOff, uint32_t poolsOff, const SampPools& pl, void** out, int cap)
{
    __try
    {
        void* info = *(void**)(base + infoOff);
        if (!info)
        {
            return 0;
        }
        void* pools = *(void**)((char*)info + poolsOff);
        if (!pools)
        {
            return 0;
        }
        void* pool = *(void**)((char*)pools + pl.pObjectOff);
        if (!pool)
        {
            return 0;
        }

        const int* used = (const int*)((char*)pool + 0x04);
        void** arr = (void**)((char*)pool + pl.objArrayOff);

        int n = 0;
        for (int i = 0; i < pl.maxObjects && n < cap; ++i)
        {
            if (!used[i] || !arr[i])
            {
                continue;
            }
            void* game = *(void**)((char*)arr[i] + 0x40);
            if (game)
            {
                out[n++] = game;
            }
        }
        return n;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

void SampIntegration::ReconcileObjects() const
{
    m_lastReconcileFrame = m_frame;

    void* buf[2100];
    int n = SampWalkObjectPool(m_base, m_infoOff, m_poolsOff, m_pools, buf, 2100);
    if (n < 0)
    {
        return;
    }

    m_serverObjs.clear();
    for (int i = 0; i < n; ++i)
    {
        m_serverObjs.insert((CObject*)buf[i]);
    }
    m_objsDirty = false;
}

void SampIntegration::OnObjectAdded(CObject*)
{
    if (m_initialized)
    {
        m_objsDirty = true;
    }
}

void SampIntegration::OnEntityRemove(CEntity* ent)
{
    if (!m_initialized || !ent || ent->m_nType != ENTITY_TYPE_OBJECT)
    {
        return;
    }
    m_serverObjs.erase((CObject*)ent);
}

bool SampIntegration::IsServerObject(CObject* obj) const
{
    if (!m_initialized || !obj)
    {
        return false;
    }
    if (m_objsDirty && m_frame != m_lastReconcileFrame)
    {
        ReconcileObjects();
    }
    return m_serverObjs.find(obj) != m_serverObjs.end();
}

bool SampIntegration::IsLocalPlayerAccessory(const CEntity* entity) const
{
    if (!m_initialized || !entity)
    {
        return false;
    }
    const auto* player = FindPlayerPed();
    if (!player)
    {
        return false;
    }
    __try
    {
        const auto game = *reinterpret_cast<std::uintptr_t*>(m_base + m_fn.refGame);
        if (!game)
        {
            return false;
        }
        const auto ped = *reinterpret_cast<std::uintptr_t*>(game + 0x08);
        if (!ped || *reinterpret_cast<CEntity**>(ped + 0x2A4) != player)
        {
            return false;
        }
        for (unsigned slot = 0; slot < 10; ++slot)
        {
            if (!*reinterpret_cast<int*>(ped + 0x4C + slot * 4))
            {
                continue;
            }
            const auto object = *reinterpret_cast<std::uintptr_t*>(ped + 0x27C + slot * 4);
            if (object && *reinterpret_cast<CEntity**>(object + 0x40) == entity)
            {
                return true;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return false;
}

void SampIntegration::EnterEdit()
{
    void* chat = SampInst(m_base, m_ui.chat);
    int mode;
    if (SampReadI(chat, 0x08, mode) && mode >= 0 && mode <= 2)
    {
        m_savedChatMode = mode;
        SampWriteI(chat, 0x08, 0);
    }

    void* death = SampInst(m_base, m_ui.death);
    int en;
    if (SampReadI(death, 0x00, en) && en >= 0 && en <= 1)
    {
        m_savedKill = en;
        SampWriteI(death, 0x00, 0);
    }

    void* inp = SampInst(m_base, m_ui.input);
    int iv;
    if (SampReadI(inp, 0x14E0, iv) && iv == 1)
    {
        SampCallThis(m_base + m_fn.inputClose, inp);
    }

    void* dlg = SampInst(m_base, m_ui.dialog);
    int dv;
    if (SampReadI(dlg, 0x28, dv) && dv == 1)
    {
        SampCallThis(m_base + m_fn.dialogHide, dlg);
    }

    void* sb = SampInst(m_base, m_ui.scoreboard);
    int sv;
    if (SampReadI(sb, 0x00, sv) && sv == 1)
    {
        SampWriteI(sb, 0x00, 0);
    }

    if (!m_sbEnablePatched && m_fn.scoreboardEnable)
    {
        plugin::patch::GetRaw(m_base + m_fn.scoreboardEnable, &m_sbEnableSaved, 1);
        uint8_t ret = 0xC3;
        plugin::patch::SetRaw(m_base + m_fn.scoreboardEnable, &ret, 1);
        FlushInstructionCache(GetCurrentProcess(), (void*)(m_base + m_fn.scoreboardEnable), 1);
        m_sbEnablePatched = true;
    }

    if (!m_helpPatched && m_fn.helpDialog)
    {
        plugin::patch::GetRaw(m_base + m_fn.helpDialog, &m_helpSaved, 1);
        uint8_t ret = 0xC3;
        plugin::patch::SetRaw(m_base + m_fn.helpDialog, &ret, 1);
        FlushInstructionCache(GetCurrentProcess(), (void*)(m_base + m_fn.helpDialog), 1);
        m_helpPatched = true;
    }

    if (!m_prepClassPatched && m_fn.prepClassSel)
    {
        plugin::patch::GetRaw(m_base + m_fn.prepClassSel, &m_prepClassSaved, 1);
        uint8_t ret = 0xC3;
        plugin::patch::SetRaw(m_base + m_fn.prepClassSel, &ret, 1);
        FlushInstructionCache(GetCurrentProcess(), (void*)(m_base + m_fn.prepClassSel), 1);
        m_prepClassPatched = true;
    }

    if (!m_lobbyPatched && m_fn.processLobby)
    {
        plugin::patch::GetRaw(m_base + m_fn.processLobby, &m_lobbySaved, 1);
        uint8_t ret = 0xC3;
        plugin::patch::SetRaw(m_base + m_fn.processLobby, &ret, 1);
        FlushInstructionCache(GetCurrentProcess(), (void*)(m_base + m_fn.processLobby), 1);
        m_lobbyPatched = true;
    }

    if (!m_r1WeatherPatched && m_ver == SampVersion::V037R1)
    {
        const uintptr_t addr = m_base + 0x9C130;
        plugin::patch::GetRaw(addr, m_r1WeatherSaved, sizeof(m_r1WeatherSaved));

        static const uint8_t ret4[3] = { 0xC2, 0x04, 0x00 };
        plugin::patch::SetRaw(addr, (void*)ret4, sizeof(ret4));
        FlushInstructionCache(GetCurrentProcess(), (void*)addr, sizeof(ret4));
        m_r1WeatherPatched = true;
    }

    SampCursorOff(m_base, m_fn);
}

bool SampIntegration::ExitEdit()
{
    if (m_helpPatched)
    {
        plugin::patch::SetRaw(m_base + m_fn.helpDialog, &m_helpSaved, 1);
        FlushInstructionCache(GetCurrentProcess(), (void*)(m_base + m_fn.helpDialog), 1);
        m_helpPatched = false;
    }

    if (m_sbEnablePatched)
    {
        plugin::patch::SetRaw(m_base + m_fn.scoreboardEnable, &m_sbEnableSaved, 1);
        FlushInstructionCache(GetCurrentProcess(), (void*)(m_base + m_fn.scoreboardEnable), 1);
        m_sbEnablePatched = false;
    }

    if (m_prepClassPatched)
    {
        plugin::patch::SetRaw(m_base + m_fn.prepClassSel, &m_prepClassSaved, 1);
        FlushInstructionCache(GetCurrentProcess(), (void*)(m_base + m_fn.prepClassSel), 1);
        m_prepClassPatched = false;
    }

    if (m_lobbyPatched)
    {
        plugin::patch::SetRaw(m_base + m_fn.processLobby, &m_lobbySaved, 1);
        FlushInstructionCache(GetCurrentProcess(), (void*)(m_base + m_fn.processLobby), 1);
        m_lobbyPatched = false;
    }

    if (m_r1WeatherPatched)
    {
        const uintptr_t addr = m_base + 0x9C130;
        plugin::patch::SetRaw(addr, m_r1WeatherSaved, sizeof(m_r1WeatherSaved));
        FlushInstructionCache(GetCurrentProcess(), (void*)addr, sizeof(m_r1WeatherSaved));
        m_r1WeatherPatched = false;
    }

    bool ok = true;
    if (m_savedChatMode >= 0)
    {
        void* chat = SampInst(m_base, m_ui.chat);
        if (chat)
        {
            SampWriteI(chat, 0x08, m_savedChatMode);
            m_savedChatMode = -1;
        }
        else
        {
            ok = false;
        }
    }
    if (m_savedKill >= 0)
    {
        void* death = SampInst(m_base, m_ui.death);
        if (death)
        {
            SampWriteI(death, 0x00, m_savedKill);
            m_savedKill = -1;
        }
        else
        {
            ok = false;
        }
    }
    return ok;
}

void SampIntegration::Update()
{
    ++m_frame;

    if (!m_initialized)
    {
        HMODULE h = GetModuleHandleA("samp.dll");
        if (!h)
        {
            return;
        }
        m_base = (uintptr_t)h;

        if (!m_versionResolved && !ResolveVersion())
        {
            return;
        }
        if (!m_overlayHooksInstalled && !InstallOverlayHooks())
        {
            Utils::FatalExit("Failed to install overlay projection hooks.");
        }
        if (!SafePtrChain(m_base, m_infoOff, m_poolsOff) || !SampInst(m_base, m_ui.chat))
        {
            return;
        }
        m_initialized = true;

        if (m_fn.f4Disable)
        {
            uint8_t jump = 0xEB;
            plugin::patch::SetRaw(m_base + m_fn.f4Disable, &jump, 1);
            FlushInstructionCache(GetCurrentProcess(), (void*)(m_base + m_fn.f4Disable), 1);
        }
        const auto accent = Theme::Argb(Theme::g_accent), text = Theme::Argb(Theme::g_statusText);
        AddChatMessageF(accent, "[waversomapstudio] {%06X}Loaded. Press {%06X}F4 {%06X}to open the editor.", text & 0xFFFFFFu,
            accent & 0xFFFFFFu, text & 0xFFFFFFu);
    }

    const bool active = g_freeCamera && g_freeCamera->m_active;

    if (active && !m_editActive)
    {
        EnterEdit();
        m_editActive = true;
    }
    else if (!active && m_editActive)
    {
        if (ExitEdit())
        {
            m_editActive = false;
        }
    }
}
