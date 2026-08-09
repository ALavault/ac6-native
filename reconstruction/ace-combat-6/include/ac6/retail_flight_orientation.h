#pragma once

// The three rotation angles of the orientation update 0x82302C88.
//
// WHERE IT SITS. 0x82302C88 is **slot 32** of the vtable 0x8200F270, the third
// pure virtual the step 0x82283898 calls -- after the control surfaces
// 0x82302DB0 (retail_flight_controls.h) and the position integrator 0x82303110
// (retail_flight_step.h), with the same float and the same position block.
//
// It applies three rotations and then extracts Euler angles. **The rotations are
// already in the product**: 0x820A9B30, 0x820A99F8 and 0x82211828 are
// rotate_820A9B30 / rotate_820A99F8 / rotate_82211828 in retail_transform.h,
// derived at A3.1 from 0x822A1E80 -- a different caller in a different
// subsystem. Slot 32 calls them in the same order A3.1 recorded, which is how
// the flight controller and the transform kernel were shown to be one kernel
// rather than two substitutes (cycle 1376).
//
// WHAT IS PORTED HERE is only the part that was missing: the **angles**. That is
// pure scalar arithmetic on the three control axes slot 30 produces and three
// per-aircraft limits, and it is the piece a differential can reach without the
// VMX128 register-file bridge.
//
// WHAT IS NOT PORTED. The Euler extraction at the end of slot 32:
//
//     [model+16] = -asin(clamp([pos+52], -1, +1))     via 0x82380570
//     [model+24] =  atan2([pos+20], [pos+36])         via 0x820936E8
//     [model+20] =  atan2([pos+48], [pos+56])         via 0x820936E8
//
// Both routines were measured against libm and are IDENTICAL at 0 ulp over 42
// cases (analysis/flight/flight-math-seams.tsv) -- with one exception that a
// port must carry: 0x820936E8 returns **zero** when both arguments are below
// 2^-16, where std::atan2 returns an angle. Substituting it unguarded puts a
// 45-degree error into the orientation on exactly the frames where the aircraft
// is level.
//
// THE ANGLES ARE IN RADIANS. Each is computed in degrees-per-second terms and
// multiplied by pi/180 at 0x82069BF4 = 0.01745329238474369, which is
// float32(pi/180) correctly rounded -- unlike 0x82008AD8, the seven-digit
// literal 0.3183099 that cycle 1374 caught masquerading as 1/pi.

#include <cstdint>

namespace ac6::retail {

// The three per-aircraft rate limits, at [model+1248], [+1252], [+1256]. They
// are set by slot 9 (0x82282408) and, on the sibling class only, resampled from
// a speed-interpolated performance table by 0x82283480 -- see
// analysis/flight/performance-table-lookup.tsv. The class this header describes
// does NOT resample them: it leaves slot 29 as the shared empty virtual
// (cycle 1379).
struct FlightRotationLimits {
  float at1248{};   // the row-0 rotation, 0x820A99F8
  float at1252{};   // the row-1 rotation, 0x820A9B30
  float at1256{};   // the row-2 rotation, 0x82211828
  bool operator==(const FlightRotationLimits&) const = default;
};

// The three control axes slot 30 writes.
struct FlightRotationAxes {
  float at304{};    // -> row 0
  float at308{};    // -> row 1
  float at312{};    // -> row 2
  bool operator==(const FlightRotationAxes&) const = default;
};

// In radians, named by the basis row each one rotates about, because that is
// what retail_transform.h calls them and a second vocabulary for one thing is
// how two ports drift apart.
struct FlightRotationAngles {
  float about_row0{};
  float about_row1{};
  float about_row2{};
  bool operator==(const FlightRotationAngles&) const = default;
};

inline constexpr float kDegreesToRadians = 0.01745329238474369F;  // 0x82069BF4
inline constexpr float kRow1Scale = 0.06666667014360428F;         // 1/15, 0x82007D5C
inline constexpr float kRow2Scale = 0.6666666865348816F;          // 2/3,  0x82069C1C

// The divisor of the row-0 angle, from 0x82302B78. It is **7.0** (0x82069D1C)
// unless **bit 4** of [model+332] is set, in which case 0x822A6400 is called on
// [model+428] and 7.0 is multiplied by what it returns.
//
// Cycle 1376's report said "bit 3". It is bit 4: `rlwinm r11,r10,28,31,31` is a
// rotate left by 28 -- a rotate RIGHT by four -- keeping bit 31, which selects
// bit 4 of the word. Corrected here rather than left in a report.
inline constexpr float kRow0Divisor = 7.0F;
inline constexpr std::uint32_t kRow0DivisorScaleBit = 1U << 4;

// THE CLAMPS ARE NOT ALIKE, and that is the detail a plausible port gets wrong.
//
//   row 1 is clamped SYMMETRICALLY to [-limit, +limit]  (0x82302CCC..0x82302CE4)
//   row 0 is clamped ABOVE ONLY, at +limit              (0x82302BE4..0x82302BF0)
//   row 2 is clamped ABOVE ONLY, at +limit              (0x82302D24..0x82302D30)
//
// So a large negative row-0 or row-2 axis is not bounded by its limit at all.
// The control `both_upper_clamps_are_one_sided` fails if all three are made
// symmetric, which is the tidier and wrong reading.
FlightRotationAngles flight_rotation_angles(const FlightRotationLimits& limits,
                                            const FlightRotationAxes& axes,
                                            float step,
                                            float row0_divisor) noexcept;

}  // namespace ac6::retail
