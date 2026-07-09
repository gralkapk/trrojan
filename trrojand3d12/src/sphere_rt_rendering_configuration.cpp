#include "trrojan/d3d12/sphere_rt_rendering_configuration.h"

#define _SPHERE_RT_BENCH_DEFINE_FACTOR(f)                                         \
const char *trrojan::d3d12::sphere_rt_rendering_configuration::factor_##f = #f

_SPHERE_RT_BENCH_DEFINE_FACTOR(data_set);
_SPHERE_RT_BENCH_DEFINE_FACTOR(frame);
_SPHERE_RT_BENCH_DEFINE_FACTOR(gpu_counter_iterations);
_SPHERE_RT_BENCH_DEFINE_FACTOR(min_prewarms);
_SPHERE_RT_BENCH_DEFINE_FACTOR(min_wall_time);
_SPHERE_RT_BENCH_DEFINE_FACTOR(spp);
_SPHERE_RT_BENCH_DEFINE_FACTOR(rec_depth);

#undef _SPHERE_RT_BENCH_DEFINE_FACTOR

namespace trrojan::d3d12 {
#define _SPHERE_RT_BENCH_INIT_FACTOR(f)                                           \
    _##f{config.get<decltype(_##f)>(factor_##f)}

    sphere_rt_rendering_configuration::sphere_rt_rendering_configuration(
        const configuration& config) :
        _SPHERE_RT_BENCH_INIT_FACTOR(data_set),
        _SPHERE_RT_BENCH_INIT_FACTOR(frame),
        _SPHERE_RT_BENCH_INIT_FACTOR(gpu_counter_iterations),
        _SPHERE_RT_BENCH_INIT_FACTOR(min_prewarms),
        _SPHERE_RT_BENCH_INIT_FACTOR(min_wall_time),
        _SPHERE_RT_BENCH_INIT_FACTOR(spp),
        _SPHERE_RT_BENCH_INIT_FACTOR(rec_depth) {}

#undef _SPHERE_RT_BENCH_INIT_FACTOR
}
