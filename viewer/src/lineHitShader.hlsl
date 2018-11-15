//Copyright (C) 2018 NewGamePlus Inc. - All Rights Reserved
//Unauthorized copying/distribution of this source code or any derived source code or binaries
//via any medium is strictly prohibited. Proprietary and confidential.

#define HLSL
#include "RaycoinViewerRaytracing.h"

[shader("closesthit")]
void LineHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.instId = g_instLine;

    float ts[3] = {g_lineAttrib[PrimitiveIndex()*3].t, g_lineAttrib[PrimitiveIndex()*3+1].t, g_lineAttrib[PrimitiveIndex()*3+2].t};
    float t = ts[0] + (ts[1]-ts[0])*attr.barycentrics.x + (ts[2]-ts[0])*attr.barycentrics.y;
    float t_norm = t / g.rayLen;
    float pulse = (sin((t-g.time)*PI_TWO)+1)/2;
    float intensity = g.ray[0] + (g.ray[1]-g.ray[0])*pulse;
    payload.diffuseColor = (t_norm < 0.5 ?
        g.rayColorStart*(1-t_norm*2) + g.rayColorMid*t_norm*2 :
        g.rayColorMid*(1-(t_norm-0.5)*2) + g.rayColorEnd*(t_norm-0.5)*2) * intensity;

    if (payload.reflection) return;

    float3 outputColor = payload.diffuseColor;
    g_screenOutput[DispatchRaysIndex().xy] = float4(outputColor, 1);
}
