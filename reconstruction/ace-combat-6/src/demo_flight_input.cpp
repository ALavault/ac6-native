// A controller wired to the contracted chain. The path is retail's; the last
// step -- binding output to command setter -- is mine.

#include "ac6/demo_flight_input.h"

#include <cmath>

namespace ac6::demo {
namespace {

// One axis through the contracted binding layer, then through my conversion.
// `apply_input_binding` returns an empty optional when the processed value is
// exactly zero, because retail STORES NOTHING in that case -- cycle 1355 found
// that with a differential on its first run. Here that means the axis
// contributes nothing at all, which is the faithful reading of "the caller's
// array keeps what the previous frame left there" for a caller that has no such
// array.
void axis_to_command(float raw, const ac6::retail::InputBinding& binding,
                     float full_scale, float rate, float& target,
                     float& increment) noexcept {
  const auto outputs = ac6::retail::apply_input_binding(raw, binding);
  if (!outputs.has_value()) {
    target = 0.0F;
    increment = 0.0F;
    return;
  }
  target = outputs->value * full_scale;
  increment = std::fabs(outputs->value) * rate;
}

}  // namespace

StickBindings default_stick_bindings() noexcept {
  ac6::retail::InputBinding binding{};
  binding.deadzone = 0.08F;
  binding.scale = 1.0F;
  binding.threshold = 0.5F;
  StickBindings bindings{};
  bindings.pitch = binding;
  bindings.roll = binding;
  bindings.yaw = binding;
  return bindings;
}

ac6::retail::FlightStick stick_from_record(
    const ac6::retail::InputRecord& record,
    const StickBindings& bindings) noexcept {
  ac6::retail::FlightStick stick{};
  // The axis-to-slot assignment is MINE: left stick Y to slot 12, X to slot 14,
  // right stick X to slot 13. Cycle 1393 established which accumulator each slot
  // feeds and cycle 1388 which axis each accumulator drives; what neither
  // established is which CONTROLLER axis retail routes to which slot.
  axis_to_command(record.axis_ly, bindings.pitch,
                  bindings.invented_full_scale_angle,
                  bindings.invented_increment_rate,
                  stick.target12, stick.increment12);
  axis_to_command(record.axis_rx, bindings.yaw,
                  bindings.invented_full_scale_angle,
                  bindings.invented_increment_rate,
                  stick.target13, stick.increment13);
  axis_to_command(record.axis_lx, bindings.roll,
                  bindings.invented_full_scale_angle,
                  bindings.invented_increment_rate,
                  stick.target14, stick.increment14);
  return stick;
}

ac6::retail::FlightStick stick_from_snapshot(
    const std::uint8_t* snapshot, const StickBindings& bindings) noexcept {
  return stick_from_record(ac6::retail::build_input_record(snapshot), bindings);
}

}  // namespace ac6::demo
