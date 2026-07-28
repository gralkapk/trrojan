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

sphere_rt_benchmark_base::sphere_rt_benchmark_base() : benchmark_base("rt-sphere-renderer") {
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

void sphere_rt_benchmark_base::optimise_order(configuration_set& inOutConfs) {
    inOutConfs.optimise_order({sphere_rt_rendering_configuration::factor_data_set,
        sphere_rt_rendering_configuration::factor_frame, benchmark_base::factor_device});
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

    create_descriptor_heaps(d3dDevice, CBV_SRV_UAV_Desc_RT_Heap_Slots::Count);
    // add descriptor heap for compute pipeline
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = CBV_SRV_UAV_Desc_Compute_Heap_Slots::Count;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        winrt::com_ptr<ID3D12DescriptorHeap> descriptor_heap_compute;
        auto const hr = d3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap_compute));
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
        set_debug_object_name(descriptor_heap_compute, "ComputeDescriptorHeap");
        _descriptor_heaps.push_back(std::move(descriptor_heap_compute));
    }

    std::vector<UINT> heap_increment_sizes(pipeline_depth());
    for (int i = 0; i < pipeline_depth(); ++i) {
        heap_increment_sizes[i] = d3dDevice->GetDescriptorHandleIncrementSize(_descriptor_heaps[i]->GetDesc().Type);
    }
    heap_increment_sizes.push_back(d3dDevice->GetDescriptorHandleIncrementSize(_descriptor_heaps.back()->GetDesc().Type));

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

        for (int i = 0; i < pipeline_depth(); ++i) {
            auto const cb_cpu_handle =
                CD3DX12_CPU_DESCRIPTOR_HANDLE{_descriptor_heaps[i]->GetCPUDescriptorHandleForHeapStart(),
                    CBV_SRV_UAV_Desc_RT_Heap_Slots::RayGenConstants, heap_increment_sizes[i]};
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = cb_ray_->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = static_cast<UINT>(sizeof(RayGenConstantsStruct));
            d3dDevice->CreateConstantBufferView(&cbvDesc, cb_cpu_handle);
        }
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

        for (int i = 0; i < pipeline_depth(); ++i) {
            auto const cb_cpu_handle =
                CD3DX12_CPU_DESCRIPTOR_HANDLE{_descriptor_heaps[i]->GetCPUDescriptorHandleForHeapStart(),
                    CBV_SRV_UAV_Desc_RT_Heap_Slots::RayTracingConstants, heap_increment_sizes[i]};
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = cb_raytracing_->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = static_cast<UINT>(sizeof(RayTracingConstantsStruct));
            d3dDevice->CreateConstantBufferView(&cbvDesc, cb_cpu_handle);
        }
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

        auto const cb_cpu_handle =
            CD3DX12_CPU_DESCRIPTOR_HANDLE{_descriptor_heaps.back()->GetCPUDescriptorHandleForHeapStart(),
                CBV_SRV_UAV_Desc_Compute_Heap_Slots::ComputeConstants, heap_increment_sizes.back()};
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = cb_compute_->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = static_cast<UINT>(sizeof(ComputeConstantsStruct));
        d3dDevice->CreateConstantBufferView(&cbvDesc, cb_cpu_handle);
    }

    // global root signature
    {
        CD3DX12_ROOT_PARAMETER rootParams[GlobalRootSigParams::Count];
        ZeroMemory(rootParams, sizeof(rootParams));
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
        ZeroMemory(rootParams, sizeof(rootParams));
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
        const auto loaded_lib = plugin::load_resource(MAKEINTRESOURCE(3000), _T("SHADER"));
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
        const auto loaded_compute_shader = plugin::load_resource(MAKEINTRESOURCE(3001), _T("SHADER"));

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
    accumulation_buffer_ = nullptr;

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

    _bundle_allocators.clear();
    create_command_allocators(_bundle_allocators, d3dDevice.get(), D3D12_COMMAND_LIST_TYPE_BUNDLE, pipeline_depth());
}

