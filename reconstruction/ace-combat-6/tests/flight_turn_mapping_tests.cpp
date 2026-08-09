// Which stick axis drives which rotation limit, by a control matrix.
//
// The campaign has called them "the three target angles", slots 12, 13 and 14,
// and has never said which is which -- correctly, because a slot number is not
// a name. `FlightRotationLimits` carries three limits documented as the row-0,
// row-1 and row-2 rotations, from 0x820A99F8, 0x820A9B30 and 0x82211828.
//
// This pairs them by intervention rather than by inspection: fly each stick
// alone, double each limit alone, and measure the angle the basis's forward row
// turns through. A pairing shows as a doubled turn on the diagonal and an
// unchanged one everywhere else.
//
// AND THE THIRD ROW IS THE CONTROL ON THE OTHER TWO. Stick 14 turns the forward
// vector by exactly zero under every limit -- which is what a rotation ABOUT
// row 2 must do to row 2. So the header's "at1256 is the row-2 rotation" is
// confirmed by behaviour and not merely by its comment.
#include "ac6/retail_flight_session.h"
#include "ac6/retail_flight_step.h"
#include "ac6/retail_transform.h"

#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(bool c, const char* w) {
  if (!c) { std::printf("FAIL  %s\n", w); ++failures; }
}
using namespace ac6::retail;

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

// `which_limit` is 0 for none, else 1248, 1252 or 1256.
float forward_turn_degrees(int stick, int which_limit) {
  FlightSessionState state{};
  FlightStick k{};
  if (stick == 12) { k.target12 = 0.5F; k.increment12 = 1.0F; }
  if (stick == 13) { k.target13 = 0.5F; k.increment13 = 1.0F; }
  if (stick == 14) { k.target14 = 0.5F; k.increment14 = 1.0F; }
  FlightModelConfig c = config();
  if (which_limit == 1248) c.limits.at1248 *= 2.0F;
  if (which_limit == 1252) c.limits.at1252 *= 2.0F;
  if (which_limit == 1256) c.limits.at1256 *= 2.0F;

  const auto start = state.basis.rows[2];
  for (int i = 0; i < 600; ++i) {
    step_flight_session(state, c, k, 1.0F / 60.0F);
  }
  const auto& now = state.basis.rows[2];
  float dot = start[0] * now[0] + start[1] * now[1] + start[2] * now[2];
  dot = dot > 1.0F ? 1.0F : (dot < -1.0F ? -1.0F : dot);
  return std::acos(dot) * 57.29577951F;
}
}  // namespace

int main() {
  const int sticks[3] = {12, 13, 14};
  const int limits[3] = {1248, 1252, 1256};
  float base[3], cell[3][3];
  for (int s = 0; s < 3; ++s) {
    base[s] = forward_turn_degrees(sticks[s], 0);
    for (int l = 0; l < 3; ++l) {
      cell[s][l] = forward_turn_degrees(sticks[s], limits[l]);
    }
  }

  check(base[0] > 1.0F, "stick 12 turns the forward row");
  check(base[1] > 0.1F, "stick 13 turns it too, and less");
  check(base[2] == 0.0F,
        "stick 14 turns it by EXACTLY zero -- a rotation about row 2 fixes row 2");

  // The diagonal doubles.
  check(std::fabs(cell[0][0] / base[0] - 2.0F) < 0.01F, "stick 12 pairs with at1248");
  check(std::fabs(cell[1][1] / base[1] - 2.0F) < 0.01F, "stick 13 pairs with at1252");

  // And every off-diagonal is unchanged, to the bit.
  for (int s = 0; s < 3; ++s) {
    for (int l = 0; l < 3; ++l) {
      if (s == l && s < 2) continue;
      check(cell[s][l] == base[s],
            "an unpaired limit changes nothing at all");
    }
  }

  std::printf("stick 12 %.4f -> %.4f  |  stick 13 %.4f -> %.4f  |  stick 14 %.4f\n",
              base[0], cell[0][0], base[1], cell[1][1], base[2]);
  if (failures == 0) std::printf("turn mapping OK\n");
  return failures == 0 ? 0 : 1;
}
