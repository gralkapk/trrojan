#pragma once

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

#include <ospray/ospray.h>

namespace trrojan::ospray {
class TRROJANOSPRAY_API render_target_base : public object {
public:
    explicit render_target_base(int width, int height);

    virtual ~render_target_base();

    void clear();

    void resize(int width, int height);

private:
};
} // namespace trrojan::ospray