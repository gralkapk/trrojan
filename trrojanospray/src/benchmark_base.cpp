#include "trrojan/ospray/benchmark_base.h"

namespace trrojan::ospray {
bool benchmark_base::can_run(trrojan::environment env, trrojan::device device) const noexcept {
    auto d = std::dynamic_pointer_cast<trrojan::ospray::device>(device);
    return (d != nullptr);
}

trrojan::result benchmark_base::run(const configuration& config) {
    // TODO Init stuff
    std::vector<std::string> changed;
    check_changed_factors(config, std::back_inserter(changed));

    auto genericDev = config.get<trrojan::device>(factor_device);
    auto device = std::dynamic_pointer_cast<trrojan::ospray::device>(genericDev);
    auto power_collector = initialise_power_collector(config);

    if (device == nullptr) {
        throw std::runtime_error("Device is not an OSPRay device.");
    }

    if (contains_any(changed, factor_device)) {
        on_device_switch(*device);
        changed.push_back(factor_viewport);
    }

    if (contains(changed, factor_viewport)) {
        // TODO: Resize the render target if the viewport has changed.
    }

    auto const retval = on_run(*device, config, power_collector, changed);

    return retval;
}

benchmark_base::benchmark_base(const std::string& name) : trrojan::graphics_benchmark_base{name} {}

void benchmark_base::on_device_switch(device& device) {}
} // namespace trrojan::ospray
