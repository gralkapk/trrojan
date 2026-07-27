#include "trrojan/ospray/measurement_context.h"

#include "trrojan/estimate_iterations.h"

namespace trrojan::ospray {
measurement_context::measurement_context() : cpu_iterations(1), cpu_timer() {}

std::uint32_t measurement_context::check_cpu_iterations(const double min_wall_time) {
    const auto elapsed = cpu_timer.elapsed_millis();
    auto retval = estimate_iterations(min_wall_time, elapsed, this->cpu_iterations);

    if (retval > this->cpu_iterations) {
        return retval;
    } else {
        this->cpu_iterations = retval;
        return 0;
    }
}
} // namespace trrojan::ospray