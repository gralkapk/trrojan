#pragma once

#include "benchmark_base.h"

#include "trrojan/d3d12/sphere_rt_data.h"

#include "SphereRTShaderStructs.hlsli"

namespace trrojan::d3d12 {
namespace CBV_SRV_UAV_Desc_Heap_Slots {
enum Value { RenderTarget = 0, AccumulationBuffer, RayGenConstants, RayTracingConstants, ParticleBuffer, Count };
}

namespace GlobalRootSigParams {
enum Value {
    OutputViewSlot = 0,
    AccelerationStructureSlot,
    RayGenConstantsSlot,
    RayTracingConstantsSlot,
    ParticleBufferSlot,
    Count
};
}

namespace ComputeRootSigParams {
enum Value {
    ParticleBufferSlot = 0,
    AABBBufferSlot,
    ComputeConstantsSlot,
    Count
};
}

class sphere_rt_benchmark_base : public benchmark_base {
public:
    virtual ~sphere_rt_benchmark_base(void) = default;

protected:
    sphere_rt_benchmark_base(const std::string& name);

    bool can_run(trrojan::environment env, trrojan::device device) const noexcept override;

    void on_device_switch(device& device) override;

    trrojan::result on_run(d3d12::device& device, const configuration& config,
        power_collector::pointer& power_collector, const std::vector<std::string>& changed) override;

private:
    bool clear_stale_data(const std::vector<std::string>& changed);
    void configure_camera(const configuration& config, const float fovy = 60.0f);
    void create_acceleration_structure(d3d12::device& device, ID3D12GraphicsCommandList* cmd_list);

    winrt::com_ptr<ID3D12RootSignature> global_root_sig_;
    winrt::com_ptr<ID3D12RootSignature> compute_root_sig_;
    winrt::com_ptr<ID3D12StateObject> raytracing_pipeline_;
    winrt::com_ptr<ID3D12PipelineState> compute_pipeline_;
    winrt::com_ptr<ID3D12Resource> cb_ray_;
    winrt::com_ptr<ID3D12Resource> cb_raytracing_;
    winrt::com_ptr<ID3D12Resource> cb_compute_;

    winrt::com_ptr<ID3D12Resource> bottomLevelBuffer_;
    winrt::com_ptr<ID3D12Resource> topLevelBuffer_;

    std::vector<winrt::com_ptr<ID3D12Resource>> render_targets_;
    std::vector<winrt::com_ptr<ID3D12Resource>> accumulation_buffers_;

    RayGenConstantsStruct* ray_gen_constants_;
    RayTracingConstantsStruct* ray_tracing_constants_;
    ComputeConstantsStruct* compute_constants_;

    sphere_rt_data data_;

    trrojan::perspective_camera _camera;
};
} // namespace trrojan::d3d12