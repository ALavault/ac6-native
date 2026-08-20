#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6demo/renderer_canonical_tiling.hpp"
#include "ac6demo/runtime_error.hpp"
#include "ac6demo/hash.hpp"
#include "ac6demo/xenos_tiling.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::byte> make_nonzero_pattern() {
  std::vector<std::byte> linear(ac6demo::kReachedResolveLinearBytes);
  for (std::uint32_t y = 0; y < ac6demo::kReachedResolveHeight; ++y) {
    for (std::uint32_t x = 0; x < ac6demo::kReachedResolveWidth; ++x) {
      const auto offset =
          (static_cast<std::size_t>(y) * ac6demo::kReachedResolveWidth + x) * 4U;
      linear[offset] = static_cast<std::byte>(x ^ (y << 1U));
      linear[offset + 1U] = static_cast<std::byte>((x >> 3U) + y);
      linear[offset + 2U] = static_cast<std::byte>((x * 17U) ^ (y * 31U));
      linear[offset + 3U] = static_cast<std::byte>(0x80U | ((x + y) & 0x7FU));
    }
  }
  return linear;
}

} // namespace

int main() {
  assert(ac6demo::reached_rgba8_tiled_offset(0U, 0U) == 0U);
  assert(ac6demo::reached_rgba8_tiled_offset(1U, 0U) == 4U);
  assert(ac6demo::reached_rgba8_tiled_offset(0U, 1U) == 0x10U);
  assert(ac6demo::reached_rgba8_tiled_offset(31U, 31U) == 0xF7CU);
  assert(ac6demo::reached_rgba8_tiled_offset(32U, 0U) == 0x1000U);
  assert(ac6demo::reached_rgba8_tiled_offset(1279U, 719U) == 0x39777CU);
  assert(ac6demo::kReachedResolveLinearBytes == 0x384000U);
  assert(ac6demo::kReachedResolveTiledExtentBytes == 0x398000U);
  assert(ac6demo::kReachedResolveTiledPaddingBytes == 0x14000U);

  std::vector<bool> occupied(ac6demo::kReachedResolveTiledExtentBytes / 4U);
  std::size_t occupied_pixels = 0U;
  for (std::uint32_t y = 0; y < ac6demo::kReachedResolveHeight; ++y) {
    for (std::uint32_t x = 0; x < ac6demo::kReachedResolveWidth; ++x) {
      const std::size_t offset = ac6demo::reached_rgba8_tiled_offset(x, y);
      assert((offset & 3U) == 0U);
      assert(!occupied[offset / 4U]);
      occupied[offset / 4U] = true;
      ++occupied_pixels;
    }
  }
  assert(occupied_pixels == static_cast<std::size_t>(1280U) * 720U);

  const auto expected = make_nonzero_pattern();
  constexpr std::byte canary{0xA5};
  std::vector<std::byte> tiled(ac6demo::kReachedResolveTiledExtentBytes,
                               canary);
  ac6demo::tile_reached_rgba8(expected, tiled);

  std::size_t preserved_padding = 0U;
  for (std::size_t word = 0U; word < occupied.size(); ++word) {
    if (occupied[word]) {
      continue;
    }
    const auto offset = word * 4U;
    assert(std::all_of(tiled.begin() + static_cast<std::ptrdiff_t>(offset),
                       tiled.begin() + static_cast<std::ptrdiff_t>(offset + 4U),
                       [](std::byte value) { return value == canary; }));
    preserved_padding += 4U;
  }
  assert(preserved_padding == ac6demo::kReachedResolveTiledPaddingBytes);

  std::vector<std::byte> roundtrip(ac6demo::kReachedResolveLinearBytes);
  ac6demo::untile_reached_rgba8(tiled, roundtrip);
  assert(roundtrip == expected);
  assert(ac6demo::Sha256::bytes(roundtrip) ==
         ac6demo::Sha256::bytes(expected));

  std::vector<std::byte> guest_tiled(
      ac6demo::kReachedResolveTiledExtentBytes, std::byte{0x7E});
  const auto canonical_linear =
      ac6demo::canonicalize_reached_tiled_writeback(
          tiled, ac6demo::Sha256::bytes(expected), guest_tiled);
  assert(canonical_linear == expected);
  std::vector<std::byte> canonical_guest_linear(
      ac6demo::kReachedResolveLinearBytes);
  ac6demo::untile_reached_rgba8(guest_tiled, canonical_guest_linear);
  assert(canonical_guest_linear == expected);
  const auto guest_before_rejection = guest_tiled;
  bool canonical_trapped = false;
  try {
    static_cast<void>(ac6demo::canonicalize_reached_tiled_writeback(
        tiled, std::string(64U, '0'), guest_tiled));
  } catch (const ac6demo::RuntimeTrap &) {
    canonical_trapped = true;
  }
  assert(canonical_trapped);
  assert(guest_tiled == guest_before_rejection);
  canonical_trapped = false;
  try {
    static_cast<void>(ac6demo::canonicalize_reached_tiled_writeback(
        tiled, "not-a-sha256", guest_tiled));
  } catch (const ac6demo::RuntimeTrap &) {
    canonical_trapped = true;
  }
  assert(canonical_trapped);
  assert(guest_tiled == guest_before_rejection);

  std::vector<std::byte> canonical_again(
      ac6demo::kReachedResolveTiledExtentBytes, std::byte{0x3C});
  ac6demo::tile_reached_rgba8(roundtrip, canonical_again);
  for (std::size_t word = 0U; word < occupied.size(); ++word) {
    const auto offset = word * 4U;
    if (occupied[word]) {
      assert(std::equal(tiled.begin() + static_cast<std::ptrdiff_t>(offset),
                        tiled.begin() + static_cast<std::ptrdiff_t>(offset + 4U),
                        canonical_again.begin() +
                            static_cast<std::ptrdiff_t>(offset)));
    } else {
      assert(std::all_of(
          canonical_again.begin() + static_cast<std::ptrdiff_t>(offset),
          canonical_again.begin() + static_cast<std::ptrdiff_t>(offset + 4U),
          [](std::byte value) { return value == std::byte{0x3C}; }));
    }
  }

  std::vector<std::byte> black_tiled(ac6demo::kReachedResolveTiledExtentBytes,
                                      std::byte{0xA5});
  std::vector<std::byte> black_linear(ac6demo::kReachedResolveLinearBytes);
  ac6demo::tile_reached_rgba8(black_linear, black_tiled);
  std::vector<std::byte> black_roundtrip(ac6demo::kReachedResolveLinearBytes);
  ac6demo::untile_reached_rgba8(black_tiled, black_roundtrip);
  assert(ac6demo::Sha256::bytes(black_roundtrip) ==
         "0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f");

  bool trapped = false;
  try {
    static_cast<void>(ac6demo::reached_rgba8_tiled_offset(1280U, 0U));
  } catch (const ac6demo::RuntimeTrap &) {
    trapped = true;
  }
  assert(trapped);

  trapped = false;
  try {
    std::vector<std::byte> short_linear(
        ac6demo::kReachedResolveLinearBytes - 4U);
    ac6demo::tile_reached_rgba8(short_linear, tiled);
  } catch (const ac6demo::RuntimeTrap &) {
    trapped = true;
  }
  assert(trapped);

  std::cout << "ac6-demo-xenos-tiling-tests: ok\n";
}
