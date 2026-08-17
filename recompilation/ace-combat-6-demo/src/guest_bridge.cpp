#include "ac6demo/guest_bridge.hpp"
#include "ac6demo/content.hpp"
#include "ac6demo/graphics.hpp"
#include "ac6demo/hash.hpp"
#include "ac6demo/ppc.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "guest_bridge/transition_memory_trace.hpp"
#include "guest_bridge/xma_import_trace.hpp"
namespace ac6demo {
bool initialize_guest_ansi_string(GuestMemory &memory,
                                  std::uint32_t destination,
                                  std::uint32_t source) {
  constexpr std::uint32_t kAnsiStringSize = 8U;
  constexpr std::uint32_t kMaximumLength = 0xFFFFU;
  if (!memory.mapped(destination, kAnsiStringSize)) {
    return false;
  }
  if (source == 0U) {
    memory.store_u16(destination, 0U);
    memory.store_u16(destination + 2U, 0U);
    memory.store_u32(destination + 4U, 0U);
    return true;
  }
  for (std::uint32_t length = 0U; length < kMaximumLength; ++length) {
    if (source > std::numeric_limits<std::uint32_t>::max() - length ||
        !memory.mapped(source + length, 1U)) {
      return false;
    }
    if (memory.load_u8(source + length) == 0U) {
      memory.store_u16(destination, static_cast<std::uint16_t>(length));
      memory.store_u16(destination + 2U,
                       static_cast<std::uint16_t>(length + 1U));
      memory.store_u32(destination + 4U, source);
      return true;
    }
  }
  return false;
}
bool write_guest_file_network_open_information(GuestMemory &memory,
                                               std::uint32_t destination,
                                               std::uint32_t length,
                                               std::uint64_t file_size) {
  constexpr std::uint32_t kStructureSize = 56U;
  constexpr std::uint32_t kFileAttributeNormal = 0x80U;
  if (length < kStructureSize || !memory.mapped(destination, kStructureSize)) {
    return false;
  }
  for (std::uint32_t offset = 0U; offset < 32U; offset += 8U) {
    memory.store_u64(destination + offset, 0U);
  }
  const auto allocation_size = (file_size + 0xFFFU) & ~std::uint64_t{0xFFFU};
  memory.store_u64(destination + 32U, allocation_size);
  memory.store_u64(destination + 40U, file_size);
  memory.store_u32(destination + 48U, kFileAttributeNormal);
  memory.store_u32(destination + 52U, 0U);
  return true;
}
} // namespace ac6demo
#ifdef AC6_DEMO_GENERATED_GUEST
#include "ppc_recomp_shared.h"
#include "guest_bridge/qualified_thunk.hpp"
#include "guest_bridge/event_handoff_trace.hpp"
#include "guest_bridge/event_post_set_trace.hpp"
#include "guest_bridge/event_handle_writer_trace.hpp"
#include "guest_bridge/event_handle_consumer_trace.hpp"
#include "guest_bridge/event_handle_payload_writer_trace.hpp"
#include "guest_bridge/frontbuffer_writer_trace.hpp"
#include "guest_bridge/graphics_interrupt_trace.hpp"
#include "guest_bridge/queue_slot_trace.hpp"
#include <ucontext.h>
namespace {
using ac6demo::GuestBridge;
using ac6demo::GuestMemory;
extern "C" void AC6_PPC_SET_TICK(std::uint64_t) noexcept;
extern "C" void AC6_PPC_SET_POST_RESUME_VECTOR_CONTEXT(
    PPCContext &, GuestBridge *, const char *, std::uint64_t,
    std::uint32_t) noexcept;
struct Reservation final {
  std::uint32_t address{}; std::uint64_t generation{};
  std::uint64_t trailing_generation{}; std::uint8_t width{};
  bool valid{};
};
struct GuestEvent final {
  bool signaled{};
  bool manual_reset{};
  std::uint32_t granted_thread{};
};
struct GuestNotifyListener final {
  std::uint64_t mask{};
  std::uint32_t max_version{};
};
struct GuestTimer final {
  std::uint32_t timer_type{};
  bool signaled{};
  bool active{};
  std::uint64_t due_tick{std::numeric_limits<std::uint64_t>::max()};
  std::uint64_t period_ticks{};
};
struct GuestSemaphore final {
  std::uint32_t count{};
  std::uint32_t maximum{};
};
struct GuestMutant final {
  std::uint32_t owner{};
  std::uint32_t recursion{};
};
struct GuestCriticalSection final {
  std::uint32_t owner{};
  std::uint32_t recursion{};
};
struct GuestIndirectCall final {
  std::uint32_t target{};
  std::uint32_t lr{};
  std::uint64_t count{};
  std::uint64_t tick{};
};
[[nodiscard]] ac6demo::GuestRegisterSnapshot
snapshot_registers(const PPCContext &context) noexcept {
  return ac6demo::GuestRegisterSnapshot{
      context.r1.u32,  context.r3.u32,  context.r4.u32,  context.r5.u32,
      context.r6.u32,  context.r7.u32,  context.r8.u32,  context.r9.u32,
      context.r10.u32, context.r11.u32, context.r12.u32, context.r13.u32,
      context.r26.u32, context.r27.u32, context.r28.u32, context.r29.u32,
      context.r30.u32, context.r31.u32};
}
thread_local GuestBridge *active_bridge = nullptr;
thread_local std::unordered_map<const PPCContext *, Reservation> reservations;
thread_local std::unordered_map<std::uint32_t, GuestCriticalSection>
    critical_sections;
thread_local std::uint32_t kernel_critical_region_depth = 0U;
thread_local std::unordered_map<std::uint32_t, std::uint32_t> tls_values;
thread_local std::unordered_map<std::uint32_t, bool> tls_slots;
thread_local std::unordered_map<std::uint32_t, GuestEvent> events;
thread_local std::unordered_map<std::uint32_t, GuestNotifyListener>
    notify_listeners;
thread_local std::unordered_map<std::uint32_t, GuestTimer> timers;
thread_local std::unordered_map<std::uint32_t, GuestMutant> mutants;
thread_local std::unordered_map<std::uint32_t, GuestSemaphore> semaphores;
thread_local std::unordered_map<std::uint32_t, GuestSemaphore>
    kernel_semaphores;
thread_local std::uint32_t next_tls_slot = 0U;
thread_local std::int32_t network_error = 10093; // WSANOTINITIALISED
thread_local std::uint32_t next_event_handle = 0xE0000000U;
thread_local std::uint32_t next_notify_handle = 0xE5000000U;
thread_local std::uint32_t next_timer_handle = 0xE6000000U;
thread_local std::uint32_t current_guest_thread_id = 1U;
thread_local const char* current_load_generated_name = nullptr;
thread_local std::uint32_t current_load_generated_line = 0U;
thread_local std::uint64_t event_set_count = 0U;
thread_local std::uint32_t last_event_set_handle = 0U;
thread_local std::uint32_t last_event_set_thread = 0U;
thread_local std::unordered_map<std::uint32_t, GuestIndirectCall>
    indirect_calls;
thread_local bool chunk_target_store_trace_active = false;
thread_local std::unordered_map<std::uint32_t, std::uint32_t> spinlock_owners;
thread_local std::unordered_map<std::uint32_t, std::uint8_t> guest_irql;
thread_local std::array<ac6demo::GuestEventPublicationSnapshot, 32U>
    event_publications{};
thread_local std::uint32_t event_publication_count = 0U;
thread_local std::uint32_t graphics_interrupt_callback = 0U;
thread_local std::uint32_t graphics_interrupt_context = 0U;
struct GuestFunctionTable final {
  const PPCFuncMapping *begin{};
  std::size_t count{};
  bool strictly_sorted{};
};
constexpr std::uint32_t kGuestThreadStackSize = 0x40000U;
constexpr std::uint32_t kGuestThreadBase = 0x7F000000U;
constexpr std::uint32_t kPrimaryGuestThreadId = 1U;
constexpr std::uint32_t kSchedulerThreadId = 0U;
constexpr std::uint8_t kWaitEvent = 1U;
constexpr std::uint8_t kWaitSemaphore = 2U;
constexpr std::uint8_t kWaitKernelEvent = 3U;
constexpr std::uint8_t kWaitThread = 4U;
constexpr std::uint8_t kWaitMutant = 5U;
constexpr std::uint8_t kWaitCriticalSection = 6U;
constexpr std::uint8_t kWaitKernelSemaphore = 7U;
constexpr std::uint8_t kWaitTimer = 8U;
constexpr std::uint32_t kWaitMultipleKey = 0xFFFFFFFFU;
constexpr std::uint64_t kNoWakeTick = std::numeric_limits<std::uint64_t>::max();
constexpr std::size_t kHostFiberStackSize = 1024U * 1024U;
constexpr std::uint64_t kHundredNanosecondsPerGuestTick = 166'667U;
constexpr std::size_t kMaxGuestActivationsPerSlice = 256U;
constexpr std::size_t kGuestMemoryOperationsPerQuantum = 10'000U;
thread_local std::size_t guest_memory_operations_since_yield = 0U;
[[nodiscard]] const GuestFunctionTable &guest_function_table() noexcept {
  static const GuestFunctionTable table = [] {
    GuestFunctionTable result{PPCFuncMappings, 0U, true};
    std::size_t index = 0U;
    while (PPCFuncMappings[index].host != nullptr) {
      if (index != 0U &&
          PPCFuncMappings[index - 1U].guest >= PPCFuncMappings[index].guest) {
        result.strictly_sorted = false;
      }
      ++index;
    }
    result.count = index;
    return result;
  }();
  return table;
}
[[nodiscard]] PPCFunc *
lookup_guest_function(std::uint32_t guest_address) noexcept {
  const auto &table = guest_function_table();
  if (!table.strictly_sorted) {
    return nullptr;
  }
  const auto *end = table.begin + table.count;
  const auto *found = std::lower_bound(
      table.begin, end, guest_address,
      [](const PPCFuncMapping &mapping, std::uint32_t address) {
        return mapping.guest < address;
      });
  return found != end && found->guest == guest_address ? found->host : nullptr;
}
struct GuestThreadExit final {};
struct GuestThreadBlocked final {};
struct GuestFiber final {
  ucontext_t context{};
  std::vector<std::byte> stack;
  std::unique_ptr<PPCContext> ppc;
  std::exception_ptr failure;
};
thread_local ucontext_t *active_scheduler_context = nullptr;
[[nodiscard]] GuestBridge &require_bridge() noexcept {
  if (active_bridge == nullptr) { std::terminate(); }
  return *active_bridge;
}
#include "guest_bridge/affinity_trace.hpp"
#include "guest_bridge/dynamic_object_vtable_trace.hpp"
void record_event_publication(std::uint32_t key, std::uint32_t lr,
                              std::uint8_t kind) noexcept {
  ++event_set_count;
  last_event_set_handle = key;
  last_event_set_thread = current_guest_thread_id;
  ac6demo::guest_bridge_detail::trace_event_handoff("publication", key, 0U, kind, current_guest_thread_id, require_bridge().tick(), lr, static_cast<std::uint32_t>(kind), 0U, 0U);
  if (event_publication_count < event_publications.size()) {
    event_publications[event_publication_count++] = ac6demo::GuestEventPublicationSnapshot{key, current_guest_thread_id, lr, kind};
  }
}
void publish_guest_event(GuestBridge &bridge, std::uint32_t handle,
                         GuestEvent &event, std::uint32_t lr) noexcept {
  record_event_publication(handle, lr, 1U);
  if (event.manual_reset) {
    event.signaled = true;
    bridge.wake_guest_waiters(kWaitEvent, handle);
    ac6demo::guest_bridge_detail::trace_event_handoff("event_wake", handle, 0U, kWaitEvent, current_guest_thread_id, bridge.tick(), lr, 3U, 0U, 0U);
    return;
  }
  const auto waiter = bridge.wake_one_guest_waiter(kWaitEvent, handle);
  if (waiter != 0U) {
    event.signaled = false;
    event.granted_thread = waiter;
  } else {
    event.signaled = true;
  }
  ac6demo::guest_bridge_detail::trace_event_handoff("event_wake", handle, 0U, kWaitEvent, current_guest_thread_id, bridge.tick(), lr, event.signaled ? 1U : 0U, waiter, 0U);
}
[[nodiscard]] bool consume_guest_event(GuestEvent &event) noexcept {
  if (event.granted_thread == current_guest_thread_id) {
    event.granted_thread = 0U;
    return true;
  }
  if (!event.signaled) {
    return false;
  }
  if (!event.manual_reset) {
    event.signaled = false;
  }
  return true;
}
[[nodiscard]] GuestMemory &memory_for(PPCContext &) noexcept {
  return require_bridge().memory();
}
void trace_render_queue_writer(PPCContext &context, std::uint32_t address,
                               std::uint32_t value, const char *generated_name,
                               std::uint32_t generated_line) {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_RENDER_QUEUE_WRITERS") != nullptr;
  constexpr std::uint32_t kProducer = 0x8238CD90U;
  constexpr std::uint32_t kConsumer = 0x8238CD94U;
  constexpr std::uint32_t kSlotStride = 96U;
  if (!enabled || (address != kProducer && address != kConsumer)) {
    return;
  }
  const bool has_previous_slot = value != 0U;
  const auto slot_index = (value + 0xFFU) & 0xFFU;
  const auto object_offset = address == kProducer ? 0x60D0U : 0x60D4U;
  const auto slot_offset = address == kProducer ? 272U : 208U;
  const auto object = address - object_offset;
  const auto slot_address = object + slot_offset + slot_index * kSlotStride;
  std::fprintf(stderr,
               "AC6_RENDER_QUEUE_WRITE address=0x%08X value=%u tick=%llu "
               "thread=%u lr=0x%08X function=%s generated_line=%u "
               "slot_index=%u slot_address=0x%08X slot_valid=%u\n",
               address, value,
               static_cast<unsigned long long>(require_bridge().tick()),
               current_guest_thread_id, static_cast<std::uint32_t>(context.lr),
               generated_name == nullptr ? "" : generated_name, generated_line,
               slot_index, slot_address, has_previous_slot ? 1U : 0U);
  if (!has_previous_slot || !memory_for(context).mapped(slot_address, kSlotStride)) {
    return;
  }
  const auto bytes = memory_for(context).load_bytes(slot_address, kSlotStride);
  std::fputs("AC6_RENDER_QUEUE_SLOT bytes=", stderr);
  for (const auto byte : bytes) {
    std::fprintf(stderr, "%02X", std::to_integer<unsigned int>(byte));
  }
  std::fputc('\n', stderr);
}
void trace_render_queue_slot_store(PPCContext &c, std::uint32_t a, std::uint32_t s, bool nz, const char *n, std::uint32_t l) {
  ac6demo::guest_bridge_detail::trace_render_queue_slot_store(a, s, nz, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(c.lr), n, l);
}
void trace_chunk_target_store(PPCContext &context, std::uint32_t address,
                             std::uint32_t size, std::uint64_t value,
                             const char *generated_name,
                             std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_CHUNK_TARGET_STORES") != nullptr;
  static thread_local std::uint32_t record_count = 0U;
  if (!enabled || !chunk_target_store_trace_active || record_count >= 8192U) {
    return;
  }
  ++record_count;
  std::fprintf(stderr,
               "AC6_CHUNK_TARGET_STORE address=0x%08X size=%u value=0x%08X "
               "tick=%llu thread=%u lr=0x%08X function=%s generated_line=%u\n",
               address, size, static_cast<std::uint32_t>(value),
               static_cast<unsigned long long>(require_bridge().tick()),
               current_guest_thread_id, static_cast<std::uint32_t>(context.lr),
               generated_name == nullptr ? "" : generated_name,
               generated_line);
}
void trace_transition_store(PPCContext &context, std::uint32_t address,
                            std::uint32_t size, std::uint32_t value,
                            const char *generated_name,
                            std::uint32_t generated_line) noexcept {
  ac6demo::guest_bridge_detail::trace_transition_store(
      address, size, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
}
struct GuestFormatSpec final {
  bool left{};
  bool alternate{};
  bool zero_pad{};
  bool plus{};
  bool space{};
  int width{-1};
  int precision{-1};
  char length{};
};
[[nodiscard]] bool load_guest_string(GuestMemory &memory, std::uint32_t address,
                                     std::size_t limit, std::string *result) {
  result->clear();
  for (std::size_t index = 0; index < limit; ++index) {
    if (address > std::numeric_limits<std::uint32_t>::max() - index ||
        !memory.mapped(address + static_cast<std::uint32_t>(index), 1U)) {
      return false;
    }
    const auto value =
        memory.load_u8(address + static_cast<std::uint32_t>(index));
    if (value == 0U) {
      return true;
    }
    result->push_back(static_cast<char>(value));
  }
  return false;
}
[[nodiscard]] bool next_guest_argument(GuestMemory &memory,
                                       std::uint32_t *cursor,
                                       std::uint64_t *value) {
  if (*cursor > std::numeric_limits<std::uint32_t>::max() - 8U ||
      !memory.mapped(*cursor, 8U)) {
    return false;
  }
  *value = memory.load_u64(*cursor);
  *cursor += 8U;
  return true;
}
[[nodiscard]] std::string apply_format_width(std::string value,
                                             const GuestFormatSpec &spec,
                                             bool numeric) {
  if (spec.width < 0 || static_cast<std::size_t>(spec.width) <= value.size()) {
    return value;
  }
  const auto padding = static_cast<std::size_t>(spec.width) - value.size();
  const char fill =
      spec.zero_pad && numeric && !spec.left && spec.precision < 0 ? '0' : ' ';
  if (spec.left) {
    value.append(padding, ' ');
    return value;
  }
  if (fill == '0' && numeric && !value.empty() &&
      (value.front() == '-' || value.front() == '+' || value.front() == ' ')) {
    value.insert(1U, padding, '0');
    return value;
  }
  if (fill == '0' && numeric && value.starts_with("0x")) {
    value.insert(2U, padding, '0');
    return value;
  }
  value.insert(0U, padding, fill);
  return value;
}
[[nodiscard]] bool
format_guest_string(GuestMemory &memory, std::uint32_t buffer,
                    std::uint32_t count, std::uint32_t format,
                    std::uint32_t arguments, std::string *output) {
  std::string pattern;
  if (!load_guest_string(memory, format, 4096U, &pattern)) {
    return false;
  }
  output->clear();
  for (std::size_t index = 0; index < pattern.size();) {
    if (pattern[index] != '%') {
      output->push_back(pattern[index++]);
      continue;
    }
    ++index;
    if (index == pattern.size()) {
      return false;
    }
    if (pattern[index] == '%') {
      output->push_back('%');
      ++index;
      continue;
    }
    GuestFormatSpec spec;
    bool parsing_flags = true;
    while (parsing_flags && index < pattern.size()) {
      switch (pattern[index]) {
      case '-':
        spec.left = true;
        break;
      case '#':
        spec.alternate = true;
        break;
      case '0':
        spec.zero_pad = true;
        break;
      case '+':
        spec.plus = true;
        break;
      case ' ':
        spec.space = true;
        break;
      default:
        parsing_flags = false;
        continue;
      }
      ++index;
    }
    if (index < pattern.size() && pattern[index] == '*') {
      std::uint64_t raw{};
      if (!next_guest_argument(memory, &arguments, &raw)) {
        return false;
      }
      const auto width = static_cast<std::int32_t>(raw);
      spec.left = width < 0;
      spec.width = width < 0 ? -width : width;
      ++index;
    } else {
      spec.width = 0;
      while (index < pattern.size() && pattern[index] >= '0' &&
             pattern[index] <= '9') {
        if (spec.width > 100000) {
          return false;
        }
        spec.width = spec.width * 10 + (pattern[index] - '0');
        ++index;
      }
      if (spec.width == 0) {
        spec.width = -1;
      }
    }
    if (index < pattern.size() && pattern[index] == '.') {
      ++index;
      spec.precision = 0;
      if (index < pattern.size() && pattern[index] == '*') {
        std::uint64_t raw{};
        if (!next_guest_argument(memory, &arguments, &raw)) {
          return false;
        }
        spec.precision = static_cast<std::int32_t>(raw);
        ++index;
      } else {
        while (index < pattern.size() && pattern[index] >= '0' &&
               pattern[index] <= '9') {
          if (spec.precision > 100000) {
            return false;
          }
          spec.precision = spec.precision * 10 + (pattern[index] - '0');
          ++index;
        }
      }
    }
    if (index < pattern.size() &&
        (pattern[index] == 'h' || pattern[index] == 'l' ||
         pattern[index] == 'z')) {
      spec.length = pattern[index++];
      if (spec.length == 'l' && index < pattern.size() &&
          pattern[index] == 'l') {
        ++index;
        spec.length = 'L';
      }
    }
    if (index == pattern.size()) {
      return false;
    }
    const char conversion = pattern[index++];
    if (conversion == 's') {
      std::uint64_t raw{};
      std::string value;
      if (!next_guest_argument(memory, &arguments, &raw) ||
          !load_guest_string(memory, static_cast<std::uint32_t>(raw), 4096U,
                             &value)) {
        return false;
      }
      if (spec.precision >= 0 &&
          static_cast<std::size_t>(spec.precision) < value.size()) {
        value.resize(static_cast<std::size_t>(spec.precision));
      }
      output->append(apply_format_width(std::move(value), spec, false));
      continue;
    }
    if (conversion == 'c') {
      std::uint64_t raw{};
      if (!next_guest_argument(memory, &arguments, &raw)) {
        return false;
      }
      std::string value(1U, static_cast<char>(raw & 0xFFU));
      output->append(apply_format_width(std::move(value), spec, false));
      continue;
    }
    if (conversion != 'd' && conversion != 'i' && conversion != 'u' &&
        conversion != 'x' && conversion != 'X' && conversion != 'o' &&
        conversion != 'p') {
      return false;
    }
    std::uint64_t raw{};
    if (!next_guest_argument(memory, &arguments, &raw)) {
      return false;
    }
    const bool signed_conversion = conversion == 'd' || conversion == 'i';
    const bool pointer_conversion = conversion == 'p';
    const bool wide_integer = spec.length == 'l' || spec.length == 'L' ||
                              spec.length == 'z' || pointer_conversion;
    std::ostringstream stream;
    if (pointer_conversion) {
      stream << "0x" << std::hex << static_cast<std::uint32_t>(raw);
    } else if (signed_conversion) {
      const auto value = wide_integer ? static_cast<std::int64_t>(raw)
                                      : static_cast<std::int32_t>(raw);
      if (value >= 0 && spec.plus) {
        stream << '+';
      } else if (value >= 0 && spec.space) {
        stream << ' ';
      }
      stream << value;
    } else {
      const auto value = wide_integer ? raw : static_cast<std::uint32_t>(raw);
      if (conversion == 'x' || conversion == 'X') {
        if (spec.alternate && value != 0U) {
          stream << (conversion == 'X' ? "0X" : "0x");
        }
        if (conversion == 'X') {
          stream << std::uppercase;
        }
        stream << std::hex << value;
      } else if (conversion == 'o') {
        if (spec.alternate && value != 0U) {
          stream << '0';
        }
        stream << std::oct << value;
      } else {
        stream << value;
      }
    }
    auto value = stream.str();
    if (spec.precision > 0 && !pointer_conversion &&
        value.size() < static_cast<std::size_t>(spec.precision)) {
      std::size_t prefix = 0U;
      if (!value.empty() && (value.front() == '-' || value.front() == '+' ||
                             value.front() == ' ')) {
        prefix = 1U;
      } else if (value.starts_with("0x") || value.starts_with("0X")) {
        prefix = 2U;
      }
      value.insert(
          prefix, static_cast<std::size_t>(spec.precision) - value.size(), '0');
    }
    output->append(apply_format_width(std::move(value), spec, true));
  }
  if (count == 0U) {
    return true;
  }
  if (!memory.mapped(buffer, count)) {
    return false;
  }
  const auto writable = std::min<std::size_t>(output->size(), count - 1U);
  for (std::size_t index = 0; index < writable; ++index) {
    memory.store_u8(buffer + static_cast<std::uint32_t>(index),
                    static_cast<std::uint8_t>((*output)[index]));
  }
  memory.store_u8(buffer + static_cast<std::uint32_t>(writable), 0U);
  return true;
}
template <typename Callable>
decltype(auto) guest_memory_access(PPCContext &context, std::uint32_t address,
                                   Callable &&callable) {
  try {
    require_bridge().yield_guest_thread_if_due();
    return std::forward<Callable>(callable)();
  } catch (const ac6demo::RuntimeTrap &error) {
    const std::string diagnostic = std::string(error.what()) +
                                   " r1=" + std::to_string(context.r1.u32) +
                                   " r3=" + std::to_string(context.r3.u32) +
                                   " r4=" + std::to_string(context.r4.u32) +
                                   " r5=" + std::to_string(context.r5.u32) +
                                   " r6=" + std::to_string(context.r6.u32) +
                                   " r7=" + std::to_string(context.r7.u32) +
                                   " r8=" + std::to_string(context.r8.u32) +
                                   " r9=" + std::to_string(context.r9.u32) +
                                   " r10=" + std::to_string(context.r10.u32) +
                                   " r11=" + std::to_string(context.r11.u32) +
                                   " r12=" + std::to_string(context.r12.u32) +
                                   " r13=" + std::to_string(context.r13.u32) +
                                   " r26=" + std::to_string(context.r26.u32) +
                                   " r27=" + std::to_string(context.r27.u32) +
                                   " r28=" + std::to_string(context.r28.u32) +
                                   " r29=" + std::to_string(context.r29.u32) +
                                   " r30=" + std::to_string(context.r30.u32) +
                                   " r31=" + std::to_string(context.r31.u32);
    throw ac6demo::RuntimeTrap(diagnostic, require_bridge().tick(),
                               static_cast<std::uint32_t>(context.lr), address);
  }
}
extern "C" void AC6_PPC_FUNCTION_ENTRY_CONTEXT(
    PPCContext &context, const char *generated_name) noexcept {
  ac6demo::guest_bridge_detail::initialize_post_resume_watch();
  (void)ac6demo::guest_bridge_detail::guest_load_site_watchers_enabled();
  AC6_PPC_SET_POST_RESUME_VECTOR_CONTEXT(context, active_bridge, generated_name, active_bridge == nullptr ? 0U : active_bridge->tick(), current_guest_thread_id);
#ifdef AC6_DEMO_ENABLE_VECTOR_READ_TRACE
  AC6_PPC_VECTOR_CONTEXT(
      context, generated_name,
      active_bridge == nullptr ? 0U : active_bridge->tick(),
      current_guest_thread_id,
      active_bridge == nullptr ? 0U : reinterpret_cast<std::uintptr_t>(
          active_bridge->memory().raw_base()));
#endif
  ac6demo::guest_bridge_detail::trace_xma_slot_function_entry(
      context, generated_name,
      active_bridge == nullptr ? 0U : active_bridge->tick(),
      current_guest_thread_id);
  if (active_bridge != nullptr) {
    ac6demo::guest_bridge_detail::trace_xma_table_entry(
        context, active_bridge->memory(), generated_name, active_bridge->tick(),
        current_guest_thread_id);
  }
  if (active_bridge != nullptr && generated_name != nullptr) {
    active_bridge->record_function_entry(generated_name);
  }
}
extern "C" void AC6_PPC_SET_LOAD_SITE(const char *generated_name,
                                       std::uint32_t generated_line) noexcept {
  const bool enabled =
      ac6demo::guest_bridge_detail::guest_load_site_watchers_enabled() ||
      ac6demo::guest_bridge_detail::post_resume_watch_enabled_fast();
  if (enabled) {
    current_load_generated_name = generated_name;
    current_load_generated_line = generated_line;
  }
}
extern "C" std::uint8_t AC6_PPC_LOAD_U8(PPCContext &context, std::uint8_t *base,
                                        std::uint32_t address) {
  (void)base;
  const auto value = guest_memory_access(
      context, address, [&] { return memory_for(context).load_u8(address); });
  ac6demo::guest_bridge_detail::trace_controller_reader(
      address, 1U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_controller_target_reader(
      address, 1U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::record_post_resume_scalar("load8", address, 1U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), current_load_generated_name, current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_xma_slot_load(
      address, 1U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_transition_load(
      address, 1U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_frontbuffer_read(
      address, 1U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  return value;
}
extern "C" std::uint16_t AC6_PPC_LOAD_U16(PPCContext &context,
                                          std::uint8_t *base,
  std::uint32_t address) {
  (void)base;
  const auto value = guest_memory_access(
      context, address, [&] { return memory_for(context).load_u16(address); });
  ac6demo::guest_bridge_detail::trace_controller_reader(
      address, 2U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_controller_target_reader(
      address, 2U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::record_post_resume_scalar("load16", address, 2U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), current_load_generated_name, current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_xma_slot_load(
      address, 2U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_transition_load(
      address, 2U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_frontbuffer_read(
      address, 2U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  return value;
}
extern "C" std::uint32_t AC6_PPC_LOAD_U32(PPCContext &context,
                                          std::uint8_t *base,
                                          std::uint32_t address) {
  (void)base; ac6demo::guest_bridge_detail::trace_xma_late_access("load32", address, 4U, false, 0U, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), current_load_generated_name, current_load_generated_line);
  if (ac6demo::guest_bridge_detail::graphics_interrupt_state_load_guard(address, current_load_generated_name)) throw ac6demo::RuntimeTrap("graphics interrupt state load outside qualified range", require_bridge().tick(), static_cast<std::uint32_t>(context.lr), address);
  const auto value = guest_memory_access(context, address, [&] { return memory_for(context).load_u32(address); });
  ac6demo::guest_bridge_detail::trace_controller_reader(
      address, 4U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_controller_target_reader(
      address, 4U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::record_post_resume_scalar("load32", address, 4U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), current_load_generated_name, current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_xma_slot_load(
      address, 4U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_transition_load(
      address, 4U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_graphics_interrupt_state_load(address, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), current_load_generated_name, current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_frontbuffer_read(
      address, 4U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  if (std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_CONSUMERS") != nullptr && (events.contains(value) || mutants.contains(value) || semaphores.contains(value) || kernel_semaphores.contains(value) || timers.contains(value) || require_bridge().is_guest_thread_handle(value))) {
    ac6demo::guest_bridge_detail::trace_event_handle_consumer(address, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), current_load_generated_name, current_load_generated_line);
    if (std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_PAYLOAD") != nullptr) {
      const auto snapshot_base = address & ~std::uint32_t{0x1FU};
      std::uint32_t words[8]{};
      std::uint32_t word_mask = 0U;
      auto &memory = memory_for(context);
      for (std::uint32_t index = 0U; index < 8U; ++index) {
        const auto delta = index * 4U;
        if (delta > std::numeric_limits<std::uint32_t>::max() - snapshot_base) {
          continue;
        }
        const auto word_address = snapshot_base + delta;
        if (memory.mapped(word_address, 4U)) {
          words[index] = memory.load_u32(word_address);
          word_mask |= 1U << index;
        }
      }
      ac6demo::guest_bridge_detail::trace_event_handle_payload(address, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), current_load_generated_name, current_load_generated_line, snapshot_base, words, word_mask);
    }
  }
  current_load_generated_name = nullptr;
  current_load_generated_line = 0U;
  return value;
}
extern "C" std::uint64_t AC6_PPC_LOAD_U64(PPCContext &context,
                                          std::uint8_t *base,
                                          std::uint32_t address) {
  (void)base;
  const auto value = guest_memory_access(
      context, address, [&] { return memory_for(context).load_u64(address); });
  ac6demo::guest_bridge_detail::trace_controller_reader(
      address, 8U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_controller_target_reader(
      address, 8U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::record_post_resume_scalar("load64", address, 8U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), current_load_generated_name, current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_xma_slot_load(
      address, 8U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_transition_load(
      address, 8U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_frontbuffer_read(
      address, 8U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  return value;
}
extern "C" void AC6_PPC_STORE_U8(PPCContext &context, std::uint8_t *base,
                                 std::uint32_t address, std::uint8_t value,
                                 const char *generated_name,
                                 std::uint32_t generated_line) {
  (void)base;
  ac6demo::guest_bridge_detail::trace_frontbuffer_write(
      address, 1U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_event_handle_payload_writer(address, 1U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  trace_chunk_target_store(context, address, 1U, value, generated_name,
                           generated_line);
  trace_transition_store(context, address, 1U, value, generated_name,
                         generated_line);
  trace_render_queue_slot_store(context, address, 1U, value != 0U, generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_ib_write(
      address, 1U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  guest_memory_access(context, address,
                      [&] { memory_for(context).store_u8(address, value); });
  ac6demo::guest_bridge_detail::record_post_resume_scalar("store8", address, 1U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_xma_slot_store(
      address, 1U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
}
extern "C" void AC6_PPC_STORE_U16(PPCContext &context, std::uint8_t *base,
                                  std::uint32_t address, std::uint16_t value,
                                  const char *generated_name,
                                  std::uint32_t generated_line) {
  (void)base;
  ac6demo::guest_bridge_detail::trace_frontbuffer_write(
      address, 2U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_event_handle_payload_writer(address, 2U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  trace_chunk_target_store(context, address, 2U, value, generated_name,
                           generated_line);
  trace_transition_store(context, address, 2U, value, generated_name,
                         generated_line);
  trace_render_queue_slot_store(context, address, 2U, value != 0U, generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_ib_write(
      address, 2U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  guest_memory_access(context, address,
                      [&] { memory_for(context).store_u16(address, value); });
  ac6demo::guest_bridge_detail::record_post_resume_scalar("store16", address, 2U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_xma_slot_store(
      address, 2U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
}
extern "C" void AC6_PPC_STORE_U32(PPCContext &context, std::uint8_t *base,
                                  std::uint32_t address, std::uint32_t value,
                                  const char *generated_name,
                                  std::uint32_t generated_line) {
  (void)base; trace_body_store(context, address, value, generated_name, generated_line); ac6demo::guest_bridge_detail::trace_xma_late_access("store32", address, 4U, true, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line); ac6demo::guest_bridge_detail::trace_xma_address_store(address, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_frontbuffer_write(
      address, 4U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_event_handle_payload_writer(address, 4U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  trace_chunk_target_store(context, address, 4U, value, generated_name,
                           generated_line);
  trace_transition_store(context, address, 4U, value, generated_name,
                         generated_line);
  trace_render_queue_slot_store(context, address, 4U, value != 0U, generated_name, generated_line);
  trace_render_queue_writer(context, address, value, generated_name, generated_line);
  if (std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_WRITERS") != nullptr &&
      (events.contains(value) || mutants.contains(value) ||
       semaphores.contains(value) || kernel_semaphores.contains(value) ||
       timers.contains(value) || require_bridge().is_guest_thread_handle(value))) {
    ac6demo::guest_bridge_detail::trace_event_handle_writer(
        address, value, require_bridge().tick(), current_guest_thread_id,
        static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  }
  ac6demo::guest_bridge_detail::trace_ib_write(
      address, 4U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  guest_memory_access(context, address,
                      [&] { memory_for(context).store_u32(address, value); });
  ac6demo::guest_bridge_detail::record_post_resume_scalar("store32", address, 4U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_xma_slot_store(
      address, 4U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  if (address == 0x7FC80714U) {
    require_bridge().apply_xenos_mmio_write(
        address, value, current_guest_thread_id,
        static_cast<std::uint32_t>(context.lr), generated_name,
        generated_line);
  }
}
extern "C" void AC6_PPC_STORE_U64(PPCContext &context, std::uint8_t *base,
                                  std::uint32_t address, std::uint64_t value,
                                  const char *generated_name,
                                  std::uint32_t generated_line) {
  (void)base;
  ac6demo::guest_bridge_detail::trace_frontbuffer_write(
      address, 8U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_event_handle_payload_writer(address, 8U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  trace_chunk_target_store(context, address, 8U, value, generated_name,
                           generated_line);
  trace_transition_store(context, address, 8U,
                         static_cast<std::uint32_t>(value), generated_name,
                         generated_line);
  trace_render_queue_slot_store(context, address, 8U, value != 0U, generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_ib_write(
      address, 8U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  guest_memory_access(context, address,
                      [&] { memory_for(context).store_u64(address, value); });
  ac6demo::guest_bridge_detail::record_post_resume_scalar("store64", address, 8U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_xma_slot_store(
      address, 8U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
}
extern "C" void AC6_PPC_STORE_U128(PPCContext &context, std::uint8_t *base,
                                   std::uint32_t address,
                                   const std::uint8_t *value,
                                   const char *generated_name,
                                   std::uint32_t generated_line) {
  (void)base;
  ac6demo::guest_bridge_detail::trace_frontbuffer_write(
      address, 16U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  if (value == nullptr) {
    throw ac6demo::RuntimeTrap("null Xenon vector store source");
  }
  ac6demo::guest_bridge_detail::trace_event_handle_payload_writer_bytes(address, 16U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  trace_chunk_target_store(
      context, address, 16U,
      std::any_of(value, value + 16U,
                  [](std::uint8_t byte) { return byte != 0U; })
          ? 1U
          : 0U,
      generated_name, generated_line);
  trace_transition_store(
      context, address, 16U,
      std::any_of(value, value + 16U,
                  [](std::uint8_t byte) { return byte != 0U; })
          ? 1U
          : 0U,
      generated_name, generated_line);
  trace_render_queue_slot_store(context, address, 16U, std::any_of(value, value + 16U, [](std::uint8_t byte) { return byte != 0U; }), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_ib_write(
      address, 16U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  std::array<std::byte, 16U> guest_bytes{};
  for (std::size_t index = 0U; index < guest_bytes.size(); ++index) {
    guest_bytes[index] = static_cast<std::byte>(value[15U - index]);
  }
  guest_memory_access(context, address, [&] {
    memory_for(context).store_bytes(address, guest_bytes);
  });
  std::array<std::uint8_t, 16U> guest_bytes_for_trace{};
  std::transform(guest_bytes.begin(), guest_bytes.end(),
                 guest_bytes_for_trace.begin(), [](std::byte byte) {
                   return static_cast<std::uint8_t>(byte);
                 });
  ac6demo::guest_bridge_detail::record_post_resume_bytes("store128", address, 16U, guest_bytes_for_trace.data(), require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
}
extern "C" void AC6_PPC_CALL_INDIRECT(PPCContext &context, std::uint8_t *base,
                                      std::uint32_t guest_address) {
  struct QualifiedVirtualDispatchSite final {
    std::uint32_t lr;
    std::array<std::uint32_t, 6> slots;
    std::size_t slot_count;
  };
  // These are exact return addresses from canonical-Ghidra bctrl callsites,
  // not a generic attempt to identify callers from LR.
  constexpr std::array kQualifiedVirtualDispatchSites{
      QualifiedVirtualDispatchSite{0x821679B0U, {4U}, 1U},
      QualifiedVirtualDispatchSite{0x8218A3ACU, {13U}, 1U},
      QualifiedVirtualDispatchSite{0x8219F014U, {4U}, 1U},
      QualifiedVirtualDispatchSite{0x8219F188U, {4U}, 1U},
      QualifiedVirtualDispatchSite{0x8219F42CU, {4U}, 1U},
      QualifiedVirtualDispatchSite{0x8219F594U, {4U}, 1U},
      QualifiedVirtualDispatchSite{0x821A36A8U, {4U}, 1U},
      QualifiedVirtualDispatchSite{0x821D2804U, {4U}, 1U},
      QualifiedVirtualDispatchSite{0x82259D30U, {4U}, 1U},
      QualifiedVirtualDispatchSite{0x82323F08U, {11U}, 1U},
      QualifiedVirtualDispatchSite{0x82321F30U, {11U}, 1U},
      QualifiedVirtualDispatchSite{0x82321F34U, {78U}, 1U},
      QualifiedVirtualDispatchSite{0x823231B8U, {3U}, 1U},
      QualifiedVirtualDispatchSite{0x820DF8E4U, {6U}, 1U},
      QualifiedVirtualDispatchSite{0x821042B0U, {6U, 7U, 8U, 9U, 10U, 11U}, 6U},
      QualifiedVirtualDispatchSite{0x82259D58U, {10U}, 1U},
      QualifiedVirtualDispatchSite{0x82259D74U, {4U}, 1U},
      QualifiedVirtualDispatchSite{0x82259DA0U, {4U}, 1U},
  };
  const auto lr = static_cast<std::uint32_t>(context.lr);
  trace_dynamic_object_vtable(context, lr, guest_address);
  if (std::getenv("AC6_DEMO_WATCH_INDIRECT_OBJECT") != nullptr && lr == 0x82321F34U) {
    auto &memory = require_bridge().memory();
    const auto object = context.r3.u32;
    std::uint32_t object_word = 0U;
    std::uint32_t slot77 = 0U;
    std::uint32_t slot78 = 0U;
    bool object_mapped = false;
    bool slot77_mapped = false;
    bool slot78_mapped = false;
    if (object != 0U && memory.mapped(object, 4U)) {
      object_mapped = true;
      object_word = memory.load_u32(object);
      const auto vtable = object_word;
      const auto slot_address = [vtable](std::uint32_t slot) {
        const auto offset = static_cast<std::uint64_t>(slot) * 4U;
        return offset <= std::numeric_limits<std::uint32_t>::max() - vtable
                   ? vtable + static_cast<std::uint32_t>(offset)
                   : 0U;
      };
      const auto slot77_address = slot_address(77U);
      const auto slot78_address = slot_address(78U);
      if (slot77_address != 0U && memory.mapped(slot77_address, 4U)) {
        slot77_mapped = true;
        slot77 = memory.load_u32(slot77_address);
      }
      if (slot78_address != 0U && memory.mapped(slot78_address, 4U)) {
        slot78_mapped = true;
        slot78 = memory.load_u32(slot78_address);
      }
    }
    std::fprintf(
        stderr,
        "AC6_INDIRECT_OBJECT lr=0x%08X target=0x%08X tick=%llu thread=%u "
        "object=0x%08X object_mapped=%u vtable=0x%08X slot77_mapped=%u "
        "slot77=0x%08X slot78_mapped=%u slot78=0x%08X r10=0x%08X\n",
        lr, guest_address,
        static_cast<unsigned long long>(require_bridge().tick()),
        current_guest_thread_id, object, object_mapped ? 1U : 0U,
        object_word, slot77_mapped ? 1U : 0U, slot77,
        slot78_mapped ? 1U : 0U, slot78, context.r10.u32);
  }
  if (std::getenv("AC6_DEMO_WATCH_INDIRECT_OBJECT") != nullptr &&
      ((lr == 0x82321F30U && guest_address == 0x820D0D10U) ||
       (lr == 0x823231B8U && guest_address == 0x820DEA08U))) {
    constexpr std::uint32_t kExpectedVtableA = 0x820064D8U;
    constexpr std::uint32_t kExpectedVtableB = 0x82006D8CU;
    const auto candidate_slot = lr == 0x82321F30U ? 11U : 3U;
    const auto expected_vtable =
        lr == 0x82321F30U ? kExpectedVtableA : kExpectedVtableB;
    auto &memory = require_bridge().memory();
    const auto object = context.r3.u32;
    std::uint32_t vtable = 0U;
    std::uint32_t slot_target = 0U;
    bool object_mapped = object != 0U && memory.mapped(object, 4U);
    if (object_mapped) {
      vtable = memory.load_u32(object);
      const auto offset = static_cast<std::uint64_t>(candidate_slot) * 4U;
      if (offset <= std::numeric_limits<std::uint32_t>::max() - vtable) {
        const auto slot_address =
            vtable + static_cast<std::uint32_t>(offset);
        if (memory.mapped(slot_address, 4U)) {
          slot_target = memory.load_u32(slot_address);
        }
      }
    }
    std::fprintf(
        stderr,
        "AC6_INDIRECT_OBJECT_SITE lr=0x%08X target=0x%08X tick=%llu "
        "thread=%u object=0x%08X object_mapped=%u vtable=0x%08X "
        "expected_vtable=0x%08X candidate_slot=%u slot_target=0x%08X "
        "slot_match=%u\n",
        lr, guest_address,
        static_cast<unsigned long long>(require_bridge().tick()),
        current_guest_thread_id, object, object_mapped ? 1U : 0U, vtable,
        expected_vtable, candidate_slot, slot_target,
        (vtable == expected_vtable && slot_target == guest_address) ? 1U
                                                                    : 0U);
  }
  std::optional<ac6demo::GuestVirtualDispatchSnapshot> virtual_dispatch;
  const auto dispatch_site = std::ranges::find_if(
      kQualifiedVirtualDispatchSites,
      [lr](const QualifiedVirtualDispatchSite &site) { return site.lr == lr; });
  if (dispatch_site != kQualifiedVirtualDispatchSites.end()) {
    auto &memory = require_bridge().memory();
    const auto object = context.r3.u32;
    if (object == 0U || !memory.mapped(object, 4U)) {
      throw ac6demo::RuntimeTrap(
          "qualified virtual dispatch object is not mapped",
          require_bridge().tick(), lr, object);
    }
    const auto vtable = memory.load_u32(object);
    const std::span<const std::uint32_t> candidate_slots{
        dispatch_site->slots.data(), dispatch_site->slot_count};
    const auto slot_address = [vtable](std::uint32_t slot) {
      return vtable + slot * static_cast<std::uint32_t>(sizeof(std::uint32_t));
    };
    const auto slots_mapped = std::ranges::all_of(
        candidate_slots, [&memory, &slot_address, vtable](std::uint32_t slot) {
          const auto address = slot_address(slot);
          return address >= vtable && memory.mapped(address, 4U);
        });
    const auto target_matches =
        slots_mapped
            ? std::ranges::count_if(
                  candidate_slots,
                  [&memory, &slot_address, guest_address](std::uint32_t slot) {
                    return memory.load_u32(slot_address(slot)) == guest_address;
                  })
            : 0;
    if (vtable == 0U || !slots_mapped || target_matches != 1) {
      throw ac6demo::RuntimeTrap("qualified virtual dispatch slot mismatch",
                                 require_bridge().tick(), lr, vtable);
    }
    const auto matching_slot = *std::ranges::find_if(
        candidate_slots,
        [&memory, &slot_address, guest_address](std::uint32_t slot) {
          return memory.load_u32(slot_address(slot)) == guest_address;
        });
    virtual_dispatch =
        ac6demo::GuestVirtualDispatchSnapshot{object, vtable, matching_slot};
  }
  auto &call = indirect_calls[current_guest_thread_id];
  call.target = guest_address;
  call.lr = lr;
  ++call.count;
  call.tick = require_bridge().tick();
  require_bridge().record_indirect_edge(
      current_guest_thread_id, lr, guest_address, snapshot_registers(context),
      virtual_dispatch);
  if (ac6demo::guest_bridge_detail::dispatch_reached_branch_delay_thunk(
      context, base, guest_address, require_bridge().memory(),
      require_bridge().tick(), lr,
          [](std::uint32_t target) { return lookup_guest_function(target); },
          [](PPCContext &thunk_context, std::uint8_t *thunk_base,
             std::uint32_t target) {
           AC6_PPC_CALL_INDIRECT(thunk_context, thunk_base, target);
         })) {
    return;
  }
  if (ac6demo::guest_bridge_detail::dispatch_reached_chunk_entry(
          context, base, guest_address, require_bridge().memory(),
          require_bridge().tick(), lr,
          [](std::uint32_t target) { return lookup_guest_function(target); },
          [](PPCContext &target_context, std::uint8_t *target_base,
             std::uint32_t target) {
            const auto function = lookup_guest_function(target);
            if (function == nullptr) {
              throw ac6demo::RuntimeTrap(
                  "qualified chunk entry branch target disappeared",
                  require_bridge().tick(),
                  static_cast<std::uint32_t>(target_context.lr), target);
            }
            const auto previous = chunk_target_store_trace_active;
            chunk_target_store_trace_active = true;
            try {
              function(target_context, target_base);
            } catch (...) {
              chunk_target_store_trace_active = previous;
              throw;
            }
            chunk_target_store_trace_active = previous;
            if (std::getenv("AC6_DEMO_WATCH_CHUNK_RETURN") != nullptr) {
              std::fprintf(
                  stderr,
                  "AC6_CHUNK_RETURN target=0x%08X tick=%llu thread=%u "
                  "r1=0x%08X r3=0x%08X r4=0x%08X r5=0x%08X "
                  "r21=0x%08X r26=0x%08X r27=0x%08X r28=0x%08X "
                  "r29=0x%08X r30=0x%08X r31=0x%08X lr=0x%08X\n",
                  target,
                  static_cast<unsigned long long>(require_bridge().tick()),
                  current_guest_thread_id, target_context.r1.u32,
                  target_context.r3.u32, target_context.r4.u32,
                  target_context.r5.u32, target_context.r21.u32,
                  target_context.r26.u32, target_context.r27.u32,
                  target_context.r28.u32, target_context.r29.u32,
                  target_context.r30.u32, target_context.r31.u32,
                  static_cast<std::uint32_t>(target_context.lr));
            }
          })) {
    return;
  }
  if (const auto function = lookup_guest_function(guest_address);
      function != nullptr) {
    invoke_body_trace(function, context, base, guest_address);
    return;
  }
  throw ac6demo::RuntimeTrap("unqualified guest indirect call",
                             require_bridge().tick(), lr, guest_address);
}
[[nodiscard]] std::int16_t saturate_s16(std::int32_t value) noexcept {
  if (value < std::numeric_limits<std::int16_t>::min()) {
    return std::numeric_limits<std::int16_t>::min();
  }
  if (value > std::numeric_limits<std::int16_t>::max()) {
    return std::numeric_limits<std::int16_t>::max();
  }
  return static_cast<std::int16_t>(value);
}
struct CivilDate final {
  std::int32_t year{};
  std::uint16_t month{};
  std::uint16_t day{};
};
[[nodiscard]] CivilDate civil_from_unix_days(std::int64_t days) noexcept {
  days += 719468;
  const auto era = (days >= 0 ? days : days - 146096) / 146097;
  const auto day_of_era = static_cast<std::uint32_t>(days - era * 146097);
  const auto year_of_era = (day_of_era - day_of_era / 1460U +
                            day_of_era / 36524U - day_of_era / 146096U) /
                           365U;
  auto year = static_cast<std::int32_t>(year_of_era) +
              static_cast<std::int32_t>(era) * 400;
  const auto day_of_year =
      day_of_era - (365U * year_of_era + year_of_era / 4U - year_of_era / 100U);
  const auto month_part = (5U * day_of_year + 2U) / 153U;
  const auto day = day_of_year - (153U * month_part + 2U) / 5U + 1U;
  const auto month =
      static_cast<std::int32_t>(month_part) + (month_part < 10U ? 3 : -9);
  year += month <= 2 ? 1 : 0;
  return CivilDate{year, static_cast<std::uint16_t>(month),
                   static_cast<std::uint16_t>(day)};
}
[[nodiscard]] bool checked_page_range(std::uint32_t address, std::uint32_t size,
                                      std::uint32_t *aligned_address,
                                      std::size_t *aligned_size) noexcept {
  if (size == 0U) {
    return false;
  }
  const auto begin = address & ~(ac6demo::kGuestPageBytes - 1U);
  const auto end = static_cast<std::uint64_t>(address) + size;
  const auto aligned_end =
      (end + ac6demo::kGuestPageBytes - 1U) &
      ~(static_cast<std::uint64_t>(ac6demo::kGuestPageBytes) - 1U);
  if (aligned_end > ac6demo::kGuestMemoryBytes || aligned_end <= begin) {
    return false;
  }
  *aligned_address = begin;
  *aligned_size = static_cast<std::size_t>(aligned_end - begin);
  return true;
}
[[nodiscard]] bool guest_range_is_owned_or_free(GuestBridge &bridge,
                                                std::uint32_t address,
                                                std::size_t size) noexcept {
  auto &memory = bridge.memory();
  const auto page_count = size / ac6demo::kGuestPageBytes;
  for (std::size_t page = 0U; page < page_count; ++page) {
    const auto page_address =
        address + static_cast<std::uint32_t>(page * ac6demo::kGuestPageBytes);
    if (memory.mapped(page_address, ac6demo::kGuestPageBytes) &&
        !bridge.owns_allocation(page_address, ac6demo::kGuestPageBytes)) {
      return false;
    }
  }
  return true;
}
[[nodiscard]] bool dispatch_allocate(PPCContext &context) {
  auto &bridge = require_bridge();
  auto &memory = bridge.memory();
  const auto base_pointer = context.r3.u32;
  const auto size_pointer = context.r4.u32;
  if (!memory.mapped(base_pointer, 4U) || !memory.mapped(size_pointer, 4U)) {
    return false;
  }
  const auto requested_base = memory.load_u32(base_pointer);
  const auto requested_size = memory.load_u32(size_pointer);
  if (requested_size == 0U) {
    return false;
  }
  // NtAllocateVirtualMemory rounds the supplied region to guest pages and
  // writes that adjusted size back to the caller.  The title immediately
  // uses this value to advance its Xenon heap; returning the byte-exact input
  // leaves a partial last page in the heap's free-list arithmetic.
  std::uint32_t allocation = requested_base;
  if (allocation == 0U) {
    const auto rounded_size =
        (static_cast<std::uint64_t>(requested_size) + ac6demo::kGuestPageBytes -
         1U) &
        ~(static_cast<std::uint64_t>(ac6demo::kGuestPageBytes) - 1U);
    if (rounded_size > std::numeric_limits<std::uint32_t>::max()) {
      return false;
    }
    allocation =
        bridge.allocate_address(static_cast<std::uint32_t>(rounded_size));
    if (allocation == 0U) {
      return false;
    }
  }
  std::uint32_t mapped_address{};
  std::size_t mapped_size{};
  if (!checked_page_range(allocation, requested_size, &mapped_address,
                          &mapped_size)) {
    return false;
  }
  // A fixed allocation may extend an existing title heap by one already
  // committed page.  Accept only pages owned by this bridge; an overlap with
  // the image, MMIO, or any other unqualified mapping remains fail-closed.
  if (!guest_range_is_owned_or_free(bridge, mapped_address, mapped_size)) {
    return false;
  }
  memory.map_zero(mapped_address, mapped_size);
  bridge.record_allocation(mapped_address, mapped_size);
  memory.store_u32(base_pointer, mapped_address);
  memory.store_u32(size_pointer, static_cast<std::uint32_t>(mapped_size));
  context.r3.s64 = 0;
  return true;
}
[[nodiscard]] std::uint64_t
decode_wait_deadline(GuestBridge &bridge, std::uint32_t timeout_pointer) {
  if (timeout_pointer == 0U) {
    return kNoWakeTick;
  }
  auto &memory = bridge.memory();
  if (!memory.mapped(timeout_pointer, 8U)) {
    throw ac6demo::RuntimeTrap("guest wait timeout is not mapped",
                               bridge.tick(), 0, timeout_pointer);
  }
  const auto timeout =
      static_cast<std::int64_t>(memory.load_u64(timeout_pointer));
  if (timeout > 0) {
    throw ac6demo::RuntimeTrap("unqualified absolute guest wait timeout",
                               bridge.tick(), 0, timeout_pointer);
  }
  const auto magnitude = timeout == std::numeric_limits<std::int64_t>::min()
                             ? std::numeric_limits<std::uint64_t>::max()
                             : static_cast<std::uint64_t>(-timeout);
  if (magnitude > kNoWakeTick - (kHundredNanosecondsPerGuestTick - 1U)) {
    return kNoWakeTick;
  }
  const auto ticks = std::max<std::uint64_t>(
      1U, (magnitude + kHundredNanosecondsPerGuestTick - 1U) /
              kHundredNanosecondsPerGuestTick);
  if (bridge.tick() > kNoWakeTick - ticks) {
    return kNoWakeTick;
  }
  return bridge.tick() + ticks;
}
void update_guest_timers(GuestBridge &bridge) noexcept {
  for (auto &[handle, timer] : timers) {
    if (!timer.active || timer.due_tick == kNoWakeTick ||
        bridge.tick() < timer.due_tick) {
      continue;
    }
    timer.signaled = true;
    if (timer.period_ticks == 0U) {
      timer.active = false;
      timer.due_tick = kNoWakeTick;
    } else {
      const auto elapsed = bridge.tick() - timer.due_tick;
      const auto periods = elapsed / timer.period_ticks + 1U;
      if (periods > (kNoWakeTick - timer.due_tick) / timer.period_ticks) {
        timer.due_tick = kNoWakeTick;
        timer.active = false;
      } else {
        timer.due_tick += periods * timer.period_ticks;
      }
    }
    bridge.wake_guest_waiters(kWaitTimer, handle);
  }
}
[[nodiscard]] std::uint64_t decode_timer_due_tick(GuestBridge &bridge,
                                                  std::uint32_t pointer) {
  auto &memory = bridge.memory();
  if (pointer == 0U || !memory.mapped(pointer, 8U)) {
    throw ac6demo::RuntimeTrap("guest timer due-time is not mapped",
                               bridge.tick(), 0, pointer);
  }
  const auto due_time = static_cast<std::int64_t>(memory.load_u64(pointer));
  if (due_time >= 0) {
    throw ac6demo::RuntimeTrap("unqualified absolute guest timer deadline",
                               bridge.tick(), 0, pointer);
  }
  const auto magnitude = due_time == std::numeric_limits<std::int64_t>::min()
                             ? std::numeric_limits<std::uint64_t>::max()
                             : static_cast<std::uint64_t>(-due_time);
  if (magnitude > kNoWakeTick - (kHundredNanosecondsPerGuestTick - 1U)) {
    return kNoWakeTick;
  }
  const auto ticks = std::max<std::uint64_t>(
      1U, (magnitude + kHundredNanosecondsPerGuestTick - 1U) /
              kHundredNanosecondsPerGuestTick);
  if (bridge.tick() > kNoWakeTick - ticks) {
    return kNoWakeTick;
  }
  return bridge.tick() + ticks;
}
[[nodiscard]] std::uint64_t
timer_period_to_ticks(std::uint32_t period_ms) noexcept {
  if (period_ms == 0U) {
    return 0U;
  }
  const auto duration = static_cast<std::uint64_t>(period_ms) * 1'000'000U;
  return std::max<std::uint64_t>(
      1U, (duration + kHundredNanosecondsPerGuestTick - 1U) /
              kHundredNanosecondsPerGuestTick);
}
[[nodiscard]] bool dispatch_import(PPCContext &context, const char *module,
                                   const char *name, std::uint16_t ordinal) {
  require_bridge().record_import_edge(
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr), module,
      name, ordinal, snapshot_registers(context));
  ac6demo::guest_bridge_detail::trace_xma_create_import(
      context, require_bridge().memory(), module, name, ordinal,
      require_bridge().tick(), current_guest_thread_id);
#include "guest_bridge/audio_memory_dispatch.hpp"
#include "guest_bridge/graphics_dispatch.hpp"
#include "guest_bridge/kernel_objects_dispatch.hpp"
#include "guest_bridge/kernel_runtime_dispatch.hpp"
#include "guest_bridge/vfs_dispatch.hpp"
#include "guest_bridge/xam_bootstrap_dispatch.hpp"
#include "guest_bridge/xam_input_dispatch.hpp"
  return false;
}
} // namespace
// clang-format off
#include "guest_bridge/constructor.hpp"
#include "guest_bridge/scheduler.hpp"
#include "guest_bridge/point_draw_trace.hpp"
#include "guest_bridge/graphics_ring.hpp"
#include "guest_bridge/graphics_mmio_cpu.hpp"
#include "guest_bridge/lifecycle.hpp"
// clang-format on
#else
namespace ac6demo {
bool generated_guest_available() noexcept { return false; }
GuestBridge::GuestBridge(GuestMemory &memory) : memory_(memory) {}
GuestBridge::~GuestBridge() = default;
bool GuestBridge::available() const noexcept { return false; }
void GuestBridge::prepare(const ThreadImage &) {}
void GuestBridge::set_tick(std::uint64_t tick) noexcept {
  tick_ = tick;
  input_.set_tick(tick);
}
void GuestBridge::run_entry(std::uint32_t) {
  throw RuntimeTrap("generated guest is not linked in this build", tick_);
}
bool GuestBridge::block_current_guest_thread(std::uint8_t, std::uint32_t,
                                             std::uint64_t) {
  throw RuntimeTrap("guest fiber scheduler is not linked in this build", tick_);
}
bool GuestBridge::wake_guest_waiters(std::uint8_t, std::uint32_t) noexcept {
  return false;
}
void GuestBridge::yield_current_guest_thread() {}
GuestSchedulerSnapshot GuestBridge::scheduler_snapshot() const noexcept {
  return {};
}
#include "guest_bridge/point_draw_trace.hpp"
#include "guest_bridge/graphics_ring.hpp"
} // namespace ac6demo

#endif
