// 0x82281608, 0x822816C0 and 0x82281778 -- three copies of one function.

#include "ac6/retail_flight_command.h"
#include "ac6/retail_input_binding.h"   // select_ge_zero, the fsel rule

#include <cmath>

namespace ac6::retail {

FlightCommandResult set_flight_command(float current, float accumulator,
                                       float increment, float target) noexcept {
  FlightCommandResult out{};
  out.accumulator = accumulator;

  // One step each way, not a modulo.
  if (target < kMinusPi) {
    target = target + kTwoPi;
  }
  else if (target > kPi) {
    target = target - kTwoPi;
  }

  bool changed = false;
  if (std::fabs(target) != kPi) {
    changed = std::fabs(current - target) > kOneDegree;
  }
  else {
    // `fsel f2,f0,f13,f12` -- a >= 0 ? +pi : -pi, and the comparison is against
    // +0.0, so a NEGATIVE ZERO current angle takes +pi. Same rule as the input
    // binding layer, and the same reason: idle axes leave -0.0 behind.
    target = select_ge_zero(current, kPi, kMinusPi);
    changed = std::fabs(std::fabs(current) - kPi) > kOneDegree;
  }

  out.target = target;
  if (changed) {
    out.changed = true;
    out.accumulator = accumulator + increment;
  }
  return out;
}

}  // namespace ac6::retail
