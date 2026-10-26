// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <d3d9.h>
#include <numbers>

inline constexpr float D3DX_PI = std::numbers::pi_v<float>;

constexpr float D3DXToRadian(float degrees)
{
    return degrees * (D3DX_PI / 180.0f);
}

constexpr float D3DXToDegree(float radians)
{
    return radians * (180.0f / D3DX_PI);
}

struct D3DXVECTOR3 : D3DVECTOR
{
    D3DXVECTOR3() = default;
    constexpr D3DXVECTOR3(float px, float py, float pz) : D3DVECTOR { px, py, pz }
    {
    }

    D3DXVECTOR3 operator+() const
    {
        return *this;
    }
    D3DXVECTOR3 operator-() const
    {
        return { -x, -y, -z };
    }
    D3DXVECTOR3 operator+(const D3DXVECTOR3& rhs) const
    {
        return { x + rhs.x, y + rhs.y, z + rhs.z };
    }
    D3DXVECTOR3 operator-(const D3DXVECTOR3& rhs) const
    {
        return { x - rhs.x, y - rhs.y, z - rhs.z };
    }
    D3DXVECTOR3 operator*(float scale) const
    {
        return { x * scale, y * scale, z * scale };
    }
    D3DXVECTOR3 operator/(float scale) const
    {
        return { x / scale, y / scale, z / scale };
    }
    D3DXVECTOR3& operator+=(const D3DXVECTOR3& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }
    D3DXVECTOR3& operator-=(const D3DXVECTOR3& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }
    D3DXVECTOR3& operator*=(float scale)
    {
        x *= scale;
        y *= scale;
        z *= scale;
        return *this;
    }
    D3DXVECTOR3& operator/=(float scale)
    {
        x /= scale;
        y /= scale;
        z /= scale;
        return *this;
    }
};

inline D3DXVECTOR3 operator*(float scale, const D3DXVECTOR3& value)
{
    return value * scale;
}

struct D3DXQUATERNION
{
    float x, y, z, w;

    D3DXQUATERNION() = default;
    constexpr D3DXQUATERNION(float px, float py, float pz, float pw) : x(px), y(py), z(pz), w(pw)
    {
    }
};

struct D3DXMATRIX : D3DMATRIX
{
    D3DXMATRIX() = default;

    constexpr D3DXMATRIX(float m11, float m12, float m13, float m14, float m21, float m22, float m23, float m24, float m31,
        float m32, float m33, float m34, float m41, float m42, float m43, float m44)
        : D3DMATRIX { m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44 }
    {
    }
};

struct D3DXCOLOR : D3DCOLORVALUE
{
    D3DXCOLOR() = default;
    constexpr D3DXCOLOR(float pr, float pg, float pb, float pa) : D3DCOLORVALUE { pr, pg, pb, pa }
    {
    }
    explicit D3DXCOLOR(D3DCOLOR color)
        : D3DCOLORVALUE { static_cast<float>((color >> 16) & 0xFF) / 255.0f, static_cast<float>((color >> 8) & 0xFF) / 255.0f,
              static_cast<float>(color & 0xFF) / 255.0f, static_cast<float>((color >> 24) & 0xFF) / 255.0f }
    {
    }

    D3DXCOLOR operator*(float scale) const
    {
        return { r * scale, g * scale, b * scale, a * scale };
    }
};

D3DXMATRIX* D3DXMatrixIdentity(D3DXMATRIX* out);
D3DXMATRIX* D3DXMatrixMultiply(D3DXMATRIX* out, const D3DXMATRIX* lhs, const D3DXMATRIX* rhs);
D3DXMATRIX operator*(const D3DXMATRIX& lhs, const D3DXMATRIX& rhs);
D3DXMATRIX* D3DXMatrixTranspose(D3DXMATRIX* out, const D3DXMATRIX* value);
D3DXMATRIX* D3DXMatrixInverse(D3DXMATRIX* out, float* determinant, const D3DXMATRIX* value);
D3DXMATRIX* D3DXMatrixScaling(D3DXMATRIX* out, float x, float y, float z);
D3DXMATRIX* D3DXMatrixTranslation(D3DXMATRIX* out, float x, float y, float z);
D3DXMATRIX* D3DXMatrixRotationX(D3DXMATRIX* out, float angle);
D3DXMATRIX* D3DXMatrixRotationY(D3DXMATRIX* out, float angle);
D3DXMATRIX* D3DXMatrixRotationZ(D3DXMATRIX* out, float angle);
D3DXMATRIX* D3DXMatrixRotationAxis(D3DXMATRIX* out, const D3DXVECTOR3* axis, float angle);
D3DXMATRIX* D3DXMatrixRotationQuaternion(D3DXMATRIX* out, const D3DXQUATERNION* quaternion);
D3DXMATRIX* D3DXMatrixOrthoLH(D3DXMATRIX* out, float width, float height, float nearZ, float farZ);

float D3DXVec3Dot(const D3DXVECTOR3* lhs, const D3DXVECTOR3* rhs);
D3DXVECTOR3* D3DXVec3Cross(D3DXVECTOR3* out, const D3DXVECTOR3* lhs, const D3DXVECTOR3* rhs);
float D3DXVec3LengthSq(const D3DXVECTOR3* value);
float D3DXVec3Length(const D3DXVECTOR3* value);
D3DXVECTOR3* D3DXVec3Normalize(D3DXVECTOR3* out, const D3DXVECTOR3* value);
D3DXVECTOR3* D3DXVec3TransformCoord(D3DXVECTOR3* out, const D3DXVECTOR3* value, const D3DXMATRIX* matrix);
D3DXVECTOR3* D3DXVec3TransformNormal(D3DXVECTOR3* out, const D3DXVECTOR3* value, const D3DXMATRIX* matrix);
D3DXVECTOR3* D3DXVec3Project(D3DXVECTOR3* out, const D3DXVECTOR3* value, const D3DVIEWPORT9* viewport,
    const D3DXMATRIX* projection, const D3DXMATRIX* view, const D3DXMATRIX* world);
D3DXVECTOR3* D3DXVec3Unproject(D3DXVECTOR3* out, const D3DXVECTOR3* value, const D3DVIEWPORT9* viewport,
    const D3DXMATRIX* projection, const D3DXMATRIX* view, const D3DXMATRIX* world);

D3DXQUATERNION* D3DXQuaternionIdentity(D3DXQUATERNION* out);
D3DXQUATERNION* D3DXQuaternionNormalize(D3DXQUATERNION* out, const D3DXQUATERNION* value);
D3DXQUATERNION* D3DXQuaternionConjugate(D3DXQUATERNION* out, const D3DXQUATERNION* value);
D3DXQUATERNION* D3DXQuaternionMultiply(D3DXQUATERNION* out, const D3DXQUATERNION* lhs, const D3DXQUATERNION* rhs);
D3DXQUATERNION* D3DXQuaternionRotationAxis(D3DXQUATERNION* out, const D3DXVECTOR3* axis, float angle);
D3DXQUATERNION* D3DXQuaternionRotationMatrix(D3DXQUATERNION* out, const D3DXMATRIX* matrix);

static_assert(sizeof(D3DXVECTOR3) == sizeof(float) * 3);
static_assert(sizeof(D3DXQUATERNION) == sizeof(float) * 4);
static_assert(sizeof(D3DXMATRIX) == sizeof(D3DMATRIX));
