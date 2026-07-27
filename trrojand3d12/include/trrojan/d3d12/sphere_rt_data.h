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

        winrt::com_ptr<ID3D12Resource> load(ID3D12GraphicsCommandList* command_list, std::string const& path, std::uint32_t frame);

        operator bool(void) const noexcept {
            return (this->data_ != nullptr);
        }

        void clear();

        const std::array<glm::vec3, 2>& bbox(void) const noexcept {
            return this->_bbox;
        }

        const glm::vec3& bbox_start(void) const noexcept {
            return this->_bbox[0];
        }

        const glm::vec3& bbox_end(void) const noexcept {
            return this->_bbox[1];
        }

        std::array<float, 3> extents(void) const;

        UINT spheres(void) const noexcept {
            return this->_cnt_spheres;
        }

        winrt::com_ptr<ID3D12Resource> data(void) const noexcept {
            return this->data_;
        }

        const float max_radius(void) const noexcept {
            return this->_max_radius;
        }
    private:
        void fit_bounding_box(const mmpld::list_header& header,
            const void* particles);

        winrt::com_ptr<ID3D12Resource> data_;

        std::array<glm::vec3, 2> _bbox;
        float _max_radius;
        UINT _cnt_spheres;
    };
}