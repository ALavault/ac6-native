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
