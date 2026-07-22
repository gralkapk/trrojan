#pragma once

#include "trrojan/device.h"

#include "trrojan/ospray/export.h"

#include <ospray/ospray.h>

namespace trrojan::ospray {
class TRROJANOSPRAY_API device : public trrojan::device_base {
public:
    using pointer = std::shared_ptr<device>;

    device();
    virtual ~device();

private:
    OSPDevice _osp_device;
};
} // namespace trrojan::ospray
