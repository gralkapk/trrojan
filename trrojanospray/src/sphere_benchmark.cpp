#include "trrojan/ospray/sphere_benchmark.h"

#include "trrojan/ospray/sphere_geometry.h"
#include "trrojan/ospray/geometric_model.h"
#include "trrojan/ospray/world.h"

namespace trrojan::ospray {

sphere_benchmark::sphere_benchmark() : benchmark_base("sphere") {
    this->add_default_manoeuvre();
}

void sphere_benchmark::on_device_switch(device& device) {
    benchmark_base::on_device_switch(device);
}

result sphere_benchmark::on_run(ospray::device& device, const configuration& config,
    power_collector::pointer& power_collector, const std::vector<std::string>& changed) {

    geometric_model sphere_model;

    // load data
    if (!this->_data) {
        auto path = config.get<std::string>(factor_path);
        auto frame = config.get<std::uint32_t>(factor_frame);
        auto [positions, radii, colors] = this->_data.load(path, frame);

        sphere_geometry geometry;
        geometry.setPositions(positions).setRadii(radii).commit();

        sphere_model.setGeometry(geometry).setColor(colors).commit();
    }

    // generate world

    // setup renderer

    // do benchmark


    return result();
}

} // namespace trrojan::ospray
