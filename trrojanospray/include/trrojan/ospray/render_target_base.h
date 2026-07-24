#pragma once

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

#include <ospray/ospray.h>

namespace trrojan::ospray {
class TRROJANOSPRAY_API render_target_base : public object {
public:
    render_target_base();

    virtual ~render_target_base();

    void clear();

    virtual void resize(unsigned int width, unsigned int height);

    virtual unsigned int present(unsigned int sync_interval);

    void copyColorTo(void* dst);

    unsigned int getWidth() const {
        return _width;
    }

    unsigned int getHeight() const {
        return _height;
    }

    float getAspectRatio() const {
        return static_cast<float>(_width) / static_cast<float>(_height);
    }

    virtual void reset_buffers();

private:
    unsigned int _width;
    unsigned int _height;
};
} // namespace trrojan::ospray