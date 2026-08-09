// The contracted flight chain, stepped. This suite does NOT verify retail --
// each piece has its own differential for that. It verifies the COMPOSITION:
// that the pieces are wired in retail's order, that state carries between
// frames, and that a stick input reaches the attitude.

#include "ac6/retail_flight_session.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("FAIL  %s\n", w); ++failures; } }

using namespace ac6::retail;

constexpr float kFrame = 0.016666668F;

FlightModelConfig config() {
  FlightModelConfig c{};
  c.limits = FlightRotationLimits{5.0F, 1.399999976158142F, 5.400000095367432F};
  c.rates304 = LiveAxisRates{4.0F, 2.0F};
  c.rates308 = LiveAxisRates{3.0F, 1.5F};
  c.rates312 = LiveAxisRates{5.0F, 2.5F};
  c.servo304 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
  c.servo308 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
  c.servo312 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
  c.rampRate952 = 3.0F;
  c.rampRate956 = 3.0F;
  c.rampThreshold404 = 0.5F;
  return c;
}

FlightStick pitch(float target) {
  FlightStick s{};
  s.target12 = target;
  s.increment12 = 1.0F;
  return s;
}

void a_neutral_stick_leaves_the_basis_alone() {
  FlightSessionState state{};
  const RetailBasis before = state.basis;
  for (int i = 0; i < 60; ++i) {
    step_flight_session(state, config(), FlightStick{}, kFrame);
  }
  check(state.basis == before, "sixty idle frames do not move the attitude");
  check(state.command36 == 0.0F, "and nothing accumulates");
}

void a_command_within_a_degree_never_reaches_the_attitude() {
  FlightSessionState state{};
  const RetailBasis before = state.basis;
  for (int i = 0; i < 60; ++i) {
    step_flight_session(state, config(), pitch(kOneDegree * 0.25F), kFrame);
  }
  check(state.basis == before,
        "a quarter-degree command is discarded every frame, so nothing moves");
}

void a_real_command_reaches_the_attitude() {
  FlightSessionState state{};
  const RetailBasis before = state.basis;
  const FlightFrame first = step_flight_session(state, config(), pitch(0.5F),
                                                kFrame);
  check(first.accepted12, "half a radian is accepted");
  check(state.command36 != 0.0F, "the accumulator moves");
  for (int i = 0; i < 30; ++i) {
    step_flight_session(state, config(), pitch(0.5F), kFrame);
  }
  check(!(state.basis == before), "and after thirty frames the attitude has moved");
}

void the_state_carries_between_frames() {
  FlightSessionState one{};
  step_flight_session(one, config(), pitch(0.5F), kFrame);
  const LiveAxisState after_one = one.axes;
  step_flight_session(one, config(), pitch(0.5F), kFrame);
  check(!(one.axes == after_one), "the second frame continues from the first");

  FlightSessionState fresh{};
  step_flight_session(fresh, config(), pitch(0.5F), kFrame);
  check(fresh.axes == after_one, "and a fresh state reproduces the first exactly");
}

void each_stick_axis_reaches_its_own_accumulator() {
  FlightSessionState state{};
  FlightStick s{};
  s.target13 = 0.5F;
  s.increment13 = 1.0F;
  step_flight_session(state, config(), s, kFrame);
  check(state.command44 != 0.0F, "slot 13 accumulates at command44");
  check(state.command36 == 0.0F, "and not at command36");
  check(state.command40 == 0.0F, "nor at command40");
}

void the_export_block_carries_the_accessors() {
  FlightSessionState state{};
  for (int i = 0; i < 10; ++i) {
    step_flight_session(state, config(), pitch(0.5F), kFrame);
  }
  const FlightFrame frame = step_flight_session(state, config(), pitch(0.5F),
                                                kFrame);
  check(frame.exported.at12 == frame.accessors.slot16, "slot 16 -> +12");
  check(frame.exported.at8 == frame.accessors.slot17, "slot 17 -> +8");
  check(frame.exported.at16 == frame.accessors.slot18, "slot 18 -> +16");
}

