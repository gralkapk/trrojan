#include "trrojan/ospray/render_target_base.h"

#include <assert.h>
#include <cstring>

namespace trrojan::ospray {
render_target_base::render_target_base()
        : _width(0)
        , _height(0) {}

render_target_base::~render_target_base() {}

void render_target_base::clear() {
    ospResetAccumulation(reinterpret_cast<OSPFrameBuffer>(getObject()));
}

void render_target_base::resize(unsigned int width, unsigned int height) {
    if (getObject() == nullptr) {
        dispose();
    }
    setObject(ospNewFrameBuffer(width, height, OSP_FB_RGBA8, OSP_FB_COLOR | OSP_FB_DEPTH | OSP_FB_ACCUM));
    _width = width;
    _height = height;
}

unsigned int render_target_base::present(unsigned int sync_interval) {
    return -1;
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

void render_target_base::reset_buffers() {
    // Implementation for resetting buffers
}
} // namespace trrojan::ospray