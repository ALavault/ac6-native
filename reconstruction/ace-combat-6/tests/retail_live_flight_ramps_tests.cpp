// Regression tests for the LIVE flight model's ramp update, 0x82303E68.

#include "ac6/retail_live_flight_ramps.h"

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

using ac6::retail::kAt368Ceiling;
using ac6::retail::kBypassBit;
using ac6::retail::LiveRampInputs;
using ac6::retail::LiveRampState;
using ac6::retail::update_live_flight_ramps;

constexpr float kFrame = 0.016666668F;

LiveRampInputs basic() {
  LiveRampInputs inputs{};
  inputs.cmd48 = 1.0F;
  inputs.cmd52 = 1.0F;
  inputs.rate952 = 3.0F;
  inputs.rate956 = 3.0F;
  inputs.threshold404 = 0.5F;
  inputs.gate1224 = true;
  return inputs;
}

void the_flag_is_bit_seven() {
  check(kBypassBit == 128U, "the bypass is bit 7");
}

void a_lag_moves_a_fraction_of_the_remaining_distance() {
  LiveRampState state{};
  const LiveRampInputs inputs = basic();
  const LiveRampState out = update_live_flight_ramps(state, inputs, kFrame);
  // (1 - 0) * (3 * step), fused.
  check_bits(out.at360, std::fmaf(1.0F, 3.0F * kFrame, 0.0F),
             "at360 takes a fraction of the gap");
  check(out.at360 > 0.0F && out.at360 < 1.0F, "and lands between the two");
}

void the_lag_stops_when_it_arrives() {
  LiveRampState state{};
  state.at360 = 1.0F;
  state.at364 = 1.0F;
  const LiveRampState out = update_live_flight_ramps(state, basic(), kFrame);
  check_bits(out.at360, 1.0F, "at the target, at360 does not move");
  check_bits(out.at364, 1.0F, "at the target, at364 does not move");
}

void a_large_step_overshoots_because_there_is_no_clamp() {
  // THE DETAIL A TIDY PORT GETS WRONG. There is no bound on either lag. With
  // rate * step > 1 the value goes past its target, and retail lets it.
  LiveRampState state{};
  const LiveRampState out = update_live_flight_ramps(state, basic(), 1.0F);
  check(out.at360 > 1.0F, "rate 3 over a one-second step overshoots the target");
}

void each_command_drives_its_own_ramp() {
  LiveRampInputs inputs = basic();
  inputs.cmd52 = 0.0F;
  const LiveRampState out = update_live_flight_ramps({}, inputs, kFrame);
  check(out.at360 > 0.0F, "cmd48 drives at360");
  check_bits(out.at364, 0.0F, "cmd52 alone drives at364");
}

void the_secondary_ramps_follow_the_threshold() {
  LiveRampInputs inputs = basic();
  inputs.rate952 = 0.0F;
  inputs.rate956 = 0.0F;

  LiveRampState above{};
  above.at360 = 1.0F;
  above.at364 = 1.0F;
  const LiveRampState up = update_live_flight_ramps(above, inputs, kFrame);
  check_bits(up.at368, kFrame, "past the threshold, at368 rises a whole step");
  check_bits(up.at372, kFrame, "and so does at372");

  LiveRampState below{};
  below.at368 = 0.5F;
  below.at372 = 0.5F;
  const LiveRampState down = update_live_flight_ramps(below, inputs, kFrame);
  check_bits(down.at368, 0.5F - kFrame, "below it, at368 falls a whole step");
  check_bits(down.at372, 0.5F - kFrame, "and so does at372");
}

void the_secondary_ramps_are_bounded() {
  LiveRampInputs inputs = basic();
  inputs.rate952 = inputs.rate956 = 0.0F;
  LiveRampState high{};
  high.at360 = high.at364 = 1.0F;
  high.at368 = high.at372 = 1.0F;
  const LiveRampState capped = update_live_flight_ramps(high, inputs, kFrame);
  check_bits(capped.at368, kAt368Ceiling, "at368 stops at its ceiling");

  LiveRampState low{};
  low.at368 = low.at372 = 1.0e-9F;
  const LiveRampState floored = update_live_flight_ramps(low, inputs, kFrame);
  check_bits(floored.at368, 0.0F, "at368 stops at zero");
  check_bits(floored.at372, 0.0F, "at372 stops at zero");
}

