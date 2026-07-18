#pragma once

#include "trrojan/configuration.h"

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

#include <glm/glm.hpp>

namespace trrojan::ospray {
class TRROJANOSPRAY_API camera_light : public object {
public:
    static const char* factor_cam_direction;

    explicit camera_light(const configuration& config);
    virtual ~camera_light() = default;

private:
    glm::vec3 _cam_direction;

};
} // namespace trrojan::ospray
