//Copyright (C) 2018 NewGamePlus Inc. - All Rights Reserved
//Unauthorized copying/distribution of this source code or any derived source code or binaries
//via any medium is strictly prohibited. Proprietary and confidential.

#pragma once
#ifndef HLSL

#define OUTPARAM(type, name)    type& name
#define INOUTPARAM(type, name)    type& name

struct float2
{
    union
    {
        struct { float x, y; };
        float a[2];
    };
};

struct float3
{
    union
    {
        struct { float x, y, z; };
        float a[3];
    };
};

struct float4
{
    union
    {
        struct { float x, y, z, w; };
        float a[4];
    };
};

struct int2
{
    union
    {
        struct { int x, y; };
        int a[2];
    };
};

struct int3
{
    union
    {
        struct { int x, y, z; };
        int a[3];
    };
};

__declspec(align(16))
struct int4
{
    union
    {
        struct { int x, y, z, w; };
        int a[4];
    };
};

struct uint2
{
    union
    {
        struct { UINT x, y; };
        UINT a[2];
    };
};

struct uint3
{
    union
    {
        struct { UINT x, y, z; };
        UINT a[3];
    };
};

__declspec(align(16))
struct uint4
{
    union
    {
        struct { UINT x, y, z, w; };
        UINT a[4];
    };
};

__declspec(align(16))
struct float4x4
{
    float   mat[16];
};

typedef UINT Bool;
typedef UINT uint;

inline
float3 operator + (const float3& a, const float3& b)
{
    return float3{ a.x + b.x, a.y + b.y, a.z + b.z };
}


inline
float3 operator - (const float3& a, const float3& b)
{
    return float3{ a.x - b.x, a.y - b.y, a.z - b.z };
}

inline
float3 operator * (const float3& a, const float3& b)
{
    return float3{ a.x * b.x, a.y * b.y, a.z * b.z };
}


inline
float3 abs(const float3& a)
{
    return float3{ std::abs(a.x), std::abs(a.y), std::abs(a.z) };
}

inline
float min(float a, float b)
{
    return std::min(a, b);
}

inline
float3 min(const float3& a, const float3& b)
{
    return float3{ std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) };
}


inline
float max(float a, float b)
{
    return std::max(a, b);
}

inline
float3 max(const float3& a, const float3& b)
{
    return float3{ std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) };
}

inline
float sign(float v)
{
    if (v < 0)
        return -1;
    return 1;
}

inline
float   dot(float3 a, float3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline
float3 cross(float3 a, float3 b)
{
    return float3
    {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

#else //HLSL

typedef float3 Vector3;
typedef bool Bool;

#endif