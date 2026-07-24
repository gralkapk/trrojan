// <copyright file="debug_render_target.cpp" company="Visualisierungsinstitut der Universität Stuttgart">
// Copyright © 2022 - 2024 Visualisierungsinstitut der Universität Stuttgart.
// Licensed under the MIT licence. See LICENCE.txt file in the project root for full licence information.
// </copyright>
// <author>Christoph Müller</author>

#include "trrojan/ospray/debug_render_target.h"

#include <cassert>
#include <sstream>

#include <tchar.h>

#include "trrojan/com_error_category.h"
#include "trrojan/log.h"

namespace trrojan::ospray {

#if !defined(TRROJAN_FOR_UWP)
/*
 * trrojan::d3d12::debug_render_target::debug_render_target
 */
debug_render_target::debug_render_target(const trrojan::ospray::device& device)
        : base()
        , _d3d12_device(device.get_d3d12Device())
        , _dxgi_factory(device.get_d3d12Factory())
        , _command_queue(device.command_queue())
        , _buffers(pipeline_depth())
        , _buffer_index(0)
        , _fence_values(pipeline_depth(), 0)
        , _command_allocators(pipeline_depth(), nullptr)
        , _command_lists(pipeline_depth(), nullptr)
        , _wnd(NULL) {
    assert(this->_wnd.is_lock_free());
    this->_msg_pump = std::thread(std::bind(&debug_render_target::do_msg, std::ref(*this)));

    // create fence
    {
        auto hr = this->_d3d12_device->CreateFence(
            _fence_values[_buffer_index], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
        if (FAILED(hr)) {
            throw std::system_error(hr, com_category());
        }

        ++_fence_values[_buffer_index];
    }

    _fence_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    if (_fence_event == NULL) {
        throw std::system_error(::GetLastError(), std::system_category());
    }

    // create command list
    {
        for (UINT i = 0; i < pipeline_depth(); ++i) {
            auto hr = this->_d3d12_device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_command_allocators[i]));
            if (FAILED(hr)) {
                throw std::system_error(hr, com_category());
            }
            hr = this->_d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _command_allocators[i].get(),
                nullptr, IID_PPV_ARGS(&_command_lists[i]));
            _command_lists[i]->Close();
        }
    }
}


/*
 * trrojan::d3d12::debug_render_target::~debug_render_target
 */
debug_render_target::~debug_render_target(void) {
    //auto hWnd = this->hWnd.exchange(NULL);
    //if (hWnd != NULL) {
    //    ::SendMessage(hWnd, WM_CLOSE, 0, 0);
    //}

    if (this->_msg_pump.joinable()) {
        log::instance().write_line(log_level::information, "Please close the "
                                                           "debug view to end the programme.");
        this->_msg_pump.join();
    }

    CloseHandle(_fence_event);
}


/*
 * trrojan::d3d12::debug_render_target::present
 */
UINT debug_render_target::present(const unsigned int sync_interval) {
    assert(this->_swap_chain != nullptr);
    
    // TODO: Copy OSPRay render target to the back buffer of the swap chain. This is currently not implemented, so the debug view will always be black.
    copyColorTo(_staging_buffer_mapped);

    auto retval = this->_swap_chain->GetCurrentBackBufferIndex();
    auto current_cmd_list = _command_lists[retval].get();
    _command_queue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList**>(&current_cmd_list));

    // Swap the buffers.
    this->_swap_chain->Present(sync_interval, 0);

    // Switch to the next buffer used by the swap chain.
    switch_buffer(retval);

    return retval;
}


D3D12_CPU_DESCRIPTOR_HANDLE debug_render_target::rtv_handle(const UINT frame) {
    auto retval = this->_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    retval.ptr += static_cast<SIZE_T>(frame) * this->_rtv_descriptor_size;
    return retval;
}


/*
 * trrojan::d3d12::debug_render_target::resize
 */
