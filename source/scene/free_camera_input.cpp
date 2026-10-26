// Source Repository: https://github.com/waverso/waversomapstudio

#include <algorithm>
#include <cmath>

#include "../utils/utils.h"
#include "../vendor/imgui/imgui_internal.h"
#include "common.h"
#include "entity_raycast.h"
#include "free_camera.h"
#include "../ui/gui.h"
#include "../hooks/game_hooks.h"
#include "../hooks/samp.h"

static constexpr int CameraCursorSkipFrames = 3;
static constexpr int CameraRmbClickMs = 160;
static constexpr int CameraPopupAntiClickMs = 250;

static float VisualViewportCenterX(float displayWidth)
{
    const float sidebarRatio = std::clamp(g_gui ? g_gui->m_sidebarOffsetRatio : 0.0f, 0.0f, 1.0f);
    return displayWidth * (1.0f - sidebarRatio) * 0.5f;
}

static float PanRayScreenX(const FreeCamera* freeCam, float screenX)
{
    float visualOffsetX = 0.0f;
    if (freeCam && freeCam->GetVisualOverlayOffsetX(visualOffsetX))
    {
        screenX -= visualOffsetX;
    }
    return screenX;
}

void FreeCamera::TrackRightMouseDrag(const ImVec2& delta)
{
    if (!m_rightMouseScenePress)
    {
        return;
    }
    m_rightMouseDragDelta.x += delta.x;
    m_rightMouseDragDelta.y += delta.y;
}

bool FreeCamera::WrapPanCursor(ImGuiIO& io)
{
    if (!m_panHasAnchor || !m_middleMouseAllowed || m_isOrbiting || !g_gameHooks || !g_gameHooks->m_device || !RsGlobal.ps
        || !RsGlobal.ps->window)
    {
        return false;
    }

    RECT clientRect = {};
    if (!GetClientRect(RsGlobal.ps->window, &clientRect))
    {
        return false;
    }

    const LONG width = clientRect.right - clientRect.left;
    const LONG height = clientRect.bottom - clientRect.top;
    if (width < 3 || height < 3)
    {
        return false;
    }

    const float sidebarRatio = std::clamp(g_gui ? g_gui->m_sidebarOffsetRatio : 0.0f, 0.0f, 0.9f);
    const LONG viewportWidth
        = std::clamp<LONG>(static_cast<LONG>(std::lround(static_cast<float>(width) * (1.0f - sidebarRatio))), 3, width);

    POINT cursorScreen = {};
    if (!GetCursorPos(&cursorScreen))
    {
        return false;
    }
    POINT cursorClient = cursorScreen;
    if (!ScreenToClient(RsGlobal.ps->window, &cursorClient))
    {
        return false;
    }

    io.MousePos = ImVec2(static_cast<float>(cursorClient.x), static_cast<float>(cursorClient.y));

    POINT wrappedClient = cursorClient;
    if (cursorClient.x <= 0)
    {
        wrappedClient.x = viewportWidth - 2;
    }
    else if (cursorClient.x >= viewportWidth - 1)
    {
        wrappedClient.x = 1;
    }
    if (cursorClient.y <= 0)
    {
        wrappedClient.y = height - 2;
    }
    else if (cursorClient.y >= height - 1)
    {
        wrappedClient.y = 1;
    }
    if (wrappedClient.x == cursorClient.x && wrappedClient.y == cursorClient.y)
    {
        return false;
    }

    auto hitAt = [&](float sx, float sy, CVector& hit)
    {
        CVector origin, direction;
        if (!Utils::ScreenToWorldRayWithView(
                g_gameHooks->m_device, PanRayScreenX(this, sx), sy, m_panStartView, origin, direction))
        {
            return false;
        }
        const float denominator = Utils::Dot3(direction, m_panPlaneNorm);
        if (fabsf(denominator) <= 0.001f)
        {
            return false;
        }
        const float distance = Utils::Dot3(m_panStartHitPosition - origin, m_panPlaneNorm) / denominator;
        hit = origin + direction * distance;
        return true;
    };

    CVector oldHit, wrappedHit;
    if (!hitAt(static_cast<float>(cursorClient.x), static_cast<float>(cursorClient.y), oldHit)
        || !hitAt(static_cast<float>(wrappedClient.x), static_cast<float>(wrappedClient.y), wrappedHit))
    {
        return false;
    }

    m_panStartHitPosition += wrappedHit - oldHit;

    POINT wrappedScreen = wrappedClient;
    ClientToScreen(RsGlobal.ps->window, &wrappedScreen);
    if (g_gameHooks)
    {
        const POINT warpClient = { wrappedClient.x, wrappedClient.y };
        g_gameHooks->NotifySyntheticCursorWarp(warpClient);
    }
    SetCursorPos(wrappedScreen.x, wrappedScreen.y);
    io.MousePos = io.MousePosPrev = ImVec2(static_cast<float>(wrappedClient.x), static_cast<float>(wrappedClient.y));
    io.MouseDelta = ImVec2(0.0f, 0.0f);
    return true;
}

