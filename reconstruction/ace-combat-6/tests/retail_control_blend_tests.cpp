#include "ac6/retail_control_blend.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("FAIL  %s\n", w); ++failures; } }
void check_bits(float a, float b, const char* w) {
  if (std::signbit(a) != std::signbit(b) || !(a == b)) {
    std::printf("FAIL  %s  (got %.9g, want %.9g)\n", w, a, b); ++failures;
  }
}
using ac6::retail::blend_control_axis;
using ac6::retail::ControlBlend;
using ac6::retail::kBlendEpsilon;
using ac6::retail::kBlendResetBit;
using ac6::retail::kSlot17Scale;
using ac6::retail::kSlot18Scale;

void the_constants_are_literals_not_reciprocals() {
  check_bits(kSlot17Scale, 0.6366198062896729F, "0.6366198 at 0x82007F78");
  check_bits(kSlot18Scale, 0.3183099031448364F, "0.3183099 at 0x82008AD8");
  check(kSlot17Scale != static_cast<float>(2.0 / 3.14159265358979323846),
        "and it is NOT float32(2/pi)");
  check(kSlot18Scale != static_cast<float>(1.0 / 3.14159265358979323846),
        "nor float32(1/pi)");
  check(kBlendResetBit == 2U, "the bit is bit 1");
}

void without_the_bit_the_stored_value_is_added() {
  const ControlBlend out = blend_control_axis(0.25F, 9.0F, 0.5F, 0U, kSlot17Scale);
  check_bits(out.value, 0.75F, "stored + axis");
  check(!out.wrote, "and nothing is written back");
}

void a_sub_epsilon_stored_value_is_ignored() {
  const float tiny = kBlendEpsilon * 0.5F;
  const ControlBlend out = blend_control_axis(0.25F, 9.0F, tiny, 0U, kSlot17Scale);
  check_bits(out.value, 0.25F, "a sub-epsilon stored value contributes nothing");
}

void with_the_bit_the_scaled_rate_can_replace_the_axis() {
  const ControlBlend big = blend_control_axis(0.1F, 1.0F, 0.0F, kBlendResetBit,
                                              kSlot17Scale);
  check_bits(big.value, 1.0F * kSlot17Scale, "a larger scaled rate wins");
  check(big.wrote, "and it is written back");

  const ControlBlend small = blend_control_axis(0.9F, 0.1F, 0.0F, kBlendResetBit,
                                                kSlot17Scale);
  check_bits(small.value, 0.9F, "a smaller one does not");
  check_bits(small.stored, 0.9F, "and the axis is what gets stored");
}

void equal_magnitudes_keep_the_axis() {
  // `ble` skips the move, so this needs a STRICT greater-than.
  const float axis = -0.5F;
  const float rate = 0.5F / kSlot17Scale;   // |scaled| == |axis|
  const ControlBlend out = blend_control_axis(axis, rate, 0.0F, kBlendResetBit,
                                              kSlot17Scale);
  check(out.value < 0.0F, "on a tie the axis is kept, sign and all");
}

void the_clamp_is_two_early_returns() {
  check_bits(blend_control_axis(2.0F, 0.0F, 0.0F, 0U, kSlot17Scale).value, 1.0F,
             "above one returns one");
  check_bits(blend_control_axis(-2.0F, 0.0F, 0.0F, 0U, kSlot17Scale).value, -1.0F,
             "below minus one returns minus one");
  check_bits(blend_control_axis(1.0F, 0.0F, 0.0F, 0U, kSlot17Scale).value, 1.0F,
             "exactly one passes through");
}

void a_nan_takes_neither_branch() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  check(std::isnan(blend_control_axis(nan, 0.0F, 0.0F, 0U, kSlot17Scale).value),
        "a NaN is returned unchanged, not clamped");
}

void the_two_slots_differ_only_in_their_constant() {
  const ControlBlend a = blend_control_axis(0.1F, 1.0F, 0.0F, kBlendResetBit,
                                            kSlot17Scale);
  const ControlBlend b = blend_control_axis(0.1F, 1.0F, 0.0F, kBlendResetBit,
                                            kSlot18Scale);
  check(a.value != b.value, "the two constants give different answers");
  check_bits(a.value / kSlot17Scale, b.value / kSlot18Scale,
             "and differ by exactly that ratio");
}

void the_controls_all_bite() {
  int always_wrote = 0, ge_instead_of_gt = 0;
  for (int i = -32; i <= 32; ++i) {
    const float axis = static_cast<float>(i) / 32.0F;
    const ControlBlend plain = blend_control_axis(axis, axis / kSlot17Scale,
                                                  0.25F, 0U, kSlot17Scale);
    if (plain.wrote) { ++always_wrote; }
    // AN EXACT TIE, built forwards. Dividing the axis by the scale and
    // multiplying back does NOT round-trip -- the first version of this control
    // did that and reported two failures that were the fixture's rounding, not
    // the port's rule. Building the axis FROM the scaled rate makes the two
    // magnitudes bit-identical by construction.
    // ... and kept small enough that the [-1, +1] clamp never fires, which the
    // second version of this control forgot: |scaled| reached 20 and every
    // answer was -1, which is the clamp doing its job and not the tie rule.
    const float rate = static_cast<float>(i) / 64.0F;
    const float scaled = rate * kSlot17Scale;
    const ControlBlend tie = blend_control_axis(-scaled, rate, 0.0F,
                                                kBlendResetBit, kSlot17Scale);
    if (scaled != 0.0F && tie.value != -scaled) { ++ge_instead_of_gt; }
  }
  std::printf("controls: wrote-without-the-bit=%d tie-took-the-rate=%d\n",
              always_wrote, ge_instead_of_gt);
  check(always_wrote == 0, "CONTROL the non-reset path must never write back");
  check(ge_instead_of_gt == 0, "CONTROL a tie must keep the axis");
}
}  // namespace

int main() {
  the_constants_are_literals_not_reciprocals();
  without_the_bit_the_stored_value_is_added();
  a_sub_epsilon_stored_value_is_ignored();
  with_the_bit_the_scaled_rate_can_replace_the_axis();
  equal_magnitudes_keep_the_axis();
  the_clamp_is_two_early_returns();
  a_nan_takes_neither_branch();
  the_two_slots_differ_only_in_their_constant();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_control_blend: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_control_blend: all cases passed\n");
  return 0;
}
