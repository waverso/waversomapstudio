// Source Repository: https://github.com/waverso/waversomapstudio

static D3DXQUATERNION EulerToQuat(const CVector& e)
{
    D3DXQUATERNION qx, qy, qz, q;
    D3DXVECTOR3 ax(1.f, 0.f, 0.f), ay(0.f, 1.f, 0.f), az(0.f, 0.f, 1.f);
    D3DXQuaternionRotationAxis(&qx, &ax, D3DXToRadian(e.x));
    D3DXQuaternionRotationAxis(&qy, &ay, D3DXToRadian(e.y));
    D3DXQuaternionRotationAxis(&qz, &az, D3DXToRadian(e.z));
    D3DXQuaternionMultiply(&q, &qy, &qx);
    D3DXQuaternionMultiply(&q, &qz, &q);
    return q;
}

static CVector QuatToEuler(const D3DXQUATERNION& q)
{
    D3DXMATRIX m;
    D3DXMatrixRotationQuaternion(&m, &q);
    float ex, ey, ez;
    float sy = -m._13;
    if (fabsf(sy) < 0.99999f)
    {
        ey = asinf(sy);
        ex = atan2f(m._23, m._33);
        ez = atan2f(m._12, m._11);
    }
    else
    {
        ey = sy > 0.f ? D3DX_PI * 0.5f : -D3DX_PI * 0.5f;
        ex = sy > 0.f ? atan2f(m._21, m._22) : atan2f(-m._21, m._22);
        ez = 0.f;
    }
    return CVector(D3DXToDegree(ex), D3DXToDegree(ey), D3DXToDegree(ez));
}

static float ExtractTwistAngleAroundAxis(const D3DXQUATERNION& q, const D3DXVECTOR3& axis)
{
    D3DXVECTOR3 ax = axis;
    D3DXVec3Normalize(&ax, &ax);
    float proj = q.x * ax.x + q.y * ax.y + q.z * ax.z;
    return 2.f * atan2f(proj, q.w);
}

static int RadiusSegments(float screenRadius)
{
    return static_cast<int>(std::clamp(
        screenRadius * D3DX_PI * 2.0f / RadiusPxPerSeg, static_cast<float>(RadiusMinSegs), static_cast<float>(RadiusMaxSegs)));
}

static void BuildRadiusEdges(LineRenderer::Vertex* edges, const D3DXVECTOR3& center, float radius, int segments, D3DCOLOR color)
{
    static bool ready[RadiusMaxSegs + 1] = {};
    static D3DXVECTOR3 circles[RadiusMaxSegs + 1][RadiusMaxSegs + 1];
    auto& circle = circles[segments];
    if (!ready[segments])
    {
        for (int segment = 1; segment <= segments; ++segment)
        {
            const float angle = D3DX_PI * 2.0f * segment / segments;
            circle[segment] = { cosf(angle), sinf(angle), 0.0f };
        }
        ready[segments] = true;
    }
    int count = 0;
    const auto addCircle = [&](const D3DXVECTOR3& u, const D3DXVECTOR3& v)
    {
        auto previous = center + u * radius;
        for (int segment = 1; segment <= segments; ++segment)
        {
            const auto point = center + (u * circle[segment].x + v * circle[segment].y) * radius;
            edges[count++] = { previous.x, previous.y, previous.z, color };
            edges[count++] = { point.x, point.y, point.z, color };
            previous = point;
        }
    };
    addCircle({ 1, 0, 0 }, { 0, 1, 0 });
    addCircle({ 0, 1, 0 }, { 0, 0, 1 });
    addCircle({ 1, 0, 0 }, { 0, 0, 1 });
}

static D3DXMATRIX MakeGizmoProj(const D3DXMATRIX& gameProj, float nearDist, float farDist)
{
    D3DXMATRIX p = gameProj;
    float zn = std::max(0.02f, nearDist * 0.5f);
    float zf = std::max(zn * 2.0f, farDist * 1.5f);
    if (gameProj._34 >= 0.0f)
    {
        p._33 = zf / (zf - zn);
        p._43 = -zn * zf / (zf - zn);
    }
    else
    {
        p._33 = zf / (zn - zf);
        p._43 = zn * zf / (zn - zf);
    }
    return p;
}

