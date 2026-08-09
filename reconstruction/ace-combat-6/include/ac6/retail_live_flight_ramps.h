#pragma once

// The ramp update of the LIVE flight model, ported from 0x82303E68.
//
// WHICH MODEL, and why this one rather than the one already contracted.
// Cycle 1384 established that the entity carries two flight models and that the
// 0x8200F270 instance at entity+2224 is CONSTRUCTED, DESTRUCTED AND NEVER
// ADDRESSED -- three sites in the whole corpus compute its address, and two of
// them are its own constructor and destructor. The model that runs is the
// 0x8200F310 branch, reached through entity+4912 and stepped through slot 15.
//
// 0x82303E68 is that model's slot 30. retail_flight_controls.h models the OTHER
// class's slot 30, faithfully, and it is not what flies.
//
// THE INTERFACE IS SHARED, THE ARITHMETIC IS NOT. Both write the same eight
// fields; this one writes two more. Both read the same five commands at
// +36..+52 and the same flag word at +332. But 0x82303E68 loads none of the
// other version's 10/3, 5/3, 2.5, 0.7, 0.99, 0.9, -0.9, 0.8 or 0.5 -- so the
// shapes differ as well as the numbers, and nothing here is an adaptation of
// that port.
//
// WHAT THIS HEADER COVERS is the ramp block, 0x82303EA4..0x82303FAC, which
// writes +360, +364, +368, +372 and +376 and nothing else. Verified rather than
// assumed: every store to r31 in the function was listed in order, and those
// five offsets appear only before 0x82303FB0. The three axes that follow --
// +304, +308, +312, +1352, +1356 -- are a separate slice.
//
// A FIRST-ORDER LAG, NOT A RAMP. The other class moves its ramps at a fixed rate
// per second. This one moves them a FRACTION OF THE REMAINING DISTANCE:
//
//     [+360] += ([+48] - [+360]) * ([+952] * step)
//
// with the coefficient carried per model at +952 and +956. That is an
// exponential approach, and at a large step it overshoots rather than
// saturating -- there is no clamp on either lag. The test
// `a_large_step_overshoots_because_there_is_no_clamp` pins that, because a port
// that "helpfully" clamped would look more correct and be wrong.

#include <cstdint>

namespace ac6::retail {

// The five fields the ramp block writes.
struct LiveRampState {
  float at360{};
  float at364{};
  float at368{};   // in [0, 1], driven by at360 against the threshold
  float at372{};   // in [0, 1], driven by at364
  float at376{};   // at360 - at364, recomputed on BOTH paths
  bool operator==(const LiveRampState&) const = default;
};

struct LiveRampInputs {
  float cmd48{};        // the target of at360
  float cmd52{};        // the target of at364
  float rate952{};      // at360's lag coefficient, per second
  float rate956{};      // at364's
  float threshold404{}; // both secondary ramps compare against this
  bool gate1224{};      // a BYTE at +1224; when zero, at368 is forced to zero
  std::uint32_t flags332{};
  bool operator==(const LiveRampInputs&) const = default;
};

// Bit 7 of [+332] -- `rlwinm r11,r11,25,31,31` is a rotate left by 25, a rotate
// RIGHT by seven, keeping bit 31. When set, retail dispatches slot 38 instead
// and the whole lag block is skipped; +376 is still recomputed afterwards, on
// the shared path. Slot 38 is not modelled, so a caller setting this bit gets
// the state back with only +376 refreshed, which is what retail does to these
// five fields and no claim about what slot 38 does to others.
inline constexpr std::uint32_t kBypassBit = 1U << 7;

// The two ceilings are 1.0 from TWO different addresses -- 0x82008ACC for +368
// and 0x82008AD0 for +372. Equal words, separate constants, and they are kept
// separate here because nothing guarantees a later game build keeps them equal.
inline constexpr float kAt368Ceiling = 1.0F;   // 0x82008ACC
inline constexpr float kAt372Ceiling = 1.0F;   // 0x82008AD0

LiveRampState update_live_flight_ramps(LiveRampState state,
                                       const LiveRampInputs& inputs,
                                       float step) noexcept;

}  // namespace ac6::retail
