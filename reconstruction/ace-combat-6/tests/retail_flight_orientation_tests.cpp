// Regression tests for the rotation angles of 0x82302C88.

#include "ac6/retail_flight_orientation.h"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::printf("FAIL  %s\n", what);
    ++failures;
  }
}

void check_bits(float actual, float expected, const char* what) {
  if (std::signbit(actual) != std::signbit(expected) || !(actual == expected)) {
    std::printf("FAIL  %s  (got %.9g, want %.9g)\n", what, actual, expected);
    ++failures;
  }
}

using ac6::retail::flight_rotation_angles;
using ac6::retail::FlightRotationAngles;
using ac6::retail::FlightRotationAxes;
using ac6::retail::FlightRotationLimits;
using ac6::retail::kDegreesToRadians;
using ac6::retail::kRow0Divisor;
using ac6::retail::kRow0DivisorScaleBit;
using ac6::retail::kRow1Scale;
using ac6::retail::kRow2Scale;

constexpr float kFrame = 0.016666668F;
const FlightRotationLimits kLimits{5.0F, 1.399999976158142F, 5.400000095367432F};

FlightRotationAngles run(FlightRotationAxes axes, float step = kFrame,
                         float divisor = kRow0Divisor) {
  return flight_rotation_angles(kLimits, axes, step, divisor);
}

void the_constants_are_the_words_of_the_image() {
  check_bits(kDegreesToRadians, 0.01745329238474369F, "pi/180 at 0x82069BF4");
  check_bits(kDegreesToRadians, static_cast<float>(3.14159265358979323846 / 180.0),
             "and it is pi/180 correctly rounded");
  check_bits(kRow1Scale, 0.06666667014360428F, "1/15 at 0x82007D5C");
  check_bits(kRow2Scale, 0.6666666865348816F, "2/3 at 0x82069C1C");
  check_bits(kRow0Divisor, 7.0F, "7.0 at 0x82069D1C");
  check(kRow0DivisorScaleBit == 16U, "the divisor-scale flag is bit 4, not bit 3");
  // The defaults the base constructor writes; they are also this suite's limits.
  check_bits(kLimits.at1248, 5.0F, "the row-0 default limit");
  check_bits(kLimits.at1252, 1.399999976158142F, "the row-1 default limit");
  check_bits(kLimits.at1256, 5.400000095367432F, "the row-2 default limit");
}

void zero_axes_give_zero_angles() {
  const FlightRotationAngles out = run(FlightRotationAxes{});
  check_bits(out.about_row0, 0.0F, "row 0 is zero");
  check_bits(out.about_row1, 0.0F, "row 1 is zero");
  check_bits(out.about_row2, 0.0F, "row 2 is zero");
}

void each_axis_drives_exactly_one_rotation() {
  const FlightRotationAngles only304 = run(FlightRotationAxes{1.0F, 0.0F, 0.0F});
  check(only304.about_row0 != 0.0F, "at304 drives row 0");
  check_bits(only304.about_row1, 0.0F, "at304 does not reach row 1");
  check_bits(only304.about_row2, 0.0F, "at304 does not reach row 2");

  const FlightRotationAngles only308 = run(FlightRotationAxes{0.0F, 1.0F, 0.0F});
  check(only308.about_row1 != 0.0F, "at308 drives row 1");
  check_bits(only308.about_row0, 0.0F, "at308 does not reach row 0");

  const FlightRotationAngles only312 = run(FlightRotationAxes{0.0F, 0.0F, 1.0F});
  check(only312.about_row2 != 0.0F, "at312 drives row 2");
  check_bits(only312.about_row1, 0.0F, "at312 does not reach row 1");
}

void the_products_are_in_retails_order() {
  // Row 1: (limit * step) * (1/15) * axis, then pi/180. Grouping differently
  // rounds differently, so this is bit equality against the same order.
  const float axis = 0.37777779F;
  float value = kLimits.at1252 * kFrame;
  value = value * kRow1Scale;
  value = value * axis;
  check_bits(run(FlightRotationAxes{0.0F, axis, 0.0F}).about_row1,
             value * kDegreesToRadians, "row 1 keeps retail's product order");

  float row0 = 1.0F / kRow0Divisor;
  row0 = row0 * kLimits.at1248;
  row0 = row0 * kFrame;
  row0 = row0 * axis;
  check_bits(run(FlightRotationAxes{axis, 0.0F, 0.0F}).about_row0,
             row0 * kDegreesToRadians, "row 0 keeps retail's product order");
}

