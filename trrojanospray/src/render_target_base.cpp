#include "trrojan/ospray/render_target_base.h"

#include <assert.h>
#include <cstring>

namespace trrojan::ospray {
render_target_base::render_target_base(int width, int height)
        : object{ospNewFrameBuffer(width, height, OSP_FB_RGBA8, OSP_FB_COLOR | OSP_FB_DEPTH | OSP_FB_ACCUM)}
        , _width(width)
        , _height(height) {}

render_target_base::~render_target_base() {}

void render_target_base::clear() {
    ospResetAccumulation(reinterpret_cast<OSPFrameBuffer>(getObject()));
}

void render_target_base::resize(int width, int height) {
    dispose();
    setObject(ospNewFrameBuffer(width, height, OSP_FB_RGBA8, OSP_FB_COLOR | OSP_FB_DEPTH | OSP_FB_ACCUM));
}

void render_target_base::copyColorTo(void* dst) {
    auto const color = ospMapFrameBuffer(reinterpret_cast<OSPFrameBuffer>(getObject()), OSP_FB_COLOR);
    assert(color != nullptr);
    if (color == nullptr) {
        return;
    }
    // TODO check framebuffer format and size
    std::memcpy(dst, color, _width * _height * 4);
    ospUnmapFrameBuffer(color, reinterpret_cast<OSPFrameBuffer>(getObject()));
}
} // namespace trrojan::ospray