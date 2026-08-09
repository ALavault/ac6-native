// 0x82303EA4..0x82303FAC, instruction by instruction.

#include "ac6/retail_live_flight_ramps.h"

#include <cmath>

namespace ac6::retail {
namespace {

// [+360] += (target - value) * (rate * step), with the multiply-add FUSED:
// retail is `fmuls f9,f12,f31` then `fmadds f0,f11,f9,f0`, so the rate-times-
// step product rounds once and the accumulate rounds once more.
float lag(float value, float target, float rate, float step) noexcept {
  const float amount = rate * step;
  return std::fmaf(target - value, amount, value);
}

// The secondary ramps: up by a whole step while the primary is past the
// threshold, down by a whole step otherwise, bounded to [0, ceiling]. Retail
// writes the unbounded value first and then overwrites it, so the bound is
// applied to the stored value rather than folded into the arithmetic.
float secondary(float value, float primary, float threshold, float ceiling,
                float step) noexcept {
  if (primary > threshold) {
    value += step;
    if (value > ceiling) {
      value = ceiling;
    }
  }
  else {
    value -= step;
    if (value < 0.0F) {
      value = 0.0F;
    }
  }
  return value;
}

}  // namespace

LiveRampState update_live_flight_ramps(LiveRampState state,
                                       const LiveRampInputs& inputs,
                                       float step) noexcept {
  if ((inputs.flags332 & kBypassBit) == 0U) {
    state.at360 = lag(state.at360, inputs.cmd48, inputs.rate952, step);
    state.at364 = lag(state.at364, inputs.cmd52, inputs.rate956, step);

    state.at368 = secondary(state.at368, state.at360, inputs.threshold404,
                            kAt368Ceiling, step);
    // The gate is a BYTE at +1224 and it is read AFTER the update, so a zero
    // byte discards the step that was just taken rather than preventing it.
    // Only at368 has one; at372 does not.
    if (!inputs.gate1224) {
      state.at368 = 0.0F;
    }
    state.at372 = secondary(state.at372, state.at364, inputs.threshold404,
                            kAt372Ceiling, step);
  }
  // 0x82303FAC is past the branch join, so this runs on both paths and reads
  // the fields back from the object.
  state.at376 = state.at360 - state.at364;
  return state;
}

}  // namespace ac6::retail
