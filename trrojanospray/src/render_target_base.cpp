#include "trrojan/ospray/render_target_base.h"

namespace trrojan::ospray {
render_target_base::render_target_base(int width, int height)
        : object{ospNewFrameBuffer(width, height, OSP_FB_RGBA8, OSP_FB_COLOR | OSP_FB_DEPTH | OSP_FB_ACCUM)} {}

render_target_base::~render_target_base() {}

void render_target_base::clear() {
    ospResetAccumulation(reinterpret_cast<OSPFrameBuffer>(getObject()));
}

void render_target_base::resize(int width, int height) {
    dispose();
    setObject(ospNewFrameBuffer(width, height, OSP_FB_RGBA8, OSP_FB_COLOR | OSP_FB_DEPTH | OSP_FB_ACCUM));
}
} // namespace trrojan::ospray