#include "trrojan/ospray/camera_light.h"

#include <glm/gtc/type_ptr.hpp>

namespace trrojan::ospray {
#define _CAMERA_LIGHT_DEFINE_FACTOR(f) const char* camera_light::factor_##f = #f

_CAMERA_LIGHT_DEFINE_FACTOR(cam_direction);

#undef _CAMERA_LIGHT_DEFINE_FACTOR

#define _CAMERA_LIGHT_INIT_FACTOR(f) _##f(config.get<decltype(_##f)>(factor_##f))

camera_light::camera_light(const configuration& config)
        : object{ospNewLight("distant")}
        , _CAMERA_LIGHT_INIT_FACTOR(cam_direction) {
    // TODO: Initialize camera light with configuration

    // color vec3f
    // intensity float
    // direction vec3f

    setParam("direction", OSP_VEC3F, glm::value_ptr(_cam_direction));
}

#undef _CAMERA_LIGHT_INIT_FACTOR
} // namespace trrojan::ospray
