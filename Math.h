#pragma once

const float c_pi = 3.14159265359f;

inline XMFLOAT3 operator * (const XMFLOAT3& a, const float b)
{
    XMFLOAT3 ret;
    ret.x = a.x * b;
    ret.y = a.y * b;
    ret.z = a.z * b;
    return ret;
}

inline XMFLOAT3 operator + (const XMFLOAT3& a, const XMFLOAT3& b)
{
    XMFLOAT3 ret;
    ret.x = a.x + b.x;
    ret.y = a.y + b.y;
    ret.z = a.z + b.z;
    return ret;
}

inline XMFLOAT3 operator - (const XMFLOAT3& a, const XMFLOAT3& b)
{
    XMFLOAT3 ret;
    ret.x = a.x - b.x;
    ret.y = a.y - b.y;
    ret.z = a.z - b.z;
    return ret;
}

inline XMFLOAT2 operator - (const XMFLOAT2& a, const XMFLOAT2& b)
{
    XMFLOAT2 ret;
    ret.x = a.x - b.x;
    ret.y = a.y - b.y;
    return ret;
}

inline XMFLOAT3 Cross (const XMFLOAT3& a, const XMFLOAT3& b)
{
    XMFLOAT3 ret;
    ret.x = a.y * b.z - a.z * b.y;
    ret.y = a.z * b.x - a.x * b.z;
    ret.z = a.x * b.y - a.y * b.x;
    return ret;
}

inline void Normalize (XMFLOAT3& v)
{
    float len = std::sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    v.x /= len;
    v.y /= len;
    v.z /= len;
}