void row_one_is_clamped_on_both_sides() {
  // A step of 1000 saturates every axis.
  const FlightRotationAngles high =
      run(FlightRotationAxes{0.0F, 1.0F, 0.0F}, 1000.0F);
  check_bits(high.about_row1, kLimits.at1252 * kDegreesToRadians,
             "row 1 saturates at +limit");
  const FlightRotationAngles low =
      run(FlightRotationAxes{0.0F, -1.0F, 0.0F}, 1000.0F);
  check_bits(low.about_row1, -kLimits.at1252 * kDegreesToRadians,
             "row 1 saturates at -limit");
}

void both_upper_clamps_are_one_sided() {
  // THE ASYMMETRY. Rows 0 and 2 have no lower bound: a large negative axis is
  // not clamped at all, and the angle grows without limit. Making all three
  // symmetric is the tidier and wrong reading.
  const FlightRotationAngles high =
      run(FlightRotationAxes{1.0F, 0.0F, 1.0F}, 1000.0F);
  check_bits(high.about_row0, kLimits.at1248 * kDegreesToRadians,
             "row 0 saturates at +limit");
  check_bits(high.about_row2, kLimits.at1256 * kDegreesToRadians,
             "row 2 saturates at +limit");

  const FlightRotationAngles low =
      run(FlightRotationAxes{-1.0F, 0.0F, -1.0F}, 1000.0F);
  check(low.about_row0 < -kLimits.at1248 * kDegreesToRadians,
        "row 0 is NOT clamped below -- it runs past -limit");
  check(low.about_row2 < -kLimits.at1256 * kDegreesToRadians,
        "row 2 is NOT clamped below -- it runs past -limit");
}

void the_row0_divisor_scales_only_row_zero() {
  const FlightRotationAxes axes{0.5F, 0.5F, 0.5F};
  const FlightRotationAngles plain = run(axes);
  const FlightRotationAngles scaled = run(axes, kFrame, kRow0Divisor * 2.0F);
  check(scaled.about_row0 != plain.about_row0, "the divisor changes row 0");
  check(std::fabs(scaled.about_row0) < std::fabs(plain.about_row0),
        "and a larger divisor makes row 0 smaller");
  check_bits(scaled.about_row1, plain.about_row1, "row 1 is untouched");
  check_bits(scaled.about_row2, plain.about_row2, "row 2 is untouched");
}

void the_controls_all_bite() {
  int symmetric = 0;
  int swapped_scales = 0;
  int degrees_not_radians = 0;
  // The step has to be large enough that the clamps are actually reached: with
  // a frame-sized step the row-0 product never gets near -limit, and the
  // symmetric-clamp control reported zero disagreements on the first run --
  // the thirty-second shape, a domain that cannot express the difference.
  const float step = 100.37777F;
  for (int i = -32; i <= 32; ++i) {
    const float axis = static_cast<float>(i) / 32.0F * 3.7F;
    const FlightRotationAxes axes{axis, axis, axis};
    const FlightRotationAngles real = run(axes, step);

    // 1. All three clamps symmetric.
    float row0 = 1.0F / kRow0Divisor;
    row0 = row0 * kLimits.at1248;
    row0 = row0 * step;
    row0 = row0 * axis;
    if (row0 < -kLimits.at1248) {
      row0 = -kLimits.at1248;
      if (row0 * kDegreesToRadians != real.about_row0) {
        ++symmetric;
      }
    }
    // 2. The two scales exchanged between rows 1 and 2.
    float swapped = kLimits.at1252 * step;
    swapped = swapped * kRow2Scale;
    swapped = swapped * axis;
    if (swapped > kLimits.at1252) {
      swapped = kLimits.at1252;
    }
    else if (!(swapped >= -kLimits.at1252)) {
      swapped = -kLimits.at1252;
    }
    if (swapped * kDegreesToRadians != real.about_row1) {
      ++swapped_scales;
    }
    // 3. Angles left in degrees.
    if (real.about_row1 != 0.0F &&
        real.about_row1 / kDegreesToRadians != real.about_row1) {
      ++degrees_not_radians;
    }
  }
  std::printf("controls: symmetric-clamp=%d swapped-scales=%d radians=%d\n",
              symmetric, swapped_scales, degrees_not_radians);
  check(symmetric > 0, "CONTROL a symmetric row-0 clamp must disagree");
  check(swapped_scales > 0, "CONTROL exchanging 1/15 and 2/3 must disagree");
  check(degrees_not_radians > 0, "CONTROL the result must be in radians");
}

}  // namespace

int main() {
  the_constants_are_the_words_of_the_image();
  zero_axes_give_zero_angles();
  each_axis_drives_exactly_one_rotation();
  the_products_are_in_retails_order();
  row_one_is_clamped_on_both_sides();
  both_upper_clamps_are_one_sided();
  the_row0_divisor_scales_only_row_zero();
  the_controls_all_bite();
  if (failures != 0) {
    std::printf("retail_flight_orientation: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("retail_flight_orientation: all cases passed\n");
  return 0;
}
