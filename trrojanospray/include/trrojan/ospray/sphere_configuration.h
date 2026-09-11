#pragma once

#include "trrojan/configuration.h"

#include "trrojan/ospray/export.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API sphere_configuration final {
public:
    static const char* factor_data_set;
    static const char* factor_frame;
    static const char* factor_counter_iterations;
    static const char* factor_min_prewarms;
    static const char* factor_min_wall_time;
    static const char* factor_spp;
    static const char* factor_rec_depth;
    static const char* factor_ao_samples;

    explicit sphere_configuration(const configuration& config);

    std::string data_set(void) const noexcept {
        return this->_data_set;
    }

    std::uint32_t frame(void) const noexcept {
        return this->_frame;
    }

    std::uint32_t counter_iterations(void) const noexcept {
        return this->_counter_iterations;
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

    std::uint32_t ao_samples(void) const noexcept {
        return this->_ao_samples;
    }

private:
    std::string _data_set;
    std::uint32_t _frame;
    std::uint32_t _counter_iterations;
    std::uint32_t _min_prewarms;
    std::uint32_t _min_wall_time;
    std::uint32_t _spp;
    std::uint32_t _rec_depth;
    std::uint32_t _ao_samples;
};
} // namespace trrojan::ospray