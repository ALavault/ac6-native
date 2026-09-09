#pragma once

// Native Mission 01 free-flight composition.
//
// Input routing, command accumulation, control surfaces, rate servo,
// orientation and the final position integrator are the existing retail
// kernels. The two explicit native boundaries are the controller-slot wiring
// (already isolated by demo_flight_input) and the speed producer that retail's
// unavailable live aerodynamic slot would normally supply. This class is not
// used by the generic MissionRuntime integrator.

#include "ac6/product_runtime.h"
#include "ac6/retail_camera_table.h"
#include "ac6/retail_flight_session.h"
#include "ac6/retail_mode2_camera.h"

#include <optional>

namespace ac6::retail {

// XInput left shoulder. The SDL keyboard adapter maps the brake key to the
// same bit, so the fifth control survives replay serialization unchanged.
inline constexpr std::uint16_t kNativeFreeFlightBrakeButton = 0x0100U;
inline constexpr std::uint32_t kNativeFreeFlightQualificationTicks = 3600U;

// Fixed input schedule for the N1b 60-second receipt. Tick zero is never
// stepped; ticks 1..1800 preserve the N1a neutral baseline, pitch, roll, yaw,
// throttle, brake and recovery. Ticks 1801..3600 sustain neutral flight.
InputFrame native_free_flight_qualification_input(
    std::uint32_t tick) noexcept;

struct RetailFreeFlightFrame final {
  FlightPosition position{};
  RetailBasis basis{identity_basis()};
  FlightBasisAttitude attitude{};
  RetailMode2CameraLocator camera{};
  float camera_fov_radians{};
  float speed_kmh{};
  bool operator==(const RetailFreeFlightFrame&) const = default;
};

class RetailFreeFlight final {
 public:
  // `anchor` is an authored, placed M01 unit position. The native launch starts
  // above it and points toward the map origin; an unplaced player origin is
  // never accepted as a fallback.
  static std::optional<RetailFreeFlight> open(
      RetailCameraRecord camera_record, CombatVector anchor) noexcept;

  bool step(float fixed_dt, InputFrame input) noexcept;
  void reset() noexcept;

  WorldFrame world_frame(std::uint64_t tick, std::uint32_t mission_id,
                         std::uint32_t active_units, EntityId player_entity,
                         InputFrame input) const noexcept;

  const RetailFreeFlightFrame& frame() const noexcept { return frame_; }
  const FlightSessionState& state() const noexcept { return state_; }
  std::uint64_t state_digest() const noexcept {
    return digest_flight_state(state_);
  }

 private:
  RetailFreeFlight() = default;
  bool refresh_frame() noexcept;

  RetailCameraRecord camera_record_{};
  FlightModelConfig model_{};
  FlightSessionState initial_state_{};
  FlightSessionState state_{};
  RetailFreeFlightFrame frame_{};
  float initial_speed_kmh_{900.0F};
  float speed_kmh_{900.0F};
};

}  // namespace ac6::retail
