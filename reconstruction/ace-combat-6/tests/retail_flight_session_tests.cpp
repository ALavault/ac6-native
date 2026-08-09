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

void check_bits(float a, float b, const char* w) {
  if (std::signbit(a) != std::signbit(b) || !(a == b)) {
    std::printf("FAIL  %s  (got %.9g, want %.9g)\n", w, a, b);
    ++failures;
  }
}

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
  check(state.accumulators.at36 == 0.0F, "and nothing accumulates");
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
  check(state.accumulators.at36 != 0.0F, "the accumulator moves");
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
  check(state.accumulators.at44 != 0.0F, "slot 13 accumulates at command44");
  check(state.accumulators.at36 == 0.0F, "and not at command36");
  check(state.accumulators.at40 == 0.0F, "nor at command40");
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

void the_position_integrates_and_the_floor_holds() {
  // The integration is retail's; the rates are this test's. One second of level
  // flight at 360 km/h along the first component: 360 * (1/3.6) = 100 units.
  FlightSessionState state{};
  state.position.at64 = 0.0F;
  state.position.at68 = 500.0F;
  state.position.at72 = 0.0F;
  FlightRates rates{};
  rates.to64 = 1.0F;
  for (int frame = 0; frame < 60; ++frame) {
    integrate_session_position(state, rates, 360.0F, 0.0F, kFrame);
  }
  check(state.position.at64 > 99.0F && state.position.at64 < 101.0F,
        "360 km/h for a second moves about 100 units");
  check_bits(state.position.at68, 500.0F, "and nothing moves the other two");
  check_bits(state.position.at72, 0.0F, "");

  // THE FLOOR IS RETAIL'S AND IT IS ON at68 ALONE. 10.0 at 0x82003214.
  FlightSessionState low{};
  low.position.at68 = 12.0F;
  FlightRates down{};
  down.to68 = -1.0F;
  for (int frame = 0; frame < 600; ++frame) {
    integrate_session_position(low, down, 360.0F, 0.0F, kFrame);
  }
  check_bits(low.position.at68, 10.0F, "the vertical component floors at 10.0");

  // CONTROL: the same descent on another component must NOT floor, or the port
  // has spread a clamp retail applies to one axis across all three.
  FlightSessionState side{};
  side.position.at64 = 12.0F;
  FlightRates sideways{};
  sideways.to64 = -1.0F;
  for (int frame = 0; frame < 600; ++frame) {
    integrate_session_position(side, sideways, 360.0F, 0.0F, kFrame);
  }
  check(side.position.at64 < 0.0F, "at64 has no floor -- the clamp is at68's alone");
}

void the_gravity_bias_only_touches_the_vertical() {
  FlightSessionState with{};
  with.position.at68 = 1000.0F;
  FlightSessionState without = with;
  const FlightRates level{};
  for (int frame = 0; frame < 60; ++frame) {
    integrate_session_position(with, level, 1.0F, kGravityKmhPerSecond, kFrame);
    integrate_session_position(without, level, 1.0F, 0.0F, kFrame);
  }
  check(with.position.at68 < without.position.at68, "the bias pulls at68 down");
  check_bits(with.position.at64, without.position.at64, "and leaves at64 alone");
  check_bits(with.position.at72, without.position.at72, "and at72");
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
      "# POSITION IS PRESENT AND ITS DIRECTION IS INVENTED, which cycle 1415\n"
      "# separated. The integrator 0x82303110 is contracted -- the 1/3.6 scale,\n"
      "# the 10.0 floor on the vertical component, the gravity bias and the\n"
      "# fusing are all retail's. What is NOT available is its INPUT: the three\n"
      "# rates come off stack slots 80/84/88 that a vector normalise fills, and\n"
      "# that normalise seeds on vrsqrtefp (0x823034CC) and vrefp (0x823034FC),\n"
      "# estimate instructions this campaign refuses to approximate. So retail\n"
      "# supplies a unit DIRECTION there and [model+32] supplies the SPEED.\n"
      "# The direction below is a basis row and the speed is a number, both\n"
      "# chosen here. Earlier versions of this file said position was absent\n"
      "# because 0x823042D0 was blocked; that is the live model's own step and\n"
      "# a different function from the contracted integrator.\n"
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
                 frame, state.accumulators.at36, state.accumulators.at44, state.accumulators.at40,
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

void the_player_interface_drives_the_same_state() {
  // The two interfaces onto the same accumulators, cycle 1405. A field passed
  // through apply_flight_input must reach the axes exactly as a stick command
  // reaches them -- and it must NOT go through the one-degree tolerance, which
  // is the setters' rule and not the accumulators'.
  FlightSessionState viaFields{};
  FlightInputFields f{};
  f.at2104 = kOneDegree * 0.25F;     // a quarter of a degree
  for (int i = 0; i < 60; ++i) {
    step_flight_session(viaFields, config(), f, kFrame);
  }
  check(viaFields.accumulators.at36 > 0.0F,
        "the player path accumulates a command the setters would discard");

  FlightSessionState viaStick{};
  for (int i = 0; i < 60; ++i) {
    step_flight_session(viaStick, config(), pitch(kOneDegree * 0.25F), kFrame);
  }
  check_bits(viaStick.accumulators.at36, 0.0F,
             "and the AI path discards it, as cycle 1393 measured");
}

void the_holds_reach_the_ramps() {
  // +48 and +52 are accumulators, and retail_live_flight_ramps reads them as its
  // two targets. The first version of this file never fed them, so both ramps
  // decayed for ever whatever the input.
  FlightSessionState state{};
  FlightInputFields f{};
  f.at2096 = 1.0F;
  for (int i = 0; i < 30; ++i) { step_flight_session(state, config(), f, kFrame); }
  check(state.accumulators.at48 > 0.0F, "+2096 accumulates into +48");
  check(state.ramps.at360 > 0.0F, "and the ramp follows it");
  check_bits(state.ramps.at364, 0.0F, "while the other ramp stays put");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--emit-trajectory") {
    emit_trajectory(argv[2]);
    return 0;
  }
  a_neutral_stick_leaves_the_basis_alone();
  the_player_interface_drives_the_same_state();
  the_holds_reach_the_ramps();
  a_command_within_a_degree_never_reaches_the_attitude();
  a_real_command_reaches_the_attitude();
  the_state_carries_between_frames();
  each_stick_axis_reaches_its_own_accumulator();
  the_export_block_carries_the_accessors();
  the_basis_stays_finite_over_a_long_run();
  the_digest_moves_with_the_state();
  the_position_integrates_and_the_floor_holds();
  the_gravity_bias_only_touches_the_vertical();
  if (failures != 0) {
    std::printf("retail_flight_session: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_flight_session: all cases passed\n");
  return 0;
}
