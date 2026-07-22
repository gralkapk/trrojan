#include "trrojan/ospray/environment.h"

#include <assert.h>

namespace trrojan::ospray {
environment::environment() : environment_base{"ospray"} {}

size_t environment::get_devices(device_list& dst) {
    for (auto d : this->_devices) {
        auto dd = std::dynamic_pointer_cast<trrojan::device_base>(d);
        assert(dd != nullptr);
        dst.push_back(std::move(dd));
    }
    return _devices.size();
}

void environment::on_activate(void) {}

void environment::on_deactivate(void) {}

void environment::on_finalise(void) {
    this->_devices.clear();
}

void environment::on_initialise(const cmd_line& cmdLine) {
    this->_devices.push_back(std::make_shared<device>());
}
} // namespace trrojan::ospray
