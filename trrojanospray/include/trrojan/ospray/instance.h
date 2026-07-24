#pragma once

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API instance : public object {
public:
    instance();
    explicit instance(OSPGroup group);
    virtual ~instance();
};
} // namespace trrojan::ospray