// Regression tests for the decays of 0x82306A38.

#include "ac6/retail_live_flight_step.h"

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

using ac6::retail::apply_live_step_decays;
using ac6::retail::kAlternateSlot39Bit;
using ac6::retail::kDecayEpsilon;
using ac6::retail::kDecayRate;
using ac6::retail::kResetBit;
using ac6::retail::kSkipAttitudeBit;
using ac6::retail::LiveStepDecays;

constexpr float kFrame = 0.016666668F;

void the_bits_are_where_the_rotate_masks_put_them() {
  check(kSkipAttitudeBit == 128U, "skip-attitude is bit 7");
  check(kAlternateSlot39Bit == 64U, "the slot-39 substitution is bit 6");
  check(kResetBit == 2U, "the reset is bit 1, the same bit as the other step");
  check_bits(kDecayRate, 3.0F, "3.0 at 0x8200134C");
  check_bits(kDecayEpsilon, 1.0F / 65536.0F, "2^-16, exact");
}

void a_field_decays_exponentially() {
  LiveStepDecays state{0.5F, -0.25F};
  const LiveStepDecays out = apply_live_step_decays(state, 0U, kFrame);
  check_bits(out.at412, std::fmaf(-(-0.25F) * kFrame, kDecayRate, -0.25F),
             "at412 decays toward zero");
  check_bits(out.at408, std::fmaf(-0.5F * kFrame, kDecayRate, 0.5F),
             "at408 decays toward zero");
  check(std::fabs(out.at408) < 0.5F, "and the magnitude shrinks");
}

void below_the_epsilon_the_field_is_left_alone() {
  // NOT snapped to zero. The axis blocks of slot 30 use the same 2^-16 to switch
  // regimes; here it merely skips the block, and the field keeps its value.
  const float tiny = kDecayEpsilon * 0.5F;
  LiveStepDecays state{tiny, -tiny};
  const LiveStepDecays out = apply_live_step_decays(state, 0U, kFrame);
  check_bits(out.at408, tiny, "at408 keeps a sub-epsilon value");
  check_bits(out.at412, -tiny, "at412 keeps a sub-epsilon value");
}

void the_reset_bit_skips_the_decays_entirely() {
  LiveStepDecays state{0.5F, 0.5F};
  const LiveStepDecays out = apply_live_step_decays(state, kResetBit, kFrame);
  check(out == state, "bit 1 leaves both fields untouched -- slot 33 runs");
}

void the_other_bits_do_not_reach_the_decays() {
  LiveStepDecays state{0.5F, 0.5F};
  const LiveStepDecays plain = apply_live_step_decays(state, 0U, kFrame);
  check(apply_live_step_decays(state, kSkipAttitudeBit, kFrame) == plain,
        "bit 7 does not reach the decays");
  check(apply_live_step_decays(state, kAlternateSlot39Bit, kFrame) == plain,
        "bit 6 does not reach the decays");
}

void the_controls_all_bite() {
  int snapped = 0, wrong_bit = 0;
  for (int i = -32; i <= 32; ++i) {
    const float v = static_cast<float>(i) * kDecayEpsilon * 0.25F;
    LiveStepDecays state{v, v};
    const LiveStepDecays real = apply_live_step_decays(state, 0U, kFrame);
    if (std::fabs(v) < kDecayEpsilon && v != 0.0F && real.at408 == 0.0F) {
      ++snapped;
    }
    if (apply_live_step_decays(state, 1U, kFrame) != real) {
      ++wrong_bit;                 // bit 0 must not be the reset
    }
  }
  std::printf("controls: snapped-to-zero=%d bit0-is-reset=%d\n",
              snapped, wrong_bit);
  check(snapped == 0, "CONTROL a sub-epsilon value must NOT be snapped to zero");
  check(wrong_bit == 0, "CONTROL bit 0 must not act as the reset");
}

}  // namespace

int main() {
  the_bits_are_where_the_rotate_masks_put_them();
  a_field_decays_exponentially();
  below_the_epsilon_the_field_is_left_alone();
  the_reset_bit_skips_the_decays_entirely();
  the_other_bits_do_not_reach_the_decays();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_live_flight_step: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_live_flight_step: all cases passed\n");
  return 0;
}
