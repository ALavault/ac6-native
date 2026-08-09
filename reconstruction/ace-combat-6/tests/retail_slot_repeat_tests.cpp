// The slot edge and auto-repeat bank, at the boundaries.

#include "ac6/retail_slot_repeat.h"
#include "ac6/retail_input.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

std::uint32_t bits_of(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

ac6::retail::AutoRepeatConfig config() { return {0.5F, 0.125F}; }

void test_inactive_resets_both() {
  ac6::retail::AutoRepeatSlot slot{9.0F, 0.125F};
  check(!ac6::retail::tick_auto_repeat(slot, false, 0.016F, config()),
        "an inactive slot does not fire");
  check(bits_of(slot.timer) == bits_of(0.0F), "the timer is cleared");
  check(bits_of(slot.limit) == bits_of(0.5F),
        "the limit goes back to the INITIAL delay, not the repeat interval");
}

void test_first_press_waits_the_initial_delay() {
  // A slot released then pressed: the reset above left limit = 0.5.
  ac6::retail::AutoRepeatSlot slot{0.0F, 0.5F};
  const float dt = 0.1F;
  int fires = 0;
  for (int frame = 0; frame < 6; ++frame) {
    if (ac6::retail::tick_auto_repeat(slot, true, dt, config())) {
      ++fires;
    }
  }
  // timer reaches 0.5 only after five adds, so exactly one fire in six frames.
  check(fires == 1, "one fire in six frames at 0.1s with a 0.5s delay");
  check(bits_of(slot.limit) == bits_of(0.125F),
        "and the limit switched to the repeat interval");
}

void test_equality_fires() {
  // fcmpu then blt: the slot fires when the timer is NOT LESS than the limit,
  // so equality fires. A port written with > would hold one frame longer.
  ac6::retail::AutoRepeatSlot slot{0.125F, 0.125F};
  check(ac6::retail::tick_auto_repeat(slot, true, 0.0F, config()),
        "timer exactly equal to the limit fires");
  ac6::retail::AutoRepeatSlot under{0.124F, 0.125F};
  check(!ac6::retail::tick_auto_repeat(under, true, 0.0F, config()),
        "and just under does not");
}

void test_reset_then_add_order() {
  // The add happens AFTER the reset, so a slot that has just fired holds the
  // elapsed time, not zero. A port that added first and reset after would hold
  // zero and take one extra frame to repeat.
  ac6::retail::AutoRepeatSlot slot{1.0F, 0.125F};
  check(ac6::retail::tick_auto_repeat(slot, true, 0.016F, config()),
        "it fires");
  check(bits_of(slot.timer) == bits_of(0.016F),
        "and the timer holds the elapsed time, not zero");
}

void test_edges_match_the_button_algebra() {
  std::array<ac6::retail::AutoRepeatSlot, ac6::retail::kSlotCount> slots{};
  const std::uint32_t previous = 0b0110U;
  const std::uint32_t active = 0b1100U;
  const auto edges =
      ac6::retail::tick_slot_repeat(active, previous, slots, 0.0F, config());
  check(edges.newly_active == (active & ~previous), "newly active is cur & ~prev");
  check(edges.newly_inactive == (previous & ~active), "newly inactive is prev & ~cur");

  // The product already computes these two sets the other way round, on device
  // buttons. They must agree, or one of the two derivations is wrong.
  const auto buttons = ac6::retail::button_edges(previous,
                                                 static_cast<std::uint16_t>(active));
  check(buttons.pressed == edges.newly_active,
        "button_edges' pressed set equals cur & ~prev");
  check(buttons.released == edges.newly_inactive,
        "button_edges' released set equals prev & ~cur");
}

void test_triggered_carries_repeats() {
  std::array<ac6::retail::AutoRepeatSlot, ac6::retail::kSlotCount> slots{};
  // Slot 0 held from a previous frame, its timer already past the limit.
  slots[0] = {1.0F, 0.125F};
  const auto edges = ac6::retail::tick_slot_repeat(1U, 1U, slots, 0.016F, config());
  check(edges.newly_active == 0U, "a held slot is not newly active");
  check((edges.triggered & 1U) != 0U,
        "but it IS triggered, because the repeat fired");
}

}  // namespace

int main() {
  test_inactive_resets_both();
  test_first_press_waits_the_initial_delay();
  test_equality_fires();
  test_reset_then_add_order();
  test_edges_match_the_button_algebra();
  test_triggered_carries_repeats();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "retail_slot_repeat=pass\n";
  return 0;
}
