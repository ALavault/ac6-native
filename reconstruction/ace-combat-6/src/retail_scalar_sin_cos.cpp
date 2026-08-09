#include "ac6/retail_scalar_sin_cos.h"

#include <cmath>
#include <cstdint>

namespace ac6::retail {
namespace {

// 0x8204F6F0..0x8204F74F, six 16-byte vectors, read from the image. Named by the
// register that loads each and by which series it belongs to.
constexpr float kCosLow[4] = {1.0F, -0.5F, 0.0416666679084301F,
                              -0.0013888889225199819F};
constexpr float kSinLow[4] = {1.0F, -0.1666666716337204F, 0.008333333767950535F,
                              -0.00019841270113829523F};
constexpr float kCosMid[4] = {2.4801587642286904e-05F, -2.755731998149713e-07F,
                              2.08767581000302e-09F, -1.147074536050896e-11F};
constexpr float kSinMid[4] = {2.7557318844628753e-06F, -2.5052107943679403e-08F,
                              1.6059044372074283e-10F, -7.647163609812713e-13F};
constexpr float kCosHigh[4] = {4.7794772561329454e-14F, -1.5619206814541513e-16F,
                               4.110317590937049e-19F, -8.896790959566848e-22F};
constexpr float kSinHigh[4] = {2.8114573589663704e-15F, -8.220635078476521e-18F,
                               1.9572941524685808e-20F, -3.868170297964731e-23F};

// vmsum4fp128. The products are rounded to float BEFORE the sum -- not fused --
// and the sum runs strictly left to right. Both halves of that were arbitrated
// against 412 executed values; see the header. A `+` chain written the obvious
// way happens to be this order, and it is spelled out anyway so that a later
// reader does not "simplify" it into a tree.
float dot4(const float (&lanes)[4], const float (&coefficients)[4]) noexcept {
  const float p0 = lanes[0] * coefficients[0];
  const float p1 = lanes[1] * coefficients[1];
  const float p2 = lanes[2] * coefficients[2];
  const float p3 = lanes[3] * coefficients[3];
  return ((p0 + p1) + p2) + p3;
}

}  // namespace

ScalarSinCos scalar_sin_cos(float angle) noexcept {
  // 0x8209CBE0. The insert keeps the argument's sign bit and overwrites the
  // rest with pi's, so this is copysign(pi, angle) done as an integer edit.
  // Adding it before the scale makes the truncation below round half away from
  // zero, which is why `floor(q + 0.5)` is not a substitute.
  const float biased = std::copysign(kPi, angle) + angle;
  const float quotient = biased * kInverseTwoPi;

  // 0x8209CBFC, `fctiwz`: truncate toward zero, saturating at the int32 limits.
  // On the reachable domain |quotient| <= 1, but the saturation is carried
  // rather than assumed away.
  float clamped = quotient;
  if (!(clamped > -2147483648.0F)) clamped = -2147483648.0F;
  if (!(clamped < 2147483647.0F)) clamped = 2147483647.0F;
  const auto whole = static_cast<std::int32_t>(clamped);

  // 0x8209CC1C, `fnmsubs`: ONE rounding, so fmaf and not a - q * two_pi.
  const float x = std::fmaf(-static_cast<float>(whole), kTwoPi, angle);

  // 0x8209CC38 loads (1, x, x^2, x^3) from the four words the scalar side just
  // wrote; the first is the 1.0 at 0x820542BC.
  const float x2 = x * x;
  const float base[4] = {1.0F, x, x2, x2 * x};

  // 0x8209CC40 squares it lane-wise, and 0x8209CC54 multiplies that by the splat
  // of lane 1. The step at 0x8209CC58 is lane 3 of the square times lane 2 of
  // the BASE -- x^6 * x^2 -- and taking it from the square instead gives x^10.
  float even[4];
  float odd[4];
  for (int lane = 0; lane < 4; ++lane) {
    even[lane] = base[lane] * base[lane];
    odd[lane] = even[lane] * x;
  }
  const float step = even[3] * base[2];

  const float cos_low = dot4(even, kCosLow);
  const float sin_low = dot4(odd, kSinLow);

  float even8[4];
  float odd8[4];
  float even16[4];
  float odd16[4];
  for (int lane = 0; lane < 4; ++lane) {
    even8[lane] = even[lane] * step;
    odd8[lane] = odd[lane] * step;
  }
  for (int lane = 0; lane < 4; ++lane) {
    even16[lane] = even8[lane] * step;
    odd16[lane] = odd8[lane] * step;
  }

  const float cos_mid = dot4(even8, kCosMid);
  const float sin_mid = dot4(odd8, kSinMid);
  const float cos_high = dot4(even16, kCosHigh);
  const float sin_high = dot4(odd16, kSinHigh);

  // 0x8209CCB4..0x8209CCD8. The grouping is retail's: high + mid first, then the
  // low term. Summing the three in any other order changes the last ulp.
  ScalarSinCos result{};
  result.cosine = (cos_high + cos_mid) + cos_low;
  result.sine = (sin_high + sin_mid) + sin_low;
  return result;
}

}  // namespace ac6::retail