void debug_render_target::resize(const unsigned int width, const unsigned int height) {
    log::instance().write_line(log_level::debug,
        "Resizing debug render target "
        "{:p} to [{}, {}].",
        static_cast<void*>(this), width, height);
    
    // Resize the window to match the requested client area. This must be done
    // before the swap chain is created, because the swap chain is created to
    // match the client area of the window.
    {
        DWORD style = ::GetWindowLong(this->_wnd, GWL_STYLE);
        DWORD styleEx = ::GetWindowLong(this->_wnd, GWL_EXSTYLE);
        RECT wndRect;

        wndRect.left = 0;
        wndRect.top = 0;
        wndRect.right = width;
        wndRect.bottom = height;
        if (::AdjustWindowRectEx(&wndRect, style, FALSE, styleEx) == FALSE) {
            auto hr = __HRESULT_FROM_WIN32(::GetLastError());
            throw std::system_error(hr, com_category());
        }

        ::SetWindowPos(this->_wnd, HWND_TOP, 0, 0, wndRect.right - wndRect.left, wndRect.bottom - wndRect.top,
            SWP_NOMOVE | SWP_SHOWWINDOW);
    }

    if (this->_swap_chain == nullptr) {
        // Initial call to resize, need to create the swap chain.
        //assert(this->device() != nullptr);

        while (this->_wnd.load() == NULL) {
            log::instance().write_line(log_level::verbose, "Waiting for the "
                                                           "debug view become available ...");
        }

        this->_swap_chain = this->create_swap_chain(this->_wnd);

        ::ShowWindow(this->_wnd, SW_SHOW);

    } else {
        // Resize an existing swap chain.
        DXGI_SWAP_CHAIN_DESC desc;

        this->reset_buffers();

        {
            auto hr = this->_swap_chain->GetDesc(&desc);
            if (FAILED(hr)) {
                throw std::system_error(hr, com_category());
            }
        }

        {
            auto hr = this->_swap_chain->ResizeBuffers(desc.BufferCount, width, height, desc.BufferDesc.Format, 0);
            if (FAILED(hr)) {
                throw std::system_error(hr, com_category());
            }
        }

    } /* end if (this->swapChain == nullptr) */

    // Re-create the RTV/DSV.
    {
        std::vector<winrt::com_ptr<ID3D12Resource>> buffers(this->pipeline_depth());

        for (UINT i = 0; i < this->pipeline_depth(); ++i) {
            auto hr = this->_swap_chain->GetBuffer(i, IID_PPV_ARGS(&buffers[i]));
            if (FAILED(hr)) {
                throw std::system_error(hr, com_category());
            }

            std::stringstream name;
            name << "debug_render_target (colour buffer " << i << ")";
            //set_debug_object_name(buffers[i], name.str().c_str());
        }
        auto desc = buffers[0]->GetDesc();
        render_target_base::resize(desc.Width, desc.Height);
        this->set_buffers(std::move(buffers), this->_swap_chain->GetCurrentBackBufferIndex());
    }

    // record command list for copying OSPRay render target to the back buffer of the swap chain
    {
        std::array<float, 4> clearColor = {1.0f, 0.0f, 0.0f, 1.0f};
        for (UINT i = 0; i < this->pipeline_depth(); ++i) {
            auto& cmd_list = this->_command_lists[i];
            auto& cmd_allocator = this->_command_allocators[i];
            cmd_allocator->Reset();
            cmd_list->Reset(cmd_allocator.get(), nullptr);

            transition_resource(cmd_list.get(), this->_buffers[i].get(), D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET);

            cmd_list->RSSetViewports(1, &this->_viewport);
            D3D12_RECT scissorRect = {0, 0, static_cast<LONG>(this->getWidth()), static_cast<LONG>(this->getHeight())};
            cmd_list->RSSetScissorRects(1, &scissorRect);

            {
                auto rtvH = this->rtv_handle(i);
                auto dsvH = this->dsv_handle(i);
                cmd_list->OMSetRenderTargets(1, &rtvH, FALSE, &dsvH);
                cmd_list->ClearRenderTargetView(rtvH, clearColor.data(), 0, nullptr);
                cmd_list->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            }

            transition_resource(cmd_list.get(), this->_buffers[i].get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
            srcLocation.pResource = this->_staging_buffer.get();
            srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLocation.SubresourceIndex = 0;
            srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srcLocation.PlacedFootprint.Footprint.Width = this->getWidth();
            srcLocation.PlacedFootprint.Footprint.Height = this->getHeight();
            srcLocation.PlacedFootprint.Footprint.Depth = 1;
            srcLocation.PlacedFootprint.Footprint.RowPitch = this->getWidth() * 4; // Assuming 4 bytes per pixel (RGBA8)
            srcLocation.PlacedFootprint.Offset = 0;
            D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
            dstLocation.pResource = this->_buffers[i].get();
            dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLocation.SubresourceIndex = 0;
            dstLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            dstLocation.PlacedFootprint.Footprint.Width = this->getWidth();
            dstLocation.PlacedFootprint.Footprint.Height = this->getHeight();
            dstLocation.PlacedFootprint.Footprint.Depth = 1;
            dstLocation.PlacedFootprint.Footprint.RowPitch = this->getWidth() * 4; // Assuming 4 bytes per pixel (RGBA8)
            cmd_list->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
            transition_resource(cmd_list.get(), this->_buffers[i].get(), D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PRESENT);

            cmd_list->Close();
        }
    }
}


/*
 * trrojan::d3d12::debug_render_target::to_uav
 */
//void trrojan::d3d12::debug_render_target::to_uav(
//        const D3D12_CPU_DESCRIPTOR_HANDLE dst,
//        ID3D12GraphicsCommandList *cmd_list) {
//    if (this->_staging_buffer == nullptr) {
//        winrt::com_ptr<ID3D12Resource> texture;
//
//        {
//            auto hr = this->_swap_chain->GetBuffer(0, ::IID_ID3D12Resource,
//                reinterpret_cast<void **>(&texture));
//            if (FAILED(hr)) {
//                throw std::system_error(hr, com_category());
//            }
//        }
//
//        auto desc = texture->GetDesc();
//        this->_staging_buffer = create_resource(this->device(), desc);
//    }
//
//    this->device()->CreateUnorderedAccessView(this->_staging_buffer, nullptr,
//        nullptr, dst);
//}


/*
 * trrojan::d3d12::debug_render_target::reset_buffers
 */
void debug_render_target::reset_buffers(void) {
    render_target_base::reset_buffers();
    // Make sure that the staging buffer is re-created when the target UAV
    // is requested the next time.
    //this->_staging_buffer = nullptr;
}

winrt::com_ptr<IDXGISwapChain3> debug_render_target::create_swap_chain(HWND hWnd) {
    assert(this->_d3d12_device != nullptr);
    assert(this->_dxgi_factory != nullptr);
    assert(this->_command_queue != nullptr);

    winrt::com_ptr<IDXGISwapChain3> retval;
    winrt::com_ptr<IDXGISwapChain1> swapChain;

    UINT width = 1, height = 1;
    {
        RECT clientRect;
        if (GetClientRect(hWnd, &clientRect)) {
            width = clientRect.right - clientRect.left;
            height = clientRect.bottom - clientRect.top;
        }
    }

    {
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Stereo = FALSE;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = pipeline_depth();
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Flags = 0;

        auto hr = this->_dxgi_factory->CreateSwapChainForHwnd(
            this->_command_queue.get(), hWnd, &desc, nullptr, nullptr, swapChain.put());
        if (FAILED(hr)) {
            throw std::system_error(hr, com_category());
        }
    }

    {
        auto hr = this->_dxgi_factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
        if (FAILED(hr)) {
            throw std::system_error(hr, com_category());
        }
    }

    if (!swapChain.try_as(retval)) {
        throw std::system_error(E_NOINTERFACE, com_category());
    }

    return retval;
}

void debug_render_target::switch_buffer(const UINT next_buffer) {
    assert(next_buffer < this->_fence_values.size());
    const auto complete_value = this->_fence_values[this->_buffer_index];

    //#if (defined(DEBUG) || defined(_DEBUG))
    //    log::instance().write_line(log_level::debug, "Render target is switching "
    //        "from buffer {0} ({1:p}) to buffer {2} ({3:p}), using fence {4} to "
    //        "to wait ...",
    //        this->_buffer_index,
    //        static_cast<void *>(this->_buffers[this->_buffer_index]),
    //        next_buffer,
    //        static_cast<void *>(this->_buffers[next_buffer]),
    //        complete_value);
    //#endif /* (defined(DEBUG) || defined(_DEBUG)) */

    // Schedule a fence for the current frame in the command queue. We will
    // wait for this one to become signalled if we activate this buffer again.
    assert(this->_fence != nullptr);
    {
        auto hr = this->_command_queue->Signal(this->_fence.get(), complete_value);
        if (FAILED(hr)) {
            throw std::system_error(hr, com_category());
        }
    }

    // Move to next frame.
    this->_buffer_index = next_buffer;
    auto& fence_value = this->_fence_values[this->_buffer_index];

    // If the next frame is not yet ready, ie the previously scheduled fence
    // was not signalled, wait for it.
    const auto completed_value = this->_fence->GetCompletedValue();
    if (completed_value < fence_value) {
        assert(this->_fence_event != NULL);
        auto hr = this->_fence->SetEventOnCompletion(fence_value, this->_fence_event);
        if (FAILED(hr)) {
            throw std::system_error(hr, com_category());
        }

        switch (WaitForSingleObjectEx(this->_fence_event, INFINITE, FALSE)) {
        case WAIT_FAILED:
            throw std::system_error(::GetLastError(), std::system_category());
        default:
            break;
        }
    }

    // Set the value for the next frame ('fence_value' is ref!), which
    // must be after the frame we have just completed.
    fence_value = complete_value + 1;
}

winrt::com_ptr<ID3D12DescriptorHeap> debug_render_target::create_descriptor_heap(
    const D3D12_DESCRIPTOR_HEAP_TYPE type, const UINT cnt) {
    assert(this->_d3d12_device != nullptr);
    winrt::com_ptr<ID3D12DescriptorHeap> retval;

    D3D12_DESCRIPTOR_HEAP_DESC desc;
    ::ZeroMemory(&desc, sizeof(desc));
    desc.NumDescriptors = cnt;
    desc.Type = type;

    auto hr =
        this->_d3d12_device->CreateDescriptorHeap(&desc, ::IID_ID3D12DescriptorHeap, reinterpret_cast<void**>(&retval));
    if (FAILED(hr)) {
        throw std::system_error(hr, com_category());
    }

    return retval;
}

void debug_render_target::create_rtv_heap() {
    this->_rtv_heap = this->create_descriptor_heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, this->pipeline_depth());
    this->_rtv_descriptor_size = this->_d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    log::instance().write_line(log_level::debug,
        "Render target {:p} allocated "
        "RTV descriptor heap {:p} with descriptor size {}.",
        static_cast<void*>(this), static_cast<void*>(this->_rtv_heap.get()), this->_rtv_descriptor_size);
}

