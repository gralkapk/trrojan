#include "SphereRTShaderStructs.hlsli"
#include "random.hlsli"
#include "LocalLighting.hlsli"

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

// https://github.com/UniStuttgart-VISUS/megamol/blob/master/plugins/optix_owl/cuda_resources/raygenPrograms.cu
float3 random_in_unit_sphere(inout LCGRand rng)
{
    float3 p;
    do
    {
        p = 2.f * float3(lcg_randomf(rng), lcg_randomf(rng), lcg_randomf(rng)) - float3(1.f, 1.f, 1.f);
    } while (dot(p, p) >= 1.f);
    return p;
}

// https://github.com/UniStuttgart-VISUS/megamol/blob/master/plugins/optix_owl/cuda_resources/raygenPrograms.cu
float4 trace_ray(RayDesc ray, inout LCGRand rng)
{
    //uint max_bounces = FrameProperties.max_bounces;
    uint max_bounces = RayTracingConstants.recursionDepth;
    float4 color = float4(1.f, 1.f, 1.f, 1.f);

    for (uint bounce = 0; true; ++bounce)
    {
        RayPayload payload = { float4(0.f, 0.f, 0.f, 0.f), 1e-3f, -1 };
        TraceRay(Scene, RAY_FLAG_FORCE_OPAQUE, ~0, 0, 1, 0, ray, payload);

        if (payload.primID == -1)
        {
            // Missed the scene
            if (bounce == 0)
            {
                return payload.color;
            }
            return color;
        }

        float3 iP = ray.Origin + ray.Direction * payload.t;
        float3 N = iP - Particles[payload.primID].position.xyz;
        if (dot(N, ray.Direction) > 0.f)
            N = -N;
        N = normalize(N);
                
        // early exit for primary rays only
        if (max_bounces == 0)
        {
            float3 D = .2f + .6f * abs(dot(N, ray.Direction));
            float4 att = payload.color * float4(D.x, D.y, D.z, 1.f);
            return att;
        }
        
        color *= payload.color;

        if (bounce >= max_bounces)
        {
            // ambient term
            return float4(0.1f, 0.1f, 0.1f, 1.0f);
        }

        ray.Origin = iP;
        ray.Direction = normalize(N + random_in_unit_sphere(rng));
        ray.TMin = 1e-3f;
        ray.TMax = 1e+8f;
    }
}

// https://github.com/UniStuttgart-VISUS/megamol/blob/master/plugins/optix_owl/cuda_resources/raygenPrograms.cu
[shader("raygeneration")]
void RaygenShaderName()
{
    float2 pixelID = (float2) DispatchRaysIndex();
    float2 fbSize = (float2) DispatchRaysDimensions();

    //uint frame_index = FrameProperties.frame_index;
    uint frame_index = RayTracingConstants.frameIdx;
    
    LCGRand rng = get_rng(frame_index);
    
    //uint spp = FrameProperties.spp;
    //int max_depth = FrameProperties.max_bounces;
    uint spp = RayTracingConstants.spp;
    //uint max_depth = RayTracingConstants.recursionDepth;

    float4 out_color = float4(0.f, 0.f, 0.f, 0.f);
    
    for (uint s = 0; s < spp; ++s)
    {
        RayDesc ray = GenerateRay((pixelID + float2(lcg_randomf(rng), lcg_randomf(rng))) / fbSize,
            1e-3f, 1e+8f);

        float4 color = trace_ray(ray, rng);
        
        out_color += color;
    }
    out_color /= spp;
    
    float4 prev_color = AccumulationBuffer[DispatchRaysIndex().xy];
    float4 accum_color = (prev_color * frame_index + out_color) / (frame_index + 1);
    AccumulationBuffer[DispatchRaysIndex().xy] = accum_color;

    //RenderTarget[DispatchRaysIndex().xy] = linear_to_srgb(accum_color);
    RenderTarget[DispatchRaysIndex().xy] = accum_color;
}

[shader("intersection")]
void IntersectionShaderName()
{
    float3 origin = WorldRayOrigin();
    float3 direction = WorldRayDirection();
 
    float4 position = Particles[PrimitiveIndex()].position;
    
    float3 oc = origin - position.xyz;
    float radius = position.w;
    float sqRadius = radius * radius;

    float b = dot(-oc, direction);
    float3 temp = oc + b * direction;
    float delta = sqRadius - dot(temp, temp);

    if (delta < 0.f)
    {
        // No intersection
        return;
    }

    float c = dot(oc, oc) - sqRadius;
    float q = b;
    if (b == 0.f)
    {
        q = q + sqrt(delta);

    }
    else
    {
        q = q + sign(b) * sqrt(delta);
    }

    float ta = c / q;
    float tb = q;
    float t = min(ta, tb);
    if (t < 0.f)
    {
        return;
    }
    if (t > RayTMin() && t < RayTCurrent())
    {
        Attributes attr;
        attr.primID = PrimitiveIndex();
        ReportHit(t, 0, attr);
    }
}

float4 unpackColor(uint color)
{
    float4 unpackedColor;
    unpackedColor.r = (color & 0xFF) / 255.0f;
    unpackedColor.g = ((color >> 8) & 0xFF) / 255.0f;
    unpackedColor.b = ((color >> 16) & 0xFF) / 255.0f;
    unpackedColor.a = ((color >> 24) & 0xFF) / 255.0f;
    return unpackedColor;
}

[shader("closesthit")]
void ClosestHitShaderName(inout RayPayload payload, in Attributes attr)
{
    payload.color = unpackColor(Particles[attr.primID].color);
    payload.t = RayTCurrent();
    payload.primID = attr.primID;
}

[shader("miss")]
void MissShaderName(inout RayPayload payload)
{
    payload.color = float4(0.f, 0.f, 0.f, 1.f);
}
