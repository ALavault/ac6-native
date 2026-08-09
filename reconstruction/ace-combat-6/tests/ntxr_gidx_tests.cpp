#include "ac6/ntxr_texture.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::printf("FAIL  %s\n", message);
    ++failures;
  }
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t at,
           std::uint32_t value) {
  bytes[at] = static_cast<std::uint8_t>(value >> 24);
  bytes[at + 1] = static_cast<std::uint8_t>(value >> 16);
  bytes[at + 2] = static_cast<std::uint8_t>(value >> 8);
  bytes[at + 3] = static_cast<std::uint8_t>(value);
}

std::vector<std::uint8_t> wrapper(std::uint32_t identifier) {
  std::vector<std::uint8_t> bytes(0x90, 0);
  std::memcpy(bytes.data(), "NTXR", 4);
  bytes[0x21] = 1;
  bytes[0x23] = 0;
  put32(bytes, 0x30, 0x70);
  std::memcpy(bytes.data() + 0x40, "eXt\0", 4);
  put32(bytes, 0x44, 0x20);
  std::memcpy(bytes.data() + 0x50, "GIDX", 4);
  put32(bytes, 0x54, 0x10);
  put32(bytes, 0x58, identifier);
  return bytes;
}

}  // namespace

int main() {
  std::vector<std::uint8_t> bytes = wrapper(0x10203040);
  check(ac6::retail::ntxr_gidx_identifier(bytes.data(), bytes.size()) ==
            0x10203040u,
        "GIDX+0x08 is the registry identifier");

  bytes = wrapper(0);
  check(!ac6::retail::ntxr_gidx_identifier(bytes.data(), bytes.size())
             .has_value(),
        "a zero registry identifier fails closed");
  bytes = wrapper(1);
  bytes[0x50] = 'X';
  check(!ac6::retail::ntxr_gidx_identifier(bytes.data(), bytes.size())
             .has_value(),
        "a missing GIDX chunk fails closed");
  bytes = wrapper(1);
  std::memcpy(bytes.data() + 0x60, "eXt\0", 4);
  put32(bytes, 0x64, 0x20);
  std::memcpy(bytes.data() + 0x70, "GIDX", 4);
  put32(bytes, 0x74, 0x10);
  put32(bytes, 0x78, 2);
  check(!ac6::retail::ntxr_gidx_identifier(bytes.data(), bytes.size())
             .has_value(),
        "two candidate GIDX chunks fail closed");
  bytes = wrapper(1);
  put32(bytes, 0x30, 0xFFFFFFF0u);
  check(!ac6::retail::ntxr_gidx_identifier(bytes.data(), bytes.size())
             .has_value(),
        "a payload base outside the wrapper fails closed");

  if (failures == 0) std::printf("NTXR GIDX reader OK\n");
  return failures == 0 ? 0 : 1;
}
