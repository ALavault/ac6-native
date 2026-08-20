#pragma once

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include "ac6demo/hash.hpp"
#include "ac6demo/renderer_canonical_tiling.hpp"
#include "ac6demo/runtime_error.hpp"
#include "ac6demo/session.hpp"
#include "ac6demo/vulkan_neutral_resolve.hpp"
#include "ac6demo/xenos_tiling.hpp"
#include "renderer_audit_screencap.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace ac6demo {

// Commit only the pixels written by the qualified 1280x720 resolve into the
// exact guest allocation named by XE_SWAP. The GPU-produced tiled bytes are
// first canonicalized through the independently tested CPU untile/tile pair.
// Padding is preserved from guest memory rather than copied from the Vulkan
// canary buffer.
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
  const auto resolved_linear = canonicalize_reached_tiled_writeback(
      resolve.tiled_bytes, resolve.linear_rgba8_sha256, guest);
  session.store_guest_bytes(resolve.destination_address, guest);

  const auto reread = session.load_guest_bytes(
      resolve.destination_address, resolve.destination_bytes);
  std::vector<std::byte> guest_linear(kReachedResolveLinearBytes);
  untile_reached_rgba8(reread, guest_linear);
  if (Sha256::bytes(std::span<const std::byte>(guest_linear)) !=
      resolve.linear_rgba8_sha256 || guest_linear != resolved_linear) {
    throw RuntimeTrap("guest present readback differs from resolved pixels");
  }
  resolve.guest_tiled_rgba8_sha256 = Sha256::bytes(reread);
  resolve.guest_linear_rgba8_sha256 = Sha256::bytes(guest_linear);
  resolve.guest_writeback = true;
  publish_renderer_audit_screencap(session, guest_linear, resolve);
  resolve.tiled_bytes.clear();
  resolve.tiled_bytes.shrink_to_fit();
}

} // namespace ac6demo

#endif
