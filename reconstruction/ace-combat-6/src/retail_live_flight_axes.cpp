// 0x82303FB0..0x823042B0, instruction by instruction.

#include "ac6/retail_live_flight_axes.h"

#include <cmath>

namespace ac6::retail {
namespace {

bool driven(float command) noexcept {
  // `blt` sets the flag when |cmd| < eps, and the `bne` that follows takes the
  // DECAY path on that flag. So the driven regime is |cmd| >= eps, and a NaN
  // command makes `blt` false and lands here too, which `!(x < eps)` reproduces
  // and `x >= eps` does not.
  return !(std::fabs(command) < kCommandEpsilon);
}

// Toward zero by `amount`, never through it. Retail writes the value and then
// overwrites it with zero on the overshoot, so the store is what is clamped.
float decay(float value, float amount) noexcept {
  if (value > 0.0F) {
    value -= amount;
    if (value < 0.0F) {
      value = 0.0F;
    }
  }
  else if (value < 0.0F) {
    value += amount;
    if (value > 0.0F) {
      value = 0.0F;
    }
  }
  return value;
}

// sign(cmd) * (|cmd| + cmd*cmd) * 2/3, written the way retail writes it: one
// fused multiply-add per sign, then a separate multiply.
float curve(float command) noexcept {
  float shaped = 0.0F;
  if (command > 0.0F) {
    shaped = std::fmaf(command, command, command);      // fmadds
  }
  else if (command < 0.0F) {
    shaped = std::fmaf(-command, command, command);     // fnmsubs
  }
  return shaped * kCurveScale;
}

float clamp_unit(float value) noexcept {
  if (value < kLowerLimit) {
    return kLowerLimit;
  }
  if (value > kUpperLimit) {
    return kUpperLimit;
  }
  return value;
}

// The lag both curved axes use: move 1.5 times the scaled gap toward the target.
float lag_to_curve(float value, float command, float rate) noexcept {
  float gap = curve(command) - value;
  gap = gap * rate;
  return clamp_unit(std::fmaf(gap, kLagGain, value));    // fmadds
}

}  // namespace

LiveAxisState update_live_flight_axes(LiveAxisState state,
                                      const LiveAxisInputs& inputs,
                                      float step) noexcept {
  const float command304 = inputs.rates304.command * step;
  const float decay304 = inputs.rates304.decay * step;
  const float command308 = inputs.rates308.command * step;
  const float decay308 = inputs.rates308.decay * step;
  const float command312 = inputs.rates312.command * step;
  const float decay312 = inputs.rates312.decay * step;

  state.at304 = driven(inputs.cmd36)
      ? lag_to_curve(state.at304, inputs.cmd36, command304)
      : decay(state.at304, decay304);

  if (driven(inputs.cmd44)) {
    // No curve and no lag here: a step in the command's direction, plus the two
    // hold timers. The positive and negative halves are INDEPENDENT `if/else`
    // pairs in retail, not one three-way branch, so both run every frame.
    if (inputs.cmd44 > 0.0F) {
      state.at308 += command308;
      state.at1352 += step;
      if (state.at308 > kUpperLimit) {
        state.at308 = kUpperLimit;
      }
    }
    else {
      state.at1352 = std::fmaf(-step, kTimerDecayFactor, state.at1352);
      if (state.at1352 < 0.0F) {
        state.at1352 = 0.0F;
      }
    }
    if (inputs.cmd44 < 0.0F) {
      state.at308 -= command308;
      state.at1356 += step;
      if (state.at308 < kLowerLimit) {
        state.at308 = kLowerLimit;
      }
    }
    else {
      state.at1356 = std::fmaf(-step, kTimerDecayFactor, state.at1356);
      if (state.at1356 < 0.0F) {
        state.at1356 = 0.0F;
      }
    }
  }
  else {
    state.at308 = decay(state.at308, decay308);
  }

  state.at312 = driven(inputs.cmd40)
      ? lag_to_curve(state.at312, inputs.cmd40, command312)
      : decay(state.at312, decay312);
  return state;
}

}  // namespace ac6::retail
