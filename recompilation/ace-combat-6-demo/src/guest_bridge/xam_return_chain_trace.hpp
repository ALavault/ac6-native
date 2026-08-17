#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace ac6demo::guest_bridge_detail {

// These are binary-qualified PAL addresses.  They are deliberately kept as
// metadata for the mapper; the runtime never turns LR or a generated symbol
// into a PC.
inline constexpr std::uint32_t kXamReturnCaller = 0x822F616CU;
inline constexpr std::uint32_t kXamReturnExclusivePc = 0x822F5EA0U;
inline constexpr std::uint32_t kXamReturnControllerObject = 0x829D153CU;
inline constexpr std::uint32_t kXamReturnExclusiveAddress =
    kXamReturnControllerObject + 0x80U;
inline constexpr std::uint32_t kXamReturnMaxAccesses = 32U;

[[nodiscard]] inline bool xam_return_chain_exclusive_site(
    const char *function, std::uint32_t generated_line) noexcept {
  return generated_line == 3948U && function != nullptr &&
         (std::string_view{function} == "sub_822F5E58" ||
          std::string_view{function} == "__imp__sub_822F5E58");
}

struct XamReturnChainState final {
  // 0 = not initialized, 1 = disabled, 2 = enabled, 3 = initializer.
  std::atomic<std::uint8_t> enabled{0U};
  // 0 = idle, 1 = armed, 2 = stopped, 3 = arm payload being published.
  std::atomic<std::uint8_t> phase{0U};
  // This is a saturating count.  It is never incremented past the bound.
  std::atomic<std::uint32_t> accesses{0U};
  std::atomic<std::uint32_t> armed_thread{0U};
  std::atomic_bool stop_logged{false};
  // Claims and terminal transitions share a tiny lock.  The lock is only
  // reached after the relaxed process-lifetime fast flag has passed.
  std::atomic_flag claim_lock = ATOMIC_FLAG_INIT;
  std::uint64_t tick{};
  std::uint32_t thread{};
  std::uint32_t user{};
  std::uint32_t flags{};
  std::uint32_t output{};
  std::uint32_t result{};
  std::array<std::uint8_t, 16U> state16{};
};

inline XamReturnChainState xam_return_chain_state{};

