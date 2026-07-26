[[nodiscard]] std::optional<std::size_t> ac6::find_ndxr_function_822c31e8_key(
    const std::span<const std::byte> bytes,
    const std::int16_t key,
    const std::size_t start_index) {
    const auto header = parse_ndxr(bytes);
    const auto capacity = ndxr_function_822c2148_record_capacity(header);
    if (start_index >= capacity) {
        return std::nullopt;
    }

    for (std::size_t index = start_index; index < capacity; ++index) {
        constexpr std::size_t first_record_offset = 0x30U;
        constexpr std::size_t record_stride = 0x30U;
        const auto offset = first_record_offset + index * record_stride;
        if (static_cast<std::int16_t>(read_be16(bytes, offset + 0x24U)) == key) {
            return index;
        }
    }

    return std::nullopt;
}