// Source Repository: https://github.com/waverso/waversomapstudio

#include "d3dx_math_compat.h"

#include <DirectXMath.h>
#include <cmath>
#include <cstring>
#include <type_traits>

namespace
{
    DirectX::XMVECTOR LoadVector(const D3DXVECTOR3& value)
    {
        return DirectX::XMVectorSet(value.x, value.y, value.z, 0.0f);
    }

    DirectX::XMVECTOR LoadPoint(const D3DXVECTOR3& value)
    {
        return DirectX::XMVectorSet(value.x, value.y, value.z, 1.0f);
    }

    DirectX::XMVECTOR LoadQuaternion(const D3DXQUATERNION& value)
    {
        return DirectX::XMVectorSet(value.x, value.y, value.z, value.w);
    }

    DirectX::XMMATRIX LoadMatrix(const D3DXMATRIX& value)
    {
        DirectX::XMFLOAT4X4 stored;
        std::memcpy(&stored, &value, sizeof(stored));
        return DirectX::XMLoadFloat4x4(&stored);
    }

    void StoreVector(D3DXVECTOR3& out, DirectX::FXMVECTOR value)
    {
        DirectX::XMFLOAT3 stored;
        DirectX::XMStoreFloat3(&stored, value);
        out = { stored.x, stored.y, stored.z };
    }

    void StoreQuaternion(D3DXQUATERNION& out, DirectX::FXMVECTOR value)
    {
        DirectX::XMFLOAT4 stored;
        DirectX::XMStoreFloat4(&stored, value);
        out = { stored.x, stored.y, stored.z, stored.w };
    }

    void StoreMatrix(D3DXMATRIX& out, DirectX::FXMMATRIX value)
    {
        DirectX::XMFLOAT4X4 stored;
        DirectX::XMStoreFloat4x4(&stored, value);
        std::memcpy(&out, &stored, sizeof(out));
    }

    bool NormalizeAxis(const D3DXVECTOR3& axis, DirectX::XMVECTOR& normalized)
    {
        const float lengthSq = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
        if (!(lengthSq > 0.0f) || !std::isfinite(lengthSq))
        {
            return false;
        }
        const float inverseLength = 1.0f / std::sqrt(lengthSq);
        if (!std::isfinite(inverseLength))
        {
            return false;
        }
        normalized = DirectX::XMVectorScale(LoadVector(axis), inverseLength);
        return true;
    }

    bool IsValidViewport(const D3DVIEWPORT9& viewport)
    {
        return viewport.Width > 0 && viewport.Height > 0 && std::isfinite(viewport.MinZ) && std::isfinite(viewport.MaxZ)
            && viewport.MinZ <= viewport.MaxZ;
    }
}

D3DXMATRIX* D3DXMatrixIdentity(D3DXMATRIX* out)
{
    if (!out)
    {
        return nullptr;
    }
    StoreMatrix(*out, DirectX::XMMatrixIdentity());
    return out;
}

D3DXMATRIX* D3DXMatrixMultiply(D3DXMATRIX* out, const D3DXMATRIX* lhs, const D3DXMATRIX* rhs)
{
    if (!out || !lhs || !rhs)
    {
        return nullptr;
    }
    const DirectX::XMMATRIX result = DirectX::XMMatrixMultiply(LoadMatrix(*lhs), LoadMatrix(*rhs));
    StoreMatrix(*out, result);
    return out;
}

D3DXMATRIX operator*(const D3DXMATRIX& lhs, const D3DXMATRIX& rhs)
{
    D3DXMATRIX out;
    D3DXMatrixMultiply(&out, &lhs, &rhs);
    return out;
}

D3DXMATRIX* D3DXMatrixTranspose(D3DXMATRIX* out, const D3DXMATRIX* value)
{
    if (!out || !value)
    {
        return nullptr;
    }
    StoreMatrix(*out, DirectX::XMMatrixTranspose(LoadMatrix(*value)));
    return out;
}

D3DXMATRIX* D3DXMatrixInverse(D3DXMATRIX* out, float* determinant, const D3DXMATRIX* value)
{
    if (!out || !value)
    {
        return nullptr;
    }
    DirectX::XMVECTOR det;
    const DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(&det, LoadMatrix(*value));
    const float detValue = DirectX::XMVectorGetX(det);
    if (determinant)
    {
        *determinant = detValue;
    }
    if (!std::isfinite(detValue) || detValue == 0.0f || DirectX::XMMatrixIsNaN(inverse) || DirectX::XMMatrixIsInfinite(inverse))
    {
        StoreMatrix(*out, DirectX::XMMatrixIdentity());
        return nullptr;
    }
    StoreMatrix(*out, inverse);
    return out;
}

D3DXMATRIX* D3DXMatrixScaling(D3DXMATRIX* out, float x, float y, float z)
{
    if (!out)
    {
        return nullptr;
    }
    StoreMatrix(*out, DirectX::XMMatrixScaling(x, y, z));
    return out;
}

