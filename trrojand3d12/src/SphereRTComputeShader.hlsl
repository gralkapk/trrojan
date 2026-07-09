#include "SphereRTShaderStructs.hlsli"

StructuredBuffer<Particle> particles : register(t0);
RWStructuredBuffer<AABB> aabbs : register(u0);
ConstantBuffer<ComputeConstantsStruct> computeConstants: register(b0);

[numthreads(32, 1, 1)]
void Main(uint3 DTid : SV_DispatchThreadID)
{
    uint idx = DTid.x
    + computeConstants.dispatchSize.x * (DTid.y + computeConstants.dispatchSize.y * DTid.z);
    
    if (idx < computeConstants.num_particles)
    {
        Particle p = particles[idx];
        float radius = p.position.w;
        float3 pos = p.position.xyz;
        AABB aabb;
        aabb.lower = pos - float3(radius, radius, radius);
        aabb.upper = pos + float3(radius, radius, radius);
        aabbs[idx] = aabb;
    }
}