// 0x82227E98..0x82227EDC: five loads, one subtraction, five direct calls.

#include "ac6/retail_flight_input_apply.h"

namespace ac6::retail {

FlightInputAccumulators apply_flight_input(
    FlightInputAccumulators state, const FlightInputFields& fields) noexcept {
  // Retail's order: 48, 52, 36, 44, 40.
  state.at48 = accumulate_flight_input(state.at48, fields.at2096,
                                       kHoldLowerLimit);
  state.at52 = accumulate_flight_input(state.at52, fields.at2100,
                                       kHoldLowerLimit);
  state.at36 = accumulate_flight_input(state.at36, fields.at2104,
                                       kAxisLowerLimit);
  // The one piece of arithmetic in the step: +44's increment is a DIFFERENCE.
  state.at44 = accumulate_flight_input(state.at44,
                                       fields.at2112 - fields.at2116,
                                       kAxisLowerLimit);
  state.at40 = accumulate_flight_input(state.at40, fields.at2108,
                                       kAxisLowerLimit);
  return state;
}

}  // namespace ac6::retail
