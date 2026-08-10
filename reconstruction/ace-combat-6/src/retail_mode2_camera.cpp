#include "ac6/retail_mode2_camera.h"

#include <algorithm>
#include <cmath>

namespace ac6::retail {
namespace {

bool finite_basis(const RetailBasis &basis) noexcept {
  return std::all_of(
      basis.rows.begin(), basis.rows.end(), [](const BasisRow &row) noexcept {
        return std::all_of(row.begin(), row.end(),
                           [](float value) { return std::isfinite(value); });
      });
}

bool finite_state(const RetailMode2CameraState &state) noexcept {
  return finite_basis(state.player_basis) &&
         std::all_of(state.player_position.begin(), state.player_position.end(),
                     [](float value) { return std::isfinite(value); }) &&
         std::isfinite(state.rotation_at_3a0) &&
         std::isfinite(state.rotation_at_3a4);
}

float transform_component(const RetailBasis &basis,
                          const std::array<float, 4> &offset,
                          std::size_t lane) noexcept {
  // The three vmsum3fp128 at 0x82260B00/+04/+08 consume the transposed player
  // rows and only xyz. This spells out that observed grouping; it does not
  // claim bit identity with Xenon's vector floating-point rounding.
  const float first = basis.rows[0][lane] * offset[0];
  const float second = basis.rows[1][lane] * offset[1];
  const float pair = first + second;
  return pair + basis.rows[2][lane] * offset[2];
}

} // namespace

std::optional<RetailMode2CameraLocator> transform_mode2_camera_locator(
    const RetailMode2CameraState &state,
    const std::array<float, 4> &local_offset) noexcept {
  if (!finite_state(state) ||
      !std::all_of(local_offset.begin(), local_offset.end(),
                   [](float value) { return std::isfinite(value); })) {
    return std::nullopt;
  }

  RetailMode2CameraLocator locator;
  locator.basis = state.player_basis;
  for (std::size_t lane = 0; lane < locator.position.size(); ++lane) {
    locator.position[lane] =
        state.player_position[lane] +
        transform_component(state.player_basis, local_offset, lane);
    if (!std::isfinite(locator.position[lane]))
      return std::nullopt;
  }

  // 0x82260B70 loads manager+0x3A4 first; 0x82260B7C then loads +0x3A0.
  rotate_820A9B30(locator.basis, state.rotation_at_3a4);
  rotate_820A99F8(locator.basis, state.rotation_at_3a0);
  return finite_basis(locator.basis) ? std::optional(locator) : std::nullopt;
}

std::optional<RetailMode2CameraLocator> resolve_mode2_base_camera_locator(
    const RetailCameraRecord &record,
    const RetailMode2CameraState &state) noexcept {
  const std::optional<std::array<float, 4>> offset = record.offset(0);
  return offset.has_value() ? transform_mode2_camera_locator(state, *offset)
                            : std::nullopt;
}

} // namespace ac6::retail
