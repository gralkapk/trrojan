#include "hlsltypemapping.hlsli"

#ifdef _MSC_VER
#pragma once
#endif /* _MSC_VER */

#ifdef _MSC_VER
// See https://msdn.microsoft.com/de-de/library/windows/desktop/bb509632(v=vs.85).aspx
#pragma pack(push)
#pragma pack(4)
#endif /* _MSC_VER */

cbuffer RayGenConstantsStruct CBUFFER(0) {
    float4x4 viewMatrixInv;
    float4x4 projectionMatrixInv;
    float4 cameraPosition;
    float zNear;
    float zFar;
    uint2 renderTargetSize;
};

cbuffer RayTracingConstantsStruct CBUFFER(1) {
    float spp;
    uint recursionDepth;
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */