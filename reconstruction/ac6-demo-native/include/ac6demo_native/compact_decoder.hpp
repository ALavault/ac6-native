#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ac6demo_native {

struct CompactNode {
    std::uint8_t type = 0;
    std::uint8_t bits = 0;
    std::uint8_t extra_bits = 0;
    std::uint16_t base = 0;
};

struct CompactTables {
    std::uint8_t primary_width = 0;
    std::uint8_t distance_width = 0;
    std::span<const CompactNode> primary;
    std::span<const CompactNode> distance;
};

enum class CompactDecodeStatus {
    complete,
    truncated_input,
    invalid_table,
    output_overflow,
};

struct CompactDecodeResult {
    CompactDecodeStatus status = CompactDecodeStatus::invalid_table;
    std::size_t input_consumed = 0;
    std::size_t output_size = 0;
};

enum class CompactXorStatus {
    complete,
    output_overflow,
};

[[nodiscard]] CompactDecodeResult decode_compact(
    std::span<const std::byte> input, std::span<std::byte> output,
    CompactTables tables) noexcept;

[[nodiscard]] CompactXorStatus xor_repeating_be64(
    std::span<std::byte> output, std::uint64_t key, std::size_t cursor,
    std::size_t limit) noexcept;

}  // namespace ac6demo_native
