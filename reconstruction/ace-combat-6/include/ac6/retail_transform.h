#pragma once

// THE transform kernel. One implementation, shared by flight orientation and by
// rendered-unit orientation.
//
// THIS IS THE POINT OF THE FILE AND IT IS A CONSTRAINT, NOT A STYLE NOTE.
// `0x822A1E80` sits on both paths: it orients the aircraft and it orients drawn
// units. Two separate ports -- a `FlightOrientation::build_matrix()` and a
// `RenderedUnit::build_matrix()` -- would be two chances to pick a different row
// or column convention, and the two would disagree in a way no single test
// catches, because each would be self-consistent. There is one kernel here and
// callers use it.
//
// WHAT 0x822A1E80 IS, from cycles 1325-1327. It is 40 instructions with no vector
// arithmetic of its own: it copies three constant vectors out of .rodata --
// 0x8204F7F0, 0x8204F800, 0x8204F810, read and found to be (1,0,0,0), (0,1,0,0),
// (0,0,1,0) -- into object+0x10/+0x20/+0x30, then calls three rotations.
//
// Twenty cycles treated it as the matrix assembly and hunted a VMX128 defect
// inside it. There is none: the defect was that Ghidra decodes vpermwi128's
// IMMEDIATE wrongly at 536 of 545 sites, so the rotations were running on wrong
// selectors. With the immediate decoded from the instruction word, the whole
// closure returns the identity at zero angles.
//
// THE COMPOSITION ORDER IS NOT THE ARGUMENT ORDER:
//
//     reset the basis to the identity
//     rotate about row 1 by angle2      0x820A9B30, called FIRST
//     rotate about row 0 by angle1      0x820A99F8, called SECOND
//     rotate about row 2 by angle3      0x82211828, called THIRD
//
// Each rotation is exact, measured over twelve sentinel cases with an asymmetric
// basis at four angles: it leaves one row untouched and rotates the other two in
// their plane. Worst deviation 1.5e-06, exactly zero at every zero angle.
//
// THE ROWS ARE NOT NAMED PITCH, YAW AND ROLL. Nothing measured which physical
// axis a row is. They are row 0, row 1 and row 2, and a caller that knows better
// may say so at its own level.
//
// WHAT IS NOT PORTED: the object's first 16 bytes. 0x822A1E80 never writes them,
// and the sentinel (17, 29, 43, 61) placed there came back untouched in all
// thirteen runs.

#include <array>
#include <cstddef>

namespace ac6::retail {

// One row of the basis: four floats, as retail stores them.
using BasisRow = std::array<float, 4>;

// The three rows at object+0x10, +0x20, +0x30. The object's first 16 bytes are
// deliberately absent: this type is the part that has been qualified.
struct RetailBasis {
  std::array<BasisRow, 3> rows{};
  bool operator==(const RetailBasis&) const = default;
};

// The identity 0x822A1E80 copies in, from the three constants named above.
RetailBasis identity_basis() noexcept;

// THE TRIGONOMETRY IS THE ONE SEAM THAT IS NOT BIT-QUALIFIED, AND IT IS NAMED
// RATHER THAN HIDDEN.
//
// Retail reaches cosine and sine through `0x8209CB70`, identified as
// XMScalarSinCos and reproduced by micro-execution including its argument
// reduction. It has NOT been ported. This uses the host library, so the native
// kernel does not agree with retail to the bit -- which is why the differential
// test carries a tolerance, and the tolerance exists for this reason and no
// other.
//
// HOW FAR OFF, MEASURED RATHER THAN ESTIMATED (cycle 1411). The claim used to be
// "seven angles" and "about 1e-06". `tools/audit_flight_math_seams.py` now sweeps
// 206 angles across the whole reachable domain [-pi, pi] -- the range the command
// setters wrap to -- and compares both outputs:
//
//     412 values: 170 identical, 182 within 2 ulp, 60 worse
//     worst absolute error 3.20e-07 (sine, near -pi); 2.98e-07 (cosine)
//
// The large ulp figures all sit where the true value is near zero, so ulp
// exaggerates them and the absolute error is the figure that means anything.
// Seven angles could not have found this and did not: a substitution needs a
// sweep of the domain, not chosen points. (Cycle 1410 refused a different port
// for the same reason, over one argument in 96.)
//
// The two other seams in this chain were swept at the same time and came back
// IDENTICAL at 0 ulp -- 0x82380570 against std::asin over 212 arguments and
// 0x820936E8 against a guarded std::atan2 over 214. So this one is the outlier,
// not the norm, and porting it would close the last scalar gap in the kernel.
//
// It is a seam so that porting XMScalarSinCos later changes one function rather
// than the kernel. A replay that must match retail bit for bit over many frames
// will need that port; a single frame does not notice.
struct SinCos {
  float sine{};
  float cosine{};
};
SinCos retail_sin_cos(float angle) noexcept;

// One plane rotation, given the pair. `kept` is the row left untouched, and the
// other two rotate in their own plane:
//
//     a' =  a*cosine + sign*sine*b
//     b' = -sign*sine*a + b*cosine
//
// The three retail rotations differ only in which row they keep and in `sign`,
// and both were measured rather than assumed:
//
//     0x820A9B30  keeps row 1, sign -1     (the middle axis, opposite the others)
//     0x820A99F8  keeps row 0, sign +1
//     0x82211828  keeps row 2, sign +1
//
// Taking the pair rather than the angle keeps the arithmetic separable from the
// trigonometry: this part IS exact, and a test can check it without the seam.
void rotate_basis(RetailBasis& basis, int kept, int sign, SinCos pair) noexcept;

// The three, named by their retail address because nothing established which
// physical axis each one turns.
void rotate_820A9B30(RetailBasis& basis, float angle) noexcept;
void rotate_820A99F8(RetailBasis& basis, float angle) noexcept;
void rotate_82211828(RetailBasis& basis, float angle) noexcept;

// 0x822A1E80 entire. The parameter names follow the retail argument registers,
// so `angle2` is the one applied first -- deliberately awkward, because the
// alternative is a name that hides the order.
RetailBasis assemble_basis(float angle1, float angle2, float angle3) noexcept;

}  // namespace ac6::retail