void FreeCamera::UpdateInputDelta(ImGuiIO& io)
{
    bool currRightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    bool mouseBlocked = io.WantCaptureMouse || (g_gui && (g_gui->m_sidebarInteracting || g_gui->IsDragCursorInputBlocked()));
    m_keyboardMovementAllowed = !mouseBlocked && !io.WantCaptureKeyboard && !io.WantTextInput && !SampTextInputActive();
    const bool movementInputActive = m_keyboardMovementAllowed && !(GetAsyncKeyState(VK_CONTROL) & 0x8000)
        && ((GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState('A') & 0x8000)
            || (GetAsyncKeyState('D') & 0x8000) || (GetAsyncKeyState('E') & 0x8000) || (GetAsyncKeyState('Q') & 0x8000)
            || (GetAsyncKeyState(VK_SPACE) & 0x8000));

    const bool rightPressed = currRightDown && !m_previousRightDown;
    const bool rightReleased = !currRightDown && m_previousRightDown;
    if (rightPressed)
    {
        m_orbitResidual = false;
        m_rightMouseScenePress = m_active && !mouseBlocked;
        if (m_rightMouseScenePress)
        {
            ImGui::ClearActiveID();
            if (!GetCursorPos(&m_rightMouseDownScreenPosition))
            {
                m_rightMouseDownScreenPosition.x = static_cast<LONG>(std::lround(io.MousePos.x));
                m_rightMouseDownScreenPosition.y = static_cast<LONG>(std::lround(io.MousePos.y));
                ClientToScreen(RsGlobal.ps->window, &m_rightMouseDownScreenPosition);
            }

            POINT clientPos = m_rightMouseDownScreenPosition;
            ScreenToClient(RsGlobal.ps->window, &clientPos);
            m_rightMouseDownClientPosition = ImVec2(static_cast<float>(clientPos.x), static_cast<float>(clientPos.y));
            m_rightMouseDownTimeMs = GetTickCount64();
            m_rightMouseDragDelta = ImVec2(0.0f, 0.0f);
            m_rightMouseHadMoveInput = false;
            m_rmbClickPending = true;
        }
    }

    if (rightReleased && m_rightMouseScenePress)
    {
        const bool releasedBeforeSkipFrames = m_skipFrames > 0;
        if (!releasedBeforeSkipFrames)
        {
            TrackRightMouseDrag(io.MouseDelta);
        }
        const float dragSq
            = m_rightMouseDragDelta.x * m_rightMouseDragDelta.x + m_rightMouseDragDelta.y * m_rightMouseDragDelta.y;
        const bool quickReleaseWithoutDrag = GetTickCount64() - m_rightMouseDownTimeMs < CameraRmbClickMs
            && dragSq < io.MouseDragThreshold * io.MouseDragThreshold;
        const bool shouldOpenPopup = (releasedBeforeSkipFrames || quickReleaseWithoutDrag) && !m_rightMouseHadMoveInput
            && !movementInputActive && GetTickCount64() - m_lastRotationEndTimeMs > CameraPopupAntiClickMs;
        if (!shouldOpenPopup)
        {
            m_lastRotationEndTimeMs = GetTickCount64();
        }
        POINT releaseClient = { static_cast<LONG>(std::lround(m_rightMouseDownClientPosition.x)),
            static_cast<LONG>(std::lround(m_rightMouseDownClientPosition.y)) };
        POINT currentScreen = {};
        if (!GetCursorPos(&currentScreen) || currentScreen.x != m_rightMouseDownScreenPosition.x
            || currentScreen.y != m_rightMouseDownScreenPosition.y)
        {
            if (g_gameHooks)
            {
                g_gameHooks->NotifySyntheticCursorWarp(releaseClient);
            }
            SetCursorPos(m_rightMouseDownScreenPosition.x, m_rightMouseDownScreenPosition.y);
        }
        io.MousePos = m_rightMouseDownClientPosition;
        io.MouseDelta = ImVec2(0.0f, 0.0f);
        if (!m_rmbClickPending)
        {
            m_deferMouseCursorFrames = CameraCursorSkipFrames;
        }

        if (shouldOpenPopup)
        {
            if (g_gui)
            {
                g_gui->QueueSceneContextMenu(m_rightMouseDownClientPosition);
            }
        }
        m_rmbClickPending = false;
        m_rightMouseScenePress = false;
    }

    m_rightMouseDown = m_active && currRightDown && m_rightMouseScenePress;
    m_previousRightDown = currRightDown;

    if (m_rmbClickPending)
    {
        m_rightMouseDragDelta.x += io.MouseDelta.x;
        m_rightMouseDragDelta.y += io.MouseDelta.y;
        const float pendingDragSq
            = m_rightMouseDragDelta.x * m_rightMouseDragDelta.x + m_rightMouseDragDelta.y * m_rightMouseDragDelta.y;
        const bool timeExceeded = GetTickCount64() - m_rightMouseDownTimeMs > CameraRmbClickMs;
        if (timeExceeded || pendingDragSq >= io.MouseDragThreshold * io.MouseDragThreshold || m_rightMouseHadMoveInput
            || movementInputActive)
        {
            m_rmbClickPending = false;
        }
    }

    bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

    bool panCurrDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !m_rightMouseDown;

    if (panCurrDown && !m_panPreviousDown)
    {
        m_orbitResidual = false;
        m_middleMouseAllowed = !mouseBlocked;
        if (m_middleMouseAllowed)
        {
            if (!GetCursorPos(&m_middleDownScreenPosition))
            {
                m_middleDownScreenPosition.x = static_cast<LONG>(std::lround(io.MousePos.x));
                m_middleDownScreenPosition.y = static_cast<LONG>(std::lround(io.MousePos.y));
                ClientToScreen(RsGlobal.ps->window, &m_middleDownScreenPosition);
            }

            POINT middleClient = m_middleDownScreenPosition;
            ScreenToClient(RsGlobal.ps->window, &middleClient);
            m_middleDownClientPosition = ImVec2(static_cast<float>(middleClient.x), static_cast<float>(middleClient.y));

            if (altHeld)
            {
                m_isOrbiting = true;
                m_panHasAnchor = false;

                CVector orbitRay = m_cameraForward;

                EntityRay::RayResult hit = EntityRay::Cast(m_position, orbitRay, 500.0f, EntityRay::LodCastMode::IgnoreHidden);
                if (!hit.pEntity)
                {
                    m_orbitDistance = 0.0f, m_orbitPivot = m_position;
                }
                else
                {
                    m_orbitPivot = hit.vecHitPos;
                    m_orbitDistance = hit.fHitDistance;
                }
            }
            else
            {
                m_isOrbiting = false;
                m_panPlaneNorm = m_cameraForward;
                m_panStartCameraPosition = m_position;
                g_gameHooks->m_device->GetTransform(D3DTS_VIEW, &m_panStartView);

                CVector origin, dir;
                if (Utils::ScreenToWorldRay(g_gameHooks->m_device, PanRayScreenX(this, m_middleDownClientPosition.x),
                        m_middleDownClientPosition.y, origin, dir))
                {
                    EntityRay::RayResult hit = EntityRay::Cast(origin, dir, 500.0f, EntityRay::LodCastMode::IgnoreHidden);
                    m_panStartHitPosition = hit.pEntity ? hit.vecHitPos : origin + dir * 500.0f;
                }
                m_panHasAnchor = true;
            }
        }
    }
    if (!panCurrDown)
    {
        m_middleMouseAllowed = false;
        if (m_isOrbiting)
        {
            m_orbitResidual = true;

            m_lastRotationEndTimeMs = GetTickCount64();
            POINT currentScreen = {};
            if (!GetCursorPos(&currentScreen) || currentScreen.x != m_middleDownScreenPosition.x
                || currentScreen.y != m_middleDownScreenPosition.y)
            {
                const POINT releaseClient = { static_cast<LONG>(std::lround(m_middleDownClientPosition.x)),
                    static_cast<LONG>(std::lround(m_middleDownClientPosition.y)) };
                if (g_gameHooks)
                {
                    g_gameHooks->NotifySyntheticCursorWarp(releaseClient);
                }
                SetCursorPos(m_middleDownScreenPosition.x, m_middleDownScreenPosition.y);
            }
            io.MousePos = m_middleDownClientPosition;
            io.MouseDelta = ImVec2(0.0f, 0.0f);
            m_deferMouseCursorFrames = CameraCursorSkipFrames;
        }
        m_isOrbiting = false;
        m_panHasAnchor = false;
    }
    m_panPreviousDown = panCurrDown;

    bool isRotating = (m_rightMouseDown && !m_rmbClickPending) || (panCurrDown && m_middleMouseAllowed && m_isOrbiting);
    const bool resetCursor = m_active && m_cursorResetPending;
    if (resetCursor)
    {
        m_deferMouseCursorFrames = CameraCursorSkipFrames;
    }
    m_moving = isRotating;
    const bool deferMouseCursor = m_deferMouseCursorFrames > 0;
    io.MouseDrawCursor = !isRotating && m_active && !SampCursorVisible() && !deferMouseCursor;
    if (deferMouseCursor)
    {
        --m_deferMouseCursorFrames;
    }

    if (isRotating || resetCursor)
    {
        const float displayW = io.DisplaySize.x > 0.0f ? io.DisplaySize.x : static_cast<float>(RsGlobal.maximumWidth);
        const float displayH = io.DisplaySize.y > 0.0f ? io.DisplaySize.y : static_cast<float>(RsGlobal.maximumHeight);
        const int cx = static_cast<int>(std::lround(VisualViewportCenterX(displayW)));
        const int cy = static_cast<int>(std::lround(displayH * 0.5f));
        ImVec2 rotationDelta = io.MouseDelta;
        POINT cursorScreen = {};
        const bool hasCursorScreen = GetCursorPos(&cursorScreen) != FALSE;
        if (hasCursorScreen)
        {
            POINT cursorClient = cursorScreen;
            if (ScreenToClient(RsGlobal.ps->window, &cursorClient))
            {
                rotationDelta.x = static_cast<float>(cursorClient.x - cx);
                rotationDelta.y = static_cast<float>(cursorClient.y - cy);
                if (fabsf(rotationDelta.x) > displayW || fabsf(rotationDelta.y) > displayH)
                {
                    rotationDelta = ImVec2(0.0f, 0.0f);
                }
            }
        }
        POINT pt = { (LONG)cx, (LONG)cy };
        if (!ClientToScreen(RsGlobal.ps->window, &pt) && resetCursor)
        {
            return;
        }
        if (!hasCursorScreen || cursorScreen.x != pt.x || cursorScreen.y != pt.y)
        {
            if (g_gameHooks)
            {
                const POINT warpClient = { static_cast<LONG>(cx), static_cast<LONG>(cy) };
                g_gameHooks->NotifySyntheticCursorWarp(warpClient);
            }
            if (!SetCursorPos(pt.x, pt.y) && resetCursor)
            {
                return;
            }
        }

        if (isRotating && !resetCursor && m_skipFrames <= 0)
        {
            TrackRightMouseDrag(rotationDelta);
            m_rotationInputAccum.x += rotationDelta.x;
            m_rotationInputAccum.y += rotationDelta.y;
        }
        else if (isRotating && !resetCursor)
        {
            m_skipFrames--;
        }
        else
        {
            m_skipFrames = CameraCursorSkipFrames;
        }

        m_lastMousePosition = ImVec2((float)cx, (float)cy);
        io.MousePos = m_lastMousePosition;
        if (resetCursor)
        {
            GuiSyncMousePosition(pt);
            m_cursorResetPending = false;
        }
    }
    else
    {
        m_skipFrames = CameraCursorSkipFrames;
    }

    if (!m_active)
    {
        return;
    }

    if (m_focus.active && panCurrDown && m_middleMouseAllowed)
    {
        m_focus.active = false;
    }

    const bool panActive = m_panHasAnchor && panCurrDown && m_middleMouseAllowed && !m_isOrbiting;
    if (panActive)
    {
        WrapPanCursor(io);
    }
    if (!mouseBlocked && !panActive)
    {
        if (io.KeyCtrl)
        {
            m_targetFovDegrees
                = std::clamp(m_targetFovDegrees - io.MouseWheel * FovScrollStepDegrees, MinFovDegrees, MaxFovDegrees);
        }
        else
        {
            m_scrollDelta += io.MouseWheel;
        }
    }

    if (m_panHasAnchor && panCurrDown && m_middleMouseAllowed && !m_isOrbiting)
    {
        CVector origin_now, dir_now;
        if (Utils::ScreenToWorldRayWithView(
                g_gameHooks->m_device, PanRayScreenX(this, io.MousePos.x), io.MousePos.y, m_panStartView, origin_now, dir_now))
        {
            float denom = Utils::Dot3(dir_now, m_panPlaneNorm);
            if (fabsf(denom) > 0.001f)
            {
                float t = Utils::Dot3(m_panStartHitPosition - origin_now, m_panPlaneNorm) / denom;
                CVector currentHitOnPlane = origin_now + dir_now * t;
                CVector offset = m_panStartHitPosition - currentHitOnPlane;
                m_position = m_panStartCameraPosition + offset;
            }
        }
        ImGui::SetMouseCursor(ImGuiMouseCursor_Drag);
    }
}
