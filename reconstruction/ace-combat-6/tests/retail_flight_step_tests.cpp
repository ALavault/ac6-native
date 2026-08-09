// Regression tests for the flight position integrator of 0x82303110.
//
// Every case tests a point where a WRONG rule and the right one diverge, and
// each of the three controls at the end reproduces a specific way to get this
// function wrong. A control that cannot fail is not a control -- cycle 1354's
// binding-layer suite is the model.

#include "ac6/retail_flight_step.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::printf("FAIL  %s\n", what);
    ++failures;
  }
}

void check_bits(float actual, float expected, const char* what) {
  // Bit equality, not a tolerance: the whole point of the fused forms is the
  // last ulp, and a tolerance would hide exactly what these cases pin.
  if (std::signbit(actual) != std::signbit(expected) || !(actual == expected)) {
    std::printf("FAIL  %s  (got %.9g, want %.9g)\n", what, actual, expected);
    ++failures;
  }
}

using ac6::retail::FlightPosition;
using ac6::retail::FlightRates;
using ac6::retail::integrate_flight_position;
using ac6::retail::kMidFloor;
using ac6::retail::kRateToStep;
using ac6::retail::scaled_rates;

// The retail sequence, transcribed independently of the port, as the oracle for
// the arithmetic cases. If this and the port were written from one expression
// the suite would only prove the expression equals itself -- the 28th shape.
FlightPosition retail_reference(FlightPosition p, FlightRates r, float scale,
                                float bias, float step) {
  const float s64 = scale * r.to64;
  float s68 = scale * r.to68;
  const float s72 = scale * r.to72;
  s68 = std::fmaf(-1.0F, bias, s68);
  FlightPosition out{};
  out.at72 = std::fmaf(s72 * step, kRateToStep, p.at72);
  out.at68 = std::fmaf(s68 * step, kRateToStep, p.at68);
  out.at64 = std::fmaf(s64 * step, kRateToStep, p.at64);
  if (out.at68 < kMidFloor) {
    out.at68 = kMidFloor;
  }
  return out;
}

// One generator, shared by the sweep and by the controls, so the two can never
// drift onto different domains. Cycle 1372 shipped a first version whose values
// were all exactly representable: fused and unfused agreed on every one of the
// 1,215 points and BOTH float controls reported zero disagreements. The guard
// `CONTROL ... must disagree` is the only reason that was noticed, and the fix
// is inputs with full mantissas -- 0.1F and 13.7F are not exact in binary, and
// a position near 4·10^3 pushes the product's low bits under the sum's ulp.
struct SweepPoint {
  ac6::retail::FlightPosition start;
  ac6::retail::FlightRates rates;
  float scale;
  float bias;
  float step;
};

SweepPoint sweep_point(int i, int j) {
  SweepPoint point{};
  point.rates = FlightRates{static_cast<float>(i) * 13.7F + 0.33333331F,
                            static_cast<float>(i) * -3.7F + 907.3F,
                            static_cast<float>(i) * 0.27F + 1.7F};
  point.start = FlightPosition{static_cast<float>(i) * 1234.5677F + 4321.7F,
                               static_cast<float>(i) * 7.3F + 4111.3F,
                               static_cast<float>(-i) * 61.7F - 9.3F};
  point.scale = 1.0F + static_cast<float>(j) * 0.1F;
  point.bias = static_cast<float>(i) * 0.7F + 0.1F;
  point.step = static_cast<float>(j) * 0.016666668F + 0.0033333334F;
  return point;
}

void the_constants_are_the_words_of_the_image() {
  // 0x82069B40 and 0x82003214, byte for byte.
  check_bits(kRateToStep, 0.2777777910232544F, "1/3.6 is the image's word");
  check_bits(kMidFloor, 10.0F, "the floor is the image's 10.0");
  // And 1/3.6 in single precision is NOT the double 1.0/3.6 rounded by accident
  // -- it is what the image stores, so the two must agree exactly.
  check_bits(kRateToStep, static_cast<float>(1.0 / 3.6),
             "1/3.6 rounds to the stored word");
}

void a_zero_step_moves_nothing_except_through_the_floor() {
  const FlightPosition start{123.5F, 400.25F, -77.0F};
  const FlightRates rates{9.0F, -4.0F, 31.0F};
  const FlightPosition out =
      integrate_flight_position(start, rates, 2.0F, 0.5F, 0.0F);
  check_bits(out.at64, start.at64, "zero step leaves at64");
  check_bits(out.at68, start.at68, "zero step leaves at68");
  check_bits(out.at72, start.at72, "zero step leaves at72");
}

