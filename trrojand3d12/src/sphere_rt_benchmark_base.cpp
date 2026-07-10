#include "trrojan/d3d12/sphere_rt_benchmark_base.h"

#include "trrojan/com_error_category.h"

#include "trrojan/clipping.h"

#include "trrojan/d3d12/d3dx12.h"
#include "trrojan/d3d12/measurement_context.h"
#include "trrojan/d3d12/plugin.h"
#include "trrojan/d3d12/utilities.h"

#include "trrojan/d3d12/sphere_rt_rendering_configuration.h"

#include "SphereRTShaderStructs.hlsli"

namespace trrojan::d3d12 {

#define STR_(X) #X
#define STR(X) STR_(X)

// clang-format off
const wchar_t* raygenShaderName            = L"" STR(RaygenShaderName) "";
const wchar_t* intersectionShaderName      = L"" STR(IntersectionShaderName) "";
const wchar_t* closestHitShaderName        = L"" STR(ClosestHitShaderName) "";
const wchar_t* missShaderName              = L"" STR(MissShaderName) "";
const wchar_t* hitGroupName                = L"HitGroup";
// clang-format on

#undef STR_
#undef STR

sphere_rt_benchmark_base::sphere_rt_benchmark_base(const std::string& name) : benchmark_base{name} {
    this->_default_configs.add_factor(factor::from_manifestations(
        sphere_rt_rendering_configuration::factor_gpu_counter_iterations, static_cast<unsigned int>(7)));
    this->_default_configs.add_factor(factor::from_manifestations(
        sphere_rt_rendering_configuration::factor_min_prewarms, static_cast<unsigned int>(4)));
    this->_default_configs.add_factor(factor::from_manifestations(
        sphere_rt_rendering_configuration::factor_min_wall_time, static_cast<unsigned int>(1000)));
    this->_default_configs.add_factor(
        factor::from_manifestations(sphere_rt_rendering_configuration::factor_spp, static_cast<unsigned int>(1)));
    this->_default_configs.add_factor(
        factor::from_manifestations(sphere_rt_rendering_configuration::factor_rec_depth, static_cast<unsigned int>(0)));

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

void sphere_rt_benchmark_base::on_device_switch(device& device) {
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

    // Compute constant buffer
    {
        cb_compute_ = create_constant_buffer(d3dDevice, sizeof(ComputeConstantsStruct));
        set_debug_object_name(cb_compute_, "ComputeConstants");
        void* ptr;
        auto hr = cb_compute_->Map(0, nullptr, &ptr);
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
        compute_constants_ = static_cast<ComputeConstantsStruct*>(ptr);
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
        auto hr =
            D3D12SerializeRootSignature(&globalRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.put(), error.put());
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
        hr = d3dDevice->CreateRootSignature(
            1, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&global_root_sig_));
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
    }

    // compute root signature
    {
        CD3DX12_ROOT_PARAMETER rootParams[ComputeRootSigParams::Count];
        rootParams[ComputeRootSigParams::ParticleBufferSlot].InitAsShaderResourceView(0);
        rootParams[ComputeRootSigParams::AABBBufferSlot].InitAsUnorderedAccessView(0);
        rootParams[ComputeRootSigParams::ComputeConstantsSlot].InitAsConstantBufferView(0);
        CD3DX12_ROOT_SIGNATURE_DESC computeRootSigDesc(ARRAYSIZE(rootParams), rootParams);
        winrt::com_ptr<ID3DBlob> signature;
        winrt::com_ptr<ID3DBlob> error;
        auto hr = D3D12SerializeRootSignature(
            &computeRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.put(), error.put());
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
        hr = d3dDevice->CreateRootSignature(
            1, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&compute_root_sig_));
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
    }

    // pipeline state object
    {
        const auto loaded_lib = plugin::load_shader_asset("SphereRTLibShader.cso");
        D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE{loaded_lib.data(), loaded_lib.size()};

        CD3DX12_STATE_OBJECT_DESC raytracingPipelineDesc{D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE};
        const auto lib = raytracingPipelineDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        lib->SetDXILLibrary(&libdxil);

        std::vector<wchar_t const*> exportNames = {
            raygenShaderName, missShaderName, intersectionShaderName, closestHitShaderName};
        lib->DefineExports(exportNames.data(), exportNames.size());

        auto hitGroup = raytracingPipelineDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetIntersectionShaderImport(intersectionShaderName);
        hitGroup->SetClosestHitShaderImport(closestHitShaderName);
        hitGroup->SetHitGroupExport(hitGroupName);
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);

        const auto shaderConfig = raytracingPipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
        const auto payloadSize = 4 * sizeof(float) + sizeof(float) + 1 * sizeof(int);
        const auto attributeSize = 2 * sizeof(float);
        shaderConfig->Config(payloadSize, attributeSize);

