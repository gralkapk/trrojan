#pragma once

#include <vector>

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API group : public object {
public:
    group();
    virtual ~group();

    group& setGeometry(const std::vector<object>& geometries);
    group& setVolume(const std::vector<object>& volumes);
    group& setLight(const std::vector<object>& lights);

    group& setGeometry(OSPGeometricModel geometry);
    group& setVolume(OSPVolumetricModel volume);
    group& setLight(OSPLight light);
};
} // namespace trrojan::ospray