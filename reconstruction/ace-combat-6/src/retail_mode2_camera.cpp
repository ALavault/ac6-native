#include "ac6/retail_mode2_camera.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

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

bool finite_vector(const std::array<float, 4> &values) noexcept {
  return std::all_of(values.begin(), values.end(),
                     [](float value) { return std::isfinite(value); });
}

bool finite_shake(const RetailMode2ShakeState &state) noexcept {
  return finite_vector(state.start) && finite_vector(state.target) &&
         finite_vector(state.output) && finite_vector(state.velocity) &&
         std::isfinite(state.elapsed);
}

bool valid_draws(const std::array<std::uint16_t, 2> &draws) noexcept {
  return std::all_of(draws.begin(), draws.end(),
                     [](std::uint16_t draw) { return draw <= 0x7FFFu; });
}

constexpr float kEpsilon =
    std::bit_cast<float>(std::uint32_t{0x37800000});
constexpr float kShakePeriod =
    std::bit_cast<float>(std::uint32_t{0x3DCCCCCD});
constexpr float kPhaseTurn =
    std::bit_cast<float>(std::uint32_t{0x3F266666});
constexpr float kPlayerScale =
    std::bit_cast<float>(std::uint32_t{0x3F6A64C3});
constexpr float kRandomScale =
    std::bit_cast<float>(std::uint32_t{0x38800100});
constexpr float kRotationSnap =
    std::bit_cast<float>(std::uint32_t{0x3A83126F});
constexpr float kPi = std::bit_cast<float>(std::uint32_t{0x40490FDB});
constexpr float kTwoPi = std::bit_cast<float>(std::uint32_t{0x40C90FDB});
constexpr float kNegativePi =
    std::bit_cast<float>(std::uint32_t{0xC0490FDB});

float retail_unit_clamp(float value) noexcept {
  // 0x8225DB04..0x8225DB20: the two comparisons are ordered and the selected
  // bound is loaded before the shared assignment.
  if (value < 0.0F)
    return 0.0F;
  if (value > 1.0F)
    return 1.0F;
  return value;
}

float retail_amplitude_clamp(float value, float amplitude) noexcept {
  const float lower = -amplitude;
  if (value < lower)
    return lower;
  if (value > amplitude)
    return amplitude;
  return value;
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

bool finite_rotation(const RetailMode2RotationState &state) noexcept {
  return std::isfinite(state.rotation_at_3a0) &&
         std::isfinite(state.rotation_at_3a4) &&
         std::isfinite(state.rotation_at_3a8);
}

float interpolate_rotation(float current, float target, float response) noexcept {
  // The retail block uses fmsubs followed by fmadds. Expressing both as fma
  // preserves the single-rounding subtraction/addition grouping on hosts
  // that provide it, while keeping the state transition explicit.
  const float delta = std::fmaf(target, 1.0F, -current);
  return std::fmaf(delta, response, current);
}

float normalise_rotation(float value, bool snap) noexcept {
  if (snap && std::fabs(value) < kRotationSnap)
    value = 0.0F;
  if (value > kPi)
    value -= kTwoPi;
  else if (value < kNegativePi)
    value += kTwoPi;
  return value;
}

} // namespace

std::optional<RetailMode2RotationState> step_mode2_camera_rotation(
    const RetailMode2RotationInput &input) noexcept {
  if (!finite_rotation(input.current) ||
      !std::isfinite(input.target_at_3a0) ||
      !std::isfinite(input.target_at_3a4) ||
      !std::isfinite(input.target_at_3a8) ||
      !std::isfinite(input.response)) {
    return std::nullopt;
  }

  RetailMode2RotationState result = input.current;
  float target_at_3a4 = input.target_at_3a4;
  if (input.wrap_at_3a4) {
    float delta = std::fmaf(target_at_3a4, 1.0F, -result.rotation_at_3a4);
    if (std::fabs(delta) > kPi) {
      if (delta < 0.0F)
        target_at_3a4 += kTwoPi;
      else
        target_at_3a4 -= kTwoPi;
    }
  }

  // 0x82262E90 stores +0x3A4 first, then +0x3A0. The final +0x3A8 update
  // follows the snap and normalisation of those fields.
  result.rotation_at_3a4 = interpolate_rotation(
      result.rotation_at_3a4, target_at_3a4, input.response);
  result.rotation_at_3a0 = interpolate_rotation(
      result.rotation_at_3a0, input.target_at_3a0, input.response);
  result.rotation_at_3a4 = normalise_rotation(result.rotation_at_3a4, true);
  result.rotation_at_3a0 = normalise_rotation(result.rotation_at_3a0, true);
  result.rotation_at_3a8 = interpolate_rotation(
      result.rotation_at_3a8, input.target_at_3a8, input.response);
  // Retail does not apply the 0x001 snap to +0x3A8; it only wraps that field.
  result.rotation_at_3a8 = normalise_rotation(result.rotation_at_3a8, false);

  return finite_rotation(result) ? std::optional(result) : std::nullopt;
}

