// 0x82302DB0, instruction by instruction.
//
// Every clamp below is written the way its branch encodes, not the way its name
// reads. `bge` skips a store when the LT bit is clear, so `if (x < limit)` is
// the store; `ble` skips when GT is clear, so `if (x > limit)` is the store. On
// a NaN both branches are taken and no store happens, which the `<` and `>`
// forms reproduce and their negations do not -- the same rule that cycle 1372
// got backwards once in retail_flight_step.cpp.

#include "ac6/retail_flight_controls.h"

#include <cmath>

namespace ac6::retail {
namespace {

// One ramp: fall by `decay` every frame, then rise by `rise` while held, with a
// floor of 0 and a ceiling of 1. Retail stores the intermediate value before
// each clamp and reloads it after, so the clamped value is what the next stage
// reads -- reproduced here by assigning through.
float step_ramp(float value, float rise, float decay, bool held) noexcept {
  value -= decay;
  if (value < 0.0F) {
    value = 0.0F;
  }
  if (held) {
    value += rise;
    if (value > 1.0F) {
      value = 1.0F;
    }
  }
  return value;
}

// The self-centring half of an axis: move toward zero by `amount`, never through
// it. Retail writes this as two mirrored blocks around `fnmsubs` and `fmadds`,
// both fused.
// `rate` and `factor` stay SEPARATE because retail fuses their product into the
// subtraction: `fnmsubs f13,f12,f9,f13` is `f13 - f12*f9` with one rounding.
// Pre-multiplying them and subtracting rounds twice and is a different function
// in the last ulp -- the same trap the integrator's fmadds carries.
float centre_axis(float value, float rate, float factor) noexcept {
  if (value > 0.0F) {
    value = std::fmaf(-rate, factor, value);   // fnmsubs f13,f12,f9,f13
    if (value < 0.0F) {
      value = 0.0F;
    }
  }
  else if (value < 0.0F) {
    value = std::fmaf(rate, factor, value);    // fmadds f13,f12,f9,f13
    if (value > 0.0F) {
      value = 0.0F;
    }
  }
  return value;
}

}  // namespace

FlightControlState update_flight_controls(FlightControlState state,
                                          const FlightControlInputs& inputs,
                                          float step) noexcept {
  // --- the two primary ramps, +360 and +364 -----------------------------
  const float ramp_rise = step * kRampRate;
  const float ramp_decay = ramp_rise * kRampDecayFactor;
  state.at360 = step_ramp(state.at360, ramp_rise, ramp_decay,
                          inputs.hold48 != 0.0F);
  state.at364 = step_ramp(state.at364, ramp_rise, ramp_decay,
                          inputs.hold52 != 0.0F);

  // --- the two secondary ramps, +368 and +372 ---------------------------
  // These do not merely rise while gated: when the gate is closed retail
  // ASSIGNS zero (0x82302EB0, 0x82302EF4) rather than letting the decay run.
  const float second_rise = step * kSecondRampRate;
  const float second_decay = second_rise * kSecondRampDecayFactor;

  state.at368 -= second_decay;
  if (state.at368 < 0.0F) {
    state.at368 = 0.0F;
  }
  if (state.at360 > kSecondRampGate) {
    state.at368 += second_rise;
    if (state.at368 > 1.0F) {
      state.at368 = 1.0F;
    }
  }
  else {
    state.at368 = 0.0F;
  }

  state.at372 -= second_decay;
  if (state.at372 < 0.0F) {
    state.at372 = 0.0F;
  }
  if (state.at364 > kSecondRampGate) {
    state.at372 += second_rise;
    if (state.at372 > 1.0F) {
      state.at372 = 1.0F;
    }
  }
  else {
    state.at372 = 0.0F;
  }

  // The interlock. `beq` at 0x82302EFC and 0x82302F04 jump PAST both stores, so
  // they run only when NEITHER primary is zero. See the header.
  if (state.at360 != 0.0F && state.at364 != 0.0F) {
    state.at368 = 0.0F;
    state.at372 = 0.0F;
  }

  state.at376 = state.at360 - state.at364;

  // --- the three axes ---------------------------------------------------
  float rate = step * kAxisRate;
  float rate308 = step * kAt308Rate;
  float rate312 = rate;
  if ((inputs.flags332 & kAxisRateScaleBit) != 0U) {
    // 1 / ((field344 + 0.1) * 10), applied to all three.
    const float divisor = (inputs.field344 + kRateScaleOffset) * kSecondRampRate;
    const float scale = 1.0F / divisor;
    rate *= scale;
    rate308 *= scale;
    rate312 *= scale;
  }

  // +304: centred, then a command scaled by 1.0 up and 0.9 down, clamped
  // asymmetrically to [-0.9, +1.0].
  state.at304 = centre_axis(state.at304, rate, kAxisCentringFactor);
  if (inputs.cmd36 != 0.0F) {
    const float gain = inputs.cmd36 > 0.0F ? 1.0F : kAt304NegativeGain;
    state.at304 = std::fmaf(inputs.cmd36 * gain, rate, state.at304);
    if (state.at304 > 1.0F) {
      state.at304 = 1.0F;
    }
    else if (state.at304 < kAt304LowerLimit) {
      state.at304 = kAt304LowerLimit;
    }
  }

  // +312: centred, then the command scaled by the rate, clamped to [-1, +1].
  state.at312 = centre_axis(state.at312, rate312, kAxisCentringFactor);
  if (inputs.cmd40 != 0.0F) {
    state.at312 = std::fmaf(inputs.cmd40, rate312, state.at312);
    if (state.at312 > 1.0F) {
      state.at312 = 1.0F;
    }
    else if (state.at312 < -1.0F) {
      state.at312 = -1.0F;
    }
  }

  // +308: centred, then driven by the SIGN of its command -- retail adds or
  // subtracts the whole rate and never multiplies by cmd44. That asymmetry with
  // the other two axes is in the listing at 0x823030C8..0x82303108.
  state.at308 = centre_axis(state.at308, rate308, kAxisCentringFactor);
  if (inputs.cmd44 > 0.0F) {
    state.at308 += rate308;
    if (state.at308 > 1.0F) {
      state.at308 = 1.0F;
    }
  }
  else if (inputs.cmd44 < 0.0F) {
    state.at308 -= rate308;
    if (state.at308 < -1.0F) {
      state.at308 = -1.0F;
    }
  }
  return state;
}

}  // namespace ac6::retail
