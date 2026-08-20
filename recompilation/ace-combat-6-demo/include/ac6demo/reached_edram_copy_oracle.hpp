#pragma once

#include "ac6demo/runtime_error.hpp"
#include "ac6demo/xenos_tiling.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ac6demo {

inline constexpr std::uint32_t kReachedNormalWidth = 640U;
inline constexpr std::uint32_t kReachedNormalHeight = 360U;
inline constexpr std::size_t kReachedNormalLinearBytes =
    static_cast<std::size_t>(kReachedNormalWidth) * kReachedNormalHeight * 4U;

inline constexpr std::uint32_t kReachedEdramSampleWidth = 1280U;
inline constexpr std::uint32_t kReachedEdramSampleHeight = 720U;
inline constexpr std::uint32_t kReachedEdramTileWidthSamples = 80U;
inline constexpr std::uint32_t kReachedEdramTileHeightSamples = 16U;
inline constexpr std::uint32_t kReachedEdramPitchTiles = 16U;
inline constexpr std::size_t kReachedEdramBytesPerSample = 4U;
inline constexpr std::size_t kReachedEdramTileBytes =
    static_cast<std::size_t>(kReachedEdramTileWidthSamples) *
    kReachedEdramTileHeightSamples * kReachedEdramBytesPerSample;
inline constexpr std::size_t kReachedEdramSurfaceBytes =
    static_cast<std::size_t>(kReachedEdramPitchTiles) *
    ((kReachedEdramSampleHeight + kReachedEdramTileHeightSamples - 1U) /
     kReachedEdramTileHeightSamples) *
    kReachedEdramTileBytes;
inline constexpr std::size_t kReachedEdramAllocationBytes = 0xA00000U;

inline constexpr std::byte kReachedEdramCanary{0x5A};
inline constexpr std::byte kReachedCopyPaddingCanary{0xA5};

[[nodiscard]] inline std::size_t
reached_edram_sample_offset(std::uint32_t sample_x,
                            std::uint32_t sample_y) {
  if (sample_x >= kReachedEdramSampleWidth ||
      sample_y >= kReachedEdramSampleHeight) {
    throw RuntimeTrap("reached EDRAM sample coordinate is out of bounds");
  }
  const std::uint32_t tile_x = sample_x / kReachedEdramTileWidthSamples;
  const std::uint32_t tile_y = sample_y / kReachedEdramTileHeightSamples;
  const std::uint32_t in_tile_x = sample_x % kReachedEdramTileWidthSamples;
  const std::uint32_t in_tile_y = sample_y % kReachedEdramTileHeightSamples;
  const std::size_t offset =
      (static_cast<std::size_t>(tile_y) * kReachedEdramPitchTiles + tile_x) *
          kReachedEdramTileBytes +
      (static_cast<std::size_t>(in_tile_y) * kReachedEdramTileWidthSamples +
       in_tile_x) *
          kReachedEdramBytesPerSample;
  if (offset + kReachedEdramBytesPerSample > kReachedEdramSurfaceBytes) {
    throw RuntimeTrap("reached EDRAM sample exceeds the qualified surface");
  }
  return offset;
}

// Materialize the reached 640x360 resolved RGBA8 image into the 1280x720
// sample-addressed EDRAM surface expected by the pinned 1x/2x-MSAA copy kernel.
// Every host pixel is replicated to its four 2x2 sample locations. Bytes beyond
// the qualified surface remain a caller-visible canary, so over-reads stay
// observable rather than being rewarded with plausible zeroes.
inline void materialize_reached_normal_rgba8_edram(
    std::span<const std::byte> normal_rgba8, std::span<std::byte> edram,
    std::byte canary = kReachedEdramCanary) {
  if (normal_rgba8.size() != kReachedNormalLinearBytes) {
    throw RuntimeTrap("reached normal RGBA8 extent is invalid");
  }
  if (edram.size() != kReachedEdramAllocationBytes) {
    throw RuntimeTrap("reached EDRAM allocation extent is invalid");
  }

  std::fill(edram.begin(), edram.end(), canary);
  for (std::uint32_t y = 0U; y < kReachedNormalHeight; ++y) {
    for (std::uint32_t x = 0U; x < kReachedNormalWidth; ++x) {
      const std::size_t source =
          (static_cast<std::size_t>(y) * kReachedNormalWidth + x) * 4U;
      for (std::uint32_t sample_y = 0U; sample_y < 2U; ++sample_y) {
        for (std::uint32_t sample_x = 0U; sample_x < 2U; ++sample_x) {
          const std::size_t destination = reached_edram_sample_offset(
              (x << 1U) | sample_x, (y << 1U) | sample_y);
          std::copy_n(normal_rgba8.begin() +
                          static_cast<std::ptrdiff_t>(source),
                      4U,
                      edram.begin() + static_cast<std::ptrdiff_t>(destination));
        }
      }
    }
  }
}

// CPU oracle for the exact reached copy profile:
// - 640x360 source resolved to four identical sample values;
// - 1280x720 destination;
// - raw destination format 6;
// - copy_dest_swap = 1, qualified by the asymmetric Vulkan harness as R/B swap.
inline void build_reached_copy_linear_oracle(
    std::span<const std::byte> normal_rgba8,
    std::span<std::byte> destination_rgba8) {
  if (normal_rgba8.size() != kReachedNormalLinearBytes) {
    throw RuntimeTrap("reached copy source extent is invalid");
  }
  if (destination_rgba8.size() != kReachedResolveLinearBytes) {
    throw RuntimeTrap("reached copy destination extent is invalid");
  }

  for (std::uint32_t y = 0U; y < kReachedResolveHeight; ++y) {
    for (std::uint32_t x = 0U; x < kReachedResolveWidth; ++x) {
      const std::size_t source =
          (static_cast<std::size_t>(y >> 1U) * kReachedNormalWidth +
           (x >> 1U)) *
          4U;
      const std::size_t destination =
          (static_cast<std::size_t>(y) * kReachedResolveWidth + x) * 4U;
      destination_rgba8[destination + 0U] = normal_rgba8[source + 2U];
      destination_rgba8[destination + 1U] = normal_rgba8[source + 1U];
      destination_rgba8[destination + 2U] = normal_rgba8[source + 0U];
      destination_rgba8[destination + 3U] = normal_rgba8[source + 3U];
    }
  }
}

inline void build_reached_copy_tiled_oracle(
    std::span<const std::byte> normal_rgba8,
    std::span<std::byte> destination_tiled,
    std::byte padding = kReachedCopyPaddingCanary) {
  if (destination_tiled.size() != kReachedResolveTiledExtentBytes) {
    throw RuntimeTrap("reached tiled copy destination extent is invalid");
  }
  std::vector<std::byte> linear(kReachedResolveLinearBytes);
  build_reached_copy_linear_oracle(normal_rgba8, linear);
  std::fill(destination_tiled.begin(), destination_tiled.end(), padding);
  tile_reached_rgba8(linear, destination_tiled);
}

[[nodiscard]] inline bool reached_copy_tiled_matches_oracle(
    std::span<const std::byte> normal_rgba8,
    std::span<const std::byte> observed_tiled,
    std::byte padding = kReachedCopyPaddingCanary) {
  if (observed_tiled.size() != kReachedResolveTiledExtentBytes) {
    throw RuntimeTrap("observed reached tiled copy extent is invalid");
  }
  std::vector<std::byte> expected(kReachedResolveTiledExtentBytes);
  build_reached_copy_tiled_oracle(normal_rgba8, expected, padding);
  return std::equal(expected.begin(), expected.end(), observed_tiled.begin());
}

} // namespace ac6demo
