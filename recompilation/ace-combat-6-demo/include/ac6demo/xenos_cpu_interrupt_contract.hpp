#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace ac6demo {

inline constexpr std::uint8_t kXenosCommandProcessorInterruptSource = 1U;
inline constexpr std::uint32_t kXenosHardwareThreadMask = 0x3FU;

struct XenosCpuInterruptRequest final {
  std::uint8_t source{kXenosCommandProcessorInterruptSource};
  std::uint8_t cpu{};
  std::uint32_t cpu_mask{};
  std::uint32_t scratch_callback{};
  std::uint32_t scratch_parameter{};
};

struct XenosCpuInterruptBatch final {
  std::array<XenosCpuInterruptRequest, 6U> requests{};
  std::size_t count{};
};

[[nodiscard]] inline XenosCpuInterruptBatch decode_xenos_cpu_interrupt(
    std::uint32_t cpu_mask, std::uint32_t scratch_callback,
    std::uint32_t scratch_parameter) {
  if (cpu_mask == 0U || (cpu_mask & ~kXenosHardwareThreadMask) != 0U) {
    throw std::invalid_argument{"invalid Xenos PM4 interrupt CPU mask"};
  }

  XenosCpuInterruptBatch result;
  for (std::uint8_t cpu = 0U; cpu < 6U; ++cpu) {
    const auto bit = std::uint32_t{1U} << cpu;
    if ((cpu_mask & bit) == 0U) {
      continue;
    }
    result.requests[result.count++] = XenosCpuInterruptRequest{
        kXenosCommandProcessorInterruptSource, cpu, cpu_mask,
        scratch_callback, scratch_parameter};
  }
  return result;
}

} // namespace ac6demo
