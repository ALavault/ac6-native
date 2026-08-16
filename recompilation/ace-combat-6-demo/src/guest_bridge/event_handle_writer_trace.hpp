#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace ac6demo::guest_bridge_detail {

// Opt-in evidence only: this hook observes guest stores and never changes them.
inline void trace_event_handle_writer(
    std::uint32_t address, std::uint32_t value, std::uint64_t tick,
    std::uint32_t thread, std::uint32_t lr, const char* generated_name,
    std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_WRITERS") != nullptr;
  static std::uint32_t record_count = 0U;
  if (!enabled || record_count >= 4096U) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_EVENT_HANDLE_WRITE address=0x%08X value=0x%08X tick=%llu "
      "thread=%u lr=0x%08X function=%s generated_line=%u\n",
      address, value, static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

}  // namespace ac6demo::guest_bridge_detail
