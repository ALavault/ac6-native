#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace ac6demo::guest_bridge_detail {

// Read-only scheduler evidence.  This hook never changes an event or waiter;
// it is opt-in and bounded so normal replay remains byte-identical.
inline void trace_event_handoff(const char* operation,
                                std::uint32_t signal_handle,
                                std::uint32_t wait_handle,
                                std::uint8_t wait_kind,
                                std::uint32_t thread,
                                std::uint64_t tick,
                                std::uint32_t lr,
                                std::uint32_t state,
                                std::uint32_t waiter,
                                std::uint32_t result) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_EVENT_HANDOFF") != nullptr;
  static const bool focused =
      std::getenv("AC6_DEMO_WATCH_EVENT_HANDOFF_FOCUSED") != nullptr;
  static std::uint32_t record_count = 0U;
  const std::string_view op = operation == nullptr ? "" : operation;
  const bool focused_operation =
      op.starts_with("signal_wait") || op.starts_with("set_") ||
      op.starts_with("pulse_") || op == "clear" || op == "event_wake" ||
      op.starts_with("wait_single_block") ||
      op.starts_with("wait_single_resume") ||
      op.starts_with("wait_single_timeout") || op == "resume_thread";
  if (!enabled || (focused && !focused_operation) ||
      record_count >= (focused ? 32768U : 4096U)) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_EVENT_HANDOFF op=%s signal=0x%08X wait=0x%08X kind=%u "
      "thread=%u tick=%llu lr=0x%08X state=0x%08X waiter=%u result=0x%08X\n",
      operation == nullptr ? "" : operation, signal_handle, wait_handle,
      static_cast<unsigned int>(wait_kind), thread,
      static_cast<unsigned long long>(tick), lr, state, waiter, result);
}

}  // namespace ac6demo::guest_bridge_detail
