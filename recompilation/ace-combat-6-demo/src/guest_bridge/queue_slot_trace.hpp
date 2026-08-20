#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace ac6demo::guest_bridge_detail {

inline void trace_render_queue_slot_store(
    std::uint32_t address, std::uint32_t size, std::uint64_t value,
    std::uint64_t tick, std::uint32_t thread, std::uint32_t lr,
    const char* generated_name, std::uint32_t generated_line) {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_RENDER_QUEUE_SLOTS") != nullptr;
  constexpr std::uint32_t kSlotBegin = 0x82386D90U;
  constexpr std::uint32_t kSlotEnd = 0x8238CDD0U;
  if (!enabled || address < kSlotBegin || size > kSlotEnd - address ||
      address + size > kSlotEnd) {
    return;
  }
  std::fprintf(stderr,
               "AC6_RENDER_QUEUE_SLOT_STORE address=0x%08X size=%u "
               "value=0x%016llX nonzero=%u tick=%llu thread=%u lr=0x%08X function=%s "
               "generated_line=%u\n",
               address, size, static_cast<unsigned long long>(value), value != 0U ? 1U : 0U,
               static_cast<unsigned long long>(tick), thread, lr,
               generated_name == nullptr ? "" : generated_name,
               generated_line);
}

}  // namespace ac6demo::guest_bridge_detail
