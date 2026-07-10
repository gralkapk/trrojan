#pragma once

#include <vector>

#include <d3d12.h>

#include "trrojan/d3d12/export.h"

namespace trrojan::d3d12 {
inline constexpr UINT64 GetAlignedSize(UINT64 size, UINT64 alignment) {
    return (size + (alignment - 1)) & ~(alignment - 1);
}

inline constexpr UINT64 GetAlignedShaderRecordSize(UINT64 size) {
    return GetAlignedSize(size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
}

inline constexpr UINT64 GetAlignedShaderTableSize(UINT64 size) {
    return GetAlignedSize(size, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
}

class TRROJAND3D12_API ShaderRecord {
public:
    ShaderRecord() = default;

    explicit ShaderRecord(const UINT8 identifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES]);

    template<typename T>
    explicit ShaderRecord(const UINT8 identifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES], const T& data);

    ShaderRecord(const ShaderRecord&) = default;
    ShaderRecord(ShaderRecord&&) noexcept = default;
    ShaderRecord& operator=(const ShaderRecord&) = default;
    ShaderRecord& operator=(ShaderRecord&&) noexcept = default;

    ~ShaderRecord() = default;

    UINT64 CopyTo(void* dest, UINT64 dest_size) const;

private:
    std::vector<std::uint8_t> record_data_;
};

struct TRROJAND3D12_API ShaderBindingTableDescriptor {
    UINT64 raygen_record_size_ = 0;
    UINT64 miss_records_offset_ = 0;
    UINT64 miss_records_size_ = 0;
    UINT64 miss_records_stride_ = 0;
    UINT64 hitgroup_records_offset_ = 0;
    UINT64 hitgroup_records_size_ = 0;
    UINT64 hitgroup_records_stride_ = 0;
    UINT64 callable_records_offset_ = 0;
    UINT64 callable_records_size_ = 0;
    UINT64 callable_records_stride_ = 0;
};

class TRROJAND3D12_API ShaderTable final {
public:
    ShaderTable() = default;
    ShaderTable(const ShaderTable&) = delete;
    ShaderTable(ShaderTable&&) = default;
    ShaderTable& operator=(const ShaderTable&) = delete;
    ShaderTable& operator=(ShaderTable&&) = default;
    ~ShaderTable() = default;

    ShaderTable& SetRayGenRecord(const ShaderRecord& record);
    ShaderTable& AddMissRecord(const ShaderRecord& record);
    ShaderTable& AddHitGroupRecord(const ShaderRecord& record);
    ShaderTable& AddCallableRecord(const ShaderRecord& record);

    UINT64 Serialize(void* buffer, UINT64 buffer_size, ShaderBindingTableDescriptor* descriptor) const;

private:
    ShaderRecord raygen_record_;
    std::vector<ShaderRecord> miss_records_;
    std::vector<ShaderRecord> hitgroup_records_;
    std::vector<ShaderRecord> callable_records_;
};

} // namespace trrojan::d3d12

#include "trrojan/d3d12/shader_table.inl"
