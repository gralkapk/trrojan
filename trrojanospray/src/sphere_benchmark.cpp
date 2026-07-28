#include "trrojan/ospray/sphere_benchmark.h"

#include "trrojan/clipping.h"
#include "trrojan/log.h"

#include "trrojan/ospray/camera.h"
#include "trrojan/ospray/camera_light.h"
#include "trrojan/ospray/geometric_model.h"
#include "trrojan/ospray/group.h"
#include "trrojan/ospray/instance.h"
#include "trrojan/ospray/sphere_geometry.h"
#include "trrojan/ospray/world.h"

#include "trrojan/ospray/sphere_configuration.h"

#include "trrojan/ospray/measurement_context.h"

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_util.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace trrojan::ospray {

sphere_benchmark::sphere_benchmark() : benchmark_base("sphere-renderer") {
    this->_default_configs.add_factor(
        factor::from_manifestations(sphere_configuration::factor_counter_iterations, static_cast<unsigned int>(7)));
    this->_default_configs.add_factor(
        factor::from_manifestations(sphere_configuration::factor_min_prewarms, static_cast<unsigned int>(4)));
    this->_default_configs.add_factor(
        factor::from_manifestations(sphere_configuration::factor_min_wall_time, static_cast<unsigned int>(1000)));
    this->_default_configs.add_factor(
        factor::from_manifestations(sphere_configuration::factor_spp, static_cast<unsigned int>(1)));
    this->_default_configs.add_factor(
        factor::from_manifestations(sphere_configuration::factor_rec_depth, static_cast<unsigned int>(0)));

    this->add_default_manoeuvre();
}

void sphere_benchmark::optimise_order(configuration_set& inOutConfs) {
    inOutConfs.optimise_order(
        {sphere_configuration::factor_data_set, sphere_configuration::factor_frame, benchmark_base::factor_device});
}

void sphere_benchmark::on_device_switch(device& device) {
    benchmark_base::on_device_switch(device);
}

result sphere_benchmark::on_run(ospray::device& device, const configuration& config,
    power_collector::pointer& power_collector, const std::vector<std::string>& changed) {
    sphere_configuration cfg{config};
    measurement_context mctx;

    ::ospray::cpp::GeometricModel sphere_model;

    // load data
    if (!this->_data) {
        log::instance().write_line(
            log_level::information, "Loading data set: ", cfg.data_set(), " frame: ", cfg.frame());
        this->_data.load(cfg.data_set(), cfg.frame());

        ::ospray::cpp::Geometry geometry("sphere");
        geometry.setParam("sphere.position",
            ::ospray::cpp::SharedData(this->_data.positions().data(), OSP_VEC3F, this->_data.positions().size()));
        geometry.setParam("sphere.radius",
            ::ospray::cpp::SharedData(this->_data.radii().data(), OSP_FLOAT, this->_data.radii().size()));
        geometry.commit();

        ::ospray::cpp::Material material("obj");
        material.commit();

        sphere_model = ::ospray::cpp::GeometricModel(geometry);

        sphere_model.setParam(
            "color", ::ospray::cpp::SharedData(this->_data.colors().data(), OSP_VEC4F, this->_data.colors().size()));
        sphere_model.setParam("material", material);
        sphere_model.commit();
    }

    configure_camera(config);

    // generate world
    ::ospray::cpp::Group sphere_group;
    sphere_group.setParam("geometry", ::ospray::cpp::CopiedData(&sphere_model, OSP_GEOMETRIC_MODEL, 1));
    sphere_group.commit();

    ::ospray::cpp::Instance sphere_instance(sphere_group);
    sphere_instance.commit();

    std::vector<::ospray::cpp::Light> lights;
    {
        ::ospray::cpp::Light light("ambient");
        light.commit();
        lights.push_back(light);
    }
    {
        ::ospray::cpp::Light light("distant");
        light.setParam("direction", OSP_VEC3F, glm::value_ptr(_camera.get_look_to() - _camera.get_look_from()));
        light.setParam("intensity", 1.0f);
        light.commit();
        lights.push_back(light);
    }

    ::ospray::cpp::World world;
    world.setParam("instance", ::ospray::cpp::CopiedData(&sphere_instance, OSP_INSTANCE, 1));
    world.setParam("light", ::ospray::cpp::CopiedData(lights.data(), OSP_LIGHT, lights.size()));
    world.commit();

    // setup renderer
    renderer_base::config renderer_config = {};
    renderer_config.spp = 1;
    renderer_config.path_length = 1;
    renderer_config.ao_distance = std::numeric_limits<float>::max();
    renderer_config.ao_samples = 0;
    renderer_config.volume_sampling_rate = 1.0f;
    renderer_base renderer(renderer_config);
    renderer.commit();

    // do benchmark
    camera::config camera_config = {};
    camera_config.aspect = _camera.get_aspect_ratio();
    camera_config.fovy = _camera.get_fovy();
    camera_config.cam_position = _camera.get_look_from();
    camera_config.cam_direction = _camera.get_look_to() - _camera.get_look_from();
    camera_config.cam_up = _camera.get_look_up();
    camera_config.nearClip = _camera.get_near_plane_dist();
    camera camera(camera_config);
    camera.commit();

    /*auto frame_future = renderer.renderFrame(
        static_cast<OSPFrameBuffer>(*render_target()), static_cast<OSPCamera>(camera), world.handle());
    ospWait(frame_future);
    auto const frame_time = ospGetTaskDuration(frame_future);

    render_target()->present(0);*/

    // TODO: prewarm iterations
    log::instance().write_line(log_level::debug, "Prewarming ...");
    {
        auto prewarms = (std::max) (1u, cfg.min_prewarms());
        mctx.cpu_timer.start();
        do {
            for (std::uint32_t i = 0; i < mctx.cpu_iterations; ++i) {
                auto frame_future = renderer.renderFrame(
                    static_cast<OSPFrameBuffer>(*render_target()), static_cast<OSPCamera>(camera), world.handle());
                ospWait(frame_future);
                render_target()->present(0);
            }
            device.wait_for_gpu();
            prewarms = mctx.check_cpu_iterations(cfg.min_wall_time());
        } while (prewarms > 0);
    }

    // TODO: measure iterations
    std::vector<float> cpu_times(cfg.counter_iterations());
    power_collector->enter_scope();
    for (std::uint32_t i = 0; i < cfg.counter_iterations(); ++i) {
        auto frame_future = renderer.renderFrame(
            static_cast<OSPFrameBuffer>(*render_target()), static_cast<OSPCamera>(camera), world.handle());
        ospWait(frame_future);
        render_target()->present(0);
        cpu_times[i] = ospGetTaskDuration(frame_future);
    }
    power_collector->leave_scope();

    std::sort(cpu_times.begin(), cpu_times.end());
    auto cpu_median = cpu_times[cpu_times.size() / 2];
    if (cpu_times.size() % 2 == 0) {
        cpu_median += cpu_times[cpu_times.size() / 2 - 1];
        cpu_median *= 0.5f;
    }

    auto const cpu_min_s = std::chrono::duration<float>(cpu_times.front());
    auto const cpu_max_s = std::chrono::duration<float>(cpu_times.back());
    auto const cpu_median_s = std::chrono::duration<float>(cpu_median);

    auto result =
        std::make_shared<basic_result>(config, std::initializer_list<std::string>{"benchmark", "particles",
                                                   "data_extents", "cpu_time_min", "cpu_time_med", "cpu_time_max"});
    result->add({this->name(), this->_data.spheres(), this->_data.extents(),
        std::chrono::duration_cast<std::chrono::milliseconds>(cpu_min_s).count(),
        std::chrono::duration_cast<std::chrono::milliseconds>(cpu_median_s).count(),
        std::chrono::duration_cast<std::chrono::milliseconds>(cpu_max_s).count()});

    return result;
}

void sphere_benchmark::configure_camera(const configuration& config, const float fovy) {
    set_aspect_from_viewport(_camera);
    _camera.set_fovy(fovy);
    graphics_benchmark_base::apply_manoeuvre(_camera, config, this->_data.bbox_start(), this->_data.bbox_end());
    trrojan::set_clipping_planes(_camera, this->_data.bbox(), this->_data.max_radius());
}

} // namespace trrojan::ospray
