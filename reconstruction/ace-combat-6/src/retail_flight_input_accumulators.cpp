// 0x82281FB0, 0x82282020, 0x82281FE8, 0x82281F40, 0x82281F78 -- one function,
// five times, differing only in the field and the lower bound.

#include "ac6/retail_flight_input_accumulators.h"

namespace ac6::retail {

float accumulate_flight_input(float value, float increment,
                              float lower) noexcept {
  value = increment + value;          // fadds f0,f1,f0 -- f1 first
  if (value < lower) {
    return lower;
  }
  if (value <= kInputUpperLimit) {    // blelr: at or below, return unchanged
    return value;
  }
  return kInputUpperLimit;
}

}  // namespace ac6::retail
