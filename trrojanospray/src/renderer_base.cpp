#include "trrojan/ospray/renderer_base.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace trrojan::ospray {
renderer_base::renderer_base(const config& config)
        : object{ospNewRenderer("scivis")}
        , _spp(config.spp)
        , _path_length(config.path_length)
        , _ao_samples(config.ao_samples)
        , _ao_distance(config.ao_distance)
        , _volume_sampling_rate(config.volume_sampling_rate) {
    // set default values for the parameters
    setParam("pixelSamples", OSP_INT, &_spp);
    setParam("maxPathLength", OSP_INT, &_path_length);
    setParam("aoSamples", OSP_INT, &_ao_samples);
    setParam("aoDistance", OSP_FLOAT, &_ao_distance);
    setParam("volumeSamplingRate", OSP_FLOAT, &_volume_sampling_rate);
    glm::vec4 background_color{0.0f, 0.0f, 0.0f, 1.0f};
    setParam("backgroundColor", OSP_VEC4F, glm::value_ptr(background_color));
}

renderer_base::~renderer_base() {}

OSPFuture renderer_base::renderFrame(OSPFrameBuffer frameBuffer, OSPCamera camera, OSPWorld world) {
    return ospRenderFrame(frameBuffer, reinterpret_cast<OSPRenderer>(getObject()), camera, world);
}

} // namespace trrojan::ospray