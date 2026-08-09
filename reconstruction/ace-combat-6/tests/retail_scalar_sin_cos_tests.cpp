#include "ac6/retail_scalar_sin_cos.h"

#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("FAIL  %s\n", w); ++failures; } }
void check_bits(float a, float b, const char* w) {
  if (std::signbit(a) != std::signbit(b) || !(a == b)) {
    std::printf("FAIL  %s  (got %.9g, want %.9g)\n", w, a, b); ++failures;
  }
}
using namespace ac6::retail;

// Values read off the micro-execution, not off libm. Several of them are values
// libm does NOT produce -- cos(pi) is -0.9999997 here and -1.0 in libm, and
// sin(pi) is -2.9057e-07 where libm gives -8.7423e-08. Those three are the whole
// reason this file exists, so they are the anchors.
struct Anchor { float angle; float sine; float cosine; };
constexpr Anchor kAnchors[] = {
    {0.0F, 0.0F, 1.0F},
    {3.1415927410125732F, -2.905726432800293e-07F, -0.9999997019767761F},
    {-3.1415927410125732F, 2.905726432800293e-07F, -0.9999997019767761F},
    {1.5707963705062866F, 0.9999999403953552F, -3.451714292168617e-08F},
    {-1.5707963705062866F, -0.9999999403953552F, -3.451714292168617e-08F},
    {0.7853981852531433F, 0.7071067690849304F, 0.7071067690849304F},
    {1.0F, 0.8414709568023682F, 0.5403023362159729F},
    {-1.0F, -0.8414709568023682F, 0.5403023362159729F},
};

void the_anchors_are_reproduced_exactly() {
  for (const Anchor& anchor : kAnchors) {
    const ScalarSinCos got = scalar_sin_cos(anchor.angle);
    check_bits(got.sine, anchor.sine, "sine");
    check_bits(got.cosine, anchor.cosine, "cosine");
  }
}

void it_is_not_the_library_and_the_test_says_where() {
  // CONTROL. If this ever agrees with libm everywhere, the port has been
  // "simplified" back into std::sin/std::cos and the seam has silently reopened.
  int disagreements = 0;
  for (const Anchor& anchor : kAnchors) {
    const ScalarSinCos got = scalar_sin_cos(anchor.angle);
    if (got.sine != std::sin(anchor.angle)) ++disagreements;
    if (got.cosine != std::cos(anchor.angle)) ++disagreements;
  }
  check(disagreements >= 4, "the port must DISAGREE with libm on several anchors");

  // And the specific one cycle 1411 measured as the worst: near pi the sine is
  // 3.3x libm's value, not a rounding difference.
  const ScalarSinCos at_pi = scalar_sin_cos(3.1415927410125732F);
  check(std::fabs(at_pi.sine) > 2.0e-07F, "sin(pi) is ~2.9e-07, not libm's 8.7e-08");
}

void the_rounding_is_half_away_from_zero() {
  // The reduction adds copysign(pi, angle) before truncating. A port using
  // floor(q + 0.5) rounds toward -inf and would break the symmetry between an
  // angle and its negation. sine is odd and cosine even to the bit here.
  for (const Anchor& anchor : kAnchors) {
    if (anchor.angle == 0.0F) continue;
    const ScalarSinCos plus = scalar_sin_cos(anchor.angle);
    const ScalarSinCos minus = scalar_sin_cos(-anchor.angle);
    check_bits(minus.sine, -plus.sine, "sine is odd to the bit");
    check_bits(minus.cosine, plus.cosine, "cosine is even to the bit");
  }
}

void negative_zero_is_not_a_special_case() {
  const ScalarSinCos got = scalar_sin_cos(-0.0F);
  check_bits(got.cosine, 1.0F, "cos(-0) is 1");
  check(got.sine == 0.0F, "sin(-0) is a zero");
}

void the_products_are_not_fused() {
  // The arbitration said: each lane product ROUNDED, then summed left to right.
  // Compilers default to -ffp-contract=fast and will fuse a*b+c into an fma,
  // which changes the last ulp and would break every anchor above. If this test
  // starts failing after a flag change, that is the cause -- CMakeLists sets
  // -ffp-contract=off for the product for exactly this reason.
  const ScalarSinCos got = scalar_sin_cos(0.7853981852531433F);
  check_bits(got.sine, 0.7071067690849304F, "pi/4 sine survives the build flags");
  check_bits(got.cosine, 0.7071067690849304F, "pi/4 cosine survives the build flags");
}

void the_reachable_domain_is_finite_and_bounded() {
  // Not a fidelity claim -- a sanity net over the sweep the differential covers,
  // so a build that mangles the reduction is caught here and not only there.
  for (int index = 0; index < 192; ++index) {
    const float angle = static_cast<float>(-3.1415927410125732 +
                                           2.0 * 3.1415927410125732 * index / 191.0);
    const ScalarSinCos got = scalar_sin_cos(angle);
    check(std::fabs(got.sine) <= 1.0001F && std::fabs(got.cosine) <= 1.0001F,
          "bounded over the sweep");
    const float unit = got.sine * got.sine + got.cosine * got.cosine;
    check(std::fabs(unit - 1.0F) < 1.0e-05F, "sin^2 + cos^2 is one to 1e-05");
  }
}

}  // namespace

int main() {
  the_anchors_are_reproduced_exactly();
  it_is_not_the_library_and_the_test_says_where();
  the_rounding_is_half_away_from_zero();
  negative_zero_is_not_a_special_case();
  the_products_are_not_fused();
  the_reachable_domain_is_finite_and_bounded();
  if (failures == 0) std::printf("retail_scalar_sin_cos: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
