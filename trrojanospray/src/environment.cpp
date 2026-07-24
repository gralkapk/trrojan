#include "trrojan/ospray/environment.h"

#include <assert.h>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <dxgidebug.h>

#include <winrt/base.h>

#include "trrojan/log.h"
#include "trrojan/text.h"
#include "trrojan/com_error_category.h"

namespace trrojan::ospray {
environment::environment() : environment_base{"ospray"} {
    ospLoadModule("cpu");
}

size_t environment::get_devices(device_list& dst) {
    for (auto d : this->_devices) {
        auto dd = std::dynamic_pointer_cast<trrojan::device_base>(d);
        assert(dd != nullptr);
        dst.push_back(std::move(dd));
    }
    return _devices.size();
}

void environment::on_activate(void) {}

void environment::on_deactivate(void) {}

void environment::on_finalise(void) {
    this->_devices.clear();
}

void environment::on_initialise(const cmd_line& cmdLine) {
    // Init debug interface
    UINT factoryFlags = 0;
#ifdef DEBUG
    winrt::com_ptr<ID3D12Debug> debug;
    {
        auto hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
        if (SUCCEEDED(hr)) {
            debug->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }

    {
        winrt::com_ptr<ID3D12Debug1> debug1;
        auto hr = debug->QueryInterface(IID_PPV_ARGS(&debug1));
        if (SUCCEEDED(hr)) {
            debug1->SetEnableGPUBasedValidation(true);
        }
    }
#endif

    winrt::com_ptr<IDXGIFactory4> dxgiFactory;
    // Create DXGI factory
    {
        auto hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&dxgiFactory));
        if (FAILED(hr)) {
            throw std::system_error(hr, com_category());
        }
    }

    // search for dedicated GPU
    winrt::com_ptr<ID3D12Device> d3d12_device;
    {
        auto hr = S_OK;
        winrt::com_ptr<IDXGIAdapter1> adapter;
        for (UINT a = 0; SUCCEEDED(hr) || (hr != DXGI_ERROR_NOT_FOUND); ++a) {
            hr = dxgiFactory->EnumAdapters1(a, adapter.put());
            if (FAILED(hr)) {
                throw std::system_error(hr, com_category());
            }
            DXGI_ADAPTER_DESC1 desc;
            hr = adapter->GetDesc1(&desc);
            if (FAILED(hr)) {
                throw std::system_error(hr, com_category());
            }
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                log::instance().write_line(
                    log_level::information, "Found dedicated GPU: \"{}\".", to_utf8(desc.Description));
                break;
            }
        }
        if (SUCCEEDED(hr)) {
            hr = D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&d3d12_device));
            if (FAILED(hr)) {
                throw std::system_error(hr, com_category());
            }
        }
    }

    // Initialize OSPRay device with D3D12 interop
    this->_devices.push_back(std::make_shared<device>(d3d12_device, dxgiFactory));
}
} // namespace trrojan::ospray
