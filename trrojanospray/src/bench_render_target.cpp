#include "trrojan/ospray/bench_render_target.h"

#include "trrojan/log.h"

namespace trrojan::ospray {
bench_render_target::bench_render_target(const trrojan::ospray::device& device) : render_target_base() {}

unsigned int bench_render_target::present(const unsigned int sync_interval) {
    // nothing to do
    return 0;
}

void bench_render_target::resize(const unsigned int width, const unsigned int height) {
    log::instance().write_line(
        log_level::debug, "Resizing render target {:p} to [{}, {}].", static_cast<void*>(this), width, height);
    render_target_base::resize(width, height);
}
}
