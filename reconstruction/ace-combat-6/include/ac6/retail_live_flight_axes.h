#pragma once

// The three axis blocks of the LIVE flight model, ported from
// 0x82303FB0..0x823042B0 -- the rest of slot 30 of the vtable 0x8200F310, whose
// ramp block is retail_live_flight_ramps.h.
//
// Together the two headers cover the whole of 0x82303E68: every store to r31 in
// that function writes one of the ten fields the pair models, and the split
// between them is at 0x82303FB0, verified by listing the stores in address
// order rather than by eye.
//
// THE RATES COME FROM TWO SIXTEEN-BYTE BLOCKS. Retail copies [+576] to the
// stack at 80..95 and [+592] to 96..111, then multiplies six of those eight
// floats by the step. Element i of the first block is axis i's COMMAND rate and
// element i of the second is its DECAY rate:
//
//     +304   command [+576+0]   decay [+592+0]
//     +308   command [+576+4]   decay [+592+4]
//     +312   command [+576+8]   decay [+592+8]
//
// The fourth element of each block is copied and never used.
//
// EACH AXIS HAS TWO REGIMES, chosen by |command| against 2^-16 -- the same word
// (0x82069C2C) that gates the vector normalise in the integrator and guards
// atan2 in 0x820936E8. Below it the axis DECAYS toward zero at its decay rate,
// without crossing; at or above it the axis is driven.
//
// AND THE DRIVE IS NOT LINEAR for +304 and +312. The target is
//
//     cmd > 0:   cmd*cmd + cmd        fmadds f0,f0,f0,f0
//     cmd < 0:   cmd - cmd*cmd        fnmsubs f0,f0,f0,f0
//
// times 2/3 -- that is sign(cmd) * (|cmd| + cmd*cmd) * 2/3, which REACHES 4/3 at
// full deflection and is then clamped to 1 by the limit rather than by the
// curve. A port that used the command directly is smooth, plausible and wrong.
//
// +308 IS DIFFERENT AGAIN, and it is the one with the timers. It has no curve
// and no lag: it steps by its command rate in the direction of the command, and
// it maintains two HOLD TIMERS at +1352 and +1356 -- one per direction --
// incremented by the step while that direction is held and decayed at TEN TIMES
// the step otherwise, floored at zero. Nothing in this function reads them.

#include <cstdint>

namespace ac6::retail {

// The five fields the axis blocks write.
struct LiveAxisState {
  float at304{};
  float at308{};
  float at312{};
  float at1352{};   // how long the +308 command has been held positive
  float at1356{};   // ... and negative
  bool operator==(const LiveAxisState&) const = default;
};

// One axis's pair of per-second rates, from element i of the two blocks.
struct LiveAxisRates {
  float command{};
  float decay{};
  bool operator==(const LiveAxisRates&) const = default;
};

struct LiveAxisInputs {
  float cmd36{};    // drives at304
  float cmd44{};    // drives at308 -- note the offsets are NOT in axis order
  float cmd40{};    // drives at312
  LiveAxisRates rates304{};
  LiveAxisRates rates308{};
  LiveAxisRates rates312{};
  bool operator==(const LiveAxisInputs&) const = default;
};

inline constexpr float kCommandEpsilon = 1.52587890625e-05F;  // 2^-16, 0x82069C2C
inline constexpr float kCurveScale = 0.6666666865348816F;     // 2/3,  0x82069C1C
inline constexpr float kLagGain = 1.5F;                       // 0x82002FD8
inline constexpr float kTimerDecayFactor = 10.0F;             // 0x82003214
inline constexpr float kUpperLimit = 1.0F;                    // 0x82001348
inline constexpr float kLowerLimit = -1.0F;                   // 0x82069B28

LiveAxisState update_live_flight_axes(LiveAxisState state,
                                      const LiveAxisInputs& inputs,
                                      float step) noexcept;

}  // namespace ac6::retail
