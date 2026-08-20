#pragma once

#include "ac6demo/hash.hpp"
#include "ac6demo/reached_edram_copy_oracle.hpp"
#include "ac6demo/runtime_error.hpp"
#include "ac6demo/xenos_tiling.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ac6demo {

enum class ReachedCopyFailureStage : std::uint8_t {
  Exact,
  EdramMaterialization,
  CopyPixels,
  DestinationPadding,
};

[[nodiscard]] inline std::string_view
reached_copy_failure_stage_name(ReachedCopyFailureStage stage) noexcept {
  switch (stage) {
  case ReachedCopyFailureStage::Exact:
    return "exact";
  case ReachedCopyFailureStage::EdramMaterialization:
    return "edram_materialization";
  case ReachedCopyFailureStage::CopyPixels:
    return "copy_pixels";
  case ReachedCopyFailureStage::DestinationPadding:
    return "destination_padding";
  }
  return "unknown";
}

struct ReachedCopyByteDifference final {
  bool present{};
  std::size_t offset{};
  std::uint8_t expected{};
  std::uint8_t observed{};
};

struct ReachedCopyPixelDifference final {
  bool present{};
  std::uint32_t x{};
  std::uint32_t y{};
  std::uint8_t channel{};
  std::uint8_t expected{};
  std::uint8_t observed{};
};

struct ReachedCopyDifferential final {
  ReachedCopyFailureStage first_failed_stage{ReachedCopyFailureStage::Exact};
  bool edram_provided{};
  bool edram_exact{true};
  bool linear_pixels_exact{true};
  bool destination_padding_exact{true};
  bool tiled_exact{true};

  std::uint64_t edram_mismatched_bytes{};
  std::uint64_t pixel_mismatched_bytes{};
  std::uint64_t pixel_mismatched_pixels{};
  std::uint64_t padding_mismatched_bytes{};

  ReachedCopyByteDifference first_edram_difference;
  ReachedCopyPixelDifference first_pixel_difference;
  ReachedCopyByteDifference first_padding_difference;

  std::string normal_rgba8_sha256;
  std::string expected_edram_sha256;
  std::string observed_edram_sha256;
  std::string expected_linear_sha256;
  std::string observed_linear_sha256;
  std::string expected_tiled_sha256;
  std::string observed_tiled_sha256;

  [[nodiscard]] bool exact() const noexcept {
    return first_failed_stage == ReachedCopyFailureStage::Exact && edram_exact &&
           linear_pixels_exact && destination_padding_exact && tiled_exact;
  }
};

namespace detail {

[[nodiscard]] inline std::uint8_t byte_value(std::byte value) noexcept {
  return std::to_integer<std::uint8_t>(value);
}

inline void record_first_byte_difference(ReachedCopyByteDifference &first,
                                         std::size_t offset,
                                         std::byte expected,
                                         std::byte observed) noexcept {
  if (first.present) {
    return;
  }
  first = {true, offset, byte_value(expected), byte_value(observed)};
}

} // namespace detail

