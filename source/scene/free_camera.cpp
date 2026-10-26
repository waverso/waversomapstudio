// Source Repository: https://github.com/waverso/waversomapstudio

#include "free_camera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../utils/utils.h"
#include "../vendor/imgui/imgui.h"
#include "CCam.h"
#include "CCamera.h"
#include "CPad.h"
#include "CPed.h"
#include "common.h"
#include "../ui/gui.h"
#include "../hooks/game_hooks.h"
#include "../hooks/samp.h"
#include "scene.h"
#include "selection.h"

static constexpr float MovementDamping = 8.0f;
static constexpr float SpeedMultiplierSmoothing = 12.0f;
static constexpr float ScrollDamping = 10.0f;
static constexpr float RotationSmoothing = 40.0f;
static constexpr float SidebarIntroDurationSeconds = 0.25f;
static constexpr float FovSmoothing = 10.0f;
#include "free_camera_projection.inl"

void FreeCamera::GetCameraVectors(CVector& outForward, CVector& outRight, CVector& outUp)
{
    outRight = CVector(cosf(m_yawRadians), sinf(m_yawRadians), 0.0f);
    outForward
        = CVector(-sinf(m_yawRadians) * cosf(m_pitchRadians), cosf(m_yawRadians) * cosf(m_pitchRadians), sinf(m_pitchRadians));
    outUp = Utils::CrossProduct(outRight, outForward);
}

void FreeCamera::GetGizmoCameraVectors(D3DXVECTOR3& outForward, D3DXVECTOR3& outRight, D3DXVECTOR3& outUp)
{
    CCam& cam = TheCamera.m_aCams[TheCamera.m_nActiveCam];
    outRight = D3DXVECTOR3(cosf(m_yawRadians), sinf(m_yawRadians), 0.0f);
    outForward = D3DXVECTOR3(cam.m_vecFront.x, cam.m_vecFront.y, cam.m_vecFront.z);
    outUp = D3DXVECTOR3(cam.m_vecUp.x, cam.m_vecUp.y, cam.m_vecUp.z);
}

static float WrapPi(float a)
{
    while (a > std::numbers::pi_v<float>)
    {
        a -= Utils::TwoPi;
    }
    while (a < -std::numbers::pi_v<float>)
    {
        a += Utils::TwoPi;
    }
    return a;
}

void FreeCamera::GetYawPitch(float& outYaw, float& outPitch) const
{
    outYaw = m_yawRadians;
    outPitch = m_pitchRadians;
}

void FreeCamera::SetLookAt(const CVector& target)
{
    CVector d = target - m_position;
    float len = d.Magnitude();
    if (len < 1e-4f)
    {
        return;
    }
    m_yawRadians = atan2f(-d.x, d.y);
    m_pitchRadians = std::clamp(asinf(std::clamp(d.z / len, -1.0f, 1.0f)), -1.55f, 1.55f);
    m_rotationInputAccum = ImVec2(0.0f, 0.0f);
    m_rotationPending = ImVec2(0.0f, 0.0f);
    m_isOrbiting = false;
    m_orbitResidual = false;
    m_focus = {};
}

void FreeCamera::StartFlyTo(const CVector& targetPos)
{
    m_focus = {};
    m_isOrbiting = false;
    m_orbitResidual = false;
    m_focus.startPosition = m_position;
    m_focus.targetPosition = targetPos;
    m_focus.elapsedSeconds = 0.0f;
    m_focus.durationSeconds = 0.78f;
    m_focus.active = true;
}

void FreeCamera::StartFlyTo(const CVector& targetPos, float yaw, float pitch)
{
    StartFlyTo(targetPos);
    m_focus.orient = true;
    m_focus.startYawRadians = m_yawRadians;
    m_focus.startPitchRadians = m_pitchRadians;
    m_focus.targetYawRadians = m_yawRadians + WrapPi(yaw - m_yawRadians);
    m_focus.targetPitchRadians = std::clamp(pitch, -1.55f, 1.55f);
    m_rotationInputAccum = ImVec2(0.0f, 0.0f);
    m_rotationPending = ImVec2(0.0f, 0.0f);
}

