#include "ac6demo/xenos_cpu_interrupt_contract.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <stdexcept>

namespace {

void test_one_hot_masks() {
  for (std::uint8_t cpu = 0U; cpu < 6U; ++cpu) {
    const auto mask = std::uint32_t{1U} << cpu;
    const auto batch = ac6demo::decode_xenos_cpu_interrupt(
        mask, 0x821C4A60U, 0x100446D4U);
    assert(batch.count == 1U);
    assert(batch.requests[0].source == 1U);
    assert(batch.requests[0].cpu == cpu);
    assert(batch.requests[0].cpu_mask == mask);
    assert(batch.requests[0].scratch_callback == 0x821C4A60U);
    assert(batch.requests[0].scratch_parameter == 0x100446D4U);
  }
}

void test_multibit_mask_preserves_order_and_payload() {
  const auto batch = ac6demo::decode_xenos_cpu_interrupt(
      0x25U, 0x821C5190U, 0x10041A00U);
  assert(batch.count == 3U);
  constexpr std::array<std::uint8_t, 3U> expected{0U, 2U, 5U};
  for (std::size_t index = 0U; index < expected.size(); ++index) {
    assert(batch.requests[index].cpu == expected[index]);
    assert(batch.requests[index].cpu_mask == 0x25U);
    assert(batch.requests[index].scratch_callback == 0x821C5190U);
    assert(batch.requests[index].scratch_parameter == 0x10041A00U);
  }
}

void test_invalid_masks_fail_closed() {
  for (const std::uint32_t mask : {0U, 0x40U, 0x84U}) {
    bool rejected = false;
    try {
      static_cast<void>(
          ac6demo::decode_xenos_cpu_interrupt(mask, 0U, 0U));
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    assert(rejected);
  }
}

} // namespace

int main() {
  test_one_hot_masks();
  test_multibit_mask_preserves_order_and_payload();
  test_invalid_masks_fail_closed();
  return 0;
}
