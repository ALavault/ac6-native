#include "ac6/retail_flight_input_router.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("FAIL  %s\n", w); ++failures; } }
void check_bits(float a, float b, const char* w) {
  if (std::signbit(a) != std::signbit(b) || !(a == b)) {
    std::printf("FAIL  %s  (got %.9g, want %.9g)\n", w, a, b); ++failures;
  }
}
using namespace ac6::retail;

// Six values no two of which are equal, so a crossed pair cannot pass.
constexpr FlightBindingOutputs kOut{{0.125F, -0.375F, 0.6171875F, -0.8203125F,
                                     0.37777779F, -0.94999999F}};

void every_arm_takes_the_two_main_axes_from_slots_zero_and_one() {
  // The one part of the table that does not move. If a future edit routes these
  // per-arm, this is what says so.
  for (const auto device : {FlightInputDevice::kDigital, FlightInputDevice::kAnalog}) {
    for (const std::uint32_t layout : {0U, 1U, 7U}) {
      const FlightInputFields f = route_flight_input_fields(kOut, 0U, device, layout);
      check_bits(f.at2104, kOut.value[0], "+2104 is always array[0]");
      check_bits(f.at2108, kOut.value[1], "+2108 is always array[1]");
    }
  }
}

void the_digital_arm_ignores_the_layout() {
  // Retail reads [r30+4] only on the analog arm; the digital head at 0x82229310
  // branches straight to the common tail. A port that hoisted the layout test
  // above the device test fails here.
  const FlightInputFields zero = route_flight_input_fields(kOut, 0x3CU,
                                                           FlightInputDevice::kDigital, 0U);
  const FlightInputFields one = route_flight_input_fields(kOut, 0x3CU,
                                                          FlightInputDevice::kDigital, 1U);
  check(zero == one, "the digital arm gives the same fields for both layouts");
  check_bits(zero.at2096, kOut.value[4], "digital +2096 is array[4]");
  check_bits(zero.at2100, kOut.value[5], "digital +2100 is array[5]");
  check_bits(zero.at2112, 1.0F, "digital +2112 is bit 2");
  check_bits(zero.at2116, 1.0F, "digital +2116 is bit 3");
}

void the_analog_arms_disagree_and_that_is_the_point() {
  // CONTROL: layout 0 and layout 1 must disagree on all four moving fields.
  // The first draft of the table had them agreeing on two, and it was wrong.
  const FlightInputFields a = route_flight_input_fields(kOut, 0U,
                                                        FlightInputDevice::kAnalog, 0U);
  const FlightInputFields b = route_flight_input_fields(kOut, 0U,
                                                        FlightInputDevice::kAnalog, 1U);
  check(a.at2096 != b.at2096, "layout swaps +2096");
  check(a.at2100 != b.at2100, "layout swaps +2100");
  check(a.at2112 != b.at2112, "layout swaps +2112");
  check(a.at2116 != b.at2116, "layout swaps +2116");

  check_bits(a.at2112, kOut.value[2], "layout 0: +2112 is array[2]");
  check_bits(a.at2116, kOut.value[3], "layout 0: +2116 is array[3]");
  check_bits(b.at2096, kOut.value[3], "layout 1: +2096 is array[3] -- REVERSED");
  check_bits(b.at2100, kOut.value[2], "layout 1: +2100 is array[2] -- REVERSED");
}

void the_bit_that_moves_moves_with_the_layout() {
  // Bit 5 alone. On layout 0 it lands on +2100; on layout 1 on +2112. Reading
  // both fields for one flag word is what separates the correct table from the
  // one the differential rejected.
  const std::uint32_t bit5 = 1U << 5;
  const FlightInputFields a = route_flight_input_fields(kOut, bit5,
                                                        FlightInputDevice::kAnalog, 0U);
  const FlightInputFields b = route_flight_input_fields(kOut, bit5,
                                                        FlightInputDevice::kAnalog, 1U);
  check_bits(a.at2100, 1.0F, "layout 0: bit 5 -> +2100");
  check_bits(a.at2096, 0.0F, "layout 0: and not +2096");
  check_bits(b.at2112, 1.0F, "layout 1: bit 5 -> +2112");
  check_bits(b.at2116, 0.0F, "layout 1: and not +2116");

  const std::uint32_t bit4 = 1U << 4;
  check_bits(route_flight_input_fields(kOut, bit4, FlightInputDevice::kAnalog, 0U).at2096,
             1.0F, "layout 0: bit 4 -> +2096");
  check_bits(route_flight_input_fields(kOut, bit4, FlightInputDevice::kAnalog, 1U).at2116,
             1.0F, "layout 1: bit 4 -> +2116");
}

void no_other_bit_is_read() {
  // Every bit except 2, 3, 4 and 5. Nothing may show on any arm.
  const std::uint32_t noise = ~0x3CU;
  for (const auto device : {FlightInputDevice::kDigital, FlightInputDevice::kAnalog}) {
    for (const std::uint32_t layout : {0U, 1U}) {
      const FlightInputFields f = route_flight_input_fields(kOut, noise, device, layout);
      const FlightInputFields q = route_flight_input_fields(kOut, 0U, device, layout);
      check(f == q, "bits outside 2..5 are not read");
    }
  }
}

void the_bit_result_is_exactly_one_or_zero() {
  // Retail selects between two loaded words with a branch. A port that scaled
  // by the bit, or masked and converted, would produce 32.0 here.
  const FlightInputFields f = route_flight_input_fields(kOut, 0xFFFFFFFFU,
                                                        FlightInputDevice::kAnalog, 0U);
  check_bits(f.at2096, 1.0F, "a set bit is exactly 1.0");
  check_bits(f.at2100, 1.0F, "");
}

void any_non_zero_layout_is_not_the_swapped_one() {
  // `cmplwi cr6,r11,1` then `beq`. Only 1 swaps; 2 does not.
  const FlightInputFields zero = route_flight_input_fields(kOut, 0U,
                                                           FlightInputDevice::kAnalog, 0U);
  const FlightInputFields two = route_flight_input_fields(kOut, 0U,
                                                          FlightInputDevice::kAnalog, 2U);
  check(zero == two, "layout 2 routes like layout 0, not like layout 1");
}

}  // namespace

int main() {
  every_arm_takes_the_two_main_axes_from_slots_zero_and_one();
  the_digital_arm_ignores_the_layout();
  the_analog_arms_disagree_and_that_is_the_point();
  the_bit_that_moves_moves_with_the_layout();
  no_other_bit_is_read();
  the_bit_result_is_exactly_one_or_zero();
  any_non_zero_layout_is_not_the_swapped_one();
  if (failures == 0) std::printf("retail_flight_input_router: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
