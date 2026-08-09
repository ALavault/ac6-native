// 0x822831E8, instruction by instruction.
//
// Retail computes all three saturation terms FIRST, then runs the three servos.
// The order matters: each saturation reads its rate BEFORE any servo has
// updated one, so a straightforward per-axis loop that interleaved them would
// use a rate that retail has not yet written.

#include "ac6/retail_flight_rate_servo.h"

#include <cmath>

namespace ac6::retail {
namespace {

float saturation(float rate, float limit, float bias) noexcept {
  // `blt` is false for a NaN limit, so a NaN takes the divide path here, which
  // `!(x < eps)` reproduces and `x >= eps` does not.
  float magnitude = 0.0F;             // the block at 0x826EB940 is all zeros
  if (!(std::fabs(limit) < kServoEpsilon)) {
    magnitude = std::fabs(rate / limit);
  }
  float value = magnitude + bias;
  if (value < 0.0F) {
    value = 0.0F;
  }
  else if (value > 1.0F) {
    value = 1.0F;
  }
  return value;
}

float servo(float rate, const RateServoAxis& axis, float saturated,
            float step) noexcept {
  const float gap = std::fmaf(axis.axis, axis.limit, -rate);   // fmsubs
  const float gain = !(std::fabs(axis.axis) < kServoEpsilon)
      ? axis.driven_gain * saturated
      : axis.centred_gain;
  rate = std::fmaf(gain * gap, step, rate);                    // fmuls, fmadds
  if (rate < -axis.limit) {
    return -axis.limit;
  }
  if (rate > axis.limit) {
    return axis.limit;
  }
  return rate;
}

}  // namespace

FlightRates3 update_flight_rate_servo(FlightRates3 rates,
                                      const RateServoAxis& first,
                                      const RateServoAxis& second,
                                      const RateServoAxis& third,
                                      float step) noexcept {
  const float s144 = saturation(rates.at144, first.limit, kBias144);
  const float s148 = saturation(rates.at148, second.limit, kBias148);
  const float s152 = saturation(rates.at152, third.limit, kBias152);

  rates.at144 = servo(rates.at144, first, s144, step);
  rates.at148 = servo(rates.at148, second, s148, step);
  rates.at152 = servo(rates.at152, third, s152, step);
  return rates;
}

}  // namespace ac6::retail
