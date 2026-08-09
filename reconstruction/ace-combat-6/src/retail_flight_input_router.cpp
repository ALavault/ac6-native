#include "ac6/retail_flight_input_router.h"

namespace ac6::retail {
namespace {

// 0x82001348 and 0x8200082C. Retail selects between these two words with a
// branch, so the result is exactly one of them and never a product.
constexpr float kSet = 1.0F;
constexpr float kClear = 0.0F;

float bit_value(std::uint32_t flags, std::uint32_t bit) noexcept {
  return (flags & (1U << bit)) != 0U ? kSet : kClear;
}

}  // namespace

FlightInputFields route_flight_input_fields(const FlightBindingOutputs& outputs,
                                            std::uint32_t button_flags,
                                            FlightInputDevice device,
                                            std::uint32_t layout) noexcept {
  FlightInputFields fields{};

  // 0x82229460..0x8222946C, the common tail: array[0] and array[1] reach +2104
  // and +2108 on every arm, and nothing else ever writes them here.
  fields.at2104 = outputs.value[0];
  fields.at2108 = outputs.value[1];

  if (device == FlightInputDevice::kDigital) {
    // 0x82229310..0x82229368.
    fields.at2112 = bit_value(button_flags, kDeviceAxisLowBit);
    fields.at2116 = bit_value(button_flags, kDeviceAxisHighBit);
    fields.at2096 = outputs.value[4];
    fields.at2100 = outputs.value[5];
    return fields;
  }

  if (layout == kSwappedLayout) {
    // 0x822293E0..0x82229438. Both pairs transposed, each reversed.
    fields.at2096 = outputs.value[3];
    fields.at2100 = outputs.value[2];
    fields.at2112 = bit_value(button_flags, kAnalogHoldHighBit);
    fields.at2116 = bit_value(button_flags, kAnalogHoldLowBit);
    return fields;
  }

  // 0x8222937C..0x822293D8.
  fields.at2112 = outputs.value[2];
  fields.at2116 = outputs.value[3];
  fields.at2096 = bit_value(button_flags, kAnalogHoldLowBit);
  fields.at2100 = bit_value(button_flags, kAnalogHoldHighBit);
  return fields;
}

}  // namespace ac6::retail
