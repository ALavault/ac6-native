#include "ac6/retail_container_index.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("FAIL  %s\n", w); ++failures; } }
using namespace ac6::retail;

constexpr std::uint32_t kBase = 0xB0000000;

void put32(std::vector<std::uint8_t>& blob, std::size_t at, std::uint32_t v) {
  blob[at] = std::uint8_t(v >> 24); blob[at + 1] = std::uint8_t(v >> 16);
  blob[at + 2] = std::uint8_t(v >> 8); blob[at + 3] = std::uint8_t(v);
}

// version 1, flag 1, header size 0x10, count 4, four arrays of four dwords.
std::vector<std::uint8_t> native_container() {
  std::vector<std::uint8_t> blob(0x200, 0);
  std::memcpy(blob.data(), "FHM ", 4);
  blob[4] = 1; blob[5] = 1; blob[6] = 0x00; blob[7] = 0x10;
  put32(blob, 0x10, 4);
  const std::uint32_t offsets[4] = {0x100, 0x180, 0x000, 0x1C0};
  const std::uint32_t lengths[4] = {0x70, 0x40, 0x00, 0x30};
  for (int i = 0; i < 4; ++i) {
    put32(blob, 0x14 + 4 * i, offsets[i]);
    put32(blob, 0x14 + 16 + 4 * i, lengths[i]);
  }
  return blob;
}

void the_header_fields_come_from_the_right_bytes() {
  const std::vector<std::uint8_t> blob = native_container();
  ContainerIndex index{};
  check(parse_container_index(index, blob.data(), blob.size(), kBase), "parses");
  check(index.version == 1, "version is file+4");
  check(index.endian == 1, "endian flag is file+5");
  check(index.header_size == 0x10, "header size is file+6");
  check(index.count == 4, "count is at file+header_size");
  check(index.base == kBase, "base is carried verbatim");
}

void the_four_arrays_are_consecutive() {
  const std::vector<std::uint8_t> blob = native_container();
  ContainerIndex index{};
  parse_container_index(index, blob.data(), blob.size(), kBase);
  check(index.array0 == kBase + 0x14, "array0 is header + 4");
  check(index.array1 == index.array0 + 4 * 4, "array1 follows it");
  check(index.array2 == index.array0 + 4 * 8, "then array2");
  check(index.array3 == index.array0 + 4 * 12, "then array3");
}

void the_getter_returns_addresses_and_nulls() {
  const std::vector<std::uint8_t> blob = native_container();
  ContainerIndex index{};
  parse_container_index(index, blob.data(), blob.size(), kBase);
  check(container_entry(index, blob.data(), blob.size(), 0) == kBase + 0x100, "index 0");
  check(container_entry(index, blob.data(), blob.size(), 1) == kBase + 0x180, "index 1");
  // A ZERO TABLE ENTRY IS A NULL, not base + 0. 0x82234DF0.
  check(container_entry(index, blob.data(), blob.size(), 2) == 0,
        "a zero offset is a null, NOT the start of the file");
  check(container_entry(index, blob.data(), blob.size(), 4) == 0, "index == count refused");
  check(container_entry(index, blob.data(), blob.size(), 99) == 0, "and beyond");
}

void the_length_comes_from_array_one() {
  const std::vector<std::uint8_t> blob = native_container();
  ContainerIndex index{};
  parse_container_index(index, blob.data(), blob.size(), kBase);
  check(container_entry_length(index, blob.data(), blob.size(), 0) == 0x70, "length 0");
  check(container_entry_length(index, blob.data(), blob.size(), 1) == 0x40, "length 1");
  // CONTROL. Subtracting neighbouring offsets gives 0x80 for entry 0, not 0x70 --
  // that difference is the padding, and reading it as the length is the mistake
  // cycle 1419 made across 292 containers.
  const std::uint32_t by_subtraction =
      (kBase + 0x180) - (kBase + 0x100);
  check(by_subtraction != container_entry_length(index, blob.data(), blob.size(), 0),
        "subtracting offsets must DISAGREE with array1 -- it counts the padding");
}

void a_byte_swapped_container_keeps_retails_raw_count_arithmetic() {
  // The path no shipped file takes. flag 0, and every parsed field reversed.
  std::vector<std::uint8_t> blob = native_container();
  blob[5] = 0;
  blob[6] = 0x10; blob[7] = 0x00;             // header size, reversed
  put32(blob, 0x10, 0x04000000);              // count 4, reversed
  ContainerIndex index{};
  parse_container_index(index, blob.data(), blob.size(), kBase);
  check(index.header_size == 0x10, "the header size is swapped back");
  check(index.count == 4, "and so is the count, at +0x00");
  // But the ARRAYS were derived from the raw word before that swap.
  check(index.array1 == index.array0 + 0x04000000u * 4u,
        "array1 uses the RAW count -- retail computes it before swapping");
  check(index.array1 != index.array0 + 4u * 4u,
        "and must NOT be the tidy value; the differential rejected that");
}

}  // namespace

int main() {
  the_header_fields_come_from_the_right_bytes();
  the_four_arrays_are_consecutive();
  the_getter_returns_addresses_and_nulls();
  the_length_comes_from_array_one();
  a_byte_swapped_container_keeps_retails_raw_count_arithmetic();
  if (failures == 0) std::printf("retail_container_index: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
