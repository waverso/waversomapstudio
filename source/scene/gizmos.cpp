// Source Repository: https://github.com/waverso/waversomapstudio

#include "gizmos.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "../vendor/imgui/imgui.h"
#include "../layout/curves.h"
#include "free_camera.h"
#include "../ui/gui.h"
#include "../hooks/game_hooks.h"
#include "../layout/layout.h"
#include "../rendering/line_renderer.h"
#include "scene.h"
#include "scene_input.h"
#include "selection.h"
#include "../ui/theme.h"
#include "../ui/ui_scale.h"

static constexpr float TorusOuter = 0.75f;
static constexpr float TorusTube = 0.014f;
static constexpr float OuterScale = 1.17f;
static constexpr float QuadOffset = 0.7f;
static constexpr float ConeLength = 0.2f;
static constexpr float ConeCenter = 1.0f;
static constexpr float ConeBaseR = 0.07f;
static constexpr float ShaftLength = ConeCenter - ConeLength * 0.5f;

static constexpr float IndViewNative = 120.f;
static constexpr float IndMarginNative = 16.f;
static constexpr float IndBarHalf = 0.60f;
static constexpr float IndBarR = 0.030f;
static constexpr float IndConeR = 0.10f;
static constexpr float IndConeLength = 0.18f;
static constexpr float IndOrthoExt = 0.95f;
static constexpr float IndCameraDistance = 5.0f;
static constexpr float IndNear = 0.1f;
static constexpr float IndFar = 100.f;
static constexpr float IndLabelGap = 0.22f;
static constexpr float FreeRotationSens = 0.005f;
static constexpr float MaxDistance = 8000.f;
static constexpr float GizmoDisabledAlpha = 0.4f;
static constexpr float GizmoBoundRadius = 1.12f;
static constexpr float GizmoFadeMinDepth = 0.46f;
static constexpr float GizmoFadeMaxDepth = 1.05f;
static constexpr float GizmoInteractionMinAlpha = 0.2f;
static constexpr float GuideHalfLength = 4000.f;
static constexpr DWORD GuideAlpha = 255;
static constexpr DWORD ArcFillAlpha = 70;
static constexpr DWORD ArcEdgeAlpha = 200;
static constexpr int ArcMaxSegs = 120;
static constexpr DWORD GhostSphereAlpha = 90;

static D3DXVECTOR3 g_mDragPlaneN, g_mDragPlaneOrigin, g_mDragAxis;
static D3DXVECTOR3 g_mDragRotationStart;
static D3DXQUATERNION g_mDragQuatStart;
static CVector g_mDragPositionStart;
static ImVec2 g_mDragMouseStart;
static ImVec2 g_mDragMousePrevious;
static bool g_mDragCtrlHeld = false;
static bool g_mDragMovedSinceCtrl = false;
static ImVec2 g_mDragScreenOffset;
static D3DXVECTOR3 g_mDragTranslationAnchor;
static float g_mDragScaleStart;
static float g_mDragCameraDistanceStart;
static float g_mDragArcAngle = 0.f;
static float g_mDragAnglePrevious = 0.f;

static CVector g_mAnchorPosition;
static D3DXQUATERNION g_mAnchorQuat;
static float g_mAnchorArcAngle = 0.f;
static bool g_mAnchorLock = false;
static bool g_mInputDone = false;

static const D3DCOLOR ColRed = D3DCOLOR_XRGB(255, 50, 50);
static const D3DCOLOR ColGreen = D3DCOLOR_XRGB(70, 255, 70);
static const D3DCOLOR ColBlue = D3DCOLOR_XRGB(50, 80, 255);
static const D3DCOLOR ColGray = D3DCOLOR_XRGB(210, 210, 210);
static const D3DCOLOR ColYellow = D3DCOLOR_XRGB(255, 220, 0);

static constexpr float RadiusHandleK = 0.8f;
static constexpr float HandleHitScale = 1.8f;
static constexpr int RadiusMinSegs = 24;
static constexpr int RadiusMaxSegs = 128;
static constexpr float RadiusPxPerSeg = 8.0f;
static constexpr float RadiusMinVal = 0.25f;
static constexpr float RadiusMaxVal = 10000.0f;
static constexpr float RadiusMinPxHandles = 28.0f;
static const D3DCOLOR RadiusColSolid = D3DCOLOR_ARGB(255, 255, 60, 60);
static const D3DCOLOR RadiusColBack = D3DCOLOR_ARGB(120, 255, 60, 60);
static const D3DCOLOR RadiusColHover = D3DCOLOR_ARGB(255, 255, 130, 130);
static const D3DCOLOR RadiusColFill = D3DCOLOR_ARGB(26, 255, 60, 60);
static const D3DXVECTOR3 g_kRadiusAxis[6] = {
    { 1.f, 0.f, 0.f },
    { -1.f, 0.f, 0.f },
    { 0.f, 1.f, 0.f },
    { 0.f, -1.f, 0.f },
    { 0.f, 0.f, 1.f },
    { 0.f, 0.f, -1.f },
};

