#include "ac6/retail_transform.h"

#include <cmath>

namespace ac6::retail {

RetailBasis identity_basis() noexcept {
  // The three constants at 0x8204F7F0, 0x8204F800 and 0x8204F810, read out of
  // the image rather than assumed from the name of the function.
  RetailBasis basis;
  basis.rows[0] = {1.0F, 0.0F, 0.0F, 0.0F};
  basis.rows[1] = {0.0F, 1.0F, 0.0F, 0.0F};
  basis.rows[2] = {0.0F, 0.0F, 1.0F, 0.0F};
  return basis;
}

SinCos retail_sin_cos(float angle) noexcept {
  // The seam. See the header: 0x8209CB70 is XMScalarSinCos and is not ported.
  return SinCos{std::sin(angle), std::cos(angle)};
}

void rotate_basis(RetailBasis& basis, int kept, int sign, SinCos pair) noexcept {
  int first = -1;
  int second = -1;
  for (int index = 0; index < 3; ++index) {
    if (index == kept) {
      continue;
    }
    (first < 0 ? first : second) = index;
  }
  // Both source rows are copied before either is written. The retail sites
  // rotate in place, so a lane-by-lane update that still read a source would
  // clobber itself -- the same hazard the vector suite tests for at the
  // instruction level.
  const BasisRow a = basis.rows[first];
  const BasisRow b = basis.rows[second];
  const float sine = static_cast<float>(sign) * pair.sine;
  for (std::size_t lane = 0; lane < 4; ++lane) {
    basis.rows[first][lane] = a[lane] * pair.cosine + sine * b[lane];
    basis.rows[second][lane] = -sine * a[lane] + b[lane] * pair.cosine;
  }
}

void rotate_820A9B30(RetailBasis& basis, float angle) noexcept {
  rotate_basis(basis, 1, -1, retail_sin_cos(angle));
}

void rotate_820A99F8(RetailBasis& basis, float angle) noexcept {
  rotate_basis(basis, 0, +1, retail_sin_cos(angle));
}

void rotate_82211828(RetailBasis& basis, float angle) noexcept {
  rotate_basis(basis, 2, +1, retail_sin_cos(angle));
}

RetailBasis assemble_basis(float angle1, float angle2, float angle3) noexcept {
  RetailBasis basis = identity_basis();
  // The order retail calls them in, which is not the order of the arguments.
  rotate_820A9B30(basis, angle2);
  rotate_820A99F8(basis, angle1);
  rotate_82211828(basis, angle3);
  return basis;
}

}  // namespace ac6::retail