void the_gate_zeroes_at368_only() {
  LiveRampInputs inputs = basic();
  inputs.rate952 = inputs.rate956 = 0.0F;
  inputs.gate1224 = false;
  LiveRampState state{};
  state.at360 = state.at364 = 1.0F;
  state.at368 = state.at372 = 0.5F;
  const LiveRampState out = update_live_flight_ramps(state, inputs, kFrame);
  check_bits(out.at368, 0.0F, "a zero gate byte clears at368");
  check(out.at372 > 0.5F, "and at372 is not gated at all");
}

void the_bypass_skips_the_lags_but_not_the_difference() {
  LiveRampInputs inputs = basic();
  inputs.flags332 = kBypassBit;
  LiveRampState state{};
  state.at360 = 0.75F;
  state.at364 = 0.25F;
  state.at368 = 0.5F;
  const LiveRampState out = update_live_flight_ramps(state, inputs, kFrame);
  check_bits(out.at360, 0.75F, "the bypass leaves at360 alone");
  check_bits(out.at368, 0.5F, "and at368");
  check_bits(out.at376, 0.5F, "but at376 is still recomputed on that path");
}

void at376_is_the_difference_after_the_lags() {
  const LiveRampState out = update_live_flight_ramps({}, basic(), kFrame);
  check_bits(out.at376, out.at360 - out.at364,
             "at376 uses the updated values, not the seeds");
}

void the_controls_all_bite() {
  int clamped_lag = 0;
  int gated_both = 0;
  int bypass_skips_difference = 0;
  for (int i = 0; i <= 32; ++i) {
    const float step = 0.05F + static_cast<float>(i) * 0.0625F;
    LiveRampInputs inputs = basic();
    inputs.gate1224 = (i % 2) == 0;
    LiveRampState state{};
    state.at360 = static_cast<float>(i) / 32.0F;
    state.at364 = 1.0F - static_cast<float>(i) / 32.0F;
    state.at368 = 0.25F;
    state.at372 = 0.25F;
    const LiveRampState real = update_live_flight_ramps(state, inputs, step);

    if (real.at360 > 1.0F) {
      ++clamped_lag;                 // a clamped port would never produce this
    }
    if (!inputs.gate1224 && real.at372 == 0.0F) {
      ++gated_both;                  // gating both would zero at372 too
    }
    LiveRampInputs bypassed = inputs;
    bypassed.flags332 = kBypassBit;
    const LiveRampState skipped = update_live_flight_ramps(state, bypassed, step);
    if (skipped.at376 != state.at360 - state.at364) {
      ++bypass_skips_difference;
    }
  }
  std::printf("controls: overshoot-seen=%d gate-touched-at372=%d "
              "bypass-skipped-376=%d\n",
              clamped_lag, gated_both, bypass_skips_difference);
  check(clamped_lag > 0,
        "CONTROL the sweep must reach an overshoot a clamped port cannot");
  check(gated_both == 0, "CONTROL the gate must NOT reach at372");
  check(bypass_skips_difference == 0,
        "CONTROL at376 must still be computed on the bypass path");
}

}  // namespace

int main() {
  the_flag_is_bit_seven();
  a_lag_moves_a_fraction_of_the_remaining_distance();
  the_lag_stops_when_it_arrives();
  a_large_step_overshoots_because_there_is_no_clamp();
  each_command_drives_its_own_ramp();
  the_secondary_ramps_follow_the_threshold();
  the_secondary_ramps_are_bounded();
  the_gate_zeroes_at368_only();
  the_bypass_skips_the_lags_but_not_the_difference();
  at376_is_the_difference_after_the_lags();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_live_flight_ramps: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_live_flight_ramps: all cases passed\n");
  return 0;
}
