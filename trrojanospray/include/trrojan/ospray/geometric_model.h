#pragma once

#include <vector>

#include "trrojan/ospray/export.h"
#include "trrojan/ospray/object.h"

#include <glm/glm.hpp>

namespace trrojan::ospray {
class TRROJANOSPRAY_API geometric_model : public object {
public:
    geometric_model();
    explicit geometric_model(OSPGeometry geometry);
    virtual ~geometric_model();

    geometric_model& setGeometry(OSPGeometry geometry) {
        setObject("geometry", geometry);
        return *this;
    }

    geometric_model& setMaterial(OSPMaterial material) {
        setObject("material", material);
        return *this;
    }

    geometric_model& setColor(const std::vector<glm::vec4>& color) {
        auto tmp_data = ospNewSharedData(color.data(), OSP_VEC4F, color.size());
        auto data = ospNewData(OSP_VEC4F, color.size());
        ospCopyData(tmp_data, data);
        setObject("color", data);
        ospRelease(data);
        ospRelease(tmp_data);
        return *this;
    }
};
} // namespace trrojan::ospray