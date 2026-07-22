#pragma once

#include "trrojan/graphics_benchmark_base.h"

#include "trrojan/ospray/device.h"
#include "trrojan/ospray/export.h"

#include "trrojan/ospray/render_target_base.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API benchmark_base : public trrojan::graphics_benchmark_base {
public:
    virtual ~benchmark_base(void) = default;

    // Inherited via graphics_benchmark_base
    bool can_run(trrojan::environment env, trrojan::device device) const noexcept override;

    // Inherited via graphics_benchmark_base
    result run(const configuration& config) override;

    std::shared_ptr<render_target_base> render_target(void) const {
        return this->_render_target;
    }

protected:
    benchmark_base(const std::string& name);

    virtual void on_device_switch(device& device);

    virtual result on_run(ospray::device& device, const configuration& config,
        power_collector::pointer& power_collector, const std::vector<std::string>& changed) = 0;

private:

    std::shared_ptr<render_target_base> _render_target;
};
} // namespace trrojan::ospray