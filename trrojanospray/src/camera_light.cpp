#include "trrojan/ospray/camera_light.h"

#include <glm/gtc/type_ptr.hpp>

namespace trrojan::ospray {
camera_light::camera_light(const glm::vec3& direction) : object{ospNewLight("distant")}, _cam_direction(direction) {
    // TODO: Initialize camera light with configuration

    // color vec3f
    // intensity float
    // direction vec3f

    setParam("direction", OSP_VEC3F, glm::value_ptr(_cam_direction));
}

} // namespace trrojan::ospray
