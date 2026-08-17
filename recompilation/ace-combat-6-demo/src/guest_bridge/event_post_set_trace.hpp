#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "xam_return_chain_trace.hpp"

namespace ac6demo::guest_bridge_detail {

// This probe is deliberately separate from the other AC6 diagnostics.  A
// process can therefore run with every existing watcher enabled without
// changing the post-resume contract: only the exact value "1" opts it in.

struct PostResumeHandoff final {
  std::uint32_t resume_pc{};
  std::uint32_t callsite{};
  std::uint32_t thread{};
  std::uint32_t signal_handle{};
  std::uint32_t wait_handle{};
  std::uint64_t tick{};
};

// 0 = idle, 1 = one thread is publishing the handoff, 2 = armed, 3 = the
// single access (or an explicit unsupported route) has consumed the probe.
// The phase is global so two host threads cannot each produce a "one-shot"
// record; the handoff payload is published before phase 2 with release order.
struct PostResumeProbeState final {
  std::atomic<std::uint8_t> phase{0U};
  // 0 = not initialized, 1 = disabled, 2 = enabled, 3 = initializer.
  std::atomic<std::uint8_t> enabled{0U};
  std::atomic<std::uint64_t> capture_attempts{0U};
  PostResumeHandoff handoff{};
};

inline PostResumeProbeState post_resume_probe_state{};

inline void initialize_post_resume_watch() noexcept {
  auto state = post_resume_probe_state.enabled.load(std::memory_order_acquire);
  if (state == 1U || state == 2U) {
    return;
  }
  std::uint8_t expected = 0U;
  if (post_resume_probe_state.enabled.compare_exchange_strong(
          expected, 3U, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    const char *value = std::getenv("AC6_DEMO_WATCH_POST_RESUME_ACCESS");
    post_resume_probe_state.enabled.store(
        value != nullptr && std::string_view{value} == "1" ? 2U : 1U,
        std::memory_order_release);
    return;
  }
  while ((state = post_resume_probe_state.enabled.load(
              std::memory_order_acquire)) == 3U) {
  }
}

[[nodiscard]] inline bool post_resume_watch_enabled_fast() noexcept {
  return post_resume_probe_state.enabled.load(std::memory_order_acquire) == 2U;
}

// Kept for non-hot-path callers and source-level diagnostics.  The only
// getenv call is in initialize_post_resume_watch(), once per process state.
[[nodiscard]] inline bool post_resume_watch_enabled() noexcept {
  initialize_post_resume_watch();
  return post_resume_watch_enabled_fast();
}

// Existing generated scalar-site watchers also use process-level opt-ins.
// Keep existing opt-ins cached, but read independent XAM state outside cache.
[[nodiscard]] inline bool guest_load_site_watchers_enabled() noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_CONSUMERS") != nullptr ||
      std::getenv("AC6_DEMO_WATCH_FRONTBUFFER_READERS") != nullptr ||
      std::getenv("AC6_DEMO_WATCH_XMA_SLOT") != nullptr;
  return enabled || xam_return_chain_watch_enabled_fast();
}

inline void trace_ib_write(std::uint32_t address, std::uint32_t width,
                           std::uint64_t tick, std::uint32_t thread,
                           std::uint32_t lr, const char *generated_name,
                           std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_IB_WRITERS") != nullptr;
  const auto end = static_cast<std::uint64_t>(address) + width;
  const bool main_ib = address < 0x1274CF54U && end > 0x1274A000U;
  const bool ring_publication = address < 0x126CA064U && end > 0x126CA058U;
  if (!enabled || (!main_ib && !ring_publication)) {
    return;
  }
  std::fprintf(
      stderr,
      "AC6_IB_WRITE region=%s address=0x%08X size=%u tick=%llu thread=%u "
      "lr=0x%08X function=%s generated_line=%u\n",
      ring_publication ? "ring_publication" : "main_ib", address, width,
      static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

// The callsite is the instruction that performed the import call.  The
// resumed guest PC is the return address in LR; keep both fields explicit and
// never label LR as a PC in an access record.
inline void arm_post_resume_access(std::uint32_t wait_handle,
                                    std::uint32_t signal_handle,
                                    std::uint32_t thread,
                                    std::uint32_t resume_pc,
                                    std::uint64_t tick) noexcept {
  initialize_post_resume_watch();
  if (!post_resume_watch_enabled_fast() || wait_handle != 0xE000004CU ||
      signal_handle != 0xE0000048U || thread != 1U ||
      resume_pc != 0x821A69CCU || resume_pc < 4U) {
    return;
  }
  std::uint8_t expected = 0U;
  if (!post_resume_probe_state.phase.compare_exchange_strong(
          expected, 1U, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return;
  }
  post_resume_probe_state.handoff = PostResumeHandoff{
      resume_pc, resume_pc - 4U, thread, signal_handle, wait_handle, tick};
  post_resume_probe_state.phase.store(2U, std::memory_order_release);
  std::fprintf(
      stderr,
      "AC6_POST_RESUME_INSTRUCTION_HANDOFF resume_pc=0x%08X "
      "callsite=0x%08X tick=%llu thread=%u signal_handle=0x%08X "
      "wait_handle=0x%08X\n",
      resume_pc, resume_pc - 4U, static_cast<unsigned long long>(tick),
      thread, signal_handle, wait_handle);
}

[[nodiscard]] inline bool claim_post_resume_access(
    std::uint32_t thread) noexcept {
  if (!post_resume_watch_enabled_fast() || thread != 1U) {
    return false;
  }
  post_resume_probe_state.capture_attempts.fetch_add(
      1U, std::memory_order_relaxed);
  if (post_resume_probe_state.phase.load(std::memory_order_acquire) != 2U) {
    return false;
  }
  std::uint8_t expected = 2U;
  return post_resume_probe_state.phase.compare_exchange_strong(
      expected, 3U, std::memory_order_acq_rel, std::memory_order_acquire);
}

inline void record_post_resume_scalar(const char *kind,
                                      std::uint32_t address,
                                      std::uint32_t size,
                                      std::uint64_t value,
                                      std::uint64_t tick,
                                      std::uint32_t thread,
                                      std::uint32_t lr,
                                      const char *generated_name,
                                      std::uint32_t generated_line) noexcept {
  record_xam_return_chain(kind, address, size, value, tick, thread, lr,
                          generated_name, generated_line);
  if (kind == nullptr || size == 0U || size > 8U ||
      !claim_post_resume_access(thread)) {
    return;
  }
  const auto mask = size == 8U
                        ? ~std::uint64_t{0U}
                        : (std::uint64_t{1U} << (size * 8U)) - 1U;
  // Numeric PPC scalar values are printed in their fixed-width big-endian
  // representation.  In particular, 1-byte values still have two digits.
  std::fprintf(
      stderr,
      "AC6_POST_RESUME_ACCESS kind=%s address=0x%08X size=%u "
      "value=0x%0*llX tick=%llu thread=%u lr=0x%08X function=%s "
      "generated_line=%u\n",
      kind, address, size, static_cast<int>(size * 2U),
      static_cast<unsigned long long>(value & mask),
      static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

inline void record_post_resume_bytes(const char *kind,
                                     std::uint32_t address,
                                     std::uint32_t size,
                                     const std::uint8_t *bytes,
                                     std::uint64_t tick,
                                     std::uint32_t thread,
                                     std::uint32_t lr,
                                     const char *generated_name,
                                     std::uint32_t generated_line) noexcept {
  record_xam_return_chain_bytes(kind, address, size, bytes, tick, thread, lr,
                                generated_name, generated_line);
  if (kind == nullptr || bytes == nullptr || size == 0U || size > 16U ||
      !claim_post_resume_access(thread)) {
    return;
  }
  std::array<char, 33U> encoded{};
  constexpr char kHex[] = "0123456789ABCDEF";
  for (std::uint32_t index = 0U; index < size; ++index) {
    encoded[index * 2U] = kHex[bytes[index] >> 4U];
    encoded[index * 2U + 1U] = kHex[bytes[index] & 0x0FU];
  }
  std::fprintf(
      stderr,
      "AC6_POST_RESUME_ACCESS kind=%s address=0x%08X size=%u bytes=%s "
      "tick=%llu thread=%u lr=0x%08X function=%s generated_line=%u\n",
      kind, address, size, encoded.data(),
      static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

// Reservation instructions are real memory routes, but their success and
// visibility are reservation-state dependent.  Refuse them rather than
// presenting an LR/site as a normal scalar load/store.  This consumes the
// qualified shot and emits an explicit, mapper-rejectable boundary row.
inline void refuse_post_resume_atomic(const char *kind,
                                      std::uint32_t address,
                                      std::uint32_t size,
                                      std::uint64_t tick,
                                      std::uint32_t thread,
                                      std::uint32_t lr,
                                      const char *reason = "atomic") noexcept {
  if (kind == nullptr || !claim_post_resume_access(thread)) {
    return;
  }
  std::fprintf(
      stderr,
      "AC6_POST_RESUME_ACCESS_REFUSED kind=%s address=0x%08X size=%u "
      "tick=%llu thread=%u lr=0x%08X reason=%s\n",
      kind, address, size, static_cast<unsigned long long>(tick), thread, lr,
      reason == nullptr ? "atomic" : reason);
}

[[nodiscard]] inline std::uint64_t post_resume_capture_attempts() noexcept {
  return post_resume_probe_state.capture_attempts.load(
      std::memory_order_relaxed);
}

// The scheduler still exposes this historical hook for other build slices.
// Post-resume arming is intentionally owned by the exact consume_guest_event
// site, so this compatibility entry point is a strict no-op.
inline void trace_event_post_set_schedule(std::uint32_t, std::uint32_t,
                                           std::uint32_t, std::uint32_t,
                                           std::uint64_t) noexcept {}

// Kept as a test-only reset hook.  Production code never calls it, so the
// one-shot bound remains process-global and deterministic.
inline void reset_post_resume_probe_for_tests() noexcept {
  post_resume_probe_state.handoff = PostResumeHandoff{};
  post_resume_probe_state.phase.store(0U, std::memory_order_release);
  post_resume_probe_state.capture_attempts.store(0U,
                                                 std::memory_order_relaxed);
  post_resume_probe_state.enabled.store(0U, std::memory_order_release);
}

}  // namespace ac6demo::guest_bridge_detail
