#pragma once

// The slot edge and auto-repeat bank, ported from 0x82211988.
//
// It is the last callee of 0x82211DF8 and the one the frame's elapsed time
// actually reaches. Cycles 1356 and 1357 both named an earlier function as the
// float's consumer and both were wrong: f1 survives 0x82211B40 (51 instructions,
// no floating-point operation) and 0x82211C10 (121, likewise) and is used here.
//
// WHAT THE FLOAT IS, FROM ITS USE. It is summed into per-slot timers compared
// against a delay and a repeat rate, so for this consumer it is an elapsed time
// per frame. It drives INPUT AUTO-REPEAT, not flight integration -- the plan
// pointed at this float while looking for the flight controller, and the
// integrator is not on this path.
//
// `.pdata` has NO ROW for 0x82211988, so its length carries no independent
// control; tools/check_listing_against_pdata.py reports that rather than
// guessing, and this port rests on the recompiled corpus alone.
//
// THE EDGES ARE THE SAME ALGEBRA THE PRODUCT ALREADY HAS. `button_edges` in
// retail_input.h computes (prev ^ cur) & cur and (prev ^ cur) & ~cur on device
// buttons; this computes cur & ~prev and prev & ~cur on SLOTS. Equal sets, read
// from a function that shares nothing with the one that rule came from.

#include <array>
#include <cstdint>

namespace ac6::retail {

inline constexpr std::size_t kSlotCount = 32;

// One 8-byte pair from this+0x1060, one per slot.
struct AutoRepeatSlot {
  float timer{};
  float limit{};
  bool operator==(const AutoRepeatSlot&) const = default;
};

// [this+0x1058] and [this+0x105C]. Both are fields, not constants: retail loads
// them per tick and a port that bakes in a delay is not this function.
struct AutoRepeatConfig {
  float initial_delay{};
  float repeat_interval{};
  bool operator==(const AutoRepeatConfig&) const = default;
};

// The three mask words 0x82211988 writes, at this+0xE4C, +0xE50 and +0xE54.
struct SlotEdges {
  std::uint32_t newly_active{};    // +0xE4C -- cur & ~prev, and nothing else
  std::uint32_t newly_inactive{};  // +0xE50 -- prev & ~cur
  std::uint32_t triggered{};       // +0xE54 -- newly_active PLUS every repeat
  bool operator==(const SlotEdges&) const = default;
};

// One slot's tick. Returns whether it fired.
//
//   inactive : timer = 0, limit = initial_delay, no fire
//   active   : fire when timer >= limit; on firing, limit becomes the repeat
//              interval and the timer is reset to zero -- and THEN the elapsed
//              time is added, so a slot that has just fired holds `elapsed`,
//              not zero. The order is the retail order and it matters at the
//              first repeat.
bool tick_auto_repeat(AutoRepeatSlot& slot, bool active, float elapsed,
                      const AutoRepeatConfig& config) noexcept;

// The whole bank, plus the two edge words.
SlotEdges tick_slot_repeat(std::uint32_t active, std::uint32_t previous,
                           std::array<AutoRepeatSlot, kSlotCount>& slots,
                           float elapsed,
                           const AutoRepeatConfig& config) noexcept;

}  // namespace ac6::retail
