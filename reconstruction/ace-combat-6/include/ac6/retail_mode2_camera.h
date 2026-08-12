#pragma once

#include "ac6/retail_camera_table.h"
#include "ac6/retail_transform.h"

#include <array>
#include <cstdint>
#include <optional>

namespace ac6::retail {

// The live inputs consumed by the mode-2 camera transform. The player locator
// supplies its three qualified basis rows and world position. The two manager
// fields are applied after the camera position is placed, in retail call order:
// +0x3A4 through 0x820A9B30, then +0x3A0 through 0x820A99F8.
struct RetailMode2CameraState final {
  RetailBasis player_basis{};
  std::array<float, 3> player_position{};
  float rotation_at_3a0{};
  float rotation_at_3a4{};
  bool operator==(const RetailMode2CameraState &) const = default;
};

// The qualified part of the CGaLocator at camera-manager+0x140: its rows and
// translation. No vptr or unqualified locator metadata is represented.
struct RetailMode2CameraLocator final {
  RetailBasis basis{};
  std::array<float, 3> position{};
  bool operator==(const RetailMode2CameraLocator &) const = default;
};

// Qualified fields of the 0x50-byte state passed to 0x8225D0A8 from
// camera-manager+0x1C0. Padding is deliberately not represented. Names
// describe only the observed dataflow: the vector at +0x20 is the one added
// to the mode-2 local camera offset by 0x8225D9F0.
struct RetailMode2ShakeState final {
  std::array<float, 4> start{};     // +0x00
  std::array<float, 4> target{};    // +0x10
  std::array<float, 4> output{};    // +0x20
  std::array<float, 4> velocity{};  // +0x30
  float elapsed{};                  // +0x40
  bool operator==(const RetailMode2ShakeState &) const = default;
};

struct RetailMode2ShakeStepResult final {
  RetailMode2ShakeState state{};
  std::uint8_t random_draws_consumed{};
  bool operator==(const RetailMode2ShakeStepResult &) const = default;
};

enum class RetailMode2DynamicBranch : std::uint8_t {
  GuardedOut,
  Reset,
  Direct,
  Scaled,
};

// Live fields read by 0x8225D9F0. The two results of 0x82380798 are injected
// instead of owning AC6's global RNG; a replay can therefore supply and audit
// the exact draws. Only values 0..32767 are accepted.
struct RetailMode2DynamicInput final {
  bool player_present{};
  bool player_is_current{};
  std::uint32_t player_flags_at_88c{};
  float player_at_890{};
  float player_at_894{};
  float player_at_898{};
  float player_at_8a0{};
  float manager_at_370{};
  float manager_at_374{};
  float frame_delta{};
  RetailMode2ShakeState shake{};
  std::array<std::uint16_t, 2> random_draws{};
  bool operator==(const RetailMode2DynamicInput &) const = default;
};

struct RetailMode2DynamicResult final {
  std::array<float, 4> local_offset{};
  RetailMode2ShakeState shake{};
  RetailMode2DynamicBranch branch{RetailMode2DynamicBranch::GuardedOut};
  std::uint8_t random_draws_consumed{};
  bool operator==(const RetailMode2DynamicResult &) const = default;
};

// The scalar rotation state written by 0x82262A28.  These are deliberately
// kept separate from RetailMode2CameraState: mode 2's locator transform
// consumes +0x3A0/+0x3A4, while the producer also carries +0x3A8 for the
// manager's other camera paths.
struct RetailMode2RotationState final {
  float rotation_at_3a0{};
  float rotation_at_3a4{};
  float rotation_at_3a8{};
  bool operator==(const RetailMode2RotationState &) const = default;
};

// Inputs after the upstream target-selection block in 0x82262A28.  The
// target fields are the values arriving at the inner fmsubs/fmadds block; the
// selector's aircraft/player reads remain outside this bounded transition.
struct RetailMode2RotationInput final {
  RetailMode2RotationState current{};
  float target_at_3a0{};
  float target_at_3a4{};
  float target_at_3a8{};
  float response{};  // manager+0x368 multiplied by the frame delta
  bool wrap_at_3a4{};  // mode 1/2 + manager+0x4A8 branch
  bool operator==(const RetailMode2RotationInput &) const = default;
};

// Live scalar fields consumed by the direct mode-2 target branch of
// 0x82262A28. This is the manager+0x4A8 == 0 path: when a target is admitted,
// +0xE88 supplies the first axis and the negated +0xE8C supplies the second.
// The alternate +0x4A8 path and modes 1/3 deliberately remain separate.
struct RetailMode2DirectTargetInput final {
  RetailMode2RotationState current{};
  bool target_present{};
  bool manager_accepts_target{}; // manager+0x4A0
  float target_at_e88{};
  float target_at_e8c{};
  float gain_at_350{};
  float gain_at_360{};
  float gain_at_364{};
  float response_rate_at_368{};
  float frame_delta{};
  // Result of the later retail identity/query guard. Ownership of that global
  // query stays outside this scalar selector until its producer lands.
  bool suppress_axes{};
  bool operator==(const RetailMode2DirectTargetInput &) const = default;
};

// Scalar inputs to the non-mode-3 tail of 0x82262508. The VMX128 block before
// 0x8226283C constructs the two axes and remains an external producer; this
// structure does not claim or reproduce that transform. The three limits are
// the same manager fields later consumed by 0x82262A28.
struct RetailMode2IndirectTargetInput final {
  RetailMode2RotationState current{};
  float first_axis{};  // f28 at 0x8226283C
  float second_axis{}; // f31 at 0x8226283C
  float gain_at_350{};
  float gain_at_360{};
  float gain_at_364{};
  float response_rate_at_368{};
  float frame_delta{};
  bool suppress_axes{};
  bool operator==(const RetailMode2IndirectTargetInput &) const = default;
};

struct RetailMode2IndirectAxes final {
  float first{};
  float second{};
  bool operator==(const RetailMode2IndirectAxes &) const = default;
};

// 0x82262A4C..0x82262E64 for mode 2 with manager+0x4A8 == 0, excluding only
// the externally-owned identity/query result represented by suppress_axes.
// Produces the exact inputs consumed by step_mode2_camera_rotation().
std::optional<RetailMode2RotationInput> select_mode2_direct_camera_rotation(
    const RetailMode2DirectTargetInput &input) noexcept;

// 0x8226283C..0x82262930 with the mode-3 flag clear. Each finite axis is
// symmetrically bounded, divided by its selected positive limit and clamped to
// [-1,+1]. Zero limits produce zero exactly as the retail branches do.
std::optional<RetailMode2IndirectAxes> normalise_mode2_indirect_camera_axes(
    const RetailMode2IndirectTargetInput &input) noexcept;

// Composes that qualified scalar tail with the mode-2 path of
// 0x82262AE4..0x82262E64. It produces the exact inputs consumed by
// step_mode2_camera_rotation(); the VMX128 axis producer stays outside.
std::optional<RetailMode2RotationInput> select_mode2_indirect_camera_rotation(
    const RetailMode2IndirectTargetInput &input) noexcept;

// 0x8225D660, used by mode 3 when manager+0x39D is clear. The four values at
// manager+0x350 form a cubic curve evaluated after clamping the parameter to
// [0,1]. Invalid input and overflow fail closed.
std::optional<float>
evaluate_mode3_camera_gain_curve(const std::array<float, 4> &coefficients,
                                 float parameter) noexcept;

// 0x82262E68..0x82262F84, the state-writing core of 0x82262A28.  It keeps
// retail's fused interpolation, shortest-path wrap for +0x3A4, final angle
// normalisation, and the 0x001 snap.  Invalid arithmetic fails closed.
std::optional<RetailMode2RotationState>
step_mode2_camera_rotation(const RetailMode2RotationInput &input) noexcept;

// 0x8225D0A8 with 0x8225C178 inlined as an injected-RNG boundary. Retail's
// vector/scalar grouping and fused output updates are retained. The result is
// pure: callers explicitly carry the returned state into the next 60 Hz tick.
std::optional<RetailMode2ShakeStepResult> step_mode2_camera_shake(
    const RetailMode2ShakeState &state, float period, float amplitude,
    float frame_delta,
    const std::array<std::uint16_t, 2> &random_draws) noexcept;

// 0x8225D9F0. Applies its guards, selects the direct/scaled/reset branch,
// advances the +0x1C0 state and adds +0x1E0 xyz to `base_offset` where retail
// does. Invalid input and floating-point overflow fail closed.
std::optional<RetailMode2DynamicResult> apply_mode2_dynamic_offset(
    const std::array<float, 4> &base_offset,
    const RetailMode2DynamicInput &input) noexcept;

const char *mode2_dynamic_branch_name(RetailMode2DynamicBranch branch) noexcept;

// 0x82260A88..0x82260B88. `local_offset` is manager+0x30 after any dynamic
// adjustment. The function copies the player basis, transforms the local
// offset into world space, adds the player position and applies the two camera
// rotations. Invalid floating-point input fails closed.
std::optional<RetailMode2CameraLocator> transform_mode2_camera_locator(
    const RetailMode2CameraState &state,
    const std::array<float, 4> &local_offset) noexcept;

// 0x82260960/+0x64 copies record stage 0 to manager+0x30 before the transform.
// Retail may then change that offset through 0x8225D9F0. This helper
// deliberately returns the BASE locator, before that dynamic adjustment;
// callers must not promote it to the complete gameplay pose until the
// adjustment is resolved.
std::optional<RetailMode2CameraLocator>
resolve_mode2_base_camera_locator(const RetailCameraRecord &record,
                                  const RetailMode2CameraState &state) noexcept;

} // namespace ac6::retail
