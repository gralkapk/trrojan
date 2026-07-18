#include "trrojan/ospray/camera.h"

#include <glm/gtc/type_ptr.hpp>

namespace trrojan::ospray {
#define _CAMERA_DEFINE_FACTOR(f) const char* camera::factor_##f = #f

_CAMERA_DEFINE_FACTOR(cam_position);
_CAMERA_DEFINE_FACTOR(cam_direction);
_CAMERA_DEFINE_FACTOR(cam_up);
_CAMERA_DEFINE_FACTOR(nearClip);
_CAMERA_DEFINE_FACTOR(fovy);
_CAMERA_DEFINE_FACTOR(aspect);

#undef _CAMERA_DEFINE_FACTOR

#define _CAMERA_INIT_FACTOR(f) _##f(config.get<decltype(_##f)>(factor_##f))

camera::camera(const configuration& config)
        : object{ospNewCamera("perspective")}
        , _CAMERA_INIT_FACTOR(cam_position)
        , _CAMERA_INIT_FACTOR(cam_direction)
        , _CAMERA_INIT_FACTOR(cam_up)
        , _CAMERA_INIT_FACTOR(nearClip)
        , _CAMERA_INIT_FACTOR(fovy)
        , _CAMERA_INIT_FACTOR(aspect) {
    // TODO: Set camera parameters
    setParam("position", OSP_VEC3F, glm::value_ptr(_cam_position));
    setParam("direction", OSP_VEC3F, glm::value_ptr(_cam_direction));
    setParam("up", OSP_VEC3F, glm::value_ptr(_cam_up));
    setParam("nearClip", OSP_FLOAT, &_nearClip);
    setParam("fovy", OSP_FLOAT, &_fovy);
    setParam("aspect", OSP_FLOAT, &_aspect);
}

#undef _CAMERA_INIT_FACTOR

camera::~camera() {}
} // namespace trrojan::ospray
