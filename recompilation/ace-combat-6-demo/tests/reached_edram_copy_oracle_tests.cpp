#ifdef NDEBUG
#error "This test uses assert(); compile with -UNDEBUG."
#endif

#include "ac6demo/hash.hpp"
#include "ac6demo/reached_edram_copy_oracle.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::array<std::byte, 4> pixel(std::span<const std::byte> bytes,
                               std::uint32_t width, std::uint32_t x,
                               std::uint32_t y) {
  const std::size_t offset =
      (static_cast<std::size_t>(y) * width + x) * 4U;
  return {bytes[offset], bytes[offset + 1U], bytes[offset + 2U],
          bytes[offset + 3U]};
}

} // namespace

int main() {
  static_assert(ac6demo::kReachedEdramSurfaceBytes == 0x384000U);
  static_assert(ac6demo::kReachedEdramAllocationBytes == 0xA00000U);
  static_assert(ac6demo::kReachedResolveLinearBytes == 0x384000U);
  static_assert(ac6demo::kReachedResolveTiledExtentBytes == 0x398000U);

  assert(ac6demo::reached_edram_sample_offset(0U, 0U) == 0U);
  assert(ac6demo::reached_edram_sample_offset(79U, 15U) == 0x13FCU);
  assert(ac6demo::reached_edram_sample_offset(80U, 0U) == 0x1400U);
  assert(ac6demo::reached_edram_sample_offset(1279U, 719U) == 0x383FFCU);

  bool trapped = false;
  try {
    static_cast<void>(
        ac6demo::reached_edram_sample_offset(1280U, 0U));
  } catch (const ac6demo::RuntimeTrap &) {
    trapped = true;
  }
  assert(trapped);

  std::vector<std::byte> uniform(ac6demo::kReachedNormalLinearBytes);
  for (std::size_t offset = 0U; offset < uniform.size(); offset += 4U) {
    uniform[offset + 0U] = std::byte{0x11};
    uniform[offset + 1U] = std::byte{0x22};
    uniform[offset + 2U] = std::byte{0x33};
    uniform[offset + 3U] = std::byte{0x44};
  }

  std::vector<std::byte> edram(ac6demo::kReachedEdramAllocationBytes,
                               std::byte{0xCC});
  ac6demo::materialize_reached_normal_rgba8_edram(uniform, edram);
  for (const auto &[x, y] :
       {std::pair{0U, 0U}, std::pair{1U, 0U}, std::pair{0U, 1U},
        std::pair{1279U, 719U}}) {
    const std::size_t offset = ac6demo::reached_edram_sample_offset(x, y);
    assert(edram[offset + 0U] == std::byte{0x11});
    assert(edram[offset + 1U] == std::byte{0x22});
    assert(edram[offset + 2U] == std::byte{0x33});
    assert(edram[offset + 3U] == std::byte{0x44});
  }
  assert(std::all_of(
      edram.begin() +
          static_cast<std::ptrdiff_t>(ac6demo::kReachedEdramSurfaceBytes),
      edram.end(), [](std::byte value) {
        return value == ac6demo::kReachedEdramCanary;
      }));

  std::vector<std::byte> linear(ac6demo::kReachedResolveLinearBytes);
  ac6demo::build_reached_copy_linear_oracle(uniform, linear);
  assert(pixel(linear, ac6demo::kReachedResolveWidth, 0U, 0U) ==
         (std::array<std::byte, 4>{std::byte{0x33}, std::byte{0x22},
                                  std::byte{0x11}, std::byte{0x44}}));
  assert(pixel(linear, ac6demo::kReachedResolveWidth, 1279U, 719U) ==
         (std::array<std::byte, 4>{std::byte{0x33}, std::byte{0x22},
                                  std::byte{0x11}, std::byte{0x44}}));
  assert(ac6demo::Sha256::bytes(linear) ==
         "66dde082635ccc6b24abba5b372ceb10173bc2b062faa2d93de7c4548bb60dc8");

  std::vector<std::byte> tiled(ac6demo::kReachedResolveTiledExtentBytes);
  ac6demo::build_reached_copy_tiled_oracle(uniform, tiled);
  assert(ac6demo::Sha256::bytes(tiled) ==
         "0bf69cf42fd6c3ac73b30c438a4db6d1664eaafa9c716b9ba330a9886c976786");
  assert(ac6demo::reached_copy_tiled_matches_oracle(uniform, tiled));

  std::vector<std::byte> untiled(ac6demo::kReachedResolveLinearBytes);
  ac6demo::untile_reached_rgba8(tiled, untiled);
  assert(untiled == linear);

  std::vector<std::byte> pattern(ac6demo::kReachedNormalLinearBytes);
  for (std::uint32_t y = 0U; y < ac6demo::kReachedNormalHeight; ++y) {
    for (std::uint32_t x = 0U; x < ac6demo::kReachedNormalWidth; ++x) {
      const std::size_t offset =
          (static_cast<std::size_t>(y) * ac6demo::kReachedNormalWidth + x) * 4U;
      pattern[offset + 0U] = static_cast<std::byte>(x);
      pattern[offset + 1U] = static_cast<std::byte>(y);
      pattern[offset + 2U] = static_cast<std::byte>(x ^ y);
      pattern[offset + 3U] = static_cast<std::byte>(0x80U | (x & 0x7FU));
    }
  }
  ac6demo::build_reached_copy_linear_oracle(pattern, linear);
  for (const auto &[x, y] :
       {std::pair{0U, 0U}, std::pair{1U, 1U}, std::pair{638U, 358U},
        std::pair{1279U, 719U}}) {
    const auto source = pixel(pattern, ac6demo::kReachedNormalWidth,
                              x >> 1U, y >> 1U);
    const auto destination =
        pixel(linear, ac6demo::kReachedResolveWidth, x, y);
    assert(destination[0] == source[2]);
    assert(destination[1] == source[1]);
    assert(destination[2] == source[0]);
    assert(destination[3] == source[3]);
  }

  ac6demo::build_reached_copy_tiled_oracle(pattern, tiled);
  assert(ac6demo::reached_copy_tiled_matches_oracle(pattern, tiled));
  tiled[ac6demo::reached_rgba8_tiled_offset(321U, 123U)] ^= std::byte{1};
  assert(!ac6demo::reached_copy_tiled_matches_oracle(pattern, tiled));

  const auto edram_before = edram;
  trapped = false;
  try {
    ac6demo::materialize_reached_normal_rgba8_edram(
        std::span<const std::byte>(uniform).first(uniform.size() - 4U), edram);
  } catch (const ac6demo::RuntimeTrap &) {
    trapped = true;
  }
  assert(trapped);
  assert(edram == edram_before);

  std::cout << "reached_edram_copy_oracle_tests: ok\n";
  return 0;
}
