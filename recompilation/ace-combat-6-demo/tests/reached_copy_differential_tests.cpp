#ifdef NDEBUG
#error "This assert-based test must be built with -UNDEBUG."
#endif

#include "ac6demo/reached_copy_differential.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::byte> spatial_normal() {
  std::vector<std::byte> result(ac6demo::kReachedNormalLinearBytes);
  for (std::uint32_t y = 0U; y < ac6demo::kReachedNormalHeight; ++y) {
    for (std::uint32_t x = 0U; x < ac6demo::kReachedNormalWidth; ++x) {
      const std::size_t offset =
          (static_cast<std::size_t>(y) * ac6demo::kReachedNormalWidth + x) * 4U;
      result[offset + 0U] = static_cast<std::byte>(x ^ (y * 3U));
      result[offset + 1U] = static_cast<std::byte>((x * 5U) + y);
      result[offset + 2U] = static_cast<std::byte>((x >> 2U) ^ (y >> 1U));
      result[offset + 3U] = static_cast<std::byte>(0x80U | ((x + y) & 0x7FU));
    }
  }
  return result;
}

std::size_t first_padding_offset() {
  std::vector<bool> used(ac6demo::kReachedResolveTiledExtentBytes, false);
  for (std::uint32_t y = 0U; y < ac6demo::kReachedResolveHeight; ++y) {
    for (std::uint32_t x = 0U; x < ac6demo::kReachedResolveWidth; ++x) {
      const std::size_t offset = ac6demo::reached_rgba8_tiled_offset(x, y);
      for (std::size_t channel = 0U; channel < 4U; ++channel) {
        used[offset + channel] = true;
      }
    }
  }
  const auto found = std::find(used.begin(), used.end(), false);
  assert(found != used.end());
  return static_cast<std::size_t>(found - used.begin());
}

} // namespace

int main() {
  const auto normal = spatial_normal();
  std::vector<std::byte> edram(ac6demo::kReachedEdramAllocationBytes);
  ac6demo::materialize_reached_normal_rgba8_edram(normal, edram);
  std::vector<std::byte> tiled(ac6demo::kReachedResolveTiledExtentBytes);
  ac6demo::build_reached_copy_tiled_oracle(normal, tiled);

  const auto exact = ac6demo::diagnose_reached_copy(normal, tiled, edram);
  assert(exact.exact());
  assert(exact.edram_provided);
  assert(exact.edram_exact);
  assert(exact.linear_pixels_exact);
  assert(exact.destination_padding_exact);
  assert(exact.tiled_exact);
  assert(exact.first_failed_stage == ac6demo::ReachedCopyFailureStage::Exact);
  ac6demo::require_exact_reached_copy(exact);

  auto bad_edram = edram;
  const std::size_t edram_offset = ac6demo::reached_edram_sample_offset(42U, 17U) + 2U;
  bad_edram[edram_offset] ^= std::byte{0x01};
  const auto edram_failure =
      ac6demo::diagnose_reached_copy(normal, tiled, bad_edram);
  assert(!edram_failure.exact());
  assert(!edram_failure.edram_exact);
  assert(edram_failure.linear_pixels_exact);
  assert(edram_failure.destination_padding_exact);
  assert(edram_failure.edram_mismatched_bytes == 1U);
  assert(edram_failure.first_edram_difference.present);
  assert(edram_failure.first_edram_difference.offset == edram_offset);
  assert(edram_failure.first_failed_stage ==
         ac6demo::ReachedCopyFailureStage::EdramMaterialization);

  auto bad_pixel = tiled;
  constexpr std::uint32_t kBadX = 1111U;
  constexpr std::uint32_t kBadY = 333U;
  constexpr std::uint8_t kBadChannel = 2U;
  const std::size_t tiled_pixel_offset =
      ac6demo::reached_rgba8_tiled_offset(kBadX, kBadY) + kBadChannel;
  bad_pixel[tiled_pixel_offset] ^= std::byte{0x40};
  const auto pixel_failure = ac6demo::diagnose_reached_copy(normal, bad_pixel);
  assert(!pixel_failure.exact());
  assert(pixel_failure.edram_exact);
  assert(!pixel_failure.linear_pixels_exact);
  assert(pixel_failure.destination_padding_exact);
  assert(pixel_failure.pixel_mismatched_bytes == 1U);
  assert(pixel_failure.pixel_mismatched_pixels == 1U);
  assert(pixel_failure.first_pixel_difference.present);
  assert(pixel_failure.first_pixel_difference.x == kBadX);
  assert(pixel_failure.first_pixel_difference.y == kBadY);
  assert(pixel_failure.first_pixel_difference.channel == kBadChannel);
  assert(pixel_failure.first_failed_stage ==
         ac6demo::ReachedCopyFailureStage::CopyPixels);

  const std::size_t padding_offset = first_padding_offset();
  auto bad_padding = tiled;
  bad_padding[padding_offset] ^= std::byte{0x20};
  const auto padding_failure =
      ac6demo::diagnose_reached_copy(normal, bad_padding);
  assert(!padding_failure.exact());
  assert(padding_failure.linear_pixels_exact);
  assert(!padding_failure.destination_padding_exact);
  assert(padding_failure.padding_mismatched_bytes == 1U);
  assert(padding_failure.first_padding_difference.present);
  assert(padding_failure.first_padding_difference.offset == padding_offset);
  assert(padding_failure.first_failed_stage ==
         ac6demo::ReachedCopyFailureStage::DestinationPadding);

  auto both = bad_pixel;
  both[padding_offset] ^= std::byte{0x08};
  const auto both_failure = ac6demo::diagnose_reached_copy(normal, both);
  assert(!both_failure.linear_pixels_exact);
  assert(!both_failure.destination_padding_exact);
  assert(both_failure.first_failed_stage ==
         ac6demo::ReachedCopyFailureStage::CopyPixels);

  bool rejected = false;
  try {
    ac6demo::require_exact_reached_copy(pixel_failure);
  } catch (const ac6demo::RuntimeTrap &) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    std::vector<std::byte> short_normal(
        ac6demo::kReachedNormalLinearBytes - 1U);
    static_cast<void>(ac6demo::diagnose_reached_copy(short_normal, tiled));
  } catch (const ac6demo::RuntimeTrap &) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    std::vector<std::byte> short_tiled(
        ac6demo::kReachedResolveTiledExtentBytes - 1U);
    static_cast<void>(ac6demo::diagnose_reached_copy(normal, short_tiled));
  } catch (const ac6demo::RuntimeTrap &) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    std::vector<std::byte> short_edram(
        ac6demo::kReachedEdramAllocationBytes - 4U);
    static_cast<void>(ac6demo::diagnose_reached_copy(normal, tiled, short_edram));
  } catch (const ac6demo::RuntimeTrap &) {
    rejected = true;
  }
  assert(rejected);

  std::cout << "reached-copy-differential-tests: ok\n";
}
