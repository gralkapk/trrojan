#include "trrojan/ospray/volume_benchmark.h"

#include "trrojan/brudervn_xfer_func.h"
#include "trrojan/clipping.h"
#include "trrojan/io.h"
#include "trrojan/log.h"
#include "trrojan/on_exit.h"

#include "trrojan/ospray/camera.h"
#include "trrojan/ospray/measurement_context.h"
#include "trrojan/ospray/renderer_base.h"
#include "trrojan/ospray/volume_configuration.h"

#include <ospray/ospray_cpp/ext/rkcommon.h>
#include <ospray/ospray_util.h>

#include <glm/gtc/type_ptr.hpp>

namespace trrojan::ospray {

volume_benchmark::volume_benchmark() : benchmark_base("volume-renderer") {
    this->_default_configs.add_factor(
        factor::from_manifestations(volume_configuration::factor_counter_iterations, static_cast<unsigned int>(7)));
    this->_default_configs.add_factor(
        factor::from_manifestations(volume_configuration::factor_min_prewarms, static_cast<unsigned int>(4)));
    this->_default_configs.add_factor(
        factor::from_manifestations(volume_configuration::factor_min_wall_time, static_cast<unsigned int>(1000)));
    this->_default_configs.add_factor(
        factor::from_manifestations(volume_configuration::factor_spp, static_cast<unsigned int>(1)));
    this->_default_configs.add_factor(
        factor::from_manifestations(volume_configuration::factor_rec_depth, static_cast<unsigned int>(0)));
    this->_default_configs.add_factor(
        factor::from_manifestations(volume_configuration::factor_ao_samples, static_cast<unsigned int>(0)));
    this->_default_configs.add_factor(
        factor::from_manifestations(volume_configuration::factor_volume_sampling_rate, static_cast<unsigned int>(1)));
    this->_default_configs.add_factor(
        factor::from_manifestations(volume_configuration::factor_fovy, static_cast<float>(60.0f)));

    this->add_default_manoeuvre();
}

void volume_benchmark::optimise_order(configuration_set& inOutConfs) {
    inOutConfs.optimise_order(
        {volume_configuration::factor_data_set, volume_configuration::factor_frame, benchmark_base::factor_device});
}

void get_scalar_range(std::vector<std::uint8_t> const& data, datraw::scalar_type type, float& out_min, float& out_max) {
    switch (type) {
    case datraw::scalar_type::uint8: {
        auto const* ptr = reinterpret_cast<std::uint8_t const*>(data.data());
        auto const [min, max] = std::minmax_element(ptr, ptr + data.size());
        out_min = static_cast<float>(*min);
        out_max = static_cast<float>(*max);
        break;
    }
    case datraw::scalar_type::int16: {
        auto const* ptr = reinterpret_cast<std::int16_t const*>(data.data());
        auto const [min, max] = std::minmax_element(ptr, ptr + data.size() / 2);
        out_min = static_cast<float>(*min);
        out_max = static_cast<float>(*max);
        break;
    }
    case datraw::scalar_type::uint16: {
        auto const* ptr = reinterpret_cast<std::uint16_t const*>(data.data());
        auto const [min, max] = std::minmax_element(ptr, ptr + data.size() / 2);
        out_min = static_cast<float>(*min);
        out_max = static_cast<float>(*max);
        break;
    }
    case datraw::scalar_type::float32: {
        auto const* ptr = reinterpret_cast<float const*>(data.data());
        auto const [min, max] = std::minmax_element(ptr, ptr + data.size() / 4);
        out_min = *min;
        out_max = *max;
        break;
    }
    case datraw::scalar_type::float64: {
        auto const* ptr = reinterpret_cast<double const*>(data.data());
        auto const [min, max] = std::minmax_element(ptr, ptr + data.size() / 8);
        out_min = static_cast<float>(*min);
        out_max = static_cast<float>(*max);
        break;
    }
    default:
        throw std::invalid_argument("Unsupported scalar type for range computation.");
    }
}

std::vector<float> normalize_volume_data(std::vector<std::uint8_t> const& data, datraw::scalar_type type, float min_scalar, float max_scalar) {
    std::vector<float> normalized_data;
    normalized_data.reserve(data.size() / sizeof(float));

    switch (type) {
    case datraw::scalar_type::uint8: {
        auto const* ptr = reinterpret_cast<std::uint8_t const*>(data.data());
        normalized_data.reserve(data.size());
        std::transform(
            ptr, ptr + data.size(), std::back_inserter(normalized_data), [min_scalar, max_scalar](std::uint8_t value) {
                return (static_cast<float>(value) - min_scalar) / (max_scalar - min_scalar);
            });
        break;
    }
    case datraw::scalar_type::int16: {
        auto const* ptr = reinterpret_cast<std::int16_t const*>(data.data());
        normalized_data.reserve(data.size() / 2);
        std::transform(ptr, ptr + data.size() / 2, std::back_inserter(normalized_data),
            [min_scalar, max_scalar](
                std::int16_t value) { return (static_cast<float>(value) - min_scalar) / (max_scalar - min_scalar); });
        break;
    }
    case datraw::scalar_type::uint16: {
        auto const* ptr = reinterpret_cast<std::uint16_t const*>(data.data());
        normalized_data.reserve(data.size() / 2);
        std::transform(ptr, ptr + data.size() / 2, std::back_inserter(normalized_data),
            [min_scalar, max_scalar](
                std::uint16_t value) { return (static_cast<float>(value) - min_scalar) / (max_scalar - min_scalar); });
        break;
    }
    case datraw::scalar_type::float32: {
        auto const* ptr = reinterpret_cast<float const*>(data.data());
        normalized_data.reserve(data.size() / 4);
        std::transform(ptr, ptr + data.size() / 4, std::back_inserter(normalized_data),
            [min_scalar, max_scalar](float value) { return (value - min_scalar) / (max_scalar - min_scalar); });
        break;
    }
    case datraw::scalar_type::float64: {
        auto const* ptr = reinterpret_cast<double const*>(data.data());
        normalized_data.reserve(data.size() / 8);
        std::transform(
            ptr, ptr + data.size() / 8, std::back_inserter(normalized_data), [min_scalar, max_scalar](double value) {
                return (static_cast<float>(value) - min_scalar) / (max_scalar - min_scalar);
            });
        break;
    }
    default:
        throw std::invalid_argument("Unsupported scalar type for range computation.");
    }

    return normalized_data;
}

::ospray::cpp::Volume volume_benchmark::load_volume(const std::string& path, const frame_type frame) {
    log::instance().write_line(log_level::debug,
        "Loading volume data "
        "from {} ...",
        path);
    auto reader = reader_type::open(path);
    this->_volume_info = reader.info();

    if (!reader.move_to(frame)) {
        throw std::invalid_argument("The given frame number does not exist.");
    }

    auto resolution = reader.info().resolution();
    if (resolution.size() != 3) {
        throw std::invalid_argument("The given data set is not a 3D volume.");
    }

    auto components = reader.info().components();
    if (components > 1) {
        throw std::invalid_argument("The number of per-voxel components of the "
                                    "given data set is not 1.");
    }

    OSPDataType data_type = OSP_UNKNOWN;
    std::uint64_t element_size = 0;
    switch (reader.info().format()) {
    case datraw::scalar_type::uint8:
        data_type = OSP_UCHAR;
        element_size = 1;
        break;
    case datraw::scalar_type::int16:
        data_type = OSP_SHORT;
        element_size = 2;
        break;
    case datraw::scalar_type::uint16:
        data_type = OSP_USHORT;
        element_size = 2;
        break;
    /*case datraw::scalar_type::float16:
        data_type = OSP_HALF;
        element_size = 2;
        break;*/
    case datraw::scalar_type::float32:
        data_type = OSP_FLOAT;
        element_size = 4;
        break;
    case datraw::scalar_type::float64:
        data_type = OSP_DOUBLE;
        element_size = 8;
        break;
    }

    if (data_type == OSP_UNKNOWN || element_size == 0) {
        throw std::invalid_argument("The given scalar data type is unknown or unsupported.");
    }

    const auto data = reader.read_current();

    get_scalar_range(data, reader.info().format(), this->_volume_scalar_range[0], this->_volume_scalar_range[1]);

    rkcommon::math::vec3ul gridDimensions(resolution[0], resolution[1], resolution[2]);
    rkcommon::math::vec3f gridOrigin(0.f, 0.f, 0.f);
    try {
        rkcommon::math::vec3f gridOrigin(
            reader.info().origin()[0], reader.info().origin()[1], reader.info().origin()[2]);
    } catch (...) {
        // Ignore any exceptions and use the default origin
    }
    rkcommon::math::vec3f gridSpacing(
        reader.info().slice_thickness()[0], reader.info().slice_thickness()[1], reader.info().slice_thickness()[2]);

    auto osp_data = ::ospray::cpp::CopiedData(reinterpret_cast<const char*>(data.data()), data_type, gridDimensions);

    ::ospray::cpp::Volume volume("structuredRegular");
    volume.setParam("gridOrigin", gridOrigin);
    volume.setParam("gridSpacing", gridSpacing);
    volume.setParam("data", osp_data);
    volume.setParam("cellCentered", false);
    volume.setParam("filter", OSP_VOLUME_FILTER_CUBIC);
    volume.setParam("gradientFilter", OSP_VOLUME_FILTER_CUBIC);
    //volume.setParam("background", 0.f);
    volume.commit();

    return volume;
}

::ospray::cpp::TransferFunction volume_benchmark::load_brudervn_xfer_func(
    const std::string& path, std::array<float, 2> const& scalar_range) {
    log::instance().write_line(log_level::debug,
        "Loading transfer function "
        "from {} ...",
        path);
    auto const data = trrojan::load_brudervn_xfer_func(path);
    return load_xfer_func(data, scalar_range);
}

::ospray::cpp::TransferFunction volume_benchmark::load_xfer_func(
    const std::vector<std::uint8_t>& data, std::array<float, 2> const& scalar_range) {
    const auto cnt = std::div(static_cast<long>(data.size()), 4l);

    if (cnt.rem != 0) {
        throw std::invalid_argument("The transfer function texture does not "
                                    "hold valid data in R8G8B8A8 format.");
    }

    std::vector<rkcommon::math::vec3f> colors;
    std::vector<float> opacities;
    for (size_t i = 0; i < cnt.quot; ++i) {
        const auto idx = i * 4;
        colors.emplace_back(static_cast<float>(data[idx]) / 255.f, static_cast<float>(data[idx + 1]) / 255.f,
            static_cast<float>(data[idx + 2]) / 255.f);
        opacities.push_back((static_cast<float>(data[idx + 3]) / 255.f)*scalar_range[1]); // TODO not entirely correct as scalar_range[0] is not considered
        //colors.back() = colors.back() * (1.0f - opacities.back());
    }
    /*using rkcommon::math::vec3f;
    std::vector<vec3f> colors = {vec3f(1.f, 0.f, 0.f), vec3f(0.f, 1.f, 0.f), vec3f(0.f, 1.f, 1.f), vec3f(1.f, 1.f, 0.f),
        vec3f(1.f, 1.f, 1.f), vec3f(1.f, 0.f, 1.f)};
    std::vector<float> opacities = {1.f, 1.f};*/

    ::ospray::cpp::TransferFunction xfer_func("piecewiseLinear");
    xfer_func.setParam("value", rkcommon::math::range1f(scalar_range[0], scalar_range[1]));
    //xfer_func.setParam("value", rkcommon::math::range1f(0.f, 1.f));
    xfer_func.setParam("color", ::ospray::cpp::CopiedData(colors));
    xfer_func.setParam("opacity", ::ospray::cpp::CopiedData(opacities));
    xfer_func.commit();

    return xfer_func;
}

::ospray::cpp::TransferFunction volume_benchmark::load_xfer_func(
    const std::string& path, std::array<float, 2> const& scalar_range) {
    log::instance().write_line(log_level::debug,
        "Loading transfer function "
        "from {} ...",
        path);
    const auto data = read_binary_file(path);
    return load_xfer_func(data, scalar_range);
}

::ospray::cpp::TransferFunction volume_benchmark::load_xfer_func(
    const configuration& config, std::array<float, 2> const& scalar_range) {
    try {
        auto path = config.get<std::string>(volume_configuration::factor_xfer_func);

        if (ends_with(path, std::string(".brudervn"))) {
            return load_brudervn_xfer_func(path, scalar_range);
        } else {
            return load_xfer_func(path, scalar_range);
        }

    } catch (...) {
        log::instance().write_line(log_level::debug, "Creating linear transfer "
                                                     "function as fallback solution.");
        std::vector<std::uint8_t> data(256 * 4);
        for (std::size_t i = 0; i < 256; ++i) {
            data[i * 4 + 0] = static_cast<std::uint8_t>(i);
            data[i * 4 + 1] = static_cast<std::uint8_t>(i);
            data[i * 4 + 2] = static_cast<std::uint8_t>(i);
            data[i * 4 + 3] = static_cast<std::uint8_t>(i);
        }

        return load_xfer_func(data, scalar_range);
    }
}

void volume_benchmark::on_device_switch(device& device) {
    benchmark_base::on_device_switch(device);
}

result volume_benchmark::on_run(ospray::device& device, const configuration& config,
    power_collector::pointer& power_collector, const std::vector<std::string>& changed) {
    volume_configuration cfg{config};
    measurement_context mctx;

    // load data
    if (!_volume) {
        _volume = load_volume(cfg.data_set(), cfg.frame());
        this->calc_bounding_box(this->_volume_bbox.front(), this->_volume_bbox.back());
    }

    // load xfer function
    if (!_xfer_func) {
        _xfer_func = load_xfer_func(config, _volume_scalar_range);
    }

    const auto volume_size = this->get_volume_resolution();

    configure_camera(config, cfg.fovy());

    ::ospray::cpp::VolumetricModel volume_model(_volume);
    volume_model.setParam("transferFunction", _xfer_func);
    volume_model.setParam("densityScale", 1.0f);
    volume_model.commit();

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

    ::ospray::cpp::Group volume_group;
    volume_group.setParam("volume", ::ospray::cpp::CopiedData(&volume_model, OSP_VOLUMETRIC_MODEL, 1));
    volume_group.setParam("light", ::ospray::cpp::CopiedData(lights.data(), OSP_LIGHT, lights.size()));
    volume_group.commit();

    ::ospray::cpp::Instance volume_instance(volume_group);
    volume_instance.commit();

    ::ospray::cpp::World world;
    world.setParam("instance", ::ospray::cpp::CopiedData(&volume_instance, OSP_INSTANCE, 1));
    //world.setParam("light", ::ospray::cpp::CopiedData(lights.data(), OSP_LIGHT, lights.size()));
    world.commit();

    // setup renderer
    renderer_base::config renderer_config = {};
    renderer_config.spp = cfg.spp();
    renderer_config.path_length = cfg.rec_depth() + 1;
    renderer_config.ao_distance = std::numeric_limits<float>::max();
    renderer_config.ao_samples = cfg.ao_samples();
    renderer_config.volume_sampling_rate = cfg.volume_sampling_rate();
    renderer_base renderer(renderer_config);
    renderer.commit();

    // update camera
    camera::config camera_config = {};
    camera_config.aspect = _camera.get_aspect_ratio();
    camera_config.fovy = _camera.get_fovy();
    camera_config.cam_position = _camera.get_look_from();
    camera_config.cam_direction = _camera.get_look_to() - _camera.get_look_from();
    camera_config.cam_up = _camera.get_look_up();
    camera_config.nearClip = _camera.get_near_plane_dist();
    camera camera(camera_config);
    camera.commit();

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

    render_target()->resetAccumulation();

    // TODO: measure iterations
    log::instance().write_line(
        log_level::debug, "Measuring CPU timings over {} iterations ...", cfg.counter_iterations());
    std::vector<float> cpu_times(cfg.counter_iterations());
    std::atomic_bool rtx_acquired{false};
    auto evt_done = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (evt_done == NULL) {
        throw std::system_error(GetLastError(), std::system_category());
    }
    on_exit([evt_done](void) { CloseHandle(evt_done); });
    auto const powerUid = enter_power_scope(
        power_collector, [&rtx_acquired]() { rtx_acquired.store(true, std::memory_order_release); },
        [evt_done](const bool) { SetEvent(evt_done); });
    std::uint32_t actual_iterations = 0;
    for (std::uint32_t i = 0; i < cfg.counter_iterations() && !rtx_acquired.load(std::memory_order_acquire);
        ++i, ++actual_iterations) {
        auto frame_future = renderer.renderFrame(
            static_cast<OSPFrameBuffer>(*render_target()), static_cast<OSPCamera>(camera), world.handle());
        ospWait(frame_future);
        render_target()->present(0);
        cpu_times[i] = ospGetTaskDuration(frame_future);
    }
    leave_power_scope(power_collector);

    render_target()->resetAccumulation();

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
        std::make_shared<basic_result>(config, std::initializer_list<std::string>{"benchmark", "powerUid", "iterations",
                                                   "data_extents", "cpu_time_min", "cpu_time_med", "cpu_time_max"});
    result->add({this->name(), powerUid, actual_iterations, volume_size,
        std::chrono::duration_cast<std::chrono::milliseconds>(cpu_min_s).count(),
        std::chrono::duration_cast<std::chrono::milliseconds>(cpu_median_s).count(),
        std::chrono::duration_cast<std::chrono::milliseconds>(cpu_max_s).count()});

    // Make sure that the download of the power data has finished before we
    // continue to the next benchmark.
    log::instance().write_line(log_level::debug, "Waiting for "
                                                 "RTx sample to be downloaded.");
    ::WaitForSingleObject(evt_done, INFINITE);

    return result;
}

void volume_benchmark::configure_camera(const configuration& config, const float fovy) {
    this->set_aspect_from_viewport(this->_camera);
    this->_camera.set_fovy(fovy);
    graphics_benchmark_base::apply_manoeuvre(
        this->_camera, config, this->_volume_bbox.front(), this->_volume_bbox.back());
    trrojan::set_clipping_planes(this->_camera, this->_volume_bbox);
}

} // namespace trrojan::ospray