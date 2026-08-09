// The flight position integrator of 0x82303110, instruction by instruction.
//
// The retail sequence, from 0x82303558 to 0x82303694:
//
//     lfs    f0,32(r31)             the model's rate scale
//     fmuls  f12,f0,f12             \
//     fmuls  f10,f0,f10              >  three rates scaled
//     fmuls  f0,f0,f9               /
//     fnmsubs f10,f25,f24,f10        f10 = f10 - f25*f24   (middle only)
//     lfs    f13,-25792(r11)         1/3.6 at 0x82069B40
//
//     lfs    f8,72(r30)
//     fmuls  f9,f0,f31               rate * step
//     fmadds f9,f9,f13,f8            += ... * (1/3.6)
//     stfs   f9,72(r30)
//
//     fmuls  f8,f10,f31
//     lfs    f9,68(r30)
//     fmadds f9,f8,f13,f9
//     stfs   f9,68(r30)
//
//     fmuls  f7,f12,f31              (issued early, at 0x82303580)
//     lfs    f9,64(r30)
//     fmadds f13,f7,f13,f9
//     stfs   f13,64(r30)
//
//     lfs    f13,68(r30)
//     lfs    f0,12820(r11)           10.0 at 0x82003214
//     fcmpu  cr6,f13,f0
//     bge    cr6,0x82303694
//     stfs   f0,68(r30)              floor, and ONLY on this component
//
// The three stores are issued in the order 72, 68, 64 and are independent, so
// the order does not affect the result -- but the floor is applied AFTER all
// three, reading 68 back from memory, and that does matter if a caller ever
// aliases the block.

#include "ac6/retail_flight_step.h"

#include <cmath>

namespace ac6::retail {

FlightRates scaled_rates(FlightRates rates, float rate_scale,
                         float mid_bias) noexcept {
  FlightRates scaled{};
  scaled.to64 = rate_scale * rates.to64;
  scaled.to68 = rate_scale * rates.to68;
  scaled.to72 = rate_scale * rates.to72;
  // fnmsubs frD,frA,frC,frB is -((frA*frC) - frB), i.e. frB - frA*frC, fused.
  // `mid_bias` is the whole product f25*f24, so the fused form is a subtraction
  // with a single rounding: fmaf(-1, mid_bias, scaled).
  scaled.to68 = std::fmaf(-1.0F, mid_bias, scaled.to68);
  return scaled;
}

FlightPosition integrate_flight_position(FlightPosition position,
                                         FlightRates rates,
                                         float rate_scale,
                                         float mid_bias,
                                         float step) noexcept {
  const FlightRates scaled = scaled_rates(rates, rate_scale, mid_bias);

  // Each component is `fmuls` then `fmadds`: the rate-times-step product is
  // rounded to single, and only the multiply-add that follows is fused.
  const float step72 = scaled.to72 * step;
  const float step68 = scaled.to68 * step;
  const float step64 = scaled.to64 * step;

  FlightPosition next{};
  next.at72 = std::fmaf(step72, kRateToStep, position.at72);
  next.at68 = std::fmaf(step68, kRateToStep, position.at68);
  next.at64 = std::fmaf(step64, kRateToStep, position.at64);

  // `bge cr6` is `bc 4,24,...`: branch when the LT bit is FALSE. An unordered
  // `fcmpu` leaves LT, GT and EQ all clear and sets SO, so a NaN TAKES the
  // branch and the floor is NOT applied.
  //
  // That is the difference between `x < floor` (false on NaN -- correct) and
  // `!(x >= floor)` (true on NaN -- wrong, and it was written that way here
  // first). Same shape as the fsel negative-zero rule in retail_input_binding.h:
  // the branch condition has to be read off the encoding, not off the mnemonic's
  // English name.
  if (next.at68 < kMidFloor) {
    next.at68 = kMidFloor;
  }
  return next;
}

}  // namespace ac6::retail
