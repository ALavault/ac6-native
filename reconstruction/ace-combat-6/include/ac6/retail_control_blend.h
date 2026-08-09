#pragma once

// The blended control accessors, ported from 0x82281E10 and 0x82281EA8.
//
// They are **slots 17 and 18** of the flight-model base vtable 0x82008B10,
// inherited unchanged by both 0x8200F270 and 0x8200F310 -- so unlike almost
// everything else in this thread, one port covers both branches.
//
// They close the loop the campaign has been building one function at a time:
//
//     retail_live_flight_axes    writes the control axes  +304, +312
//     retail_flight_rate_servo   turns them into rates    +144, +152
//     retail_live_flight_step    decays the stored values +408, +412
//     THESE                      blend all three into one clamped control value
//
// EACH IS 38 INSTRUCTIONS, no call and no vector, and they are the same function
// with different fields and a different constant. Kept as one implementation
// with parameters rather than two copies, because a later divergence between the
// two would then be a visible edit rather than a silent drift.
//
// THE TWO CONSTANTS ARE SEVEN-DIGIT DECIMAL LITERALS, and that is now the third
// and fourth of them in this subsystem:
//
//     slot 17   0x82007F78 = 0.6366198062896729    float32(0.6366198), ~2/pi
//     slot 18   0x82008AD8 = 0.3183099031448364    float32(0.3183099), ~1/pi
//
// Neither is the correctly rounded reciprocal: float32(2/pi) and float32(1/pi)
// are different words. Cycle 1374 found the second of these masquerading as 1/pi
// and cycle 1386 found 0.15915495 masquerading as 1/(2*pi). A port writing
// `2.0f/M_PI` is one ulp off on every use.
//
// THE BIT IS BIT 1 -- the same reset bit the two steps use, decoded from
// `rlwinm r11,r11,31,31,31`, a rotate RIGHT by one.

#include <cstdint>

namespace ac6::retail {

inline constexpr std::uint32_t kBlendResetBit = 1U << 1;
inline constexpr float kBlendEpsilon = 1.52587890625e-05F;   // 2^-16, 0x82069C2C
inline constexpr float kSlot17Scale = 0.6366198062896729F;   // 0x82007F78
inline constexpr float kSlot18Scale = 0.3183099031448364F;   // 0x82008AD8

// `value` is what retail returns in f1; `stored` is what it leaves in +408 or
// +412, and `wrote` says whether it stored at all -- on the non-reset path the
// field is READ and not written, so a caller that always wrote back would
// overwrite the decay the step applies.
struct ControlBlend {
  float value{};
  float stored{};
  bool wrote{};
  bool operator==(const ControlBlend&) const = default;
};

// On the reset path the rate, scaled, REPLACES the axis when its magnitude is
// larger -- `ble` skips the move, so the replacement needs a strict greater-than
// and equal magnitudes keep the axis. Off it, the stored value is ADDED to the
// axis, and only when its own magnitude is at or above 2^-16.
//
// The clamp is two early returns: below -1 it returns -1, above +1 it returns
// +1, and a NaN takes neither branch and is returned unchanged.
ControlBlend blend_control_axis(float axis, float rate, float stored,
                                std::uint32_t flags332, float scale) noexcept;

}  // namespace ac6::retail
