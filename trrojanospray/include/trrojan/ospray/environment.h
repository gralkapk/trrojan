#pragma once

#include "trrojan/environment.h"

#include "trrojan/ospray/device.h"
#include "trrojan/ospray/export.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API environment : public trrojan::environment_base {
public:
    using device_list = environment_base::device_list;
    using pointer = std::shared_ptr<environment>;

    environment();
    virtual ~environment() = default;

    // Inherited via environment_base
    size_t get_devices(device_list& dst) override;

    /// <inheritdoc />
    virtual void on_activate(void);

    /// <inheritdoc />
    virtual void on_deactivate(void);

    /// <inheritdoc />
    virtual void on_finalise(void);

    /// <inheritdoc />
    virtual void on_initialise(const cmd_line& cmdLine);

private:
    std::vector<device::pointer> _devices;
};
} // namespace trrojan::ospray
