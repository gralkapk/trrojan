#include "trrojan/ospray/world.h"

namespace trrojan::ospray {
world::world(const std::vector<OSPInstance>& instances, const std::vector<OSPLight>& lights, bool compact)
        : object{ospNewWorld()} {
    // TODO: Initialize world with instances and lights
    setParam("instance", OSP_OBJECT, instances.data());
    setParam("light", OSP_OBJECT, lights.data());
    setParam("compactMode", OSP_BOOL, &compact);
}

OSPBounds world::getBounds() const {
    return ospGetBounds(getObject());
}
} // namespace trrojan::ospray
