#pragma once

#include "trrojan/configuration.h"

#include "trrojan/d3d12/export.h"

namespace trrojan::d3d12 {
    class TRROJAND3D12_API sphere_rt_rendering_configuration final {
    public:
        static const char *factor_data_set;
        static const char *factor_frame;
        static const char *factor_gpu_counter_iterations;
        static const char *factor_min_prewarms;
        static const char *factor_min_wall_time;
        static const char *factor_spp;
        static const char *factor_rec_depth;

        explicit sphere_rt_rendering_configuration(const configuration& config);

        std::string data_set(void) const noexcept {
            return this->_data_set;
        }

        std::uint32_t frame(void) const noexcept {
            return this->_frame;
        }

        std::uint32_t gpu_counter_iterations(void) const noexcept {
            return this->_gpu_counter_iterations;
        }

        std::uint32_t min_prewarms(void) const noexcept {
            return this->_min_prewarms;
        }

        std::uint32_t min_wall_time(void) const noexcept {
            return this->_min_wall_time;
        }

        std::uint32_t spp(void) const noexcept {
            return this->_spp;
        }

        std::uint32_t rec_depth(void) const noexcept {
            return this->_rec_depth;
        }

    private:
        std::string _data_set;
        std::uint32_t _frame;
        std::uint32_t _gpu_counter_iterations;
        std::uint32_t _min_prewarms;
        std::uint32_t _min_wall_time;
        std::uint32_t _spp;
        std::uint32_t _rec_depth;
    };
} // namespace trrojan::d3d12