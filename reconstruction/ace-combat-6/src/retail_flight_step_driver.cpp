// 0x82283898, the flight model's per-frame entry point.

#include "ac6/retail_flight_step_driver.h"

namespace ac6::retail {

FlightControlState apply_flight_step(FlightControlState state,
                                     const FlightControlInputs& inputs,
                                     float step) noexcept {
  // Slot 30 first, with the step's own float.
  state = update_flight_controls(state, inputs, step);

  // Then the reset, which discards five of slot 30's eight outputs. +368, +372
  // and +376 survive -- that asymmetry is what makes the ordering observable.
  if ((inputs.flags332 & kStepResetBit) != 0U) {
    state.at360 = 0.0F;
    state.at364 = 0.0F;
    state.at304 = 0.0F;
    state.at308 = 0.0F;
    state.at312 = 0.0F;
  }
  return state;
}

}  // namespace ac6::retail
