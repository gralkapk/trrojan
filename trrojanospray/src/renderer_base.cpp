#include "trrojan/ospray/renderer_base.h"

namespace trrojan::ospray {
#define _RENDERER_DEFINE_FACTOR(f) const char* renderer_base::factor_##f = #f

_RENDERER_DEFINE_FACTOR(spp);
_RENDERER_DEFINE_FACTOR(path_length);
_RENDERER_DEFINE_FACTOR(ao_samples);
_RENDERER_DEFINE_FACTOR(ao_distance);
_RENDERER_DEFINE_FACTOR(volume_sampling_rate);

#undef _RENDERER_DEFINE_FACTOR

renderer_base::renderer_base() : object{ospNewRenderer("scivis")} {

}

renderer_base::~renderer_base() {}
} // namespace trrojan::ospray