//Copyright (C) 2018 NewGamePlus Inc. - All Rights Reserved
//Unauthorized copying/distribution of this source code or any derived source code or binaries
//via any medium is strictly prohibited. Proprietary and confidential.

#define HLSL
#include "RaycoinViewerRaytracing.h"

[shader("miss")]
void Miss(inout RayPayload payload)
{
    payload.instId = g_instMiss;
    payload.diffuseColor = g.bgColor*g.diffuse;
    if (payload.reflection) return;

    float3 outputColor;
    if (g.trace) outputColor = trace(payload);
    else outputColor = payload.diffuseColor;
    g_screenOutput[DispatchRaysIndex().xy] = float4(outputColor, 1);
}

