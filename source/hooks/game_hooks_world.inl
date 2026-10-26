// Source Repository: https://github.com/waverso/waversomapstudio

static bool(__cdecl* g_originalCameraCheckPeds)(const CVector&, const CVector&, float&)
    = reinterpret_cast<bool(__cdecl*)(const CVector&, const CVector&, float&)>(0x4154A0);

static bool __cdecl EditorCameraCheckPeds(const CVector& source, const CVector& normal, float& nearest)
{
    if (g_freeCamera && g_freeCamera->m_active)
    {
        return false;
    }
    return g_originalCameraCheckPeds(source, normal, nearest);
}

static void __stdcall FreeCamWrapper()
{
    if (g_freeCamera)
    {
        g_freeCamera->Process();
    }
}

static void __declspec(naked) HookProcessCamera()
{
    __asm
    {
		pushad
		call FreeCamWrapper
		popad

		movzx ecx, byte ptr[esi + 0x59]
		imul ecx, 0x238

		push 0x52BA10
		retn
    }
}

static void ForceIplLoadRequest(const CVector2D& posn, IplDef* def)
{
    const float margin = def->m_bInterior ? 550.0f : 0.0f;
    if (def->m_boundBox.IsCircleInside(posn, margin))
    {
        def->m_bLoadRequest = true;
    }
}

RwCamera* __cdecl GameHooks::HookRwCameraBeginUpdate(RwCamera* camera)
{
    RwCamera* result = g_gameHooks->m_originalRwCameraBeginUpdate(camera);
    if (result && camera == TheCamera.m_pRwCamera && g_freeCamera)
    {
        g_freeCamera->ApplyVisualNavigationRender();
    }
    return result;
}

bool __cdecl GameHooks::HookSpriteCalcScreenCoors(
    const RwV3d& posn, RwV3d* out, float* width, float* height, bool checkMaxVisible, bool checkMinVisible)
{
    const bool visible = g_gameHooks->m_originalSpriteCalcScreenCoors(posn, out, width, height, checkMaxVisible, checkMinVisible);
    if (visible && out && g_freeCamera)
    {
        g_freeCamera->RemapVisualScreenCoordinates(*out);
    }
    return visible;
}

bool __cdecl GameHooks::HookClockGetIsTimeInRange(unsigned char from, unsigned char to)
{
    if (g_freeCamera && g_freeCamera->m_active)
    {
        return true;
    }
    return g_gameHooks->m_originalClockGetIsTimeInRange(from, to);
}

CVector __cdecl GameHooks::HookFindPlayerCoors(int playerId)
{
    if (g_freeCamera->m_active)
    {
        return g_freeCamera->m_position;
    }
    return g_gameHooks->m_originalFindPlayerCoors(playerId);
}

void GameHooks::SquashCameraTransition()
{
    TheCamera.m_nTransitionDuration = 0;
    TheCamera.m_nTransitionDurationTargetCoors = 0;
    TheCamera.m_nTimeTransitionStart = CTimer::m_snTimeInMilliseconds - 1;
    TheCamera.m_bTransitionState = false;
    TheCamera.m_bTransitionJUSTStarted = false;
    TheCamera.m_fFractionInterToStopMoving = 1.0f;
    TheCamera.m_fFractionInterToStopCatchUp = 1.0f;
    TheCamera.m_fFractionInterToStopMovingTarget = 1.0f;
    TheCamera.m_fFractionInterToStopCatchUpTarget = 1.0f;
}

void __fastcall GameHooks::HookStartTransition(CCamera* cam, void*, short newMode)
{
    g_gameHooks->m_originalStartTransition(cam, newMode);
    if (g_freeCamera && g_freeCamera->m_active)
    {
        SquashCameraTransition();
    }
}

void __cdecl GameHooks::HookEnsureIplsAreInMemory(const CVector& posn)
{
    if (g_freeCamera->m_active && g_renderInteriors)
    {
        if (CStreaming::ms_disableStreaming)
        {
            return;
        }
        CIplStore::SetIplsRequired(posn, 0);
        return;
    }

    g_gameHooks->m_originalEnsureIplsAreInMemory(posn);
}

void __cdecl GameHooks::HookSetIfIplIsRequired(const CVector2D& posn, IplDef* def)
{
    if (g_freeCamera->m_active && g_renderInteriors)
    {
        return ForceIplLoadRequest(posn, def);
    }
    g_gameHooks->m_originalSetIfIplIsRequired(posn, def);
}

void __cdecl GameHooks::HookSetIfIplIsRequiredReducedBb(const CVector2D& posn, IplDef* def)
{
    if (g_freeCamera->m_active && g_renderInteriors)
    {
        return ForceIplLoadRequest(posn, def);
    }
    g_gameHooks->m_originalSetIfIplIsRequiredReducedBb(posn, def);
}

void __cdecl GameHooks::HookSetIfInteriorIplIsRequired(const CVector2D& posn, IplDef* def)
{
    if (g_freeCamera->m_active && g_renderInteriors)
    {
        return ForceIplLoadRequest(posn, def);
    }
    g_gameHooks->m_originalSetIfInteriorIplIsRequired(posn, def);
}
