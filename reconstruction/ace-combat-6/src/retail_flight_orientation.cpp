// The three rotation angles of 0x82302C88, instruction by instruction.
//
// Every multiply below is a plain `fmuls` in retail -- there is no fused
// multiply-add anywhere in this arithmetic -- so the order of the products is
// the whole of the fidelity, and it is preserved literally rather than
// simplified into one expression.

#include "ac6/retail_flight_orientation.h"

#include <algorithm>
#include <cmath>

namespace ac6::retail {

FlightRotationAngles flight_rotation_angles(const FlightRotationLimits& limits,
                                            const FlightRotationAxes& axes,
                                            float step,
                                            float row0_divisor) noexcept {
  FlightRotationAngles angles{};

  // Row 1, at 0x82302CB4..0x82302CF0. Applied FIRST by slot 32.
  //     f11 = limit * step ; f0 = f11 * (1/15) ; f0 = f0 * axis
  {
    float value = limits.at1252 * step;
    value = value * kRow1Scale;
    value = value * axes.at308;
    // `bgt` jumps to the store with f13 still +limit; falling through negates
    // it first, so this is a symmetric clamp written as one shared store.
    if (value > limits.at1252) {
      value = limits.at1252;
    }
    else if (!(value >= -limits.at1252)) {
      value = -limits.at1252;
    }
    angles.about_row1 = value * kDegreesToRadians;
  }

  // Row 0, at 0x82302BC4..0x82302BFC, inside the helper 0x82302B78.
  //     f13 = 1.0 / divisor ; f13 = f13 * limit ; f13 = f13 * step ;
  //     f13 = f13 * axis
  {
    float value = 1.0F / row0_divisor;
    value = value * limits.at1248;
    value = value * step;
    value = value * axes.at304;
    if (value > limits.at1248) {     // `ble` skips the store: upper bound only
      value = limits.at1248;
    }
    angles.about_row0 = value * kDegreesToRadians;
  }

  // Row 2, at 0x82302D08..0x82302D34. Applied THIRD.
  //     f11 = limit * step ; f13 = f11 * (2/3) ; f13 = f13 * axis
  {
    float value = limits.at1256 * step;
    value = value * kRow2Scale;
    value = value * axes.at312;
    if (value > limits.at1256) {     // upper bound only, again
      value = limits.at1256;
    }
    angles.about_row2 = value * kDegreesToRadians;
  }
  return angles;
}

FlightBasisAttitude flight_basis_attitude(const RetailBasis& basis) noexcept {
  // 0x820936E8 returns zero when both arguments are smaller than 2^-16. That
  // guard is observable on a level aircraft and prevents atan2(near-zero,
  // near-zero) from manufacturing a 45-degree roll.
  constexpr float kAtanGuard = 1.0F / 65536.0F;
  const auto guarded_atan2 = [](const float y, const float x) noexcept {
    return std::fabs(y) < kAtanGuard && std::fabs(x) < kAtanGuard
               ? 0.0F
               : std::atan2(y, x);
  };
  return {
      -std::asin(std::clamp(basis.rows[2][1], -1.0F, 1.0F)),
      guarded_atan2(basis.rows[2][0], basis.rows[2][2]),
      guarded_atan2(basis.rows[0][1], basis.rows[1][1]),
  };
}

}  // namespace ac6::retail
