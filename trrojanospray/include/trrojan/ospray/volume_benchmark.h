#pragma once

#include "trrojan/camera.h"
#include "trrojan/datraw_base.h"

#include "trrojan/ospray/benchmark_base.h"
#include "trrojan/ospray/export.h"

#include "trrojan/ospray/renderer_base.h"

#include <ospray/ospray_cpp.h>

namespace trrojan::ospray {
class TRROJANOSPRAY_API volume_benchmark : public benchmark_base, public trrojan::datraw_base {
public:
    volume_benchmark();
    virtual ~volume_benchmark(void) = default;

    void optimise_order(configuration_set& inOutConfs) override;

protected:
    static ::ospray::cpp::Volume load_volume(const std::string& path, const frame_type frame);

    static ::ospray::cpp::TransferFunction load_brudervn_xfer_func(const std::string& path);
    static ::ospray::cpp::TransferFunction load_xfer_func(const std::vector<std::uint8_t>& data);
    static ::ospray::cpp::TransferFunction load_xfer_func(const std::string& path);
    static ::ospray::cpp::TransferFunction load_xfer_func(const configuration& config);

    void on_device_switch(device& device) override;

    result on_run(ospray::device& device, const configuration& config, power_collector::pointer& power_collector,
        const std::vector<std::string>& changed) override;

private:
    void configure_camera(const configuration& config, const float fovy = 60.0f);

    std::shared_ptr<renderer_base> _renderer;

    trrojan::perspective_camera _camera;

    ::ospray::cpp::Volume _volume;
    ::ospray::cpp::TransferFunction _xfer_func;

    std::array<glm::vec3, 2> _volume_bbox;
};
} // namespace trrojan::ospray
