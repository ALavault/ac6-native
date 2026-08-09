#pragma once

// The flight model's INPUT accumulators, ported from five thirteen-instruction
// functions: 0x82281FB0, 0x82282020, 0x82281FE8, 0x82281F40 and 0x82281F78.
//
// THESE ARE THE ANSWER TO SIX CYCLES OF SEARCH. Cycles 1399-1404 looked for
// whatever turns a controller into a flight command, and looked for it as a
// CALLER OF THE VIRTUAL SETTERS at slots 12, 13 and 14. It found five
// refutations and no caller, because the premise was wrong: the input path does
// not use virtual dispatch at all.
//
//     0x82234040   the entity's per-frame function
//       -> 0x82229250   copies the contracted binding layer's output arrays
//                       from [player+0xE58...] into entity+2104..+2116
//         -> 0x82227E10 reads those and calls THESE FIVE, directly, on the live
//                       flight model at entity+4912
//
// So the virtual setters (retail_flight_command, contracted at cycle 1393) are
// the AI's interface -- they take a target angle and discard anything within a
// degree -- and these five are the player's. Two interfaces onto the same three
// fields, and the campaign found the second one first.
//
// EACH IS THE SAME THIRTEEN INSTRUCTIONS:
//
//     f0 = [field] + f1
//     [field] = f0
//     if f0 < lower:  [field] = lower
//     else if f0 > upper: [field] = upper
//
// with the intermediate STORED BEFORE the clamp, exactly as the blend accessors
// do (retail_control_blend), so an aliasing caller sees the unclamped value.
//
// THE TWO GROUPS HAVE DIFFERENT LOWER BOUNDS, and that is the detail that says
// what they are:
//
//     +36, +40, +44   lower -1.0 (0x82069B28)   the three signed axis commands
//     +48, +52        lower  0.0 (0x8200082C)   the two HOLD inputs
//
// +36, +40 and +44 are the commands retail_live_flight_axes reads; +48 and +52
// are the holds retail_live_flight_ramps reads. Five accumulators, and between
// them they are every input the contracted slot 30 consumes.

#include <cstdint>

namespace ac6::retail {

inline constexpr float kInputUpperLimit = 1.0F;    // 0x82001348
inline constexpr float kAxisLowerLimit = -1.0F;    // 0x82069B28
inline constexpr float kHoldLowerLimit = 0.0F;     // 0x8200082C

// Which field each function accumulates into, by byte offset.
inline constexpr int kAccumulator36 = 36;   // 0x82281FB0
inline constexpr int kAccumulator40 = 40;   // 0x82282020
inline constexpr int kAccumulator44 = 44;   // 0x82281FE8
inline constexpr int kAccumulator48 = 48;   // 0x82281F40
inline constexpr int kAccumulator52 = 52;   // 0x82281F78

// Add and clamp. `lower` is -1 for the three axes and 0 for the two holds; the
// upper bound is 1 for all five.
//
// The comparison is `blt` then `ble`: below the lower bound it stores the lower
// bound, at or below the upper it returns, and above it stores the upper. A NaN
// takes neither branch's early exit and lands on the upper store -- `blt` is
// false and `ble` is false -- so a NaN comes out as +1.0, which `x < lower` and
// `!(x <= upper)` reproduce in that order and no other ordering does.
float accumulate_flight_input(float value, float increment, float lower) noexcept;

}  // namespace ac6::retail
