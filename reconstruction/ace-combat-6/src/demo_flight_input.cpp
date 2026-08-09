// A controller wired to the contracted chain. Every rule is retail's; only the
// wiring is mine.

#include "ac6/demo_flight_input.h"

namespace ac6::demo {
namespace {

// One axis through the CONTRACTED binding layer. `apply_input_binding` returns
// an empty optional when the processed value is exactly zero, because retail
// STORES NOTHING then -- cycle 1355 found that with a differential on its first
// run. An untouched entity field means a zero increment, which is the faithful
// reading for a caller with no previous frame to preserve.
float binding_value(float raw, const ac6::retail::InputBinding& binding) noexcept {
  const auto outputs = ac6::retail::apply_input_binding(raw, binding);
  return outputs.has_value() ? outputs->value : 0.0F;
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
  bindings.throttle = binding;
  return bindings;
}

ac6::retail::FlightInputFields fields_from_record(
    const ac6::retail::InputRecord& record,
    const StickBindings& bindings) noexcept {
  ac6::retail::FlightInputFields fields{};
  // ESTABLISHED that these two are the binding layer's first two outputs
  // (cycle 1404); CHOSEN that they are pitch and roll.
  fields.at2104 = binding_value(record.axis_ly, bindings.pitch);
  fields.at2108 = binding_value(record.axis_lx, bindings.roll);
  // NOT ESTABLISHED at all: retail's +44 takes [+2112] - [+2116], a difference
  // of two fields nothing has traced. Driving it from one signed axis split
  // across the pair reproduces the arithmetic exactly while making no claim
  // about where retail's two halves come from.
  const float yaw = binding_value(record.axis_rx, bindings.yaw);
  fields.at2112 = yaw > 0.0F ? yaw : 0.0F;
  fields.at2116 = yaw < 0.0F ? -yaw : 0.0F;
  // The two holds. Their sources are unknown; the right trigger is a choice.
  fields.at2096 = binding_value(record.axis_ry, bindings.throttle);
  fields.at2100 = 0.0F;
  return fields;
}

ac6::retail::FlightInputFields fields_from_snapshot(
    const std::uint8_t* snapshot, const StickBindings& bindings) noexcept {
  return fields_from_record(ac6::retail::build_input_record(snapshot), bindings);
}

}  // namespace ac6::demo
