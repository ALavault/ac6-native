// The controller bridge. Every rule is contracted; only the wiring is chosen.

#include "ac6/demo_flight_input.h"
#include "ac6/demo_flight_view.h"
#include "ac6/retail_flight_session.h"

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
using ac6::demo::default_stick_bindings;
using ac6::demo::fields_from_record;

InputRecord idle() {
  InputRecord r{};
  // What retail leaves on an idle stick: NEGATIVE ZERO, measured at cycle 1323.
  r.axis_lx = -0.0F; r.axis_ly = -0.0F; r.axis_rx = -0.0F; r.axis_ry = -0.0F;
  return r;
}

void an_idle_stick_produces_no_increments() {
  const FlightInputFields f = fields_from_record(idle(), default_stick_bindings());
  check_bits(f.at2104, 0.0F, "no pitch");
  check_bits(f.at2108, 0.0F, "no roll");
  check_bits(f.at2112, 0.0F, "no yaw, positive half");
  check_bits(f.at2116, 0.0F, "no yaw, negative half");
  check_bits(f.at2096, 0.0F, "no hold");
}

void a_deflection_inside_the_deadzone_produces_nothing() {
  InputRecord r = idle(); r.axis_ly = 0.05F;
  check_bits(fields_from_record(r, default_stick_bindings()).at2104, 0.0F,
             "the contracted binding layer's deadzone reaches the fields");
}

void the_yaw_axis_splits_across_the_difference_pair() {
  // retail's +44 takes [+2112] - [+2116]. Driving one signed axis across the
  // pair reproduces that arithmetic without claiming where retail's halves
  // come from.
  InputRecord r = idle(); r.axis_rx = 0.5F;
  FlightInputFields f = fields_from_record(r, default_stick_bindings());
  check(f.at2112 > 0.0F && f.at2116 == 0.0F, "a positive yaw fills the first");
  const float positive = f.at2112 - f.at2116;

  r = idle(); r.axis_rx = -0.5F;
  f = fields_from_record(r, default_stick_bindings());
  check(f.at2116 > 0.0F && f.at2112 == 0.0F, "a negative yaw fills the second");
  check_bits(f.at2112 - f.at2116, -positive, "and the difference is odd");
}

void each_axis_reaches_one_field() {
  InputRecord r = idle(); r.axis_ly = 0.5F;
  FlightInputFields f = fields_from_record(r, default_stick_bindings());
  check(f.at2104 != 0.0F, "left Y -> +2104");
  check_bits(f.at2108, 0.0F, "and not +2108");
  check_bits(f.at2112, 0.0F, "nor the yaw pair");
}

void the_whole_path_flies_and_draws() {
  FlightModelConfig config{};
  config.limits = FlightRotationLimits{5.0F, 1.399999976158142F, 5.400000095367432F};
  config.rates304 = LiveAxisRates{4.0F, 2.0F};
  config.rates308 = LiveAxisRates{3.0F, 1.5F};
  config.rates312 = LiveAxisRates{5.0F, 2.5F};
  config.servo304 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
  config.servo308 = config.servo304; config.servo312 = config.servo304;
  config.rampRate952 = 3.0F; config.rampRate956 = 3.0F;
  config.rampThreshold404 = 0.5F;

  InputRecord r = idle(); r.axis_ly = 1.0F;
  const FlightInputFields fields = fields_from_record(r, default_stick_bindings());
  FlightSessionState state{};
  const RetailBasis before = state.basis;
  for (int frame = 0; frame < 600; ++frame) {
    step_flight_session(state, config, fields, 0.016666668F);
  }
  check(!(state.basis == before),
        "ten seconds of a held stick moves the attitude -- and every rule "
        "between the two is contracted");
  check(state.accumulators.at36 > 0.0F, "the accumulator filled");

  ac6::demo::Image image; image.width = 96; image.height = 54;
  image.clear(0, 0, 0);
  ac6::demo::draw_flight_view(image, state.basis, ac6::demo::DemoCamera{});
  std::size_t drawn = 0;
  for (std::size_t i = 0; i + 2 < image.rgb.size(); i += 3) {
    if (!(image.rgb[i] == 24 && image.rgb[i + 1] == 32 && image.rgb[i + 2] == 56)) {
      ++drawn;
    }
  }
  check(drawn > 0, "and the view draws from it");
}

void a_quarter_degree_command_is_no_longer_discarded() {
  // The player path has no one-degree tolerance -- that is the AI setters' rule.
  // This is the behavioural difference the old invented conversion hid.
  InputRecord r = idle(); r.axis_ly = 0.5F;
  const FlightInputFields f = fields_from_record(r, default_stick_bindings());
  FlightInputAccumulators acc{};
  acc = apply_flight_input(acc, f);
  check(acc.at36 > 0.0F, "the player path accumulates immediately");
}
}  // namespace

int main() {
  an_idle_stick_produces_no_increments();
  a_deflection_inside_the_deadzone_produces_nothing();
  the_yaw_axis_splits_across_the_difference_pair();
  each_axis_reaches_one_field();
  the_whole_path_flies_and_draws();
  a_quarter_degree_command_is_no_longer_discarded();
  if (failures != 0) {
    std::printf("demo_flight_input: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("demo_flight_input: all cases passed\n");
  return 0;
}
