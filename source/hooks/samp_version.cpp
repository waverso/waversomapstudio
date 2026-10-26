// Source Repository: https://github.com/waverso/waversomapstudio

#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <wincrypt.h>

#include "../utils/utils.h"
#include "samp.h"

#pragma comment(lib, "advapi32.lib")

static bool FileMd5Hex(const char* path, char outHex[33])
{
    HANDLE f = CreateFileA(
        path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    bool ok = false;
    if (CryptAcquireContextA(&prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if (CryptCreateHash(prov, CALG_MD5, 0, 0, &hash))
        {
            BYTE buf[8192];
            DWORD rd = 0;
            ok = true;
            while (ReadFile(f, buf, sizeof(buf), &rd, nullptr) && rd > 0)
            {
                if (!CryptHashData(hash, buf, rd, 0))
                {
                    ok = false;
                    break;
                }
            }

            BYTE digest[16];
            DWORD dl = sizeof(digest);
            if (ok && CryptGetHashParam(hash, HP_HASHVAL, digest, &dl, 0) && dl == 16)
            {
                static const char* hx = "0123456789abcdef";
                for (int i = 0; i < 16; ++i)
                {
                    outHex[i * 2] = hx[digest[i] >> 4];
                    outHex[i * 2 + 1] = hx[digest[i] & 0xF];
                }
                outHex[32] = 0;
            }
            else
            {
                ok = false;
            }

            CryptDestroyHash(hash);
        }
        CryptReleaseContext(prov, 0);
    }

    CloseHandle(f);
    return ok;
}

bool SampIntegration::ResolveVersion()
{
    char path[MAX_PATH];
    if (!GetModuleFileNameA((HMODULE)m_base, path, MAX_PATH))
    {
        return false;
    }

    char md5[33];
    if (!FileMd5Hex(path, md5))
    {
        return false;
    }

    struct SampVersionEntry
    {
        const char* md5;
        SampVersion ver;
        uint32_t info;
        uint32_t pools;
        SampUi ui;
        SampFunctions fn;
        SampPools pl;
        SampOverlay overlay;
    };
    static const SampVersionEntry kSupported[] = {
        { "1d22eaa2605717ddf215f68e861de378", SampVersion::V037R1, 0x21A0F8, 0x3CD,
            { 0x21A0E4, 0x21A0B4, 0x21A0E8, 0x21A0B8, 0x21A0EC },
            { 0x21A10C, 0x9BD30, 0x658E0, 0x6B210, 0x6AD30, 0x797B, 0x55, 0x645A0, 0x3EE0, 0xA57A0, 0x67450 },
            { 0x04, 0xFA4, 1000 }, { 0x675B0, 0x686C0, 0x689C0, 0x12C980 } },
        { "5ba5f0be7af99dfd03fb39e88a970a2b", SampVersion::V037R5, 0x26EB94, 0x3DE,
            { 0x26EB80, 0x26EB4C, 0x26EB84, 0x26EB50, 0x26EB88 },
            { 0x26EBAC, 0xA06F0, 0x69580, 0x6F860, 0x6F3D0, 0x7CF7, 0x61, 0x68170, 0x4080, 0x89B0, 0x6BB30 },
            { 0x0C, 0xFA4, 1000 }, { 0x6BC90, 0x6CDA0, 0x6D0A0, 0x140DB0 } },
        { "449e4f985215ffb5bffadf23551c0d50", SampVersion::V03DLR1, 0x2ACA24, 0x3DE,
            { 0x2ACA10, 0x2AC9DC, 0x2ACA14, 0x2AC9E0, 0x2ACA18 },
            { 0x2ACA3C, 0xA0530, 0x68FC0, 0x6F2A0, 0x6EE10, 0x7A01, 0x61, 0x67BE0, 0x3F60, 0x86A0, 0x6B570 },
            { 0x14, 0x20D4, 2100 }, { 0x6B6D0, 0x6C7E0, 0x6CAE0, 0x17EC40 } },
    };

    m_versionResolved = true;
    for (const SampVersionEntry& r : kSupported)
    {
        if (_stricmp(md5, r.md5) == 0)
        {
            m_ver = r.ver;
            m_infoOff = r.info;
            m_poolsOff = r.pools;
            m_ui = r.ui;
            m_fn = r.fn;
            m_pools = r.pl;
            m_overlay = r.overlay;
            return true;
        }
    }

    Utils::FatalExit("Unsupported SA-MP client version.\nSupported: 0.3.7-R1, 0.3.7-R5, 0.3.DL.");
    return false;
}