// Produces a stage-local differential certificate for the exact PAL demo copy
// profile. `observed_edram` may be empty when the runtime only exposes the
// normal-draw readback and the final tiled resolve. All validation is performed
// before caller-owned memory could be modified; this function allocates and
// mutates only its own oracle buffers.
[[nodiscard]] inline ReachedCopyDifferential diagnose_reached_copy(
    std::span<const std::byte> normal_rgba8,
    std::span<const std::byte> observed_tiled,
    std::span<const std::byte> observed_edram = {},
    std::byte edram_canary = kReachedEdramCanary,
    std::byte padding_canary = kReachedCopyPaddingCanary) {
  if (normal_rgba8.size() != kReachedNormalLinearBytes) {
    throw RuntimeTrap("reached copy differential normal extent is invalid");
  }
  if (observed_tiled.size() != kReachedResolveTiledExtentBytes) {
    throw RuntimeTrap("reached copy differential tiled extent is invalid");
  }
  if (!observed_edram.empty() &&
      observed_edram.size() != kReachedEdramAllocationBytes) {
    throw RuntimeTrap("reached copy differential EDRAM extent is invalid");
  }

  ReachedCopyDifferential result;
  result.normal_rgba8_sha256 = Sha256::bytes(normal_rgba8);

  std::vector<std::byte> expected_linear(kReachedResolveLinearBytes);
  build_reached_copy_linear_oracle(normal_rgba8, expected_linear);
  result.expected_linear_sha256 = Sha256::bytes(expected_linear);

  std::vector<std::byte> observed_linear(kReachedResolveLinearBytes);
  untile_reached_rgba8(observed_tiled, observed_linear);
  result.observed_linear_sha256 = Sha256::bytes(observed_linear);

  for (std::size_t pixel = 0U;
       pixel < static_cast<std::size_t>(kReachedResolveWidth) *
                   kReachedResolveHeight;
       ++pixel) {
    bool pixel_differs = false;
    for (std::size_t channel = 0U; channel < 4U; ++channel) {
      const std::size_t offset = pixel * 4U + channel;
      if (expected_linear[offset] == observed_linear[offset]) {
        continue;
      }
      pixel_differs = true;
      ++result.pixel_mismatched_bytes;
      if (!result.first_pixel_difference.present) {
        result.first_pixel_difference = {
            true,
            static_cast<std::uint32_t>(pixel % kReachedResolveWidth),
            static_cast<std::uint32_t>(pixel / kReachedResolveWidth),
            static_cast<std::uint8_t>(channel),
            detail::byte_value(expected_linear[offset]),
            detail::byte_value(observed_linear[offset]),
        };
      }
    }
    result.pixel_mismatched_pixels += pixel_differs ? 1U : 0U;
  }
  result.linear_pixels_exact = result.pixel_mismatched_bytes == 0U;

  std::vector<std::byte> expected_tiled(kReachedResolveTiledExtentBytes);
  build_reached_copy_tiled_oracle(normal_rgba8, expected_tiled,
                                  padding_canary);
  result.expected_tiled_sha256 = Sha256::bytes(expected_tiled);
  result.observed_tiled_sha256 = Sha256::bytes(observed_tiled);
  result.tiled_exact = std::equal(expected_tiled.begin(), expected_tiled.end(),
                                  observed_tiled.begin());

  std::vector<bool> pixel_addressed(kReachedResolveTiledExtentBytes, false);
  for (std::uint32_t y = 0U; y < kReachedResolveHeight; ++y) {
    for (std::uint32_t x = 0U; x < kReachedResolveWidth; ++x) {
      const std::size_t offset = reached_rgba8_tiled_offset(x, y);
      for (std::size_t channel = 0U; channel < 4U; ++channel) {
        pixel_addressed[offset + channel] = true;
      }
    }
  }
  for (std::size_t offset = 0U; offset < observed_tiled.size(); ++offset) {
    if (pixel_addressed[offset] ||
        observed_tiled[offset] == expected_tiled[offset]) {
      continue;
    }
    ++result.padding_mismatched_bytes;
    detail::record_first_byte_difference(result.first_padding_difference,
                                         offset, expected_tiled[offset],
                                         observed_tiled[offset]);
  }
  result.destination_padding_exact = result.padding_mismatched_bytes == 0U;

  result.edram_provided = !observed_edram.empty();
  if (result.edram_provided) {
    std::vector<std::byte> expected_edram(kReachedEdramAllocationBytes);
    materialize_reached_normal_rgba8_edram(normal_rgba8, expected_edram,
                                            edram_canary);
    result.expected_edram_sha256 = Sha256::bytes(expected_edram);
    result.observed_edram_sha256 = Sha256::bytes(observed_edram);
    for (std::size_t offset = 0U; offset < expected_edram.size(); ++offset) {
      if (expected_edram[offset] == observed_edram[offset]) {
        continue;
      }
      ++result.edram_mismatched_bytes;
      detail::record_first_byte_difference(result.first_edram_difference,
                                           offset, expected_edram[offset],
                                           observed_edram[offset]);
    }
    result.edram_exact = result.edram_mismatched_bytes == 0U;
  }

  if (result.edram_provided && !result.edram_exact) {
    result.first_failed_stage =
        ReachedCopyFailureStage::EdramMaterialization;
  } else if (!result.linear_pixels_exact) {
    result.first_failed_stage = ReachedCopyFailureStage::CopyPixels;
  } else if (!result.destination_padding_exact) {
    result.first_failed_stage =
        ReachedCopyFailureStage::DestinationPadding;
  }
  return result;
}

inline void require_exact_reached_copy(const ReachedCopyDifferential &result) {
  if (!result.exact()) {
    throw RuntimeTrap("reached copy differential failed at stage " +
                      std::string(
                          reached_copy_failure_stage_name(result.first_failed_stage)));
  }
}

} // namespace ac6demo
