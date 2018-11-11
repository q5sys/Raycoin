//Copyright (C) 2018 NewGamePlus Inc. - All Rights Reserved
//Unauthorized copying/distribution of this source code or any derived source code or binaries
//via any medium is strictly prohibited. Proprietary and confidential.

#define HLSL
#include "RaycoinViewerRaytracing.h"

[shader("closesthit")]
void Hit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.rayT = RayTCurrent();
    payload.prim = PrimitiveIndex();
    payload.diffuseColor = g_mat.diffuseColor*g.diffuseColor;

    payload.emissiveColor = 0;
    if (g_mat.hitT[0] != 0)
    {
        //sphere is in hash result path, highlight it
        float3 pos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
        float3 dir = pos - g_mat.hitCenter;
        float2 polar = {acos(dir[1]), atan2(dir[2],dir[0])};
        for (int i = 0; i < g_sphereHitMax && g_mat.hitT[i] != 0; ++i)
        {
            float t = g_mat.hitT[i];
            float t_norm = t / g.rayLen;
            float2 hitPolar = {g_mat.hitPolar[i][0], g_mat.hitPolar[i][1]};
            //shortest distance around sphere / great circle distance from spherical law of cosines
            float arcdist = acos(cos(hitPolar[0])*cos(polar[0]) + sin(hitPolar[0])*sin(polar[0])*cos(abs(hitPolar[1]-polar[1])));
            float pulse = (sin((t+arcdist-g.time)*PI_TWO)+1)/2;
            float intensity = g.rayHit[0] + (g.rayHit[1]-g.rayHit[0])*pulse;
            float3 rayColor = (t_norm < 0.5 ?
                g.rayColorStart*(1-t_norm*2) + g.rayColorMid*t_norm*2 :
                g.rayColorMid*(1-(t_norm-0.5)*2) + g.rayColorEnd*(t_norm-0.5)*2);
            payload.emissiveColor += payload.diffuseColor*intensity/2 + rayColor*intensity/2;
        }
    }

    payload.label = g_mat.label;
    payload.instId = InstanceID();
    if (payload.reflection) return;

    float3 outputColor;
    if (g.trace)
        outputColor = trace(payload);
    else
    {
        float3 normal = g_attrib[PrimitiveIndex()].norm;
        outputColor = payload.emissiveColor + g.ambientColor*payload.diffuseColor +
            lighting(   payload.diffuseColor*g.diffuse, payload.diffuseColor, g.specular, g.gloss,
                        normal, WorldRayDirection(), g.sunDirection, g.sunColor,
                        float3(0,0,0), 0);
    }
    g_screenOutput[DispatchRaysIndex().xy] = float4(outputColor, 1);
}
