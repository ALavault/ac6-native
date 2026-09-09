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

void put16(std::vector<std::uint8_t>& bytes, std::size_t at,
           std::uint16_t value) {
  bytes[at] = static_cast<std::uint8_t>(value >> 8);
  bytes[at + 1U] = static_cast<std::uint8_t>(value);
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

std::vector<std::uint8_t> pack(const std::uint16_t count,
                               const std::uint32_t identifier_base) {
  constexpr std::size_t kDescriptorBase = 0x10U;
  constexpr std::size_t kSectionBase = 0x60U;
  constexpr std::size_t kStride = 0x50U;
  constexpr std::size_t kDataBase = 0x1000U;
  // Every generated shape is at most 32x36 texels. BC1's Xenos tile padding
  // therefore occupies one 32x32-block tile: 8,192 bytes.
  constexpr std::size_t kSurfaceBytes = 8192U;
  std::vector<std::uint8_t> bytes(kDataBase + count * kSurfaceBytes, 0U);
  std::memcpy(bytes.data(), "NTXR", 4U);
  bytes[4] = 2U;
  put16(bytes, 0x06U, count);
  put16(bytes, kDescriptorBase + 0x0CU,
        static_cast<std::uint16_t>(kSectionBase - kDescriptorBase));
  for (std::uint16_t index = 0U; index < count; ++index) {
    const std::size_t descriptor =
        index == 0U ? kDescriptorBase
                    : kSectionBase + static_cast<std::size_t>(index - 1U) *
                                         kStride;
    const std::size_t next_descriptor =
        kSectionBase + static_cast<std::size_t>(index) * kStride;
    bytes[descriptor + 0x11U] = 1U;
    bytes[descriptor + 0x13U] = 0U;
    put16(bytes, descriptor + 0x14U,
          static_cast<std::uint16_t>(4U * (index + 1U)));
    put16(bytes, descriptor + 0x16U,
          static_cast<std::uint16_t>(4U * (count + 1U)));
    const std::size_t data = kDataBase + index * kSurfaceBytes;
    put32(bytes, descriptor + 0x20U,
          static_cast<std::uint32_t>(data - descriptor));
    std::memcpy(bytes.data() + next_descriptor - 0x20U, "eXt\0", 4U);
    put32(bytes, next_descriptor - 0x1CU, 0x20U);
    std::memcpy(bytes.data() + next_descriptor - 0x10U, "GIDX", 4U);
    put32(bytes, next_descriptor - 0x0CU, 0x10U);
    put32(bytes, next_descriptor - 0x08U, identifier_base + index);
    bytes[data] = static_cast<std::uint8_t>(index + 1U);
  }
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

  // Small deterministic generation budget, no added PBT dependency: for every
  // pack size 1..8 and every valid key, selection must return the exact encoded
  // shape. This is an oracle property over the whole bounded generated domain,
  // not a restatement of the parser's offset arithmetic.
  bool generated_selection = true;
  for (std::uint16_t count = 1U; count <= 8U; ++count) {
    const std::uint32_t base = 0x10002000U + count * 0x10U;
    const std::vector<std::uint8_t> generated = pack(count, base);
    for (std::uint16_t index = 0U; index < count; ++index) {
      ac6::retail::NtxrRefusal refusal = ac6::retail::NtxrRefusal::BadHeader;
      const auto texture = ac6::retail::decode_ntxr_pack_texture(
          generated.data(), generated.size(), base + index, false, &refusal);
      generated_selection =
          generated_selection && texture.has_value() &&
          refusal == ac6::retail::NtxrRefusal::None &&
          texture->width == 4U * (index + 1U) &&
          texture->height == 4U * (count + 1U) &&
          texture->pixels.size() ==
              static_cast<std::size_t>(texture->width) * texture->height;
    }
  }
  check(generated_selection,
        "generated packs select every GIDX and preserve its exact shape");

  std::vector<std::uint8_t> generated = pack(3U, 0x10003000U);
  ac6::retail::NtxrRefusal refusal = ac6::retail::NtxrRefusal::None;
  check(!ac6::retail::decode_ntxr_pack_texture(
             generated.data(), generated.size(), 0x10003FFFU, false, &refusal)
             .has_value() &&
            refusal == ac6::retail::NtxrRefusal::IdentifierNotFound,
        "an absent pack key has a distinct refusal");
  put32(generated, 0xA8U, 0x10003000U);
  check(!ac6::retail::decode_ntxr_pack_texture(
             generated.data(), generated.size(), 0x10003000U, false, &refusal)
             .has_value() &&
            refusal == ac6::retail::NtxrRefusal::BadHeader,
        "a duplicate pack key fails closed");
  generated = pack(3U, 0x10003000U);
  generated.pop_back();
  check(!ac6::retail::decode_ntxr_pack_texture(
             generated.data(), generated.size(), 0x10003002U, false, &refusal)
             .has_value() &&
            refusal == ac6::retail::NtxrRefusal::PayloadSizeMismatch,
        "a truncated final surface fails closed");
  generated = pack(3U, 0x10003000U);
  put16(generated, 0x06U, 0U);
  check(!ac6::retail::decode_ntxr_pack_texture(
             generated.data(), generated.size(), 0x10003000U, false, &refusal)
             .has_value() &&
            refusal == ac6::retail::NtxrRefusal::BadHeader,
        "a zero pack count fails closed");
  generated = pack(3U, 0x10003000U);
  put16(generated, 0x100U + 0x14U, 1U);
  check(!ac6::retail::decode_ntxr_pack_texture(
             generated.data(), generated.size(), 0x10003000U, false, &refusal)
             .has_value() &&
            refusal == ac6::retail::NtxrRefusal::BadHeader,
        "a nonzero terminator descriptor fails closed");

  if (failures == 0) std::printf("NTXR GIDX reader OK\n");
  return failures == 0 ? 0 : 1;
}
