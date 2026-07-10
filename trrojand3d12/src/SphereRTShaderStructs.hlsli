#include "hlsltypemapping.hlsli"

#ifdef _MSC_VER
#pragma once
#endif /* _MSC_VER */

struct AABB {
    float3 lower;
    float3 upper;
};

struct Particle
{
    float4 position; // xyz = position, w = radius
    uint4 color; // rgba
};

#ifdef _MSC_VER
// See https://msdn.microsoft.com/de-de/library/windows/desktop/bb509632(v=vs.85).aspx
#pragma pack(push)
#pragma pack(4)
#define CB __declspec(align(16)) struct
#else
#define CB struct
#endif /* _MSC_VER */

CB RayGenConstantsStruct {
    float4x4 viewMatrixInv;
    float4x4 projectionMatrixInv;
    float4 cameraPosition;
    uint2 renderTargetSize;
    float zNear;
    float zFar;
    float padding[24];
};

CB RayTracingConstantsStruct
{
    float spp;
    uint recursionDepth;
    float padding[60];
};

CB ComputeConstantsStruct
{
    uint3 dispatchSize;
    uint num_particles;
    float padding[60];
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */