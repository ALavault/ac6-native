// Regression tests for the flight control update of 0x82302DB0.
//
// The micro-execution differential (tools/audit_flight_controls_microexec.py)
// is what proves the port matches retail: 23 cases, 184 float values, bit for
// bit. These are the cases that keep it that way without a Ghidra run, and each
// control at the end is a specific way to get this function wrong.

#include "ac6/retail_flight_controls.h"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::printf("FAIL  %s\n", what);
    ++failures;
  }
}

void check_bits(float actual, float expected, const char* what) {
  if (std::signbit(actual) != std::signbit(expected) || !(actual == expected)) {
    std::printf("FAIL  %s  (got %.9g, want %.9g)\n", what, actual, expected);
    ++failures;
  }
}

using ac6::retail::FlightControlInputs;
using ac6::retail::FlightControlState;
using ac6::retail::kAt304LowerLimit;
using ac6::retail::kAxisRateScaleBit;
using ac6::retail::kRampDecayFactor;
using ac6::retail::kRampRate;
using ac6::retail::kSecondRampGate;
using ac6::retail::update_flight_controls;

constexpr float kFrame = 0.016666668F;

FlightControlState run(FlightControlState state, FlightControlInputs inputs,
                       float step = kFrame) {
  return update_flight_controls(state, inputs, step);
}

void the_constants_are_the_words_of_the_image() {
  check_bits(kRampRate, 3.3333332538604736F, "10/3 at 0x82007B90");
  check_bits(kRampRate, static_cast<float>(10.0 / 3.0), "and it is 10/3");
  check_bits(kSecondRampGate, 0.9900000095367432F, "0.99 at 0x82069E50");
  check_bits(kAt304LowerLimit, -0.8999999761581421F, "-0.9 at 0x82007F84");
  check(kAxisRateScaleBit == 4U, "the rate-scale bit is bit 2");
}

void a_ramp_rises_while_held_and_falls_at_half_that_rate() {
  const FlightControlState risen = run({}, FlightControlInputs{.hold48 = 1.0F});
  // Held from zero: one decay (clamped at zero) then one rise.
  check_bits(risen.at360, kFrame * kRampRate, "held from zero rises one step");

  FlightControlState state{};
  state.at360 = 0.5F;
  const FlightControlState fallen = run(state, FlightControlInputs{});
  check_bits(fallen.at360, 0.5F - (kFrame * kRampRate) * kRampDecayFactor,
             "unheld falls by half the rise rate");
}

void a_ramp_is_bounded_by_zero_and_one() {
  FlightControlState low{};
  low.at360 = 1.0e-9F;
  check_bits(run(low, FlightControlInputs{}).at360, 0.0F,
             "the decay stops at zero");

  FlightControlState high{};
  high.at360 = 0.999F;
  check_bits(run(high, FlightControlInputs{.hold48 = 1.0F}).at360, 1.0F,
             "the rise saturates at one");
}

void the_secondary_ramp_is_assigned_zero_when_its_gate_is_shut() {
  // Not "decays to zero" -- ASSIGNED. With a half-full secondary and a primary
  // below the gate, a decaying implementation leaves 0.4x and this leaves 0.
  FlightControlState state{};
  state.at360 = 0.5F;
  state.at368 = 0.5F;
  check_bits(run(state, FlightControlInputs{}).at368, 0.0F,
             "a shut gate assigns zero rather than decaying");
}

void both_primaries_live_kills_both_secondaries() {
  // The interlock, and it reads the opposite way from the obvious guess: the
  // stores run only when NEITHER primary is zero.
  FlightControlState state{};
  state.at360 = 1.0F;
  state.at364 = 1.0F;
  state.at368 = 0.5F;
  state.at372 = 0.5F;
  const FlightControlState both =
      run(state, FlightControlInputs{.hold48 = 1.0F, .hold52 = 1.0F});
  check_bits(both.at368, 0.0F, "both primaries live zeroes at368");
  check_bits(both.at372, 0.0F, "both primaries live zeroes at372");

  // With only one primary live, the secondary survives.
  FlightControlState one{};
  one.at360 = 1.0F;
  one.at368 = 0.5F;
  const FlightControlState kept = run(one, FlightControlInputs{.hold48 = 1.0F});
  check(kept.at368 > 0.0F, "one primary at zero leaves its secondary alive");
}

void at376_is_the_difference_of_the_updated_primaries() {
  FlightControlState state{};
  state.at360 = 0.75F;
  state.at364 = 0.25F;
  const FlightControlState out = run(state, FlightControlInputs{});
  check_bits(out.at376, out.at360 - out.at364,
             "at376 is the difference AFTER both updates, not before");
}

void an_axis_centres_without_crossing_zero() {
  FlightControlState positive{};
  positive.at304 = 1.0e-9F;
  check_bits(run(positive, FlightControlInputs{}).at304, 0.0F,
             "a tiny positive axis lands on zero, not below");

  FlightControlState negative{};
  negative.at312 = -1.0e-9F;
  check_bits(run(negative, FlightControlInputs{}).at312, 0.0F,
             "a tiny negative axis lands on zero, not above");
}

