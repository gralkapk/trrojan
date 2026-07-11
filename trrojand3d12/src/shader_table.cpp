#include "trrojan/d3d12/shader_table.h"

#include <assert.h>
#include <stdexcept>

namespace trrojan::d3d12 {
ShaderRecord::ShaderRecord(const UINT8 identifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES])
        : record_data_(GetAlignedShaderRecordSize(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES), 0) {
    std::memcpy(record_data_.data(), identifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
}

UINT64 ShaderRecord::CopyTo(void* dest, UINT64 dest_size) const {
    if (dest != nullptr) {
        if (dest_size < record_data_.size()) {
            throw std::runtime_error("Destination buffer is too small for shader record.");
        }
        std::memcpy(dest, record_data_.data(), record_data_.size());
    }
    return static_cast<UINT64>(record_data_.size());
}

ShaderTable& ShaderTable::SetRayGenRecord(const ShaderRecord& record) {
    raygen_record_ = record;
    return *this;
}

ShaderTable& ShaderTable::AddMissRecord(const ShaderRecord& record) {
    miss_records_.push_back(record);
    return *this;
}

ShaderTable& ShaderTable::AddHitGroupRecord(const ShaderRecord& record) {
    hitgroup_records_.push_back(record);
    return *this;
}

ShaderTable& ShaderTable::AddCallableRecord(const ShaderRecord& record) {
    callable_records_.push_back(record);
    return *this;
}

UINT64 ShaderTable::Serialize(void* buffer, UINT64 buffer_size, ShaderBindingTableDescriptor* descriptor) const {
    assert(descriptor != nullptr);

    // compute size of the shader table
    descriptor->raygen_record_size_ = raygen_record_.CopyTo(nullptr, 0);
    UINT64 sbt_size = GetAlignedShaderTableSize(descriptor->raygen_record_size_);
    descriptor->miss_records_offset_ = sbt_size;
    {
        const auto max_el = std::max_element(miss_records_.begin(), miss_records_.end(),
            [&](const ShaderRecord& a, const ShaderRecord& b) { return a.CopyTo(nullptr, 0) < b.CopyTo(nullptr, 0); });
        if (max_el != miss_records_.end()) {
            descriptor->miss_records_stride_ = max_el->CopyTo(nullptr, 0);
        } else {
            descriptor->miss_records_stride_ = 0;
        }
    }
    sbt_size +=
        (descriptor->miss_records_size_ = GetAlignedShaderTableSize(descriptor->miss_records_stride_ * miss_records_.size()));
    descriptor->hitgroup_records_offset_ = sbt_size;
    {
        const auto max_el = std::max_element(hitgroup_records_.begin(), hitgroup_records_.end(),
            [&](const ShaderRecord& a, const ShaderRecord& b) { return a.CopyTo(nullptr, 0) < b.CopyTo(nullptr, 0); });
        if (max_el != hitgroup_records_.end()) {
            descriptor->hitgroup_records_stride_ = max_el->CopyTo(nullptr, 0);
        } else {
            descriptor->hitgroup_records_stride_ = 0;
        }
    }
    sbt_size += (descriptor->hitgroup_records_size_ =
                     GetAlignedShaderTableSize(descriptor->hitgroup_records_stride_ * hitgroup_records_.size()));
    descriptor->callable_records_offset_ = sbt_size;
    {
        const auto max_el = std::max_element(callable_records_.begin(), callable_records_.end(),
            [&](const ShaderRecord& a, const ShaderRecord& b) { return a.CopyTo(nullptr, 0) < b.CopyTo(nullptr, 0); });
        if (max_el != callable_records_.end()) {
            descriptor->callable_records_stride_ = max_el->CopyTo(nullptr, 0);
        } else {
            descriptor->callable_records_stride_ = 0;
        }
    }
    sbt_size += (descriptor->callable_records_size_ =
                     GetAlignedShaderTableSize(descriptor->callable_records_stride_ * callable_records_.size()));

    // copy the shader table to the buffer if provided
    if (buffer != nullptr) {
        if (buffer_size < sbt_size) {
            throw std::runtime_error("Destination buffer is too small for shader table.");
        }
        // Copy each record type to the buffer
        UINT64 offset = 0;
        raygen_record_.CopyTo(static_cast<std::uint8_t*>(buffer) + offset, buffer_size - offset);
        offset = descriptor->miss_records_offset_;
        for (const auto& record : miss_records_) {
            record.CopyTo(static_cast<std::uint8_t*>(buffer) + offset, buffer_size - offset);
            offset += descriptor->miss_records_stride_;
        }
        offset = descriptor->hitgroup_records_offset_;
        for (const auto& record : hitgroup_records_) {
            record.CopyTo(static_cast<std::uint8_t*>(buffer) + offset, buffer_size - offset);
            offset += descriptor->hitgroup_records_stride_;
        }
        offset = descriptor->callable_records_offset_;
        for (const auto& record : callable_records_) {
            record.CopyTo(static_cast<std::uint8_t*>(buffer) + offset, buffer_size - offset);
            offset += descriptor->callable_records_stride_;
        }
    }

    return sbt_size;
}

} // namespace trrojan::d3d12