// The input binding layer, at the boundaries where a wrong rule and the right
// one diverge -- not in the middle where they agree.

#include "ac6/retail_input_binding.h"
#include "ac6/retail_input_record.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

std::uint32_t bits_of(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

void check_bits(float got, std::uint32_t want, const std::string& what) {
  if (bits_of(got) != want) {
    std::cerr << "FAIL " << what << ": got 0x" << std::hex << bits_of(got)
              << " want 0x" << want << std::dec << "\n";
    ++failures;
  }
}

// The layer stores nothing when the processed value is exactly zero, so a test
// that wants a value has to say which case it is in. These two make that
// explicit rather than letting an empty optional read as a pass.
float value_of(float input, const ac6::retail::InputBinding& b) {
  const auto out = ac6::retail::apply_input_binding(input, b);
  check(out.has_value(), "expected the binding to store something");
  return out ? out->value : 0.0F;
}

float step_of(float input, const ac6::retail::InputBinding& b) {
  const auto out = ac6::retail::apply_input_binding(input, b);
  check(out.has_value(), "expected the binding to store something");
  return out ? out->step : 0.0F;
}

ac6::retail::InputBinding binding() {
  ac6::retail::InputBinding b;
  b.threshold = 0.75F;
  b.deadzone = 0.25F;
  b.scale = 2.0F;
  return b;
}

void test_deadzone_boundary() {
  const auto b = binding();
  // Exactly at the deadzone the subtraction gives 0, which is NOT < 0, so it
  // takes the scale branch and still yields 0. Below it, the guard fires. Both
  // produce zero, and a rule with the comparison the other way round produces
  // zero too -- so the discriminating point is just above.
  check(!ac6::retail::apply_input_binding(0.25F, b).has_value(),
        "at the deadzone the value is zero, so NOTHING is stored");
  check(!ac6::retail::apply_input_binding(0.20F, b).has_value(),
        "below the deadzone likewise");
  const float above = value_of(0.30F, b);
  check(std::fabs(above - (0.30F - 0.25F) * 2.0F) < 1e-6F,
        "just above the deadzone the scale applies");
}

void test_clamp_and_sign() {
  const auto b = binding();
  // (1.0 - 0.25) * 2 = 1.5, clamped to 1.
  check_bits(value_of(1.0F, b), bits_of(1.0F),
             "the scaled value clamps at 1");
  check_bits(value_of(-1.0F, b), bits_of(-1.0F),
             "and the sign is restored after clamping");
  // The magnitude is taken first, so a negative input never reaches the clamp
  // as a negative -- a port that scaled the signed value would clamp only the
  // positive side.
  const float negative = value_of(-0.30F, b);
  check(std::fabs(negative + (0.30F - 0.25F) * 2.0F) < 1e-6F,
        "a negative input is processed by magnitude then re-signed");
}

void test_negative_zero() {
  // THE ONE THAT MATTERS. Cycle 1323 measured that an idle axis leaves NEGATIVE
  // zero in the record, so every idle binding is called with -0.0. fsel compares
  // against +0.0 and -0.0 >= 0.0 is true, so the positive branch is taken and
  // the output is POSITIVE zero.
  const auto b = binding();
  check(!ac6::retail::apply_input_binding(-0.0F, b).has_value(),
        "an idle axis stores nothing at all -- the slot keeps last frame's value");
  check_bits(ac6::retail::select_ge_zero(-0.0F, 1.0F, -1.0F), bits_of(1.0F),
             "select_ge_zero sends -0.0 down the positive branch");
  check_bits(ac6::retail::select_ge_zero(-1.0F, 1.0F, -1.0F), bits_of(-1.0F),
             "and a genuinely negative value down the other");
}

void test_step_has_three_regions() {
  const auto b = binding();
  // Inside the deadzone: zero.
  check(!ac6::retail::apply_input_binding(0.10F, b).has_value(),
        "inside the deadzone nothing is stored, step included");
  // Beyond the threshold: saturated.
  check_bits(step_of(0.90F, b), bits_of(1.0F),
             "step saturates to +1 beyond the threshold");
  check_bits(step_of(-0.90F, b), bits_of(-1.0F),
             "and to -1 on the other side");
  // BETWEEN them the RAW VALUE passes through. Cycle 1353 called this a
  // "three-state sign" and it is not; this case is the one that says so.
  check_bits(step_of(0.50F, b), bits_of(0.50F),
             "step passes the input through between deadzone and threshold");
  check_bits(step_of(-0.50F, b), bits_of(-0.50F),
             "including its sign");
  // The boundaries themselves: the guards are < and >, so equality falls in the
  // middle band on both sides.
  // At exactly the deadzone the VALUE is zero, so the slot is skipped and the
  // step never reaches an array -- the middle-band rule is shown just above it.
  check(!ac6::retail::apply_input_binding(0.25F, b).has_value(),
        "at exactly the deadzone the slot is skipped");
  check_bits(step_of(0.75F, b), bits_of(0.75F),
             "step at exactly the threshold is the input, not one");
}

void test_invert() {
  ac6::retail::BindingOutputs o;
  o.value = 0.5F;
  o.step = 1.0F;
  const auto flipped = ac6::retail::invert_outputs(o);
  check_bits(flipped.value, bits_of(-0.5F), "invert negates the value");
  check_bits(flipped.step, bits_of(-1.0F), "invert negates the step");
  // Negating a zero is not a no-op, and the sign reaches the output array.
  ac6::retail::BindingOutputs zero;
  check_bits(ac6::retail::invert_outputs(zero).value, 0x80000000U,
             "inverting +0.0 gives -0.0");
}

void test_slot_agrees_with_the_record() {
  // The whole point of this behaviour: it reads the record with the rule the
  // record's own port already implements. If these ever disagree, one of the two
  // derivations was wrong.
  for (unsigned bit = 0; bit < 20; ++bit) {
    check(ac6::retail::float_slot_for_bit(bit) == (bit + 3) * 4,
          "the record's slot rule is (bit + 3) * 4");
  }
  check(ac6::retail::binding_descriptor_index(0) == 6,
        "the descriptor index starts at +6");
  check(ac6::retail::binding_descriptor_index(17) == 23,
        "and tracks the record bit");
}

}  // namespace

int main() {
  test_deadzone_boundary();
  test_clamp_and_sign();
  test_negative_zero();
  test_step_has_three_regions();
  test_invert();
  test_slot_agrees_with_the_record();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "retail_input_binding=pass\n";
  return 0;
}
