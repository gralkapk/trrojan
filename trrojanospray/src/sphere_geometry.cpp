#include "trrojan/ospray/sphere_geometry.h"

namespace trrojan::ospray {
sphere_geometry::sphere_geometry() : object{ospNewGeometry("sphere")} {}

sphere_geometry::sphere_geometry(OSPData positions, OSPData radii) : object{ospNewGeometry("sphere")} {
    setPositions(positions);
    setRadii(radii);
}

sphere_geometry& sphere_geometry::setPositions(OSPData positions) {
    setObject("sphere.position", positions);
    return *this;
}

sphere_geometry& sphere_geometry::setRadii(OSPData radii) {
    setObject("sphere.radius", radii);
    return *this;
}

sphere_geometry& sphere_geometry::setGlobalRadius(float radius) {
    setParam("radius", OSP_FLOAT, &radius);
    return *this;
}

sphere_geometry& sphere_geometry::setPositions(const std::vector<glm::vec3>& positions) {
    auto tmp_data = ospNewSharedData(positions.data(), OSP_VEC3F, positions.size());
    auto data = ospNewData(OSP_VEC3F, positions.size());
    ospCopyData(tmp_data, data);
    setPositions(data);
    ospRelease(data);
    ospRelease(tmp_data);
    return *this;
}

sphere_geometry& sphere_geometry::setRadii(const std::vector<float>& radii) {
    auto tmp_data = ospNewSharedData(radii.data(), OSP_FLOAT, radii.size());
    auto data = ospNewData(OSP_FLOAT, radii.size());
    ospCopyData(tmp_data, data);
    setRadii(data);
    ospRelease(data);
    ospRelease(tmp_data);
    return *this;
}
} // namespace trrojan::ospray
