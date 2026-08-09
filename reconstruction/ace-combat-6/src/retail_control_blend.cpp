// 0x82281E10 and 0x82281EA8, instruction by instruction. They differ only in
// which fields they touch and which constant they scale by.

#include "ac6/retail_control_blend.h"

#include <cmath>

namespace ac6::retail {

ControlBlend blend_control_axis(float axis, float rate, float stored,
                                std::uint32_t flags332, float scale) noexcept {
  ControlBlend out{};
  out.stored = stored;
  float value = axis;

  if ((flags332 & kBlendResetBit) != 0U) {
    const float scaled = rate * scale;
    // `ble` skips the move, so equal magnitudes keep the axis.
    if (std::fabs(scaled) > std::fabs(value)) {
      value = scaled;
    }
    out.stored = value;
    out.wrote = true;
  }
  else if (!(std::fabs(stored) < kBlendEpsilon)) {
    value = stored + value;
  }

  // Two early returns, in this order. A NaN takes neither and comes back out.
  if (value < -1.0F) {
    out.value = -1.0F;
  }
  else if (value > 1.0F) {
    out.value = 1.0F;
  }
  else {
    out.value = value;
  }
  return out;
}

}  // namespace ac6::retail
