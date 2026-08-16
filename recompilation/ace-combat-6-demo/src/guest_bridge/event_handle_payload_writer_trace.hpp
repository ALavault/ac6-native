#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace ac6demo::guest_bridge_detail {

inline bool event_handle_payload_range_intersects(std::uint32_t address,
                                                  std::uint32_t size) noexcept {
  const auto end = static_cast<std::uint64_t>(address) + size;
  return (address < 0x82934760U && end > 0x82934740U) ||
         (address < 0x82933FA0U && end > 0x82933F80U);
}

inline void trace_event_handle_payload_writer(
    std::uint32_t address, std::uint32_t size, std::uint64_t value,
    std::uint64_t tick, std::uint32_t thread, std::uint32_t lr,
    const char* generated_name, std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_PAYLOAD_WRITERS") != nullptr;
  static std::uint32_t record_count = 0U;
  if (!enabled || record_count >= 16384U ||
      !event_handle_payload_range_intersects(address, size)) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_EVENT_HANDLE_PAYLOAD_WRITE address=0x%08X size=%u "
      "value=0x%016llX tick=%llu thread=%u lr=0x%08X function=%s "
      "generated_line=%u\n",
      address, size, static_cast<unsigned long long>(value),
      static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

inline void trace_event_handle_payload_writer_bytes(
    std::uint32_t address, std::uint32_t size, const std::uint8_t* bytes,
    std::uint64_t tick, std::uint32_t thread, std::uint32_t lr,
    const char* generated_name, std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_PAYLOAD_WRITERS") != nullptr;
  static std::uint32_t record_count = 0U;
  if (!enabled || record_count >= 16384U || bytes == nullptr ||
      !event_handle_payload_range_intersects(address, size)) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_EVENT_HANDLE_PAYLOAD_WRITE address=0x%08X size=%u bytes=",
      address, size);
  for (std::uint32_t index = 0U; index < size && index < 16U; ++index) {
    std::fprintf(stderr, "%02X", bytes[index]);
  }
  std::fprintf(stderr,
               " tick=%llu thread=%u lr=0x%08X function=%s "
               "generated_line=%u\n",
               static_cast<unsigned long long>(tick), thread, lr,
               generated_name == nullptr ? "" : generated_name,
               generated_line);
}

}  // namespace ac6demo::guest_bridge_detail
