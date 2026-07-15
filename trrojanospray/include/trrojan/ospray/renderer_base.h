#pragma once

#include <cstdint>

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API renderer_base : public object {
public:
    static const char* factor_spp;
    static const char* factor_path_length;
    static const char* factor_ao_samples;
    static const char* factor_ao_distance;
    static const char* factor_volume_sampling_rate;

    renderer_base();
    virtual ~renderer_base();

private:
    std::int32_t _spp;
    std::int32_t _path_length;
    std::int32_t _ao_samples;
    float _ao_distance;
    float _volume_sampling_rate;
};
} // namespace trrojan::ospray