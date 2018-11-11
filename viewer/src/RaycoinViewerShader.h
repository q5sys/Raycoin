//Copyright (C) 2018 NewGamePlus Inc.
//Distributed under the MIT software license


#pragma once
#include "hlslCompat.h"

static const int g_sphereDim = 32;
static const int g_sphereCount = g_sphereDim*g_sphereDim*g_sphereDim;
static const int g_sphereLOD = 3;
static const int g_sphereTriCount = 20 * (4*4*4); //20 * (4^g_sphereLOD)
static const int g_screenDim = 1024;
static const int g_hitCount = 32;
// allow for a fraction of ray misses
static const int g_missMax = g_hitCount / 4;
static const int g_pathMax = g_hitCount + g_missMax;
// ray must travel half way through field
static const float g_targetDepth = ((g_sphereDim/2) * 4) / 2;
static const float g_targetDepthSqr = g_targetDepth*g_targetDepth;

#ifdef HLSL
#define blake2s_inBytesMax g_hitCount*4
#endif
