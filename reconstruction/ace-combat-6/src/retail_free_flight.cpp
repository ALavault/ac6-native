#include "ac6/retail_free_flight.h"

#include "ac6/demo_flight_input.h"

#include <algorithm>
#include <cmath>

namespace ac6::retail {
namespace {

FlightModelConfig native_model_config() noexcept {
  FlightModelConfig config{};
  // The three constructor limits are retail constants. The remaining gains
  // are the bounded native free-flight profile used by the composed-kernel
  // tests; they are not presented as a decoded F-16 performance table.
  config.limits = {5.0F, 1.399999976158142F, 5.400000095367432F};
  config.rates304 = {4.0F, 2.0F};
  config.rates308 = {3.0F, 1.5F};
  config.rates312 = {5.0F, 2.5F};
  config.servo304 = {0.0F, 0.0F, 2.0F, 3.0F};
  config.servo308 = config.servo304;
  config.servo312 = config.servo304;
  config.rampRate952 = 3.0F;
  config.rampRate956 = 3.0F;
  config.rampThreshold404 = 0.5F;
  return config;
}

float signed_axis(const std::int16_t value) noexcept {
  return std::clamp(static_cast<float>(value) / 32767.0F, -1.0F, 1.0F);
}

FlightInputFields input_fields(const InputFrame input) noexcept {
  InputRecord record{};
  record.axis_ly = signed_axis(input.pitch);
  record.axis_lx = signed_axis(input.roll);
  record.axis_rx = signed_axis(input.yaw);
  record.axis_ry = static_cast<float>(input.throttle) / 255.0F;
  return ac6::demo::fields_from_record(record,
                                        ac6::demo::default_stick_bindings());
}

bool finite_position(const CombatVector position) noexcept {
  return std::isfinite(position.x) && std::isfinite(position.y) &&
         std::isfinite(position.z);
}

}  // namespace

InputFrame native_free_flight_qualification_input(
    const std::uint32_t tick) noexcept {
  InputFrame input{};
  if (tick > 300U && tick <= 540U) {
    input.pitch = 18000;
  } else if (tick > 540U && tick <= 780U) {
    input.roll = 18000;
  } else if (tick > 780U && tick <= 1020U) {
    input.yaw = 18000;
  } else if (tick > 1020U && tick <= 1260U) {
    input.throttle = 255U;
  } else if (tick > 1260U && tick <= 1500U) {
    input.buttons = kNativeFreeFlightBrakeButton;
  }
  return input;
}

std::optional<RetailFreeFlight> RetailFreeFlight::open(
    RetailCameraRecord camera_record, const CombatVector anchor) noexcept {
  const std::optional<std::array<float, 4>> offset = camera_record.offset(0);
  if (!offset.has_value() || !finite_position(anchor) ||
      !(camera_record.fov_radians() > 0.0F) ||
      !std::isfinite(camera_record.fov_radians())) {
    return std::nullopt;
  }
  for (const float value : *offset) {
    if (!std::isfinite(value)) return std::nullopt;
  }

  RetailFreeFlight flight;
  flight.camera_record_ = std::move(camera_record);
  flight.model_ = native_model_config();
  flight.state_.position = {
      anchor.x, std::max(anchor.y + 400.0F, 400.0F), anchor.z};
  const float toward_origin = std::atan2(-anchor.x, -anchor.z);
  if (std::isfinite(toward_origin)) {
    rotate_820A9B30(flight.state_.basis, toward_origin);
  }
  flight.initial_state_ = flight.state_;
  if (!flight.refresh_frame()) return std::nullopt;
  return flight;
}

bool RetailFreeFlight::step(float fixed_dt, const InputFrame input) noexcept {
  if (!(fixed_dt > 0.0F) || fixed_dt > 0.25F) fixed_dt = 1.0F / 60.0F;
  (void)step_flight_session(state_, model_, input_fields(input), fixed_dt);

  constexpr float kThrottleAccelerationKmhPerSecond = 420.0F;
  constexpr float kBrakeDecelerationKmhPerSecond = 720.0F;
  constexpr float kMinimumSpeedKmh = 360.0F;
  constexpr float kMaximumSpeedKmh = 1800.0F;
  const float throttle = static_cast<float>(input.throttle) / 255.0F;
  const float brake = (input.buttons & kNativeFreeFlightBrakeButton) != 0U
                          ? 1.0F
                          : 0.0F;
  speed_kmh_ = std::clamp(
      speed_kmh_ +
          (throttle * kThrottleAccelerationKmhPerSecond -
           brake * kBrakeDecelerationKmhPerSecond) *
              fixed_dt,
      kMinimumSpeedKmh, kMaximumSpeedKmh);

  const BasisRow& forward = state_.basis.rows[2];
  integrate_session_position(
      state_, {forward[0], forward[1], forward[2]}, speed_kmh_, 0.0F,
      fixed_dt);
  return refresh_frame();
}

void RetailFreeFlight::reset() noexcept {
  state_ = initial_state_;
  speed_kmh_ = initial_speed_kmh_;
  (void)refresh_frame();
}

bool RetailFreeFlight::refresh_frame() noexcept {
  const RetailMode2CameraState camera_state{
      state_.basis,
      {state_.position.at64, state_.position.at68, state_.position.at72},
      0.0F,
      0.0F};
  const std::optional<RetailMode2CameraLocator> camera =
      resolve_mode2_base_camera_locator(camera_record_, camera_state);
  if (!camera.has_value()) return false;
  frame_.position = state_.position;
  frame_.basis = state_.basis;
  frame_.attitude = flight_basis_attitude(state_.basis);
  frame_.camera = *camera;
  frame_.camera_fov_radians = camera_record_.fov_radians();
  frame_.speed_kmh = speed_kmh_;
  return true;
}

WorldFrame RetailFreeFlight::world_frame(
    const std::uint64_t tick, const std::uint32_t mission_id,
    const std::uint32_t active_units, const EntityId player_entity,
    const InputFrame input) const noexcept {
  WorldFrame world{};
  world.tick = tick;
  world.mission_id = mission_id;
  world.position_x = frame_.position.at64;
  world.position_y = frame_.position.at68;
  world.position_z = frame_.position.at72;
  world.pitch = frame_.attitude.pitch;
  world.roll = frame_.attitude.roll;
  world.yaw = frame_.attitude.yaw;
  world.speed = frame_.speed_kmh / 3.6F;
  world.active_units = active_units;
  world.player_entity = player_entity;
  world.camera_x = frame_.camera.position[0];
  world.camera_y = frame_.camera.position[1];
  world.camera_z = frame_.camera.position[2];
  constexpr float kLookDistance = 400.0F;
  world.camera_target_x = world.camera_x +
                          frame_.camera.basis.rows[2][0] * kLookDistance;
  world.camera_target_y = world.camera_y +
                          frame_.camera.basis.rows[2][1] * kLookDistance;
  world.camera_target_z = world.camera_z +
                          frame_.camera.basis.rows[2][2] * kLookDistance;
  world.input = input;
  return world;
}

}  // namespace ac6::retail
