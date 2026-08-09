// Regression tests for the axis blocks of 0x82303E68.

#include "ac6/retail_live_flight_axes.h"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) { std::printf("FAIL  %s\n", what); ++failures; }
}

void check_bits(float actual, float expected, const char* what) {
  if (std::signbit(actual) != std::signbit(expected) || !(actual == expected)) {
    std::printf("FAIL  %s  (got %.9g, want %.9g)\n", what, actual, expected);
    ++failures;
  }
}

using ac6::retail::kCommandEpsilon;
using ac6::retail::kCurveScale;
using ac6::retail::kLagGain;
using ac6::retail::kTimerDecayFactor;
using ac6::retail::LiveAxisInputs;
using ac6::retail::LiveAxisRates;
using ac6::retail::LiveAxisState;
using ac6::retail::update_live_flight_axes;

constexpr float kFrame = 0.016666668F;

LiveAxisInputs rates() {
  LiveAxisInputs inputs{};
  inputs.rates304 = LiveAxisRates{4.0F, 2.0F};
  inputs.rates308 = LiveAxisRates{3.0F, 1.5F};
  inputs.rates312 = LiveAxisRates{5.0F, 2.5F};
  return inputs;
}

void the_constants_are_the_words_of_the_image() {
  check_bits(kCommandEpsilon, 1.52587890625e-05F, "2^-16 at 0x82069C2C");
  check_bits(kCommandEpsilon, 1.0F / 65536.0F, "and it is exact");
  check_bits(kCurveScale, 0.6666666865348816F, "2/3 at 0x82069C1C");
  check_bits(kLagGain, 1.5F, "1.5 at 0x82002FD8");
  check_bits(kTimerDecayFactor, 10.0F, "10.0 at 0x82003214");
}

void a_command_below_the_epsilon_decays() {
  LiveAxisState state{};
  state.at304 = 0.5F;
  state.at312 = -0.5F;
  LiveAxisInputs inputs = rates();
  inputs.cmd36 = kCommandEpsilon * 0.5F;
  inputs.cmd40 = 0.0F;
  const LiveAxisState out = update_live_flight_axes(state, inputs, kFrame);
  check_bits(out.at304, 0.5F - 2.0F * kFrame, "at304 decays at its decay rate");
  check_bits(out.at312, -0.5F + 2.5F * kFrame, "at312 decays toward zero");
}

void the_decay_does_not_cross_zero() {
  LiveAxisState state{};
  state.at304 = 1.0e-9F;
  state.at312 = -1.0e-9F;
  const LiveAxisState out = update_live_flight_axes(state, rates(), kFrame);
  check_bits(out.at304, 0.0F, "at304 stops at zero");
  check_bits(out.at312, 0.0F, "at312 stops at zero");
}

void the_epsilon_boundary_is_where_the_regimes_meet() {
  LiveAxisState state{};
  state.at304 = 0.5F;
  LiveAxisInputs below = rates();
  below.cmd36 = std::nextafterf(kCommandEpsilon, 0.0F);
  LiveAxisInputs at = rates();
  at.cmd36 = kCommandEpsilon;
  check(update_live_flight_axes(state, below, kFrame).at304 !=
        update_live_flight_axes(state, at, kFrame).at304,
        "one ulp either side of 2^-16 chooses different regimes");
}

void the_drive_curve_is_quadratic_and_overshoots_one() {
  // cmd + cmd*cmd, times 2/3, reaches 4/3 at full deflection: the target is
  // beyond the limit and the limit is what stops it, not the curve.
  const float target = std::fmaf(1.0F, 1.0F, 1.0F) * kCurveScale;
  check(target > 1.0F, "the curve's target at full deflection exceeds 1");

  LiveAxisInputs inputs = rates();
  inputs.cmd36 = 1.0F;
  const LiveAxisState out = update_live_flight_axes({}, inputs, kFrame);
  const float gap = (target - 0.0F) * (4.0F * kFrame);
  check_bits(out.at304, std::fmaf(gap, kLagGain, 0.0F),
             "at304 moves 1.5 times the scaled gap toward it");
}

void the_curve_is_odd_in_the_command() {
  LiveAxisInputs up = rates();
  up.cmd36 = 0.7F;
  LiveAxisInputs down = rates();
  down.cmd36 = -0.7F;
  const float a = update_live_flight_axes({}, up, kFrame).at304;
  const float b = update_live_flight_axes({}, down, kFrame).at304;
  check_bits(a, -b, "the curve is odd: equal magnitudes, opposite signs");
}

void the_driven_axes_are_clamped_to_the_unit_interval() {
  LiveAxisInputs inputs = rates();
  inputs.cmd36 = 1.0F;
  inputs.cmd40 = -1.0F;
  const LiveAxisState out = update_live_flight_axes({}, inputs, 1000.0F);
  check_bits(out.at304, 1.0F, "at304 stops at +1");
  check_bits(out.at312, -1.0F, "at312 stops at -1");
}

