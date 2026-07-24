#include "trrojan/ospray/instance.h"

namespace trrojan::ospray {
instance::instance() : object{ospNewInstance()} {}
instance::instance(OSPGroup group) : object{ospNewInstance(group)} {}
instance::~instance() {}
} // namespace trrojan::ospray