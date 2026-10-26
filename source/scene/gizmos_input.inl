// Source Repository: https://github.com/waverso/waversomapstudio

static void UpdateDragCtrlState(bool ctrlHeld, bool& previousCtrl, bool& movedSinceCtrl)
{
    if (ctrlHeld != previousCtrl)
    {
        previousCtrl = ctrlHeld;
        movedSinceCtrl = false;
    }
}

static bool CanUpdateDrag(bool ctrlHeld, bool pointerMoved, bool& movedSinceCtrl)
{
    movedSinceCtrl |= ctrlHeld && pointerMoved;
    return !ctrlHeld || movedSinceCtrl;
}

void Gizmos::ProcessInput()
{
    g_mInputDone = false;
    g_mAnchorLock = false;
    if (!m_device || !g_freeCamera->m_active || !m_visible)
    {
        return;
    }
    const bool wasDragging = m_dragging;
    g_mAnchorPosition = m_position;
    g_mAnchorQuat = GetVisualQuat();
    g_mAnchorArcAngle = wasDragging ? g_mDragArcAngle : 0.f;
    DoInput(BuildFrame(g_mAnchorPosition, g_mAnchorQuat));
    g_mAnchorLock = wasDragging || m_dragging;
    g_mInputDone = true;
}

void Gizmos::DoInput(const Frame& f)
{
    if (!m_enabled || m_mode == GizmoMode::None)
    {
        m_hover = m_dragPart = Part::None;
        m_dragging = m_radiusDragging = m_boxAreaDragging = false;
        g_mDragArcAngle = 0.0f;
        m_radiusHover = m_radiusDragHandle = m_boxAreaHover = m_boxAreaFace = -1;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const bool lbPressed = SceneInput::IsMouseClicked(ImGuiMouseButton_Left);
    const bool mouseBlocked
        = (g_gui && g_gui->IsDragCursorInputBlocked()) || (io.MouseDown[0] && io.MouseDownOwned[0]) || ImGui::IsAnyItemActive();
    UpdateDragCtrlState(io.KeyCtrl, g_mDragCtrlHeld, g_mDragMovedSinceCtrl);
    if (mouseBlocked && !m_dragging && !m_radiusDragging && !m_boxAreaDragging)
    {
        m_hover = Part::None;
        m_radiusHover = m_boxAreaHover = -1;
        Curves::ClearHover();
        return;
    }
    D3DVIEWPORT9 vp;
    m_device->GetViewport(&vp);
    D3DXMATRIX view, proj;
    m_device->GetTransform(D3DTS_VIEW, &view);
    m_device->GetTransform(D3DTS_PROJECTION, &proj);

    D3DXVECTOR3 mn((float)io.MousePos.x, (float)io.MousePos.y, 0.f);
    D3DXVECTOR3 mfar((float)io.MousePos.x, (float)io.MousePos.y, 1.f);
    D3DXVec3Unproject(&mn, &mn, &vp, &proj, &view, nullptr);
    D3DXVec3Unproject(&mfar, &mfar, &vp, &proj, &view, nullptr);
    const D3DXVECTOR3 ro = mn;
    D3DXVECTOR3 rd = mfar - mn;
    D3DXVec3Normalize(&rd, &rd);

    auto RayPlaneHit = [&](const D3DXVECTOR3& rayO, const D3DXVECTOR3& rayD, const D3DXVECTOR3& normal, const D3DXVECTOR3& origin,
                           D3DXVECTOR3& outHit) -> bool
    {
        float denom = D3DXVec3Dot(&rayD, &normal);
        if (fabsf(denom) < 1e-4f)
        {
            return false;
        }
        D3DXVECTOR3 pv = origin - rayO;
        outHit = rayO + rayD * (D3DXVec3Dot(&pv, &normal) / denom);
        return true;
    };

    auto RayFromScreen = [&](float sx, float sy, D3DXVECTOR3& outRo, D3DXVECTOR3& outRd)
    {
        D3DXVECTOR3 a(sx, sy, 0.f), b(sx, sy, 1.f);
        D3DXVec3Unproject(&a, &a, &vp, &proj, &view, nullptr);
        D3DXVec3Unproject(&b, &b, &vp, &proj, &view, nullptr);
        outRo = a;
        outRd = b - a;
        D3DXVec3Normalize(&outRd, &outRd);
    };

    auto AxisFacingPlaneN = [&](const D3DXVECTOR3& axis) -> D3DXVECTOR3
    {
        D3DXVECTOR3 n = f.toCam - axis * D3DXVec3Dot(&f.toCam, &axis);
        if (D3DXVec3LengthSq(&n) < 1e-4f)
        {
            D3DXVECTOR3 up(0.f, 0.f, 1.f);
            if (fabsf(D3DXVec3Dot(&axis, &up)) > 0.99f)
            {
                up = D3DXVECTOR3(1.f, 0.f, 0.f);
            }
            D3DXVec3Cross(&n, &axis, &up);
        }
        D3DXVECTOR3 tmp;
        D3DXVec3Normalize(&tmp, &n);
        return tmp;
    };

    auto TransformAxis = [&](const D3DXVECTOR3& localAxis) -> D3DXVECTOR3
    {
        D3DXVECTOR3 worldAxis;
        D3DXVec3TransformNormal(&worldAxis, &localAxis, &f.visualRot);
        D3DXVECTOR3 tmp;
        D3DXVec3Normalize(&tmp, &worldAxis);
        return tmp;
    };

    auto RadiusHandlePos = [&](int i) -> D3DXVECTOR3
    {
        D3DXVECTOR3 c(m_radiusCenter.x, m_radiusCenter.y, m_radiusCenter.z);
        return c + g_kRadiusAxis[i] * m_radiusValue;
    };
    if (m_boxAreaActive && m_boxAreaDragging)
    {
        Curves::ClearHover();
        if (!io.MouseDown[0] || mouseBlocked || g_freeCamera->m_moving || io.WantCaptureKeyboard || g_freeCamera->m_panHasAnchor)
        {
            m_boxAreaDragging = false;
            m_boxAreaFace = -1;
            return;
        }
        const ImVec2 delta(io.MousePos.x - m_boxAreaPreviousMouse.x, io.MousePos.y - m_boxAreaPreviousMouse.y);
        m_boxAreaPreviousMouse = io.MousePos;
        const bool pointerMoved = HasAxisMovement(delta, m_boxAreaFaceStart, m_boxAreaAxis * f.scale, vp, proj, view);
        if (!CanUpdateDrag(io.KeyCtrl, pointerMoved, g_mDragMovedSinceCtrl))
        {
            return;
        }
        const ImVec2 cursor(io.MousePos.x - m_boxAreaCursorOffset.x, io.MousePos.y - m_boxAreaCursorOffset.y);
        D3DXVECTOR3 hit;
        if (!TranslationCursorHit(cursor, m_boxAreaFaceStart, m_boxAreaPlaneNormal, m_boxAreaAxis * f.scale, vp, proj, view, hit))
        {
            return;
        }
        const D3DXVECTOR3 offset = hit - m_boxAreaFaceStart;
        float value = m_boxAreaStartValue + D3DXVec3Dot(&offset, &m_boxAreaAxis);
        if (io.KeyCtrl && m_translationGridSnap > 0.0f)
        {
            value = floorf(value / m_translationGridSnap + 0.5f) * m_translationGridSnap;
        }
        const float center = SetBoxAreaFace(m_boxAreaValue, m_boxAreaFace, value, m_boxAreaOpposite);
        if (m_boxAreaFace % 3 == 0)
        {
            m_boxAreaPosition.x = center;
        }
        else if (m_boxAreaFace % 3 == 1)
        {
            m_boxAreaPosition.y = center;
        }
        else
        {
            m_boxAreaPosition.z = center;
        }
        return;
    }

    if (m_radiusActive && m_radiusDragging)
    {
        Curves::ClearHover();
        if (!io.MouseDown[0] || mouseBlocked || g_freeCamera->m_moving || io.WantCaptureKeyboard || g_freeCamera->m_panHasAnchor)
        {
            m_radiusDragging = false;
            m_radiusDragHandle = -1;
            return;
        }
        D3DXVECTOR3 c(m_radiusCenter.x, m_radiusCenter.y, m_radiusCenter.z);
        D3DXVECTOR3 ad = g_kRadiusAxis[m_radiusDragHandle];
        const ImVec2 mouseDelta(io.MousePos.x - g_mDragMousePrevious.x, io.MousePos.y - g_mDragMousePrevious.y);
        g_mDragMousePrevious = io.MousePos;
        const bool pointerMoved = HasAxisMovement(mouseDelta, c + ad * m_radiusValue, ad * f.scale, vp, proj, view);
        if (!CanUpdateDrag(io.KeyCtrl, pointerMoved, g_mDragMovedSinceCtrl))
        {
            return;
        }
        D3DXVECTOR3 ro2, rd2;
        RayFromScreen(io.MousePos.x - m_radiusAnchor.x, io.MousePos.y - m_radiusAnchor.y, ro2, rd2);
        D3DXVECTOR3 w0 = ro2 - c;
        float b = D3DXVec3Dot(&rd2, &ad);
        float dpar = D3DXVec3Dot(&rd2, &w0);
        float epar = D3DXVec3Dot(&ad, &w0);
        float denom = 1.f - b * b;
        if (fabsf(denom) > 1e-5f)
        {
            float tc = (epar - b * dpar) / denom;
            if (tc < RadiusMinVal)
            {
                tc = RadiusMinVal;
            }
            if (tc > m_radiusMax)
            {
                tc = m_radiusMax;
            }
            m_radiusValue = tc;
        }
        return;
    }

    if (!m_dragging)
    {
        const bool guiBlocked = g_gui
            && (g_gui->m_sidebarInteracting || g_gui->IsMouseOverGizmoPanel() || g_gui->IsMouseOverUtilityPanel()
                || g_gui->IsMouseOverToolbar());
        if ((io.MouseDown[0] && !lbPressed) || g_freeCamera->m_moving || io.WantCaptureMouse || io.WantCaptureKeyboard
            || g_freeCamera->m_panHasAnchor || guiBlocked)
        {
            m_hover = Part::None;
            m_radiusHover = m_boxAreaHover = -1;
            Curves::ClearHover();
        }
        else
        {
            float minDist = FLT_MAX;
            Part closestPart = Part::None;

            auto TestMesh = [&](const D3D9ProceduralMesh& mesh, const D3DXMATRIX& world, Part part)
            {
                D3DXMATRIX inv;
                D3DXMatrixInverse(&inv, nullptr, &world);
                D3DXVECTOR3 lo, ld;
                D3DXVec3TransformCoord(&lo, &ro, &inv);
                D3DXVec3TransformNormal(&ld, &rd, &inv);
                D3DXVec3Normalize(&ld, &ld);

                float dist = 0.f;
                DWORD fi = 0;
                if (mesh.Intersect(lo, ld, dist, &fi))
                {
                    D3DXVECTOR3 localHit = lo + ld * dist;
                    D3DXVECTOR3 worldHit;
                    D3DXVec3TransformCoord(&worldHit, &localHit, &world);

                    D3DXVECTOR3 pv = worldHit - ro;
                    float d = D3DXVec3Length(&pv);
                    if (d < minDist)
                    {
                        minDist = d;
                        closestPart = part;
                    }
                }
            };

            if (m_rotationEnabled && f.fadeAlpha >= GizmoInteractionMinAlpha
                && (m_mode == GizmoMode::Rotation || m_mode == GizmoMode::Both))
            {
                TestMesh(m_rotationTorusCollisionMesh, f.billRot * f.matBigST, Part::RingOuter);
                if (m_rotationXyEnabled)
                {
                    TestMesh(m_rotationTorusCollisionMesh, f.rotX * f.matST, Part::RingX);
                    TestMesh(m_rotationTorusCollisionMesh, f.rotY * f.matST, Part::RingY);
                }
                TestMesh(m_rotationTorusCollisionMesh, f.rotZ * f.matST, Part::RingZ);
                TestMesh(m_rotationSphereMesh, f.matSTNoRot, Part::RingInner);
                if (m_mode == GizmoMode::Both)
                {
                    if (closestPart == Part::RingInner)
                    {
                        minDist = FLT_MAX;
                    }
                    else if (closestPart == Part::RingOuter)
                    {
                        minDist *= 1.2f;
                    }
                    else if (closestPart != Part::None)
                    {
                        minDist *= 1.03f;
                    }
                }
            }
            if (f.fadeAlpha >= GizmoInteractionMinAlpha && (m_mode == GizmoMode::Position || m_mode == GizmoMode::Both))
            {
                D3DXMATRIX mv;
                TestMesh(m_positionCenterSphereCollisionMesh, f.matST, Part::SphereCenter);
                if (closestPart == Part::SphereCenter)
                {
                    minDist *= 0.5f;
                }

                D3DXMatrixTranslation(&mv, 0.f, 0.f, 0.5f);
                TestMesh(m_positionAxisCollisionMesh, mv * f.rotX * f.matST, Part::AxisX);
                TestMesh(m_positionAxisCollisionMesh, mv * f.rotY * f.matST, Part::AxisY);
                TestMesh(m_positionAxisCollisionMesh, mv * f.rotZ * f.matST, Part::AxisZ);

                const float qa = D3DXToRadian(90.f);
                D3DXMATRIX rXY, rXZ, rYZ;
                D3DXMatrixRotationZ(&rXY, qa);
                D3DXMatrixTranslation(&mv, QuadOffset, QuadOffset, 0.f);
                TestMesh(m_positionAxisQuadCollisionMesh, rXY * mv * f.matST, Part::QuadXY);
                D3DXMatrixRotationX(&rXZ, qa);
                D3DXMatrixTranslation(&mv, QuadOffset, 0.f, QuadOffset);
                TestMesh(m_positionAxisQuadCollisionMesh, rXZ * mv * f.matST, Part::QuadXZ);
                D3DXMatrixRotationY(&rYZ, qa);
                D3DXMatrixTranslation(&mv, 0.f, QuadOffset, QuadOffset);
                TestMesh(m_positionAxisQuadCollisionMesh, rYZ * mv * f.matST, Part::QuadYZ);
            }
            int rHover = -1;
            if (m_radiusActive && RadiusScreenPx(f) >= RadiusMinPxHandles)
            {
                float rBest = FLT_MAX;
                for (int i = 0; i < 6; ++i)
                {
                    D3DXVECTOR3 hp = RadiusHandlePos(i);
                    if (HitTestHandle({ hp.x, hp.y, hp.z }, { ro.x, ro.y, ro.z }, { rd.x, rd.y, rd.z }, rBest))
                    {
                        rHover = i;
                    }
                }
            }
            m_radiusHover = rHover;
            if (rHover >= 0)
            {
                closestPart = Part::None;
            }
            m_hover = closestPart;

            m_boxAreaHover = -1;
            if (m_boxAreaActive)
            {
                float best = FLT_MAX;
                for (int face = 0; face < 6; ++face)
                {
                    const D3DXVECTOR3 point = BoxAreaFacePosition(face);
                    if (HitTestHandle({ point.x, point.y, point.z }, { ro.x, ro.y, ro.z }, { rd.x, rd.y, rd.z }, best))
                    {
                        m_boxAreaHover = face;
                    }
                }
                if (m_boxAreaHover >= 0)
                {
                    m_hover = Part::None;
                    m_radiusHover = -1;
                }
            }
            if (m_hover != Part::None || m_radiusHover >= 0 || m_boxAreaHover >= 0)
            {
                if (m_radiusHover < 0 && m_boxAreaHover < 0
                    && Curves::UpdateHover({ ro.x, ro.y, ro.z }, { rd.x, rd.y, rd.z }, true))
                {
                    m_hover = Part::None;
                }
                else
                {
                    Curves::ClearHover();
                }
            }
            if (lbPressed && m_boxAreaHover >= 0)
            {
                m_boxAreaFace = m_boxAreaHover;
                const int axis = m_boxAreaFace % 3;
                m_boxAreaAxis = D3DXVECTOR3(axis == 0 ? 1.0f : 0.0f, axis == 1 ? 1.0f : 0.0f, axis == 2 ? 1.0f : 0.0f);
                m_boxAreaFaceStart = BoxAreaFacePosition(m_boxAreaFace);
                const D3DXVECTOR3 toCamera = f.camPos - m_boxAreaFaceStart;
                m_boxAreaPlaneNormal = toCamera - m_boxAreaAxis * D3DXVec3Dot(&toCamera, &m_boxAreaAxis);
                if (D3DXVec3LengthSq(&m_boxAreaPlaneNormal) < 1e-6f)
                {
                    return;
                }
                D3DXVec3Normalize(&m_boxAreaPlaneNormal, &m_boxAreaPlaneNormal);
                D3DXVECTOR3 screen;
                D3DXVec3Project(&screen, &m_boxAreaFaceStart, &vp, &proj, &view, nullptr);
                m_boxAreaCursorOffset = ImVec2(io.MousePos.x - screen.x, io.MousePos.y - screen.y);
                m_boxAreaPreviousMouse = io.MousePos;
                g_mDragMovedSinceCtrl = false;
                m_boxAreaStartValue = D3DXVec3Dot(&m_boxAreaFaceStart, &m_boxAreaAxis);
                const D3DXVECTOR3 opposite = BoxAreaFacePosition((m_boxAreaFace + 3) % 6);
                m_boxAreaOpposite = D3DXVec3Dot(&opposite, &m_boxAreaAxis);
                m_boxAreaDragging = true;
            }
            else if (lbPressed && m_radiusHover >= 0)
            {
                m_radiusDragging = true;
                g_mDragMousePrevious = io.MousePos;
                g_mDragMovedSinceCtrl = false;
                m_radiusDragHandle = m_radiusHover;
                m_radiusStart = m_radiusValue;
                D3DXVECTOR3 hp = RadiusHandlePos(m_radiusHover);
                D3DXVECTOR3 hpScr;
                D3DXVec3Project(&hpScr, &hp, &vp, &proj, &view, nullptr);
                m_radiusAnchor = ImVec2(io.MousePos.x - hpScr.x, io.MousePos.y - hpScr.y);
            }
            else if (lbPressed && f.fadeAlpha >= GizmoInteractionMinAlpha && m_hover != Part::None)
            {
                m_dragging = true;
                m_dragPart = m_hover;
                g_mDragPlaneOrigin = f.renderPos;
                g_mDragArcAngle = 0.f;
                g_mDragAnglePrevious = 0.f;

                D3DXVECTOR3 pv = D3DXVECTOR3(m_position.x, m_position.y, m_position.z) - f.camPos;
                float pvLen = D3DXVec3Length(&pv);
                g_mDragCameraDistanceStart = pvLen;

                g_mDragQuatStart = m_quat;
                g_mDragMouseStart = io.MousePos;
                g_mDragMousePrevious = io.MousePos;
                g_mDragMovedSinceCtrl = false;
                g_mDragPositionStart = m_position;
                g_mDragScaleStart = f.scale;

                if (m_dragPart == Part::SphereCenter)
                {
                    g_mDragPlaneN = f.toCam;
                    g_mDragAxis = D3DXVECTOR3(0.f, 0.f, 0.f);
                }
                else if (m_dragPart == Part::AxisX)
                {
                    g_mDragAxis = TransformAxis(D3DXVECTOR3(1.f, 0.f, 0.f));
                    g_mDragPlaneN = AxisFacingPlaneN(g_mDragAxis);
                }
                else if (m_dragPart == Part::AxisY)
                {
                    g_mDragAxis = TransformAxis(D3DXVECTOR3(0.f, 1.f, 0.f));
                    g_mDragPlaneN = AxisFacingPlaneN(g_mDragAxis);
                }
                else if (m_dragPart == Part::AxisZ)
                {
                    g_mDragAxis = TransformAxis(D3DXVECTOR3(0.f, 0.f, 1.f));
                    g_mDragPlaneN = AxisFacingPlaneN(g_mDragAxis);
                }
                else if (m_dragPart == Part::QuadXY)
                {
                    g_mDragPlaneN = TransformAxis(D3DXVECTOR3(0.f, 0.f, 1.f));
                    g_mDragAxis = D3DXVECTOR3(0.f, 0.f, 0.f);
                }
                else if (m_dragPart == Part::QuadXZ)
                {
                    g_mDragPlaneN = TransformAxis(D3DXVECTOR3(0.f, 1.f, 0.f));
                    g_mDragAxis = D3DXVECTOR3(0.f, 0.f, 0.f);
                }
                else if (m_dragPart == Part::QuadYZ)
                {
                    g_mDragPlaneN = TransformAxis(D3DXVECTOR3(1.f, 0.f, 0.f));
                    g_mDragAxis = D3DXVECTOR3(0.f, 0.f, 0.f);
                }
                else if (m_dragPart == Part::RingX)
                {
                    g_mDragAxis = TransformAxis(D3DXVECTOR3(1.f, 0.f, 0.f));
                    g_mDragPlaneN = g_mDragAxis;
                }
                else if (m_dragPart == Part::RingY)
                {
                    g_mDragAxis = TransformAxis(D3DXVECTOR3(0.f, 1.f, 0.f));
                    g_mDragPlaneN = g_mDragAxis;
                }
                else if (m_dragPart == Part::RingZ)
                {
                    g_mDragAxis = TransformAxis(D3DXVECTOR3(0.f, 0.f, 1.f));
                    g_mDragPlaneN = g_mDragAxis;
                }
                else if (m_dragPart == Part::RingOuter)
                {
                    g_mDragAxis = f.toCam;
                    g_mDragPlaneN = f.toCam;
                }
                else if (m_dragPart == Part::RingInner)
                {
                    g_mDragPlaneN = f.toCam;
                    g_mDragAxis = D3DXVECTOR3(0.f, 0.f, 0.f);
                }

                if (m_dragPart >= Part::RingX && m_dragPart <= Part::RingOuter)
                {
                    D3DXVECTOR3 hit;
                    if (RayPlaneHit(ro, rd, g_mDragPlaneN, g_mDragPlaneOrigin, hit))
                    {
                        D3DXVECTOR3 v = hit - g_mDragPlaneOrigin;
                        D3DXVec3Normalize(&g_mDragRotationStart, &v);
                    }
                    else
                    {
                        g_mDragRotationStart = D3DXVECTOR3(0.f, 0.f, 0.f);
                    }
                }
                else if (m_dragPart >= Part::AxisX && m_dragPart <= Part::QuadYZ)
                {
                    D3DXVECTOR3 hit;
                    if (f.scale <= 0.0f
                        || !TranslationCursorHit(
                            io.MousePos, f.renderPos, g_mDragPlaneN, g_mDragAxis * f.scale, vp, proj, view, hit))
                    {
                        m_dragging = false;
                        m_dragPart = Part::None;
                        return;
                    }
                    g_mDragTranslationAnchor = (hit - f.renderPos) / f.scale;
                }
                else
                {
                    g_mDragPlaneOrigin = D3DXVECTOR3(m_position.x, m_position.y, m_position.z);
                    D3DXVECTOR3 startScreen;
                    D3DXVec3Project(&startScreen, &g_mDragPlaneOrigin, &vp, &proj, &view, nullptr);
                    g_mDragScreenOffset = ImVec2(io.MousePos.x - startScreen.x, io.MousePos.y - startScreen.y);
                }
            }
        }
    }
    else
    {
        Curves::ClearHover();
        if (!io.MouseDown[0] || mouseBlocked || g_freeCamera->m_moving || io.WantCaptureKeyboard || g_freeCamera->m_panHasAnchor)
        {
            if (m_space == GizmoSpace::Global)
            {
                D3DXQUATERNION combined;
                D3DXQuaternionMultiply(&combined, &m_quat, &m_dragVisQuat);
                m_quat = combined;
                D3DXQuaternionNormalize(&m_quat, &m_quat);
                D3DXQuaternionIdentity(&m_dragVisQuat);
            }
            m_dragging = false;
            m_dragPart = Part::None;
            return;
        }

        const ImVec2 mouseDelta(io.MousePos.x - g_mDragMousePrevious.x, io.MousePos.y - g_mDragMousePrevious.y);
        g_mDragMousePrevious = io.MousePos;
        bool pointerMoved = mouseDelta.x != 0.0f || mouseDelta.y != 0.0f;
        if (m_dragPart >= Part::AxisX && m_dragPart <= Part::AxisZ)
        {
            pointerMoved = HasAxisMovement(mouseDelta, g_mDragPlaneOrigin, g_mDragAxis * g_mDragScaleStart, vp, proj, view);
        }

        if (m_dragPart == Part::SphereCenter || m_dragPart == Part::AxisX || m_dragPart == Part::AxisY
            || m_dragPart == Part::AxisZ || m_dragPart == Part::QuadXY || m_dragPart == Part::QuadXZ
            || m_dragPart == Part::QuadYZ)
        {
            if (!CanUpdateDrag(io.KeyCtrl, pointerMoved, g_mDragMovedSinceCtrl))
            {
                return;
            }
            D3DXVECTOR3 startV(g_mDragPositionStart.x, g_mDragPositionStart.y, g_mDragPositionStart.z);

            D3DXVECTOR3 hit;
            if (m_dragPart == Part::SphereCenter)
            {
                D3DXVECTOR3 adjRo, adjRd;
                RayFromScreen(io.MousePos.x - g_mDragScreenOffset.x, io.MousePos.y - g_mDragScreenOffset.y, adjRo, adjRd);
                if (!RayPlaneHit(adjRo, adjRd, g_mDragPlaneN, g_mDragPlaneOrigin, hit))
                {
                    return;
                }
            }
            else
            {
                D3DXVECTOR3 cursorHit;
                if (!TranslationCursorHit(
                        io.MousePos, startV, g_mDragPlaneN, g_mDragAxis * g_mDragScaleStart, vp, proj, view, cursorHit)
                    || !ResolveTranslationAnchor(cursorHit, g_mDragTranslationAnchor, f.camPos, f.scale / f.camDist, hit))
                {
                    return;
                }
            }

            D3DXVECTOR3 rawDelta = hit - startV;
            D3DXVECTOR3 projDelta(0.f, 0.f, 0.f);

            if (m_dragPart == Part::SphereCenter)
            {
                projDelta = rawDelta;
            }
            else if (m_dragPart == Part::AxisX || m_dragPart == Part::AxisY || m_dragPart == Part::AxisZ)
            {
                float pr = D3DXVec3Dot(&rawDelta, &g_mDragAxis);
                projDelta = g_mDragAxis * pr;
            }
            else
            {
                float pn = D3DXVec3Dot(&rawDelta, &g_mDragPlaneN);
                projDelta = rawDelta - g_mDragPlaneN * pn;
            }
            D3DXVECTOR3 virtualWorld = startV + projDelta;

            D3DXVECTOR3 fromCam = virtualWorld - f.camPos;
            float forwardDist = D3DXVec3Dot(&fromCam, &f.camForward);
            float distV = D3DXVec3Length(&fromCam);

            float limitVar = std::max(MaxDistance, g_mDragCameraDistanceStart);
            float limitOuter = limitVar * 1.25f;

            const float minFront = 0.25f;
            const float frontFalloff = 0.75f;

            float mult = 1.f;
            if (forwardDist < minFront)
            {
                mult = 0.f;
            }
            else if (forwardDist < minFront + frontFalloff)
            {
                mult = (forwardDist - minFront) / frontFalloff;
            }

            if (distV >= limitOuter)
            {
                mult = 0.f;
            }
            else if (distV > limitVar)
            {
                float distMult = 1.f - (distV - limitVar) / (limitOuter - limitVar);
                if (distMult < mult)
                {
                    mult = distMult;
                }
            }

            D3DXVECTOR3 finalDelta = projDelta * mult;

            CVector newPos = g_mDragPositionStart;
            newPos.x += finalDelta.x;
            newPos.y += finalDelta.y;
            newPos.z += finalDelta.z;

            if (io.KeyCtrl && m_translationGridSnap > 0.f)
            {
                const float g = m_translationGridSnap;
                auto SnapVal = [g](float v)
                {
                    return floorf(v / g + 0.5f) * g;
                };

                if (m_space == GizmoSpace::Global)
                {
                    if (m_dragPart == Part::AxisX)
                    {
                        newPos.x = SnapVal(newPos.x);
                    }
                    else if (m_dragPart == Part::AxisY)
                    {
                        newPos.y = SnapVal(newPos.y);
                    }
                    else if (m_dragPart == Part::AxisZ)
                    {
                        newPos.z = SnapVal(newPos.z);
                    }
                    else if (m_dragPart == Part::QuadXY)
                    {
                        newPos.x = SnapVal(newPos.x);
                        newPos.y = SnapVal(newPos.y);
                    }
                    else if (m_dragPart == Part::QuadXZ)
                    {
                        newPos.x = SnapVal(newPos.x);
                        newPos.z = SnapVal(newPos.z);
                    }
                    else if (m_dragPart == Part::QuadYZ)
                    {
                        newPos.y = SnapVal(newPos.y);
                        newPos.z = SnapVal(newPos.z);
                    }
                    else if (m_dragPart == Part::SphereCenter)
                    {
                        newPos.x = SnapVal(newPos.x);
                        newPos.y = SnapVal(newPos.y);
                        newPos.z = SnapVal(newPos.z);
                    }
                }
                else
                {
                    D3DXVECTOR3 worldDelta(
                        newPos.x - g_mDragPositionStart.x, newPos.y - g_mDragPositionStart.y, newPos.z - g_mDragPositionStart.z);
                    D3DXMATRIX invRot;
                    D3DXMatrixTranspose(&invRot, &f.visualRot);
                    D3DXVECTOR3 localDelta;
                    D3DXVec3TransformNormal(&localDelta, &worldDelta, &invRot);

                    if (m_dragPart == Part::AxisX)
                    {
                        localDelta.x = SnapVal(localDelta.x);
                    }
                    else if (m_dragPart == Part::AxisY)
                    {
                        localDelta.y = SnapVal(localDelta.y);
                    }
                    else if (m_dragPart == Part::AxisZ)
                    {
                        localDelta.z = SnapVal(localDelta.z);
                    }
                    else if (m_dragPart == Part::QuadXY)
                    {
                        localDelta.x = SnapVal(localDelta.x);
                        localDelta.y = SnapVal(localDelta.y);
                    }
                    else if (m_dragPart == Part::QuadXZ)
                    {
                        localDelta.x = SnapVal(localDelta.x);
                        localDelta.z = SnapVal(localDelta.z);
                    }
                    else if (m_dragPart == Part::QuadYZ)
                    {
                        localDelta.y = SnapVal(localDelta.y);
                        localDelta.z = SnapVal(localDelta.z);
                    }
                    else if (m_dragPart == Part::SphereCenter)
                    {
                        localDelta.x = SnapVal(localDelta.x);
                        localDelta.y = SnapVal(localDelta.y);
                        localDelta.z = SnapVal(localDelta.z);
                    }

                    D3DXVECTOR3 worldSnap;
                    D3DXVec3TransformNormal(&worldSnap, &localDelta, &f.visualRot);
                    newPos = g_mDragPositionStart;
                    newPos.x += worldSnap.x;
                    newPos.y += worldSnap.y;
                    newPos.z += worldSnap.z;
                }
            }

            m_position = newPos;
        }
        else if (m_dragPart == Part::RingX || m_dragPart == Part::RingY || m_dragPart == Part::RingZ
            || m_dragPart == Part::RingOuter)
        {
            D3DXVECTOR3 hit;
            if (!RayPlaneHit(ro, rd, g_mDragPlaneN, g_mDragPlaneOrigin, hit))
            {
                return;
            }

            D3DXVECTOR3 cur = hit - g_mDragPlaneOrigin;
            float lenSq = D3DXVec3LengthSq(&cur);
            if (lenSq < 1e-6f)
            {
                return;
            }
            D3DXVec3Normalize(&cur, &cur);

            if (D3DXVec3LengthSq(&g_mDragRotationStart) < 1e-6f)
            {
                g_mDragRotationStart = cur;
                return;
            }

            D3DXVECTOR3 crossV;
            D3DXVec3Cross(&crossV, &g_mDragRotationStart, &cur);
            float dotV = D3DXVec3Dot(&g_mDragRotationStart, &cur);
            float angle = atan2f(D3DXVec3Dot(&crossV, &g_mDragAxis), dotV);
            const bool rotationMoved = pointerMoved && fabsf(angle - g_mDragAnglePrevious) >= 1e-7f;
            g_mDragAnglePrevious = angle;
            if (!CanUpdateDrag(io.KeyCtrl, rotationMoved, g_mDragMovedSinceCtrl))
            {
                return;
            }

            if (io.KeyCtrl && m_rotationGridSnap > 0.f)
            {
                float snapRad = D3DXToRadian(m_rotationGridSnap);
                if (m_space == GizmoSpace::Local)
                {
                    angle = floorf(angle / snapRad + 0.5f) * snapRad;
                }
                else
                {
                    D3DXQUATERNION qDeltaTry;
                    D3DXQuaternionRotationAxis(&qDeltaTry, &g_mDragAxis, angle);
                    D3DXQUATERNION qFinal;
                    if (m_space == GizmoSpace::Local)
                    {
                        D3DXQuaternionMultiply(&qFinal, &g_mDragQuatStart, &qDeltaTry);
                    }
                    else
                    {
                        D3DXQuaternionMultiply(&qFinal, &m_quat, &qDeltaTry);
                    }
                    D3DXQuaternionNormalize(&qFinal, &qFinal);

                    float fullTwist = ExtractTwistAngleAroundAxis(qFinal, g_mDragAxis);
                    float snappedTwist = floorf(fullTwist / snapRad + 0.5f) * snapRad;
                    angle += (snappedTwist - fullTwist);
                }
            }

            g_mDragArcAngle = angle;
            if (g_mDragArcAngle > D3DX_PI)
            {
                g_mDragArcAngle -= 2.f * D3DX_PI;
            }
            else if (g_mDragArcAngle < -D3DX_PI)
            {
                g_mDragArcAngle += 2.f * D3DX_PI;
            }

            D3DXQUATERNION qDelta;
            D3DXQuaternionRotationAxis(&qDelta, &g_mDragAxis, angle);

            if (m_space == GizmoSpace::Local)
            {
                D3DXQUATERNION qNew;
                D3DXQuaternionMultiply(&qNew, &g_mDragQuatStart, &qDelta);
                D3DXQuaternionNormalize(&m_quat, &qNew);
            }
            else
            {
                m_dragVisQuat = qDelta;
                D3DXQuaternionNormalize(&m_dragVisQuat, &m_dragVisQuat);
            }
        }
        else if (m_dragPart == Part::RingInner)
        {
            if (!CanUpdateDrag(io.KeyCtrl, pointerMoved, g_mDragMovedSinceCtrl))
            {
                return;
            }
            ImVec2 dm = ImVec2(io.MousePos.x - g_mDragMouseStart.x, io.MousePos.y - g_mDragMouseStart.y);

            D3DXMATRIX matViewInv;
            m_device->GetTransform(D3DTS_VIEW, &view);
            D3DXMatrixInverse(&matViewInv, nullptr, &view);
            D3DXVECTOR3 camRight(matViewInv._11, matViewInv._12, matViewInv._13);
            D3DXVECTOR3 camUp(matViewInv._21, matViewInv._22, matViewInv._23);

            float angY = dm.x * FreeRotationSens;
            float angX = dm.y * FreeRotationSens;

            if (io.KeyCtrl && m_rotationGridSnap > 0.f)
            {
                float snapRad = D3DXToRadian(m_rotationGridSnap);
                angY = floorf(angY / snapRad + 0.5f) * snapRad;
                angX = floorf(angX / snapRad + 0.5f) * snapRad;
            }

            D3DXQUATERNION qx, qy, qDelta;
            D3DXQuaternionRotationAxis(&qy, &camUp, angY);
            D3DXQuaternionRotationAxis(&qx, &camRight, angX);
            D3DXQuaternionMultiply(&qDelta, &qx, &qy);

            if (m_space == GizmoSpace::Local)
            {
                D3DXQUATERNION qNew;
                D3DXQuaternionMultiply(&qNew, &g_mDragQuatStart, &qDelta);
                D3DXQuaternionNormalize(&m_quat, &qNew);
            }
            else
            {
                D3DXQuaternionNormalize(&m_dragVisQuat, &qDelta);
            }
        }
    }
}
