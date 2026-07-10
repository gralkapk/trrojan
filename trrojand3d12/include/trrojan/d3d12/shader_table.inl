template<typename T>
inline trrojan::d3d12::ShaderRecord::ShaderRecord(
    const UINT8 identifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES], const T& data)
        : record_data_(GetAlignedShaderRecordSize(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(T)), 0) {
    std::memcpy(record_data_.data(), identifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    std::memcpy(record_data_.data() + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &data, sizeof(T));
};
