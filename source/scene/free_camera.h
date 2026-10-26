// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include "../utils/d3dx_math_compat.h"
#include "../vendor/imgui/imgui.h"
#include "CVector.h"

class FreeCamera
{
    public:
    struct FocusState
    {
        bool active = false;
        CVector startPosition = {};
        CVector targetPosition = {};
        float elapsedSeconds = 0.0f;
        float durationSeconds = 0.78f;
        bool orient = false;
        float startYawRadians = 0.0f;
        float targetYawRadians = 0.0f;
        float startPitchRadians = 0.0f;
        float targetPitchRadians = 0.0f;
    };

    void Update();
    void Process();
    void UpdateInput(ImGuiIO& io);
    void GetCameraVectors(CVector& outForward, CVector& outRight, CVector& outUp);
    void GetGizmoCameraVectors(D3DXVECTOR3& outForward, D3DXVECTOR3& outRight, D3DXVECTOR3& outUp);
    void BeginVisualNavigationRender();
    void ApplyVisualNavigationRender();
    void SetVisualWorldProjectionPass(bool active);
    bool RemapVisualScreenCoordinates(RwV3d& screen) const;
    bool GetVisualOverlayOffsetX(float& outOffset) const;
    bool ScreenToWorldRayForPicking(float sx, float sy, CVector& outOrigin, CVector& outDir) const;
    void EndVisualNavigationRender();
    void GetYawPitch(float& outYaw, float& outPitch) const;
    void SetLookAt(const CVector& target);
    void StartFlyTo(const CVector& targetPos);
    void StartFlyTo(const CVector& targetPos, float yaw, float pitch);
    void FocusSelection();

    bool m_active = false;
    bool m_moving = false;

    bool m_panHasAnchor = false;
    CVector m_position = { 0.0f, 0.0f, 0.0f };
    FocusState m_focus = {};

    float m_targetFovDegrees = 0.0f;
    float m_currentFovDegrees = 0.0f;

    float m_mouseSensitivity = DefaultMouseSensitivity;
    float m_baseSpeed = DefaultBaseSpeed;
    float m_scrollSpeed = DefaultScrollSpeed;
    float m_fastMultiplier = DefaultFastMultiplier;
    float m_slowMultiplier = DefaultSlowMultiplier;

    static constexpr float MinFovDegrees = 20.0f, MaxFovDegrees = 110.0f, DefaultFovDegrees = 70.0f;
    static constexpr float FovScrollStepDegrees = 3.0f;
    static constexpr float MinMouseSensitivity = 0.5f, MaxMouseSensitivity = 10.0f, DefaultMouseSensitivity = 2.5f;
    static constexpr float MinBaseSpeed = 10.0f, MaxBaseSpeed = 150.0f, DefaultBaseSpeed = 60.0f;
    static constexpr float MinScrollSpeed = 20.0f, MaxScrollSpeed = 130.0f, DefaultScrollSpeed = 70.0f;
    static constexpr float MinFastMultiplier = 2.0f, MaxFastMultiplier = 5.0f, DefaultFastMultiplier = 4.0f;
    static constexpr float MinSlowMultiplier = 0.05f, MaxSlowMultiplier = 0.5f, DefaultSlowMultiplier = 0.2f;

    private:
    void UpdateInputDelta(ImGuiIO& io);
    void TrackRightMouseDrag(const ImVec2& delta);
    bool WrapPanCursor(ImGuiIO& io);
    void UpdateGamePos();

    CVector m_cameraForward = {};
    float m_yawRadians = 0.0f;
    float m_pitchRadians = 0.0f;
    bool m_wasActive = false;

    bool m_rightMouseDown = false;
    bool m_rightMouseScenePress = false;
    bool m_rmbClickPending = false;
    bool m_rightMouseHadMoveInput = false;
    POINT m_rightMouseDownScreenPosition = {};
    ImVec2 m_rightMouseDownClientPosition = {};
    ULONGLONG m_rightMouseDownTimeMs = 0;
    ImVec2 m_rightMouseDragDelta = ImVec2(0.0f, 0.0f);
    ULONGLONG m_lastRotationEndTimeMs = 0;
    POINT m_middleDownScreenPosition = {};
    ImVec2 m_middleDownClientPosition = {};
    ImVec2 m_rotationInputAccum = ImVec2(0.0f, 0.0f);
    ImVec2 m_rotationPending = ImVec2(0.0f, 0.0f);
    float m_scrollThrottle = 0.0f;
    float m_scrollDelta = 0.0f;
    float m_sidebarIntroElapsed = 0.0f;

    CVector m_velocity = { 0.0f, 0.0f, 0.0f };
    float m_currentSpeedMultiplier = 1.0f;

    bool m_keyboardMovementAllowed = false;
    bool m_middleMouseAllowed = false;

    bool m_isOrbiting = false;
    bool m_orbitResidual = false;
    CVector m_orbitPivot = { 0.0f, 0.0f, 0.0f };
    float m_orbitDistance = 0.0f;

    CVector m_panStartCameraPosition = {};
    CVector m_panPlaneNorm = {};
    CVector m_panStartHitPosition = {};
    D3DXMATRIX m_panStartView = {};

    ImVec2 m_lastMousePosition = {};
    int m_skipFrames = 0;
    int m_deferMouseCursorFrames = 0;
    bool m_cursorResetPending = false;
    bool m_previousRightDown = false;
    bool m_panPreviousDown = false;
    LARGE_INTEGER m_previousTime = {};
    bool m_gamePositionWasActive = false;
    bool m_visualViewportActive = false;
    bool m_visualWorldProjectionPass = false;
    D3DVIEWPORT9 m_visualViewportSaved = {};
    D3DMATRIX m_visualProjectionSaved = {};
    D3DMATRIX m_visualRwProjectionSaved = {};
    D3DVIEWPORT9 m_visualViewportApplied = {};
    D3DMATRIX m_visualProjectionApplied = {};
    bool m_visualRayStateValid = false;
};

extern FreeCamera* g_freeCamera;
