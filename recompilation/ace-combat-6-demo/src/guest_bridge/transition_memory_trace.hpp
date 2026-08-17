#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <utility>

namespace ac6demo::guest_bridge_detail {

inline const std::optional<std::pair<std::uint32_t, std::uint32_t>> &
transition_trace_range() noexcept {
  static const auto range = []()
      -> std::optional<std::pair<std::uint32_t, std::uint32_t>> {
    const char *text = std::getenv("AC6_DEMO_WATCH_TICK_RANGE");
    if (text == nullptr || *text == '\0') {
      return std::nullopt;
    }
    char *separator = nullptr;
    const auto begin = std::strtoull(text, &separator, 0);
    if (separator == text || *separator != ':') {
      return std::nullopt;
    }
    char *end = nullptr;
    const auto end_value = std::strtoull(separator + 1, &end, 0);
    if (end == separator + 1 || *end != '\0' || begin > 0xFFFFFFFFULL ||
        end_value > 0x100000000ULL || begin >= end_value) {
      return std::nullopt;
    }
    return std::pair{static_cast<std::uint32_t>(begin),
                     static_cast<std::uint32_t>(end_value)};
  }();
  return range;
}

inline bool transition_trace_address_allowed(std::uint32_t address) noexcept {
  const auto &range = transition_trace_range();
  return !range.has_value() ||
         (address >= range->first && address < range->second);
}

inline void trace_transition_store(
    std::uint32_t address, std::uint32_t size, std::uint32_t value,
    std::uint64_t tick, std::uint32_t thread, std::uint32_t lr,
    const char *generated_name, std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_TICK_STORES") != nullptr;
  static thread_local std::uint32_t record_count = 0U;
  const bool guest_stack = address >= 0x7F000000U && address < 0x80000000U;
  if (!enabled || guest_stack || tick < 268U || tick > 269U ||
      !transition_trace_address_allowed(address) || record_count >= 8192U) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_TICK_STORE address=0x%08X size=%u value=0x%08X tick=%llu "
      "thread=%u lr=0x%08X function=%s generated_line=%u\n",
      address, size, value, static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

inline void trace_transition_load(
    std::uint32_t address, std::uint32_t size, std::uint64_t value,
    std::uint64_t tick, std::uint32_t thread, std::uint32_t lr,
    const char *generated_name, std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_TICK_LOADS") != nullptr;
  static thread_local std::uint32_t record_count = 0U;
  if (!enabled || tick < 268U || tick > 269U ||
      !transition_trace_address_allowed(address) || record_count >= 8192U) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_TICK_LOAD address=0x%08X size=%u value=0x%08X tick=%llu "
      "thread=%u lr=0x%08X function=%s generated_line=%u\n",
      address, size, static_cast<std::uint32_t>(value),
      static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

inline void trace_controller_reader(
    std::uint32_t address, std::uint32_t size, std::uint64_t value,
    std::uint64_t tick, std::uint32_t thread, std::uint32_t lr,
    const char *generated_name, std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_CONTROLLER_READERS") != nullptr;
  static thread_local std::uint32_t record_count = 0U;
  constexpr std::uint32_t kControllerFieldsBegin = 0x829D1550U;
  constexpr std::uint32_t kControllerFieldsEnd = 0x829D15C4U;
  if (!enabled || tick != 252U || address < kControllerFieldsBegin ||
      address >= kControllerFieldsEnd || size == 0U || size > 8U ||
      record_count >= 512U) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_CONTROLLER_READ address=0x%08X size=%u value=0x%08X tick=%llu "
      "thread=%u lr=0x%08X function=%s generated_line=%u\n",
      address, size, static_cast<std::uint32_t>(value),
      static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

inline void trace_controller_target_reader(
    std::uint32_t address, std::uint32_t size, std::uint64_t value,
    std::uint64_t tick, std::uint32_t thread, std::uint32_t lr,
    const char *generated_name, std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_CONTROLLER_TARGET_READERS") != nullptr;
  static thread_local std::uint32_t record_count = 0U;
  constexpr std::uint32_t kTarget = 0x829D15BCU;
  if (!enabled || size == 0U || size > 8U ||
      static_cast<std::uint64_t>(address) > kTarget ||
      static_cast<std::uint64_t>(address) + size <= kTarget ||
      record_count >= 8192U) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_CONTROLLER_TARGET_READ address=0x%08X size=%u value=0x%08X "
      "tick=%llu thread=%u lr=0x%08X function=%s generated_line=%u\n",
      address, size, static_cast<std::uint32_t>(value),
      static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

inline void trace_input_semantic_access(
    const char *kind, std::uint32_t address, std::uint32_t size,
    std::uint64_t value, std::uint64_t tick, std::uint32_t thread,
    std::uint32_t lr, const char *generated_name,
    std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_INPUT_SEMANTICS") != nullptr;
  static thread_local std::uint32_t record_count = 0U;
  constexpr std::uint32_t kNormalized = 0x827B37E0U;
  constexpr std::uint32_t kLogicalBegin = 0x82798480U;
  constexpr std::uint32_t kLogicalEnd = 0x8279848CU;
  const auto end = static_cast<std::uint64_t>(address) + size;
  const bool normalized =
      static_cast<std::uint64_t>(address) < kNormalized + 4U &&
      end > kNormalized;
  const bool logical = static_cast<std::uint64_t>(address) < kLogicalEnd &&
                       end > kLogicalBegin;
  if (!enabled || size == 0U || size > 8U || (!normalized && !logical) ||
      record_count >= 8192U) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_INPUT_SEMANTIC_ACCESS kind=%s address=0x%08X size=%u "
      "value=0x%08X tick=%llu thread=%u lr=0x%08X function=%s "
      "generated_line=%u\n",
      kind == nullptr ? "" : kind, address, size,
      static_cast<std::uint32_t>(value),
      static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

inline void trace_task_list_access(
    const char *kind, std::uint32_t address, std::uint32_t size,
    std::uint64_t value, std::uint64_t tick, std::uint32_t thread,
    std::uint32_t lr, const char *generated_name,
    std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_TASK_LIST") != nullptr;
  static thread_local std::uint32_t record_count = 0U;
  constexpr std::uint32_t kBegin = 0x18970400U;
  constexpr std::uint32_t kEnd = 0x18970440U;
  const auto end = static_cast<std::uint64_t>(address) + size;
  if (!enabled || size == 0U || size > 8U || address < kBegin ||
      end > kEnd || record_count >= 8192U) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_TASK_LIST_ACCESS kind=%s address=0x%08X size=%u "
      "value=0x%08X tick=%llu thread=%u lr=0x%08X function=%s "
      "generated_line=%u\n",
      kind == nullptr ? "" : kind, address, size,
      static_cast<std::uint32_t>(value),
      static_cast<unsigned long long>(tick), thread, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
}

} // namespace ac6demo::guest_bridge_detail