trrojan::result sphere_rt_benchmark_base::on_run(d3d12::device& device, const configuration& config,
    power_collector::pointer& power_collector, const std::vector<std::string>& changed) {
    sphere_rt_rendering_configuration cfg{config};
    std::vector<gpu_timer::millis_type> gpu_times;
    const auto gpu_freq = gpu_timer::get_timestamp_frequency(device.command_queue().get());
    measurement_context mctx(device, 2, this->pipeline_depth());
    stats_query::value_type pipeline_stats;
    stats_query stats_query(device.d3d_device().get(), 1, 1);

    std::vector<UINT> descriptor_heap_sizes(this->pipeline_depth());
    for (int i = 0; i < this->pipeline_depth(); ++i) {
        descriptor_heap_sizes[i] =
            device.d3d_device()->GetDescriptorHandleIncrementSize(_descriptor_heaps[i]->GetDesc().Type);
    }

    clear_stale_data(changed);

    // Load the data if necessary.
    if (!this->data_) {
        log::instance().write_line(log_level::information,
            "Loading data set \""
            "{}\" ...",
            cfg.data_set());
        auto cmd_list = this->create_graphics_command_list();
        auto upload = this->data_.load(cmd_list.get(), cfg.data_set(), cfg.frame());
        device.close_and_execute_command_list(cmd_list);

        log::instance().write_line(log_level::verbose, "Waiting for data "
                                                       "set to be loaded to the GPU ...");
        device.wait_for_gpu();

        // set views
        {
            auto const data = this->data_.data();
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = this->data_.spheres();
            srvDesc.Buffer.StructureByteStride = sizeof(Particle);
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

            for (int i = 0; i < this->pipeline_depth(); ++i) {
                auto const srvDescHandle =
                    CD3DX12_CPU_DESCRIPTOR_HANDLE(_descriptor_heaps[i]->GetCPUDescriptorHandleForHeapStart(),
                        CBV_SRV_UAV_Desc_RT_Heap_Slots::ParticleBuffer, descriptor_heap_sizes[i]);
                device.d3d_device()->CreateShaderResourceView(data.get(), &srvDesc, srvDescHandle);
            }
            auto const srvDescHandle =
                CD3DX12_CPU_DESCRIPTOR_HANDLE(_descriptor_heaps.back()->GetCPUDescriptorHandleForHeapStart(),
                    CBV_SRV_UAV_Desc_Compute_Heap_Slots::ParticleBuffer, descriptor_heap_sizes.back());
            device.d3d_device()->CreateShaderResourceView(data.get(), &srvDesc, srvDescHandle);
        }

        // TODO create acceleration data structure
        reset_command_list(cmd_list);
        create_acceleration_structure(device, cmd_list.get());
    }

    configure_camera(config);

    // set raygen constants
    {
        auto const pos = _camera.get_look_from();
        ray_gen_constants_->cameraPosition = DirectX::XMFLOAT3(pos.x, pos.y, pos.z);
        auto const view_inv = _camera.get_inverse_view_mx();
        auto const proj_inv = _camera.get_inverse_projection_mx();
        ray_gen_constants_->viewMatrixInv = DirectX::XMFLOAT4X4(&view_inv[0][0]);
        ray_gen_constants_->projectionMatrixInv = DirectX::XMFLOAT4X4(&proj_inv[0][0]);
        const auto viewport = config.get<benchmark_base::viewport_type>(factor_viewport);
        ray_gen_constants_->renderTargetSize =
            DirectX::XMUINT2(viewport[0], viewport[1]);
        ray_gen_constants_->zNear = _camera.get_near_plane_dist();
        ray_gen_constants_->zFar = _camera.get_far_plane_dist();
    }

    // set ray tracing constants
    {
        ray_tracing_constants_->spp = 1;
        ray_tracing_constants_->recursionDepth = 0;
    }

    // create the UAVs for the rt render targets
    if (render_targets_.empty() || accumulation_buffer_ == nullptr) {
        const auto viewport = config.get<benchmark_base::viewport_type>(factor_viewport);
        // TODO descriptor heaps?
        render_targets_.resize(pipeline_depth());

        for (int i = 0; i < pipeline_depth(); ++i) {
            render_targets_[i] = create_texture(device.d3d_device(), viewport[0], viewport[1],
                DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
            set_debug_object_name(render_targets_[i], ("RenderTarget_" + std::to_string(i)).c_str());
            auto const uavDescHandle =
                CD3DX12_CPU_DESCRIPTOR_HANDLE(_descriptor_heaps[i]->GetCPUDescriptorHandleForHeapStart(),
                    CBV_SRV_UAV_Desc_RT_Heap_Slots::RenderTarget, descriptor_heap_sizes[i]);
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            device.d3d_device()->CreateUnorderedAccessView(render_targets_[i].get(), nullptr, &uavDesc, uavDescHandle);
        }

        if (accumulation_buffer_ == nullptr) {
            accumulation_buffer_ =
                create_texture(device.d3d_device(), viewport[0], viewport[1], DXGI_FORMAT_R32G32B32A32_FLOAT,
                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
            set_debug_object_name(accumulation_buffer_, "AccumulationBuffer");
            for (int i = 0; i < pipeline_depth(); ++i) {
                auto const uavDescHandle =
                    CD3DX12_CPU_DESCRIPTOR_HANDLE(_descriptor_heaps[i]->GetCPUDescriptorHandleForHeapStart(),
                        CBV_SRV_UAV_Desc_RT_Heap_Slots::AccumulationBuffer, descriptor_heap_sizes[i]);
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                device.d3d_device()->CreateUnorderedAccessView(
                    accumulation_buffer_.get(), nullptr, &uavDesc, uavDescHandle);
            }
        }
    }


    std::vector<winrt::com_ptr<ID3D12GraphicsCommandList>> cmd_lists(pipeline_depth());
    std::vector<winrt::com_ptr<ID3D12GraphicsCommandList5>> dxr_cmd_lists(pipeline_depth());
    std::vector<winrt::com_ptr<ID3D12GraphicsCommandList5>> dxr_bundles(pipeline_depth());
    for (UINT i = 0; i < pipeline_depth(); ++i) {
        cmd_lists[i] = this->create_graphics_command_list(i);
        std::string name = "RTCommandList_" + std::to_string(i);
        set_debug_object_name(cmd_lists[i], name.c_str());
        auto hr = cmd_lists[i]->QueryInterface(IID_PPV_ARGS(&dxr_cmd_lists[i]));
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
        auto bundle = this->create_command_list(D3D12_COMMAND_LIST_TYPE_BUNDLE, i);
        hr = bundle->QueryInterface(IID_PPV_ARGS(&dxr_bundles[i]));
        if (FAILED(hr)) {
            throw std::system_error(hr, trrojan::com_category());
        }
    }


    gpu_timer::millis_type cpu_time;

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
            transition_resource(cmd_lists[i].get(), render_targets_[i].get(), D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COPY_SOURCE);

            enable_target(cmd_lists[i].get(), i, D3D12_RESOURCE_STATE_COPY_DEST);
            clear_target(cmd_lists[i].get(), i);
            copy_to_target(cmd_lists[i].get(), render_targets_[i].get(), i);
            disable_target(cmd_lists[i].get(), i, D3D12_RESOURCE_STATE_COPY_DEST);

            transition_resource(cmd_lists[i].get(), render_targets_[i].get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                D3D12_RESOURCE_STATE_COMMON);

            close_command_list(cmd_lists[i].get());
            /*device.close_and_execute_command_list(cmd_lists[i]);
            present_target(config);

            device.wait_for_gpu();*/
        }

        // record bundle
        for (UINT i = 0; i < pipeline_depth(); ++i) {
            auto& dxr_cmd_list = dxr_bundles[i];
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
            dispatchRaysDesc.RayGenerationShaderRecord.StartAddress = sbtBuffer_->GetGPUVirtualAddress();
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
            /*transition_resource(dxr_cmd_list.get(), render_targets_[i].get(), D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COPY_SOURCE);

            enable_target(dxr_cmd_list.get(), i, D3D12_RESOURCE_STATE_COPY_DEST);
            clear_target(dxr_cmd_list.get(), i);
            copy_to_target(dxr_cmd_list.get(), render_targets_[i].get(), i);
            disable_target(dxr_cmd_list.get(), i, D3D12_RESOURCE_STATE_COPY_DEST);

            transition_resource(dxr_cmd_list.get(), render_targets_[i].get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                D3D12_RESOURCE_STATE_COMMON);*/

            close_command_list(dxr_cmd_list.get());
            /*device.close_and_execute_command_list(cmd_lists[i]);
            present_target(config);

            device.wait_for_gpu();*/
        }

        /*for (UINT i = 0; i < 20000; ++i) {
            auto cmd_list = cmd_lists[this->buffer_index()];
            device.execute_command_list(cmd_list);
            present_target(config);
            device.wait_for_gpu();
        }*/


        // Do prewarming and compute number of CPU iterations at the same time.
        log::instance().write_line(log_level::debug, "Prewarming ...");
        {
            auto prewarms = (std::max)(1u, cfg.min_prewarms());

            mctx.cpu_timer.start();
            do {
                for (std::uint32_t i = 0; i < mctx.cpu_iterations; ++i) {
                    auto cmd_list = cmd_lists[this->buffer_index()];
                    device.execute_command_list(cmd_list);
                    present_target(config);
                }
                device.wait_for_gpu();
                prewarms = mctx.check_cpu_iterations(cfg.min_wall_time());
            } while (prewarms > 0);
        }

        // Do the wall clock measurement using the prepared command lists.
        log::instance().write_line(log_level::debug,
            "Measuring wall clock "
            "timings over {} iterations ...",
            mctx.cpu_iterations);
        {
            mctx.cpu_timer.start();
            for (std::uint32_t i = 0; i < mctx.cpu_iterations; ++i) {
                auto cmd_list = cmd_lists[this->buffer_index()];
                device.execute_command_list(cmd_list);
                this->present_target(config);
            }
            device.wait_for_gpu();
            cpu_time = mctx.cpu_timer.elapsed_millis();
        }

        // Do the GPU counter measurements using individual command lists.
        gpu_times.resize(cfg.gpu_counter_iterations());
        power_collector->enter_scope();
        for (std::uint32_t i = 0; i < cfg.gpu_counter_iterations(); ++i) {
            log::instance().write_line(log_level::debug,
                "GPU counter measurement "
                "#{}.",
                i);
            auto cmd_list = cmd_lists[this->buffer_index()];
            reset_command_list(cmd_list);

            auto heap = _descriptor_heaps[this->buffer_index()].get();
            
            cmd_list->SetComputeRootSignature(global_root_sig_.get());
            cmd_list->SetDescriptorHeaps(1, &heap);

            
            mctx.gpu_timer.start_frame();
            mctx.gpu_timer.start(cmd_list.get(), 0);
            cmd_list->ExecuteBundle(dxr_bundles[this->buffer_index()].get());
            transition_resource(cmd_list.get(), render_targets_[this->buffer_index()].get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COPY_SOURCE);

            enable_target(cmd_list.get(), this->buffer_index(), D3D12_RESOURCE_STATE_COPY_DEST);
            //clear_target(cmd_list.get(), this->buffer_index());
            copy_to_target(cmd_list.get(), render_targets_[this->buffer_index()].get(), this->buffer_index());
            disable_target(cmd_list.get(), this->buffer_index(), D3D12_RESOURCE_STATE_COPY_DEST);

            transition_resource(cmd_list.get(), render_targets_[this->buffer_index()].get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                D3D12_RESOURCE_STATE_COMMON);
            mctx.gpu_timer.end(cmd_list.get(), 0);
            const auto timer_index = mctx.gpu_timer.end_frame(cmd_list.get());

            device.close_and_execute_command_list(cmd_list);
            this->present_target(config);

            device.wait_for_gpu();
            gpu_times[i] = gpu_timer::to_milliseconds(mctx.gpu_timer.evaluate(timer_index, 0), gpu_freq);
        }
        power_collector->leave_scope();

        // Obtain pipeline statistics.
        log::instance().write_line(log_level::debug, "Collecting pipeline "
                                                     "statistics ...");
        {
            auto cmd_list = cmd_lists[this->buffer_index()];
            reset_command_list(cmd_list);

            auto heap = _descriptor_heaps[this->buffer_index()].get();

            cmd_list->SetComputeRootSignature(global_root_sig_.get());
            cmd_list->SetDescriptorHeaps(1, &heap);
            
            stats_query.begin_frame();

            stats_query.begin(cmd_list.get(), 0);
            cmd_list->ExecuteBundle(dxr_bundles[this->buffer_index()].get());
            transition_resource(cmd_list.get(), render_targets_[this->buffer_index()].get(),
                D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);

            enable_target(cmd_list.get(), this->buffer_index(), D3D12_RESOURCE_STATE_COPY_DEST);
            //clear_target(cmd_list.get(), this->buffer_index());
            copy_to_target(cmd_list.get(), render_targets_[this->buffer_index()].get(), this->buffer_index());
            disable_target(cmd_list.get(), this->buffer_index(), D3D12_RESOURCE_STATE_COPY_DEST);

            transition_resource(cmd_list.get(), render_targets_[this->buffer_index()].get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
            stats_query.end(cmd_list.get(), 0);
            const auto stats_index = stats_query.end_frame(cmd_list.get());

            device.close_and_execute_command_list(cmd_list);
            this->present_target(config);

            // Wait until the results are here.
            device.wait_for_gpu();

            pipeline_stats = stats_query.evaluate(stats_index, 0);
        }
    }

    const auto gpu_median = calc_median(gpu_times);
    // Prepare the result set.
    auto retval = std::make_shared<basic_result>(config,
        std::initializer_list<std::string>{"benchmark", "particles", "data_extents", "ia_vertices", "ia_primitives",
            "vs_invokes", "gs_invokes", "gs_primitives", "c_invokes", "c_primitives", "ps_invokes", "hs_invokes",
            "ds_invokes", "cs_invokes", "gpu_time_min",
            "gpu_time_med", "gpu_time_max", "wall_time_iterations", "wall_time", "wall_time_avg"});

    // Output the results.
    retval->add({this->name(), this->data_.spheres(), this->data_.extents(), pipeline_stats.IAVertices,
        pipeline_stats.IAPrimitives, pipeline_stats.VSInvocations, pipeline_stats.GSInvocations,
        pipeline_stats.GSPrimitives, pipeline_stats.CInvocations, pipeline_stats.CPrimitives,
        pipeline_stats.PSInvocations, pipeline_stats.HSInvocations, pipeline_stats.DSInvocations,
        pipeline_stats.CSInvocations, gpu_times.front(),
        gpu_median, gpu_times.back(), mctx.cpu_iterations, cpu_time,
        static_cast<double>(cpu_time) / mctx.cpu_iterations});

    return retval;
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

    std::vector<UINT> heap_increment_sizes(pipeline_depth());
    for (int i = 0; i < pipeline_depth(); ++i) {
        heap_increment_sizes[i] = d3dDevice->GetDescriptorHandleIncrementSize(_descriptor_heaps[i]->GetDesc().Type);
    }
    heap_increment_sizes.push_back(
        d3dDevice->GetDescriptorHandleIncrementSize(_descriptor_heaps.back()->GetDesc().Type));


    auto aabb_buffer = create_buffer(device.d3d_device(), data_.spheres() * sizeof(AABB), 0,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    set_debug_object_name(aabb_buffer, "AABBBuffer");
    // set AABB view
    {
        auto const uavDescHandle =
            CD3DX12_CPU_DESCRIPTOR_HANDLE(_descriptor_heaps.back()->GetCPUDescriptorHandleForHeapStart(),
                CBV_SRV_UAV_Desc_Compute_Heap_Slots::AABBBuffer, heap_increment_sizes.back());
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = data_.spheres();
        uavDesc.Buffer.StructureByteStride = sizeof(AABB);
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        d3dDevice->CreateUnorderedAccessView(aabb_buffer.get(), nullptr, &uavDesc, uavDescHandle);
    }

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
            auto heap = _descriptor_heaps.back().get();
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
                compute_constants_->dispatchSize = {
                    static_cast<UINT>(thread_group_size_x / 32 + 1), thread_group_size_y, 1};
                compute_constants_->num_particles = num_particles;
                dxrCmdList->Dispatch(compute_constants_->dispatchSize.x, compute_constants_->dispatchSize.y,
                    compute_constants_->dispatchSize.z);

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

            device.close_and_execute_command_list(cmd_list);
            device.wait_for_gpu();
        }
    }
}

} // namespace trrojan::d3d12