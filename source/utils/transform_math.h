// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cmath>
#include <numbers>

#include "CVector.h"

namespace Utils
{
    inline constexpr float TwoPi = 2.0f * std::numbers::pi_v<float>;
    inline constexpr float HalfPi = 0.5f * std::numbers::pi_v<float>;

    static float Dot3(const CVector& a, const CVector& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static CVector CrossProduct(CVector a, CVector b)
    {
        CVector out;
        RwV3d axis {};
        RwV3dCrossProduct(&axis, &a, &b);
        out.x = axis.x, out.y = axis.y, out.z = axis.z;
        return out;
    }

    static float RwLen(const RwV3d& v)
    {
        return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    static CVector ToWorld(const RwMatrix& m, const RwV3d& v)
    {
        return { m.right.x * v.x + m.up.x * v.y + m.at.x * v.z + m.pos.x, m.right.y * v.x + m.up.y * v.y + m.at.y * v.z + m.pos.y,
            m.right.z * v.x + m.up.z * v.y + m.at.z * v.z + m.pos.z };
    }

    struct RotationBasis
    {
        CVector right = CVector(1.0f, 0.0f, 0.0f);
        CVector forward = CVector(0.0f, 1.0f, 0.0f);
        CVector up = CVector(0.0f, 0.0f, 1.0f);
    };

    static RotationBasis EulerDegreesToBasis(const float deg[3])
    {
        constexpr float DegreesToRadians = static_cast<float>(std::numbers::pi / 180.0);
        const float cX = cosf(deg[0] * DegreesToRadians), sX = sinf(deg[0] * DegreesToRadians);
        const float cY = cosf(deg[1] * DegreesToRadians), sY = sinf(deg[1] * DegreesToRadians);
        const float cZ = cosf(deg[2] * DegreesToRadians), sZ = sinf(deg[2] * DegreesToRadians);

        RotationBasis m;
        m.right = CVector(cZ * cY - (sZ * sX) * sY, (cZ * sY) * sX + sZ * cY, -sY * cX);
        m.forward = CVector(-sZ * cX, cZ * cX, sX);
        m.up = CVector((sZ * sX) * cY + cZ * sY, sZ * sY - (cZ * sX) * cY, cY * cX);
        return m;
    }

    static void BasisToEulerDegrees(const RotationBasis& m, float outDeg[3])
    {
        constexpr float RadiansToDegrees = static_cast<float>(180.0 / std::numbers::pi);
        float sX = m.forward.z;
        if (sX > 1.0f)
        {
            sX = 1.0f;
        }
        if (sX < -1.0f)
        {
            sX = -1.0f;
        }
        const float rx = asinf(sX);
        float ry, rz;
        if (fabsf(sX) < 0.9999f)
        {
            rz = atan2f(-m.forward.x, m.forward.y);
            ry = atan2f(-m.right.z, m.up.z);
        }
        else
        {
            rz = atan2f(m.right.y, m.right.x);
            ry = 0.0f;
        }
        outDeg[0] = rx * RadiansToDegrees;
        outDeg[1] = ry * RadiansToDegrees;
        outDeg[2] = rz * RadiansToDegrees;
    }

    static CVector TransformDirection(const RotationBasis& m, const CVector& v)
    {
        return CVector(m.right.x * v.x + m.forward.x * v.y + m.up.x * v.z, m.right.y * v.x + m.forward.y * v.y + m.up.y * v.z,
            m.right.z * v.x + m.forward.z * v.y + m.up.z * v.z);
    }

    static CVector InverseTransformDirection(const RotationBasis& m, const CVector& v)
    {
        return CVector(Dot3(v, m.right), Dot3(v, m.forward), Dot3(v, m.up));
    }

    static RotationBasis ComposeRotationBases(const RotationBasis& p, const RotationBasis& l)
    {
        RotationBasis r;
        r.right = TransformDirection(p, l.right);
        r.forward = TransformDirection(p, l.forward);
        r.up = TransformDirection(p, l.up);
        return r;
    }

    static RotationBasis TransposeRotationBasis(const RotationBasis& m)
    {
        RotationBasis t;
        t.right = CVector(m.right.x, m.forward.x, m.up.x);
        t.forward = CVector(m.right.y, m.forward.y, m.up.y);
        t.up = CVector(m.right.z, m.forward.z, m.up.z);
        return t;
    }
}
