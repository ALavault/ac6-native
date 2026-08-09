#pragma once

// The flight position integrator, ported from the tail of 0x82303110.
//
// WHERE IT SITS, and this took five cycles to establish because none of it is
// reachable by following call edges:
//
//   0x82303110 has NO direct caller. Its only non-.pdata reference is one .rdata
//   word at 0x8200F2EC -- slot +0x7C (index 31) of the 36-slot vtable 0x8200F270,
//   which carries ZERO at vtable-4 and so has no RTTI (cycle 1369).
//
//   0x8200F270 belongs to a class built by the ctor/dtor/deleting-dtor triple
//   0x82302B28 / 0x82302B68 / 0x82302C28, constructed as a member subobject at
//   entity+2224 of the 10,672-byte object of 0x8222BEC8 -- an ACE6::CAce6Thread
//   carrying ACE6::CAce6ObjDesc and seven galib::CGaLocator poses (cycle 1370).
//
//   Its base class 0x82282220 installs 0x82008B10, whose slots 30, 31 and 32 all
//   hold 0x82380428 = _purecall. They are PURE VIRTUAL, and 0x82303110 is this
//   class's override of number 31. The base's slot 11, 0x82283898, is the step:
//
//       fmr   f31,f1                 its own float argument
//       lwz   r11,112(r31)
//       addi  r30,r11,96             r5 = [this+112] + 96
//       call slot 30 (this)
//       call slot 31 (this, f1 = f31, r5 = r30)      <- 0x82303110
//       call slot 32 (this, f1 = f31, r5 = r30)
//
//   so ONE float reaches every stage of the update (cycle 1371).
//
// WHAT `r30` IS, and cycle 1368 left this unsaid. `mr r30,r5` at 0x8230312C is
// the ONLY write to r30 in 359 instructions, so the integrated position is not a
// field of the flight model at all: it lives at [model+112] + 96 + 64/68/72.
//
// THE CONSTANTS ARE TWO WORDS OF THE IMAGE, read rather than assumed:
// 0x82069B40 = 0.2777777910232544 (exactly 1/3.6) and 0x82003214 = 10.0.
//
// THE FUSED OPERATIONS MATTER. Retail integrates with `fmadds`, which rounds
// once; `a * b + c` in C++ rounds twice and disagrees in the last ulp. Every
// `fmadds`/`fnmsubs` below is std::fmaf for that reason, and the control
// `integration is fused, not multiply-then-add` fails without it.

#include <cstdint>

namespace ac6::retail {

// The three position components, at r5 + 64 / 68 / 72. Named by offset because
// nothing in the binary names them: `mid` is the one carrying the 10.0 floor,
// which is what an altitude would carry, but that reading is not derived.
struct FlightPosition {
  float at64{};
  float at68{};
  float at72{};
  bool operator==(const FlightPosition&) const = default;
};

// The three rates, before the model's own scale is applied. They are f12, f10
// and f0 at 0x82303560..0x82303570, and they land on at64, at68 and at72
// respectively -- note the middle rate is the one the bias corrects.
struct FlightRates {
  float to64{};
  float to68{};
  float to72{};
  bool operator==(const FlightRates&) const = default;
};

// 1/3.6 at 0x82069B40, verbatim as the image stores it.
inline constexpr float kRateToStep = 0.2777777910232544F;

// 10.0 at 0x82003214, the floor applied to `at68` and to nothing else.
inline constexpr float kMidFloor = 10.0F;

// UNRESOLVED, AND DELIBERATELY AN ARGUMENT RATHER THAN A GUESS.
//
//   rate_scale is [model+32], clamped just above at 0x82303548 against a value
//   in f26 that this cycle did not read.
//   mid_bias is the product f25*f24 in `fnmsubs f10,f25,f24,f10`, i.e. the
//   middle rate has a correction subtracted before integration. Neither factor
//   was traced to its source.
//
// Both are inputs here. A port that dropped them would silently be a different
// function, and a port that invented values for them would be worse.
FlightPosition integrate_flight_position(FlightPosition position,
                                         FlightRates rates,
                                         float rate_scale,
                                         float mid_bias,
                                         float step) noexcept;

// The scaled, bias-corrected rates the integrator also feeds to its direction
// output at [model+128/132/136]. Exposed because the integration and the
// direction share these three values, and computing them twice is how the two
// drift apart. The direction write itself is NOT ported: it normalises unless
// all three components fall below an epsilon held in f11, and f11 was not read.
FlightRates scaled_rates(FlightRates rates, float rate_scale,
                         float mid_bias) noexcept;

}  // namespace ac6::retail
