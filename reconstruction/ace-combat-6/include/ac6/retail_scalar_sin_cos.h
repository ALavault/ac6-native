#pragma once

// XMScalarSinCos, ported from 0x8209CB70.
//
// WHY THIS EXISTS. `retail_transform.h` has carried a seam since cycle 1325: the
// rotation kernel called `std::sin`/`std::cos` where retail calls this routine,
// and its own comment said so. Cycle 1411 measured how far apart they are, over
// the whole reachable domain rather than at seven angles:
//
//     412 values: 170 identical, 182 within 2 ulp, 60 worse
//     worst absolute error 3.20e-07 (sine), 2.98e-07 (cosine)
//
// Every rotation in the flight chain goes through it, so that was the largest
// fidelity gap left on the scalar side. This closes it: the port reproduces the
// micro-executed routine at 412 of 412 values, bit for bit.
//
// WHAT THE ROUTINE IS. 92 instructions, a leaf, no calls. A Taylor series to the
// 23rd order, evaluated four terms at a time in the vector unit:
//
//     q  = trunc((a + copysign(pi, a)) * float32(1/(2*pi)))    0x8209CBEC..CC00
//     x  = fmaf(-(float)q, float32(2*pi), a)                   0x8209CC1C
//     v  = (1, x, x^2, x^3)                                    0x8209CC38
//     cos = dot(v*v,        1/0!, -1/2!, 1/4!, -1/6!)
//         + dot(v*v*x^8,    1/8!, -1/10!, 1/12!, -1/14!)
//         + dot(v*v*x^16,   1/16!, -1/18!, 1/20!, -1/22!)
//     sin = the same over the odd lanes, 1/1! through -1/23!
//
// WHERE EACH NUMBER COMES FROM. 1/(2*pi) is loaded at 0x8209CBF4, off the base
// 0x8209CBF0 builds, from 0x82069BEC; 2*pi at 0x8209CB84 from 0x82069BF0; the vector's leading 1.0 at
// 0x8209CB8C from 0x820542BC; and the six coefficient vectors by the six
// `lvx128` between 0x8209CB9C and 0x8209CBD8, spanning 0x8204F6F0 through
// 0x8204F740.
//
// HOW THE SIX PARTIAL SUMS COMBINE. high + mid first, at 0x8209CCB4 for cosine
// and 0x8209CCC0 for sine, then the low term. Cosine is stored at 0x8209CCCC
// and sine at 0x8209CCD8, so the LAST write is sine even though sine is the
// first output pointer -- an order this port preserves because "not observable
// today" is not "safe to reorder".
//
// THE REDUCTION IS BUILT OUT OF THE ARGUMENT'S OWN WORD. `rlwimi r11,r10,30,1,31`
// at 0x8209CBE0 rotates 0x01243F6D left by 30 -- which is pi's bit pattern,
// 0x40490FDB -- and inserts bits 1..31 of it over the angle's, keeping the
// angle's SIGN BIT. That is `copysign(pi, a)` written as an integer insert, and
// adding it before scaling by 1/(2*pi) turns a truncation into round-half-away-
// from-zero. A port that used `std::round` or `floor(q + 0.5)` would differ on
// negative angles and on exact halves.
//
// THE SUBTRACTION IS FUSED. `fnmsubs` is one rounding, so `std::fmaf` and not
// `a - q * two_pi`. This is the same detail as `retail_flight_step`'s.
//
// THE ONE THING THAT WAS ARBITRATED. `vmsum4fp128` is a four-lane dot product and
// the ISA reference this campaign has does not fix its summation order, which
// decides the last ulp. Eight candidates -- four association orders crossed with
// fused and unfused products -- were scored against the micro-executed result at
// all 412 values:
//
//     ((p0 + p1) + p2) + p3, products rounded first   412/412   <- the only one
//     the same with fused products                    298/412
//     (p0 + p1) + (p2 + p3), products rounded         292/412
//     right-to-left or reversed, either way           223 or 188/412
//
// One reading reproduces every value; the next best misses 114. That is an
// arbitration by cross-match, and `--arbitrate` re-runs it.
//
// WHAT IS ASSERTED AND WHAT IS NOT. The micro-execution ran with
// `asserted_semantics` EMPTY -- no model of this campaign's was involved, every
// instruction came from the SLEIGH module. So this port reproduces the executed
// retail code exactly. It does NOT establish that the module reproduces the
// console; that is the campaign's standing assumption, and the summation order
// above is the place where it would show if it were wrong.
//
// THE COEFFICIENTS ARE EXACT RECIPROCAL FACTORIALS, checked rather than assumed:
// two constants in this campaign turned out to be seven-digit decimals wearing a
// reciprocal's clothes (cycles 1374, 1386). These are not, and neither is
// float32(1/(2*pi)) at 0x82069BEC -- it is the correctly rounded reciprocal.
//
// Verified by `tools/audit_scalar_sin_cos_microexec.py`: 206 angles over
// [-pi, pi], 412 values, no tolerance.

namespace ac6::retail {

// sine and cosine, in the order 0x8209CB70 writes them: [r3] then [r4].
struct ScalarSinCos {
  float sine{};
  float cosine{};
  bool operator==(const ScalarSinCos&) const = default;
};

// 2*pi and 1/(2*pi) as the image holds them, at 0x82069BF0 and 0x82069BEC.
inline constexpr float kTwoPi = 6.2831854820251465F;
inline constexpr float kInverseTwoPi = 0.15915493667125702F;
// 0x40490FDB, the word the rlwimi inserts.
inline constexpr float kPi = 3.1415927410125732F;

ScalarSinCos scalar_sin_cos(float angle) noexcept;

}  // namespace ac6::retail
