#include "ac6demo_native/compact_decoder.hpp"

#include <array>
#include <cstddef>
#include <iostream>

namespace {

using ac6demo_native::CompactDecodeStatus;
using ac6demo_native::CompactNode;
using ac6demo_native::CompactTables;
using ac6demo_native::CompactXorStatus;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void test_literal_backref_and_termination() {
    const std::array<CompactNode, 4> primary{{
        {1, 2, 0, static_cast<std::uint16_t>('A')},
        {2, 2, 0, 2},
        {3, 2, 0, 0},
        {},
    }};
    const std::array<CompactNode, 2> distance{{{2, 1, 0, 1}, {}}};
    const std::array<std::byte, 2> input{
        std::byte{0x10}, std::byte{0x08}};
    std::array<std::byte, 3> output{};

    const auto result = ac6demo_native::decode_compact(
        input, output, CompactTables{2, 1, primary, distance});
    require(result.status == CompactDecodeStatus::complete,
            "literal/backref stream completes");
    require(result.output_size == 3U && output[0] == std::byte{'A'} &&
                output[1] == std::byte{'A'} && output[2] == std::byte{'A'},
            "overlapping backref reproduces AAA");
}

void test_type4_transition() {
    const std::array<CompactNode, 4> primary{{
        {4, 2, 1, 0},
        {1, 2, 0, static_cast<std::uint16_t>('B')},
        {3, 2, 0, 0},
        {},
    }};
    const std::array<CompactNode, 1> distance{{}};
    const std::array<std::byte, 2> input{
        std::byte{0x10}, std::byte{0x01}};
    std::array<std::byte, 1> output{};

    const auto result = ac6demo_native::decode_compact(
        input, output, CompactTables{2, 0, primary, distance});
    require(result.status == CompactDecodeStatus::complete &&
                result.output_size == 1U && output[0] == std::byte{'B'},
            "type4 transition reaches literal");
}

void test_truncated_input() {
    const std::array<CompactNode, 4> primary{{{3, 2, 0, 0}, {}, {}, {}}};
    const std::array<CompactNode, 1> distance{{}};
    const std::array<std::byte, 1> input{std::byte{0x00}};
    std::array<std::byte, 1> output{};

    const auto result = ac6demo_native::decode_compact(
        input, output, CompactTables{2, 0, primary, distance});
    require(result.status == CompactDecodeStatus::truncated_input,
            "short reservoir input is rejected");
}

void test_repeating_be64_xor() {
    std::array<std::byte, 10> output{
        std::byte{0x00}, std::byte{0x11}, std::byte{0x22}, std::byte{0x33},
        std::byte{0x44}, std::byte{0x55}, std::byte{0x66}, std::byte{0x77},
        std::byte{0x88}, std::byte{0x99}};
    const auto status = ac6demo_native::xor_repeating_be64(
        output, 0x0102030405060708ULL, 0U, output.size());
    const std::array<std::byte, 10> expected{
        std::byte{0x01}, std::byte{0x13}, std::byte{0x21}, std::byte{0x37},
        std::byte{0x41}, std::byte{0x53}, std::byte{0x61}, std::byte{0x7f},
        std::byte{0x89}, std::byte{0x9b}};
    require(status == CompactXorStatus::complete && output == expected,
            "big-endian repeating XOR matches the PPC block/tail path");
}

void test_repeating_be64_bounds() {
    std::array<std::byte, 4> output{};
    require(ac6demo_native::xor_repeating_be64(output, 0U, 3U, 2U) ==
                CompactXorStatus::output_overflow,
            "reversed XOR bounds are rejected");
    require(ac6demo_native::xor_repeating_be64(output, 0U, 0U, 5U) ==
                CompactXorStatus::output_overflow,
            "XOR limit past output is rejected");
}

}  // namespace

int main() {
    test_literal_backref_and_termination();
    test_type4_transition();
    test_truncated_input();
    test_repeating_be64_xor();
    test_repeating_be64_bounds();
    return 0;
}
