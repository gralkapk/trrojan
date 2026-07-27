#pragma once

#include "trrojan/timer.h"

#include "trrojan/ospray/export.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API measurement_context final {
public:
    measurement_context();

    std::uint32_t check_cpu_iterations(const double min_wall_time);

    std::uint32_t cpu_iterations;

    timer cpu_timer;
};
} // namespace trrojan::ospray
