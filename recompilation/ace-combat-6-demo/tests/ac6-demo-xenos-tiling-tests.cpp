#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6demo/runtime_error.hpp"
#include "ac6demo/hash.hpp"
#include "ac6demo/xenos_tiling.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
  assert(ac6demo::reached_rgba8_tiled_offset(0U, 0U) == 0U);
  assert(ac6demo::reached_rgba8_tiled_offset(1U, 0U) == 4U);
  assert(ac6demo::reached_rgba8_tiled_offset(0U, 1U) == 0x10U);
  assert(ac6demo::reached_rgba8_tiled_offset(31U, 31U) == 0xF7CU);
  assert(ac6demo::reached_rgba8_tiled_offset(32U, 0U) == 0x1000U);
  assert(ac6demo::reached_rgba8_tiled_offset(1279U, 719U) == 0x39777CU);

  std::vector<bool> occupied(ac6demo::kReachedResolveTiledExtentBytes / 4U);
  std::vector<std::byte> tiled(ac6demo::kReachedResolveTiledExtentBytes);
  for (std::uint32_t y = 0; y < ac6demo::kReachedResolveHeight; ++y) {
    for (std::uint32_t x = 0; x < ac6demo::kReachedResolveWidth; ++x) {
      const std::size_t offset = ac6demo::reached_rgba8_tiled_offset(x, y);
      assert((offset & 3U) == 0U);
      assert(!occupied[offset / 4U]);
      occupied[offset / 4U] = true;
      tiled[offset] = static_cast<std::byte>(x);
      tiled[offset + 1U] = static_cast<std::byte>(y);
      tiled[offset + 2U] = static_cast<std::byte>(x >> 8U);
      tiled[offset + 3U] = static_cast<std::byte>(y >> 8U);
    }
  }

  std::vector<std::byte> linear(ac6demo::kReachedResolveLinearBytes);
  ac6demo::untile_reached_rgba8(tiled, linear);
  const auto pixel = [&](std::uint32_t x, std::uint32_t y,
                         std::uint32_t channel) {
    return linear[(static_cast<std::size_t>(y) *
                   ac6demo::kReachedResolveWidth + x) * 4U + channel];
  };
  assert(pixel(1279U, 719U, 0U) == static_cast<std::byte>(1279U));
  assert(pixel(1279U, 719U, 1U) == static_cast<std::byte>(719U));
  assert(pixel(1279U, 719U, 2U) == static_cast<std::byte>(1279U >> 8U));
  assert(pixel(1279U, 719U, 3U) == static_cast<std::byte>(719U >> 8U));

  std::vector<std::byte> black_tiled(ac6demo::kReachedResolveTiledExtentBytes,
                                      std::byte{0xA5});
  for (std::uint32_t y = 0; y < ac6demo::kReachedResolveHeight; ++y) {
    for (std::uint32_t x = 0; x < ac6demo::kReachedResolveWidth; ++x) {
      const auto offset = ac6demo::reached_rgba8_tiled_offset(x, y);
      std::fill_n(black_tiled.begin() + static_cast<std::ptrdiff_t>(offset),
                  4U, std::byte{0});
    }
  }
  std::vector<std::byte> black_linear(ac6demo::kReachedResolveLinearBytes);
  ac6demo::untile_reached_rgba8(black_tiled, black_linear);
  assert(ac6demo::Sha256::bytes(black_linear) ==
         "0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f");

  bool trapped = false;
  try {
    static_cast<void>(ac6demo::reached_rgba8_tiled_offset(1280U, 0U));
  } catch (const ac6demo::RuntimeTrap &) {
    trapped = true;
  }
  assert(trapped);
  std::cout << "ac6-demo-xenos-tiling-tests: ok\n";
}
