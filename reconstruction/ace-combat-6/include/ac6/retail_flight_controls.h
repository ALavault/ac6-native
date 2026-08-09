#pragma once

// The flight control-surface update, ported from 0x82302DB0.
//
// WHERE IT SITS. It is **slot 30** of the vtable 0x8200F270 -- the first of the
// three pure virtuals that the base-class step 0x82283898 calls, immediately
// before the position integrator 0x82303110 (slot 31, see retail_flight_step.h)
// and 0x82302C88 (slot 32). The step passes it `this` and its own float, so the
// `step` argument here is the same scalar the integrator gets, in seconds.
//
// WHY IT IS PORTABLE IN ONE PIECE. 216 instructions with **no call and no vector
// instruction** -- measured, not assumed, by counting mnemonics across the whole
// body. Its footprint was bounded before it was read: a capsule seeded the model
// with a per-offset pattern and dumped it back, and the eight words it changed
// are exactly the eight `stfs` offsets in the listing.
//
//     reads    +36 +40 +44 +48 +52   the commands
//              +304 +308 +312        the three axes, read-modify-write
//              +332                  a flag word; only bit 2 is used
//              +344                  the same field that scales gravity in the
//                                    integrator (retail_flight_step.h)
//              +360 +364 +368 +372   the ramps, read-modify-write
//     writes   +304 +308 +312 +360 +364 +368 +372 +376
//
// FIVE OF THOSE EIGHT ARE THE ONES THE STEP CLEARS. Cycle 1371 read
// 0x82283898 zeroing +360, +364, +304, +308 and +312 when bit 0 of [this+332] is
// set, before calling slot 33. So this function's outputs are the state that a
// reset drops on the floor, which is the strongest available evidence that they
// ARE the control state rather than a scratch area.
//
// THE CONSTANTS ARE FOURTEEN WORDS OF THE IMAGE, all resolved in
// analysis/flight/flight-integrator-constants.tsv and its slot-30 companion:
// 10/3 at 0x82007B90, 0.5 at 0x82001354, 0.0 at 0x8200082C, 1.0 at 0x82001348,
// 10.0 at 0x82003214, 0.8 at 0x82069ECC, 0.99 at 0x82069E50, 5/3 at 0x82007B8C,
// 2.5 at 0x82002FDC, 0.1 at 0x82002FD4, 0.7 at 0x82002FD0, 0.9 at 0x82069C3C,
// -0.9 at 0x82007F84, -1.0 at 0x82069B28.
//
// NAMES. The offsets keep their numbers. Nothing in the binary names these
// fields, the class has no RTTI (cycle 1369), and a guessed name -- "aileron",
// "airbrake" -- would be a claim this port cannot support. What the code says
// about their ROLES is written against each field below and nowhere else.

#include <cstdint>

namespace ac6::retail {

// The eight words slot 30 writes. All are read back on the next frame, so this
// is state, not output.
struct FlightControlState {
  float at304{};   // an axis: asymmetric, clamped to [-0.9, +1.0]
  float at308{};   // an axis: driven by the SIGN of its command, not its value
  float at312{};   // an axis: symmetric, clamped to [-1, +1]
  float at360{};   // a ramp in [0, 1]
  float at364{};   // a ramp in [0, 1]
  float at368{};   // a second ramp, gated on at360 > 0.99
  float at372{};   // a second ramp, gated on at364 > 0.99
  float at376{};   // at360 - at364, written and never read here
  bool operator==(const FlightControlState&) const = default;
};

// Everything slot 30 reads and does not write.
struct FlightControlInputs {
  float cmd36{};       // -> at304. Scaled by 1.0 when positive, 0.9 when negative
  float cmd40{};       // -> at312
  float cmd44{};       // -> at308, by sign only
  float hold48{};      // != 0 raises at360
  float hold52{};      // != 0 raises at364
  float field344{};    // the gravity-scaling field; see `flags332` bit 2
  std::uint32_t flags332{};
  bool operator==(const FlightControlInputs&) const = default;
};

// Bit 2 of [+332] -- `rlwinm r10,r11,30,31,31` is `(word >> 2) & 1`. When set,
// all three axis rates are divided by `(field344 + 0.1) * 10`, which is an
// authority reduction that depends on the same field gravity is scaled by.
inline constexpr std::uint32_t kAxisRateScaleBit = 1U << 2;

// The ramps at +360/+364 fall at half the rate they rise: `10/3` per second up,
// and `10/3 * 0.5` per second down. The secondary ramps at +368/+372 use `10.0`
// up and `10.0 * 0.8` down, and only rise once their primary passes 0.99.
inline constexpr float kRampRate = 3.3333332538604736F;        // 0x82007B90
inline constexpr float kRampDecayFactor = 0.5F;                // 0x82001354
inline constexpr float kSecondRampRate = 10.0F;                // 0x82003214
inline constexpr float kSecondRampDecayFactor = 0.800000011920929F;   // 0x82069ECC
inline constexpr float kSecondRampGate = 0.9900000095367432F;  // 0x82069E50

// The axes self-centre at 0.7 of their rate before the command is applied.
inline constexpr float kAxisRate = 1.6666666269302368F;        // 0x82007B8C
inline constexpr float kAt308Rate = 2.5F;                      // 0x82002FDC
inline constexpr float kAxisCentringFactor = 0.699999988079071F;  // 0x82002FD0
inline constexpr float kAt304NegativeGain = 0.8999999761581421F;  // 0x82069C3C
inline constexpr float kAt304LowerLimit = -0.8999999761581421F;   // 0x82007F84
inline constexpr float kRateScaleOffset = 0.10000000149011612F;   // 0x82002FD4

// AN INTERLOCK, and it reads the other way round from the obvious guess.
// 0x82302EF8 branches PAST the two stores when either at360 or at364 is exactly
// zero, so the zeroing runs only when BOTH are non-zero: the two secondary ramps
// cannot both be live while both primaries are. Reading `beq` as "skip" rather
// than "do" is the whole difference, and the test
// `both_primaries_live_kills_both_secondaries` fails if it is inverted.
FlightControlState update_flight_controls(FlightControlState state,
                                          const FlightControlInputs& inputs,
                                          float step) noexcept;

}  // namespace ac6::retail