void FreeCamera::FocusSelection()
{
    if (g_scene)
    {
        g_scene->UpdateWorldTransforms();
    }
    CVector center;
    float standoff;
    if (!Selection::ComputeFocusTarget(center, standoff))
    {
        return;
    }

    constexpr float pitchMin = -1.2f, pitchMax = 0.15f;
    if (m_pitchRadians < pitchMin || m_pitchRadians > pitchMax)
    {
        const float pitch = std::clamp(m_pitchRadians, pitchMin, pitchMax);
        const CVector forward(-sinf(m_yawRadians) * cosf(pitch), cosf(m_yawRadians) * cosf(pitch), sinf(pitch));
        StartFlyTo(center - forward * standoff, m_yawRadians, pitch);
    }
    else
    {
        StartFlyTo(center - m_cameraForward * standoff);
    }
}

void FreeCamera::Process()
{
    if (m_active)
    {
        CCam& activeCam = TheCamera.m_aCams[TheCamera.m_nActiveCam];

        if (!m_wasActive)
        {
            m_position = activeCam.m_vecSource;
            m_yawRadians = atan2f(-activeCam.m_vecFront.x, activeCam.m_vecFront.y);
            m_pitchRadians = asinf(activeCam.m_vecFront.z);
            m_velocity = { 0.0f, 0.0f, 0.0f };
            m_currentSpeedMultiplier = 1.0f;
            m_scrollThrottle = 0.0f;
            m_scrollDelta = 0.0f;
            m_rotationInputAccum = ImVec2(0.0f, 0.0f);
            m_rotationPending = ImVec2(0.0f, 0.0f);
            m_focus = {};
            m_panHasAnchor = false;
            m_sidebarIntroElapsed = 0.0f;

            if (m_targetFovDegrees <= 0.0f)
            {
                m_targetFovDegrees = activeCam.m_fFOV;
            }
            m_currentFovDegrees = activeCam.m_fFOV;

            if (g_gui)
            {
                g_gui->m_sidebarOffsetRatio = 0.0f;
            }

            GameHooks::SquashCameraTransition();

            m_wasActive = true;
        }

        LARGE_INTEGER currTime, freq;
        QueryPerformanceCounter(&currTime);
        QueryPerformanceFrequency(&freq);

        float dt = (m_previousTime.QuadPart > 0) ? (float)(currTime.QuadPart - m_previousTime.QuadPart) / (float)freq.QuadPart
                                                 : (1.0f / 60.0f);
        m_previousTime = currTime;
        dt = std::clamp(dt, 0.00005f, 0.05f);

        m_rotationPending.x += m_rotationInputAccum.x * m_mouseSensitivity / 1000.0f;
        m_rotationPending.y += m_rotationInputAccum.y * m_mouseSensitivity / 1000.0f;
        m_rotationInputAccum = ImVec2(0.0f, 0.0f);

        float rotAlpha = 1.0f - expf(-RotationSmoothing * dt);
        float dYaw = m_rotationPending.x * rotAlpha;
        float dPitch = m_rotationPending.y * rotAlpha;
        m_rotationPending.x -= dYaw;
        m_rotationPending.y -= dPitch;

        m_yawRadians -= dYaw;
        m_pitchRadians -= dPitch;
        m_pitchRadians = std::clamp(m_pitchRadians, -1.55f, 1.55f);

        CVector right, up;
        GetCameraVectors(m_cameraForward, right, up);

        bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        bool ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool panActive = m_panHasAnchor && m_middleMouseAllowed && !m_isOrbiting;

        float targetMult = 1.0f;
        if (altHeld)
        {
            targetMult = m_slowMultiplier;
        }
        else if (shiftHeld && m_keyboardMovementAllowed)
        {
            targetMult = m_fastMultiplier;
        }

        m_currentSpeedMultiplier += (targetMult - m_currentSpeedMultiplier) * (1.0f - expf(-SpeedMultiplierSmoothing * dt));

        CVector moveInput(0.0f, 0.0f, 0.0f);
        if (m_keyboardMovementAllowed && !m_isOrbiting && !ctrlHeld && !panActive)
        {
            if (GetAsyncKeyState('W') & 0x8000)
            {
                moveInput.x += m_cameraForward.x;
                moveInput.y += m_cameraForward.y;
                moveInput.z += m_cameraForward.z;
            }
            if (GetAsyncKeyState('S') & 0x8000)
            {
                moveInput.x -= m_cameraForward.x;
                moveInput.y -= m_cameraForward.y;
                moveInput.z -= m_cameraForward.z;
            }
            if (GetAsyncKeyState('D') & 0x8000)
            {
                moveInput.x += right.x;
                moveInput.y += right.y;
                moveInput.z += right.z;
            }
            if (GetAsyncKeyState('A') & 0x8000)
            {
                moveInput.x -= right.x;
                moveInput.y -= right.y;
                moveInput.z -= right.z;
            }
            if (GetAsyncKeyState('E') & 0x8000 || GetAsyncKeyState(VK_SPACE) & 0x8000)
            {
                moveInput.z += 1.0f;
            }
            if (GetAsyncKeyState('Q') & 0x8000)
            {
                moveInput.z -= 1.0f;
            }

            float len = sqrtf(moveInput.x * moveInput.x + moveInput.y * moveInput.y + moveInput.z * moveInput.z);
            if (len > 0.001f)
            {
                moveInput.x /= len;
                moveInput.y /= len;
                moveInput.z /= len;
            }
        }

        if (m_rightMouseScenePress && moveInput.Magnitude() > 0.0f)
        {
            m_rightMouseHadMoveInput = true;
        }

        bool scrolled = fabsf(m_scrollDelta) > 0.0f;
        float scrollMult = shiftHeld ? m_fastMultiplier : (altHeld ? m_slowMultiplier : 1.0f);
        if (panActive)
        {
            m_scrollDelta = 0.0f;
            m_scrollThrottle = 0.0f;
            m_velocity = { 0.0f, 0.0f, 0.0f };
            scrolled = false;
        }
        m_scrollThrottle += m_scrollDelta * m_scrollSpeed * scrollMult;
        m_scrollDelta = 0.0f;
        m_scrollThrottle *= expf(-ScrollDamping * dt);
        if (fabsf(m_scrollThrottle) < 0.01f)
        {
            m_scrollThrottle = 0.0f;
        }

        CVector targetVel = moveInput * (m_baseSpeed * m_currentSpeedMultiplier);
        targetVel.x += m_cameraForward.x * m_scrollThrottle;
        targetVel.y += m_cameraForward.y * m_scrollThrottle;
        targetVel.z += m_cameraForward.z * m_scrollThrottle;
        m_velocity += (targetVel - m_velocity) * (1.0f - expf(-MovementDamping * dt));

        m_position += m_velocity * dt;

        const bool orbitActive = m_isOrbiting || m_orbitResidual;
        if (orbitActive)
        {
            if (m_orbitResidual)
            {
                const bool residualSpent
                    = m_rotationPending.x * m_rotationPending.x + m_rotationPending.y * m_rotationPending.y < 1e-8f;
                const bool inputCancels = moveInput.Magnitude() > 0.0f || scrolled || m_rightMouseDown
                    || ImGui::IsMouseDown(ImGuiMouseButton_Middle) || m_focus.active;
                if (residualSpent || inputCancels)
                {
                    m_orbitResidual = false;
                }
            }
            if (m_isOrbiting || m_orbitResidual)
            {
                m_position = m_orbitPivot - m_cameraForward * m_orbitDistance;
            }
        }

        if (m_focus.active)
        {
            bool lookInput = m_focus.orient && (fabsf(dYaw) > 1e-5f || fabsf(dPitch) > 1e-5f);
            if (m_isOrbiting || moveInput.Magnitude() > 0.0f || scrolled || lookInput)
            {
                m_focus.active = false;
            }
            else
            {
                m_focus.elapsedSeconds = std::min(m_focus.elapsedSeconds + dt, m_focus.durationSeconds);
                float t = m_focus.elapsedSeconds / m_focus.durationSeconds;
                t = t * t * (3.0f - 2.0f * t);
                m_position = m_focus.startPosition + (m_focus.targetPosition - m_focus.startPosition) * t;
                if (m_focus.orient)
                {
                    m_yawRadians = m_focus.startYawRadians + (m_focus.targetYawRadians - m_focus.startYawRadians) * t;
                    m_pitchRadians = std::clamp(
                        m_focus.startPitchRadians + (m_focus.targetPitchRadians - m_focus.startPitchRadians) * t, -1.55f, 1.55f);
                    GetCameraVectors(m_cameraForward, right, up);
                }
                m_velocity = { 0.0f, 0.0f, 0.0f };
                m_scrollThrottle = 0.0f;
                if (m_focus.elapsedSeconds >= m_focus.durationSeconds)
                {
                    m_focus.active = false;
                }
            }
        }

        activeCam.m_vecSource = m_position;
        activeCam.m_vecFront = m_cameraForward;
        activeCam.m_vecUp = up;
        TheCamera.m_vecGameCamPos = m_position;

        m_currentFovDegrees += (m_targetFovDegrees - m_currentFovDegrees) * (1.0f - expf(-FovSmoothing * dt));
        activeCam.m_fFOV = std::clamp(m_currentFovDegrees, MinFovDegrees, MaxFovDegrees);

        if (g_gui && TheCamera.m_pRwCamera)
        {
            const float target = g_gui->GetSidebarWidthRatio();
            m_sidebarIntroElapsed = std::min(m_sidebarIntroElapsed + dt, SidebarIntroDurationSeconds);
            const float linearT = m_sidebarIntroElapsed / SidebarIntroDurationSeconds;
            const float smoothT = linearT * linearT * (3.0f - 2.0f * linearT);
            g_gui->m_sidebarOffsetRatio = m_sidebarIntroElapsed < SidebarIntroDurationSeconds ? target * smoothT : target;
            TheCamera.m_pRwCamera->viewOffset.x = 0.0f;
        }

        if (CPad* pad = CPad::GetPad(0))
        {
            pad->DisablePlayerControls = 1;
        }
        if (CPed* p = FindPlayerPed())
        {
            p->bIsVisible = false;
        }
    }
    else if (m_wasActive)
    {
        if (CPad* pad = CPad::GetPad(0))
        {
            pad->DisablePlayerControls = 0;
        }

        m_wasActive = false;
        m_velocity = { 0.0f, 0.0f, 0.0f };
        m_currentSpeedMultiplier = 1.0f;
        m_keyboardMovementAllowed = false;
        m_deferMouseCursorFrames = 0;
        m_rmbClickPending = false;
        m_middleMouseAllowed = false;
        m_isOrbiting = false;
        m_orbitResidual = false;
        m_panHasAnchor = false;
        m_scrollThrottle = 0.0f;
        m_scrollDelta = 0.0f;
        m_rotationInputAccum = ImVec2(0.0f, 0.0f);
        m_rotationPending = ImVec2(0.0f, 0.0f);
        m_focus = {};
        m_sidebarIntroElapsed = 0.0f;

        if (g_gui)
        {
            g_gui->m_sidebarOffsetRatio = 0.0f;
        }
        if (TheCamera.m_pRwCamera)
        {
            TheCamera.m_pRwCamera->viewOffset.x = 0.0f;
            TheCamera.m_pRwCamera->projectionType = rwPERSPECTIVE;
        }
    }
}

void FreeCamera::Update()
{
    UpdateGamePos();

    if (Utils::IsKeyPressedOnce(VK_F4) && (m_active || SampIsActive()))
    {
        m_active = !m_active;
        m_cursorResetPending = m_active;
    }

    const bool imguiTextInput = ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput;
    if (m_active && !m_focus.active && !imguiTextInput && !SampTextInputActive() && !GuiIsAnyModalOpen()
        && Utils::IsKeyPressedOnce('F'))
    {
        FocusSelection();
    }
}

void FreeCamera::UpdateInput(ImGuiIO& io)
{
    UpdateInputDelta(io);
}
