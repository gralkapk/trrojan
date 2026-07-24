#pragma once

#include "trrojan/camera.h"

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/benchmark_base.h"

#include "trrojan/ospray/sphere_osp_data.h"

#include "trrojan/ospray/renderer_base.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API sphere_benchmark : public benchmark_base {
public:
    sphere_benchmark();
    virtual ~sphere_benchmark(void) = default;

    void optimise_order(configuration_set& inOutConfs) override;

protected:
    void on_device_switch(device& device) override;

    // Inherited via benchmark_base
    result on_run(ospray::device& device, const configuration& config, power_collector::pointer& power_collector,
        const std::vector<std::string>& changed) override;
private:
    void configure_camera(const configuration& config, const float fovy = 60.0f);

    sphere_osp_data _data;

    std::shared_ptr<renderer_base> _renderer;

    trrojan::perspective_camera _camera;
};
} // namespace trrojan::ospray