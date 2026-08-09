#include "ac6/retail_slot_gather.h"

namespace ac6::retail {

std::uint32_t gather_active_slots(
    std::uint32_t record_flags,
    const std::array<std::uint32_t, kSlotMaskCount>& slot_masks) noexcept {
  // 0x82211B40's first three instructions: a flag word of zero returns before
  // touching the caller's mask at all.
  if (record_flags == 0U) {
    return 0U;
  }
  std::uint32_t active = 0U;
  for (std::size_t slot = 0; slot < kSlotMaskCount; ++slot) {
    if ((slot_masks[slot] & record_flags) != 0U) {
      active |= 1U << slot;
    }
  }
  return active;
}

}  // namespace ac6::retail