void each_rate_reaches_its_own_component() {
  // Distinct sentinels per axis: a port that crossed two components passes a
  // symmetric fixture and fails this one.
  const FlightPosition start{1000.0F, 1000.0F, 1000.0F};
  const FlightPosition out = integrate_flight_position(
      start, FlightRates{36.0F, 72.0F, 108.0F}, 1.0F, 0.0F, 1.0F);
  // rate * 1 * (1/3.6): 36 -> 10, 72 -> 20, 108 -> 30, to within the stored
  // constant, so compare against the reference rather than round numbers.
  const FlightPosition want = retail_reference(
      start, FlightRates{36.0F, 72.0F, 108.0F}, 1.0F, 0.0F, 1.0F);
  check_bits(out.at64, want.at64, "rate to64 reaches at64");
  check_bits(out.at68, want.at68, "rate to68 reaches at68");
  check_bits(out.at72, want.at72, "rate to72 reaches at72");
  check(out.at64 != out.at68 && out.at68 != out.at72,
        "the three components stay distinct");
}

void the_bias_corrects_the_middle_rate_and_no_other() {
  const FlightPosition start{500.0F, 500.0F, 500.0F};
  const FlightRates rates{10.0F, 10.0F, 10.0F};
  const FlightPosition without =
      integrate_flight_position(start, rates, 1.0F, 0.0F, 1.0F);
  const FlightPosition with =
      integrate_flight_position(start, rates, 1.0F, 4.0F, 1.0F);
  check_bits(with.at64, without.at64, "the bias leaves at64 alone");
  check_bits(with.at72, without.at72, "the bias leaves at72 alone");
  check(with.at68 < without.at68, "the bias subtracts from the middle rate");
}

void the_floor_applies_to_the_middle_component_only() {
  // Start all three below 10 with no motion: only at68 is lifted.
  const FlightPosition start{1.0F, 1.0F, 1.0F};
  const FlightPosition out =
      integrate_flight_position(start, FlightRates{}, 1.0F, 0.0F, 1.0F);
  check_bits(out.at64, 1.0F, "the floor does not touch at64");
  check_bits(out.at72, 1.0F, "the floor does not touch at72");
  check_bits(out.at68, kMidFloor, "the floor lifts at68");
}

void the_floor_is_strict_less_than() {
  // Exactly 10.0 must pass through untouched -- `bge` is taken at equality.
  // Distinguishing `<` from `<=` is invisible anywhere except at the boundary.
  const FlightPosition at_floor{0.0F, kMidFloor, 0.0F};
  const FlightPosition out =
      integrate_flight_position(at_floor, FlightRates{}, 1.0F, 0.0F, 1.0F);
  check_bits(out.at68, kMidFloor, "exactly 10.0 is not rewritten");

  const float below = std::nextafterf(kMidFloor, 0.0F);
  const FlightPosition just_below{0.0F, below, 0.0F};
  check_bits(
      integrate_flight_position(just_below, FlightRates{}, 1.0F, 0.0F, 1.0F)
          .at68,
      kMidFloor, "one ulp below 10.0 is lifted");
}

void a_nan_takes_the_branch_and_keeps_its_nan() {
  // `bge` branches when LT is false. An unordered fcmpu clears LT, so the
  // branch is taken and the store is skipped. `!(x >= floor)` would apply the
  // floor here and be wrong.
  const FlightPosition start{0.0F, std::numeric_limits<float>::quiet_NaN(),
                             0.0F};
  const FlightPosition out =
      integrate_flight_position(start, FlightRates{}, 1.0F, 0.0F, 1.0F);
  check(std::isnan(out.at68), "a NaN middle component is not floored");
}

void the_integration_matches_the_reference_over_a_sweep() {
  int compared = 0;
  for (int i = -40; i <= 40; ++i) {
    for (int j = -7; j <= 7; ++j) {
      const SweepPoint p = sweep_point(i, j);
      const FlightPosition got = integrate_flight_position(
          p.start, p.rates, p.scale, p.bias, p.step);
      const FlightPosition want =
          retail_reference(p.start, p.rates, p.scale, p.bias, p.step);
      check_bits(got.at64, want.at64, "sweep at64");
      check_bits(got.at68, want.at68, "sweep at68");
      check_bits(got.at72, want.at72, "sweep at72");
      ++compared;
    }
  }
  check(compared == 81 * 15, "the sweep ran its whole domain");
}

// ---------------------------------------------------------------------------
// Controls. Each is a plausible wrong port, and each must disagree with the
// reference somewhere in the sweep. A control that passes is a test that proves
// nothing, and saying so out loud is the point.
// ---------------------------------------------------------------------------

