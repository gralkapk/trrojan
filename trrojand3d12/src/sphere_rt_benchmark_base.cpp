#include "trrojan/d3d12/sphere_rt_benchmark_base.h"

#include "trrojan/com_error_category.h"

#include "trrojan/d3d12/d3dx12.h"
#include "trrojan/d3d12/plugin.h"
#include "trrojan/d3d12/measurement_context.h"
#include "trrojan/d3d12/utilities.h"

#include "SphereRTShaderStructs.hlsli"

#define _SPHERE_RT_BENCH_DEFINE_FACTOR(f)                                         \
const char *trrojan::d3d12::sphere_rt_benchmark_base::factor_##f = #f

_SPHERE_RT_BENCH_DEFINE_FACTOR(data_set);
_SPHERE_RT_BENCH_DEFINE_FACTOR(frame);
_SPHERE_RT_BENCH_DEFINE_FACTOR(gpu_counter_iterations);
_SPHERE_RT_BENCH_DEFINE_FACTOR(min_prewarms);
_SPHERE_RT_BENCH_DEFINE_FACTOR(min_wall_time);
_SPHERE_RT_BENCH_DEFINE_FACTOR(spp);
_SPHERE_RT_BENCH_DEFINE_FACTOR(rec_depth);

#undef _SPHERE_RT_BENCH_DEFINE_FACTOR

namespace trrojan::d3d12 {
    sphere_rt_benchmark_base::sphere_rt_benchmark_base(const std::string& name)
        : benchmark_base{ name }
    {
        this->_default_configs.add_factor(factor::from_manifestations(
            factor_gpu_counter_iterations, static_cast<unsigned int>(7)));
        this->_default_configs.add_factor(factor::from_manifestations(
            factor_min_prewarms, static_cast<unsigned int>(4)));
        this->_default_configs.add_factor(factor::from_manifestations(
            factor_min_wall_time, static_cast<unsigned int>(1000)));
        this->_default_configs.add_factor(factor::from_manifestations(
            factor_spp, static_cast<unsigned int>(1)));
        this->_default_configs.add_factor(factor::from_manifestations(
            factor_rec_depth, static_cast<unsigned int>(0)));

        this->add_default_manoeuvre();
    }

    bool sphere_rt_benchmark_base::can_run(trrojan::environment env, trrojan::device device) const noexcept {
        if (!benchmark_base::can_run(env, device)) {
            return false;
        }
        auto d = std::dynamic_pointer_cast<trrojan::d3d12::device>(device);
        if (d == nullptr) {
            return false;
        }
        assert(d->d3d_device() != nullptr);
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 supportedFeatures = {};
        d->d3d_device()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &supportedFeatures, sizeof(supportedFeatures));

