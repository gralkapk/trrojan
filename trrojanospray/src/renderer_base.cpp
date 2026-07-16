#include "trrojan/ospray/renderer_base.h"

namespace trrojan::ospray {
#define _RENDERER_DEFINE_FACTOR(f) const char* renderer_base::factor_##f = #f

_RENDERER_DEFINE_FACTOR(spp);
_RENDERER_DEFINE_FACTOR(path_length);
_RENDERER_DEFINE_FACTOR(ao_samples);
_RENDERER_DEFINE_FACTOR(ao_distance);
_RENDERER_DEFINE_FACTOR(volume_sampling_rate);

#undef _RENDERER_DEFINE_FACTOR

#define _RENDERER_INIT_FACTOR(f) _##f(config.get<decltype(_##f)>(factor_##f))

renderer_base::renderer_base(const configuration& config)
        : object{ospNewRenderer("scivis")}
        , _RENDERER_INIT_FACTOR(spp)
        , _RENDERER_INIT_FACTOR(path_length)
        , _RENDERER_INIT_FACTOR(ao_samples)
        , _RENDERER_INIT_FACTOR(ao_distance)
        , _RENDERER_INIT_FACTOR(volume_sampling_rate) {
    // set default values for the parameters
    setParam("pixelSamples", OSP_INT, &_spp);
    setParam("maxPathLength", OSP_INT, &_path_length);
    setParam("aoSamples", OSP_INT, &_ao_samples);
    setParam("aoDistance", OSP_FLOAT, &_ao_distance);
    setParam("volumeSamplingRate", OSP_FLOAT, &_volume_sampling_rate);
}

#undef _RENDERER_INIT_FACTOR

renderer_base::~renderer_base() {}

OSPFuture renderer_base::renderFrame(OSPFrameBuffer frameBuffer, OSPCamera camera, OSPWorld world) {
    return ospRenderFrame(frameBuffer, reinterpret_cast<OSPRenderer>(getObject()), camera, world);
}

} // namespace trrojan::ospray