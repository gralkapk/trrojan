#pragma once

#include "trrojan/configuration.h"

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

#include <glm/glm.hpp>

namespace trrojan::ospray {
class TRROJANOSPRAY_API camera_light : public object {
public:
    explicit camera_light(const glm::vec3 &direction);
    virtual ~camera_light() = default;

private:
    glm::vec3 _cam_direction;

};
} // namespace trrojan::ospray
