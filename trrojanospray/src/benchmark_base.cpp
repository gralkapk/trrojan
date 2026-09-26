#include "trrojan/ospray/benchmark_base.h"

#include "trrojan/log.h"

#include "trrojan/ospray/debug_render_target.h"

namespace trrojan::ospray {
#define _OSP_BENCH_DEFINE_FACTOR(f) \
const std::string benchmark_base::factor_##f(#f)

_OSP_BENCH_DEFINE_FACTOR(debug_view);

#undef _OSP_BENCH_DEFINE_FACTOR

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
    auto power_collector = power_collector::get(config);

    if (device == nullptr) {
        throw std::runtime_error("Device is not an OSPRay device.");
    }

    auto isDebugView = config.get<bool>(factor_debug_view);

    if (contains(changed, factor_debug_view)) {
        changed.push_back(factor_device);
        changed.push_back(factor_viewport);
    }

    if (isDebugView) {
        auto is_device_change = contains(changed, factor_device);
        if (is_device_change) {
            log::instance().write_line(log_level::verbose, "Forcing the "
                                                           "debug render target to be re-created as the device has "
                                                           "changed.");
            this->_debug_target = nullptr;
        }

        if (!this->_debug_target) {
            log::instance().write_line(log_level::verbose, "Lazy creation of "
                                                           "OSP debug render target.");
            this->_debug_target = std::make_shared<debug_render_target>(*device);
        }

        // Overwrite device and render target
        this->_render_target = this->_debug_target;

        // Invoke device switch once the target has been changed.
        if (is_device_change) {
            log::instance().write_line(log_level::verbose, "Reallocating "
                                                           "graphics resources after switch to debug device ...");
            this->on_device_switch(*device);
        }
    } else {
        // Check whether the device has been changed. This should always be done
        // first, because all GPU resources, which depend on the content of the
        // configuration depend on the device as their storage location.
        if (contains_any(changed, factor_device)) {
            log::instance().write_line(log_level::verbose, "The OSP device has "
                                                           "changed. Reallocating all graphics resources ...");
            this->_render_target = std::make_shared<bench_render_target>(*device);
            //this->render_target->use_reversed_depth_buffer(true);
            this->on_device_switch(*device);
            // If the device has changed, force the viewport to be re-created:
            changed.push_back(factor_viewport);
        }
    }

    /*if (contains_any(changed, factor_device)) {
        on_device_switch(*device);
        changed.push_back(factor_viewport);
    }*/

    if (contains(changed, factor_viewport)) {
        // TODO: Resize the render target if the viewport has changed.
        auto vp = config.get<viewport_type>(factor_viewport);
        log::instance().write_line(log_level::verbose,
            "Resizing the "
            "benchmarking render target to {0} x {1} px ...",
            vp[0], vp[1]);
        _render_target->resize(vp[0], vp[1]);
        _render_target->commit();
    }

    auto const retval = on_run(*device, config, power_collector, changed);

    return retval;
}

benchmark_base::benchmark_base(const std::string& name) : trrojan::graphics_benchmark_base{name} {}

void benchmark_base::on_device_switch(device& device) {}

void benchmark_base::set_aspect_from_viewport(trrojan::camera& camera) {
    camera.set_aspect_ratio(_render_target->getAspectRatio());
}
} // namespace trrojan::ospray
