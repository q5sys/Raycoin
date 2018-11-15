//Copyright (C) 2018 NewGamePlus Inc. - All Rights Reserved
//Unauthorized copying/distribution of this source code or any derived source code or binaries
//via any medium is strictly prohibited. Proprietary and confidential.

#define HLSL
#include "RaycoinViewerCompute.h"
#include "blake2s.hlsli"

#define BLOCK_SIZE 256

cbuffer cbCS : register(b0) { InitFieldConstants g; };
RWByteAddressBuffer g_hitShaderTable : register(u0);
RWByteAddressBuffer g_instanceDescs : register(u1);

static const float g_sphereInterspaceMin = 0.04; //small spacer to ensure spheres don't overlap with each other or origin
static const float2 g_posRandRange = {-(1-g_sphereInterspaceMin/2), 1-g_sphereInterspaceMin/2};

#ifndef COMPUTE_ONLY
static const uint g_hitShaderColorOffset = 32;
static const uint g_hitShaderLabelOffset = g_hitShaderColorOffset + 3*4;
static const uint g_hitShaderHitTOffset = g_hitShaderLabelOffset + 4;
static const uint g_hitShaderStride = 128;
#else
static const uint g_hitShaderLabelOffset = 32;
static const uint g_hitShaderStride = 64;
#endif
static const uint g_instanceDescsTmOffset = 0;
static const uint g_instanceDescsStride = 64;

#define FLOAT_SIG_MASK 0x007FFFFFU
#define FLOAT_EXP_ZERO 0x3F800000U
float normalizedFloat(uint raw)                 { return asfloat((raw&FLOAT_SIG_MASK) | FLOAT_EXP_ZERO | 1) - 1.f; }
float uniformRand(uint r, float2 range)         { return (range[1]-range[0])*normalizedFloat(r) + range[0]; }

[numthreads(BLOCK_SIZE, 1, 1)]
void main(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    blake2s_state hash_s;
    blake2s_init(hash_s);
    blake2s_in hash_in;
    hash_in[0] = g.seed[0];
    hash_in[1] = g.seed[1];
    hash_in[2][0] = DTid.x;
    blake2s(hash_s, hash_in, 36);
    
    int start = DTid.x * g_sphereCount/BLOCK_SIZE;
    int end = start + g_sphereCount/BLOCK_SIZE;
    for (int i = start; i < end; ++i)
    {
        int h = i % 2; //each round uses half of generated randomness
#ifndef COMPUTE_ONLY
        //sphere color, not used in hashing so does not consume randomness, xor so that color doesn't correlate with position
        g_hitShaderTable.Store(i*g_hitShaderStride + g_hitShaderColorOffset, asuint(uniformRand(hash_s.h[h][0]^hash_s.h[h][1], float2(0,1))));
        g_hitShaderTable.Store(i*g_hitShaderStride + g_hitShaderColorOffset + 4, asuint(uniformRand(hash_s.h[h][0]^hash_s.h[h][2], float2(0,1))));
        g_hitShaderTable.Store(i*g_hitShaderStride + g_hitShaderColorOffset + 4*2, asuint(uniformRand(hash_s.h[h][0]^hash_s.h[h][3], float2(0,1))));
#endif
        //sphere label
        g_hitShaderTable.Store(i*g_hitShaderStride + g_hitShaderLabelOffset, hash_s.h[h][0]);
#ifndef COMPUTE_ONLY
        //zero out sphere hit T
        g_hitShaderTable.Store3(i*g_hitShaderStride + g_hitShaderHitTOffset, uint3(0,0,0));
#endif
        //sphere position 
        float3 pos = {
            (i%g_sphereDim - g_sphereDim/2) * 4 + uniformRand(hash_s.h[h][1], g_posRandRange),
            ((i%(g_sphereDim*g_sphereDim))/g_sphereDim - g_sphereDim/2) * 4 + uniformRand(hash_s.h[h][2], g_posRandRange),
            (i/(g_sphereDim*g_sphereDim) - g_sphereDim/2) * 4 + 2 + uniformRand(hash_s.h[h][3], g_posRandRange) 
        };
        g_instanceDescs.Store(i*g_instanceDescsStride + g_instanceDescsTmOffset + (0*4 + 3)*4, asuint(pos.x));
        g_instanceDescs.Store(i*g_instanceDescsStride + g_instanceDescsTmOffset + (1*4 + 3)*4, asuint(pos.y));
        g_instanceDescs.Store(i*g_instanceDescsStride + g_instanceDescsTmOffset + (2*4 + 3)*4, asuint(pos.z));

        if (i % 2 == 1) blake2s(hash_s, hash_in); //generate more randomness
    }
}
