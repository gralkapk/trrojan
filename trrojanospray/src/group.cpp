#include "trrojan/ospray/group.h"

#include "ospray/ospray_util.h"

namespace trrojan::ospray {
group::group() : object{ospNewGroup()} {}
group::~group() {}
group& group::setGeometry(OSPGeometricModel geometry) {
    /*setObject("geometry", geometry);*/
    ospSetObjectAsData(getObject(), "geometry", OSP_GEOMETRIC_MODEL, geometry);
    return *this;
}
group& group::setVolume(OSPVolumetricModel volume) {
    setObject("volume", volume);
    return *this;
}
group& group::setLight(OSPLight light) {
    setObject("light", light);
    return *this;
}
//group& group::setGeometry(const std::vector<object>& geometries) {
//    setObject("geometry", geometries.data());
//    return *this;
//}
//group& group::setVolume(const std::vector<object>& volumes) {
//    setObject("volume", volumes.data());
//    return *this;
//}
//group& group::setLight(const std::vector<object>& lights) {
//    setObject("light", lights.data());
//    return *this;
//}
} // namespace trrojan::ospray