#include "trrojan/ospray/object.h"

#include <ospray/ospray_util.h>

namespace trrojan::ospray {
object::object() : _osp_object{nullptr} {}

object::object(OSPObject osp_object) : _osp_object{osp_object} {}

object::~object() {
    if (_osp_object) {
        ospRelease(_osp_object);
    }
}

object& object::commit() {
    ospCommit(_osp_object);
    return *this;
}

object& object::setParam(const char* id, OSPDataType type, const void* mem) {
    if (_osp_object) {
        ospSetParam(_osp_object, id, type, mem);
    }
    return *this;
}

object& object::removeParam(const char* id) {
    if (_osp_object) {
        ospRemoveParam(_osp_object, id);
    }
    return *this;
}

object& object::setObject(const char* id, OSPObject osp_object) {
    ospSetObject(_osp_object, id, osp_object);
    return *this;
}

void object::dispose() {
    if (_osp_object) {
        ospRelease(_osp_object);
        _osp_object = nullptr;
    }
}
} // namespace trrojan::ospray