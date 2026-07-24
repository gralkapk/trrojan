#pragma once

#include <cstdint>

#include "trrojan/configuration.h"

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

    struct config {
        std::int32_t spp;
        std::int32_t path_length;
        std::int32_t ao_samples;
        float ao_distance;
        float volume_sampling_rate;
    };

    renderer_base(const config& config);
    virtual ~renderer_base();

    OSPFuture renderFrame(OSPFrameBuffer frameBuffer, OSPCamera camera, OSPWorld world);

private:
    std::int32_t _spp;
    std::int32_t _path_length;
    std::int32_t _ao_samples;
    float _ao_distance;
    float _volume_sampling_rate;
};
} // namespace trrojan::ospray