FlightPosition control_unfused(FlightPosition p, FlightRates r, float scale,
                               float bias, float step) {
  const float s64 = scale * r.to64;
  const float s68 = scale * r.to68 - bias;      // separate rounding
  const float s72 = scale * r.to72;
  FlightPosition out{};
  out.at72 = s72 * step * kRateToStep + p.at72; // multiply then add
  out.at68 = s68 * step * kRateToStep + p.at68;
  out.at64 = s64 * step * kRateToStep + p.at64;
  if (out.at68 < kMidFloor) {
    out.at68 = kMidFloor;
  }
  return out;
}

FlightPosition control_floor_on_all_three(FlightPosition p, FlightRates r,
                                          float scale, float bias, float step) {
  FlightPosition out = retail_reference(p, r, scale, bias, step);
  if (out.at64 < kMidFloor) {
    out.at64 = kMidFloor;
  }
  if (out.at72 < kMidFloor) {
    out.at72 = kMidFloor;
  }
  return out;
}

FlightPosition control_double_precision(FlightPosition p, FlightRates r,
                                        float scale, float bias, float step) {
  const double k = kRateToStep;
  FlightPosition out{};
  out.at64 = static_cast<float>(
      static_cast<double>(scale) * r.to64 * step * k + p.at64);
  out.at68 = static_cast<float>(
      (static_cast<double>(scale) * r.to68 - bias) * step * k + p.at68);
  out.at72 = static_cast<float>(
      static_cast<double>(scale) * r.to72 * step * k + p.at72);
  if (out.at68 < kMidFloor) {
    out.at68 = kMidFloor;
  }
  return out;
}

void the_controls_all_bite() {
  int unfused = 0;
  int floored = 0;
  int doubled = 0;
  for (int i = -40; i <= 40; ++i) {
    for (int j = -7; j <= 7; ++j) {
      const SweepPoint p = sweep_point(i, j);
      const FlightPosition want =
          retail_reference(p.start, p.rates, p.scale, p.bias, p.step);
      if (!(control_unfused(p.start, p.rates, p.scale, p.bias, p.step) ==
            want)) {
        ++unfused;
      }
      if (!(control_floor_on_all_three(p.start, p.rates, p.scale, p.bias,
                                       p.step) == want)) {
        ++floored;
      }
      if (!(control_double_precision(p.start, p.rates, p.scale, p.bias,
                                     p.step) == want)) {
        ++doubled;
      }
    }
  }
  std::printf("controls: unfused=%d floor-on-all=%d double-precision=%d\n",
              unfused, floored, doubled);
  check(unfused > 0, "CONTROL multiply-then-add must disagree");
  check(floored > 0, "CONTROL flooring all three must disagree");
  check(doubled > 0, "CONTROL double precision must disagree");
}

void scaled_rates_is_the_same_arithmetic_the_integrator_uses() {
  // The integrator and the direction output share these three values, so the
  // helper must not be a second, subtly different copy.
  const FlightRates rates{7.5F, -2.25F, 60.0F};
  const FlightRates scaled = scaled_rates(rates, 3.0F, 1.5F);
  check_bits(scaled.to64, 3.0F * 7.5F, "scaled to64");
  check_bits(scaled.to72, 3.0F * 60.0F, "scaled to72");
  check_bits(scaled.to68, std::fmaf(-1.0F, 1.5F, 3.0F * -2.25F), "scaled to68");

  // And integrating with a step of exactly 1 and a zero start reproduces
  // scaled * 1/3.6 on every axis, which ties the two entry points together.
  const FlightPosition out =
      integrate_flight_position(FlightPosition{0.0F, 1.0e9F, 0.0F}, rates, 3.0F,
                                1.5F, 1.0F);
  check_bits(out.at64, std::fmaf(scaled.to64 * 1.0F, kRateToStep, 0.0F),
             "integrate agrees with scaled_rates on at64");
  check_bits(out.at72, std::fmaf(scaled.to72 * 1.0F, kRateToStep, 0.0F),
             "integrate agrees with scaled_rates on at72");
}

}  // namespace

int main() {
  the_constants_are_the_words_of_the_image();
  a_zero_step_moves_nothing_except_through_the_floor();
  each_rate_reaches_its_own_component();
  the_bias_corrects_the_middle_rate_and_no_other();
  the_floor_applies_to_the_middle_component_only();
  the_floor_is_strict_less_than();
  a_nan_takes_the_branch_and_keeps_its_nan();
  the_integration_matches_the_reference_over_a_sweep();
  scaled_rates_is_the_same_arithmetic_the_integrator_uses();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_flight_step: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_flight_step: all cases passed\n");
  return 0;
}
