#include "ac6/retail_flight_input_accumulators.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("FAIL  %s\n", w); ++failures; } }
void check_bits(float a, float b, const char* w) {
  if (std::signbit(a) != std::signbit(b) || !(a == b)) {
    std::printf("FAIL  %s  (got %.9g, want %.9g)\n", w, a, b); ++failures;
  }
}
using ac6::retail::accumulate_flight_input;
using ac6::retail::kAxisLowerLimit;
using ac6::retail::kHoldLowerLimit;
using ac6::retail::kInputUpperLimit;

void the_constants_are_the_words_of_the_image() {
  check_bits(kInputUpperLimit, 1.0F, "1.0 at 0x82001348");
  check_bits(kAxisLowerLimit, -1.0F, "-1.0 at 0x82069B28");
  check_bits(kHoldLowerLimit, 0.0F, "0.0 at 0x8200082C");
}

void it_accumulates() {
  check_bits(accumulate_flight_input(0.25F, 0.25F, kAxisLowerLimit), 0.5F,
             "the increment is added");
  check_bits(accumulate_flight_input(0.5F, -0.25F, kAxisLowerLimit), 0.25F,
             "and a negative increment subtracts");
}

void the_axes_clamp_to_minus_one_and_the_holds_to_zero() {
  check_bits(accumulate_flight_input(-0.9F, -0.5F, kAxisLowerLimit), -1.0F,
             "an axis stops at -1");
  check_bits(accumulate_flight_input(0.1F, -0.5F, kHoldLowerLimit), 0.0F,
             "a hold stops at 0");
  check(accumulate_flight_input(0.1F, -0.5F, kAxisLowerLimit) < 0.0F,
        "and an axis with the same input goes negative");
}

void both_clamp_to_one_above() {
  check_bits(accumulate_flight_input(0.9F, 0.5F, kAxisLowerLimit), 1.0F,
             "an axis stops at +1");
  check_bits(accumulate_flight_input(0.9F, 0.5F, kHoldLowerLimit), 1.0F,
             "and so does a hold");
}

void exactly_one_passes_through() {
  // `ble` returns at the upper bound, so 1.0 is not rewritten.
  check_bits(accumulate_flight_input(0.5F, 0.5F, kAxisLowerLimit), 1.0F,
             "exactly 1.0 is returned, not re-stored");
  check_bits(accumulate_flight_input(-0.5F, -0.5F, kAxisLowerLimit), -1.0F,
             "and exactly -1.0 too");
}

void a_nan_comes_out_as_the_upper_bound() {
  // blt is false on a NaN, and so is ble, so it falls through to the upper
  // store. `x < lower` then `!(x <= upper)` reproduces that; any other ordering
  // does not.
  const float nan = std::numeric_limits<float>::quiet_NaN();
  check_bits(accumulate_flight_input(nan, 0.0F, kAxisLowerLimit),
             kInputUpperLimit, "a NaN comes out as +1.0");
}

void the_increment_is_added_first() {
  // `fadds f0,f1,f0` -- the increment is the first operand. Addition commutes in
  // IEEE for finite values, but not for signed zeros: (-0.0) + (+0.0) is +0.0
  // either way, so this pins the ORDER by construction rather than by luck.
  check_bits(accumulate_flight_input(-0.0F, 0.0F, kAxisLowerLimit), 0.0F,
             "0.0 + -0.0 is +0.0");
}

void the_controls_all_bite() {
  int wrong_lower = 0, clamped_at_equal = 0;
  for (int i = -32; i <= 32; ++i) {
    const float v = static_cast<float>(i) / 16.0F;
    const float real = accumulate_flight_input(v, 0.0F, kHoldLowerLimit);
    if (v < 0.0F && real != 0.0F) { ++wrong_lower; }
    if (v == 1.0F && real != 1.0F) { ++clamped_at_equal; }
  }
  std::printf("controls: hold-went-negative=%d clamped-at-equal=%d\n",
              wrong_lower, clamped_at_equal);
  check(wrong_lower == 0, "CONTROL a hold must never be negative");
  check(clamped_at_equal == 0, "CONTROL exactly 1.0 must pass through");
}
}  // namespace

int main() {
  the_constants_are_the_words_of_the_image();
  it_accumulates();
  the_axes_clamp_to_minus_one_and_the_holds_to_zero();
  both_clamp_to_one_above();
  exactly_one_passes_through();
  a_nan_comes_out_as_the_upper_bound();
  the_increment_is_added_first();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_flight_input_accumulators: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_flight_input_accumulators: all cases passed\n");
  return 0;
}
