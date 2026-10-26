// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cmath>
#include <d3d9.h>
#include <map>

#include "CPed.h"
#include "CPedIntelligence.h"
#include "CTaskManager.h"
#include "CVector.h"
#include "common.h"
#include "d3dx_math_compat.h"
#include "plugin.h"
#include "transform_math.h"

namespace Utils
{
    static bool IntersectsRaySphere(const CVector& o, const CVector& d, const CVector& c, float r, float maxD)
    {
        CVector oc = c - o;
        float b = Dot3(oc, d);
        float cc = Dot3(oc, oc) - r * r;
        if (cc > 0.f && b < 0.f)
        {
            return false;
        }
        float disc = b * b - cc;
        if (disc < 0.f)
        {
            return false;
        }
        return (b - sqrtf(disc)) <= maxD;
    }

    static bool IntersectRayTriangle(
        const CVector& o, const CVector& d, const CVector& v0, const CVector& v1, const CVector& v2, bool cull, float& t)
    {
        constexpr float Epsilon = 1e-7f;
        CVector e1 = v1 - v0, e2 = v2 - v0;
        CVector h = CrossProduct(d, e2);
        float a = Dot3(e1, h);
        if (cull && a < Epsilon)
        {
            return false;
        }
        if (fabsf(a) < Epsilon)
        {
            return false;
        }
        float f = 1.f / a;
        CVector s = o - v0;
        float u = f * Dot3(s, h);
        if (u < 0.f || u > 1.f)
        {
            return false;
        }
        CVector q = CrossProduct(s, e1);
        float v = f * Dot3(d, q);
        if (v < 0.f || u + v > 1.f)
        {
            return false;
        }
        t = f * Dot3(e2, q);
        return t > Epsilon;
    }

    static bool ScreenToWorldRayWithMatrices(float sx, float sy, const D3DVIEWPORT9& vp, const D3DMATRIX& proj,
        const D3DMATRIX& view, CVector& outOrigin, CVector& outDir)
    {
        if (vp.Width == 0 || vp.Height == 0)
        {
            return false;
        }
        if (sx < static_cast<float>(vp.X) || sy < static_cast<float>(vp.Y) || sx >= static_cast<float>(vp.X + vp.Width)
            || sy >= static_cast<float>(vp.Y + vp.Height))
        {
            return false;
        }

        D3DXMATRIX world = {};
        D3DXMatrixIdentity(&world);

        D3DXVECTOR3 vN(sx, sy, 0.f), vF(sx, sy, 1.f);
        const auto* projDx = reinterpret_cast<const D3DXMATRIX*>(&proj);
        const auto* viewDx = reinterpret_cast<const D3DXMATRIX*>(&view);
        D3DXVec3Unproject(&vN, &vN, &vp, projDx, viewDx, &world);
        D3DXVec3Unproject(&vF, &vF, &vp, projDx, viewDx, &world);

        outOrigin = CVector(vN.x, vN.y, vN.z);
        CVector d(vF.x - vN.x, vF.y - vN.y, vF.z - vN.z);
        const float len = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        if (len <= 0.0001f)
        {
            return false;
        }
        outDir = d * (1.0f / len);
        return true;
    }

    static bool ScreenToWorldRay(IDirect3DDevice9* device, float sx, float sy, CVector& outOrigin, CVector& outDir)
    {
        if (!device)
        {
            return false;
        }

        D3DVIEWPORT9 vp = {};
        D3DXMATRIX proj = {}, view = {};
        if (FAILED(device->GetViewport(&vp)) || vp.Width == 0 || vp.Height == 0)
        {
            return false;
        }
        if (FAILED(device->GetTransform(D3DTS_PROJECTION, &proj)))
        {
            return false;
        }
        if (FAILED(device->GetTransform(D3DTS_VIEW, &view)))
        {
            return false;
        }
        return ScreenToWorldRayWithMatrices(sx, sy, vp, proj, view, outOrigin, outDir);
    }

    static bool ScreenToWorldRayWithView(
        IDirect3DDevice9* device, float sx, float sy, const D3DXMATRIX& viewMat, CVector& outOrigin, CVector& outDir)
    {
        if (!device)
        {
            return false;
        }

        D3DVIEWPORT9 vp = {};
        D3DXMATRIX proj = {};
        if (FAILED(device->GetViewport(&vp)) || vp.Width == 0 || vp.Height == 0)
        {
            return false;
        }
        if (FAILED(device->GetTransform(D3DTS_PROJECTION, &proj)))
        {
            return false;
        }
        return ScreenToWorldRayWithMatrices(sx, sy, vp, proj, viewMat, outOrigin, outDir);
    }

    static void AllowMouseMovement(bool allow)
    {
        *(BYTE*)0x6194A0 = allow ? 0xE9 : 0xC3;
    }

    static void FatalExit(const char* msg)
    {
        HWND wnd = (RsGlobal.ps && RsGlobal.ps->window) ? (HWND)RsGlobal.ps->window : nullptr;
        if (wnd)
        {
            ShowWindow(wnd, SW_MINIMIZE);
        }
        MessageBoxA(wnd, msg, MOD_NAME, MB_ICONERROR | MB_OK | MB_TOPMOST | MB_SETFOREGROUND | MB_SYSTEMMODAL);
        ExitProcess(1);
    }

    static void DisembarkInstantly()
    {
        CTaskManager* taskManager = &FindPlayerPed()->m_pIntelligence->m_TaskMgr;

        for (int i = 0; i < TASK_PRIMARY_MAX; i++)
        {
            CTask* pTask = taskManager->m_aPrimaryTasks[i];
            if (pTask)
            {
                pTask->MakeAbortable(FindPlayerPed(), ABORT_PRIORITY_IMMEDIATE, NULL);
                if (i != TASK_PRIMARY_DEFAULT)
                {
                    taskManager->SetTask(NULL, i, 0);
                }
            }
        }

        for (int i = 0; i < TASK_SECONDARY_MAX; i++)
        {
            CTask* pTask = taskManager->m_aSecondaryTasks[i];
            if (pTask)
            {
                pTask->MakeAbortable(FindPlayerPed(), ABORT_PRIORITY_IMMEDIATE, NULL);
                if (i != TASK_SECONDARY_FACIAL_COMPLEX)
                {
                    taskManager->SetTaskSecondary(NULL, i);
                }
            }
        }
    }

    static std::map<int, bool> g_keyStates;
    static bool IsKeyPressedOnce(int key)
    {
        if (plugin::KeyPressed(key))
        {
            if (!g_keyStates[key])
            {
                g_keyStates[key] = true;
                return true;
            }
        }
        else
        {
            g_keyStates[key] = false;
        }
        return false;
    }
}
