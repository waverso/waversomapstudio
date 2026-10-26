// Source Repository: https://github.com/waverso/waversomapstudio

#include <climits>
#include <cstddef>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include <windows.h>

#include "../hooks/game_hooks.h"
#include "../rendering/material_text_renderer.h"
#include "object_runtime.h"
#include "../scene/scene.h"

namespace
{
    bool MaterialSizeToWH(int ms, int& w, int& h)
    {
        switch (ms)
        {
            case 10:
                w = 32;
                h = 32;
                return true;
            case 20:
                w = 64;
                h = 32;
                return true;
            case 30:
                w = 64;
                h = 64;
                return true;
            case 40:
                w = 128;
                h = 32;
                return true;
            case 50:
                w = 128;
                h = 64;
                return true;
            case 60:
                w = 128;
                h = 128;
                return true;
            case 70:
                w = 256;
                h = 32;
                return true;
            case 80:
                w = 256;
                h = 64;
                return true;
            case 90:
                w = 256;
                h = 128;
                return true;
            case 100:
                w = 256;
                h = 256;
                return true;
            case 110:
                w = 512;
                h = 64;
                return true;
            case 120:
                w = 512;
                h = 128;
                return true;
            case 130:
                w = 512;
                h = 256;
                return true;
            case 140:
                w = 512;
                h = 512;
                return true;
        }
        return false;
    }

    void Utf8ToAnsi(const char* text, size_t length, std::string& ansi)
    {
        if (!text || length == 0 || length > static_cast<size_t>(INT_MAX))
        {
            return;
        }
        const int sourceLength = static_cast<int>(length);
        const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, sourceLength, nullptr, 0);
        if (wideLength <= 0)
        {
            ansi.assign(text, length);
            return;
        }
        std::vector<wchar_t> wide(static_cast<size_t>(wideLength));
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, sourceLength, wide.data(), wideLength) != wideLength)
        {
            ansi.assign(text, length);
            return;
        }
        const int ansiLength
            = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide.data(), wideLength, nullptr, 0, "?", nullptr);
        if (ansiLength <= 0)
        {
            return;
        }
        ansi.resize(static_cast<size_t>(ansiLength));
        if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide.data(), wideLength, ansi.data(), ansiLength, "?", nullptr)
            != ansiLength)
        {
            ansi.clear();
        }
    }

    unsigned int HashTextParams(const MaterialTextData& d)
    {
        unsigned int hsh = 2166136261u;
        auto mix = [&](const void* p, size_t n)
        {
            const unsigned char* b = (const unsigned char*)p;
            for (size_t i = 0; i < n; ++i)
            {
                hsh ^= b[i];
                hsh *= 16777619u;
            }
        };
        mix(d.text, strnlen(d.text, sizeof(d.text)));
        mix(d.fontFace, strnlen(d.fontFace, sizeof(d.fontFace)));
        mix(&d.materialSize, sizeof(d.materialSize));
        mix(&d.fontSize, sizeof(d.fontSize));
        mix(&d.bold, sizeof(d.bold));
        mix(&d.fontColorArgb, sizeof(d.fontColorArgb));
        mix(&d.backgroundColorArgb, sizeof(d.backgroundColorArgb));
        mix(&d.alignment, sizeof(d.alignment));
        return hsh ? hsh : 1u;
    }
}

RwTexture* ObjectRuntime::BuildMaterialTextTexture(const MaterialTextData& d)
{
    int w = 0, h = 0;
    if (!MaterialSizeToWH(d.materialSize, w, h))
    {
        return nullptr;
    }
    try
    {
        std::string text("", 0);
        Utf8ToAnsi(d.text, strnlen(d.text, sizeof(d.text)), text);
        std::vector<RwUInt32> pixels(static_cast<size_t>(w) * h);
        if (!RenderMaterialText(g_gameHooks ? g_gameHooks->m_device : nullptr, d, w, h, text, pixels))
        {
            return nullptr;
        }
        RwRaster* raster = RwRasterCreate(w, h, 0, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        if (!raster)
        {
            return nullptr;
        }
        RwUInt8* dst = RwRasterLock(raster, 0, rwRASTERLOCKWRITE);
        if (!dst)
        {
            RwRasterDestroy(raster);
            return nullptr;
        }
        for (int y = 0; y < h; ++y)
        {
            memcpy(dst + static_cast<size_t>(y) * raster->stride, pixels.data() + static_cast<size_t>(y) * w,
                static_cast<size_t>(w) * 4);
        }
        RwRasterUnlock(raster);
        RwTexture* result = RwTextureCreate(raster);
        if (result)
        {
            RwTextureSetFilterMode(result, rwFILTERLINEAR);
        }
        else
        {
            RwRasterDestroy(raster);
        }
        return result;
    }
    catch (const std::bad_alloc&)
    {
        return nullptr;
    }
}

void ObjectRuntime::ReleaseItemMaterialTexts(int idx)
{
    for (auto& text : m_itemStates[idx].matTexts)
    {
        m_textCache.Release(text.entry);
        text = MaterialTextState {};
    }
}

void ObjectRuntime::SyncItemMaterialTexts(int idx)
{
    const ItemData& data = g_scene->data[idx];
    const int count = data.MaterialTextCount();
    const ULONGLONG now = GetTickCount64();
    for (int m = 0; m < 16; ++m)
    {
        auto& text = m_itemStates[idx].matTexts[m];
        const auto* params = m < count ? &data.materialTexts[m] : nullptr;
        if (!params || !params->text[0])
        {
            m_textCache.Release(text.entry);
            text = MaterialTextState {};
            continue;
        }
        if (text.entry && MaterialTextCache::Matches(*text.entry, *params))
        {
            continue;
        }
        if (now < text.retryAtMs)
        {
            continue;
        }
        int width = 0, height = 0;
        if (!MaterialSizeToWH(params->materialSize, width, height) || params->fontSize <= 0 || params->fontSize > 4096)
        {
            m_textCache.Release(text.entry);
            text = MaterialTextState {};
            continue;
        }
        bool failed = false;
        auto* entry = m_textCache.Acquire(
            *params, HashTextParams(*params), static_cast<size_t>(width) * height * 4,
            [&]
            {
                return BuildMaterialTextTexture(*params);
            },
            failed);
        if (entry)
        {
            m_textCache.Release(text.entry);
            text.entry = entry;
            text.tex = entry->texture;
            text.retryAtMs = 0;
        }
        else if (failed)
        {
            text.retryAtMs = now + 1000;
        }
    }
}
