#include "trrojan/ospray/camera.h"

#include <glm/gtc/type_ptr.hpp>

namespace trrojan::ospray {
camera::camera(const config& config)
        : object{ospNewCamera("perspective")}
        , _cam_position(config.cam_position)
        , _cam_direction(config.cam_direction)
        , _cam_up(config.cam_up)
        , _nearClip(config.nearClip)
        , _fovy(config.fovy)
        , _aspect(config.aspect) {
    // TODO: Set camera parameters
    setParam("position", OSP_VEC3F, glm::value_ptr(_cam_position));
    setParam("direction", OSP_VEC3F, glm::value_ptr(_cam_direction));
    setParam("up", OSP_VEC3F, glm::value_ptr(_cam_up));
    setParam("nearClip", OSP_FLOAT, &_nearClip);
    setParam("fovy", OSP_FLOAT, &_fovy);
    setParam("aspect", OSP_FLOAT, &_aspect);
}

camera::~camera() {}
} // namespace trrojan::ospray
