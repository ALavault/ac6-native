#pragma once

#include "ac6demo/hash.hpp"
#include "ac6demo/runtime_error.hpp"
#include "ac6demo/xenos_tiling.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ac6demo {

// Canonicalizes one reached GPU tiled payload before it is allowed to modify
// the guest-owned XE_SWAP allocation. The destination is left byte-identical
// on every failure because tiling begins only after the linear digest matches.
[[nodiscard]] inline std::vector<std::byte>
canonicalize_reached_tiled_writeback(
    std::span<const std::byte> gpu_tiled,
    std::string_view expected_linear_sha256,
    std::span<std::byte> guest_tiled) {
  const auto valid_sha256 = [](std::string_view digest) noexcept {
    if (digest.size() != 64U) return false;
    for (const char character : digest) {
      const bool decimal = character >= '0' && character <= '9';
      const bool lower_hex = character >= 'a' && character <= 'f';
      if (!decimal && !lower_hex) return false;
    }
    return true;
  };
  if (!valid_sha256(expected_linear_sha256)) {
    throw RuntimeTrap("reached writeback linear digest is not SHA-256");
  }

  std::vector<std::byte> linear(kReachedResolveLinearBytes);
  untile_reached_rgba8(gpu_tiled, linear);
  const std::string observed = Sha256::bytes(linear);
  if (observed != expected_linear_sha256) {
    throw RuntimeTrap(
        "Vulkan tiled resolve differs from its qualified linear pixels");
  }

  // tile_reached_rgba8 writes only pixel-addressed words. All padding bytes
  // remain those loaded from the guest allocation by the caller.
  tile_reached_rgba8(linear, guest_tiled);
  return linear;
}

} // namespace ac6demo