void debug_render_target::create_dsv_heap() {
    this->_dsv_heap = this->create_descriptor_heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
    log::instance().write_line(log_level::debug,
        "Render target {:p} allocated "
        "DSV descriptor heap {:p}.",
        static_cast<void*>(this), static_cast<void*>(this->_dsv_heap.get()));
}

void debug_render_target::set_buffers(
    const std::vector<winrt::com_ptr<ID3D12Resource>>& buffers, const UINT buffer_index) {
    if (buffers.size() < this->_buffers.size()) {
        throw std::invalid_argument("A buffer must be provided for each stage "
                                    "of the pipeline. You have provided less buffers than there are "
                                    "render target views.");
    }
    if (buffer_index >= this->pipeline_depth()) {
        throw std::invalid_argument("The current buffer index cannot be "
                                    "outside the range of valid buffers.");
    }

    log::instance().write_line(log_level::debug,
        "Setting {0} new colour "
        "buffer(s) in render target {1:p} ({2} provided by caller). The next "
        "frame is reset to be at {3}.",
        this->_buffers.size(), static_cast<void*>(this), buffers.size(), buffer_index);

    // Retain the buffers.
    std::copy_n(buffers.begin(), this->_buffers.size(), this->_buffers.begin());

    // Reset the current buffer to the user-defined value. This allows swap
    // chain-based subclasses to force the current index of the swap chain.
    this->_buffer_index = buffer_index;

    // Lazily create the RTV heap.
    if (this->_rtv_heap == nullptr) {
        this->create_rtv_heap();
    }

    // Create the RTVs from the buffers.
    {
        auto handle = this->_rtv_heap->GetCPUDescriptorHandleForHeapStart();
        for (auto& b : this->_buffers) {
            log::instance().write_line(log_level::debug,
                "Colour buffer "
                "{0:p} is render target view 0x{1:x}.",
                static_cast<void*>(b.get()), handle.ptr);
            this->_d3d12_device->CreateRenderTargetView(b.get(), nullptr, handle);
            handle.ptr += this->_rtv_descriptor_size;
        }
    }

    // Lazily create the DSV heap.
    if (this->_dsv_heap == nullptr) {
        this->create_dsv_heap();
    }

    // Create the depth buffer and the DSV.
    this->_depth_buffer = nullptr;

    {
        D3D12_CLEAR_VALUE clearValue;
        ::ZeroMemory(&clearValue, sizeof(clearValue));
        clearValue.Format = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth = 1.0f; // TODO: inverse depth?
        clearValue.DepthStencil.Stencil = 0;

        auto desc = buffers.front()->GetDesc();
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

        D3D12_HEAP_PROPERTIES props;
        ::ZeroMemory(&props, sizeof(props));
        props.Type = D3D12_HEAP_TYPE_DEFAULT;
        props.CreationNodeMask = 1;
        props.VisibleNodeMask = 1;

        auto hr = this->_d3d12_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, ::IID_ID3D12Resource,
            reinterpret_cast<void**>(&this->_depth_buffer));
        if (FAILED(hr)) {
            throw std::system_error(hr, com_category());
        }

        //set_debug_object_name(this->_depth_buffer, "depth_buffer");
    }

    {
        D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

        auto handle = this->_dsv_heap->GetCPUDescriptorHandleForHeapStart();
        log::instance().write_line(log_level::debug,
            "Depth buffer {0:p} "
            "is depth/stencil view 0x{1:x}.",
            static_cast<void*>(this->_depth_buffer.get()), handle.ptr);
        this->_d3d12_device->CreateDepthStencilView(this->_depth_buffer.get(), &desc, handle);
    }

    // create staging buffer for copying OSPRay render target to the back buffer of the swap chain
    {
        auto bDdesc = buffers.front()->GetDesc();
        D3D12_RESOURCE_DESC desc = {};
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.Width = bDdesc.Width * bDdesc.Height * 4;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.DepthOrArraySize = 1;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        D3D12_HEAP_PROPERTIES props = {};
        props.Type = D3D12_HEAP_TYPE_UPLOAD;
        props.CreationNodeMask = 1;
        props.VisibleNodeMask = 1;

        auto hr = this->_d3d12_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&_staging_buffer));
        if (FAILED(hr)) {
            throw std::system_error(hr, com_category());
        }

        hr = _staging_buffer->Map(0, nullptr, reinterpret_cast<void**>(&_staging_buffer_mapped));
        if (FAILED(hr)) {
            throw std::system_error(hr, com_category());
        }
    }

    // Fill the viewport dimensions.
    {
        auto desc = buffers.front()->GetDesc();
        this->_viewport.Height = static_cast<float>(desc.Height);
        this->_viewport.MaxDepth = 1.0f;
        this->_viewport.MinDepth = 0.0f;
        this->_viewport.TopLeftX = 0.0f;
        this->_viewport.TopLeftY = 0.0f;
        this->_viewport.Width = static_cast<float>(desc.Width);
        log::instance().write_line(log_level::debug,
            "Viewport starts at "
            "({}, {}) with size [{}, {}].",
            this->_viewport.TopLeftX, this->_viewport.TopLeftY, this->_viewport.Width, this->_viewport.Height);
    }
}