        const auto globalRootSignature =
            raytracingPipelineDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        globalRootSignature->SetRootSignature(global_root_sig_.get());

        const auto pipelineConfig =
            raytracingPipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
        pipelineConfig->Config(1);

        auto hr = dxrDevice->CreateStateObject(raytracingPipelineDesc, IID_PPV_ARGS(&raytracing_pipeline_));
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
    }

    // compute pipeline state object
    {
        const auto loaded_compute_shader = plugin::load_shader_asset("SphereRTComputeShader.cso");

        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = compute_root_sig_.get();
        computePsoDesc.CS = CD3DX12_SHADER_BYTECODE{loaded_compute_shader.data(), loaded_compute_shader.size()};

        auto hr = dxrDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&compute_pipeline_));
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
    }

    // TODO clear resources
    render_targets_.clear();
    accumulation_buffers_.clear();

    create_descriptor_heaps(d3dDevice, CBV_SRV_UAV_Desc_Heap_Slots::Count);

    // create shader table
    winrt::com_ptr<ID3D12StateObjectProperties> stateObjectProperties;
    auto hr = raytracing_pipeline_.as(IID_PPV_ARGS(&stateObjectProperties));
    if (FAILED(hr)) {
        throw std::system_error(hr, trrojan::com_category());
    }
    assert(stateObjectProperties != nullptr);
    
    auto sbt_ = ShaderTable();
    sbt_.SetRayGenRecord(
            ShaderRecord(reinterpret_cast<UINT8*>(stateObjectProperties->GetShaderIdentifier(raygenShaderName))))
        .AddMissRecord(ShaderRecord(reinterpret_cast<UINT8*>(stateObjectProperties->GetShaderIdentifier(missShaderName))))
        .AddHitGroupRecord(
            ShaderRecord(reinterpret_cast<UINT8*>(stateObjectProperties->GetShaderIdentifier(hitGroupName))));

    auto const sbt_size = sbt_.Serialize(nullptr, 0, &shader_table_descriptor_);
    sbtBuffer_ = create_upload_buffer(d3dDevice.get(), sbt_size);
    assert(sbtBuffer_ != nullptr);
    set_debug_object_name(sbtBuffer_, "ShaderBindingTable");
    {
        void* data;
        auto hr = sbtBuffer_->Map(0, nullptr, &data);
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
        sbt_.Serialize(data, sbt_size, &shader_table_descriptor_);
        sbtBuffer_->Unmap(0, nullptr);
    }
}

