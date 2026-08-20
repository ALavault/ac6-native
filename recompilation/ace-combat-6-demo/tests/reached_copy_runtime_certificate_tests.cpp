#ifdef NDEBUG
#error "This test uses assert(); compile with -UNDEBUG."
#endif

#include "ac6demo/reached_copy_runtime_certificate.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> spatial_normal() {
  std::vector<std::byte> result(ac6demo::kReachedNormalLinearBytes);
  for (std::uint32_t y = 0U; y < ac6demo::kReachedNormalHeight; ++y) {
    for (std::uint32_t x = 0U; x < ac6demo::kReachedNormalWidth; ++x) {
      const std::size_t offset =
          (static_cast<std::size_t>(y) * ac6demo::kReachedNormalWidth + x) *
          4U;
      result[offset + 0U] = static_cast<std::byte>(x);
      result[offset + 1U] = static_cast<std::byte>(y);
      result[offset + 2U] = static_cast<std::byte>((x >> 3U) ^ (y >> 2U));
      result[offset + 3U] = std::byte{0xFF};
    }
  }
  return result;
}

} // namespace

int main() {
  const auto normal = spatial_normal();
  std::vector<std::byte> tiled(ac6demo::kReachedResolveTiledExtentBytes);
  ac6demo::build_reached_copy_tiled_oracle(normal, tiled);

  const auto exact =
      ac6demo::certify_reached_copy_runtime(normal, tiled);
  assert(exact.writeback_allowed());
  assert(exact.differential.first_failed_stage ==
         ac6demo::ReachedCopyFailureStage::Exact);
  assert(exact.trace_line().find("stage=exact") != std::string::npos);
  assert(exact.trace_line().find("exact=1") != std::string::npos);
  ac6demo::require_reached_copy_runtime_writeback(exact);

  auto pixel_corrupt = tiled;
  const std::size_t pixel_offset = ac6demo::reached_rgba8_tiled_offset(19U, 23U);
  pixel_corrupt[pixel_offset + 2U] ^= std::byte{0x01};
  const auto pixel =
      ac6demo::certify_reached_copy_runtime(normal, pixel_corrupt);
  assert(!pixel.writeback_allowed());
  assert(pixel.differential.first_failed_stage ==
         ac6demo::ReachedCopyFailureStage::CopyPixels);
  assert(pixel.differential.pixel_mismatched_bytes == 1U);
  assert(pixel.differential.pixel_mismatched_pixels == 1U);
  assert(pixel.trace_line().find("stage=copy_pixels") != std::string::npos);
  assert(pixel.trace_line().find("first_pixel_x=19") != std::string::npos);

  std::vector<bool> addressed(ac6demo::kReachedResolveTiledExtentBytes, false);
  for (std::uint32_t y = 0U; y < ac6demo::kReachedResolveHeight; ++y) {
    for (std::uint32_t x = 0U; x < ac6demo::kReachedResolveWidth; ++x) {
      const std::size_t offset = ac6demo::reached_rgba8_tiled_offset(x, y);
      for (std::size_t channel = 0U; channel < 4U; ++channel) {
        addressed[offset + channel] = true;
      }
    }
  }
  std::size_t padding_offset = 0U;
  while (padding_offset < addressed.size() && addressed[padding_offset]) {
    ++padding_offset;
  }
  assert(padding_offset < addressed.size());
  auto padding_corrupt = tiled;
  padding_corrupt[padding_offset] ^= std::byte{0x01};
  const auto padding =
      ac6demo::certify_reached_copy_runtime(normal, padding_corrupt);
  assert(!padding.writeback_allowed());
  assert(padding.differential.first_failed_stage ==
         ac6demo::ReachedCopyFailureStage::DestinationPadding);
  assert(padding.differential.padding_mismatched_bytes == 1U);
  assert(padding.trace_line().find("stage=destination_padding") !=
         std::string::npos);
  assert(padding.trace_line().find("first_padding_offset=") !=
         std::string::npos);

  bool refused = false;
  try {
    ac6demo::require_reached_copy_runtime_writeback(pixel);
  } catch (const ac6demo::RuntimeTrap &trap) {
    refused = std::string(trap.what()).find("stage=copy_pixels") !=
              std::string::npos;
  }
  assert(refused);
  return 0;
}
