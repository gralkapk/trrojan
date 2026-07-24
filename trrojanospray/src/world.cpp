#include "trrojan/ospray/world.h"

#include <ospray/ospray_util.h>

namespace trrojan::ospray {
world::world(const std::vector<OSPInstance>& instances, const std::vector<OSPLight>& lights, bool compact)
        : object{ospNewWorld()} {
    // TODO: Initialize world with instances and lights
    setParam("instance", OSP_OBJECT, instances.data());
    setParam("light", OSP_OBJECT, lights.data());
    setParam("compactMode", OSP_BOOL, &compact);
}

world::world(OSPInstance instance, OSPLight light, bool compact) : object{ospNewWorld()} {
    ospSetObjectAsData(getObject(), "instance", OSP_INSTANCE, instance);
    ospSetObjectAsData(getObject(), "light", OSP_LIGHT, light);
    setParam("compactMode", OSP_BOOL, &compact);
}

OSPBounds world::getBounds() const {
    return ospGetBounds(getObject());
}
} // namespace trrojan::ospray
