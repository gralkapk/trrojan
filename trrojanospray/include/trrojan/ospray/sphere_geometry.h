#pragma once

#include "trrojan/ospray/object.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API sphere_geometry final : public object {
public:
    sphere_geometry();

    virtual ~sphere_geometry() = default;

    sphere_geometry& setPositions(OSPData positions);
    sphere_geometry& setRadii(OSPData radii);
    sphere_geometry& setGlobalRadius(float radius);

private:
};
}