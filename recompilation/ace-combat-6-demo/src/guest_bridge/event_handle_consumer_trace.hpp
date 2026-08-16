#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace ac6demo::guest_bridge_detail {

// Opt-in evidence only: this hook observes guest loads and never changes them.
inline void trace_event_handle_consumer(
    std::uint32_t address, std::uint32_t value, std::uint64_t tick,
    std::uint32_t thread, std::uint32_t lr, const char* generated_name,
    std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_CONSUMERS") != nullptr;
  static std::uint32_t record_count = 0U;
  if (!enabled || record_count >= 8192U) {
    return;
  }
  ++record_count;
  std::fprintf(stderr,
               "AC6_EVENT_HANDLE_READ address=0x%08X value=0x%08X "
               "tick=%llu thread=%u lr=0x%08X function=%s generated_line=%u\n",
               address, value, static_cast<unsigned long long>(tick), thread,
               lr, generated_name == nullptr ? "" : generated_name,
               generated_line);
}

inline void trace_event_handle_payload(
    std::uint32_t address, std::uint32_t value, std::uint64_t tick,
    std::uint32_t thread, std::uint32_t lr, const char* generated_name,
    std::uint32_t generated_line, std::uint32_t snapshot_base,
    const std::uint32_t* words, std::uint32_t word_mask) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_PAYLOAD") != nullptr;
  static std::uint32_t record_count = 0U;
  if (!enabled || record_count >= 8192U) {
    return;
  }
  ++record_count;
  std::fprintf(stderr,
               "AC6_EVENT_HANDLE_PAYLOAD address=0x%08X value=0x%08X "
               "tick=%llu thread=%u lr=0x%08X function=%s generated_line=%u "
               "base=0x%08X mask=0x%02X words",
               address, value, static_cast<unsigned long long>(tick), thread,
               lr, generated_name == nullptr ? "" : generated_name,
               generated_line, snapshot_base, word_mask);
  for (std::uint32_t index = 0U; index < 8U; ++index) {
    std::fprintf(stderr, " %08X", words[index]);
  }
  std::fputc('\n', stderr);
}

}  // namespace ac6demo::guest_bridge_detail