static bool TranslationCursorHit(ImVec2 cursor, const D3DXVECTOR3& origin, const D3DXVECTOR3& normal, const D3DXVECTOR3& axis,
    const D3DVIEWPORT9& viewport, const D3DXMATRIX& projection, const D3DXMATRIX& view, D3DXVECTOR3& hit)
{
    if (D3DXVec3LengthSq(&axis) > 0.0f)
    {
        D3DXVECTOR3 screenOrigin, screenEnd;
        const D3DXVECTOR3 end = origin + axis;
        D3DXVec3Project(&screenOrigin, &origin, &viewport, &projection, &view, nullptr);
        D3DXVec3Project(&screenEnd, &end, &viewport, &projection, &view, nullptr);
        const float dx = screenEnd.x - screenOrigin.x;
        const float dy = screenEnd.y - screenOrigin.y;
        const float lengthSq = dx * dx + dy * dy;
        if (lengthSq < 1e-6f)
        {
            return false;
        }
        const float t = ((cursor.x - screenOrigin.x) * dx + (cursor.y - screenOrigin.y) * dy) / lengthSq;
        cursor = ImVec2(screenOrigin.x + dx * t, screenOrigin.y + dy * t);
    }
    D3DXVECTOR3 rayOrigin(cursor.x, cursor.y, 0.0f);
    D3DXVECTOR3 rayEnd(cursor.x, cursor.y, 1.0f);
    D3DXMATRIX inverseView, identity;
    D3DXMatrixIdentity(&identity);
    if (!D3DXMatrixInverse(&inverseView, nullptr, &view)
        || !D3DXVec3Unproject(&rayOrigin, &rayOrigin, &viewport, &projection, &identity, nullptr)
        || !D3DXVec3Unproject(&rayEnd, &rayEnd, &viewport, &projection, &identity, nullptr))
    {
        return false;
    }
    D3DXVECTOR3 direction = rayEnd - rayOrigin;
    D3DXVec3Normalize(&direction, &direction);
    D3DXVECTOR3 viewOrigin, viewNormal;
    D3DXVec3TransformCoord(&viewOrigin, &origin, &view);
    D3DXVec3TransformNormal(&viewNormal, &normal, &view);
    const float denominator = D3DXVec3Dot(&direction, &viewNormal);
    if (fabsf(denominator) < 1e-4f)
    {
        return false;
    }
    const D3DXVECTOR3 offset = viewOrigin - rayOrigin;
    D3DXVECTOR3 delta = direction * (D3DXVec3Dot(&offset, &viewNormal) / denominator) - offset;
    D3DXVec3TransformNormal(&delta, &delta, &inverseView);
    hit = origin + delta;
    return std::isfinite(hit.x) && std::isfinite(hit.y) && std::isfinite(hit.z);
}

static bool HasAxisMovement(ImVec2 delta, const D3DXVECTOR3& origin, const D3DXVECTOR3& axis, const D3DVIEWPORT9& viewport,
    const D3DXMATRIX& projection, const D3DXMATRIX& view)
{
    D3DXVECTOR3 screenOrigin, screenEnd;
    const D3DXVECTOR3 end = origin + axis;
    D3DXVec3Project(&screenOrigin, &origin, &viewport, &projection, &view, nullptr);
    D3DXVec3Project(&screenEnd, &end, &viewport, &projection, &view, nullptr);
    const float dx = screenEnd.x - screenOrigin.x;
    const float dy = screenEnd.y - screenOrigin.y;
    return fabsf(delta.x * dx + delta.y * dy) > 1e-4f * std::hypot(dx, dy);
}

