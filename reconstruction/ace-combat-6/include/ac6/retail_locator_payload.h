#pragma once

// Static PAL default.xex evidence for the locator payload copy at 0x822A6710.
// Qualification: ghidra-projects/ace-combat-6, target PAL default.xex, module
// default.xex, SHA-256
// acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde.
//
// Its .pdata body contains 42 instructions. At 0x822A677C..0x822A6798 four
// lvx128/stvx128 pairs copy child+0x70/+0x80/+0x90/+0xA0 to
// player+0x90/+0xA0/+0xB0/+0xC0. The static corpus selects class 0, index 0,
// and constructs this locator for M01..M15 (bitmap 0x7FFF).
//
// This type records only the proven 0x40-byte transfer. The bytes are not
// floats and are deliberately not a RetailBasis: static evidence does not
// authorize numerical interpretation or installation in an unproved runtime.

#include <array>
#include <cstddef>
#include <type_traits>

namespace ac6::retail {

inline constexpr std::size_t kRetailLocatorPayloadBytes = 0x40;

struct alignas(16) RetailLocatorPayload {
  std::array<std::byte, kRetailLocatorPayloadBytes> bytes;
};

static_assert(sizeof(RetailLocatorPayload) == kRetailLocatorPayloadBytes);
static_assert(alignof(RetailLocatorPayload) == 16);
static_assert(std::is_trivial_v<RetailLocatorPayload>);
static_assert(std::is_trivially_copyable_v<RetailLocatorPayload>);

// Copies exactly the four vectors proved above. Identical source and
// destination are accepted; no byte is interpreted or canonicalized.
void copy_locator_payload(RetailLocatorPayload& destination,
                          const RetailLocatorPayload& source) noexcept;

}  // namespace ac6::retail
