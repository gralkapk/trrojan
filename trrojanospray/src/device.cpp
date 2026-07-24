#include "trrojan/ospray/device.h"

#include "trrojan/com_error_category.h"

namespace trrojan::ospray {
device::device(winrt::com_ptr<ID3D12Device> d3d12_device, winrt::com_ptr<IDXGIFactory4> factory)
        : _osp_device{"cpu"}
        , _d3d12_device(d3d12_device)
        , _dxgi_factory(factory)
        , _next_fence(0) {
    assert(d3d12_device != nullptr);
    assert(factory != nullptr);

    std::string logOutput = "cout";
    std::string errorOutput = "cerr";
    _osp_device.setParam("logOutput", logOutput);
    _osp_device.setParam("errorOutput", errorOutput);

#ifdef DEBUG
    std::int32_t logLevel = OSP_LOG_DEBUG;

    bool debug = true;
    _osp_device.setParam("debug", debug);
#else
    std::int32_t logLevel = OSP_LOG_ERROR;
#endif // DEBUG
    _osp_device.setParam("logLevel", logLevel);

    std::int32_t num_threads = -1;
    _osp_device.setParam("numThreads", num_threads);

    bool affinity = false;
    _osp_device.setParam("setAffinity", affinity);

    bool disableMipMap = false;
    _osp_device.setParam("disableMipMapGeneration", disableMipMap);

    bool warnAsError = false;
    _osp_device.setParam("warnAsError", warnAsError);

    //-----------------------------------
    _osp_device.commit();
    _osp_device.setCurrent();

    _command_queue = make_cmd_queue();
    _fence = make_fence();
}

device::~device() {
    //ospDeviceRelease(_osp_device);
    ospShutdown();
}

void device::close_and_execute_command_list(ID3D12GraphicsCommandList* cmd_list) {
    assert(cmd_list != nullptr);
    auto hr = cmd_list->Close();
    if (FAILED(hr)) {
        throw std::system_error(hr, com_category());
    }
    execute_command_list(cmd_list);
}

void device::execute_command_list(ID3D12CommandList* cmd_list) {
    assert(cmd_list != nullptr);
    _command_queue->ExecuteCommandLists(1, &cmd_list);
}

void device::wait_for_gpu() {
    auto fence = _fence;
    auto value = _next_fence++;

    auto event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!event) {
        throw std::system_error(GetLastError(), std::system_category());
    }

    auto hr = fence->SetEventOnCompletion(value, event);
    if (FAILED(hr)) {
        CloseHandle(event);
        throw std::system_error(hr, com_category());
    }

    hr = _command_queue->Signal(fence.get(), value);
    if (FAILED(hr)) {
        CloseHandle(event);
        throw std::system_error(hr, com_category());
    }

    switch (WaitForSingleObjectEx(event, INFINITE, FALSE)) {
    case WAIT_FAILED:
        CloseHandle(event);
        throw std::system_error(GetLastError(), std::system_category());
    case WAIT_OBJECT_0:
        break;
    default:
        CloseHandle(event);
        throw std::system_error(E_FAIL, com_category());
    }

    CloseHandle(event);
}

winrt::com_ptr<ID3D12CommandQueue> device::make_cmd_queue() {
    assert(this->_d3d12_device != nullptr);
    winrt::com_ptr<ID3D12CommandQueue> retval;
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    auto hr = this->_d3d12_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&retval));
    if (FAILED(hr)) {
        throw std::system_error(hr, com_category());
    }
    return retval;
}

winrt::com_ptr<ID3D12Fence> device::make_fence() {
    assert(this->_d3d12_device != nullptr);
    winrt::com_ptr<ID3D12Fence> retval;
    auto hr = this->_d3d12_device->CreateFence(_next_fence++, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&retval));
    if (FAILED(hr)) {
        throw std::system_error(hr, com_category());
    }
    return retval;
}
} // namespace trrojan::ospray