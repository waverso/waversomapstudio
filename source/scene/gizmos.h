// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include "../rendering/d3d9_procedural_mesh.h"
#include "../utils/d3dx_math_compat.h"
#include "../utils/transform_math.h"
#include "../vendor/imgui/imgui.h"
#include "scene_data.h"

enum class GizmoMode
{
    Position,
    Rotation,
    Both,
    None
};
enum class GizmoSpace
{
    Local,
    Global
};

class Gizmos
{
    public:
    static constexpr float MinScale = 0.7f;
    static constexpr float MaxScale = 1.3f;
    static constexpr float DefaultScale = 1.0f;

    class HandleBatch
    {
        public:
        explicit HandleBatch(const Gizmos* gizmos) : m_gizmos(gizmos)
        {
        }
        float Radius(const CVector& position);
        bool HitTest(const CVector& position, const CVector& origin, const CVector& direction, float& distance);

        private:
        const Gizmos* m_gizmos;
        D3DXVECTOR3 m_camera;
        float m_size = 0;
        float m_projectionY = 0;
        bool m_initialized = false;
        bool m_valid = false;
    };

    Gizmos();
    ~Gizmos();

    void ProcessInput();
    void Render();
    void RenderAxisIndicator();
    void DrawAxisIndicatorLabels();
    float GetAxisIndicatorLeft() const;
    float GetGizmoScale() const;
    void SetGizmoScale(float scale);
    float GetAxisIndicatorScale() const;
    void SetAxisIndicatorScale(float scale);
    void SetGizmoTransformMode(GizmoMode mode);
    void SetGizmoSpace(GizmoSpace space);
    void SetGizmoPosition(CVector pos);
    void SetGizmoQuat(const D3DXQUATERNION& q);
    void SetGizmoRotation(const Utils::RotationBasis& rotation);
    float GetHandleWorldRadius(const CVector& position) const;
    bool HitTestHandle(const CVector& position, const CVector& origin, const CVector& direction, float& distance) const;
    void SetGizmoEnabled(bool enabled);
    void SetRotationXYEnabled(bool enabled);
    void SetRotationEnabled(bool enabled);
    void SetGizmoVisible(bool visible);
    void SetSelectionBoundsVisible(bool visible);
    bool IsSelectionBoundsVisible() const;
    bool IsDragging() const;
    D3DXQUATERNION GetDragRotationDelta() const;
    bool IsHoveringOrDragging() const;
    void SetRadiusGizmo(bool active, const CVector& center, float radius, float maxRadius = 10000.0f);
    bool IsRadiusDragging() const;
    float GetRadiusValue() const;
    void SetBoxAreaGizmo(bool active, const CVector& position, const BoxAreaData& area);
    bool IsBoxAreaDragging() const;
    const BoxAreaData& GetBoxAreaValue() const;
    CVector GetBoxAreaPosition() const;
    void SetTranslationGridSnap(float v);
    float GetTranslationGridSnap() const;
    void SetRotationGridSnap(float v);
    float GetRotationGridSnap() const;
    CVector GetGizmoPosition() const;
    Utils::RotationBasis GetGizmoRotation() const;
    GizmoMode GetGizmoTransformMode() const;
    GizmoSpace GetGizmoSpace() const;

    private:
    enum class Part
    {
        None,
        AxisX,
        AxisY,
        AxisZ,
        QuadXY,
        QuadXZ,
        QuadYZ,
        SphereCenter,
        RingX,
        RingY,
        RingZ,
        RingInner,
        RingOuter
    };

    struct Frame
    {
        D3DXVECTOR3 camPos, renderPos, toCam, camForward;
        float scale, camDist, viewDepth, nearestViewDepth, fadeAlpha, vFovHalf;
        D3DXMATRIX matT, matST, matSTNoRot, matBigST, billRot, visualRot;
        D3DXMATRIX rotX, rotY, rotZ;
    };

    Frame BuildFrame(const CVector& pos, const D3DXQUATERNION& qVis) const;
    void DrawMeshCol(const D3D9ProceduralMesh& mesh, const D3DXMATRIX& world, D3DCOLOR col, bool ambient = true);
    D3DCOLOR Hi(Part p, D3DCOLOR base) const;
    D3DCOLOR HiA(Part p, D3DCOLOR base) const;
    void DoRender(const Frame& f);
    void DoInput(const Frame& f);
    void DrawDragGuides(const Frame& f);
    D3DXVECTOR3 RadiusDrawCenter() const;
    float RadiusScreenPx(const Frame& f);
    void RenderRadius(const Frame& f);
    void RenderRadiusHandles(const Frame& f);
    D3DXVECTOR3 BoxAreaFacePosition(int face, bool rendered = false) const;
    void RenderBoxArea();
    void RenderBoxAreaHandles(const Frame& f);
    void RenderSceneBounds();
    D3DXQUATERNION GetVisualQuat() const;

    D3D9ProceduralMesh m_cylinderMesh, m_coneMesh, m_sphereMesh, m_rotationSphereMesh, m_quadMesh, m_torusMesh;
    D3D9ProceduralMesh m_positionAxisCollisionMesh, m_positionCenterSphereCollisionMesh, m_positionAxisQuadCollisionMesh,
        m_rotationTorusCollisionMesh;
    IDirect3DDevice9* m_device = nullptr;

    ImVec2 m_indLabelPosition[3] = {};
    bool m_indLabelValid = false;

    GizmoMode m_mode = GizmoMode::Position;
    GizmoSpace m_space = GizmoSpace::Local;
    CVector m_position = {};
    D3DXQUATERNION m_quat;
    D3DXQUATERNION m_dragVisQuat;
    static constexpr float DefaultGizmoSize = 0.185f;
    float m_size = DefaultGizmoSize;
    float m_axisIndicatorScale = DefaultScale;
    float m_renderAlpha = 1.0f;

    Part m_hover = Part::None, m_dragPart = Part::None;
    bool m_dragging = false;
    bool m_enabled = true;
    bool m_rotationXyEnabled = true;
    bool m_rotationEnabled = true;
    bool m_visible = true;
    bool m_selectionBoundsVisible = false;
    float m_translationGridSnap = 5.0f;
    float m_rotationGridSnap = 45.f;

    bool m_radiusActive = false;
    CVector m_radiusCenter = {};
    float m_radiusValue = 0.0f;
    float m_radiusMax = 10000.0f;
    int m_radiusHover = -1;
    bool m_radiusDragging = false;
    int m_radiusDragHandle = -1;
    ImVec2 m_radiusAnchor = {};
    float m_radiusStart = 0.0f;
    bool m_boxAreaActive = false;
    bool m_boxAreaDragging = false;
    int m_boxAreaHover = -1;
    int m_boxAreaFace = -1;
    CVector m_boxAreaPosition = {};
    BoxAreaData m_boxAreaValue;
    D3DXVECTOR3 m_boxAreaFaceStart, m_boxAreaAxis, m_boxAreaPlaneNormal;
    ImVec2 m_boxAreaCursorOffset, m_boxAreaPreviousMouse;
    float m_boxAreaStartValue = 0.0f;
    float m_boxAreaOpposite = 0.0f;
};

extern Gizmos* g_gizmos;