D3DXMATRIX* D3DXMatrixTranslation(D3DXMATRIX* out, float x, float y, float z)
{
    if (!out)
    {
        return nullptr;
    }
    StoreMatrix(*out, DirectX::XMMatrixTranslation(x, y, z));
    return out;
}

D3DXMATRIX* D3DXMatrixRotationX(D3DXMATRIX* out, float angle)
{
    if (!out)
    {
        return nullptr;
    }
    StoreMatrix(*out, DirectX::XMMatrixRotationX(angle));
    return out;
}

D3DXMATRIX* D3DXMatrixRotationY(D3DXMATRIX* out, float angle)
{
    if (!out)
    {
        return nullptr;
    }
    StoreMatrix(*out, DirectX::XMMatrixRotationY(angle));
    return out;
}

D3DXMATRIX* D3DXMatrixRotationZ(D3DXMATRIX* out, float angle)
{
    if (!out)
    {
        return nullptr;
    }
    StoreMatrix(*out, DirectX::XMMatrixRotationZ(angle));
    return out;
}

D3DXMATRIX* D3DXMatrixRotationAxis(D3DXMATRIX* out, const D3DXVECTOR3* axis, float angle)
{
    if (!out || !axis)
    {
        return nullptr;
    }
    DirectX::XMVECTOR normalized;
    if (!std::isfinite(angle) || !NormalizeAxis(*axis, normalized))
    {
        StoreMatrix(*out, DirectX::XMMatrixIdentity());
        return out;
    }
    StoreMatrix(*out, DirectX::XMMatrixRotationNormal(normalized, angle));
    return out;
}

D3DXMATRIX* D3DXMatrixRotationQuaternion(D3DXMATRIX* out, const D3DXQUATERNION* quaternion)
{
    if (!out || !quaternion)
    {
        return nullptr;
    }
    StoreMatrix(*out, DirectX::XMMatrixRotationQuaternion(LoadQuaternion(*quaternion)));
    return out;
}

D3DXMATRIX* D3DXMatrixOrthoLH(D3DXMATRIX* out, float width, float height, float nearZ, float farZ)
{
    if (!out)
    {
        return nullptr;
    }
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(nearZ) || !std::isfinite(farZ) || width == 0.0f
        || height == 0.0f || nearZ == farZ)
    {
        StoreMatrix(*out, DirectX::XMMatrixIdentity());
        return nullptr;
    }
    StoreMatrix(*out, DirectX::XMMatrixOrthographicLH(width, height, nearZ, farZ));
    return out;
}

float D3DXVec3Dot(const D3DXVECTOR3* lhs, const D3DXVECTOR3* rhs)
{
    return lhs && rhs ? lhs->x * rhs->x + lhs->y * rhs->y + lhs->z * rhs->z : 0.0f;
}

D3DXVECTOR3* D3DXVec3Cross(D3DXVECTOR3* out, const D3DXVECTOR3* lhs, const D3DXVECTOR3* rhs)
{
    if (!out || !lhs || !rhs)
    {
        return nullptr;
    }
    const D3DXVECTOR3 result { lhs->y * rhs->z - lhs->z * rhs->y, lhs->z * rhs->x - lhs->x * rhs->z,
        lhs->x * rhs->y - lhs->y * rhs->x };
    *out = result;
    return out;
}

float D3DXVec3LengthSq(const D3DXVECTOR3* value)
{
    return value ? D3DXVec3Dot(value, value) : 0.0f;
}

float D3DXVec3Length(const D3DXVECTOR3* value)
{
    return std::sqrt(D3DXVec3LengthSq(value));
}

D3DXVECTOR3* D3DXVec3Normalize(D3DXVECTOR3* out, const D3DXVECTOR3* value)
{
    if (!out || !value)
    {
        return nullptr;
    }
    const float lengthSq = D3DXVec3LengthSq(value);
    if (!(lengthSq > 0.0f) || !std::isfinite(lengthSq))
    {
        *out = { 0.0f, 0.0f, 0.0f };
        return out;
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSq);
    *out = *value * inverseLength;
    return out;
}

D3DXVECTOR3* D3DXVec3TransformCoord(D3DXVECTOR3* out, const D3DXVECTOR3* value, const D3DXMATRIX* matrix)
{
    if (!out || !value || !matrix)
    {
        return nullptr;
    }
    StoreVector(*out, DirectX::XMVector3TransformCoord(LoadPoint(*value), LoadMatrix(*matrix)));
    return out;
}

D3DXVECTOR3* D3DXVec3TransformNormal(D3DXVECTOR3* out, const D3DXVECTOR3* value, const D3DXMATRIX* matrix)
{
    if (!out || !value || !matrix)
    {
        return nullptr;
    }
    StoreVector(*out, DirectX::XMVector3TransformNormal(LoadVector(*value), LoadMatrix(*matrix)));
    return out;
}

