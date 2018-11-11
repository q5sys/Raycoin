//Copyright (C) 2018 NewGamePlus Inc. - All Rights Reserved
//Unauthorized copying/distribution of this source code or any derived source code or binaries
//via any medium is strictly prohibited. Proprietary and confidential.

#include "blake2s.hlsli"

struct RayPayload
{
    bool reflection;
    float rayT;
    int prim;
    float3 diffuseColor;
    float3 emissiveColor;
    uint label;
    int instId;
};

struct PathElem_
{
    float3 pos;
    float3 dir;
    float rayT;
    float3 norm;
    float3 diffuseColor;
    float3 emissiveColor;
    int instId;
};

static const float FLT_MAX = asfloat(0x7F7FFFFF);
static const float PI = 3.1415926535897932384626433832795f;
static const float PI_TWO = PI*2;
static const float PI_HALF = PI/2;

RaytracingAccelerationStructure g_accel : register(t0);
RWTexture2D<float4> g_screenOutput : register(u0);
cbuffer cb : register(b0) { HitShaderConstants g; }
cbuffer matcb : register(b1) { MaterialRootConstants g_mat; }
StructuredBuffer<Attrib> g_attrib : register(t1);
StructuredBuffer<LineAttrib> g_lineAttrib : register(t2);
RWStructuredBuffer<TraceResult> g_traceResult : register(u1);

static const float g_specularAlbedo = 0.04; //dielectric albedo
static const float g_rimExp = 10;

//fresnel approximation
float fresnel(float cosa, float exp) { return pow(1 - cosa, exp); }

//modulate the diffuse and specular albedo by fresnel
void schlick(inout float3 diffuse, inout float3 specular, float3 lightDir, float3 halfVec)
{
    float f = fresnel(saturate(dot(lightDir, halfVec)), 5.0);
    specular *= g_specularAlbedo + (1-g_specularAlbedo)*f;
    diffuse *= 1-f;
}

float3 lighting(
    float3  diffuseColor,   //diffuse albedo
    float3  specularColor,  //specular albedo
    float   specularMask,   //roughness
    float   gloss,          //specular power
    float3  normal,         //world-space normal
    float3  viewDir,        //world-space vector from eye to point
    float3  lightDir,       //world-space vector from point to light
    float3  lightColor,     //radiance of directional light
    float3  gi,             //global illumination
    float   reflectFull     //0 for sparkle trick: instead of simply adding GI to reflect across the surface,
                            //modulate it with diffuse to ensure each sphere has shape definition and the full dynamic range from ambient to GI intensity
    )
{
    float3 halfVec = normalize(lightDir - viewDir);
    float nDotH = saturate(dot(halfVec, normal));
    float nDotL = saturate(dot(normal, lightDir));
    float nDotV = saturate(-dot(normal, viewDir));
    schlick(diffuseColor, specularColor, lightDir, halfVec);
    float phong = pow(nDotH, gloss) * (gloss + 2) / 8;
    float rim = fresnel(nDotV, g_rimExp); //make the rim shiny
    float3 diffuseTerm = lightColor*diffuseColor;
    float3 specularTerm = lightColor*specularColor*phong*specularMask;
    float3 giTerm = gi*specularMask;
    return (nDotL*(diffuseTerm + specularTerm + giTerm*(1-reflectFull)) + giTerm*reflectFull)*(1-rim) + (specularTerm + giTerm)*rim;
}

static const float2x2 g_rot_sqrt2deg = {0.999695398, -0.0246801768,
                                        0.0246801768, 0.999695398};

