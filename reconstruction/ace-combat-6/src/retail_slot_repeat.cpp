#include "ac6/retail_slot_repeat.h"

namespace ac6::retail {

bool tick_auto_repeat(AutoRepeatSlot& slot, bool active, float elapsed,
                      const AutoRepeatConfig& config) noexcept {
  if (!active) {
    // 0x82211A08: the timer is cleared and the limit goes back to the initial
    // delay, so a slot released and pressed again waits the full delay.
    slot.timer = 0.0F;
    slot.limit = config.initial_delay;
    return false;
  }
  // 0x822119DC: fcmpu then blt, so the slot fires when the timer is NOT less
  // than the limit -- equality fires.
  const bool fired = !(slot.timer < slot.limit);
  if (fired) {
    slot.limit = config.repeat_interval;
    slot.timer = 0.0F;
  }
  // 0x822119F8: the add happens on both paths, after any reset.
  slot.timer = slot.timer + elapsed;
  return fired;
}

SlotEdges tick_slot_repeat(std::uint32_t active, std::uint32_t previous,
                           std::array<AutoRepeatSlot, kSlotCount>& slots,
                           float elapsed,
                           const AutoRepeatConfig& config) noexcept {
  SlotEdges edges;
  // 0x82211988's first six instructions, andc both ways.
  edges.newly_active = active & ~previous;
  edges.newly_inactive = previous & ~active;
  edges.triggered = active & ~previous;
  for (std::size_t slot = 0; slot < kSlotCount; ++slot) {
    const bool is_active = ((active >> slot) & 1U) != 0U;
    if (tick_auto_repeat(slots[slot], is_active, elapsed, config)) {
      edges.triggered |= 1U << slot;
    }
  }
  return edges;
}

}  // namespace ac6::retail
