// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <cmath>

#include "../utils/d3dx_math_compat.h"

namespace Races
{
    struct MarkerLighting
    {
        D3DXVECTOR3 ambient, diffuse, direction;
    };

    inline D3DCOLOR LitMarkerColor(
        D3DCOLOR prelight, D3DCOLOR tint, const D3DXVECTOR3& normal, const MarkerLighting& lighting, float ambient, float diffuse)
    {
        const float intensity = (std::max)(0.0f, -D3DXVec3Dot(&normal, &lighting.direction)) * diffuse;
        const auto channel = [&](int shift, float ambientLight, float directLight)
        {
            const float brightness = std::clamp(
                ambientLight * ambient + directLight * intensity + static_cast<float>((prelight >> shift) & 255) / 255.0f, 0.0f,
                1.0f);
            return static_cast<DWORD>(static_cast<float>((tint >> shift) & 255) * brightness);
        };
        return D3DCOLOR_ARGB(tint >> 24, channel(16, lighting.ambient.x, lighting.diffuse.x),
            channel(8, lighting.ambient.y, lighting.diffuse.y), channel(0, lighting.ambient.z, lighting.diffuse.z));
    }

    inline unsigned char MarkerAlpha(unsigned char alpha, float size, float distance)
    {
        const float fraction = distance >= size + 12.0f ? 1.0f
            : distance <= size + 1.0f                   ? 0.65f
                                                        : 1.0f - (size + 12.0f - distance) * (0.35f / 11.0f);
        return static_cast<unsigned char>(static_cast<float>(alpha) * fraction);
    }

    inline D3DXMATRIX FinishTransform(const D3DXVECTOR3& position, const D3DXMATRIX& view, const D3DXMATRIX& projection,
        const D3DVIEWPORT9& viewport, float width, float height, float radius, float fov)
    {
        const float scale = radius * 70.0f / (std::max)(0.001f, fov);
        const float x = scale * width / std::max<DWORD>(1, viewport.Width) / (std::max)(0.001f, std::fabs(projection._11));
        const float y = scale * height / std::max<DWORD>(1, viewport.Height) / (std::max)(0.001f, std::fabs(projection._22));
        D3DXMATRIX matrix;
        D3DXMatrixIdentity(&matrix);
        matrix._11 = view._11 * x;
        matrix._12 = view._21 * x;
        matrix._13 = view._31 * x;
        matrix._21 = view._12 * y;
        matrix._22 = view._22 * y;
        matrix._23 = view._32 * y;
        matrix._31 = view._13;
        matrix._32 = view._23;
        matrix._33 = view._33;
        matrix._41 = position.x;
        matrix._42 = position.y;
        matrix._43 = position.z;
        return matrix;
    }

    inline D3DXMATRIX MarkerTransform(const D3DXVECTOR3& position, const D3DXVECTOR3& normal, float size, bool tube)
    {
        D3DXVECTOR3 up;
        D3DXVec3Normalize(&up, &normal);
        if (D3DXVec3LengthSq(&up) < 0.5f)
        {
            up = D3DXVECTOR3(0, 0, 1);
        }
        const D3DXVECTOR3 reference = std::fabs(up.z) < 0.999f ? D3DXVECTOR3(0, 0, 1) : D3DXVECTOR3(0, 1, 0);
        D3DXVECTOR3 right, forward;
        D3DXVec3Cross(&right, &reference, &up);
        D3DXVec3Normalize(&right, &right);
        D3DXVec3Cross(&forward, &up, &right);
        const float height = tube ? size * 20.0f : size;
        D3DXMATRIX matrix;
        D3DXMatrixIdentity(&matrix);
        matrix._11 = right.x * size;
        matrix._12 = right.y * size;
        matrix._13 = right.z * size;
        matrix._21 = forward.x * size;
        matrix._22 = forward.y * size;
        matrix._23 = forward.z * size;
        matrix._31 = up.x * height;
        matrix._32 = up.y * height;
        matrix._33 = up.z * height;
        matrix._41 = position.x;
        matrix._42 = position.y;
        matrix._43 = position.z;
        return matrix;
    }

    inline bool BoundsVisible(const D3DXVECTOR3& minimum, const D3DXVECTOR3& maximum, const D3DXMATRIX& transform)
    {
        unsigned int outside = 63;
        for (int corner = 0; corner < 8; ++corner)
        {
            const float x = corner & 1 ? maximum.x : minimum.x;
            const float y = corner & 2 ? maximum.y : minimum.y;
            const float z = corner & 4 ? maximum.z : minimum.z;
            const float cx = x * transform._11 + y * transform._21 + z * transform._31 + transform._41;
            const float cy = x * transform._12 + y * transform._22 + z * transform._32 + transform._42;
            const float cz = x * transform._13 + y * transform._23 + z * transform._33 + transform._43;
            const float cw = x * transform._14 + y * transform._24 + z * transform._34 + transform._44;
            unsigned int mask = 0;
            if (cx < -cw)
            {
                mask |= 1;
            }
            if (cx > cw)
            {
                mask |= 2;
            }
            if (cy < -cw)
            {
                mask |= 4;
            }
            if (cy > cw)
            {
                mask |= 8;
            }
            if (cz < 0)
            {
                mask |= 16;
            }
            if (cz > cw)
            {
                mask |= 32;
            }
            outside &= mask;
        }
        return outside == 0;
    }

    inline bool GroundBoundsVisible(const D3DXVECTOR3& minimum, const D3DXVECTOR3& maximum, const D3DXVECTOR3& position,
        float size, const D3DXMATRIX& transform)
    {
        const float extent = std::hypot((std::max)(std::fabs(minimum.x), std::fabs(maximum.x)),
                                 (std::max)(std::fabs(minimum.y), std::fabs(maximum.y)))
            * size;
        const float offset = size / 20.0f;
        const float bottom = (std::min)({ position.z, -1000.0f - offset, position.z + 1.0f - offset });
        const float top = (std::max)({ position.z, -1000.0f - offset, position.z + 1.0f - offset });
        return BoundsVisible(D3DXVECTOR3(position.x - extent, position.y - extent, bottom + minimum.z * size * 20.0f),
            D3DXVECTOR3(position.x + extent, position.y + extent, top + maximum.z * size * 20.0f), transform);
    }

    inline bool RayTriangle(const D3DXVECTOR3& origin, const D3DXVECTOR3& direction, const D3DXVECTOR3& a, const D3DXVECTOR3& b,
        const D3DXVECTOR3& c, float& distance)
    {
        const D3DXVECTOR3 ab = b - a, ac = c - a, offset = origin - a;
        D3DXVECTOR3 cross;
        D3DXVec3Cross(&cross, &direction, &ac);
        const float determinant = D3DXVec3Dot(&ab, &cross);
        if (std::fabs(determinant) < 0.0000001f)
        {
            return false;
        }
        const float u = D3DXVec3Dot(&offset, &cross) / determinant;
        if (u < 0.0f || u > 1.0f)
        {
            return false;
        }
        D3DXVec3Cross(&cross, &offset, &ab);
        const float v = D3DXVec3Dot(&direction, &cross) / determinant;
        if (v < 0.0f || u + v > 1.0f)
        {
            return false;
        }
        const float hit = D3DXVec3Dot(&ac, &cross) / determinant;
        if (!std::isfinite(hit) || hit < 0.0f || hit >= distance)
        {
            return false;
        }
        distance = hit;
        return true;
    }
}