inline void initialize_xam_return_chain_watch() noexcept {
  auto state = xam_return_chain_state.enabled.load(std::memory_order_acquire);
  if (state == 1U || state == 2U) return;

  std::uint8_t expected = 0U;
  if (xam_return_chain_state.enabled.compare_exchange_strong(
          expected, 3U, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    const char *value = std::getenv("AC6_DEMO_WATCH_XAM_RETURN_CHAIN");
    xam_return_chain_state.enabled.store(
        value != nullptr && std::string_view{value} == "1" ? 2U : 1U,
        std::memory_order_release);
    return;
  }
  while (xam_return_chain_state.enabled.load(std::memory_order_acquire) == 3U) {
  }
}

// Hot-path callers use only this cached flag.  No getenv or string work is
// performed after process initialization.
[[nodiscard]] inline bool xam_return_chain_watch_enabled_fast() noexcept {
  return xam_return_chain_state.enabled.load(std::memory_order_relaxed) == 2U;
}

inline void arm_xam_return_chain(std::uint32_t caller_lr, std::uint64_t tick,
                                 std::uint32_t thread, std::uint32_t user,
                                 std::uint32_t flags, std::uint32_t output,
                                 std::uint32_t result,
                                 const std::uint8_t *state16) noexcept {
  initialize_xam_return_chain_watch();
  if (!xam_return_chain_watch_enabled_fast() ||
      caller_lr != kXamReturnCaller || thread == 0U ||
      output != kXamReturnControllerObject || result != 0U ||
      state16 == nullptr) {
    return;
  }

  std::uint8_t expected = 0U;
  if (!xam_return_chain_state.phase.compare_exchange_strong(
          expected, 3U, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return;
  }
  xam_return_chain_state.tick = tick;
  xam_return_chain_state.thread = thread;
  xam_return_chain_state.armed_thread.store(thread, std::memory_order_release);
  xam_return_chain_state.user = user;
  xam_return_chain_state.flags = flags;
  xam_return_chain_state.output = output;
  xam_return_chain_state.result = result;
  xam_return_chain_state.accesses.store(0U, std::memory_order_relaxed);
  xam_return_chain_state.stop_logged.store(false, std::memory_order_relaxed);
  for (std::size_t i = 0; i < 16U; ++i) {
    xam_return_chain_state.state16[i] = state16[i];
  }
  std::fprintf(
      stderr,
      "AC6_XAM_RETURN_CHAIN_ARM caller_lr=0x%08X tick=%llu thread=%u "
      "user=%u flags=0x%08X output=0x%08X result=0x%08X state16=",
      caller_lr, static_cast<unsigned long long>(tick), thread, user, flags,
      output, result);
  for (const auto byte : xam_return_chain_state.state16) {
    std::fprintf(stderr, "%02X", byte);
  }
  std::fputc('\n', stderr);
  xam_return_chain_state.phase.store(1U, std::memory_order_release);
}

inline void xam_return_chain_lock() noexcept {
  while (xam_return_chain_state.claim_lock.test_and_set(
      std::memory_order_acquire)) {
  }
}

inline void xam_return_chain_unlock() noexcept {
  xam_return_chain_state.claim_lock.clear(std::memory_order_release);
}

inline void xam_return_chain_stop(const char *reason) noexcept {
  if (!xam_return_chain_watch_enabled_fast()) return;
  xam_return_chain_lock();
  const auto phase = xam_return_chain_state.phase.load(std::memory_order_relaxed);
  if (phase == 1U) {
    xam_return_chain_state.phase.store(2U, std::memory_order_release);
    xam_return_chain_state.stop_logged.store(true, std::memory_order_relaxed);
    std::fprintf(
        stderr, "AC6_XAM_RETURN_CHAIN_STOP reason=%s accesses=%u\n",
        reason == nullptr ? "unknown" : reason,
        xam_return_chain_state.accesses.load(std::memory_order_relaxed));
  }
  xam_return_chain_unlock();
}

inline void xam_return_chain_emit_stop(const char *reason) noexcept {
  if (xam_return_chain_state.phase.load(std::memory_order_acquire) != 2U) {
    return;
  }
  bool expected = false;
  if (xam_return_chain_state.stop_logged.compare_exchange_strong(
          expected, true, std::memory_order_acq_rel,
          std::memory_order_relaxed)) {
    std::fprintf(stderr, "AC6_XAM_RETURN_CHAIN_STOP reason=%s accesses=%u\n",
                 reason == nullptr ? "unknown" : reason,
                 xam_return_chain_state.accesses.load(std::memory_order_relaxed));
  }
}

// A caller supplies the current guest thread on every claim.  A mismatched
// thread returns before taking the budget lock, so it can never consume a
// claim.  `terminal` closes the phase as part of the same critical section;
// this is used for refusal and the qualified exclusive target.
[[nodiscard]] inline bool xam_return_chain_claim(std::uint32_t thread,
                                                 bool terminal = false) noexcept {
  if (!xam_return_chain_watch_enabled_fast() || thread == 0U ||
      thread != xam_return_chain_state.armed_thread.load(
          std::memory_order_acquire)) {
    return false;
  }
  xam_return_chain_lock();
  const auto phase = xam_return_chain_state.phase.load(std::memory_order_relaxed);
  const auto armed = xam_return_chain_state.armed_thread.load(
      std::memory_order_relaxed);
  auto count = xam_return_chain_state.accesses.load(std::memory_order_relaxed);
  if (phase != 1U || armed != thread || count >= kXamReturnMaxAccesses) {
    if (phase == 1U && count >= kXamReturnMaxAccesses) {
      xam_return_chain_state.phase.store(2U, std::memory_order_release);
    }
    xam_return_chain_unlock();
    return false;
  }
  ++count;
  xam_return_chain_state.accesses.store(count, std::memory_order_relaxed);
  if (terminal || count == kXamReturnMaxAccesses) {
    xam_return_chain_state.phase.store(2U, std::memory_order_release);
  }
  xam_return_chain_unlock();
  return true;
}

inline void print_xam_value_be(const std::uint8_t *bytes,
                              std::uint32_t size) noexcept {
  std::fprintf(stderr, "0x");
  for (std::uint32_t i = 0U; i < size; ++i) {
    std::fprintf(stderr, "%02X", bytes[i]);
  }
}

inline void record_xam_return_chain(
    const char *kind, std::uint32_t address, std::uint32_t size,
    std::uint64_t value_be, std::uint64_t tick, std::uint32_t thread,
    std::uint32_t lr, const char *function,
    std::uint32_t generated_line) noexcept {
  const bool target = kind != nullptr &&
                      std::string_view{kind} == "store32" &&
                      address == kXamReturnExclusiveAddress;
  const bool qualified = target &&
                         xam_return_chain_exclusive_site(function, generated_line);
  if (size == 0U || size > 8U ||
      !xam_return_chain_claim(thread, target)) {
    return;
  }
  std::fprintf(
      stderr,
      "AC6_XAM_RETURN_CHAIN_ACCESS kind=%s address=0x%08X size=%u "
      "value_be=0x%0*llX tick=%llu thread=%u lr=0x%08X function=%s "
      "generated_line=%u\n",
      kind == nullptr ? "" : kind, address, size,
      static_cast<int>(size * 2U), static_cast<unsigned long long>(value_be),
      static_cast<unsigned long long>(tick), thread, lr,
      function == nullptr ? "" : function, generated_line);
  if (target && !qualified) {
    std::fprintf(stderr,
                 "AC6_XAM_RETURN_CHAIN_ACCESS_REFUSED kind=store32 "
                 "address=0x%08X reason=site_not_allowlisted\n", address);
  }
  if (xam_return_chain_state.phase.load(std::memory_order_acquire) == 2U) {
    xam_return_chain_emit_stop(
        qualified
            ? "qualified_store_exclusive"
            : target ? "site_not_allowlisted" : "bound");
  }
}

// Vector values are intentionally not reduced to a 64-bit integer.  The
// bytes are the post-operation Xenon memory order and are printed as one
// complete big-endian U128 value.
inline void record_xam_return_chain_bytes(
    const char *kind, std::uint32_t address, std::uint32_t size,
    const std::uint8_t *bytes, std::uint64_t tick, std::uint32_t thread,
    std::uint32_t lr, const char *function,
    std::uint32_t generated_line) noexcept {
  if (bytes == nullptr || size != 16U ||
      !xam_return_chain_claim(thread)) {
    return;
  }
  std::fprintf(
      stderr,
      "AC6_XAM_RETURN_CHAIN_ACCESS kind=%s address=0x%08X size=16 "
      "value_be=",
      kind == nullptr ? "" : kind, address);
  print_xam_value_be(bytes, 16U);
  std::fprintf(stderr,
               " tick=%llu thread=%u lr=0x%08X function=%s generated_line=%u\n",
               static_cast<unsigned long long>(tick), thread, lr,
               function == nullptr ? "" : function, generated_line);
  if (xam_return_chain_state.accesses.load(std::memory_order_relaxed) ==
      kXamReturnMaxAccesses) {
    xam_return_chain_emit_stop("bound");
  }
}

// Reservation instructions are captured only after the operation.  A site
// PC must be supplied by a qualified metadata consumer; runtime hooks pass
// zero because LR/function names are not PCs.  An absent or non-allowlisted
// site emits a refusal and terminates the shot after preserving the record.
inline void record_xam_return_chain_atomic(
    const char *kind, std::uint32_t address, std::uint32_t size,
    std::uint64_t value_be, bool success, std::uint64_t tick,
    std::uint32_t thread, std::uint32_t lr, const char *function,
    std::uint32_t generated_line, std::uint32_t site_pc = 0U) noexcept {
  if (!xam_return_chain_watch_enabled_fast() ||
      !xam_return_chain_claim(thread, true)) {
    return;
  }
  std::fprintf(
      stderr,
      "AC6_XAM_RETURN_CHAIN_ATOMIC kind=%s address=0x%08X size=%u "
      "value_be=0x%0*llX success=%u tick=%llu thread=%u lr=0x%08X "
      "function=%s generated_line=%u site_pc=0x%08X\n",
      kind == nullptr ? "" : kind, address, size,
      static_cast<int>(size * 2U), static_cast<unsigned long long>(value_be),
      success ? 1U : 0U, static_cast<unsigned long long>(tick), thread, lr,
      function == nullptr ? "" : function, generated_line, site_pc);
  if (site_pc != kXamReturnExclusivePc ||
      address != kXamReturnExclusiveAddress) {
    std::fprintf(
        stderr,
        "AC6_XAM_RETURN_CHAIN_ACCESS_REFUSED kind=%s address=0x%08X "
        "size=%u value_be=0x%0*llX success=%u tick=%llu thread=%u "
        "lr=0x%08X function=%s generated_line=%u site_pc=0x%08X "
        "reason=site_not_allowlisted\n",
        kind == nullptr ? "" : kind, address, size,
        static_cast<int>(size * 2U), static_cast<unsigned long long>(value_be),
        success ? 1U : 0U, static_cast<unsigned long long>(tick), thread, lr,
        function == nullptr ? "" : function, generated_line, site_pc);
  }
  xam_return_chain_emit_stop(
      success && site_pc == kXamReturnExclusivePc &&
              address == kXamReturnExclusiveAddress
          ? "qualified_store_exclusive"
          : "atomic_unqualifiable");
}

// Compatibility entry point used by old generated slices.  It now records a
// complete post-operation atomic row instead of consuming a budget before the
// operation and dropping its value/result.
inline void refuse_xam_return_chain_atomic(
    const char *kind, std::uint32_t address, std::uint32_t size,
    std::uint64_t tick, std::uint32_t thread, std::uint32_t lr,
    const char *function, std::uint32_t generated_line,
    std::uint64_t value_be = 0U, bool success = false,
    std::uint32_t site_pc = 0U) noexcept {
  record_xam_return_chain_atomic(kind, address, size, value_be, success, tick,
                                 thread, lr, function, generated_line, site_pc);
}

inline void reset_xam_return_chain_for_tests() noexcept {
  xam_return_chain_state.phase.store(0U, std::memory_order_release);
  xam_return_chain_state.accesses.store(0U, std::memory_order_relaxed);
  xam_return_chain_state.stop_logged.store(false, std::memory_order_relaxed);
  xam_return_chain_state.armed_thread.store(0U, std::memory_order_release);
  xam_return_chain_state.thread = 0U;
  xam_return_chain_state.user = 0U;
  xam_return_chain_state.flags = 0U;
  xam_return_chain_state.output = 0U;
  xam_return_chain_state.result = 0U;
  xam_return_chain_state.state16.fill(0U);
  xam_return_chain_state.claim_lock.clear(std::memory_order_release);
  xam_return_chain_state.enabled.store(0U, std::memory_order_release);
}

}  // namespace ac6demo::guest_bridge_detail
