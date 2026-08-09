// A controller wired to the contracted chain. Every rule is retail's; only the
// wiring is mine -- and cycle 1409 made that wiring smaller.

#include "ac6/demo_flight_input.h"

#include "ac6/retail_flight_input_router.h"

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
  // CHOSEN: which controller axis fills which binding slot. Four raw axes into
  // the first four slots of the binding layer's output array. The per-player
  // table that decides this in retail is loaded from data this campaign has not
  // reached, so it is a choice -- but it is now the ONLY choice in this file.
  ac6::retail::FlightBindingOutputs outputs{};
  outputs.value[0] = binding_value(record.axis_ly, bindings.pitch);
  outputs.value[1] = binding_value(record.axis_lx, bindings.roll);
  const float yaw = binding_value(record.axis_rx, bindings.yaw);
  outputs.value[2] = yaw > 0.0F ? yaw : 0.0F;
  outputs.value[3] = yaw < 0.0F ? -yaw : 0.0F;
  outputs.value[4] = binding_value(record.axis_ry, bindings.throttle);
  outputs.value[5] = 0.0F;

  // ESTABLISHED, cycle 1409: everything from here to the entity's six fields is
  // retail's own routing, ported from 0x82229310..0x8222946C and differentially
  // verified. The demo picks the analog device and layout 0, which is the arm
  // whose fields the layout-1 response curve does not touch -- so these six
  // values are final rather than pre-curve.
  //
  // The holds at +2096 and +2100 take BUTTON BITS on this arm, not the analog
  // trigger the previous version fed them. That was an invention this cycle
  // deleted rather than confirmed; the trigger now reaches slot 4, which only
  // the digital-device arm forwards.
  std::uint32_t buttons = 0U;
  if (outputs.value[4] > 0.5F) {
    buttons |= 1U << ac6::retail::kAnalogHoldLowBit;  // CHOSEN: trigger as a hold
  }
  return ac6::retail::route_flight_input_fields(
      outputs, buttons, ac6::retail::FlightInputDevice::kAnalog, 0U);
}

ac6::retail::FlightInputFields fields_from_snapshot(
    const std::uint8_t* snapshot, const StickBindings& bindings) noexcept {
  return fields_from_record(ac6::retail::build_input_record(snapshot), bindings);
}

}  // namespace ac6::demo
