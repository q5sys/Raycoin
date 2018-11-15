//Copyright (C) 2018 NewGamePlus Inc. - All Rights Reserved
//Unauthorized copying/distribution of this source code or any derived source code or binaries
//via any medium is strictly prohibited. Proprietary and confidential.

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