#include "gizmos_geometry.inl"

Gizmos::Gizmos()
{
    m_device = g_gameHooks->m_device;

    D3DXQuaternionIdentity(&m_quat);
    D3DXQuaternionIdentity(&m_dragVisQuat);

    m_cylinderMesh.CreateCylinder(m_device, TorusTube, TorusTube, ShaftLength, 6, 1);
    m_coneMesh.CreateCylinder(m_device, ConeBaseR, 0.0f, ConeLength, 12, 1);
    m_sphereMesh.CreateSphere(m_device, 0.07f, 14, 14);
    m_rotationSphereMesh.CreateSphere(m_device, TorusOuter, 24, 24);
    m_quadMesh.CreateBox(m_device, 0.16f, 0.16f, 0.005f);
    m_torusMesh.CreateTorus(m_device, TorusTube, TorusOuter, 6, 32);

    m_positionAxisCollisionMesh.CreateCylinder(m_device, 0.12f, 0.12f, 1.16f, 7, 1);
    m_positionCenterSphereCollisionMesh.CreateSphere(m_device, 0.17f, 7, 7);
    m_positionAxisQuadCollisionMesh.CreateBox(m_device, 0.3f, 0.3f, 0.005f);
    m_rotationTorusCollisionMesh.CreateTorus(m_device, TorusTube * 5.f, TorusOuter, 4, 32);
}

Gizmos::~Gizmos()
{
}

void Gizmos::SetGizmoTransformMode(GizmoMode m)
{
    m_mode = m;
}
void Gizmos::SetGizmoSpace(GizmoSpace s)
{
    m_space = s;
    D3DXQuaternionIdentity(&m_dragVisQuat);
}
void Gizmos::SetGizmoPosition(CVector pos)
{
    m_position = pos;
    D3DXQuaternionIdentity(&m_dragVisQuat);
}
void Gizmos::SetGizmoRotation(const Utils::RotationBasis& rotation)
{
    D3DXMATRIX matrix;
    D3DXMatrixIdentity(&matrix);
    matrix._11 = rotation.right.x;
    matrix._12 = rotation.right.y;
    matrix._13 = rotation.right.z;
    matrix._21 = rotation.forward.x;
    matrix._22 = rotation.forward.y;
    matrix._23 = rotation.forward.z;
    matrix._31 = rotation.up.x;
    matrix._32 = rotation.up.y;
    matrix._33 = rotation.up.z;
    D3DXQUATERNION quaternion;
    D3DXQuaternionRotationMatrix(&quaternion, &matrix);
    SetGizmoQuat(quaternion);
}
float Gizmos::GetGizmoScale() const
{
    return m_size / DefaultGizmoSize;
}
void Gizmos::SetGizmoScale(float scale)
{
    m_size = DefaultGizmoSize * std::clamp(scale, MinScale, MaxScale);
}
float Gizmos::GetAxisIndicatorScale() const
{
    return m_axisIndicatorScale;
}
void Gizmos::SetAxisIndicatorScale(float scale)
{
    m_axisIndicatorScale = std::clamp(scale, MinScale, MaxScale);
}

void Gizmos::SetGizmoEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled)
    {
        m_hover = m_dragPart = Part::None;
        m_dragging = false;
        g_mDragArcAngle = 0.0f;
        SetBoxAreaGizmo(false, CVector(), BoxAreaData());
    }
}
void Gizmos::SetRotationXYEnabled(bool enabled)
{
    m_rotationXyEnabled = enabled;
    if (!enabled && (m_hover == Part::RingX || m_hover == Part::RingY))
    {
        m_hover = Part::None;
    }
    if (!enabled && (m_dragPart == Part::RingX || m_dragPart == Part::RingY))
    {
        m_dragging = false;
        m_dragPart = Part::None;
        g_mDragArcAngle = 0.0f;
    }
}

