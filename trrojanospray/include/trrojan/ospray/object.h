#pragma once

#include "trrojan/ospray/export.h"

#include <ospray/ospray.h>

namespace trrojan::ospray {
class TRROJANOSPRAY_API object {
public:
    object();
    object(OSPObject osp_object);
    virtual ~object();

    object& commit();

    object& setParam(const char* id, OSPDataType type, const void* mem);
    object& removeParam(const char* id);

    object& setObject(const char* id, OSPObject osp_object);

    operator bool() const {
        return getObject() != nullptr;
    }

    template<typename T>
    operator T() const {
        return reinterpret_cast<T>(getObject());
    }

protected:
    OSPObject getObject() const {
        return _osp_object;
    }

    void setObject(OSPObject osp_object) {
        if (_osp_object) {
            ospRelease(_osp_object);
        }
        _osp_object = osp_object;
    }

    void dispose();

private:
    OSPObject _osp_object;
};
} // namespace trrojan::ospray