#pragma once

#include <atomic>

#include "trrojan/device.h"

#include "trrojan/ospray/export.h"

#include <ospray/ospray.h>
#include <ospray/ospray_cpp.h>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>

#include <winrt/base.h>

namespace trrojan::ospray {
class TRROJANOSPRAY_API device : public trrojan::device_base {
public:
    using pointer = std::shared_ptr<device>;

    device(winrt::com_ptr<ID3D12Device> d3d12_device, winrt::com_ptr<IDXGIFactory4> d3d12_factory);
    virtual ~device();

    void close_and_execute_command_list(ID3D12GraphicsCommandList* cmd_list);
    void execute_command_list(ID3D12CommandList* cmd_list);

    void wait_for_gpu();

    winrt::com_ptr<ID3D12Device> get_d3d12Device() const {
        return _d3d12_device;
    }

    winrt::com_ptr<IDXGIFactory4> get_d3d12Factory() const {
        return _dxgi_factory;
    }

    winrt::com_ptr<ID3D12CommandQueue> command_queue() const {
        return _command_queue;
    }

private:
    winrt::com_ptr<ID3D12CommandQueue> make_cmd_queue();
    winrt::com_ptr<ID3D12Fence> make_fence();

    ::ospray::cpp::Device _osp_device;
    winrt::com_ptr<ID3D12Device> _d3d12_device;
    winrt::com_ptr<IDXGIFactory4> _dxgi_factory;

    winrt::com_ptr<ID3D12CommandQueue> _command_queue;
    winrt::com_ptr<ID3D12Fence> _fence;
    std::atomic<UINT64> _next_fence;
};
} // namespace trrojan::ospray
