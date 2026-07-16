#pragma once

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API camera : public object {
public:
    camera(float fov, float aspectRatio);
    ~camera();
private:
};
} // namespace trrojan::ospray