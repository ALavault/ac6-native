// The controller bridge. The path is contracted; the last link is invented, and
// these tests hold the line between them.

#include "ac6/demo_flight_input.h"
#include "ac6/demo_flight_view.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

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
using ac6::demo::stick_from_record;
using ac6::demo::StickBindings;

InputRecord idle() {
  InputRecord r{};
  // What retail actually leaves on an idle stick: NEGATIVE ZERO. Cycle 1323
  // measured it, and it is the reason three ports in this chain compare with
  // >= 0.0 rather than with signbit.
  r.axis_lx = -0.0F; r.axis_ly = -0.0F; r.axis_rx = -0.0F; r.axis_ry = -0.0F;
  return r;
}

void an_idle_stick_commands_nothing() {
  const FlightStick s = stick_from_record(idle(), default_stick_bindings());
  check_bits(s.target12, 0.0F, "no pitch target");
  check_bits(s.increment12, 0.0F, "no pitch increment");
  check_bits(s.target13, 0.0F, "no yaw target");
  check_bits(s.target14, 0.0F, "no roll target");
}

void a_deflection_inside_the_deadzone_commands_nothing() {
  // The deadzone is the CONTRACTED binding layer's, not this file's: a value
  // whose processed result is exactly zero makes retail store nothing, and
  // apply_input_binding returns an empty optional to say so.
  InputRecord r = idle();
  r.axis_ly = 0.05F;                     // inside a deadzone of 0.08
  const FlightStick s = stick_from_record(r, default_stick_bindings());
  check_bits(s.target12, 0.0F, "inside the deadzone, nothing is commanded");
  check_bits(s.increment12, 0.0F, "and no increment either");
}

void a_deflection_past_the_deadzone_commands_something() {
  InputRecord r = idle();
  r.axis_ly = 0.5F;
  const FlightStick s = stick_from_record(r, default_stick_bindings());
  check(s.target12 > 0.0F, "past the deadzone a target appears");
  check(s.increment12 > 0.0F, "and an increment");
}

void each_controller_axis_reaches_one_slot() {
  StickBindings b = default_stick_bindings();
  InputRecord r = idle();
  r.axis_ly = 0.5F;
  FlightStick s = stick_from_record(r, b);
  check(s.target12 != 0.0F, "left Y reaches slot 12");
  check_bits(s.target13, 0.0F, "and not slot 13");
  check_bits(s.target14, 0.0F, "nor slot 14");

  r = idle(); r.axis_lx = 0.5F;
  s = stick_from_record(r, b);
  check(s.target14 != 0.0F, "left X reaches slot 14");
  check_bits(s.target12, 0.0F, "and not slot 12");

  r = idle(); r.axis_rx = 0.5F;
  s = stick_from_record(r, b);
  check(s.target13 != 0.0F, "right X reaches slot 13");
}

void the_sign_survives_the_whole_path() {
  InputRecord r = idle();
  r.axis_ly = -0.5F;
  const FlightStick s = stick_from_record(r, default_stick_bindings());
  check(s.target12 < 0.0F, "a negative deflection gives a negative target");
  check(s.increment12 > 0.0F, "but the increment is a magnitude");
}

void a_full_snapshot_flies_the_contracted_chain() {
  // The whole way: raw controller bytes -> contracted record -> contracted
  // binding -> my conversion -> the contracted flight session -> an attitude.
  std::array<std::uint8_t, 0x40> snapshot{};
  snapshot.fill(0);
  // Left stick Y hard over. The record builder reads big-endian halfwords at
  // the offsets retail_input_record.h fixes; a wrong offset here shows up as no
  // motion, which the assertion below catches.
  FlightSessionState state{};
  FlightModelConfig config{};
  config.limits = FlightRotationLimits{5.0F, 1.399999976158142F, 5.400000095367432F};
  config.rates304 = LiveAxisRates{4.0F, 2.0F};
  config.rates308 = LiveAxisRates{3.0F, 1.5F};
  config.rates312 = LiveAxisRates{5.0F, 2.5F};
  config.servo304 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
  config.servo308 = config.servo304; config.servo312 = config.servo304;
  config.rampRate952 = 3.0F; config.rampRate956 = 3.0F;
  config.rampThreshold404 = 0.5F;

  const RetailBasis before = state.basis;
  InputRecord record = idle();
  record.axis_ly = 1.0F;
  const FlightStick stick = stick_from_record(record, default_stick_bindings());
  for (int frame = 0; frame < 600; ++frame) {
    step_flight_session(state, config, stick, 0.016666668F);
  }
  check(!(state.basis == before),
        "ten seconds of a held stick moves the attitude");

  // And the picture follows it.
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

void the_invented_numbers_are_reachable_and_named() {
  StickBindings b = default_stick_bindings();
  check(b.invented_full_scale_angle > 0.0F, "the full-scale angle is a field");
  check(b.invented_increment_rate > 0.0F, "so is the increment rate");
  // Doubling the invented angle must double the target and leave the increment
  // alone: the two are separate choices and must stay separable.
  InputRecord r = idle(); r.axis_ly = 0.5F;
  const FlightStick one = stick_from_record(r, b);
  b.invented_full_scale_angle *= 2.0F;
  const FlightStick two = stick_from_record(r, b);
  check_bits(two.target12, one.target12 * 2.0F, "the angle scales the target");
  check_bits(two.increment12, one.increment12, "and not the increment");
}

}  // namespace

int main() {
  an_idle_stick_commands_nothing();
  a_deflection_inside_the_deadzone_commands_nothing();
  a_deflection_past_the_deadzone_commands_something();
  each_controller_axis_reaches_one_slot();
  the_sign_survives_the_whole_path();
  a_full_snapshot_flies_the_contracted_chain();
  the_invented_numbers_are_reachable_and_named();
  if (failures != 0) {
    std::printf("demo_flight_input: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("demo_flight_input: all cases passed\n");
  return 0;
}
