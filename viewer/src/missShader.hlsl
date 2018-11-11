//Copyright (C) 2018 NewGamePlus Inc.
//Distributed under the MIT software license


#define HLSL
#include "RaycoinViewerRaytracing.h"

[shader("miss")]
void Miss(inout RayPayload payload)
{
    payload.instId = g_instMiss;
#ifndef COMPUTE_ONLY
    payload.diffuseColor = g.bgColor*g.diffuse;
#endif
    if (payload.reflection) return;

#ifndef COMPUTE_ONLY
    float3 outputColor;
    if (g.trace) outputColor = trace(payload);
    else outputColor = payload.diffuseColor;
    g_screenOutput[DispatchRaysIndex().xy] = float4(outputColor, 1);
#else
    trace(payload);
#endif
}

