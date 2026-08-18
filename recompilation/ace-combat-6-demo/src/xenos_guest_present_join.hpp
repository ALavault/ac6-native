#pragma once

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include "ac6demo/hash.hpp"
#include "ac6demo/runtime_error.hpp"
#include "ac6demo/session.hpp"
#include "ac6demo/vulkan_neutral_resolve.hpp"
#include "ac6demo/xenos_tiling.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <span>

namespace ac6demo {

// Commit only the pixels written by the qualified 1280x720 resolve into the
// exact guest allocation named by XE_SWAP.  Padding is preserved from guest
// memory, rather than copied from the Vulkan canary buffer.
inline void commit_reached_guest_present(
    DemoSession &session, VulkanNeutralResolveResult &resolve) {
  constexpr std::uint32_t kAddress = 0x1374A000U;
  if (!resolve.present_joined || resolve.destination_address != kAddress ||
      resolve.width != kReachedResolveWidth ||
      resolve.height != kReachedResolveHeight ||
      resolve.destination_bytes != kReachedResolveTiledExtentBytes ||
      resolve.tiled_bytes.size() != kReachedResolveTiledExtentBytes) {
    throw RuntimeTrap("unqualified guest present writeback");
  }
  auto guest = session.load_guest_bytes(
      resolve.destination_address, resolve.destination_bytes);
  for (std::uint32_t y = 0U; y < kReachedResolveHeight; ++y) {
    for (std::uint32_t x = 0U; x < kReachedResolveWidth; ++x) {
      const auto offset = reached_rgba8_tiled_offset(x, y);
      std::copy_n(resolve.tiled_bytes.begin() +
                      static_cast<std::ptrdiff_t>(offset),
                  4U, guest.begin() + static_cast<std::ptrdiff_t>(offset));
    }
  }
  session.store_guest_bytes(resolve.destination_address, guest);
  const auto reread = session.load_guest_bytes(
      resolve.destination_address, resolve.destination_bytes);
  std::vector<std::byte> guest_linear(kReachedResolveLinearBytes);
  untile_reached_rgba8(reread, guest_linear);
  if (Sha256::bytes(std::span<const std::byte>(guest_linear)) !=
      resolve.linear_rgba8_sha256) {
    throw RuntimeTrap("guest present readback differs from resolved pixels");
  }
  resolve.guest_tiled_rgba8_sha256 = Sha256::bytes(reread);
  resolve.guest_linear_rgba8_sha256 = Sha256::bytes(guest_linear);
  // Ad hoc visual-review dump, opt-in only -- this campaign's readback is
  // otherwise reported only as a SHA256, never a saved image. Writes a
  // plain PPM (P6, RGB, alpha dropped) so pnmtopng can convert it the same
  // way every other committed capture in this repo already is.
  if (const char *ppm_path = std::getenv("AC6_DEMO_DUMP_READBACK_PPM");
      ppm_path != nullptr) {
    if (FILE *file = std::fopen(ppm_path, "wb"); file != nullptr) {
      std::fprintf(file, "P6\n%u %u\n255\n", kReachedResolveWidth,
                   kReachedResolveHeight);
      for (std::size_t pixel = 0; pixel < guest_linear.size(); pixel += 4U) {
        std::fwrite(&guest_linear[pixel], 1, 3, file);
      }
      std::fclose(file);
    }
  }
  resolve.guest_writeback = true;
  resolve.tiled_bytes.clear();
  resolve.tiled_bytes.shrink_to_fit();
}

} // namespace ac6demo

#endif
