#pragma once

#include "ac6/retail_camera_table.h"
#include "ac6/retail_transform.h"

#include <array>
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