void the_basis_stays_finite_over_a_long_run() {
  // Ten seconds of full deflection. The point is not a value but that nothing
  // in the composed chain diverges or produces a NaN.
  FlightSessionState state{};
  for (int i = 0; i < 600; ++i) {
    FlightStick s{};
    s.target12 = 1.0F;  s.increment12 = 1.0F;
    s.target13 = -1.0F; s.increment13 = 1.0F;
    s.target14 = 0.7F;  s.increment14 = 1.0F;
    step_flight_session(state, config(), s, kFrame);
  }
  bool finite = true;
  for (const auto& row : state.basis.rows) {
    for (const float value : row) {
      if (!std::isfinite(value)) { finite = false; }
    }
  }
  check(finite, "600 frames of full deflection stay finite");
  check(std::isfinite(state.rates.at144) && std::isfinite(state.axes.at304),
        "and so do the rates and axes");
}

void the_digest_moves_with_the_state() {
  FlightSessionState a{};
  FlightSessionState b{};
  check(digest_flight_state(a) == digest_flight_state(b),
        "two fresh states digest alike");
  step_flight_session(a, config(), pitch(0.5F), kFrame);
  check(digest_flight_state(a) != digest_flight_state(b),
        "and one frame moves it");
}

// A scripted three-second manoeuvre, emitted as one row per frame. This is the
// substrate a demo draws: not a picture, but the attitude the contracted chain
// produces from a stick input, in a form that can be diffed.
void emit_trajectory(const char* path) {
  std::FILE* out = std::fopen(path, "w");
  if (out == nullptr) { return; }
  std::fprintf(out,
      "# The contracted flight chain, stepped 180 times at 1/60 s.\n"
      "# Produced by ac6-retail-flight-session-tests --emit-trajectory.\n"
      "#\n"
      "# The stick: one second of pitch, one of roll, one centred. Nothing here\n"
      "# is retail data -- the aircraft numbers are the base constructor's\n"
      "# defaults and the gains are chosen. What IS retail is every rule the\n"
      "# chain applies to them, each with its own micro-execution differential.\n"
      "#\n"
      "# POSITION IS ABSENT, and not by omission: the live model's position step\n"
      "# is 0x823042D0, which cycle 1383 showed depends on vrefp and vrsqrtefp,\n"
      "# estimate instructions whose exact bits belong to the console. The\n"
      "# aeroplane changes attitude here; it does not move.\n"
      "#\n"
      "# frame\tcmd36\tcmd44\tcmd40\tat304\tat308\tat312\trate144\t"
      "row0x\trow0y\trow0z\trow1x\trow1y\trow1z\trow2x\trow2y\trow2z\n");
  FlightSessionState state{};
  for (int frame = 0; frame < 180; ++frame) {
    FlightStick s{};
    if (frame < 60) { s.target12 = 0.8F; s.increment12 = 1.0F; }
    else if (frame < 120) { s.target14 = -0.6F; s.increment14 = 1.0F; }
    step_flight_session(state, config(), s, kFrame);
    std::fprintf(out, "%d\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g\t%.9g",
                 frame, state.command36, state.command44, state.command40,
                 state.axes.at304, state.axes.at308, state.axes.at312,
                 state.rates.at144);
    for (const auto& row : state.basis.rows) {
      for (int i = 0; i < 3; ++i) { std::fprintf(out, "\t%.9g", row[i]); }
    }
    std::fprintf(out, "\n");
  }
  std::fprintf(out, "# final state digest (FNV-1a 64): 0x%016llx\n",
               static_cast<unsigned long long>(digest_flight_state(state)));
  std::fclose(out);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--emit-trajectory") {
    emit_trajectory(argv[2]);
    return 0;
  }
  a_neutral_stick_leaves_the_basis_alone();
  a_command_within_a_degree_never_reaches_the_attitude();
  a_real_command_reaches_the_attitude();
  the_state_carries_between_frames();
  each_stick_axis_reaches_its_own_accumulator();
  the_export_block_carries_the_accessors();
  the_basis_stays_finite_over_a_long_run();
  the_digest_moves_with_the_state();
  if (failures != 0) {
    std::printf("retail_flight_session: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_flight_session: all cases passed\n");
  return 0;
}
