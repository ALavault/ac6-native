#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace ac6demo::guest_bridge_detail {

inline bool graphics_interrupt_trace_enabled() noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_GRAPHICS_INTERRUPT") != nullptr;
  return enabled;
}

inline bool graphics_interrupt_state_trace_enabled() noexcept {
  return graphics_interrupt_trace_enabled() &&
         std::getenv("AC6_DEMO_WATCH_GRAPHICS_INTERRUPT_STATE") != nullptr;
}

inline void trace_graphics_interrupt_registration(
    std::uint32_t callback, std::uint32_t context, std::uint64_t tick,
    std::uint32_t thread) noexcept {
  if (!graphics_interrupt_trace_enabled()) {
    return;
  }
  std::fprintf(stderr,
               "AC6_GRAPHICS_INTERRUPT_REGISTER callback=0x%08X "
               "context=0x%08X tick=%llu thread=%u\n",
               callback, context, static_cast<unsigned long long>(tick),
               thread);
}

inline void trace_graphics_interrupt_call(
    std::uint32_t callback, std::uint32_t context, std::uint32_t source,
    std::uint64_t tick, std::uint32_t thread) noexcept {
  if (!graphics_interrupt_trace_enabled()) {
    return;
  }
  static std::uint32_t record_count = 0U;
  if (record_count >= 8192U) {
    return;
  }
  ++record_count;
  std::fprintf(stderr,
               "AC6_GRAPHICS_INTERRUPT_CALL callback=0x%08X "
               "context=0x%08X source=%u tick=%llu thread=%u\n",
               callback, context, source,
               static_cast<unsigned long long>(tick), thread);
}

inline void trace_graphics_interrupt_pm4(std::uint8_t cpu,
                                         std::uint64_t tick) noexcept {
  if (!graphics_interrupt_trace_enabled()) {
    return;
  }
  std::fprintf(stderr, "AC6_GRAPHICS_INTERRUPT_PM4 cpu=%u tick=%llu\n",
               static_cast<unsigned>(cpu),
               static_cast<unsigned long long>(tick));
}

inline void trace_graphics_interrupt_gate(std::uint32_t value, bool mapped,
                                          std::uint32_t source,
                                          std::uint64_t tick) noexcept {
  if (!graphics_interrupt_state_trace_enabled() || source != 0U) {
    return;
  }
  std::fprintf(stderr,
               "AC6_GRAPHICS_INTERRUPT_GATE address=0x7FC86544 "
               "mapped=%u value=0x%08X source=%u tick=%llu\n",
               mapped ? 1U : 0U, value, source,
               static_cast<unsigned long long>(tick));
}

inline void trace_graphics_interrupt_state_load(
    std::uint32_t address, std::uint32_t value, std::uint64_t tick,
    std::uint32_t thread, std::uint32_t lr, const char *generated_name,
    std::uint32_t generated_line) noexcept {
  if (!graphics_interrupt_state_trace_enabled() || generated_name == nullptr) {
    return;
  }
  const std::string_view function{generated_name};
  const bool interrupt_queue_state = function.find("821C5190") !=
                                     std::string_view::npos;
  const bool callback_gate = function.find("821B9710") !=
                             std::string_view::npos &&
                             address == 0x7FC86544U;
  if (!interrupt_queue_state && !callback_gate) {
    return;
  }
  static std::uint32_t record_count = 0U;
  if (record_count >= 256U) {
    return;
  }
  ++record_count;
  std::fprintf(stderr,
               "AC6_GRAPHICS_INTERRUPT_STATE_LOAD address=0x%08X "
               "value=0x%08X lr=0x%08X tick=%llu thread=%u "
               "function=%s line=%u\n",
               address, value, lr, static_cast<unsigned long long>(tick),
               thread, generated_name, generated_line);
}

inline bool graphics_interrupt_state_load_guard(
    std::uint32_t address, const char *generated_name) noexcept {
  if (!graphics_interrupt_state_trace_enabled() || generated_name == nullptr) {
    return false;
  }
  const std::string_view function{generated_name};
  if (function.find("821C5190") == std::string_view::npos) {
    return false;
  }
  return address != 0x82000608U && address != 0x000101BEU &&
         (address < 0x10045A84U || address > 0x10045AA0U) &&
         address != 0x10046E18U && address != 0x10044494U;
}

} // namespace ac6demo::guest_bridge_detail
