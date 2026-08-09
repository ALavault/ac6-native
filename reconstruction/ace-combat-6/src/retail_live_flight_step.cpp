// The arithmetic half of 0x82306A38: the two decays at 0x82306B40..0x82306BB4.

#include "ac6/retail_live_flight_step.h"

#include <cmath>

namespace ac6::retail {
namespace {

// fneg / fmuls / fmadds: value + (-value * step) * 3, with the final
// multiply-add fused and the negation and step-multiply rounded separately.
float decay_once(float value, float step) noexcept {
  if (std::fabs(value) < kDecayEpsilon) {
    return value;                     // `bne` skips the whole block
  }
  const float scaled = -value * step;
  return std::fmaf(scaled, kDecayRate, value);
}

}  // namespace

LiveStepDecays apply_live_step_decays(LiveStepDecays state,
                                      std::uint32_t flags332,
                                      float step) noexcept {
  if ((flags332 & kResetBit) != 0U) {
    return state;                     // retail calls slot 33 instead
  }
  // Retail does +412 first and +408 second; they are independent, but the order
  // is kept so a reader can follow the listing.
  state.at412 = decay_once(state.at412, step);
  state.at408 = decay_once(state.at408, step);
  return state;
}

}  // namespace ac6::retail
