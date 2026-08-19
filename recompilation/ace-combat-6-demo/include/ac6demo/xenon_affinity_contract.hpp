#pragma once

#include <cstdint>
#include <stdexcept>

namespace ac6demo {

inline constexpr std::uint8_t kXenonHardwareThreadCount = 6U;
inline constexpr std::uint32_t kXenonAffinityMask = 0x3FU;

[[nodiscard]] constexpr bool
valid_xenon_affinity_mask(std::uint32_t mask) noexcept {
  return mask != 0U && (mask & ~kXenonAffinityMask) == 0U &&
         (mask & (mask - 1U)) == 0U;
}

[[nodiscard]] constexpr std::uint32_t
xenon_affinity_mask_from_processor(std::uint8_t processor) {
  if (processor >= kXenonHardwareThreadCount) {
    throw std::out_of_range{"Xenon processor is outside 0..5"};
  }
  return std::uint32_t{1U} << processor;
}

[[nodiscard]] constexpr std::uint8_t
xenon_processor_from_affinity_mask(std::uint32_t mask) {
  if (!valid_xenon_affinity_mask(mask)) {
    throw std::invalid_argument{
        "Xenon affinity mask is not one-hot in bits 0..5"};
  }
  std::uint8_t processor = 0U;
  while ((mask >> processor) != 1U) {
    ++processor;
  }
  return processor;
}

struct XenonAffinityTransition final {
  std::uint32_t previous_mask{};
  std::uint32_t requested_mask{};
  std::uint8_t previous_processor{};
  std::uint8_t requested_processor{};
};

// KeSetAffinityThread returns the previous KAFFINITY in r3. It does not take
// an output pointer in r5. The bridge models an unpinned thread as its current
// published processor, which is CPU 0 for a newly created GuestThread.
[[nodiscard]] constexpr XenonAffinityTransition
make_xenon_affinity_transition(std::uint8_t current_processor,
                               std::uint32_t requested_mask) {
  return XenonAffinityTransition{
      xenon_affinity_mask_from_processor(current_processor), requested_mask,
      current_processor, xenon_processor_from_affinity_mask(requested_mask)};
}

} // namespace ac6demo
