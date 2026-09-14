#include "trrojan/ospray/volume_configuration.h"

namespace trrojan::ospray {
#define _VOLUME_DEFINE_FACTOR(f) const char* volume_configuration::factor_##f = #f

_VOLUME_DEFINE_FACTOR(data_set);
_VOLUME_DEFINE_FACTOR(frame);
_VOLUME_DEFINE_FACTOR(counter_iterations);
_VOLUME_DEFINE_FACTOR(min_prewarms);
_VOLUME_DEFINE_FACTOR(min_wall_time);
_VOLUME_DEFINE_FACTOR(spp);
_VOLUME_DEFINE_FACTOR(rec_depth);
_VOLUME_DEFINE_FACTOR(ao_samples);
_VOLUME_DEFINE_FACTOR(volume_sampling_rate);
_VOLUME_DEFINE_FACTOR(xfer_func);
_VOLUME_DEFINE_FACTOR(fovy);

#undef _VOLUME_DEFINE_FACTOR

#define _VOLUME_INIT_FACTOR(f)                 \
    _##f {                                     \
        config.get<decltype(_##f)>(factor_##f) \
    }

volume_configuration::volume_configuration(const configuration& config)
        : _VOLUME_INIT_FACTOR(data_set)
        , _VOLUME_INIT_FACTOR(frame)
        , _VOLUME_INIT_FACTOR(counter_iterations)
        , _VOLUME_INIT_FACTOR(min_prewarms)
        , _VOLUME_INIT_FACTOR(min_wall_time)
        , _VOLUME_INIT_FACTOR(spp)
        , _VOLUME_INIT_FACTOR(rec_depth)
        , _VOLUME_INIT_FACTOR(ao_samples)
        , _VOLUME_INIT_FACTOR(volume_sampling_rate)
        , _VOLUME_INIT_FACTOR(xfer_func)
        , _VOLUME_INIT_FACTOR(fovy) {}

#undef _VOLUME_INIT_FACTOR

} // namespace trrojan::ospray