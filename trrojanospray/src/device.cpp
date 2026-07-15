#include "trrojan/ospray/device.h"

namespace trrojan::ospray {
device::device() : _osp_device{ospNewDevice("cpu")} {
    // TODO set parameters for the device
    ospDeviceSetParam(_osp_device, "logOutput", OSP_STRING, "cout");
    ospDeviceSetParam(_osp_device, "errorOutput", OSP_STRING, "cerr");
#ifdef DEBUG
    std::uint32_t logLevel = OSP_LOG_DEBUG;
#else
    std::uint32_t logLevel = OSP_LOG_ERROR;
#endif // DEBUG
    ospDeviceSetParam(_osp_device, "logLevel", OSP_UINT, &logLevel);

    //-----------------------------------
    ospDeviceCommit(_osp_device);
    ospSetCurrentDevice(_osp_device);
}

device::~device() {
    ospDeviceRelease(_osp_device);
    ospShutdown();
}
} // namespace trrojan::ospray