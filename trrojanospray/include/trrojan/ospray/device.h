#pragma once

#include "trrojan/device.h"

#include "trrojan/ospray/export.h"

#include <ospray/ospray.h>

namespace trrojan::ospray {
class TRROJANOSPRAY_API device : public trrojan::device_base {
public:
    device();
    virtual ~device();

private:
    OSPDevice _osp_device;
};
} // namespace trrojan::ospray
