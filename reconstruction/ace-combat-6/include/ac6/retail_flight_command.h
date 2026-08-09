#pragma once

// The flight model's command input API, ported from 0x82281608, 0x822816C0 and
// 0x82281778 -- slots 12, 13 and 14 of the base vtable 0x82008B10, inherited
// unchanged by every branch.
//
// THIS IS HOW THE GAME FLIES THE AEROPLANE. Cycle 1388 established that the
// live model's slot 30 reads its three commands from [+36], [+40] and [+44];
// these three setters are what writes them, and they are the only members of
// the flight-model family that do. A native session drives the contracted
// flight chain by calling these, and nothing else.
//
// Each takes an INCREMENT and a TARGET ANGLE and returns whether it changed
// anything -- retail returns 0 for changed and 1 for unchanged, which is
// inverted here into a bool because a C++ caller reading `if (set(...))` would
// otherwise get it backwards.
//
//     target = wrap(target) into [-pi, +pi]      ONE step, not a modulo
//     if |target| != pi:
//         changed = |current - target| > pi/180
//     else:
//         target  = current >= 0 ? +pi : -pi     fsel, so -0.0 takes +pi
//         changed = | |current| - pi | > pi/180
//     if changed:
//         [target_field] = target
//         [flag_field]   = 1                     a BYTE
//         [accumulator] += increment
//
// THE TOLERANCE IS ONE DEGREE. 0x82675554 is pi/180 -- the same value slot 32
// uses to convert its angles -- so a command within a degree of where the model
// already points is DISCARDED, increment and all. A port that always stored
// would accumulate a command retail drops.
//
// THE WRAP IS ONE STEP. `f2 += 2*pi` once if below -pi, `f2 -= 2*pi` once if
// above; nothing loops. A target three turns out stays three turns out, and the
// |target| == pi branch below is then unreachable for it. That is retail's
// behaviour and it is reproduced rather than repaired.
//
// AND THE ACCUMULATORS ARE NOT IN SLOT ORDER: slot 12 adds to [+36], slot 13 to
// [+44] and slot 14 to [+40]. Cycle 1388 derived the same crossing from the
// consumer's side -- +36 drives at304, +44 drives at308, +40 drives at312 --
// and the two derivations agree, which is the first time in this thread a field
// mapping has been met coming the other way.

#include <cstdint>

namespace ac6::retail {

// One axis's four fields, by byte offset.
struct FlightCommandSlots {
  int current{};       // +16, +20, +24
  int target{};        // +60, +68, +76
  int flag{};          // +56, +64, +72   -- a byte
  int accumulator{};   // +36, +44, +40
  bool operator==(const FlightCommandSlots&) const = default;
};

inline constexpr FlightCommandSlots kSlot12{16, 60, 56, 36};
inline constexpr FlightCommandSlots kSlot13{20, 68, 64, 44};
inline constexpr FlightCommandSlots kSlot14{24, 76, 72, 40};

inline constexpr float kPi = 3.1415927410125732F;        // 0x82069BB0
inline constexpr float kMinusPi = -3.1415927410125732F;  // 0x8206A044
inline constexpr float kTwoPi = 6.2831854820251465F;     // 0x82069BF0
inline constexpr float kOneDegree = 0.01745329238474369F; // 0x82675554, pi/180

// What one call changes. `changed` is retail's r3 inverted: retail returns 0
// when it stored and 1 when it did not.
struct FlightCommandResult {
  bool changed{};
  float target{};       // what would be written to the target field
  float accumulator{};  // the accumulator after the increment
  bool operator==(const FlightCommandResult&) const = default;
};

FlightCommandResult set_flight_command(float current, float accumulator,
                                       float increment, float target) noexcept;

}  // namespace ac6::retail
