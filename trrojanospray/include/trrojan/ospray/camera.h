#pragma once

#include <glm/glm.hpp>

#include "trrojan/configuration.h"

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API camera : public object {
public:
    static const char* factor_cam_position;
    static const char* factor_cam_direction;
    static const char* factor_cam_up;
    static const char* factor_nearClip;
    static const char* factor_fovy;
    static const char* factor_aspect;

    camera(const configuration& config);
    ~camera();
private:
    glm::vec3 _cam_position;
    glm::vec3 _cam_direction;
    glm::vec3 _cam_up;
    float _nearClip;
    float _fovy;
    float _aspect;
};
} // namespace trrojan::ospray