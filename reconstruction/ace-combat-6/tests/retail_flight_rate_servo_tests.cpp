// Regression tests for the rate servo of 0x822831E8.

#include "ac6/retail_flight_rate_servo.h"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("FAIL  %s\n", w); ++failures; } }
void check_bits(float a, float b, const char* w) {
  if (std::signbit(a) != std::signbit(b) || !(a == b)) {
    std::printf("FAIL  %s  (got %.9g, want %.9g)\n", w, a, b); ++failures;
  }
}

using ac6::retail::FlightRates3;
using ac6::retail::kBias144;
using ac6::retail::kBias148;
using ac6::retail::kBias152;
using ac6::retail::kServoEpsilon;
using ac6::retail::RateServoAxis;
using ac6::retail::update_flight_rate_servo;

constexpr float kFrame = 0.016666668F;

RateServoAxis axis(float a, float limit = 5.0F) {
  RateServoAxis out{};
  out.axis = a;
  out.limit = limit;
  out.driven_gain = 2.0F;
  out.centred_gain = 3.0F;
  return out;
}

void the_three_biases_are_different() {
  check_bits(kBias144, 0.5F, "0.5 at 0x82001354");
  check_bits(kBias148, 0.800000011920929F, "0.8 at 0x82069ECC");
  check_bits(kBias152, 0.4000000059604645F, "0.4 at 0x82069CB4");
  check(kBias144 != kBias148 && kBias148 != kBias152,
        "they are three different constants, not one used three times");
}

void a_centred_axis_uses_the_unscaled_gain() {
  // |axis| < eps takes the centred branch, and that gain is NOT scaled by the
  // saturation term. This is the switch that reads the other way round from the
  // obvious guess.
  FlightRates3 rates{1.0F, 0.0F, 0.0F};
  const FlightRates3 out = update_flight_rate_servo(
      rates, axis(0.0F), axis(0.0F), axis(0.0F), kFrame);
  const float gap = std::fmaf(0.0F, 5.0F, -1.0F);
  check_bits(out.at144, std::fmaf(3.0F * gap, kFrame, 1.0F),
             "the centred gain is used plain");
}

void a_driven_axis_scales_its_gain_by_the_saturation() {
  FlightRates3 rates{2.5F, 0.0F, 0.0F};
  const FlightRates3 out = update_flight_rate_servo(
      rates, axis(1.0F), axis(0.0F), axis(0.0F), kFrame);
  float sat = std::fabs(2.5F / 5.0F) + kBias144;
  if (sat > 1.0F) { sat = 1.0F; }
  const float gap = std::fmaf(1.0F, 5.0F, -2.5F);
  check_bits(out.at144, std::fmaf(2.0F * sat * gap, kFrame, 2.5F),
             "the driven gain is scaled by the saturation term");
}

void the_saturation_is_clamped_to_one() {
  // |rate/limit| = 1 plus a bias of 0.5 is 1.5, and the clamp brings it to 1.
  FlightRates3 rates{5.0F, 0.0F, 0.0F};
  const FlightRates3 out = update_flight_rate_servo(
      rates, axis(1.0F), axis(0.0F), axis(0.0F), kFrame);
  const float gap = std::fmaf(1.0F, 5.0F, -5.0F);
  check_bits(out.at144, std::fmaf(2.0F * 1.0F * gap, kFrame, 5.0F),
             "the saturation stops at one");
}

void a_degenerate_limit_gives_a_saturation_of_the_bias_alone() {
  // The fallback block at 0x826EB940 is all zeros, so the normalised magnitude
  // is zero rather than a division guard.
  FlightRates3 rates{0.25F, 0.0F, 0.0F};
  RateServoAxis degenerate = axis(1.0F, kServoEpsilon * 0.5F);
  const FlightRates3 out = update_flight_rate_servo(
      rates, degenerate, axis(0.0F), axis(0.0F), kFrame);
  const float gap = std::fmaf(1.0F, degenerate.limit, -0.25F);
  float value = std::fmaf(2.0F * kBias144 * gap, kFrame, 0.25F);
  if (value > degenerate.limit) { value = degenerate.limit; }
  check_bits(out.at144, value, "a degenerate limit uses the bias alone");
}

void the_rate_is_clamped_to_the_limit() {
  FlightRates3 rates{};
  const FlightRates3 up = update_flight_rate_servo(
      rates, axis(1.0F), axis(-1.0F), axis(0.0F), 1000.0F);
  check_bits(up.at144, 5.0F, "the rate stops at +limit");
  check_bits(up.at148, -5.0F, "and at -limit");
}

void each_axis_is_independent() {
  FlightRates3 rates{};
  const FlightRates3 out = update_flight_rate_servo(
      rates, axis(1.0F), axis(0.0F), axis(0.0F), kFrame);
  check(out.at144 != 0.0F, "the first axis moves");
  check_bits(out.at148, 0.0F, "and does not reach the second");
  check_bits(out.at152, 0.0F, "nor the third");
}

void the_saturations_are_computed_before_any_servo_runs() {
  // All three read their rate BEFORE any is updated. Interleaving would use a
  // rate retail has not yet written; with equal inputs the three outputs must
  // still differ only by their biases.
  FlightRates3 rates{2.0F, 2.0F, 2.0F};
  const FlightRates3 out = update_flight_rate_servo(
      rates, axis(1.0F), axis(1.0F), axis(1.0F), kFrame);
  check(out.at144 != out.at148 && out.at148 != out.at152,
        "the three differ, and only their biases differ");
}

void the_controls_all_bite() {
  int shared_bias = 0, swapped_gain = 0;
  for (int i = -16; i <= 16; ++i) {
    const float a = static_cast<float>(i) / 16.0F;
    FlightRates3 rates{a * 4.0F, a * 4.0F, a * 4.0F};
    const FlightRates3 real = update_flight_rate_servo(
        rates, axis(a), axis(a), axis(a), kFrame);
    // 1. One bias for all three axes.
    if (real.at144 != real.at148 || real.at148 != real.at152) {
      ++shared_bias;
    }
    // 2. The saturation scaling the CENTRED gain instead of the driven one.
    if (std::fabs(a) >= kServoEpsilon) {
      float sat = std::fabs(rates.at144 / 5.0F) + kBias144;
      if (sat > 1.0F) { sat = 1.0F; }
      const float gap = std::fmaf(a, 5.0F, -rates.at144);
      float swapped = std::fmaf(2.0F * gap, kFrame, rates.at144);
      if (swapped < -5.0F) { swapped = -5.0F; }
      else if (swapped > 5.0F) { swapped = 5.0F; }
      if (swapped != real.at144) {
        ++swapped_gain;
      }
    }
  }
  std::printf("controls: shared-bias=%d unscaled-driven-gain=%d\n",
              shared_bias, swapped_gain);
  check(shared_bias > 0, "CONTROL one shared bias must disagree");
  check(swapped_gain > 0,
        "CONTROL leaving the driven gain unscaled must disagree");
}

}  // namespace

int main() {
  the_three_biases_are_different();
  a_centred_axis_uses_the_unscaled_gain();
  a_driven_axis_scales_its_gain_by_the_saturation();
  the_saturation_is_clamped_to_one();
  a_degenerate_limit_gives_a_saturation_of_the_bias_alone();
  the_rate_is_clamped_to_the_limit();
  each_axis_is_independent();
  the_saturations_are_computed_before_any_servo_runs();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_flight_rate_servo: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_flight_rate_servo: all cases passed\n");
  return 0;
}
