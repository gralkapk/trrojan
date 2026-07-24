#pragma once

#include <vector>

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API world : public object {
public:
    explicit world(
        std::vector<OSPInstance> const& instances, std::vector<OSPLight> const& lights, bool compact = false);
    explicit world(OSPInstance instance, OSPLight light, bool compact = false);
    virtual ~world() = default;

    OSPBounds getBounds() const;
};
} // namespace trrojan::ospray
