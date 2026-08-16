#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstddef>

namespace ac6demo::guest_bridge_detail {

inline void trace_frontbuffer_read(std::uint32_t address, std::uint32_t width,
                                   std::uint64_t value, std::uint64_t tick,
                                   std::uint32_t thread, std::uint32_t lr,
                                   const char *generated_name,
                                   std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_FRONTBUFFER_READERS") != nullptr;
  constexpr std::uint32_t kFrontbufferBegin = 0x1374A000U;
  constexpr std::uint32_t kFrontbufferEnd = 0x13AE2000U;
  const auto end = static_cast<std::uint64_t>(address) + width;
  if (!enabled || address >= kFrontbufferEnd || end <= kFrontbufferBegin) {
    return;
  }
  static std::uint32_t record_count = 0U;
  if (record_count >= 8192U) {
    return;
  }
  ++record_count;
  std::fprintf(stderr,
               "AC6_FRONTBUFFER_GUEST_READ address=0x%08X size=%u "
               "value=0x%016llX tick=%llu thread=%u lr=0x%08X "
               "function=%s generated_line=%u\n",
               address, width, static_cast<unsigned long long>(value),
               static_cast<unsigned long long>(tick), thread, lr,
               generated_name == nullptr ? "" : generated_name,
               generated_line);
}

inline void trace_frontbuffer_vector_read(std::uint32_t address,
                                          const std::byte *bytes,
                                          std::size_t width,
                                          std::uint64_t tick,
                                          std::uint32_t thread,
                                          std::uint32_t lr,
                                          const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_FRONTBUFFER_READERS") != nullptr;
  constexpr std::uint32_t kFrontbufferBegin = 0x1374A000U;
  constexpr std::uint32_t kFrontbufferEnd = 0x13AE2000U;
  const auto end = static_cast<std::uint64_t>(address) + width;
  if (!enabled || bytes == nullptr || width == 0U || width > 16U ||
      address >= kFrontbufferEnd || end <= kFrontbufferBegin) {
    return;
  }
  static std::uint32_t record_count = 0U;
  if (record_count >= 8192U) {
    return;
  }
  ++record_count;
  std::fprintf(stderr,
               "AC6_FRONTBUFFER_VECTOR_READ address=0x%08X size=%zu "
               "tick=%llu thread=%u lr=0x%08X function=%s bytes=",
               address, width, static_cast<unsigned long long>(tick), thread,
               lr, generated_name == nullptr ? "" : generated_name);
  for (std::size_t index = 0U; index < width; ++index) {
    std::fprintf(stderr, "%02X", std::to_integer<unsigned int>(bytes[index]));
  }
  std::fputc('\n', stderr);
}

inline void trace_frontbuffer_write(std::uint32_t address,
                                    std::uint32_t width,
                                    std::uint64_t tick,
                                    std::uint32_t thread,
                                    std::uint32_t lr,
                                    const char* generated_name,
                                    std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_FRONTBUFFER_WRITERS") != nullptr;
  constexpr std::uint32_t kFrontbufferBegin = 0x1374A000U;
  constexpr std::uint32_t kFrontbufferEnd = 0x13AE2000U;
  const auto end = static_cast<std::uint64_t>(address) + width;
  if (!enabled || address >= kFrontbufferEnd || end <= kFrontbufferBegin) {
    return;
  }
  std::fprintf(stderr,
               "AC6_FRONTBUFFER_WRITE address=0x%08X size=%u tick=%llu "
               "thread=%u lr=0x%08X function=%s generated_line=%u\n",
               address, width, static_cast<unsigned long long>(tick), thread,
               lr, generated_name == nullptr ? "" : generated_name,
               generated_line);
}

} // namespace ac6demo::guest_bridge_detail
