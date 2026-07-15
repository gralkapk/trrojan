#include "trrojan/ospray/sphere_geometry.h"

namespace trrojan::ospray {
sphere_geometry::sphere_geometry() : object{ospNewGeometry("sphere")} {}

sphere_geometry& sphere_geometry::setPositions(OSPData positions) {
    setParam("sphere.position", OSP_DATA, positions);
    return *this;
}

sphere_geometry& sphere_geometry::setRadii(OSPData radii) {
    setParam("sphere.radius", OSP_DATA, radii);
    return *this;
}

sphere_geometry& sphere_geometry::setGlobalRadius(float radius) {
    setParam("radius", OSP_FLOAT, &radius);
    return *this;
}
} // namespace trrojan::ospray
