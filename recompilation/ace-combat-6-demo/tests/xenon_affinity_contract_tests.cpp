#ifdef NDEBUG
#error "This assert-based test must be built with -UNDEBUG."
#endif

#include "ac6demo/xenon_affinity_contract.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <stdexcept>

int main() {
  constexpr std::array<std::uint32_t, 6U> masks{1U, 2U, 4U, 8U, 16U, 32U};
  for (std::uint8_t processor = 0U; processor < masks.size(); ++processor) {
    assert(ac6demo::valid_xenon_affinity_mask(masks[processor]));
    assert(ac6demo::xenon_affinity_mask_from_processor(processor) ==
           masks[processor]);
    assert(ac6demo::xenon_processor_from_affinity_mask(masks[processor]) ==
           processor);
  }

  assert(!ac6demo::valid_xenon_affinity_mask(0U));
  assert(!ac6demo::valid_xenon_affinity_mask(3U));
  assert(!ac6demo::valid_xenon_affinity_mask(0x40U));
  assert(!ac6demo::valid_xenon_affinity_mask(0x30U));

  const auto first = ac6demo::make_xenon_affinity_transition(0U, 0x10U);
  assert(first.previous_mask == 0x01U);
  assert(first.requested_mask == 0x10U);
  assert(first.previous_processor == 0U);
  assert(first.requested_processor == 4U);

  const auto repin = ac6demo::make_xenon_affinity_transition(4U, 0x20U);
  assert(repin.previous_mask == 0x10U);
  assert(repin.requested_processor == 5U);

  bool bad_processor = false;
  try {
    (void)ac6demo::make_xenon_affinity_transition(6U, 1U);
  } catch (const std::out_of_range &) {
    bad_processor = true;
  }
  assert(bad_processor);

  bool bad_mask = false;
  try {
    (void)ac6demo::make_xenon_affinity_transition(0U, 0x30U);
  } catch (const std::invalid_argument &) {
    bad_mask = true;
  }
  assert(bad_mask);
  return 0;
}