        return supportedFeatures.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
    }

    void sphere_rt_benchmark_base::on_device_switch(device& device)
    {
        assert(device.d3d_device() != nullptr);
        benchmark_base::on_device_switch(device);

        auto d3dDevice = device.d3d_device();
        winrt::com_ptr<ID3D12Device5> dxrDevice;
        d3dDevice->QueryInterface(IID_PPV_ARGS(&dxrDevice));

        // RayGen constant buffer
        {
            cb_ray_ = create_constant_buffer(d3dDevice, sizeof(RayGenConstantsStruct));
            set_debug_object_name(cb_ray_, "RayGenConstants");

            void* ptr;
            auto hr = cb_ray_->Map(0, nullptr, &ptr);
            if (FAILED(hr)) {
                throw std::system_error(hr, trrojan::com_category());
            }
            ray_gen_constants_ = static_cast<RayGenConstantsStruct*>(ptr);
        }

        // Ray Tracing constant buffer
        {
            cb_raytracing_ = create_constant_buffer(d3dDevice, sizeof(RayTracingConstantsStruct));
            set_debug_object_name(cb_raytracing_, "RayTracingConstants");

            void* ptr;
            auto hr = cb_raytracing_->Map(0, nullptr, &ptr);
            if (FAILED(hr)) {
                throw std::system_error(hr, trrojan::com_category());
            }
            ray_tracing_constants_ = static_cast<RayTracingConstantsStruct*>(ptr);
        }

        // global root signature
        {
            CD3DX12_ROOT_PARAMETER rootParams[GlobalRootSigParams::Count];
            CD3DX12_DESCRIPTOR_RANGE uavRange;
            uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);
            rootParams[GlobalRootSigParams::OutputViewSlot].InitAsDescriptorTable(1, &uavRange);
            rootParams[GlobalRootSigParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
            rootParams[GlobalRootSigParams::RayGenConstantsSlot].InitAsConstantBufferView(0);
            rootParams[GlobalRootSigParams::RayTracingConstantsSlot].InitAsConstantBufferView(1);
            rootParams[GlobalRootSigParams::ParticleBufferSlot].InitAsShaderResourceView(1);
            CD3DX12_ROOT_SIGNATURE_DESC globalRootSigDesc(ARRAYSIZE(rootParams), rootParams);
            winrt::com_ptr<ID3DBlob> signature;
            winrt::com_ptr<ID3DBlob> error;
            auto hr = D3D12SerializeRootSignature(&globalRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.put(), error.put());
            if (FAILED(hr)) {
                throw std::system_error(hr, trrojan::com_category());
            }
            hr = d3dDevice->CreateRootSignature(1, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&global_root_sig_));
            if (FAILED(hr)) {
                throw std::system_error(hr, trrojan::com_category());
            }
        }

        // compute root signature
        {
            // TODO: implement compute root signature
        }

        // pipeline state object
        {
            const auto loaded_lib = plugin::load_shader_asset("raytracing_pipeline.cso");
            D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE{ loaded_lib.data(), loaded_lib.size() };

            CD3DX12_STATE_OBJECT_DESC raytracingPipelineDesc{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };
            const auto lib = raytracingPipelineDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
            lib->SetDXILLibrary(&libdxil);

            std::vector<wchar_t const*> exportNames = {
            L"raygenShaderName", L"missShaderName", L"intersectionShaderName", L"closestHitShaderName"};
            lib->DefineExports(exportNames.data(), exportNames.size());

            auto hitGroup = raytracingPipelineDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
            hitGroup->SetIntersectionShaderImport(L"intersectionShaderName");
            hitGroup->SetClosestHitShaderImport(L"closestHitShaderName");
            hitGroup->SetHitGroupExport(L"hitGroupName");
            hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);

            const auto shaderConfig = raytracingPipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
            const auto payloadSize = 4 * sizeof(float) + sizeof(float) + 1 * sizeof(int);
            const auto attributeSize = 2 * sizeof(float);
            shaderConfig->Config(payloadSize, attributeSize);

            const auto globalRootSignature = raytracingPipelineDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
            globalRootSignature->SetRootSignature(global_root_sig_.get());

            const auto pipelineConfig = raytracingPipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
            pipelineConfig->Config(1);

            auto hr = dxrDevice->CreateStateObject(raytracingPipelineDesc, IID_PPV_ARGS(&raytracing_pipeline_));
            if (FAILED(hr)) {
                throw std::system_error(hr, trrojan::com_category());
            }
        }

        // TODO clear resources
        render_targets_.clear();

        this->create_descriptor_heaps(d3dDevice, CBV_SRV_UAV_Desc_Heap_Slots::Count);
    }

    trrojan::result sphere_rt_benchmark_base::on_run(d3d12::device& device, const configuration& config, power_collector::pointer& power_collector, const std::vector<std::string>& changed) {
        const auto gpu_freq = gpu_timer::get_timestamp_frequency(
            device.command_queue().get());
        measurement_context mctx(device, 2, this->pipeline_depth());

        // Load the data if necessary.
        if (!this->data_) {
            log::instance().write_line(log_level::information, "Loading data set \""
                "{}\" ...", cfg.data_set());
            auto cmd_list = this->create_graphics_command_list();
            auto upload = this->data_.load(cmd_list.get(), shader_code, cfg,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
                | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            device.close_and_execute_command_list(cmd_list);

            log::instance().write_line(log_level::verbose, "Waiting for data "
                "set to be loaded to the GPU ...");
            device.wait_for_gpu();
        }

        return trrojan::result();
    }
    
} // namespace trrojan::d3d12