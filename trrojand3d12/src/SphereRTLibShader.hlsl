#include "SphereRTShaderStructs.hlsli"

RWTexture2D<float4> RenderTarget : register(u0);
RWTexture2D<float4> AccumulationBuffer : register(u1);
RaytracingAccelerationStructure Scene : register(t0, space0);
ConstantBuffer<RayGenConstantsStruct> RayGenConstants : register(b0);
ConstantBuffer<RayTracingConstantsStruct> RayTracingConstants : register(b1);
StructuredBuffer<Particle> Particles : register(t1);

// https://sibaku.github.io/computer-graphics/2017/01/10/Camera-Ray-Generation.html
RayDesc GenerateRay(float2 px, float zNear = 1e-3f, float zFar = 1e+8f)
{
    RayDesc ray;

    ray.Origin = RayGenConstants.cameraPosition.xyz;

    float2 pxNDS = float2(px.x * 2.f - 1.f, 1.f - px.y * 2.f);

    float3 pointNDS = float3(pxNDS, -1.f);
    float4 pointNDSH = float4(pointNDS, 1.f);

    float4 dirEye = mul(RayGenConstants.projectionMatrixInv, pointNDSH);
    dirEye.w = 0.f;

    float3 dirWorld = mul(RayGenConstants.viewMatrixInv, dirEye).xyz;

    ray.Direction = normalize(dirWorld);

    ray.TMin = zNear;
    ray.TMax = zFar;

    return ray;
}

struct Attributes
{
    uint primID;
    float a;
};

struct RayPayload
{
    float4 color;
    float t;
    int primID;
};

[shader("raygeneration")]
void RayGenShader()
{
}

[shader("intersection")]
void IntersectionShader()
{
}

[shader("closesthit")]
void ClosestHitShader(inout RayPayload payload, in Attributes attr)
{
}

[shader("miss")]
void MissShaderName(inout RayPayload payload)
{
    payload.color = float4(0.f, 0.f, 0.f, 1.f);
}
