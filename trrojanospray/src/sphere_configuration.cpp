#include "trrojan/ospray/sphere_configuration.h"

namespace trrojan::ospray {
#define _SPHERE_DEFINE_FACTOR(f) const char* sphere_configuration::factor_##f = #f

_SPHERE_DEFINE_FACTOR(data_set);
_SPHERE_DEFINE_FACTOR(frame);
_SPHERE_DEFINE_FACTOR(counter_iterations);
_SPHERE_DEFINE_FACTOR(min_prewarms);
_SPHERE_DEFINE_FACTOR(min_wall_time);
_SPHERE_DEFINE_FACTOR(spp);
_SPHERE_DEFINE_FACTOR(rec_depth);
_SPHERE_DEFINE_FACTOR(ao_samples);

#undef _SPHERE_DEFINE_FACTOR

#define _SPHERE_INIT_FACTOR(f)                 \
    _##f {                                     \
        config.get<decltype(_##f)>(factor_##f) \
    }

sphere_configuration::sphere_configuration(const configuration& config)
        : _SPHERE_INIT_FACTOR(data_set)
        , _SPHERE_INIT_FACTOR(frame)
        , _SPHERE_INIT_FACTOR(counter_iterations)
        , _SPHERE_INIT_FACTOR(min_prewarms)
        , _SPHERE_INIT_FACTOR(min_wall_time)
        , _SPHERE_INIT_FACTOR(spp)
        , _SPHERE_INIT_FACTOR(rec_depth)
        , _SPHERE_INIT_FACTOR(ao_samples) {}

#undef _SPHERE_INIT_FACTOR

} // namespace trrojan::ospray