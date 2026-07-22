#pragma once

#include <vector>

#include "trrojan/ospray/object.h"

#include <glm/glm.hpp>

namespace trrojan::ospray {
class TRROJANOSPRAY_API sphere_geometry final : public object {
public:
    sphere_geometry();

    explicit sphere_geometry(OSPData positions, OSPData radii);

    virtual ~sphere_geometry() = default;

    sphere_geometry& setPositions(OSPData positions);
    sphere_geometry& setRadii(OSPData radii);
    sphere_geometry& setGlobalRadius(float radius);

    sphere_geometry& setPositions(const std::vector<glm::vec3>& positions);
    sphere_geometry& setRadii(const std::vector<float>& radii);

private:
};
}