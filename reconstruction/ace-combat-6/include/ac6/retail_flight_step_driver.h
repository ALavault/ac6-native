#pragma once

// The step itself, ported from 0x82283898.
//
// This is **slot 11** of the flight-model base vtable 0x82008B10, inherited
// unchanged by 0x8200F270. Cycle 1379 established that having a non-empty slot
// 11 is what makes a model fly: the sibling classes override it with the shared
// empty `blr`, so this one function is the whole per-frame entry point of the
// flight model this campaign has ported.
//
//     fmr   f31,f1                  its own float argument, in seconds
//     lwz   r11,112(r31)
//     addi  r30,r11,96              r5 = [this+112] + 96, the position block
//     call slot 30 (this)                        -> 0x82302DB0, retail_flight_controls
//     call slot 31 (this, f1 = f31, r5 = r30)    -> 0x82303110, retail_flight_step
//     call slot 32 (this, f1 = f31, r5 = r30)    -> 0x82302C88, retail_flight_orientation
//   if bit 1 of [this+332]:
//     [this+360] = [this+364] = [this+304] = [this+308] = [this+312] = 0
//     call slot 33 (this, f1 = f31, r5 = r30)    -> 0x82282C50, not read
//     call 0x82282938 (this, f1 = f31, r5 = r30)
//     call 0x82326FE8 (this)
//
// ONE FLOAT REACHES EVERY STAGE, unchanged. That is the fact this header exists
// to carry, and it is why the three contracted behaviours share one `step`
// argument rather than each deriving its own.
//
// THE RESET IS BIT 1, NOT BIT 0. `rlwinm r11,r11,31,31,31` is a rotate left by
// 31 -- a rotate RIGHT by one -- keeping bit 31, which selects bit 1. Cycles
// 1371 and 1375 both wrote "bit 0". The composite differential settled it by
// measurement before the arithmetic was read: with bit 0 the run made four
// stubbed calls and changed nothing, with bit 1 it made five and zeroed exactly
// the five fields above.
//
// AND THE RESET RUNS AFTER SLOT 30, not instead of it. Slot 30 computes the
// control state and the reset then throws five of its eight outputs away. The
// three it does NOT clear -- +368, +372 and +376 -- keep what slot 30 wrote,
// which is the observable that distinguishes this ordering from any other.

#include "ac6/retail_flight_controls.h"

#include <cstdint>

namespace ac6::retail {

inline constexpr std::uint32_t kStepResetBit = 1U << 1;

// The position block the step hands to slots 31, 32 and 33.
constexpr std::uint32_t flight_step_position_block(std::uint32_t at112) noexcept {
  return at112 + 96;
}

// Slot 30 followed by the conditional reset. The integrator and the orientation
// update are not composed here: they take the position block, not the model, and
// each is contracted on its own.
FlightControlState apply_flight_step(FlightControlState state,
                                     const FlightControlInputs& inputs,
                                     float step) noexcept;

}  // namespace ac6::retail
