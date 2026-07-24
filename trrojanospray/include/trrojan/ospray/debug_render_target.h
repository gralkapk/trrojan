// <copyright file="debug_render_target.h" company="Visualisierungsinstitut der Universität Stuttgart">
// Copyright © 2022 - 2024 Visualisierungsinstitut der Universität Stuttgart.
// Licensed under the MIT licence. See LICENCE.txt file in the project root for full licence information.
// </copyright>
// <author>Christoph Müller</author>

#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include <Windows.h>

#include <winrt/base.h>

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/render_target_base.h"
#include "trrojan/ospray/device.h"

#include <d3d12.h>
#include <dxgi1_4.h>


/* Forward declarations. */
struct DebugConstants;


namespace trrojan {
namespace ospray {

#if !defined(TRROJAN_FOR_UWP)
    /// <summary>
    /// The debug view is a render target using a visible window.
    /// </summary>
    /// <remarks>
    /// This debug render target works by copying the data from a UAV that is
    /// used as the actual render target. This way, it is ensured that we
    /// actually see what the real thing is doing rather than having a
    /// completely different setup that might hide or induce undesired effects.
    /// </remarks>
    class TRROJANOSPRAY_API debug_render_target : public render_target_base {

    public:

        /// <summary>
        /// Initialises a new instance.
        /// </summary>
        debug_render_target(const trrojan::ospray::device& device);

        /// <summary>
        /// Finalises the instance.
        /// </summary>
        virtual ~debug_render_target(void);

        /// <inheritdoc />
        unsigned int present(const unsigned int sync_interval) override;

        // <inheritdoc />
        void resize(unsigned int width, unsigned int height) override;

        ///// <inheritdoc />
        //void to_uav(const D3D12_CPU_DESCRIPTOR_HANDLE dst,
        //    ID3D12GraphicsCommandList *cmd_list) override;

    protected:

        /// <inheritdoc />
        void reset_buffers(void) override;

        UINT pipeline_depth(void) const {
            return 2;
        }

        winrt::com_ptr<IDXGISwapChain3> create_swap_chain(HWND hWnd);

    private:

        typedef render_target_base base;

        void debug_render_target::switch_buffer(const UINT next_buffer);

        winrt::com_ptr<ID3D12DescriptorHeap> create_descriptor_heap(
            const D3D12_DESCRIPTOR_HEAP_TYPE type, const UINT cnt);
        void create_rtv_heap();
        void create_dsv_heap();
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle(const UINT frame);
        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle(const UINT frame) {
            return this->_dsv_heap->GetCPUDescriptorHandleForHeapStart();
        }
        void set_buffers(const std::vector<winrt::com_ptr<ID3D12Resource>>& buffers, const UINT buffer_index);
        void transition_resource(ID3D12GraphicsCommandList* cmd_list, ID3D12Resource* resource,
            D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

        /// <summary>
        /// The name of the window class we are registering for the debug view.
        /// </summary>
        static const TCHAR *WINDOW_CLASS;

        /// <summary>
        /// The handler for window messages which makes the message pump exit if
        /// the escape key was pressed.
        /// </summary>
        static LRESULT WINAPI wnd_proc(HWND hWnd, UINT msg, WPARAM wParam,
            LPARAM lParam);

        /// <summary>
        /// Runs the message dispatcher.
        /// </summary>
        void do_msg(void);

        winrt::com_ptr<ID3D12Device> _d3d12_device;
        winrt::com_ptr<IDXGIFactory4> _dxgi_factory;
        winrt::com_ptr<ID3D12CommandQueue> _command_queue;
        std::vector<winrt::com_ptr<ID3D12Resource>> _buffers;
        winrt::com_ptr<ID3D12Resource> _depth_buffer;
        UINT _buffer_index;

        winrt::com_ptr<ID3D12DescriptorHeap> _rtv_heap;
        winrt::com_ptr<ID3D12DescriptorHeap> _dsv_heap;
        UINT _rtv_descriptor_size = 0;

        std::vector<UINT64> _fence_values;
        winrt::com_ptr<ID3D12Fence> _fence;
        HANDLE _fence_event;

        std::vector<winrt::com_ptr<ID3D12CommandAllocator>> _command_allocators;
        std::vector<winrt::com_ptr<ID3D12GraphicsCommandList>> _command_lists;

        D3D12_VIEWPORT _viewport;

        /// <summary>
        /// The message pumping thread.
        /// </summary>
        std::thread _msg_pump;

        /// <summary>
        /// The staging buffer used to back the UAV that we provide to external
        /// users for writing to the debug render target. If the render target
        /// is enabled, this staging buffer will actually become the target.
        /// Before presenting the debug target, its content will be copied to
        /// the back buffer of the swap chain.
        /// </summary>
        winrt::com_ptr<ID3D12Resource> _staging_buffer;
        void* _staging_buffer_mapped = nullptr;

        /// <summary>
        /// The swap chain for the window.
        /// </summary>
        winrt::com_ptr<IDXGISwapChain3> _swap_chain;

        /// <summary>
        /// The handle of the debug window.
        /// </summary>
        std::atomic<HWND> _wnd;
    };
#endif /* !defined(TRROJAN_FOR_UWP) */

} /* namespace ospray */
} /* namespace trrojan */
