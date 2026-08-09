#pragma once

// The rate servo, ported from 0x822831E8.
//
// WHERE IT SITS. It is a DIRECT call -- not a virtual -- made by the live flight
// model's step 0x82306A38 (contracted as retail_live_flight_step) immediately
// after slot 30, and by 0x82329968. It is the FIRST CONSUMER of the control
// state that retail_live_flight_axes produces: it reads +304, +308 and +312 and
// turns them into three rates at +144, +148 and +152.
//
// 166 instructions, no call and no vector instruction, and its whole footprint
// is those three floats -- measured statically before it was read.
//
// THE STEP PASSES r5 = this+304 AND THE FUNCTION NEVER USES IT. Every read is
// off r3. The argument is real and dead, and saying so is cheaper than a reader
// later assuming it matters.
//
// WHAT IT COMPUTES, per axis:
//
//     target   = axis * limit
//     gap      = target - rate                       fmsubs, fused
//     gain     = |axis| >= 2^-16 ? driven_gain * saturation
//                                : centred_gain
//     rate    += gain * gap * step                   fmadds, fused
//     clamp rate to [-limit, +limit]
//
// where `saturation` is a per-axis bias added to the rate's own normalised
// magnitude and clamped to [0, 1]:
//
//     saturation = clamp(|rate / limit| + bias, 0, 1)
//
// THE THREE BIASES DIFFER: 0.5 for the first axis (0x82001354), 0.8 for the
// second (0x82069ECC) and 0.4 for the third (0x82069CB4). They are not one
// constant used three times, and a port that shared one is wrong on two axes.
//
// AND THE GAIN SWITCH IS THE OTHER WAY ROUND FROM THE OBVIOUS GUESS. The
// saturation term scales the DRIVEN gain, not the centred one: `bne` on
// "|axis| < eps" jumps to the centred branch, which loads its gain and uses it
// unscaled. So a saturated aircraft loses authority while the stick is deflected
// and recovers it at neutral, which is the opposite of a damping term.
//
// WHEN THE LIMIT IS DEGENERATE -- |limit| < 2^-16 -- the normalised magnitude is
// not computed at all; retail reads it from a sixteen-byte block at 0x826EB940,
// which is ALL ZEROS. So the saturation collapses to the bias alone. That is
// modelled as zero rather than as a division guard, because it is what the image
// contains and not what would be sensible.

#include <cstdint>

namespace ac6::retail {

struct FlightRates3 {
  float at144{};
  float at148{};
  float at152{};
  bool operator==(const FlightRates3&) const = default;
};

// One axis's inputs. `driven` is [+544..+552], `centred` is [+560..+568].
struct RateServoAxis {
  float axis{};
  float limit{};
  float driven_gain{};
  float centred_gain{};
  bool operator==(const RateServoAxis&) const = default;
};

inline constexpr float kServoEpsilon = 1.52587890625e-05F;  // 2^-16, 0x82069C2C
inline constexpr float kBias144 = 0.5F;                     // 0x82001354
inline constexpr float kBias148 = 0.800000011920929F;       // 0x82069ECC
inline constexpr float kBias152 = 0.4000000059604645F;      // 0x82069CB4

FlightRates3 update_flight_rate_servo(FlightRates3 rates,
                                      const RateServoAxis& first,
                                      const RateServoAxis& second,
                                      const RateServoAxis& third,
                                      float step) noexcept;

}  // namespace ac6::retail