trrojan::result sphere_rt_benchmark_base::on_run(d3d12::device& device, const configuration& config,
    power_collector::pointer& power_collector, const std::vector<std::string>& changed) {
    sphere_rt_rendering_configuration cfg{config};
    const auto gpu_freq = gpu_timer::get_timestamp_frequency(device.command_queue().get());
    measurement_context mctx(device, 2, this->pipeline_depth());


    clear_stale_data(changed);

    // Load the data if necessary.
    if (!this->data_) {
        log::instance().write_line(log_level::information,
            "Loading data set \""
            "{}\" ...",
            cfg.data_set());
        auto cmd_list = this->create_graphics_command_list();
        auto upload = this->data_.load(cmd_list.get(),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            cfg.data_set(), cfg.frame());
        device.close_and_execute_command_list(cmd_list);

        log::instance().write_line(log_level::verbose, "Waiting for data "
                                                       "set to be loaded to the GPU ...");
        device.wait_for_gpu();

        // TODO create acceleration data structure
        create_acceleration_structure(device, cmd_list.get());
    }

    configure_camera(config);

    // create the UAVs for the rt render targets
    if (render_targets_.empty() || accumulation_buffers_.empty()) {
        const auto viewport = config.get<benchmark_base::viewport_type>(factor_viewport);
        // TODO descriptor heaps?
        render_targets_.resize(pipeline_depth());
        accumulation_buffers_.resize(pipeline_depth());

        for (auto& rt : render_targets_) {
            rt = create_texture(device.d3d_device(), viewport[0], viewport[1], DXGI_FORMAT_R8G8B8A8_UNORM,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        }

        for (auto& ab : accumulation_buffers_) {
            ab = create_texture(device.d3d_device(), viewport[0], viewport[1], DXGI_FORMAT_R32G32B32A32_FLOAT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        }
    }


    std::vector<winrt::com_ptr<ID3D12GraphicsCommandList>> cmd_lists(pipeline_depth());
    std::vector<winrt::com_ptr<ID3D12GraphicsCommandList5>> dxr_cmd_lists(pipeline_depth());
    for (UINT i = 0; i < pipeline_depth(); ++i) {
        cmd_lists[i] = this->create_graphics_command_list(i);
        auto hr = cmd_lists[i]->QueryInterface(IID_PPV_ARGS(&dxr_cmd_lists[i]));
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
    }
    // TODO TEST basic rendering command list
    {
        for (UINT i = 0; i < pipeline_depth(); ++i) {
            auto& dxr_cmd_list = dxr_cmd_lists[i];
            auto heap = _descriptor_heaps[i].get();
            // TODO record commands
            dxr_cmd_list->SetComputeRootSignature(global_root_sig_.get());
            dxr_cmd_list->SetDescriptorHeaps(1, &heap);

            // set resource views
            // TODO set UAV table
            dxr_cmd_list->SetComputeRootDescriptorTable(
                GlobalRootSigParams::OutputViewSlot, heap->GetGPUDescriptorHandleForHeapStart());
            dxr_cmd_list->SetComputeRootShaderResourceView(
                GlobalRootSigParams::AccelerationStructureSlot, topLevelBuffer_->GetGPUVirtualAddress());
            dxr_cmd_list->SetComputeRootConstantBufferView(
                GlobalRootSigParams::RayGenConstantsSlot, cb_ray_->GetGPUVirtualAddress());
            dxr_cmd_list->SetComputeRootConstantBufferView(
                GlobalRootSigParams::RayTracingConstantsSlot, cb_raytracing_->GetGPUVirtualAddress());
            dxr_cmd_list->SetComputeRootShaderResourceView(
                GlobalRootSigParams::ParticleBufferSlot, data_.data()->GetGPUVirtualAddress());

            // set the pipeline state
            dxr_cmd_list->SetPipelineState1(raytracing_pipeline_.get());

            // dispatch rays
            D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = {};
            // raygen
            dispatchRaysDesc.RayGenerationShaderRecord.StartAddress =
                sbtBuffer_->GetGPUVirtualAddress();
            dispatchRaysDesc.RayGenerationShaderRecord.SizeInBytes = shader_table_descriptor_.raygen_record_size_;
            // miss
            dispatchRaysDesc.MissShaderTable.StartAddress =
                sbtBuffer_->GetGPUVirtualAddress() + shader_table_descriptor_.miss_records_offset_;
            dispatchRaysDesc.MissShaderTable.SizeInBytes = shader_table_descriptor_.miss_records_size_;
            dispatchRaysDesc.MissShaderTable.StrideInBytes = shader_table_descriptor_.miss_records_stride_;
            // hitgroup
            dispatchRaysDesc.HitGroupTable.StartAddress =
                sbtBuffer_->GetGPUVirtualAddress() + shader_table_descriptor_.hitgroup_records_offset_;
            dispatchRaysDesc.HitGroupTable.SizeInBytes = shader_table_descriptor_.hitgroup_records_size_;
            dispatchRaysDesc.HitGroupTable.StrideInBytes = shader_table_descriptor_.hitgroup_records_stride_;

            const auto viewport = config.get<benchmark_base::viewport_type>(factor_viewport);
            dispatchRaysDesc.Width = viewport[0];
            dispatchRaysDesc.Height = viewport[1];
            dispatchRaysDesc.Depth = 1;

            dxr_cmd_list->DispatchRays(&dispatchRaysDesc);

            // TODO copy the render target to the back buffer

        }
    }

    return trrojan::result();
}

bool sphere_rt_benchmark_base::clear_stale_data(const std::vector<std::string>& changed) {
    const auto retval = contains_any(
        changed, sphere_rt_rendering_configuration::factor_data_set, sphere_rt_rendering_configuration::factor_frame);

    if (retval) {
        data_.clear();
    }

    return retval;
}

void sphere_rt_benchmark_base::configure_camera(const configuration& config, const float fovy) {
    set_aspect_from_viewport(_camera);
    _camera.set_fovy(fovy);
    graphics_benchmark_base::apply_manoeuvre(_camera, config, data_.bbox_start(), data_.bbox_end());
    trrojan::set_clipping_planes(_camera, data_.bbox(), data_.max_radius());
}

void sphere_rt_benchmark_base::create_acceleration_structure(
    d3d12::device& device, ID3D12GraphicsCommandList* cmd_list) {
    auto d3dDevice = device.d3d_device();
    winrt::com_ptr<ID3D12Device5> dxrDevice;
    {
        auto hr = d3dDevice->QueryInterface(IID_PPV_ARGS(&dxrDevice));
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
        assert(dxrDevice != nullptr);
    }

    winrt::com_ptr<ID3D12GraphicsCommandList5> dxrCmdList;
    {
        auto hr = cmd_list->QueryInterface(IID_PPV_ARGS(&dxrCmdList));
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
        assert(dxrCmdList != nullptr);
    }

    auto aabb_buffer = create_buffer(device.d3d_device(), data_.spheres() * sizeof(AABB), 0,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    set_debug_object_name(aabb_buffer, "AABBBuffer");

    // acceleration data structure
    {
        D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
        geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
        geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geomDesc.AABBs.AABBCount = data_.spheres();
        geomDesc.AABBs.AABBs.StartAddress = aabb_buffer->GetGPUVirtualAddress();
        geomDesc.AABBs.AABBs.StrideInBytes = sizeof(AABB);

        // top level input
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
        topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        topLevelInputs.NumDescs = 1;
        topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

        // bottom level input
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = {};
        bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        bottomLevelInputs.NumDescs = 1;
        bottomLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        bottomLevelInputs.pGeometryDescs = &geomDesc;

        // allocation sizes
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
        dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
        dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);

        auto const maxScratchSize =
            (std::max) (topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes);

        // allocate scratch buffer, top level and bottom level buffers
        auto scratchBuffer = create_buffer(device.d3d_device(), maxScratchSize, 0,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        set_debug_object_name(scratchBuffer, "ScratchBuffer");

        topLevelBuffer_ = create_buffer(device.d3d_device(), topLevelPrebuildInfo.ResultDataMaxSizeInBytes, 0,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
        set_debug_object_name(topLevelBuffer_, "TopLevelBuffer");

        bottomLevelBuffer_ = create_buffer(device.d3d_device(), bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, 0,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
        set_debug_object_name(bottomLevelBuffer_, "BottomLevelBuffer");

        auto instanceDescsBuffer =
            create_upload_buffer(device.d3d_device().get(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

        // set instance desc for bottom level
        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
        instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1.f;
        instanceDesc.InstanceMask = 1;
        instanceDesc.InstanceID = 0;
        instanceDesc.AccelerationStructure = bottomLevelBuffer_->GetGPUVirtualAddress();

        {
            std::uint8_t* mappedData = nullptr;
            auto const hr = instanceDescsBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
            if (FAILED(hr)) {
                throw std::system_error(hr, trrojan::com_category());
            }
            assert(mappedData != nullptr);
            std::memcpy(mappedData, &instanceDesc, sizeof(instanceDesc));
            instanceDescsBuffer->Unmap(0, nullptr);
        }

        topLevelInputs.InstanceDescs = instanceDescsBuffer->GetGPUVirtualAddress();

        // set top level desc
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
        topLevelBuildDesc.Inputs = topLevelInputs;
        topLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress();
        topLevelBuildDesc.DestAccelerationStructureData = topLevelBuffer_->GetGPUVirtualAddress();

        // set bottom level desc
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
        bottomLevelBuildDesc.Inputs = bottomLevelInputs;
        bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress();
        bottomLevelBuildDesc.DestAccelerationStructureData = bottomLevelBuffer_->GetGPUVirtualAddress();

        // build acceleration structure
        {
            // TODO need specific frame? (this->buffer_index())
            auto heap = _descriptor_heaps[0].get();
            assert(heap != nullptr);
            assert(heap->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            // compute AABBs for spheres
            {
                dxrCmdList->SetComputeRootSignature(compute_root_sig_.get());
                dxrCmdList->SetPipelineState(compute_pipeline_.get());

                dxrCmdList->SetDescriptorHeaps(1, &heap);

                // set views on data
                dxrCmdList->SetComputeRootShaderResourceView(
                    ComputeRootSigParams::ParticleBufferSlot, data_.data()->GetGPUVirtualAddress());
                dxrCmdList->SetComputeRootUnorderedAccessView(
                    ComputeRootSigParams::AABBBufferSlot, aabb_buffer->GetGPUVirtualAddress());
                dxrCmdList->SetComputeRootConstantBufferView(
                    ComputeRootSigParams::ComputeConstantsSlot, cb_compute_->GetGPUVirtualAddress());

                // dispatch compute shader to compute AABBs
                auto const num_particles = data_.spheres();
                auto base_size = std::ceilf(std::sqrtf(num_particles));
                auto const thread_group_size_x = static_cast<UINT>(base_size);
                auto const thread_group_size_y = static_cast<UINT>(num_particles / base_size + 1);
                compute_constants_->dispatchSize = {thread_group_size_x, thread_group_size_y, 1};
                compute_constants_->num_particles = num_particles;
                dxrCmdList->Dispatch(thread_group_size_x / 32 + 1, thread_group_size_y, 1);

                // set barrier on AABB buffer to make sure compute shader is done before building acceleration structure
                auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(aabb_buffer.get());
                dxrCmdList->ResourceBarrier(1, &barrier);
            }

            // build bottom level
            {
                dxrCmdList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
                auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelBuffer_.get());
                dxrCmdList->ResourceBarrier(1, &barrier);
            }

            // build top level
            {
                dxrCmdList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
            }

            device.close_and_execute_command_list(dxrCmdList.get());
            device.wait_for_gpu();
        }
    }
}

} // namespace trrojan::d3d12