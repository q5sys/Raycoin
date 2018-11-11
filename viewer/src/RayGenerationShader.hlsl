//Copyright (C) 2018 NewGamePlus Inc.
//Distributed under the MIT software license


#define HLSL
#include "RaycoinViewerRaytracing.h"

void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
    float2 xy = index + 0.5; // center in the middle of the pixel
    float2 screenPos = xy / g.resolution * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates
    screenPos.y = -screenPos.y;

    // Unproject into a ray
    float4 unprojected = mul(g.cameraToWorld, float4(screenPos, 0, 1));
    float3 world = unprojected.xyz / unprojected.w;
    origin = g.worldCameraPosition;
    direction = normalize(world - origin);
}

[shader("raygeneration")]
void RayGen()
{
    float3 origin, direction;

    uint2 index;
    if (g.verifyPos.x < 0) index = DispatchRaysIndex().xy;
    else index = g.verifyPos;
    GenerateCameraRay(index, origin, direction);

    RayDesc rayDesc = {origin, 0, direction, FLT_MAX};
    RayPayload payload;
    payload.reflection = false;
    TraceRay(g_accel, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0,0,1,0, rayDesc, payload);
}

