//Copyright (C) 2018 NewGamePlus Inc. - All Rights Reserved
//Unauthorized copying/distribution of this source code or any derived source code or binaries
//via any medium is strictly prohibited. Proprietary and confidential.

#pragma once
#include "RaycoinViewerShader.h"

static const int g_instMiss = -1;
static const int g_instLine = -2;
static const int g_sphereHitMax = 3;

struct HitShaderConstants
{
    float4x4 cameraToWorld;
    Vector3 worldCameraPosition;
#ifndef COMPUTE_ONLY
    Vector3 bgColor;
    Vector3 diffuseColor;
    Vector3 ambientColor;
    Vector3 sunColor;
    Vector3 sunDirection; 
    Vector3 rayColorStart;
    Vector3 rayColorMid;
    Vector3 rayColorEnd; //Vector3 is 3 bytes in shader / 4 bytes outside, shader packing rules keep sync if a large type follows
#endif
    uint4   targetHash[2];
    int2    verifyPos;
    float2  resolution;
#ifndef COMPUTE_ONLY
    float   diffuse;
    float   gloss;
    float   specular;
    int     shadePathMax;
    float   reflect;
    float   reflectFull;
    float2  ray;
    float2  rayHit;
    float   rayLen;
    float   time;
    Bool    shadeLastMissOnly;
    Bool    trace;
    Bool    compute;
    Bool    hashing;
#endif
    Bool    bestHash;
};

struct Attrib
{
    float3 norm;
};

struct LineAttrib
{
    float t;
};

struct MaterialRootConstants
{
#ifndef COMPUTE_ONLY
    float3 diffuseColor;
#endif
    uint label;
#ifndef COMPUTE_ONLY
    Vector3 hitT; //float[g_sphereHitMax], in HLSL each float would be packed as a float4
    Vector3 hitCenter;
    Vector3 hitPolar[g_sphereHitMax];
#endif
};

struct PathElem
{
    float3 pos;
    float rayT;
    float3 dir;
    int instId;
};

struct TraceResult
{
    Bool success;
    int2 pos;
    float depth;
    uint4 hash[2];
#ifndef COMPUTE_ONLY
    PathElem path[g_pathMax];
    int pathCount;
#endif
    int hashBestSync;
#ifndef COMPUTE_ONLY
    uint2 padding;
#else
    uint3 padding;
#endif
};

#ifdef HLSL
#include "trace.hlsli"
#endif