float3 trace(RayPayload initPayload)
{
    PathElem_ path[g_pathMax];
    blake2s_state hash_s;
    blake2s_init(hash_s);
    blake2s_in hash_in;
    int hit = 0;
    int miss = 0;
    float depthSqrMax = 0;
    float3 pos = WorldRayOrigin();
    float3 dir = WorldRayDirection();
    int pathMax = g.compute ? g_pathMax : g.shadePathMax;
    int p;
    for (p = 0; p < pathMax && hit < g_hitCount; ++p)
    {
        RayPayload payload;
        if (p)
        {
            RayDesc rayDesc = {pos, 0, dir, FLT_MAX};
            payload.reflection = true;                             //don't show lines through a miss
            TraceRay(g_accel, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, !miss ? ~0 : ~(-g_instLine), 0, 1, 0, rayDesc, payload);
        }
        else
            payload = initPayload;
        path[p].pos = pos;
        path[p].dir = dir;
        path[p].instId = payload.instId;
        path[p].diffuseColor = payload.diffuseColor;
        path[p].emissiveColor = payload.emissiveColor;

        bool endTrace = false;
        switch (payload.instId)
        {
        case g_instMiss:
        {
            ++miss;

            //restart at origin and rotate a bit to ensure hit
            pos = float3(0,0,0);
            dir.xy = mul(g_rot_sqrt2deg, dir.xy);
            dir.xz = mul(g_rot_sqrt2deg, dir.xz);
            break;
        }
        case g_instLine:
        {
            endTrace = true;
            break;
        }
        default:
        {
            path[p].rayT = payload.rayT;
            path[p].norm = g_attrib[payload.prim].norm;
            hash_in[hit>>2][hit&3] = payload.label;
            ++hit;

            pos += dir * payload.rayT;
            dir = reflect(dir, path[p].norm);
            depthSqrMax = max(dot(pos,pos), depthSqrMax);
            break;
        }
        }
        if (endTrace) { ++p; break; }
    }

    if (g.compute && g.hashing)
    {
        if (hit == g_hitCount && depthSqrMax >= g_targetDepthSqr)
        {
            blake2s(hash_s, hash_in, blake2s_inBytesMax);
            bool success = false;
            int i;
            for (i = 0; i < 8; ++i)
            {
                uint h = hash_s.h[i>>2][i&3];
                uint t = g.targetHash[i>>2][i&3];
                if (h > t) break;       //hash greater than target, fail
                else if (h < t) { success = true; break; } //hash less than target, success
            }
            if (i == 8) success = true; //hash equal to target, success
            if (success)
            {
                //acquire lock on the results
                bool resultExists;
                InterlockedCompareExchange(g_traceResult[1].success, false, true, resultExists);
                if (!resultExists)
                {
                    g_traceResult[1].pos = g.verifyPos.x < 0 ? DispatchRaysIndex().xy : g.verifyPos;
                    g_traceResult[1].depth = sqrt(depthSqrMax);
                    g_traceResult[1].hash = hash_s.h;
                    g_traceResult[1].pathCount = p;
                    for (int i = 0; i < p; ++i)
                    {
                        g_traceResult[1].path[i].pos = path[i].pos;
                        g_traceResult[1].path[i].dir = path[i].dir;
                        g_traceResult[1].path[i].rayT = path[i].rayT;
                        g_traceResult[1].path[i].instId = path[i].instId;
                    }
                }
            }
            else
            {
                //log best hash, we want to avoid waiting long for a lock, so settle for less than perfect logging
                int triesMax = 5;
                while (triesMax--)
                {
                    DeviceMemoryBarrier();
                    int syncPre = g_traceResult[0].hashBestSync;
                    bool success = false;
                    int i;
                    for (i = 0; i < 8; ++i)
                    {
                        uint h = hash_s.h[i>>2][i&3];
                        uint t = g_traceResult[0].hash[i>>2][i&3];
                        if (h > t) break;       //hash greater than target, fail
                        else if (h < t) { success = true; break; } //hash less than target, success
                    }
                    if (i == 8) success = true; //hash equal to target, success
                    int syncPost;
                    InterlockedAdd(g_traceResult[0].hashBestSync, 1, syncPost);
                    if (syncPre != syncPost) continue; //hash was changed on us mid-flight
                    if (!success) break; //at this point our success result is valid

                    //acquire lock on the results
                    bool resultExists;
                    InterlockedCompareExchange(g_traceResult[0].success, false, true, resultExists);
                    if (resultExists) continue;
                    g_traceResult[0].pos = g.verifyPos.x < 0 ? DispatchRaysIndex().xy : g.verifyPos;
                    g_traceResult[0].depth = sqrt(depthSqrMax);
                    g_traceResult[0].hash = hash_s.h;
                    g_traceResult[0].pathCount = p;
                    for (i = 0; i < p; ++i)
                    {
                        g_traceResult[0].path[i].pos = path[i].pos;
                        g_traceResult[0].path[i].dir = path[i].dir;
                        g_traceResult[0].path[i].rayT = path[i].rayT;
                        g_traceResult[0].path[i].instId = path[i].instId;
                    }
                    g_traceResult[0].success = false;
                    DeviceMemoryBarrier();
                    break;
                }
            }
        }
    }

    int last = min(p, g.shadePathMax)-1;
    float3 outputColor = {0,0,0};
    for (int i = last; i >= 0; --i)
    {
        switch (path[i].instId)
        {
        case g_instMiss:
        {
            if (!g.shadeLastMissOnly || i == last)
                outputColor = path[i].diffuseColor*0.5 + outputColor*g.specular*g.reflect*0.5;
            break;
        }
        case g_instLine:
        {
            outputColor = path[i].diffuseColor;
            break;
        }
        default:
        {
            outputColor = path[i].emissiveColor + g.ambientColor*path[i].diffuseColor +
                lighting(   path[i].diffuseColor*g.diffuse, path[i].diffuseColor, g.specular, g.gloss,
                            path[i].norm, path[i].dir, g.sunDirection, g.sunColor,
                            outputColor*g.reflect, g.reflectFull);
            break;
        }
        }
    }
    return outputColor;
}