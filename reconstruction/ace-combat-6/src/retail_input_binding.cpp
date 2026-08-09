#include "ac6/retail_input_binding.h"

#include <cmath>

namespace ac6::retail {

BindingOutputs apply_input_binding(float value,
                                   const InputBinding& binding) noexcept {
  BindingOutputs outputs;

  // 0x82211D18..0x82211D40. The magnitude is taken first and the sign restored
  // last, which is why an input of -0.0 comes out as +0.0 rather than -0.0.
  float scaled = std::fabs(value) - binding.deadzone;
  if (scaled < 0.0F) {
    scaled = 0.0F;
  } else {
    scaled = scaled * binding.scale;
    if (scaled > 1.0F) {
      scaled = 1.0F;
    }
  }
  outputs.value = select_ge_zero(value, scaled, -scaled);

  // 0x82211D7C..0x82211DA0. Three regions, and the middle one passes the input
  // through untouched -- see the header.
  float step = value;
  const float magnitude = std::fabs(value);
  if (magnitude < binding.deadzone) {
    step = 0.0F;
  }
  if (magnitude > binding.threshold) {
    step = select_ge_zero(step, 1.0F, -1.0F);
  }
  outputs.step = step;
  return outputs;
}

BindingOutputs invert_outputs(BindingOutputs outputs) noexcept {
  // 0x82211DB4/0x82211DB8: two fneg, so -0.0 becomes +0.0 and back. Negating a
  // zero is not a no-op here and the sign is carried into the output arrays.
  outputs.value = -outputs.value;
  outputs.step = -outputs.step;
  return outputs;
}

}  // namespace ac6::retail
