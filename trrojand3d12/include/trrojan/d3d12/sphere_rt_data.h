#pragma once

#include <d3d12.h>

#include <winrt/base.h>

#include <glm/glm.hpp>

#include "trrojan/d3d12/export.h"
#include "trrojan/random_sphere_generator.h"

namespace trrojan::d3d12 {
    class TRROJAND3D12_API sphere_rt_data final {
    public:
        sphere_rt_data();
        ~sphere_rt_data() = default;

        winrt::com_ptr<ID3D12Resource> load(ID3D12GraphicsCommandList* command_list, D3D12_RESOURCE_STATES state, std::string const& path, std::uint32_t frame);

        operator bool(void) const noexcept {
            return (this->data_ != nullptr);
        }
    private:
        void fit_bounding_box(const mmpld::list_header& header,
            const void* particles);

        winrt::com_ptr<ID3D12Resource> data_;

        std::array<glm::vec3, 2> _bbox;
        float _max_radius;
    };
}