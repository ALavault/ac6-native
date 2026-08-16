#if defined(AC6_DEMO_GENERATED_GUEST) || defined(AC6_DEMO_ENABLE_VECTOR_READ_TRACE)

#include "ppc_config.h"
#include "ppc_context_base.h"
#include "ac6demo/guest_bridge.hpp"
#include "event_post_set_trace.hpp"
#ifdef AC6_DEMO_ENABLE_VECTOR_READ_TRACE
#include "frontbuffer_writer_trace.hpp"
#endif

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace {
thread_local PPCContext *current_context = nullptr;
thread_local const char *current_function = nullptr;
thread_local std::uint64_t current_tick = 0U;
thread_local std::uint32_t current_thread = 0U;
#ifdef AC6_DEMO_GENERATED_GUEST
thread_local ac6demo::GuestBridge *current_bridge = nullptr;
#endif
}  // namespace

#ifdef AC6_DEMO_GENERATED_GUEST
extern "C" {
std::atomic_bool AC6_PPC_POST_RESUME_VECTOR_FAST_ENABLED{false};
}

extern "C" void AC6_PPC_SET_POST_RESUME_VECTOR_CONTEXT(
    PPCContext &context, ac6demo::GuestBridge *bridge,
    const char *function, std::uint64_t tick,
    std::uint32_t thread) noexcept {
  current_context = &context;
  current_bridge = bridge;
  current_function = function;
  current_tick = tick;
  current_thread = thread;
  ac6demo::guest_bridge_detail::initialize_post_resume_watch();
  AC6_PPC_POST_RESUME_VECTOR_FAST_ENABLED.store(
      ac6demo::guest_bridge_detail::post_resume_watch_enabled_fast(),
      std::memory_order_release);
}

extern "C" void AC6_PPC_RECORD_POST_RESUME_VECTOR_READ(
    const void *source, const char *generated_name,
    std::uint32_t generated_line) noexcept {
  if (!AC6_PPC_POST_RESUME_VECTOR_FAST_ENABLED.load(
          std::memory_order_relaxed) || source == nullptr ||
      current_bridge == nullptr || current_context == nullptr) {
    return;
  }
  const auto raw = reinterpret_cast<std::uintptr_t>(
      current_bridge->memory().raw_base());
  const auto pointer = reinterpret_cast<std::uintptr_t>(source);
  if (raw == 0U || pointer < raw) {
    return;
  }
  const auto offset = pointer - raw;
  if (offset >= 0x1'0000'0000ULL) {
    return;
  }
  const auto address = static_cast<std::uint32_t>(offset);
  if (!current_bridge->memory().mapped(address, 16U)) {
    return;
  }
  std::array<std::uint8_t, 16U> bytes{};
  std::memcpy(bytes.data(), source, bytes.size());
  ac6demo::guest_bridge_detail::record_post_resume_bytes(
      "load128", address, 16U, bytes.data(), current_tick, current_thread,
      static_cast<std::uint32_t>(current_context->lr), generated_name,
      generated_line);
}
#endif

#ifdef AC6_DEMO_ENABLE_VECTOR_READ_TRACE
extern "C" std::uintptr_t AC6_PPC_GUEST_RAW_BASE = 0U;

extern "C" void AC6_PPC_VECTOR_CONTEXT(
    PPCContext &context, const char *function, std::uint64_t tick,
    std::uint32_t thread, std::uintptr_t raw_base) noexcept {
  current_context = &context;
  current_function = function;
  current_tick = tick;
  current_thread = thread;
  AC6_PPC_GUEST_RAW_BASE = raw_base;
}

extern "C" void AC6_PPC_RECORD_VECTOR_READ(
    std::uint32_t address, const void *source) noexcept {
  if (source == nullptr) {
    return;
  }
  std::array<std::byte, 16U> bytes{};
  std::memcpy(bytes.data(), source, bytes.size());
  const auto lr = current_context == nullptr
                      ? 0U
                      : static_cast<std::uint32_t>(current_context->lr);
  ac6demo::guest_bridge_detail::trace_frontbuffer_vector_read(
      address, bytes.data(), bytes.size(), current_tick, current_thread, lr,
      current_function);
}
#endif

#endif
