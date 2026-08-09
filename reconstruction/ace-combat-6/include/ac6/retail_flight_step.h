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
// THE CONSTANTS ARE WORDS OF THE IMAGE, read rather than assumed. Cycle 1374
// resolved all fifteen this function loads, into
// analysis/flight/flight-integrator-constants.tsv; the four that matter here are
// 0x82069B40 = 1/3.6, 0x82003214 = 10.0, 0x8200F308 = 9.8/3.6 and
// 0x82069C2C = 2^-16. One of them is a trap: 0x82008AD8 is 0.3183099031448364,
// which is the seven-digit DECIMAL LITERAL 0.3183099 and NOT float32(1/pi)
// (0.31830987334251404). A port writing 1.0f/M_PI would be one ulp off.
//
// THE FUSED OPERATIONS MATTER. Retail integrates with `fmadds`, which rounds
// once; `a * b + c` in C++ rounds twice and disagrees in the last ulp. Every
// `fmadds`/`fnmsubs` below is std::fmaf for that reason, and the control
// `integration is fused, not multiply-then-add` fails without it.

#include <cstdint>

namespace ac6::retail {

// The three position components, at r5 + 64 / 68 / 72. Named by offset because
// nothing in the binary names them -- but `at68` is the vertical one, on the two
// grounds set out under kGravityKmhPerSecond below. The offset names are kept
// anyway: they are what the evidence is anchored to, and a rename would make the
// derivation unreadable against the listing.
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

// WHICH COMPONENT IS WHICH AXIS, and cycle 1372 wrote this as an open question.
//
// `at68` is the VERTICAL component. Two independent supports, neither of them a
// name:
//
//   1. It is the only one carrying a floor, and the floor is 10.0 -- the shape
//      of a minimum-altitude clamp and of nothing else in a position step.
//   2. Its bias `mid_bias` is a GRAVITY term. `f25` is built at 0x82303300:
//
//          lfs   f0,344(r31)          a model field
//          fcmpu cr6,f0,f26           against 0.0
//          lfs   f25,-3320(r11)       9.8/3.6, at 0x8200F308
//          beq   -> keep it
//          fmuls f0,f0,30.0           otherwise scale by [model+344] * 30
//          fmuls f25,f0,f25
//
//      and 0x8200F308 is float32(9.8/3.6) EXACTLY -- not float32(9.81/3.6),
//      which is 2.7249999. Only the middle component has it subtracted.
//
// THIS ALSO SETTLES THE UNIT of the step. Cycle 1372 said of 1/3.6: "the
// constant is measured, the unit assignment is not." The same function loads
// 9.8/3.6 from the class's own constant pool, so the image carries g and the
// km/h->m/s divisor as a matched pair: their ratio is 9.8. `step` is seconds
// and the rates are km/h.
inline constexpr float kGravityKmhPerSecond = 2.722222328186035F;

// 2**-16 at 0x82069C2C. The epsilon deciding whether the vector normalise at
// 0x823035EC runs: when all three scaled rates are below it in absolute value
// the block is skipped. The normalise is still not ported -- but the threshold
// is no longer unread, and cycle 1373's capsule seeded 1e30 in its place only to
// force that branch, which its docstring states.
inline constexpr float kNormaliseEpsilon = 1.52587890625e-05F;

// UNRESOLVED, AND DELIBERATELY AN ARGUMENT RATHER THAN A GUESS.
//
//   rate_scale is [model+32]. Just above the window, 0x82303548 clamps it from
//   below against f26 -- which cycle 1374 read: f26 is 0.0, from 0x8200082C. So
//   retail's own rate_scale is never negative on entry to this window. The port
//   does NOT enforce that, because the clamp is outside what it claims to port.
//
//   mid_bias is the product f25*f24 in `fnmsubs f10,f25,f24,f10`. Cycle 1374
//   resolved f25: it is 9.8/3.6, scaled by [model+344] * 30 when that field is
//   non-zero (0x82303300). f24 is `frsp(f1) * 0.3183099` at 0x82303278, where f1
//   is returned by a call this cycle did not follow -- so the product is still an
//   argument.
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
// drift apart. The direction write itself is NOT ported: it normalises with
// vrsqrtefp unless all three components fall below kNormaliseEpsilon, and that
// block is VMX128 -- outside what the micro-execution differential covers, which
// tools/audit_flight_step_microexec.py states before it states anything else.
FlightRates scaled_rates(FlightRates rates, float rate_scale,
                         float mid_bias) noexcept;

}  // namespace ac6::retail