void at308_has_no_curve_and_no_lag() {
  LiveAxisInputs small = rates();
  small.cmd44 = 0.01F;
  LiveAxisInputs large = rates();
  large.cmd44 = 900.0F;
  check_bits(update_live_flight_axes({}, small, kFrame).at308,
             update_live_flight_axes({}, large, kFrame).at308,
             "at308 uses only the SIGN of its command");
  check_bits(update_live_flight_axes({}, small, kFrame).at308, 3.0F * kFrame,
             "and steps by its command rate");
}

void the_hold_timers_run_per_direction() {
  LiveAxisInputs held = rates();
  held.cmd44 = 1.0F;
  LiveAxisState state{};
  state.at1352 = 0.25F;
  state.at1356 = 0.25F;
  const LiveAxisState out = update_live_flight_axes(state, held, kFrame);
  check_bits(out.at1352, 0.25F + kFrame, "the positive timer accumulates");
  check_bits(out.at1356,
             std::fmaf(-kFrame, kTimerDecayFactor, 0.25F),
             "and the negative one decays at ten times the step");
}

void the_hold_timers_are_floored_at_zero() {
  LiveAxisInputs held = rates();
  held.cmd44 = 1.0F;
  LiveAxisState state{};
  state.at1356 = 1.0e-9F;
  check_bits(update_live_flight_axes(state, held, kFrame).at1356, 0.0F,
             "the decaying timer stops at zero");
}

void an_undriven_at308_leaves_its_timers_alone() {
  // The timers live inside the driven branch, so a command below the epsilon
  // neither accumulates nor decays them.
  LiveAxisState state{};
  state.at308 = 0.5F;
  state.at1352 = 0.25F;
  state.at1356 = 0.25F;
  const LiveAxisState out = update_live_flight_axes(state, rates(), kFrame);
  check_bits(out.at1352, 0.25F, "at1352 is untouched when undriven");
  check_bits(out.at1356, 0.25F, "at1356 is untouched when undriven");
  check_bits(out.at308, 0.5F - 1.5F * kFrame, "but at308 still decays");
}

void each_command_reaches_only_its_own_axis() {
  LiveAxisInputs inputs = rates();
  inputs.cmd36 = 0.5F;
  const LiveAxisState out = update_live_flight_axes({}, inputs, kFrame);
  check(out.at304 != 0.0F, "cmd36 drives at304");
  check_bits(out.at308, 0.0F, "and reaches neither at308");
  check_bits(out.at312, 0.0F, "nor at312");
}

void the_controls_all_bite() {
  int linear_target = 0;
  int at308_scaled = 0;
  int timers_when_undriven = 0;
  for (int i = -32; i <= 32; ++i) {
    const float cmd = static_cast<float>(i) / 32.0F;
    LiveAxisInputs inputs = rates();
    inputs.cmd36 = cmd;
    inputs.cmd44 = cmd;
    LiveAxisState state{};
    state.at304 = cmd * 0.5F;
    state.at308 = -cmd * 0.5F;
    state.at1352 = 0.25F;
    const LiveAxisState real = update_live_flight_axes(state, inputs, kFrame);

    // 1. A linear target instead of the quadratic curve.
    if (std::fabs(cmd) >= kCommandEpsilon) {
      const float linear = (cmd * kCurveScale - state.at304) * (4.0F * kFrame);
      if (std::fmaf(linear, kLagGain, state.at304) != real.at304) {
        ++linear_target;
      }
    }
    // 2. at308 scaling by its command.
    LiveAxisInputs bigger = inputs;
    bigger.cmd44 = cmd * 40.0F;
    if (update_live_flight_axes(state, bigger, kFrame).at308 != real.at308) {
      ++at308_scaled;
    }
    // 3. Timers moving while undriven.
    if (std::fabs(cmd) < kCommandEpsilon && real.at1352 != 0.25F) {
      ++timers_when_undriven;
    }
  }
  std::printf("controls: linear-target=%d at308-scaled=%d timers-undriven=%d\n",
              linear_target, at308_scaled, timers_when_undriven);
  check(linear_target > 0, "CONTROL a linear target must disagree");
  check(at308_scaled == 0, "CONTROL scaling cmd44 must NOT move at308");
  check(timers_when_undriven == 0,
        "CONTROL the timers must not move when at308 is undriven");
}

}  // namespace

int main() {
  the_constants_are_the_words_of_the_image();
  a_command_below_the_epsilon_decays();
  the_decay_does_not_cross_zero();
  the_epsilon_boundary_is_where_the_regimes_meet();
  the_drive_curve_is_quadratic_and_overshoots_one();
  the_curve_is_odd_in_the_command();
  the_driven_axes_are_clamped_to_the_unit_interval();
  at308_has_no_curve_and_no_lag();
  the_hold_timers_run_per_direction();
  the_hold_timers_are_floored_at_zero();
  an_undriven_at308_leaves_its_timers_alone();
  each_command_reaches_only_its_own_axis();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_live_flight_axes: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_live_flight_axes: all cases passed\n");
  return 0;
}
