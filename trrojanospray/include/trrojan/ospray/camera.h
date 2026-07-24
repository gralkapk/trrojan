#pragma once

#include <glm/glm.hpp>

#include "trrojan/configuration.h"

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API camera : public object {
public:
    struct config {
        glm::vec3 cam_position;
        glm::vec3 cam_direction;
        glm::vec3 cam_up;
        float nearClip;
        float fovy;
        float aspect;
    };

    camera(const config& config);
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