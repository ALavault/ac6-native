#include "ac6/retail_flight_input_apply.h"

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
using namespace ac6::retail;

void each_field_reaches_one_accumulator() {
  FlightInputFields f{};
  f.at2104 = 0.25F;
  const FlightInputAccumulators out = apply_flight_input({}, f);
  check_bits(out.at36, 0.25F, "+2104 -> +36");
  check_bits(out.at40, 0.0F, "and reaches nothing else");
  check_bits(out.at44, 0.0F, "");
  check_bits(out.at48, 0.0F, "");
  check_bits(out.at52, 0.0F, "");

  f = FlightInputFields{}; f.at2108 = 0.25F;
  check_bits(apply_flight_input({}, f).at40, 0.25F, "+2108 -> +40");
  f = FlightInputFields{}; f.at2096 = 0.25F;
  check_bits(apply_flight_input({}, f).at48, 0.25F, "+2096 -> +48");
  f = FlightInputFields{}; f.at2100 = 0.25F;
  check_bits(apply_flight_input({}, f).at52, 0.25F, "+2100 -> +52");
}

void the_third_axis_is_a_difference() {
  // THE detail. Two fields, subtracted, and only for +44.
  FlightInputFields f{};
  f.at2112 = 0.75F;
  f.at2116 = 0.25F;
  check_bits(apply_flight_input({}, f).at44, 0.5F, "+44 gets 2112 - 2116");

  f.at2112 = 0.25F; f.at2116 = 0.75F;
  check_bits(apply_flight_input({}, f).at44, -0.5F, "and the sign follows");

  f.at2112 = f.at2116 = 0.5F;
  check_bits(apply_flight_input({}, f).at44, 0.0F,
             "equal fields cancel -- a pass-through port would give 0.5");
}

void the_holds_clamp_at_zero_and_the_axes_at_minus_one() {
  FlightInputFields f{};
  f.at2096 = -0.5F;   // a hold, driven negative
  f.at2104 = -0.5F;   // an axis, the same
  FlightInputAccumulators state{};
  const FlightInputAccumulators out = apply_flight_input(state, f);
  check_bits(out.at48, 0.0F, "the hold stops at zero");
  check_bits(out.at36, -0.5F, "the axis does not");
}

void it_accumulates_across_frames() {
  FlightInputFields f{};
  f.at2104 = 0.25F;
  FlightInputAccumulators state{};
  for (int i = 0; i < 3; ++i) { state = apply_flight_input(state, f); }
  check_bits(state.at36, 0.75F, "three frames accumulate");
  for (int i = 0; i < 10; ++i) { state = apply_flight_input(state, f); }
  check_bits(state.at36, 1.0F, "and then saturate at the accumulator's clamp");
}

void the_controls_all_bite() {
  int passthrough = 0, hold_went_negative = 0;
  for (int i = -16; i <= 16; ++i) {
    FlightInputFields f{};
    f.at2112 = static_cast<float>(i) / 16.0F;
    f.at2116 = 0.25F;
    f.at2096 = static_cast<float>(i) / 16.0F;
    const FlightInputAccumulators out = apply_flight_input({}, f);
    if (out.at44 != f.at2112) { ++passthrough; }
    if (out.at48 < 0.0F) { ++hold_went_negative; }
  }
  std::printf("controls: difference-vs-passthrough=%d hold-negative=%d\n",
              passthrough, hold_went_negative);
  check(passthrough > 0, "CONTROL a pass-through for +44 must disagree");
  check(hold_went_negative == 0, "CONTROL a hold must never be negative");
}
}  // namespace

int main() {
  each_field_reaches_one_accumulator();
  the_third_axis_is_a_difference();
  the_holds_clamp_at_zero_and_the_axes_at_minus_one();
  it_accumulates_across_frames();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_flight_input_apply: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_flight_input_apply: all cases passed\n");
  return 0;
}