void debug_render_target::transition_resource(ID3D12GraphicsCommandList* cmd_list, ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    cmd_list->ResourceBarrier(1, &barrier);
}


/*
 * trrojan::d3d12::debug_render_target::WINDOW_CLASS
 */
const TCHAR* debug_render_target::WINDOW_CLASS = _T("trrojectd3d12dbg");


/*
 * trrojan::d3d12::debug_render_target::wnd_proc
 */
LRESULT debug_render_target::wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto that = reinterpret_cast<debug_render_target*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    CREATESTRUCTW* cs = nullptr;
    LRESULT retval = 0;

    switch (msg) {
    case WM_CREATE:
        cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        if ((::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams)) == 0)) {
            retval = ::GetLastError();
        } else {
            retval = 0;
        }
        break;

    case WM_CLOSE:
        ::PostQuitMessage(0);
        break;

    case WM_KEYDOWN:
        if ((wParam == VK_ESCAPE) && (that != nullptr)) {
            ::PostQuitMessage(0);
        }
        break;

    default:
        retval = ::DefWindowProc(hWnd, msg, wParam, lParam);
        break;
    }

    return retval;
}


/*
 * trrojan::d3d12::debug_render_target::do_msg
 */
void debug_render_target::do_msg(void) {
    MSG msg;

    const auto hInstance = ::GetModuleHandle(NULL);
    if (hInstance == NULL) {
        auto hr = __HRESULT_FROM_WIN32(::GetLastError());
        throw std::system_error(hr, com_category());
    }

    {
        WNDCLASSEX wndClass;
        if (!::GetClassInfoEx(hInstance, WINDOW_CLASS, &wndClass)) {
            ::ZeroMemory(&wndClass, sizeof(WNDCLASSEX));
            wndClass.cbSize = sizeof(WNDCLASSEX);
            wndClass.style = CS_CLASSDC;
            wndClass.lpfnWndProc = debug_render_target::wnd_proc;
            wndClass.hInstance = hInstance;
            wndClass.lpszClassName = WINDOW_CLASS;

            if (!::RegisterClassEx(&wndClass)) {
                auto hr = __HRESULT_FROM_WIN32(::GetLastError());
                throw std::system_error(hr, com_category());
            }
        }
    }

    {
        DWORD style = WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX;
        DWORD styleEx = 0;

        /* Create the window. */
        this->_wnd = ::CreateWindowEx(styleEx, WINDOW_CLASS, _T("TRRojan"), style, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, this);
        if (this->_wnd.load() == NULL) {
            auto hr = __HRESULT_FROM_WIN32(::GetLastError());
            throw std::system_error(hr, com_category());
        }
    }

    while (::GetMessage(&msg, NULL, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
}
#endif /* !defined(TRROJAN_FOR_UWP) */

} // namespace trrojan::ospray