static bool ResolveTranslationAnchor(
    const D3DXVECTOR3& hit, const D3DXVECTOR3& anchor, const D3DXVECTOR3& camera, float scaleFactor, D3DXVECTOR3& position)
{
    const D3DXVECTOR3 offset = hit - camera;
    const double bx = static_cast<double>(anchor.x) * scaleFactor;
    const double by = static_cast<double>(anchor.y) * scaleFactor;
    const double bz = static_cast<double>(anchor.z) * scaleFactor;
    const double a = 1.0 - bx * bx - by * by - bz * bz;
    if (a <= 1e-6)
    {
        return false;
    }
    const double b = offset.x * bx + offset.y * by + offset.z * bz;
    const double c = static_cast<double>(offset.x) * offset.x + static_cast<double>(offset.y) * offset.y
        + static_cast<double>(offset.z) * offset.z;
    const double distance = (sqrt(b * b + a * c) - b) / a;
    position = D3DXVECTOR3(static_cast<float>(hit.x - bx * distance), static_cast<float>(hit.y - by * distance),
        static_cast<float>(hit.z - bz * distance));
    return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

static float SetBoxAreaFace(BoxAreaData& area, int face, float value, float opposite)
{
    if (face < 0 || face >= 6)
    {
        return opposite;
    }
    const int axis = face % 3;
    const float direction = face < 3 ? -1.0f : 1.0f;
    if (std::isfinite(value))
    {
        if (axis == 2)
        {
            value = std::max(value, TransformData::MinZ * 2 - opposite);
        }
        const double size = direction * (static_cast<double>(value) - opposite);
        area.SetSize(axis,
            static_cast<float>(
                std::clamp(size, static_cast<double>(BoxAreaData::MinSize), static_cast<double>(BoxAreaData::MaxSize))));
    }
    return static_cast<float>(opposite + direction * area.GetSize(axis) * 0.5);
}

static void BuildBoxAreaCorners(const D3DXVECTOR3& center, const BoxAreaData& area, D3DXVECTOR3 (&corners)[8])
{
    for (int corner = 0; corner < 8; ++corner)
    {
        corners[corner] = center
            + D3DXVECTOR3((corner & 1 ? 0.5f : -0.5f) * area.GetSize(0), (corner & 2 ? 0.5f : -0.5f) * area.GetSize(1),
                (corner & 4 ? 0.5f : -0.5f) * area.GetSize(2));
    }
}

float Gizmos::GetHandleWorldRadius(const CVector& position) const
{
    return HandleBatch(this).Radius(position);
}

bool Gizmos::HitTestHandle(const CVector& position, const CVector& origin, const CVector& direction, float& distance) const
{
    return HandleBatch(this).HitTest(position, origin, direction, distance);
}

float Gizmos::HandleBatch::Radius(const CVector& position)
{
    if (!m_initialized)
    {
        m_initialized = true;
        if (!m_gizmos || !m_gizmos->m_device)
        {
            return 0.07f;
        }
        D3DXMATRIX view, inverseView, projection;
        if (FAILED(m_gizmos->m_device->GetTransform(D3DTS_VIEW, &view))
            || FAILED(m_gizmos->m_device->GetTransform(D3DTS_PROJECTION, &projection))
            || !D3DXMatrixInverse(&inverseView, nullptr, &view))
        {
            return 0.07f;
        }
        m_camera = { inverseView._41, inverseView._42, inverseView._43 };
        m_size = m_gizmos->m_size;
        m_projectionY = projection._22;
        m_valid = true;
    }
    if (!m_valid)
    {
        return 0.07f;
    }
    const D3DXVECTOR3 offset(position.x - m_camera.x, position.y - m_camera.y, position.z - m_camera.z);
    const float distance = std::max(0.001f, D3DXVec3Length(&offset));
    return 0.07f * m_size * RadiusHandleK * distance / std::max(0.001f, m_projectionY);
}

bool Gizmos::HandleBatch::HitTest(const CVector& position, const CVector& origin, const CVector& direction, float& distance)
{
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (length < 1e-8f)
    {
        return false;
    }
    const float x = position.x - origin.x, y = position.y - origin.y, z = position.z - origin.z;
    const float along = (x * direction.x + y * direction.y + z * direction.z) / length;
    const float radius = Radius(position) * HandleHitScale;
    const float discriminant = radius * radius - (x * x + y * y + z * z - along * along);
    if (discriminant < 0)
    {
        return false;
    }
    const float root = std::sqrt(discriminant);
    const float hit = along >= root ? along - root : along + root;
    if (hit < 0 || hit >= distance)
    {
        return false;
    }
    distance = hit;
    return true;
}

Gizmos::Frame Gizmos::BuildFrame(const CVector& pos, const D3DXQUATERNION& qVis) const
{
    Frame f;
    D3DXMATRIX matView, matInvView;
    m_device->GetTransform(D3DTS_VIEW, &matView);
    D3DXMatrixInverse(&matInvView, nullptr, &matView);
    D3DXVECTOR3 camPos(matInvView._41, matInvView._42, matInvView._43);
    D3DXVECTOR3 targetPos(pos.x, pos.y, pos.z);
    D3DXVECTOR3 diff = targetPos - camPos;

    if (D3DXVec3LengthSq(&diff) < 1e-6f)
    {
        diff = D3DXVECTOR3(0.f, 1.f, 0.f);
    }

    D3DXVECTOR3 dir;
    D3DXVec3Normalize(&dir, &diff);
    f.toCam = -dir;
    f.camPos = camPos;
    f.renderPos = targetPos;
    f.camDist = std::max(0.001f, D3DXVec3Length(&diff));
    f.camForward = D3DXVECTOR3(matInvView._31, matInvView._32, matInvView._33);
    f.viewDepth = D3DXVec3Dot(&diff, &f.camForward);

    D3DXMATRIX matProj;
    m_device->GetTransform(D3DTS_PROJECTION, &matProj);
    float vFovHalf = atanf(1.0f / matProj._22);
    f.scale = f.camDist * tanf(vFovHalf) * m_size;
    f.vFovHalf = vFovHalf;
    f.nearestViewDepth = f.viewDepth - f.scale * GizmoBoundRadius;
    float fadeT = std::clamp((f.nearestViewDepth - GizmoFadeMinDepth) / (GizmoFadeMaxDepth - GizmoFadeMinDepth), 0.0f, 1.0f);
    f.fadeAlpha = fadeT * fadeT * (3.0f - 2.0f * fadeT);

    D3DXMATRIX matS;
    D3DXMatrixScaling(&matS, f.scale, f.scale, f.scale);
    D3DXMatrixTranslation(&f.matT, f.renderPos.x, f.renderPos.y, f.renderPos.z);

    D3DXMatrixRotationQuaternion(&f.visualRot, &qVis);

    f.matSTNoRot = matS * f.matT;
    f.matST = matS * f.visualRot * f.matT;

    const float bigS = f.scale * OuterScale;
    D3DXMATRIX matBigS;
    D3DXMatrixScaling(&matBigS, bigS, bigS, bigS);
    f.matBigST = matBigS * f.matT;

    D3DXMatrixRotationY(&f.rotX, D3DXToRadian(90.f));
    D3DXMatrixRotationX(&f.rotY, D3DXToRadian(-90.f));
    D3DXMatrixIdentity(&f.rotZ);

    D3DXVECTOR3 worldUp(0.f, 0.f, 1.f);
    if (fabsf(D3DXVec3Dot(&f.toCam, &worldUp)) > 0.99f)
    {
        worldUp = D3DXVECTOR3(1.f, 0.f, 0.f);
    }
    D3DXVECTOR3 bbX, bbY;
    D3DXVec3Cross(&bbX, &worldUp, &f.toCam);
    D3DXVec3Normalize(&bbX, &bbX);
    D3DXVec3Cross(&bbY, &f.toCam, &bbX);
    D3DXVec3Normalize(&bbY, &bbY);
    f.billRot = D3DXMATRIX(
        bbX.x, bbX.y, bbX.z, 0.f, bbY.x, bbY.y, bbY.z, 0.f, f.toCam.x, f.toCam.y, f.toCam.z, 0.f, 0.f, 0.f, 0.f, 1.f);
    return f;
}
