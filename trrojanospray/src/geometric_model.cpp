#include "trrojan/ospray/geometric_model.h"

namespace trrojan::ospray {
geometric_model::geometric_model() : object{ospNewGeometricModel()} {}
geometric_model::geometric_model(OSPGeometry geometry) : object{ospNewGeometricModel(geometry)} {}

geometric_model::~geometric_model() {}

} // namespace trrojan::ospray