void at304_is_asymmetric_in_gain_and_in_limit() {
  const FlightControlState up = run({}, FlightControlInputs{.cmd36 = 0.8F});
  const FlightControlState down = run({}, FlightControlInputs{.cmd36 = -0.8F});
  // Gain 1.0 up, 0.9 down: the magnitudes must NOT match.
  check(std::fabs(up.at304) != std::fabs(down.at304),
        "the gain is asymmetric: 1.0 up, 0.9 down");

  FlightControlState state{};
  state.at304 = -0.89F;
  const FlightControlState clamped =
      run(state, FlightControlInputs{.cmd36 = -1.0F}, 1.0F);
  check_bits(clamped.at304, kAt304LowerLimit,
             "the lower limit is -0.9, not -1.0");
}

void at308_uses_the_sign_of_its_command_and_not_its_magnitude() {
  FlightControlState state{};
  state.at308 = 0.2F;
  const FlightControlState small = run(state, FlightControlInputs{.cmd44 = 0.01F});
  const FlightControlState large = run(state, FlightControlInputs{.cmd44 = 900.0F});
  check_bits(small.at308, large.at308,
             "at308 ignores the magnitude of cmd44 entirely");
  check(small.at308 > 0.2F, "and a positive command still moves it up");
}

void bit_two_and_only_bit_two_scales_the_axis_rates() {
  FlightControlInputs inputs{};
  inputs.cmd36 = 0.5F;
  inputs.field344 = 4.0F;

  inputs.flags332 = 0U;
  const float unscaled = run({}, inputs).at304;
  inputs.flags332 = kAxisRateScaleBit;
  const float scaled = run({}, inputs).at304;
  check(scaled != unscaled, "bit 2 changes the axis rate");
  check(std::fabs(scaled) < std::fabs(unscaled),
        "and (4.0 + 0.1) * 10 > 1, so it reduces authority");

  inputs.flags332 = 0xFFFBU;             // every bit but bit 2
  check_bits(run({}, inputs).at304, unscaled, "no other bit has this effect");
}

// ---------------------------------------------------------------------------
// Controls: each is a plausible misreading, and each must disagree.
// ---------------------------------------------------------------------------

void the_controls_all_bite() {
  int interlock_inverted = 0;
  int gate_decays = 0;
  int at308_scaled = 0;
  int symmetric_gain = 0;

  for (int i = 0; i < 64; ++i) {
    const float t = static_cast<float>(i) / 64.0F;
    FlightControlState state{};
    state.at360 = t;
    state.at364 = 1.0F - t;
    state.at368 = 0.5F;
    state.at372 = 0.5F;
    state.at304 = t - 0.5F;
    state.at308 = 0.5F - t;
    FlightControlInputs inputs{};
    inputs.cmd36 = t - 0.5F;
    inputs.cmd44 = t - 0.5F;
    inputs.hold48 = t > 0.5F ? 1.0F : 0.0F;
    inputs.hold52 = 1.0F;
    const FlightControlState real = run(state, inputs);

    // 1. The interlock read as "either primary zero -> kill" instead of "both
    //    non-zero -> kill".
    const bool inverted_kill = (state.at360 == 0.0F || state.at364 == 0.0F);
    const bool real_kill = (real.at360 != 0.0F && real.at364 != 0.0F);
    if (inverted_kill != real_kill) {
      ++interlock_inverted;
    }

    // 2. A shut gate decaying instead of assigning zero.
    if (real.at360 <= kSecondRampGate && 0.5F - (kFrame * 10.0F) * 0.8F > 0.0F) {
      ++gate_decays;
    }

    // 3. at308 scaling by its command instead of using the sign.
    FlightControlInputs bigger = inputs;
    bigger.cmd44 = inputs.cmd44 * 50.0F;
    if (run(state, bigger).at308 != real.at308) {
      ++at308_scaled;
    }

    // 4. Symmetric gain on at304.
    FlightControlInputs mirrored = inputs;
    mirrored.cmd36 = -inputs.cmd36;
    if (std::fabs(run(state, mirrored).at304) == std::fabs(real.at304) &&
        inputs.cmd36 != 0.0F) {
      ++symmetric_gain;
    }
  }
  std::printf("controls: interlock-inverted=%d gate-decays=%d "
              "at308-scaled=%d symmetric-gain=%d\n",
              interlock_inverted, gate_decays, at308_scaled, symmetric_gain);
  check(interlock_inverted > 0,
        "CONTROL an inverted interlock must disagree somewhere");
  check(gate_decays > 0, "CONTROL the shut-gate cases must exist in the sweep");
  check(at308_scaled == 0,
        "CONTROL scaling cmd44 must NOT change at308 -- it is sign-only");
  check(symmetric_gain == 0,
        "CONTROL mirroring cmd36 must NOT give an equal magnitude");
}

}  // namespace

int main() {
  the_constants_are_the_words_of_the_image();
  a_ramp_rises_while_held_and_falls_at_half_that_rate();
  a_ramp_is_bounded_by_zero_and_one();
  the_secondary_ramp_is_assigned_zero_when_its_gate_is_shut();
  both_primaries_live_kills_both_secondaries();
  at376_is_the_difference_of_the_updated_primaries();
  an_axis_centres_without_crossing_zero();
  at304_is_asymmetric_in_gain_and_in_limit();
  at308_uses_the_sign_of_its_command_and_not_its_magnitude();
  bit_two_and_only_bit_two_scales_the_axis_rates();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_flight_controls: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_flight_controls: all cases passed\n");
  return 0;
}
