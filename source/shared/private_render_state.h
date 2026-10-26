// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <array>
#include <d3d9.h>
#include <rw/rpuvanim.h>
#include <rw/rpworld.h>

#include "private_render_cache.h"

namespace SharedPrivateRender
{
    constexpr std::array<RwRenderState, 12> SafeRwStates { rwRENDERSTATETEXTURERASTER, rwRENDERSTATESRCBLEND,
        rwRENDERSTATEDESTBLEND, rwRENDERSTATEFOGENABLE, rwRENDERSTATECULLMODE, rwRENDERSTATEZTESTENABLE,
        rwRENDERSTATEZWRITEENABLE, rwRENDERSTATESHADEMODE, rwRENDERSTATEVERTEXALPHAENABLE, rwRENDERSTATETEXTUREADDRESS,
        rwRENDERSTATETEXTUREFILTER, rwRENDERSTATEALPHATESTFUNCTIONREF };

    class RenderWareStateGuard
    {
        public:
        RenderWareStateGuard()
        {
            for (std::size_t i = 0; i < SafeRwStates.size(); ++i)
            {
                valid[i] = RwRenderStateGet(SafeRwStates[i], &values[i]) != FALSE;
                if (valid[i])
                {
                    ++validCount;
                }
            }
        }
        ~RenderWareStateGuard()
        {
            for (std::size_t i = 0; i < SafeRwStates.size(); ++i)
            {
                if (valid[i])
                {
                    RwRenderStateSet(SafeRwStates[i], values[i]);
                }
            }
        }
        std::size_t ValidCount() const
        {
            return validCount;
        }

        private:
        std::array<void*, SafeRwStates.size()> values {};
        std::array<bool, SafeRwStates.size()> valid {};
        std::size_t validCount = 0;
    };

    inline void ResetTextureStagesToDefault()
    {
        reinterpret_cast<void(__cdecl*)()>(0x7FD100)();
    }

    inline void InvalidateRwShaderCaches()
    {
        InvalidateGeometryBindings();
    }

    inline void InvalidateRwLastSentRenderStates()
    {
        RwUInt32* lastSent = reinterpret_cast<RwUInt32*>(0xC98E88);
        for (RwUInt32 i = 0; i < 210; ++i)
        {
            lastSent[i] = 0xFFFFFFFF;
        }
    }

    inline void InvalidateRwTextureMatrixCache(RwUInt32 state)
    {
        *reinterpret_cast<void**>(0xC97C70 + state * sizeof(void*)) = nullptr;
    }

    struct StateBlockGuard
    {
        IDirect3DStateBlock9* sb = nullptr;
        IDirect3DDevice9* dev = nullptr;
        explicit StateBlockGuard(IDirect3DDevice9* d) : dev(d)
        {
            if (dev)
            {
                dev->CreateStateBlock(D3DSBT_ALL, &sb);
            }
        }
        ~StateBlockGuard()
        {
            if (sb)
            {
                sb->Apply();
                sb->Release();
            }
            if (dev)
            {
                ResetTextureStagesToDefault();
                InvalidateRwShaderCaches();
            }
        }
        bool IsValid() const
        {
            return sb != nullptr;
        }
    };

    inline void ForceRenderState(RwRenderState state, void* value, void* flipValue)
    {
        RwRenderStateSet(state, flipValue);
        RwRenderStateSet(state, value);
    }

    inline void EstablishFixedFunctionPipeline(IDirect3DDevice9* device)
    {
        if (!device)
        {
            return;
        }
        device->SetPixelShader(nullptr);
        device->SetVertexShader(nullptr);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
        device->SetRenderState(D3DRS_ALPHAREF, 2);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }
}