D3DXVECTOR3* D3DXVec3Project(D3DXVECTOR3* out, const D3DXVECTOR3* value, const D3DVIEWPORT9* viewport,
    const D3DXMATRIX* projection, const D3DXMATRIX* view, const D3DXMATRIX* world)
{
    if (!out || !value || !viewport || !projection || !view)
    {
        return nullptr;
    }
    if (!IsValidViewport(*viewport))
    {
        *out = { 0.0f, 0.0f, 0.0f };
        return nullptr;
    }
    const DirectX::XMMATRIX worldMatrix = world ? LoadMatrix(*world) : DirectX::XMMatrixIdentity();
    const DirectX::XMVECTOR projected = DirectX::XMVector3Project(LoadPoint(*value), static_cast<float>(viewport->X),
        static_cast<float>(viewport->Y), static_cast<float>(viewport->Width), static_cast<float>(viewport->Height),
        viewport->MinZ, viewport->MaxZ, LoadMatrix(*projection), LoadMatrix(*view), worldMatrix);
    StoreVector(*out, projected);
    return out;
}

D3DXVECTOR3* D3DXVec3Unproject(D3DXVECTOR3* out, const D3DXVECTOR3* value, const D3DVIEWPORT9* viewport,
    const D3DXMATRIX* projection, const D3DXMATRIX* view, const D3DXMATRIX* world)
{
    if (!out || !value || !viewport || !projection || !view)
    {
        return nullptr;
    }
    if (!IsValidViewport(*viewport))
    {
        *out = { 0.0f, 0.0f, 0.0f };
        return nullptr;
    }
    const DirectX::XMMATRIX worldMatrix = world ? LoadMatrix(*world) : DirectX::XMMatrixIdentity();
    const DirectX::XMVECTOR unprojected = DirectX::XMVector3Unproject(LoadPoint(*value), static_cast<float>(viewport->X),
        static_cast<float>(viewport->Y), static_cast<float>(viewport->Width), static_cast<float>(viewport->Height),
        viewport->MinZ, viewport->MaxZ, LoadMatrix(*projection), LoadMatrix(*view), worldMatrix);
    StoreVector(*out, unprojected);
    return out;
}

D3DXQUATERNION* D3DXQuaternionIdentity(D3DXQUATERNION* out)
{
    if (!out)
    {
        return nullptr;
    }
    *out = { 0.0f, 0.0f, 0.0f, 1.0f };
    return out;
}

D3DXQUATERNION* D3DXQuaternionNormalize(D3DXQUATERNION* out, const D3DXQUATERNION* value)
{
    if (!out || !value)
    {
        return nullptr;
    }
    const float lengthSq = value->x * value->x + value->y * value->y + value->z * value->z + value->w * value->w;
    if (!(lengthSq > 0.0f) || !std::isfinite(lengthSq))
    {
        return D3DXQuaternionIdentity(out);
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSq);
    *out = { value->x * inverseLength, value->y * inverseLength, value->z * inverseLength, value->w * inverseLength };
    return out;
}

D3DXQUATERNION* D3DXQuaternionConjugate(D3DXQUATERNION* out, const D3DXQUATERNION* value)
{
    if (!out || !value)
    {
        return nullptr;
    }
    *out = { -value->x, -value->y, -value->z, value->w };
    return out;
}

D3DXQUATERNION* D3DXQuaternionMultiply(D3DXQUATERNION* out, const D3DXQUATERNION* lhs, const D3DXQUATERNION* rhs)
{
    if (!out || !lhs || !rhs)
    {
        return nullptr;
    }
    StoreQuaternion(*out, DirectX::XMQuaternionMultiply(LoadQuaternion(*lhs), LoadQuaternion(*rhs)));
    return out;
}

D3DXQUATERNION* D3DXQuaternionRotationAxis(D3DXQUATERNION* out, const D3DXVECTOR3* axis, float angle)
{
    if (!out || !axis)
    {
        return nullptr;
    }
    DirectX::XMVECTOR normalized;
    if (!std::isfinite(angle) || !NormalizeAxis(*axis, normalized))
    {
        return D3DXQuaternionIdentity(out);
    }
    StoreQuaternion(*out, DirectX::XMQuaternionRotationNormal(normalized, angle));
    return out;
}

D3DXQUATERNION* D3DXQuaternionRotationMatrix(D3DXQUATERNION* out, const D3DXMATRIX* matrix)
{
    if (!out || !matrix)
    {
        return nullptr;
    }
    StoreQuaternion(*out, DirectX::XMQuaternionRotationMatrix(LoadMatrix(*matrix)));
    return out;
}

static_assert(std::is_trivially_copyable<D3DXMATRIX>::value);
static_assert(sizeof(D3DXMATRIX) == sizeof(DirectX::XMFLOAT4X4));
