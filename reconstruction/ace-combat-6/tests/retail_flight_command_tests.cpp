#include "ac6/retail_flight_command.h"

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
using ac6::retail::FlightCommandResult;
using ac6::retail::kMinusPi;
using ac6::retail::kOneDegree;
using ac6::retail::kPi;
using ac6::retail::kSlot12;
using ac6::retail::kSlot13;
using ac6::retail::kSlot14;
using ac6::retail::kTwoPi;
using ac6::retail::set_flight_command;

void the_constants_are_the_words_of_the_image() {
  check_bits(kPi, 3.1415927410125732F, "pi at 0x82069BB0");
  check_bits(kTwoPi, 6.2831854820251465F, "2pi at 0x82069BF0");
  check_bits(kOneDegree, 0.01745329238474369F, "pi/180 at 0x82675554");
  check_bits(kOneDegree, static_cast<float>(3.14159265358979323846 / 180.0),
             "and it really is one degree");
}

void the_accumulators_are_not_in_slot_order() {
  check(kSlot12.accumulator == 36, "slot 12 accumulates at +36");
  check(kSlot13.accumulator == 44, "slot 13 at +44, not +40");
  check(kSlot14.accumulator == 40, "slot 14 at +40, not +44");
}

void a_command_within_one_degree_is_discarded() {
  const FlightCommandResult out =
      set_flight_command(0.5F, 10.0F, 7.0F, 0.5F + kOneDegree * 0.5F);
  check(!out.changed, "half a degree away changes nothing");
  check_bits(out.accumulator, 10.0F, "and the increment is DISCARDED");
}

void a_command_beyond_one_degree_is_taken() {
  const FlightCommandResult out =
      set_flight_command(0.5F, 10.0F, 7.0F, 0.5F + kOneDegree * 2.0F);
  check(out.changed, "two degrees away changes something");
  check_bits(out.accumulator, 17.0F, "and the increment is added");
}

void the_boundary_is_strictly_greater_than() {
  // `bgt` -- exactly one degree is NOT enough.
  const float target = 0.5F + kOneDegree;
  const FlightCommandResult out = set_flight_command(0.5F, 0.0F, 1.0F, target);
  const bool beyond = std::fabs(0.5F - target) > kOneDegree;
  check(out.changed == beyond, "the comparison is strict, and matches it");
}

void the_wrap_is_one_step_each_way() {
  const FlightCommandResult below =
      set_flight_command(0.0F, 0.0F, 1.0F, kMinusPi - 1.0F);
  check_bits(below.target, (kMinusPi - 1.0F) + kTwoPi, "below -pi wraps up once");
  const FlightCommandResult above =
      set_flight_command(0.0F, 0.0F, 1.0F, kPi + 1.0F);
  check_bits(above.target, (kPi + 1.0F) - kTwoPi, "above +pi wraps down once");
}

void a_far_target_stays_out_of_range() {
  // ONE step, not a modulo. Three turns out stays out.
  const float far_target = kPi + 3.0F * kTwoPi;
  const FlightCommandResult out = set_flight_command(0.0F, 0.0F, 1.0F, far_target);
  check(out.target > kPi, "a target three turns out is still out of range");
}

void exactly_pi_takes_the_sign_of_the_current_angle() {
  const FlightCommandResult positive = set_flight_command(0.5F, 0.0F, 1.0F, kPi);
  check_bits(positive.target, kPi, "a positive current angle keeps +pi");
  const FlightCommandResult negative = set_flight_command(-0.5F, 0.0F, 1.0F, kPi);
  check_bits(negative.target, kMinusPi, "a negative one flips to -pi");
}

void negative_zero_takes_the_positive_branch() {
  // fsel compares against +0.0. The input path leaves -0.0 on idle axes, which
  // is why retail_input_binding.h carries the same rule.
  const FlightCommandResult out = set_flight_command(-0.0F, 0.0F, 1.0F, kPi);
  check_bits(out.target, kPi, "-0.0 is >= 0 and takes +pi");
}

void the_controls_all_bite() {
  int always_stored = 0, modulo_wrap = 0, signbit_rule = 0;
  for (int i = -32; i <= 32; ++i) {
    const float current = static_cast<float>(i) / 32.0F * kPi;
    const float target = current + kOneDegree * (static_cast<float>(i % 3));
    const FlightCommandResult real =
        set_flight_command(current, 5.0F, 1.0F, target);
    if (!real.changed && real.accumulator != 5.0F) { ++always_stored; }
    const FlightCommandResult far =
        set_flight_command(current, 0.0F, 1.0F, kPi + 3.0F * kTwoPi);
    if (far.target <= kPi) { ++modulo_wrap; }
    if (i == 0) {
      const FlightCommandResult zero =
          set_flight_command(-0.0F, 0.0F, 1.0F, kPi);
      if (zero.target != kPi) { ++signbit_rule; }
    }
  }
  std::printf("controls: stored-when-unchanged=%d modulo-wrap=%d "
              "signbit-on-zero=%d\n", always_stored, modulo_wrap, signbit_rule);
  check(always_stored == 0, "CONTROL an unchanged call must not accumulate");
  check(modulo_wrap == 0, "CONTROL the wrap must be one step, not a modulo");
  check(signbit_rule == 0, "CONTROL -0.0 must take the positive branch");
}
}  // namespace

int main() {
  the_constants_are_the_words_of_the_image();
  the_accumulators_are_not_in_slot_order();
  a_command_within_one_degree_is_discarded();
  a_command_beyond_one_degree_is_taken();
  the_boundary_is_strictly_greater_than();
  the_wrap_is_one_step_each_way();
  a_far_target_stays_out_of_range();
  exactly_pi_takes_the_sign_of_the_current_angle();
  negative_zero_takes_the_positive_branch();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_flight_command: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_flight_command: all cases passed\n");
  return 0;
}
