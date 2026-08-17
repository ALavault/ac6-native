#pragma once

// The world-position resolver, ported from 0x822953F0.
//
// This function was the goal's named debt: the native world places units at the
// relative offsets of their Obj sub-records, because nothing had said how the
// payload's numbers become world coordinates. 0x822953F0 says it.
//
// Historical analyses loaded the program as A2ALT-32addr and stopped at
// `halt_baddata()`. The canonical project now uses PowerPC:BE:64:Xenon; the
// VMX128 read below is therefore qualified against the active project.
//
// Its one call site is 0x82295BF0, the default arm of the ten-way switch at
// 0x82295B90 that 0x82295A88 runs for a tag-2 order:
//
//     82295bd0  stfs f31,0x50(r1)     ; the caller's out vector, zeroed
//     82295bd4  or   r5,r30,r30       ; r5 = the order's position record
//     82295bdc  addi r4,r1,0x50       ; r4 = the out vector
//     82295be4  or   r3,r31,r31       ; r3 = the acting unit, which is unused
//     82295bf0  bl   0x822953f0
//
// The record it reads, and the rule:
//
//     82295410  lfs f0,0x8(r31)   \
//     8229541c  lfs f0,0xc(r31)    >  the triple at +0x08, +0x0C, +0x10
//     82295428  lfs f0,0x10(r31)  /
//     82295414  lbz r10,0x42(r31) ; the mode byte
//     82295420  cmplwi cr6,r10,0x1
//     82295438  bne cr6,0x8229555c ; not 1: the triple IS the world position
//
// When the mode byte is 1, bytes +0x43 and +0x44 name a unit through
// 0x82270380 on the unit manager at global+0x2D3B4; 0x82093808 takes that
// unit's heading as atan2 of its +0x30 and +0x38; 0x8209CB70 turns the heading
// into a sine and a cosine; three vmsum3fp128 rotate the triple by that heading
// about the vertical axis - the middle row of the matrix is the constant
// (0,1,0) at 0x8204F800, so the y is never rotated - and the result is added to
// the unit's +0x40, +0x44 and +0x48.
//
// Measured on Mission 01, the mode byte separates two populations that were not
// separated by hand: its 811 mode-0 records span x in [-57600, 63000] and z in
// [-63000, 55600], while its 79 mode-1 records have median 0 on all three axes
// and span at most a few thousand. World coordinates on one side, offsets on
// the other, exactly as the branch requires.
//
// Cycle 1124 anchored those offsets to a layout instead of to one function's
// habits. The unit constructor at 0x820A77BC-0x820A7818 writes three constant
// vectors into a fresh object and zeroes a fourth:
//
//     object + 0x20  <- (1,0,0,0)   at 0x8204F7F0
//     object + 0x30  <- (0,1,0,0)   at 0x8204F800
//     object + 0x40  <- (0,0,1,0)   at 0x8204F810
//     object + 0x50  <- 0, 0, 0     stored one float at a time
//
// which is a four-row transform - X, Y, Z, translation - initialised to the
// identity. And 0x82270380 returns object+0x10 rather than the object
// (0x82270434: addi r3,r31,0x10), so this resolver's +0x30 and +0x38 are the
// object's +0x40 and +0x48 - the forward row's x and z, which is what a heading
// of atan2(x, z) needs - and its +0x40, +0x44, +0x48 are the object's +0x50,
// +0x54, +0x58: the translation. Everything cycle 1122 read lands where the
// constructor puts it.

#include "ac6/retail_scenario.h"

#include <cstdint>
#include <optional>

namespace ac6::retail {

// What the resolver needs from the anchor unit: the position at object +0x40,
// +0x44, +0x48, and the heading 0x82093808 computes from +0x30 and +0x38.
struct AnchorFrame {
  ScenarioVector position{};
  float heading{};  // radians, atan2(forward.x, forward.z)
  bool operator==(const AnchorFrame&) const = default;
};

// The heading of 0x82093808: atan2 of the object's +0x30 and +0x38, which
// 0x820936E8 answers with a constant when both are under its epsilon rather
// than calling atan2 on two zeros.
float anchor_heading(float forward_x, float forward_z) noexcept;

// Bit 0 of the record's +0x40. Retail then replaces the y with a value a
// virtual on the object at global+0x36084 returns, plus the record's own y.
// What that object is has not been established, so the caller supplies the
// height or the resolver refuses.
inline constexpr std::uint16_t kPositionHeightFromQuery = 0x0001;

// 0x822953F0. Returns nothing when the record needs something the caller did
// not supply - an anchor for mode 1, a queried height for the flag - because a
// resolver that guessed either would be inventing a coordinate.
std::optional<ScenarioVector> resolve_world_position(
    const ScenarioPositionRecord& record,
    const AnchorFrame* anchor = nullptr,
    const float* queried_height = nullptr) noexcept;

}  // namespace ac6::retail
