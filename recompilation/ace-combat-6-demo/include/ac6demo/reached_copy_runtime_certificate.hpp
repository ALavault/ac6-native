#pragma once

#include "ac6demo/reached_copy_differential.hpp"
#include "ac6demo/runtime_error.hpp"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <span>
#include <string>

namespace ac6demo {

// Runtime-facing summary of the stage-local copy differential. The complete
// differential remains available for audit, while the summary provides a
// stable, single-line trace contract for product runs.
struct ReachedCopyRuntimeCertificate final {
  ReachedCopyDifferential differential;

  [[nodiscard]] bool writeback_allowed() const noexcept {
    return differential.exact();
  }

  [[nodiscard]] std::string trace_line() const {
    std::ostringstream output;
    output << "AC6_COPY_DIFFERENTIAL"
           << " stage="
           << reached_copy_failure_stage_name(
                  differential.first_failed_stage)
           << " exact=" << (differential.exact() ? 1 : 0)
           << " edram_provided=" << (differential.edram_provided ? 1 : 0)
           << " edram_mismatched_bytes="
           << differential.edram_mismatched_bytes
           << " pixel_mismatched_bytes="
           << differential.pixel_mismatched_bytes
           << " pixel_mismatched_pixels="
           << differential.pixel_mismatched_pixels
           << " padding_mismatched_bytes="
           << differential.padding_mismatched_bytes
           << " normal_sha256=" << differential.normal_rgba8_sha256
           << " expected_linear_sha256="
           << differential.expected_linear_sha256
           << " observed_linear_sha256="
           << differential.observed_linear_sha256
           << " expected_tiled_sha256="
           << differential.expected_tiled_sha256
           << " observed_tiled_sha256="
           << differential.observed_tiled_sha256;
    if (differential.first_pixel_difference.present) {
      const auto &pixel = differential.first_pixel_difference;
      output << " first_pixel_x=" << pixel.x
             << " first_pixel_y=" << pixel.y
             << " first_pixel_channel="
             << static_cast<unsigned>(pixel.channel)
             << " first_pixel_expected="
             << static_cast<unsigned>(pixel.expected)
             << " first_pixel_observed="
             << static_cast<unsigned>(pixel.observed);
    }
    if (differential.first_padding_difference.present) {
      const auto &padding = differential.first_padding_difference;
      output << " first_padding_offset=" << padding.offset
             << " first_padding_expected="
             << static_cast<unsigned>(padding.expected)
             << " first_padding_observed="
             << static_cast<unsigned>(padding.observed);
    }
    return output.str();
  }
};

[[nodiscard]] inline ReachedCopyRuntimeCertificate
certify_reached_copy_runtime(
    std::span<const std::byte> normal_rgba8,
    std::span<const std::byte> observed_tiled,
    std::span<const std::byte> observed_edram = {}) {
  return {diagnose_reached_copy(normal_rgba8, observed_tiled,
                                observed_edram)};
}

inline void require_reached_copy_runtime_writeback(
    const ReachedCopyRuntimeCertificate &certificate) {
  if (!certificate.writeback_allowed()) {
    throw RuntimeTrap("reached copy runtime certificate refused writeback: " +
                      certificate.trace_line());
  }
}

} // namespace ac6demo