std::optional<RetailMode2ShakeStepResult> step_mode2_camera_shake(
    const RetailMode2ShakeState &state, float period, float amplitude,
    float frame_delta,
    const std::array<std::uint16_t, 2> &random_draws) noexcept {
  if (!finite_shake(state) || !std::isfinite(period) ||
      !std::isfinite(amplitude) || !std::isfinite(frame_delta) ||
      !valid_draws(random_draws)) {
    return std::nullopt;
  }

  RetailMode2ShakeStepResult result;
  result.state = state;
  if (std::fabs(period) < kEpsilon)
    return result;

  // 0x8225D108..0x8225D140. The refresh comparison is strictly greater.
  if (result.state.elapsed > period) {
    result.state.elapsed = 0.0F;
    result.state.velocity = {};
    result.state.start = result.state.target;
    const float scale = amplitude * kRandomScale;
    result.state.target[0] =
        std::fmaf(static_cast<float>(random_draws[0]), scale, -amplitude);
    result.state.target[1] =
        std::fmaf(static_cast<float>(random_draws[1]), scale, -amplitude);
    result.state.target[2] = 0.0F;
    result.random_draws_consumed = 2;
  }

  float phase = result.state.elapsed / period;
  phase = retail_unit_clamp(phase);
  std::array<float, 4> delta{};
  for (std::size_t lane = 0; lane < delta.size(); ++lane)
    delta[lane] = result.state.target[lane] - result.state.start[lane];

  if (phase < kPhaseTurn) {
    // 0x8225D1D0..0x8225D228 updates xyz with separate multiply/add rounds.
    for (std::size_t lane = 0; lane < 3; ++lane) {
      const float increment = delta[lane] * frame_delta;
      result.state.velocity[lane] += increment;
    }
  } else {
    // 0x8225D230..0x8225D2D4 doubles, scales, then vector-subtracts all lanes.
    for (std::size_t lane = 0; lane < delta.size(); ++lane) {
      const float doubled = delta[lane] * 2.0F;
      const float scaled = doubled * frame_delta;
      result.state.velocity[lane] -= scaled;
    }
  }

  // 0x8225D2D8..0x8225D368: x/y are fused integrations and clamped; z is
  // explicitly cleared in velocity. The output vector's z/w retain the delta
  // lanes copied to the stack (normally zero under the retail state invariant).
  const float next_x = std::fmaf(
      frame_delta, result.state.velocity[0], result.state.output[0]);
  const float next_y = std::fmaf(
      result.state.velocity[1], frame_delta, result.state.output[1]);
  result.state.velocity[2] = 0.0F;
  result.state.output = delta;
  result.state.output[0] = retail_amplitude_clamp(next_x, amplitude);
  result.state.output[1] = retail_amplitude_clamp(next_y, amplitude);
  result.state.elapsed += frame_delta;
  return finite_shake(result.state) ? std::optional(result) : std::nullopt;
}

std::optional<RetailMode2DynamicResult> apply_mode2_dynamic_offset(
    const std::array<float, 4> &base_offset,
    const RetailMode2DynamicInput &input) noexcept {
  if (!finite_vector(base_offset) || !finite_shake(input.shake) ||
      !valid_draws(input.random_draws) ||
      !std::isfinite(input.player_at_890) ||
      !std::isfinite(input.player_at_894) ||
      !std::isfinite(input.player_at_898) ||
      !std::isfinite(input.player_at_8a0) ||
      !std::isfinite(input.manager_at_370) ||
      !std::isfinite(input.manager_at_374) ||
      !std::isfinite(input.frame_delta)) {
    return std::nullopt;
  }

  RetailMode2DynamicResult result;
  result.local_offset = base_offset;
  result.shake = input.shake;
  if (!input.player_present || !input.player_is_current ||
      (input.player_flags_at_88c & 0x2u) != 0) {
    return result;
  }

  const float scaled = input.player_at_894 * kPlayerScale;
  if (!std::isfinite(scaled))
    return std::nullopt;

  std::optional<RetailMode2ShakeStepResult> stepped;
  if (std::fabs(input.player_at_8a0) >= kEpsilon) {
    result.branch = RetailMode2DynamicBranch::Direct;
    const float amplitude = input.manager_at_370 * input.player_at_8a0;
    stepped = step_mode2_camera_shake(input.shake, kShakePeriod, amplitude,
                                      input.frame_delta, input.random_draws);
  } else if (input.player_at_890 > scaled) {
    result.branch = RetailMode2DynamicBranch::Scaled;
    const float numerator = input.player_at_890 - scaled;
    const float denominator =
        std::fmaf(input.player_at_898, kPlayerScale, -scaled);
    float ratio = (numerator / denominator) * 2.0F;
    ratio = retail_unit_clamp(ratio);
    const float amplitude = input.manager_at_374 * ratio;
    stepped = step_mode2_camera_shake(input.shake, kShakePeriod, amplitude,
                                      input.frame_delta, input.random_draws);
  } else {
    result.branch = RetailMode2DynamicBranch::Reset;
    result.shake = {};
    return result;
  }

  if (!stepped.has_value())
    return std::nullopt;
  result.shake = stepped->state;
  result.random_draws_consumed = stepped->random_draws_consumed;
  for (std::size_t lane = 0; lane < 3; ++lane) {
    result.local_offset[lane] += result.shake.output[lane];
  }
  return finite_vector(result.local_offset) ? std::optional(result)
                                             : std::nullopt;
}

const char *mode2_dynamic_branch_name(
    RetailMode2DynamicBranch branch) noexcept {
  switch (branch) {
  case RetailMode2DynamicBranch::GuardedOut:
    return "guarded_out";
  case RetailMode2DynamicBranch::Reset:
    return "reset";
  case RetailMode2DynamicBranch::Direct:
    return "direct";
  case RetailMode2DynamicBranch::Scaled:
    return "scaled";
  }
  return "invalid";
}

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
