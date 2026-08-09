#pragma once

// The LIVE flight model's per-frame step, ported from 0x82306A38.
//
// This is **slot 15** of the vtable 0x8200F310. Cycle 1379 established that the
// 0x8200F310 branch overrides slot 11 -- the base class's step -- with the
// shared empty `blr` and is driven from slot 15 instead; cycle 1384 established
// that this branch is the one the entity actually drives. So this function, not
// 0x82283898, is what runs every frame on the aeroplane.
//
//     fmr   f31,f1                 its own float, in seconds
//     mr    r30,r5                 the position block, passed in
//     call slot 30 (this)                      0x82303E68  contracted
//     call 0x82283168 (this, f1, f2 = [+376])
//     call 0x822831E8 (this, f1, r5 = this+304)
//   unless bit 7 of [this+332]:
//     bit 6 set  -> call slot 39 (this, f1, r5)
//     bit 6 clear-> call slot 31 (this, f1, r5), then 0x82304AB8
//     call slot 32 (this, f1, r5)
//     call 0x82282938 (this, f1, r5)
//   if bit 1 of [this+332]:  call slot 33 (this, f1, r5)
//   else:                    decay [+412] and [+408]
//     call 0x82281C18 (this, f1 = [+32], f2 = [r5+68])
//     call 0x82282E20 (this, f1 = [r5+68])
//     call 0x82283480 (this, f1 = [+32])        the performance-table lookup
//     call 0x82326FE8 (this)
//
// TWO THINGS IT SHARES WITH THE OTHER STEP. 0x82282938 and 0x82326FE8 are called
// by 0x82283898 as well, in the same positions relative to the virtuals. And the
// reset bit is bit 1 of [+332] in both -- the same bit, selecting slot 33 in
// both. The two drivers are variants of one design, which is why the earlier
// reading of 0x82283898 carried over.
//
// AND ONE IT DOES NOT: it calls the performance-table lookup 0x82283480 EVERY
// FRAME. Cycle 1383 measured that; cycle 1378 had said it happened only on
// reset, and this is the function that refutes it.
//
// WHAT THIS HEADER MODELS is the part that is arithmetic rather than dispatch:
// the two exponential decays. Everything else is a call, and the composite
// differential exercises the dispatches against the contracted slot 30.

#include <cstdint>

namespace ac6::retail {

// Bit 7 skips the three attitude virtuals entirely; bit 6 substitutes slot 39
// for slot 31; bit 1 substitutes slot 33 for the two decays. All three are
// rotate-mask idioms and all three were decoded by hand and then confirmed by
// running the flag values -- `rlwinm rD,rS,25,31,31` is a rotate RIGHT by seven.
inline constexpr std::uint32_t kSkipAttitudeBit = 1U << 7;
inline constexpr std::uint32_t kAlternateSlot39Bit = 1U << 6;
inline constexpr std::uint32_t kResetBit = 1U << 1;

// 3.0 at 0x8200134C, and the same 2^-16 the axes use.
inline constexpr float kDecayRate = 3.0F;
inline constexpr float kDecayEpsilon = 1.52587890625e-05F;

// [+412] and [+408]. Each decays by `value * step * 3` -- an exponential, fused
// as `fmadds` -- and only while its own magnitude is at or above 2^-16. Below
// that the field is LEFT ALONE rather than snapped to zero, which is the
// opposite of what the axis blocks in slot 30 do with the same epsilon, and the
// test `below_the_epsilon_the_field_is_left_alone` fails if the two are
// conflated.
struct LiveStepDecays {
  float at408{};
  float at412{};
  bool operator==(const LiveStepDecays&) const = default;
};

LiveStepDecays apply_live_step_decays(LiveStepDecays state,
                                      std::uint32_t flags332,
                                      float step) noexcept;

}  // namespace ac6::retail
