// Regression tests for the step 0x82283898.

#include "ac6/retail_flight_step_driver.h"

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

using ac6::retail::apply_flight_step;
using ac6::retail::FlightControlInputs;
using ac6::retail::FlightControlState;
using ac6::retail::flight_step_position_block;
using ac6::retail::kStepResetBit;
using ac6::retail::update_flight_controls;

constexpr float kFrame = 0.016666668F;

FlightControlState seeded() {
  FlightControlState state{};
  state.at304 = 0.5F;
  state.at308 = -0.5F;
  state.at312 = 0.75F;
  state.at360 = 0.5F;
  state.at364 = 0.25F;
  state.at368 = 0.5F;
  state.at372 = 0.5F;
  return state;
}

FlightControlInputs held(std::uint32_t flags) {
  FlightControlInputs inputs{};
  inputs.hold48 = 1.0F;
  inputs.flags332 = flags;
  return inputs;
}

void the_position_block_is_the_field_plus_ninety_six() {
  check(flight_step_position_block(0xB4000000U) == 0xB4000060U,
        "r5 = [this+112] + 96");
}

void the_reset_is_bit_one() {
  check(kStepResetBit == 2U, "the reset bit is bit 1, not bit 0");

  // Bit 0 must do nothing at all: the measurement that settled this is that a
  // run with bit 0 made four stubbed calls and changed nothing, while bit 1
  // made five.
  const FlightControlState with_bit0 = apply_flight_step(seeded(), held(1U), kFrame);
  const FlightControlState with_none = apply_flight_step(seeded(), held(0U), kFrame);
  check(with_bit0 == with_none, "bit 0 is not the reset");
}

void without_the_bit_the_step_is_slot_thirty() {
  const FlightControlState direct =
      update_flight_controls(seeded(), held(0U), kFrame);
  const FlightControlState stepped = apply_flight_step(seeded(), held(0U), kFrame);
  check(direct == stepped, "with the bit clear the step is exactly slot 30");
}

void the_reset_clears_five_fields_and_spares_three() {
  const FlightControlState before =
      update_flight_controls(seeded(), held(kStepResetBit), kFrame);
  const FlightControlState after =
      apply_flight_step(seeded(), held(kStepResetBit), kFrame);

  check_bits(after.at304, 0.0F, "at304 is cleared");
  check_bits(after.at308, 0.0F, "at308 is cleared");
  check_bits(after.at312, 0.0F, "at312 is cleared");
  check_bits(after.at360, 0.0F, "at360 is cleared");
  check_bits(after.at364, 0.0F, "at364 is cleared");

  // THE ORDERING IS OBSERVABLE HERE. The three survivors hold what slot 30
  // computed, so the reset runs AFTER it and not instead of it.
  check_bits(after.at368, before.at368, "at368 keeps slot 30's value");
  check_bits(after.at372, before.at372, "at372 keeps slot 30's value");
  check_bits(after.at376, before.at376, "at376 keeps slot 30's value");
}

void the_reset_runs_after_slot_thirty_not_instead_of_it() {
  // If the reset short-circuited slot 30, at376 would be the seed's difference
  // (0.5 - 0.25 = 0.25). Slot 30 moves both ramps first, so it is not.
  const FlightControlState after =
      apply_flight_step(seeded(), held(kStepResetBit), kFrame);
  check(after.at376 != 0.25F,
        "at376 is slot 30's difference, not the seed's -- the reset does not "
        "replace slot 30");
}

void the_controls_all_bite() {
  int short_circuit = 0;
  int cleared_all_eight = 0;
  int wrong_bit = 0;
  for (int i = 0; i < 32; ++i) {
    FlightControlState state = seeded();
    state.at360 = static_cast<float>(i) / 32.0F;
    state.at364 = 1.0F - static_cast<float>(i) / 32.0F;
    const FlightControlState real =
        apply_flight_step(state, held(kStepResetBit), kFrame);

    // 1. A reset that skipped slot 30 entirely.
    FlightControlState skipped = state;
    skipped.at304 = skipped.at308 = skipped.at312 = 0.0F;
    skipped.at360 = skipped.at364 = 0.0F;
    if (!(skipped == real)) {
      ++short_circuit;
    }
    // 2. A reset that cleared all eight.
    FlightControlState all_eight = real;
    all_eight.at368 = all_eight.at372 = all_eight.at376 = 0.0F;
    if (!(all_eight == real)) {
      ++cleared_all_eight;
    }
    // 3. The reset keyed on bit 0.
    if (!(apply_flight_step(state, held(1U), kFrame) == real)) {
      ++wrong_bit;
    }
  }
  std::printf("controls: short-circuit=%d cleared-all-eight=%d wrong-bit=%d\n",
              short_circuit, cleared_all_eight, wrong_bit);
  check(short_circuit > 0, "CONTROL skipping slot 30 must disagree");
  check(cleared_all_eight > 0, "CONTROL clearing all eight must disagree");
  check(wrong_bit > 0, "CONTROL keying the reset on bit 0 must disagree");
}

}  // namespace

int main() {
  the_position_block_is_the_field_plus_ninety_six();
  the_reset_is_bit_one();
  without_the_bit_the_step_is_slot_thirty();
  the_reset_clears_five_fields_and_spares_three();
  the_reset_runs_after_slot_thirty_not_instead_of_it();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_flight_step_driver: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_flight_step_driver: all cases passed\n");
  return 0;
}