void Gizmos::SetRotationEnabled(bool enabled)
{
    m_rotationEnabled = enabled;
}
void Gizmos::SetGizmoVisible(bool visible)
{
    m_visible = visible;
    if (!visible)
    {
        m_hover = m_dragPart = Part::None;
        m_dragging = false;
        g_mDragArcAngle = 0.0f;
        SetBoxAreaGizmo(false, CVector(), BoxAreaData());
    }
}
void Gizmos::SetSelectionBoundsVisible(bool visible)
{
    m_selectionBoundsVisible = visible;
}
bool Gizmos::IsSelectionBoundsVisible() const
{
    return m_selectionBoundsVisible;
}
bool Gizmos::IsDragging() const
{
    return m_visible && m_enabled && m_dragging;
}
void Gizmos::SetGizmoQuat(const D3DXQUATERNION& q)
{
    m_quat = q;
    D3DXQuaternionNormalize(&m_quat, &m_quat);
    D3DXQuaternionIdentity(&m_dragVisQuat);
}
bool Gizmos::IsHoveringOrDragging() const
{
    return m_visible && m_enabled
        && (m_dragging || m_hover != Part::None || m_radiusDragging || m_radiusHover >= 0 || m_boxAreaDragging
            || m_boxAreaHover >= 0);
}
void Gizmos::SetRadiusGizmo(bool active, const CVector& center, float radius, float maxRadius)
{
    m_radiusActive = active;
    m_radiusMax = std::clamp(maxRadius, RadiusMinVal, RadiusMaxVal);
    if (active)
    {
        m_radiusCenter = center;
        if (!m_radiusDragging)
        {
            m_radiusValue = std::clamp(radius, RadiusMinVal, m_radiusMax);
        }
    }
    else
    {
        m_radiusHover = -1;
        m_radiusDragging = false;
        m_radiusDragHandle = -1;
    }
}
bool Gizmos::IsRadiusDragging() const
{
    return m_visible && m_enabled && m_radiusActive && m_radiusDragging;
}
float Gizmos::GetRadiusValue() const
{
    return m_radiusValue;
}

void Gizmos::SetBoxAreaGizmo(bool active, const CVector& position, const BoxAreaData& area)
{
    m_boxAreaActive = active;
    if (!m_boxAreaDragging)
    {
        m_boxAreaPosition = position;
        m_boxAreaValue = area;
    }
    if (!active)
    {
        m_boxAreaDragging = false;
        m_boxAreaHover = m_boxAreaFace = -1;
    }
}

bool Gizmos::IsBoxAreaDragging() const
{
    return m_visible && m_enabled && m_boxAreaActive && m_boxAreaDragging;
}

const BoxAreaData& Gizmos::GetBoxAreaValue() const
{
    return m_boxAreaValue;
}

CVector Gizmos::GetBoxAreaPosition() const
{
    return m_boxAreaPosition;
}

D3DXQUATERNION Gizmos::GetDragRotationDelta() const
{
    D3DXQUATERNION d;
    D3DXQuaternionIdentity(&d);
    if (!m_dragging)
    {
        return d;
    }
    if (m_space == GizmoSpace::Global)
    {
        return m_dragVisQuat;
    }

    D3DXQUATERNION cs;
    D3DXQuaternionConjugate(&cs, &g_mDragQuatStart);
    D3DXQuaternionMultiply(&d, &cs, &m_quat);
    D3DXQuaternionNormalize(&d, &d);
    return d;
}
void Gizmos::SetTranslationGridSnap(float v)
{
    m_translationGridSnap = v;
}
float Gizmos::GetTranslationGridSnap() const
{
    return m_translationGridSnap;
}
void Gizmos::SetRotationGridSnap(float v)
{
    m_rotationGridSnap = v;
}
float Gizmos::GetRotationGridSnap() const
{
    return m_rotationGridSnap;
}
CVector Gizmos::GetGizmoPosition() const
{
    return m_position;
}
Utils::RotationBasis Gizmos::GetGizmoRotation() const
{
    D3DXMATRIX matrix;
    D3DXMatrixRotationQuaternion(&matrix, &m_quat);
    return { { matrix._11, matrix._12, matrix._13 }, { matrix._21, matrix._22, matrix._23 },
        { matrix._31, matrix._32, matrix._33 } };
}
GizmoMode Gizmos::GetGizmoTransformMode() const
{
    return m_mode;
}
GizmoSpace Gizmos::GetGizmoSpace() const
{
    return m_space;
}
D3DXQUATERNION Gizmos::GetVisualQuat() const
{
    const D3DXQUATERNION q = m_space == GizmoSpace::Local ? m_quat : m_dragVisQuat;
    if (m_rotationXyEnabled)
    {
        return q;
    }
    return EulerToQuat(CVector(0.0f, 0.0f, QuatToEuler(q).z));
}

#include "gizmos_input.inl"
#include "gizmos_render.inl"
#include "gizmos_indicator.inl"
#include "gizmos_bounds.inl"
