#include "trrojan/ospray/camera.h"

namespace trrojan::ospray {
camera::camera(float fov, float aspectRatio) : object{ospNewCamera("perspective")} {
    // TODO: Set camera parameters
    setParam("position", OSP_VEC3F, nullptr);
    setParam("direction", OSP_VEC3F, nullptr);
    setParam("up", OSP_VEC3F, nullptr);
    setParam("nearClip", OSP_FLOAT, nullptr);
    setParam("fovy", OSP_FLOAT, &fov);
    setParam("aspect", OSP_FLOAT, &aspectRatio);
}
camera::~camera() {}
} // namespace trrojan::ospray
