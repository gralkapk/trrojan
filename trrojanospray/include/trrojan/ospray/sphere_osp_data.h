#pragma once

#include <glm/glm.hpp>

#include "trrojan/ospray/export.h"
#include "trrojan/random_sphere_generator.h"

namespace trrojan::ospray {
    class TRROJANOSPRAY_API sphere_osp_data final {
    public:
        sphere_osp_data();
        ~sphere_osp_data() = default;

        void load(
            std::string const& path, std::uint32_t frame);

        operator bool(void) const noexcept {
            return (!this->_positions.empty() && !this->_radii.empty() && !this->_colors.empty());
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

        UINT spheres(void) const noexcept {
            return this->_cnt_spheres;
        }

        /*RETVAL data(
            void) const noexcept {
            return {this->_positions, this->_radii, this->_colors};
        }*/

        const float max_radius(void) const noexcept {
            return this->_max_radius;
        }

        const std::vector<glm::vec3>& positions(void) const noexcept {
            return this->_positions;
        }

        const std::vector<float>& radii(void) const noexcept {
            return this->_radii;
        }

        const std::vector<glm::vec4>& colors(void) const noexcept {
            return this->_colors;
        }
    private:
        void fit_bounding_box(const mmpld::list_header& header,
            const void* particles);

        std::vector<glm::vec3> _positions;
        std::vector<float> _radii;
        std::vector<glm::vec4> _colors;

        std::array<glm::vec3, 2> _bbox;
        float _max_radius;
        UINT _cnt_spheres;
    };
}