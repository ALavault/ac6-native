// The contracted flight chain, composed in retail's order.

#include "ac6/retail_flight_session.h"

#include <cstring>

namespace ac6::retail {
namespace {

// One command setter, applied to the state. Retail's setters take `this` and
// reach four fields; here the four are passed explicitly so the composition is
// readable against the derivation rather than hidden behind an offset table.
bool apply_command(float& current, float& accumulator, float target,
                   float increment) noexcept {
  const FlightCommandResult result =
      set_flight_command(current, accumulator, increment, target);
  accumulator = result.accumulator;
  if (result.changed) {
    // Retail stores the target in a separate field and raises a flag; the
    // consumer of that field is not ported, so this session tracks only what
    // the contracted chain reads back -- the accumulator. The target is
    // returned to the caller in the frame instead of being dropped silently.
    current = result.target;
  }
  return result.changed;
}

}  // namespace

namespace {

// Everything after the commands are in place. Both entry points share it, which
// is the point: the two interfaces differ only in how the accumulators are
// written.
FlightFrame step_after_commands(FlightSessionState& state,
                                const FlightModelConfig& config,
                                FlightFrame frame, float step) noexcept {
  // 2. slot 30 -- the ramps first, then the axes, as the listing has them.
  LiveRampInputs ramp_inputs{};
  // THE HOLDS, which the first version of this file never fed. +48 and +52 are
  // accumulators like the three axes, and retail_live_flight_ramps reads them as
  // its two targets; leaving them zero made both ramps decay for ever.
  ramp_inputs.cmd48 = state.accumulators.at48;
  ramp_inputs.cmd52 = state.accumulators.at52;
  ramp_inputs.rate952 = config.rampRate952;
  ramp_inputs.rate956 = config.rampRate956;
  ramp_inputs.threshold404 = config.rampThreshold404;
  ramp_inputs.gate1224 = config.rampGate1224;
  ramp_inputs.flags332 = state.flags332;
  state.ramps = update_live_flight_ramps(state.ramps, ramp_inputs, step);

  LiveAxisInputs axis_inputs{};
  axis_inputs.cmd36 = state.accumulators.at36;
  axis_inputs.cmd44 = state.accumulators.at44;
  axis_inputs.cmd40 = state.accumulators.at40;
  axis_inputs.rates304 = config.rates304;
  axis_inputs.rates308 = config.rates308;
  axis_inputs.rates312 = config.rates312;
  state.axes = update_live_flight_axes(state.axes, axis_inputs, step);

  // 3. the rate servo. Its axis values come from what slot 30 just wrote.
  RateServoAxis first = config.servo304;
  RateServoAxis second = config.servo308;
  RateServoAxis third = config.servo312;
  first.axis = state.axes.at304;
  first.limit = config.limits.at1248;
  second.axis = state.axes.at308;
  second.limit = config.limits.at1252;
  third.axis = state.axes.at312;
  third.limit = config.limits.at1256;
  state.rates = update_flight_rate_servo(state.rates, first, second, third, step);

  // 4. the decays.
  state.decays = apply_live_step_decays(state.decays, state.flags332, step);

  // 5. the rotation angles, then the basis, in A3.1's order.
  FlightRotationAxes rotation_axes{};
  rotation_axes.at304 = state.axes.at304;
  rotation_axes.at308 = state.axes.at308;
  rotation_axes.at312 = state.axes.at312;
  frame.angles = flight_rotation_angles(config.limits, rotation_axes, step,
                                        config.row0Divisor);
  rotate_820A9B30(state.basis, frame.angles.about_row1);
  rotate_820A99F8(state.basis, frame.angles.about_row0);
  rotate_82211828(state.basis, frame.angles.about_row2);

  // 6. the accessors, then the export block.
  frame.accessors.slot16 = state.axes.at308;
  frame.accessors.slot17 =
      blend_control_axis(state.axes.at304, state.rates.at144, state.decays.at408,
                         state.flags332, kSlot17Scale).value;
  frame.accessors.slot18 =
      blend_control_axis(state.axes.at312, state.rates.at152, state.decays.at412,
                         state.flags332, kSlot18Scale).value;

  FlightExportFields export_fields{};
  export_fields.at32 = 0.0F;
  export_fields.at356 = 0.0F;
  export_fields.at368 = state.ramps.at368;
  export_fields.at372 = state.ramps.at372;
  export_fields.at376 = state.ramps.at376;
  frame.exported = export_flight_state_slot20(frame.exported, frame.accessors,
                                              export_fields);
  return frame;
}

}  // namespace

FlightFrame step_flight_session(FlightSessionState& state,
                                const FlightModelConfig& config,
                                const FlightStick& stick, float step) noexcept {
  FlightFrame frame{};
  frame.accepted12 = apply_command(state.current16, state.accumulators.at36,
                                   stick.target12, stick.increment12);
  frame.accepted13 = apply_command(state.current20, state.accumulators.at44,
                                   stick.target13, stick.increment13);
  frame.accepted14 = apply_command(state.current24, state.accumulators.at40,
                                   stick.target14, stick.increment14);
  return step_after_commands(state, config, frame, step);
}

FlightFrame step_flight_session(FlightSessionState& state,
                                const FlightModelConfig& config,
                                const FlightInputFields& fields,
                                float step) noexcept {
  FlightFrame frame{};
  state.accumulators = apply_flight_input(state.accumulators, fields);
  return step_after_commands(state, config, frame, step);
}

std::uint64_t digest_flight_state(const FlightSessionState& state) noexcept {
  // FNV-1a 64 over the raw bytes, the same as RetailInputLog. Floats are hashed
  // as bytes deliberately: two states that differ in the last bit are two
  // different states, and that is exactly what a replay must catch.
  std::uint64_t hash = 1469598103934665603ULL;
  const auto* bytes = reinterpret_cast<const unsigned char*>(&state);
  for (std::size_t index = 0; index < sizeof(state); ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ULL;
  }
  return hash;
}

}  // namespace ac6::retail
