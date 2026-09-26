#pragma once


#include "trrojan/ospray/device.h"
#include "trrojan/ospray/export.h"
#include "trrojan/ospray/render_target_base.h"

namespace trrojan::ospray {
class TRROJANOSPRAY_API bench_render_target : public render_target_base {
public:
    bench_render_target(const trrojan::ospray::device& device);

    unsigned int present(const unsigned int sync_interval) override;

    void resize(unsigned int width, unsigned int height) override;
};
} // namespace trrojan::ospray