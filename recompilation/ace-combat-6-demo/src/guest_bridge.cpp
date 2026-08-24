#include "ac6demo/guest_bridge.hpp"
#include "ac6demo/content.hpp"
#include "ac6demo/graphics.hpp"
#include "ac6demo/hash.hpp"
#include "ac6demo/ppc.hpp"
#include "ac6demo/xaudio_callback_cpu_contract.hpp"
#include "ac6demo/xenon_affinity_contract.hpp"
#include <algorithm>
#include <array>
#include <atomic>
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
#include <numeric>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "guest_bridge/transition_memory_trace.hpp"
#include "guest_bridge/graphics_interrupt_trace.hpp"
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

std::optional<std::size_t>
qualified_title_vertex_snapshot_size(std::uint32_t base_index,
                                     std::uint32_t index_count) noexcept {
  constexpr std::size_t kVertexStride = 13U * sizeof(std::uint32_t);
  if (index_count != 4U ||
      (base_index != 0U && base_index != 4U && base_index != 8U)) {
    return std::nullopt;
  }
  return (base_index + index_count) * kVertexStride;
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
#include "guest_bridge/notification_state.hpp"
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
// XNotifyCreateListener: the saved system notifications are delivered to the
// first eligible listener only. "If two listeners with overlapping categories
// are created, only one will receive the saved notifications."
thread_local bool saved_notifications_delivered = false;
thread_local std::uint32_t next_timer_handle = 0xE6000000U;
thread_local std::uint32_t current_guest_thread_id = 1U;
thread_local std::uint32_t current_import_lr = 0U;
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
#include "guest_bridge/loading_resource_poll_trace.hpp"
#include "guest_bridge/swg_native_call_trace.hpp"
#include "guest_bridge/title_terminal_trace.hpp"
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

std::atomic<std::uint64_t> title_writer_trace_order{};

[[nodiscard]] bool title_writer_trace_enabled() noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_TITLE_WRITER") != nullptr;
  return enabled;
}

[[nodiscard]] std::uint64_t next_title_writer_trace_order() noexcept {
  return title_writer_trace_order.fetch_add(1U, std::memory_order_relaxed) + 1U;
}

void trace_title_writer(const PPCContext &context,
                        const char *generated_name) noexcept {
  if (!title_writer_trace_enabled() || generated_name == nullptr ||
      active_bridge == nullptr) {
    return;
  }
  const auto tick = active_bridge->tick();
  if (tick < 200U || tick > 235U) {
    return;
  }
  constexpr std::array<std::string_view, 9U> targets{
      "821A7160", "821A4808", "82119488", "82118FA0", "821185A8",
      "82118D18", "82119048", "82118A28", "821B86F8"};
  const auto target = std::find_if(
      targets.begin(), targets.end(), [&](std::string_view address) {
        return std::string_view(generated_name).find(address) !=
               std::string_view::npos;
      });
  if (target == targets.end()) {
    return;
  }
  static std::array<std::uint32_t, targets.size()> counts{};
  const auto target_index = static_cast<std::size_t>(target - targets.begin());
  if (counts[target_index]++ >= 256U) {
    return;
  }

  auto &memory = active_bridge->memory();
  const auto read_u32 = [&](std::uint32_t address) noexcept {
    return address != 0U && memory.mapped(address, 4U)
               ? memory.load_u32(address)
               : 0U;
  };
  const auto read_field = [&](std::uint32_t base,
                              std::uint32_t offset) noexcept {
    return base <= std::numeric_limits<std::uint32_t>::max() - offset
               ? read_u32(base + offset)
               : 0U;
  };
  const auto selector = read_u32(0x823C252CU);
  const auto row = selector < 3U ? 0x826F61C8U + 108U * selector : 0U;
  const auto p = read_field(row, 0x20U);
  const auto q = read_field(row, 0x44U);
  const auto r = read_field(row, 0x68U);
  const auto cp = read_u32(0x826F61C0U);
  const auto cq = read_u32(0x826F61BCU);
  const auto cr = read_u32(0x826F630CU);
  const auto record = *target == "82118FA0" ? context.r4.u32
                      : (*target == "821185A8" ? context.r5.u32
                      : ((*target == "82118D18" || *target == "82119048")
                             ? context.r4.u32
                             : 0U));
  const auto dst_p = static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(p) + 52U * cp);
  const auto dst_q = static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(q) + 20U * cq);
  const auto dst_r = static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(r) + 4U * cr);
  std::fprintf(
      stderr,
      "AC6_TITLE_WRITER order=%llu tick=%llu thread=%u function=%s "
      "lr=0x%08X r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X "
      "r7=0x%08X r8=0x%08X selector=%u P=0x%08X Q=0x%08X R=0x%08X "
      "cP=%u cQ=%u cR=%u dstP=0x%08X dstQ=0x%08X dstR=0x%08X "
      "record=0x%08X record_0=0x%08X record_4=0x%08X "
      "record_8=0x%08X record_C=0x%08X record_14=0x%08X "
      "record_18=0x%08X record_20=0x%08X record_24=0x%08X\n",
      static_cast<unsigned long long>(next_title_writer_trace_order()),
      static_cast<unsigned long long>(tick), current_guest_thread_id,
      generated_name, static_cast<std::uint32_t>(context.lr), context.r3.u32,
      context.r4.u32, context.r5.u32, context.r6.u32, context.r7.u32,
      context.r8.u32, selector, p, q, r, cp, cq, cr, dst_p, dst_q, dst_r,
      record, read_field(record, 0U), read_field(record, 4U),
      read_field(record, 8U), read_field(record, 0x0CU),
      read_field(record, 0x14U), read_field(record, 0x18U),
      read_field(record, 0x20U), read_field(record, 0x24U));
}

void trace_title_writer_store(const PPCContext &context,
                              std::uint32_t address, std::uint32_t size,
                              std::uint32_t value,
                              const char *generated_name,
                              std::uint32_t generated_line) noexcept {
  if (!title_writer_trace_enabled() || active_bridge == nullptr) {
    return;
  }
  const auto tick = active_bridge->tick();
  if (tick < 200U || tick > 235U) {
    return;
  }
  // Keep this probe bounded to the three qualified title vertex pairs and the
  // selector/owner words that choose them.  The exact translated shader
  // consumes P (fetch slot 95); keep adjacent Q observable as a coherency
  // guard without treating it as the shader source.
  constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 7U> ranges{{
      {0x103FB890U, 0x103FBA30U}, {0x103F1000U, 0x103F11A0U},
      {0x104A4890U, 0x104A4960U}, {0x10558120U, 0x105581F0U},
      {0x823C252CU, 0x823C2530U}, {0x826F61BCU, 0x826F61C4U},
      {0x826F630CU, 0x826F6310U}}};
  const auto end = static_cast<std::uint64_t>(address) + size;
  const bool intersects = std::ranges::any_of(ranges, [&](const auto &range) {
    return address < range.second && end > range.first;
  });
  if (!intersects) {
    return;
  }
  auto &memory = active_bridge->memory();
  const auto aligned = address & ~3U;
  const auto old_value = memory.mapped(aligned, 4U)
                             ? memory.load_u32(aligned)
                             : 0U;
  std::fprintf(
      stderr,
      "AC6_TITLE_WRITER_STORE order=%llu tick=%llu thread=%u "
      "address=0x%08X size=%u old=0x%08X value=0x%08X lr=0x%08X "
      "function=%s generated_line=%u\n",
      static_cast<unsigned long long>(next_title_writer_trace_order()),
      static_cast<unsigned long long>(tick), current_guest_thread_id, address,
      size, old_value, value, static_cast<std::uint32_t>(context.lr),
      generated_name == nullptr ? "" : generated_name, generated_line);
}

void trace_title_writer_draw_boundary(std::uint64_t tick,
                                      std::uint32_t thread) noexcept {
  if (!title_writer_trace_enabled() || tick < 200U || tick > 235U) {
    return;
  }
  std::fprintf(stderr,
               "AC6_TITLE_WRITER_DRAW order=%llu tick=%llu thread=%u "
               "address=0x103FB890 size=208\n",
               static_cast<unsigned long long>(next_title_writer_trace_order()),
               static_cast<unsigned long long>(tick), thread);
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
void trace_render_queue_slot_store(PPCContext &c, std::uint32_t a, std::uint32_t s, std::uint64_t v, const char *n, std::uint32_t l) {
  ac6demo::guest_bridge_detail::trace_render_queue_slot_store(a, s, v, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(c.lr), n, l);
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

void trace_mode_state_store(const PPCContext &context, std::uint32_t address,
                            std::uint32_t value,
                            const char *generated_name,
                            std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_MODE_STATE") != nullptr &&
      std::getenv("AC6_DEMO_WATCH_TICK_WINDOW") != nullptr;
  if (!enabled || active_bridge == nullptr ||
      !ac6demo::guest_bridge_detail::transition_trace_tick_allowed(
          active_bridge->tick())) {
    return;
  }
  auto &memory = active_bridge->memory();
  if (!memory.mapped(0x827435F8U, 4U)) {
    return;
  }
  const auto manager = memory.load_u32(0x827435F8U);
  if (manager == 0U || !memory.mapped(manager, 0x1CU)) {
    return;
  }
  const auto task = memory.load_u32(manager + 0x08U);
  const std::array fields{
      std::pair{manager + 0x08U, "manager_08"},
      std::pair{manager + 0x0CU, "manager_0C"},
      std::pair{manager + 0x10U, "manager_10"},
      std::pair{manager + 0x14U, "manager_14"},
      std::pair{manager + 0x18U, "manager_18"},
      std::pair{task == 0U ? 0U : task + 0x0CU, "task_0C"},
      std::pair{task == 0U ? 0U : task + 0x44U, "task_44"},
      std::pair{task == 0U ? 0U : task + 0x70U, "task_70"},
  };
  const auto field = std::ranges::find_if(fields, [&](const auto &candidate) {
    return candidate.first != 0U && candidate.first == address;
  });
  if (field == fields.end() || !memory.mapped(address, 4U)) {
    return;
  }
  std::fprintf(
      stderr,
      "AC6_MODE_FIELD_STORE tick=%llu thread=%u field=%s "
      "address=0x%08X old=0x%08X new=0x%08X lr=0x%08X "
      "function=%s generated_line=%u manager=0x%08X task=0x%08X\n",
      static_cast<unsigned long long>(active_bridge->tick()),
      current_guest_thread_id, field->second, address,
      memory.load_u32(address), value,
      static_cast<std::uint32_t>(context.lr),
      generated_name == nullptr ? "" : generated_name, generated_line,
      manager, task);
}
#include "guest_bridge/guest_format.hpp"
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

static void trace_record_type_route(const PPCContext &context,
                                    const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_RECORD_TYPE_ROUTE") != nullptr;
  static const bool early_enabled =
      std::getenv("AC6_DEMO_WATCH_RECORD_TYPE_ROUTE_EARLY") != nullptr;
  static const bool avi_enabled =
      std::getenv("AC6_DEMO_WATCH_AVI_RECEIVER") != nullptr;
  if ((!enabled && !early_enabled && !avi_enabled) || generated_name == nullptr ||
      active_bridge == nullptr) {
    return;
  }
  const auto tick = active_bridge->tick();
  const auto window_begin = early_enabled ? 190U : 2990U;
  const auto window_end = early_enabled ? 430U : 3040U;
  if (tick < window_begin || tick > window_end) {
    return;
  }
  const auto is_target = [&](const char *address) noexcept {
    return std::strstr(generated_name, address) != nullptr;
  };
  const bool is_dispatch = is_target("8210A1C0");
  const bool is_record_builder = is_target("82117410");
  const bool is_record_consumer = is_target("820FEFA8");
  const bool is_queue_worker = is_target("820FFCA0");
  const bool is_avi_target = is_target("82165CC0");
  const bool is_avi_child = is_dispatch && avi_enabled;
  const bool trace_route_target =
      (enabled || early_enabled) &&
      (is_dispatch || is_record_builder || is_record_consumer ||
                  is_queue_worker);
  const bool trace_avi_target =
      avi_enabled && (is_avi_target || is_avi_child || is_record_builder);
  if (!trace_route_target && !trace_avi_target) {
    return;
  }
  std::uint32_t record_type = 0U;
  if (is_record_consumer &&
      require_bridge().memory().mapped(context.r3.u32 + 64U, 4U)) {
    record_type = require_bridge().memory().load_u32(context.r3.u32 + 64U);
  }
  std::uint32_t record_10c = 0U;
  if (is_record_consumer &&
      require_bridge().memory().mapped(context.r3.u32 + 0x10CU, 4U)) {
    record_10c = require_bridge().memory().load_u32(context.r3.u32 + 0x10CU);
  }
  std::fprintf(
      stderr,
      "AC6_RECORD_TYPE_ROUTE tick=%llu function=%s lr=0x%08X "
      "r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X r7=0x%08X r8=0x%08X "
      "selector=0x%04X record_type=0x%08X record_10c=0x%08X "
      "avi_target=%u avi_child=%u\n",
      static_cast<unsigned long long>(tick), generated_name,
      static_cast<std::uint32_t>(context.lr), context.r3.u32, context.r4.u32,
      context.r5.u32, context.r6.u32, context.r7.u32, context.r8.u32,
      (is_dispatch || is_avi_target) ? (context.r6.u32 & 0xFFFFU) : 0U,
      record_type, record_10c, is_avi_target ? 1U : 0U,
      is_avi_child ? 1U : 0U);
}

static void trace_provider_population(const PPCContext &context,
                                      const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_PROVIDER_POPULATION") != nullptr;
  static std::atomic_uint32_t record_count{0U};
  if (!enabled || generated_name == nullptr || active_bridge == nullptr) {
    return;
  }
  const auto tick = active_bridge->tick();
  if (tick < 190U || tick > 430U) {
    return;
  }
  constexpr std::array<std::string_view, 8U> targets{
      "82114350", "82114798", "82114A58", "82115018",
      "82165490", "82165AF8", "82165CC0", "82165E68"};
  const auto target = std::find_if(
      targets.begin(), targets.end(), [&](std::string_view address) {
        return std::string_view(generated_name).find(address) !=
               std::string_view::npos;
      });
  if (target == targets.end() ||
      record_count.fetch_add(1U, std::memory_order_relaxed) >= 128U) {
    return;
  }
  auto &memory = active_bridge->memory();
  const auto load = [&](std::uint32_t address,
                        std::uint32_t offset) noexcept -> std::uint32_t {
    return address != 0U && memory.mapped(address + offset, 4U)
               ? memory.load_u32(address + offset)
               : 0U;
  };
  std::fprintf(
      stderr,
      "AC6_PROVIDER_POPULATION tick=%llu function=%s lr=0x%08X "
      "r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X r7=0x%08X r8=0x%08X "
      "r20=0x%08X r3_624=0x%08X r3_628=0x%08X r3_3A8=0x%08X "
      "r3_3AC=0x%08X r20_624=0x%08X r20_628=0x%08X r20_3A8=0x%08X "
      "r20_3AC=0x%08X r4_12=0x%08X r4_16=0x%08X\n",
      static_cast<unsigned long long>(tick), generated_name,
      static_cast<std::uint32_t>(context.lr), context.r3.u32, context.r4.u32,
      context.r5.u32, context.r6.u32, context.r7.u32, context.r8.u32,
      context.r20.u32, load(context.r3.u32, 0x624U),
      load(context.r3.u32, 0x628U), load(context.r3.u32, 0x3A8U),
      load(context.r3.u32, 0x3ACU), load(context.r20.u32, 0x624U),
      load(context.r20.u32, 0x628U), load(context.r20.u32, 0x3A8U),
      load(context.r20.u32, 0x3ACU), load(context.r4.u32, 0x12U),
      load(context.r4.u32, 0x16U));
}

static void trace_swg_context(const PPCContext &context,
                              const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_SWG_CONTEXT") != nullptr;
  if (!enabled || generated_name == nullptr || active_bridge == nullptr) {
    return;
  }
  const auto is_target = [&](const char *address) noexcept {
    return std::strstr(generated_name, address) != nullptr;
  };
  if (!is_target("820D29E0") && !is_target("82324188") &&
      !is_target("82165CC0") && !is_target("8210A1C0") &&
      !is_target("82117410")) {
    return;
  }

  auto &memory = active_bridge->memory();
  const auto owner = context.r4.u32;
  std::uint32_t owner_vtable = 0U;
  std::uint32_t context_object = 0U;
  std::uint32_t context_vtable = 0U;
  std::uint32_t slot4 = 0U;
  if (owner != 0U && memory.mapped(owner, 4U)) {
    owner_vtable = memory.load_u32(owner);
  }
  if (owner != 0U && memory.mapped(owner + 0xE8U, 4U)) {
    context_object = memory.load_u32(owner + 0xE8U);
  }
  if (context_object != 0U && memory.mapped(context_object, 4U)) {
    context_vtable = memory.load_u32(context_object);
  }
  if (context_vtable != 0U && memory.mapped(context_vtable + 4U, 4U)) {
    slot4 = memory.load_u32(context_vtable + 4U);
  }
  std::fprintf(
      stderr,
      "AC6_SWG_CONTEXT tick=%llu function=%s lr=0x%08X thread=%u "
      "r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X r7=0x%08X "
      "r8=0x%08X owner_vtable=0x%08X context=0x%08X "
      "context_vtable=0x%08X slot4=0x%08X\n",
      static_cast<unsigned long long>(active_bridge->tick()), generated_name,
      static_cast<std::uint32_t>(context.lr), current_guest_thread_id,
      context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32,
      context.r7.u32, context.r8.u32, owner_vtable, context_object,
      context_vtable, slot4);
}

static void trace_swg_slot4(const PPCContext &context,
                            const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_SWG_SLOT4") != nullptr;
  if (!enabled || generated_name == nullptr || active_bridge == nullptr) {
    return;
  }
  const auto tick = active_bridge->tick();
  if (tick < 190U || tick > 430U) {
    return;
  }
  constexpr std::array<std::string_view, 5U> targets{
      "820D0DB8", "820D18C8", "823233B0", "82323468", "82323808"};
  const auto target = std::find_if(
      targets.begin(), targets.end(), [&](std::string_view address) {
        return std::string_view(generated_name).find(address) !=
               std::string_view::npos;
      });
  if (target == targets.end()) {
    return;
  }
  auto &memory = active_bridge->memory();
  const auto read_field = [&](std::uint32_t base,
                              std::uint32_t offset) noexcept -> std::uint32_t {
    if (base == 0U || base > std::numeric_limits<std::uint32_t>::max() - offset ||
        !memory.mapped(base + offset, 4U)) {
      return 0U;
    }
    return memory.load_u32(base + offset);
  };
  const auto object = context.r3.u32;
  const auto param = context.r5.u32;
  std::fprintf(
      stderr,
      "AC6_SWG_SLOT4 tick=%llu function=%s lr=0x%08X thread=%u "
      "r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X r7=0x%08X r8=0x%08X "
      "obj0=0x%08X obj10=0x%08X obj14=0x%08X obj20=0x%08X "
      "param0=0x%08X param4=0x%08X param8=0x%08X paramc=0x%08X\n",
      static_cast<unsigned long long>(tick), generated_name,
      static_cast<std::uint32_t>(context.lr), current_guest_thread_id,
      context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32,
      context.r7.u32, context.r8.u32, read_field(object, 0U),
      read_field(object, 0x10U), read_field(object, 0x14U),
      read_field(object, 0x20U), read_field(param, 0U), read_field(param, 4U),
      read_field(param, 8U), read_field(param, 0x0CU));
}

static void trace_swg_blob(const PPCContext &context,
                           const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_SWG_BLOB") != nullptr;
  if (!enabled || generated_name == nullptr || active_bridge == nullptr) {
    return;
  }
  const auto tick = active_bridge->tick();
  if (tick < 190U || tick > 430U) {
    return;
  }
  constexpr std::array<std::string_view, 3U> targets{
      "82326B80", "82326420", "823233B0"};
  const auto target = std::find_if(
      targets.begin(), targets.end(), [&](std::string_view address) {
        return std::string_view(generated_name).find(address) !=
               std::string_view::npos;
      });
  if (target == targets.end()) {
    return;
  }
  static std::uint32_t event_count = 0U;
  if (event_count++ >= 256U) {
    return;
  }

  auto &memory = active_bridge->memory();
  const auto read_u32 = [&](std::uint32_t address) noexcept {
    return address != 0U && memory.mapped(address, 4U)
               ? memory.load_u32(address)
               : 0U;
  };
  const auto add = [](std::uint32_t base,
                      std::uint32_t offset) noexcept -> std::uint32_t {
    return base <= std::numeric_limits<std::uint32_t>::max() - offset
               ? base + offset
               : 0U;
  };
  const auto dump_words = [&](const char *label, std::uint32_t address,
                              std::uint32_t count) noexcept {
    std::fprintf(stderr, " %s=0x%08X", label, address);
    for (std::uint32_t i = 0U; i < count; ++i) {
      std::fprintf(stderr, "[%u]=0x%08X", i, read_u32(add(address, i * 4U)));
    }
  };

  std::fprintf(stderr,
               "AC6_SWG_BLOB tick=%llu function=%s lr=0x%08X thread=%u "
               "r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X r7=0x%08X "
               "r8=0x%08X",
               static_cast<unsigned long long>(tick), generated_name,
               static_cast<std::uint32_t>(context.lr), current_guest_thread_id,
               context.r3.u32, context.r4.u32, context.r5.u32,
               context.r6.u32, context.r7.u32, context.r8.u32);
  if (std::string_view(generated_name).find("82326B80") !=
      std::string_view::npos) {
    const auto storage = context.r3.u32;
    const auto base = context.r4.u32;
    const auto blob = context.r5.u32;
    dump_words("storage", storage, 8U);
    dump_words("base", base, 8U);
    dump_words("blob", blob, 8U);
    dump_words("blob_p4", read_u32(add(blob, 4U)), 4U);
    dump_words("blob_p8", read_u32(add(blob, 8U)), 4U);
    dump_words("blob_pc", read_u32(add(blob, 0x0CU)), 4U);
    dump_words("blob_p10", read_u32(add(blob, 0x10U)), 4U);
  } else {
    const auto owner = context.r3.u32;
    const auto storage = read_u32(add(owner, 0x20U));
    const auto base = read_u32(storage);
    const auto offsets = read_u32(add(storage, 0x38U));
    const auto element = context.r4.u32;
    const auto list_index = read_u32(add(element, 8U));
    const auto list_entry = add(offsets, add(list_index * 8U, 4U));
    const auto list = add(base, read_u32(list_entry));
    const auto count = read_u32(list);
    std::fprintf(stderr,
                 " owner=0x%08X storage=0x%08X base=0x%08X offsets=0x%08X "
                 "element=0x%08X list_index=0x%08X list=0x%08X count=0x%08X",
                 owner, storage, base, offsets, element, list_index, list,
                 count);
    auto record = add(list, 4U);
    for (std::uint32_t i = 0U; i < std::min(count, 4U); ++i) {
      std::fprintf(stderr,
                   " record%u=0x%08X/type=0x%08X/next=0x%08X/aux=0x%08X/"
                   "draw=0x%08X",
                   i, record, read_u32(record), read_u32(add(record, 4U)),
                   read_u32(add(record, 8U)), read_u32(add(record, 0x0CU)));
      record = add(base, read_u32(add(record, 4U)));
    }
    dump_words("storage_p20", read_u32(add(storage, 0x20U)), 4U);
    dump_words("storage_p28", read_u32(add(storage, 0x28U)), 4U);
  }
  std::fputc('\n', stderr);
}

static void trace_acc_resource_join(const PPCContext &context,
                                    const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_ACC_RESOURCE_JOIN") != nullptr;
  static const bool title_only =
      std::getenv("AC6_DEMO_WATCH_ACC_RESOURCE_TITLE_ONLY") != nullptr;
  static const std::uint64_t start_tick = []() noexcept -> std::uint64_t {
    const char *text =
        std::getenv("AC6_DEMO_WATCH_ACC_RESOURCE_START_TICK");
    return text == nullptr ? 0U : std::strtoull(text, nullptr, 0);
  }();
  if (!enabled || generated_name == nullptr || active_bridge == nullptr) {
    return;
  }
  if (active_bridge->tick() < start_tick ||
      (title_only && active_bridge->tick() < 190U)) {
    return;
  }

  constexpr std::array<std::string_view, 11U> targets{
      "821A0180", "8219E428", "821A02C0", "8219E768", "8219E580",
      "821A00E8", "8219F080", "820EB200", "820EA9A0", "82095DF0",
      "821DEED8"};
  const auto target = std::find_if(
      targets.begin(), targets.end(), [&](std::string_view address) {
        return std::string_view(generated_name).find(address) !=
               std::string_view::npos;
      });
  if (target == targets.end()) {
    return;
  }
  static std::array<std::uint32_t, targets.size()> counts{};
  const auto target_index = static_cast<std::size_t>(target - targets.begin());
  if (counts[target_index]++ >= 64U) {
    return;
  }

  auto &memory = active_bridge->memory();
  const auto object = context.r3.u32;
  const auto read_u32 = [&](std::uint32_t address) noexcept {
    return address != 0U && memory.mapped(address, 4U)
               ? memory.load_u32(address)
               : 0U;
  };
  const auto read_field = [&](std::uint32_t base,
                              std::uint32_t offset) noexcept {
    return base <= std::numeric_limits<std::uint32_t>::max() - offset
               ? read_u32(base + offset)
               : 0U;
  };
  const auto source = *target == "8219E580"
                          ? read_field(object, 0x18U)
                          : (*target == "821A0180" ? context.r5.u32
                                                   : context.r4.u32);
  const auto index = *target == "8219E580"
                         ? read_field(object, 0x1CU)
                         : (*target == "8219E428" ? context.r8.u32
                                                   : context.r5.u32);
  const auto table = source <= std::numeric_limits<std::uint32_t>::max() -
                                   0x20U
                         ? source + 0x20U
                         : 0U;
  const auto table_base = read_field(table, 4U);
  const auto table_offsets = read_field(table, 12U);
  const auto table_offset0 = read_u32(table_offsets);
  const auto table_entry0 =
      table_base <= std::numeric_limits<std::uint32_t>::max() - table_offset0
          ? table_base + table_offset0
          : 0U;
  const auto geometry = context.r5.u32;
  const auto draw = context.r4.u32;
  const auto draw_index = read_field(draw, 12U);
  const auto resource_slot =
      object <= std::numeric_limits<std::uint32_t>::max() - 16U -
                    4U * draw_index
          ? read_field(object, 16U + 4U * draw_index)
          : 0U;
  std::fprintf(
      stderr,
      "AC6_ACC_RESOURCE_JOIN tick=%llu function=%s lr=0x%08X thread=%u "
      "r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X r7=0x%08X "
      "r8=0x%08X source=0x%08X index=0x%08X table=0x%08X "
      "table_count=0x%08X table_base=0x%08X table_offsets=0x%08X "
      "table_parallel=0x%08X table_offset0=0x%08X table_entry0=0x%08X "
      "table_entry0_0=0x%08X table_entry0_4=0x%08X "
      "draw_index=0x%08X resource_slot=0x%08X "
      "geometry_0=0x%08X geometry_4=0x%08X geometry_16=0x%08X "
      "geometry_20=0x%08X geometry_48=0x%08X geometry_52=0x%08X\n",
      static_cast<unsigned long long>(active_bridge->tick()), generated_name,
      static_cast<std::uint32_t>(context.lr), current_guest_thread_id,
      context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32,
      context.r7.u32, context.r8.u32, source, index, table, read_u32(table),
      table_base, table_offsets, read_field(table, 16U), table_offset0,
      table_entry0, read_field(table_entry0, 0U), read_field(table_entry0, 4U),
      draw_index, resource_slot, read_field(geometry, 0U),
      read_field(geometry, 4U), read_field(geometry, 16U),
      read_field(geometry, 20U), read_field(geometry, 48U),
      read_field(geometry, 52U));
}

static void trace_brandlogo_draw_selector(
    const PPCContext &context, const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_BRANDLOGO_DRAW_SELECTOR") != nullptr;
  static const std::uint64_t start_tick = []() noexcept -> std::uint64_t {
    const char *text =
        std::getenv("AC6_DEMO_WATCH_BRANDLOGO_DRAW_SELECTOR_START_TICK");
    return text == nullptr ? std::uint64_t{190U}
                           : static_cast<std::uint64_t>(
                                 std::strtoull(text, nullptr, 0));
  }();
  if (!enabled || generated_name == nullptr || active_bridge == nullptr ||
      active_bridge->tick() < start_tick) {
    return;
  }

  const std::string_view function{generated_name};
  const bool frame_gate = function.find("823266F8") != std::string_view::npos;
  const bool list_walk = function.find("82326420") != std::string_view::npos;
  const bool draw_call = function.find("820EB200") != std::string_view::npos &&
                         static_cast<std::uint32_t>(context.lr) == 0x82325ED4U;
  if (!frame_gate && !list_walk && !draw_call) {
    return;
  }

  static std::uint32_t event_count = 0U;
  if (event_count++ >= 512U) {
    return;
  }

  auto &memory = active_bridge->memory();
  const auto read_u8 = [&](std::uint32_t address) noexcept {
    return address != 0U && memory.mapped(address, 1U)
               ? memory.load_u8(address)
               : std::uint8_t{0U};
  };
  const auto read_u32 = [&](std::uint32_t address) noexcept {
    return address != 0U && memory.mapped(address, 4U)
               ? memory.load_u32(address)
               : 0U;
  };
  const auto add = [](std::uint32_t base, std::uint32_t offset) noexcept {
    return base <= std::numeric_limits<std::uint32_t>::max() - offset
               ? base + offset
               : 0U;
  };

  if (frame_gate) {
    const auto owner = context.r3.u32;
    const auto frame_begin = read_u32(add(owner, 40U));
    const auto frame_end = read_u32(add(owner, 44U));
    const auto frame_index = read_u32(add(owner, 220U));
    const bool valid_index =
        frame_end >= frame_begin &&
        frame_index < (frame_end - frame_begin) / 8U;
    const auto frame_entry = valid_index ? add(frame_begin, frame_index * 8U)
                                         : 0U;
    const auto frame_offset = read_u32(frame_entry);
    const auto frame_count = read_u32(add(frame_entry, 4U));
    const auto storage = read_u32(add(owner, 32U));
    const auto storage_base = read_u32(storage);
    const auto selected_frame = add(storage_base, frame_offset);
    std::fprintf(
        stderr,
        "AC6_BRANDLOGO_FRAME_GATE tick=%llu thread=%u owner=0x%08X "
        "enabled=%u frame_index=0x%08X frame_begin=0x%08X "
        "frame_end=0x%08X index_valid=%u frame_entry=0x%08X "
        "frame_offset=0x%08X frame_count=0x%08X selected=0x%08X\n",
        static_cast<unsigned long long>(active_bridge->tick()),
        current_guest_thread_id, owner, read_u8(add(owner, 215U)), frame_index,
        frame_begin, frame_end, valid_index ? 1U : 0U, frame_entry,
        frame_offset, frame_count, selected_frame);
    return;
  }

  if (list_walk) {
    const auto owner = context.r3.u32;
    const auto element = context.r4.u32;
    const auto list_index = read_u32(add(element, 8U));
    const auto storage = read_u32(add(owner, 32U));
    const auto storage_base = read_u32(storage);
    const auto list_offsets = read_u32(add(storage, 56U));
    const auto list_entry =
        list_index <= (std::numeric_limits<std::uint32_t>::max() - 4U) / 8U
            ? add(list_offsets, list_index * 8U + 4U)
            : 0U;
    const auto list_offset = read_u32(list_entry);
    const auto list = add(storage_base, list_offset);
    const auto list_count = read_u32(list);
    std::fprintf(
        stderr,
        "AC6_BRANDLOGO_LIST tick=%llu thread=%u owner=0x%08X "
        "element=0x%08X list_index=0x%08X storage=0x%08X "
        "base=0x%08X offsets=0x%08X entry=0x%08X offset=0x%08X "
        "list=0x%08X count=0x%08X",
        static_cast<unsigned long long>(active_bridge->tick()),
        current_guest_thread_id, owner, element, list_index, storage,
        storage_base, list_offsets, list_entry, list_offset, list, list_count);
    auto record = add(list, 4U);
    const auto bounded_count = std::min(list_count, 8U);
    for (std::uint32_t index = 0U; index < bounded_count; ++index) {
      const auto type = read_u32(record);
      const auto next = read_u32(add(record, 4U));
      const auto draw_index = read_u32(add(record, 12U));
      std::fprintf(stderr,
                   " record%u=0x%08X/type=0x%08X/next=0x%08X/draw=0x%08X",
                   index, record, type, next, draw_index);
      record = add(storage_base, next);
    }
    std::fputc('\n', stderr);
    return;
  }

  const auto self = context.r3.u32;
  const auto element = context.r4.u32;
  const auto draw_index = read_u32(add(element, 12U));
  const auto slot_address =
      draw_index <= (std::numeric_limits<std::uint32_t>::max() - 16U) / 4U
          ? add(self, 16U + 4U * draw_index)
          : 0U;
  std::fprintf(
      stderr,
      "AC6_BRANDLOGO_DRAW_SELECT tick=%llu thread=%u self=0x%08X "
      "element=0x%08X type=0x%08X next=0x%08X draw_index=0x%08X "
      "slot_address=0x%08X slot=0x%08X\n",
      static_cast<unsigned long long>(active_bridge->tick()),
      current_guest_thread_id, self, element, read_u32(element),
      read_u32(add(element, 4U)), draw_index, slot_address,
      read_u32(slot_address));
}

static void trace_brandlogo_owner_update(
    const PPCContext &context, const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_BRANDLOGO_OWNER_UPDATE") != nullptr;
  if (!enabled || generated_name == nullptr || active_bridge == nullptr) {
    return;
  }
  const auto tick = active_bridge->tick();
  if (tick < 190U || tick > 430U) {
    return;
  }
  const std::string_view function{generated_name};
  const bool parent_loop = function.find("820DE878") != std::string_view::npos;
  const bool child_call = function.find("82324118") != std::string_view::npos;
  const bool owner_update = function.find("82323BB8") != std::string_view::npos;
  const bool owner_reset = function.find("82323808") != std::string_view::npos;
  const bool owner_callback =
      function.find("82322D28") != std::string_view::npos ||
      function.find("82322D68") != std::string_view::npos ||
      function.find("82322DB0") != std::string_view::npos ||
      function.find("823229D8") != std::string_view::npos;
  if (!parent_loop && !child_call && !owner_update && !owner_reset &&
      !owner_callback) {
    return;
  }
  static std::uint32_t event_count = 0U;
  if (event_count++ >= 256U) {
    return;
  }
  auto &memory = active_bridge->memory();
  const auto read_u8 = [&](std::uint32_t address) noexcept {
    return address != 0U && memory.mapped(address, 1U)
               ? memory.load_u8(address)
               : std::uint8_t{0U};
  };
  const auto read_u32 = [&](std::uint32_t address) noexcept {
    return address != 0U && memory.mapped(address, 4U)
               ? memory.load_u32(address)
               : 0U;
  };
  const auto add = [](std::uint32_t base, std::uint32_t offset) noexcept {
    return base <= std::numeric_limits<std::uint32_t>::max() - offset
               ? base + offset
               : 0U;
  };
  const auto dump_owner = [&](std::uint32_t owner) noexcept {
    std::fprintf(
        stderr,
        " owner=0x%08X f20=0x%08X f2c=0x%08X f30=0x%08X "
        "b0=0x%02X b1=0x%02X b2=0x%02X b3=0x%02X "
        "u216=0x%08X u220=0x%08X u224=0x%08X u228=0x%08X "
        "u232=0x%08X u236=0x%08X u240=0x%08X u244=0x%08X "
        "u248=0x%08X u252=0x%08X frame0=0x%08X frame1=0x%08X "
        "ctrl=0x%08X ctrl244=0x%08X child420=0x%02X",
        owner, read_u32(add(owner, 0x20U)), read_u32(add(owner, 0x2CU)),
        read_u32(add(owner, 0x30U)), read_u8(add(owner, 0xD4U)),
        read_u8(add(owner, 0xD5U)), read_u8(add(owner, 0xD6U)),
        read_u8(add(owner, 0xD7U)), read_u32(add(owner, 216U)),
        read_u32(add(owner, 220U)),
        read_u32(add(owner, 224U)), read_u32(add(owner, 228U)),
        read_u32(add(owner, 232U)), read_u32(add(owner, 236U)),
        read_u32(add(owner, 240U)), read_u32(add(owner, 244U)),
        read_u32(add(owner, 248U)), read_u32(add(owner, 252U)),
        read_u32(add(owner, 40U)), read_u32(add(owner, 44U)),
        read_u32(add(owner, 412U)),
        read_u32(add(read_u32(add(owner, 412U)), 244U)),
        read_u8(add(owner, 0x1A4U)));
  };

  std::fprintf(stderr,
               "AC6_BRANDLOGO_OWNER_UPDATE tick=%llu function=%s "
               "lr=0x%08X thread=%u r3=0x%08X r4=0x%08X r5=0x%08X",
               static_cast<unsigned long long>(tick), generated_name,
               static_cast<std::uint32_t>(context.lr), current_guest_thread_id,
               context.r3.u32, context.r4.u32, context.r5.u32);
  if (parent_loop) {
    const auto parent = context.r3.u32;
    const auto begin = read_u32(add(parent, 44U));
    const auto end = read_u32(add(parent, 48U));
    std::fprintf(stderr, " parent=0x%08X begin=0x%08X end=0x%08X",
                 parent, begin, end);
    for (std::uint32_t cursor = begin, n = 0U;
         cursor != 0U && cursor < end && n < 8U; cursor += 8U, ++n) {
      const auto child = read_u32(cursor);
      std::fprintf(stderr, " child%u=0x%08X", n, child);
      dump_owner(child);
    }
  } else if (child_call) {
    std::fprintf(stderr, " parent_array=0x%08X child=0x%08X", context.r3.u32,
                 context.r4.u32);
    dump_owner(context.r4.u32);
  } else {
    dump_owner(context.r3.u32);
    std::fprintf(stderr, " aux_arg=0x%08X", context.r4.u32);
  }
  std::fputc('\n', stderr);
}

static void trace_brandlogo_owner_store(const PPCContext &context,
                                        std::uint32_t address,
                                        std::uint32_t size,
                                        std::uint32_t value,
                                        const char *generated_name,
                                        std::uint32_t generated_line) noexcept;

static void trace_brandlogo_consumer(const PPCContext &context,
                                     const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_BRANDLOGO_CONSUMER") != nullptr;
  static const std::uint64_t start_tick = []() noexcept -> std::uint64_t {
    const char *text =
        std::getenv("AC6_DEMO_WATCH_BRANDLOGO_CONSUMER_START_TICK");
    return text == nullptr ? 0U : std::strtoull(text, nullptr, 0);
  }();
  if (!enabled || generated_name == nullptr || active_bridge == nullptr) {
    return;
  }
  if (active_bridge->tick() < start_tick) {
    return;
  }

  constexpr std::array<std::string_view, 8U> targets{
      "82119488", "82118FA0", "821185A8", "821186B0",
      "821187A8", "821188B0", "821B4D80", "821B5168"};
  const auto target = std::find_if(
      targets.begin(), targets.end(), [&](std::string_view address) {
        return std::string_view(generated_name).find(address) !=
               std::string_view::npos;
      });
  if (target == targets.end()) {
    return;
  }
  static std::array<std::uint32_t, targets.size()> counts{};
  const auto target_index = static_cast<std::size_t>(target - targets.begin());
  if (counts[target_index]++ >= 64U) {
    return;
  }

  auto &memory = active_bridge->memory();
  const auto read_u32 = [&](std::uint32_t address) noexcept {
    return address != 0U && memory.mapped(address, 4U)
               ? memory.load_u32(address)
               : 0U;
  };
  const auto read_field = [&](std::uint32_t base,
                              std::uint32_t offset) noexcept {
    return base <= std::numeric_limits<std::uint32_t>::max() - offset
               ? read_u32(base + offset)
               : 0U;
  };
  const auto owner_count = read_u32(0x826F6310U);
  const auto brand_owner = read_u32(0x826F6320U);
  const auto record = *target == "821185A8" ? context.r5.u32
                      : (*target == "82118FA0" ? context.r4.u32 : 0U);
  std::fprintf(
      stderr,
      "AC6_BRANDLOGO_CONSUMER tick=%llu function=%s lr=0x%08X thread=%u "
      "r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X "
      "renderer_resource=0x%08X owner_count=0x%08X "
      "brand_owner=0x%08X brand_head=0x%08X "
      "record_flags=0x%08X record_resource=0x%08X record_type=0x%08X\n",
      static_cast<unsigned long long>(active_bridge->tick()), generated_name,
      static_cast<std::uint32_t>(context.lr), current_guest_thread_id,
      context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32,
      read_u32(0x826F61B8U), owner_count, brand_owner,
      read_field(brand_owner, 32U), read_field(record, 4U),
      read_field(record, 8U), read_field(record, 20U));
}

static void trace_brandlogo_submit(const PPCContext &context,
                                   const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_BRANDLOGO_SUBMIT") != nullptr;
  if (!enabled || generated_name == nullptr || active_bridge == nullptr) {
    return;
  }

  constexpr std::array<std::string_view, 17U> targets{
      "821BA780", "821BA5F8", "821BA058", "821B9810",
      "821B9BC8", "821C41F8", "821C4A60", "821C57D0",
      "821B8ED8", "821B9120", "821BA1F8", "821BAA78",
      "821C4CA8", "821C4D30", "821C5458", "821C5190",
      "821B9710"};
  const auto target = std::find_if(
      targets.begin(), targets.end(), [&](std::string_view address) {
        return std::string_view(generated_name).find(address) !=
               std::string_view::npos;
      });
  if (target == targets.end()) {
    return;
  }
  const auto tick = active_bridge->tick();
  const auto target_index = static_cast<std::size_t>(target - targets.begin());
  const bool callback_path = target_index >= 8U || *target == "821C4A60";
  if (tick > 225U || (!callback_path && tick < 190U)) {
    return;
  }
  static std::array<std::uint32_t, targets.size()> counts{};
  if (counts[target_index]++ >= 128U) {
    return;
  }

  auto &memory = active_bridge->memory();
  const auto read_u32 = [&](std::uint32_t address) noexcept {
    return address != 0U && memory.mapped(address, 4U)
               ? memory.load_u32(address)
               : 0U;
  };
  const auto read_u8 = [&](std::uint32_t address) noexcept {
    return address != 0U && memory.mapped(address, 1U)
               ? memory.load_u8(address)
               : 0U;
  };
  const auto device_slot = read_u32(0x82000608U);
  const auto device = read_u32(device_slot);
  std::fprintf(
      stderr,
      "AC6_BRANDLOGO_SUBMIT tick=%llu function=%s lr=0x%08X thread=%u "
      "r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X r7=0x%08X "
      "device_slot=0x%08X device=0x%08X cursor=0x%08X bound=0x%08X "
      "pending=0x%08X irq_queue=0x%08X primary_limit=0x%08X "
      "mode=0x%02X flags=0x%02X primary_sink=0x%08X "
      "ring_wptr=0x%08X ring_base=0x%08X ring_mask=0x%08X\n",
      static_cast<unsigned long long>(tick), generated_name,
      static_cast<std::uint32_t>(context.lr), current_guest_thread_id,
      context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32,
      context.r7.u32, device_slot, device, read_u32(device + 0x30U),
      read_u32(device + 0x38U), read_u32(device + 0x2AF8U),
      read_u32(device + 0x2A94U), read_u32(device + 0x2A9CU),
      read_u8(device + 0x2ABCU), read_u8(device + 0x2ABDU),
      read_u32(device + 0x5404U), read_u32(device + 0x2AC8U),
      read_u32(device + 0x3A18U), read_u32(device + 0x3A1CU));
}

static void trace_draw_scheduler(const PPCContext &context,
                                 const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_DRAW_SCHEDULER") != nullptr;
  if (!enabled || generated_name == nullptr || active_bridge == nullptr) {
    return;
  }
  const auto tick = active_bridge->tick();
  if (tick < 2990U || tick > 3020U) {
    return;
  }
  constexpr std::array<std::string_view, 9U> targets{
      "822DA8D8", "822E3D28", "822F0610", "822E3380", "822F84E0",
      "821B6078", "821B5B10", "821B58B0", "821B55C0"};
  const auto target = std::find_if(
      targets.begin(), targets.end(), [&](std::string_view address) {
        return std::string_view(generated_name).find(address) !=
               std::string_view::npos;
      });
  if (target == targets.end()) {
    return;
  }

  auto &memory = active_bridge->memory();
  const auto node = context.r4.u32;
  std::uint32_t vtable = 0U;
  std::uint32_t next = 0U;
  std::uint32_t slot3 = 0U;
  std::uint32_t slot5 = 0U;
  std::array<std::uint32_t, 7U> draw_fields{};
  if (*target == "822E3380" && node != 0U && memory.mapped(node, 8U)) {
    vtable = memory.load_u32(node);
    next = memory.load_u32(node + 4U);
    if (vtable <= std::numeric_limits<std::uint32_t>::max() - 24U &&
        memory.mapped(vtable + 12U, 12U)) {
      slot3 = memory.load_u32(vtable + 12U);
      slot5 = memory.load_u32(vtable + 20U);
    }
  } else if (*target == "822F84E0" && context.r3.u32 != 0U &&
             memory.mapped(context.r3.u32, 44U)) {
    vtable = memory.load_u32(context.r3.u32);
    for (std::size_t index = 0U; index < draw_fields.size(); ++index) {
      draw_fields[index] = memory.load_u32(
          context.r3.u32 + 16U + static_cast<std::uint32_t>(index) * 4U);
    }
  }
  std::fprintf(
      stderr,
      "AC6_DRAW_SCHEDULER tick=%llu function=%s lr=0x%08X thread=%u "
      "r3=0x%08X r4=0x%08X r5=0x%08X node_vtable=0x%08X "
      "node_next=0x%08X slot3=0x%08X slot5=0x%08X "
      "f10=0x%08X f14=0x%08X f18=0x%08X f1C=0x%08X "
      "f20=0x%08X f24=0x%08X f28=0x%08X\n",
      static_cast<unsigned long long>(tick), generated_name,
      static_cast<std::uint32_t>(context.lr), current_guest_thread_id,
      context.r3.u32, context.r4.u32, context.r5.u32, vtable, next, slot3,
      slot5, draw_fields[0], draw_fields[1], draw_fields[2], draw_fields[3],
      draw_fields[4], draw_fields[5], draw_fields[6]);
}

static void trace_title_vertex_allocation(
    const PPCContext &context, const char *generated_name) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_TITLE_VERTEX_ALLOCATION") != nullptr;
  if (!enabled || generated_name == nullptr || active_bridge == nullptr) {
    return;
  }
  constexpr std::array<std::string_view, 4U> targets{
      "821C07F8", "821C0458", "821BEFF0", "821BEE60"};
  const auto target = std::find_if(
      targets.begin(), targets.end(), [&](std::string_view address) {
        return std::string_view(generated_name).find(address) !=
               std::string_view::npos;
      });
  if (target == targets.end()) {
    return;
  }
  static std::array<std::uint32_t, targets.size()> counts{};
  const auto target_index = static_cast<std::size_t>(target - targets.begin());
  if (counts[target_index]++ >= 64U) {
    return;
  }
  auto &memory = active_bridge->memory();
  constexpr std::uint32_t kGlobalGuard = 0x827AD41EU;
  constexpr std::uint32_t kObjectGuardOffset = 0x56ECU;
  const bool global_mapped = memory.mapped(kGlobalGuard, 1U);
  const auto object = context.r3.u32;
  const bool object_guard_address_valid =
      object <= std::numeric_limits<std::uint32_t>::max() -
                    kObjectGuardOffset;
  const auto object_guard_address =
      object_guard_address_valid ? object + kObjectGuardOffset : 0U;
  const bool object_guard_mapped = object_guard_address_valid &&
      memory.mapped(object_guard_address, 1U);
  std::fprintf(
      stderr,
      "AC6_TITLE_VERTEX_ALLOC_ENTRY tick=%llu function=%s lr=0x%08X "
      "thread=%u r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X "
      "r7=0x%08X r8=0x%08X global_guard_mapped=%u "
      "global_guard=0x%02X object_guard_mapped=%u object_guard=0x%02X\n",
      static_cast<unsigned long long>(active_bridge->tick()), generated_name,
      static_cast<std::uint32_t>(context.lr), current_guest_thread_id,
      context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32,
      context.r7.u32, context.r8.u32, global_mapped ? 1U : 0U,
      global_mapped ? memory.load_u8(kGlobalGuard) : 0U,
      object_guard_mapped ? 1U : 0U,
      object_guard_mapped ? memory.load_u8(object_guard_address) : 0U);
}

extern "C" void AC6_PPC_FUNCTION_ENTRY_CONTEXT(
    PPCContext &context, const char *generated_name) noexcept {
  trace_title_terminal_function_entry(context, generated_name);
  trace_draw_scheduler(context, generated_name);
  trace_swg_context(context, generated_name);
  trace_swg_slot4(context, generated_name);
  trace_swg_blob(context, generated_name);
  trace_acc_resource_join(context, generated_name);
  trace_brandlogo_draw_selector(context, generated_name);
  trace_brandlogo_owner_update(context, generated_name);
  trace_brandlogo_consumer(context, generated_name);
  trace_title_writer(context, generated_name);
  trace_brandlogo_submit(context, generated_name);
  trace_title_vertex_allocation(context, generated_name);
  trace_record_type_route(context, generated_name);
  trace_provider_population(context, generated_name);
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
    trace_loading_provider_entry(context, generated_name);
    trace_loading_resource_poll_entry(generated_name); active_bridge->record_function_entry(generated_name);
  }
}
extern "C" void AC6_PPC_SET_LOAD_SITE(const char *generated_name,
                                       std::uint32_t generated_line) noexcept {
  const bool enabled =
      ac6demo::guest_bridge_detail::guest_load_site_watchers_enabled() ||
      ac6demo::guest_bridge_detail::post_resume_watch_enabled_fast() ||
      title_terminal_trace_enabled();
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
static void trace_as_context_counter_access(const PPCContext &context,
                                            std::uint32_t address,
                                            std::uint32_t value,
                                            const char *kind);

extern "C" std::uint32_t AC6_PPC_LOAD_U32(PPCContext &context,
                                          std::uint8_t *base,
                                          std::uint32_t address) {
  (void)base; ac6demo::guest_bridge_detail::trace_xma_late_access("load32", address, 4U, false, 0U, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), current_load_generated_name, current_load_generated_line);
  if (ac6demo::guest_bridge_detail::graphics_interrupt_state_load_guard(address, current_load_generated_name)) throw ac6demo::RuntimeTrap("graphics interrupt state load outside qualified range", require_bridge().tick(), static_cast<std::uint32_t>(context.lr), address);
  const auto value = guest_memory_access(context, address, [&] { return memory_for(context).load_u32(address); });
  trace_as_context_counter_access(context, address, value, "LOAD");
  ac6demo::guest_bridge_detail::trace_controller_reader(
      address, 4U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_controller_target_reader(
      address, 4U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), current_load_generated_name,
      current_load_generated_line);
  ac6demo::guest_bridge_detail::trace_input_semantic_access(
      "load32", address, 4U, value, require_bridge().tick(),
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr),
      current_load_generated_name, current_load_generated_line); ac6demo::guest_bridge_detail::trace_task_list_access("load32", address, 4U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), current_load_generated_name, current_load_generated_line);
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
  static const bool watch_handle_consumers = std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_CONSUMERS") != nullptr;
  static const bool watch_handle_payload = std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_PAYLOAD") != nullptr;
  if (watch_handle_consumers && (events.contains(value) || mutants.contains(value) || semaphores.contains(value) || kernel_semaphores.contains(value) || timers.contains(value) || require_bridge().is_guest_thread_handle(value))) {
    ac6demo::guest_bridge_detail::trace_event_handle_consumer(address, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), current_load_generated_name, current_load_generated_line);
    if (watch_handle_payload) {
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
  trace_brandlogo_owner_store(context, address, 1U, value, generated_name,
                              generated_line);
  ac6demo::guest_bridge_detail::trace_frontbuffer_write(
      address, 1U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_event_handle_payload_writer(address, 1U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  trace_chunk_target_store(context, address, 1U, value, generated_name,
                           generated_line);
  trace_transition_store(context, address, 1U, value, generated_name,
                         generated_line);
  trace_render_queue_slot_store(context, address, 1U, value, generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_ib_write(
      address, 1U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_addr_range_write(
      address, 1U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  trace_title_writer_store(context, address, 1U, value, generated_name,
                           generated_line);
  guest_memory_access(context, address, [&] {
    const auto observation = trace_title_terminal_prepare_store(
        context, address, 1U, generated_name, generated_line);
    memory_for(context).store_u8(address, value);
    trace_title_terminal_commit_store(observation);
  });
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
  trace_render_queue_slot_store(context, address, 2U, value, generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_ib_write(
      address, 2U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_addr_range_write(
      address, 2U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  trace_title_writer_store(context, address, 2U, value, generated_name,
                           generated_line);
  guest_memory_access(context, address, [&] {
    const auto observation = trace_title_terminal_prepare_store(
        context, address, 2U, generated_name, generated_line);
    memory_for(context).store_u16(address, value);
    trace_title_terminal_commit_store(observation);
  });
  ac6demo::guest_bridge_detail::record_post_resume_scalar("store16", address, 2U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_xma_slot_store(
      address, 2U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
}
extern "C" void AC6_PPC_STORE_U32(PPCContext &context, std::uint8_t *base,
                                  std::uint32_t address, std::uint32_t value,
                                  const char *generated_name,
                                  std::uint32_t generated_line) {
  (void)base; trace_brandlogo_owner_store(context, address, 4U, value, generated_name, generated_line); trace_body_store(context, address, value, generated_name, generated_line); ac6demo::guest_bridge_detail::trace_xma_late_access("store32", address, 4U, true, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line); ac6demo::guest_bridge_detail::trace_xma_address_store(address, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  trace_as_context_counter_access(context, address, value, "STORE");
  ac6demo::guest_bridge_detail::trace_input_semantic_access(
      "store32", address, 4U, value, require_bridge().tick(),
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr),
      generated_name, generated_line); ac6demo::guest_bridge_detail::trace_task_list_access("store32", address, 4U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_frontbuffer_write(
      address, 4U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_event_handle_payload_writer(address, 4U, value, require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  trace_chunk_target_store(context, address, 4U, value, generated_name,
                           generated_line);
  trace_transition_store(context, address, 4U, value, generated_name,
                         generated_line);
  trace_mode_state_store(context, address, value, generated_name,
                         generated_line);
  trace_render_queue_slot_store(context, address, 4U, value, generated_name, generated_line);
  trace_render_queue_writer(context, address, value, generated_name, generated_line);
  static const bool watch_handle_writers = std::getenv("AC6_DEMO_WATCH_EVENT_HANDLE_WRITERS") != nullptr;
  if (watch_handle_writers &&
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
  ac6demo::guest_bridge_detail::trace_addr_range_write(
      address, 4U, value, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  trace_title_writer_store(context, address, 4U, value, generated_name,
                           generated_line);
  guest_memory_access(context, address, [&] {
    const auto observation = trace_title_terminal_prepare_store(
        context, address, 4U, generated_name, generated_line);
    memory_for(context).store_u32(address, value);
    trace_title_terminal_commit_store(observation);
  });
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
  trace_render_queue_slot_store(context, address, 8U, value, generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_ib_write(
      address, 8U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_addr_range_write(
      address, 8U, static_cast<std::uint32_t>(value), require_bridge().tick(),
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr),
      generated_name, generated_line);
  trace_title_writer_store(context, address, 8U,
                           static_cast<std::uint32_t>(value), generated_name,
                           generated_line);
  guest_memory_access(context, address, [&] {
    const auto observation = trace_title_terminal_prepare_store(
        context, address, 8U, generated_name, generated_line);
    memory_for(context).store_u64(address, value);
    trace_title_terminal_commit_store(observation);
  });
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
  trace_render_queue_slot_store(context, address, 16U, std::any_of(value, value + 16U, [](std::uint8_t byte) { return byte != 0U; }) ? 1U : 0U, generated_name, generated_line);
  ac6demo::guest_bridge_detail::trace_ib_write(
      address, 16U, require_bridge().tick(), current_guest_thread_id,
      static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
  const auto first_guest_dword =
      (static_cast<std::uint32_t>(value[15U]) << 24U) |
      (static_cast<std::uint32_t>(value[14U]) << 16U) |
      (static_cast<std::uint32_t>(value[13U]) << 8U) |
      static_cast<std::uint32_t>(value[12U]);
  ac6demo::guest_bridge_detail::trace_addr_range_write(
      address, 16U, first_guest_dword, require_bridge().tick(),
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr),
      generated_name, generated_line);
  trace_title_writer_store(context, address, 16U, first_guest_dword,
                           generated_name, generated_line);
  std::array<std::byte, 16U> guest_bytes{};
  std::array<std::uint8_t, 16U> guest_bytes_for_trace{};
  for (std::size_t index = 0U; index < guest_bytes.size(); ++index) {
    const auto guest_byte = value[15U - index];
    guest_bytes[index] = static_cast<std::byte>(guest_byte);
    guest_bytes_for_trace[index] = guest_byte;
  }
  guest_memory_access(context, address, [&] {
    const auto observation = trace_title_terminal_prepare_store(
        context, address, 16U, generated_name, generated_line);
    memory_for(context).store_bytes(address, guest_bytes);
    trace_title_terminal_commit_store(observation);
  });
  ac6demo::guest_bridge_detail::record_post_resume_bytes("store128", address, 16U, guest_bytes_for_trace.data(), require_bridge().tick(), current_guest_thread_id, static_cast<std::uint32_t>(context.lr), generated_name, generated_line);
}

static void trace_movie_frame_dispatch(const PPCContext &context,
                                       std::uint32_t lr,
                                       std::uint32_t target) {
  const auto tick = require_bridge().tick();
  if (std::getenv("AC6_DEMO_WATCH_MOVIE_FRAME_DISPATCH") == nullptr ||
      lr != 0x82323E4CU || tick < 2990U || tick > 3020U) {
    return;
  }
  std::fprintf(stderr,
               "AC6_MOVIE_FRAME_DISPATCH tick=%llu target=0x%08X "
               "r3=0x%08X r4=0x%08X r25=0x%08X r26=0x%08X r31=0x%08X\n",
               static_cast<unsigned long long>(tick), target, context.r3.u32,
               context.r4.u32, context.r25.u32, context.r26.u32,
               context.r31.u32);
}

static void trace_brandlogo_owner_vcall(const PPCContext &context,
                                        std::uint32_t lr,
                                        std::uint32_t target) noexcept {
  if (std::getenv("AC6_DEMO_WATCH_BRANDLOGO_OWNER_VCALL") == nullptr ||
      active_bridge == nullptr) {
    return;
  }
  const auto tick = active_bridge->tick();
  if (tick < 220U || tick > 240U) {
    return;
  }
  constexpr std::array kCallsites{
      std::pair<std::uint32_t, std::uint32_t>{0x82323DC8U, 1U},
      std::pair<std::uint32_t, std::uint32_t>{0x82323DDCU, 12U},
      std::pair<std::uint32_t, std::uint32_t>{0x82323E4CU, 0U},
      std::pair<std::uint32_t, std::uint32_t>{0x82323E7CU, 6U},
      std::pair<std::uint32_t, std::uint32_t>{0x82323EB4U, 10U},
      std::pair<std::uint32_t, std::uint32_t>{0x82323F08U, 11U},
      std::pair<std::uint32_t, std::uint32_t>{0x82323F20U, 8U},
  };
  const auto it = std::find_if(
      kCallsites.begin(), kCallsites.end(),
      [lr](const auto &entry) { return entry.first == lr; });
  if (it == kCallsites.end()) {
    return;
  }
  auto &memory = active_bridge->memory();
  constexpr std::uint32_t kOwner = 0x2E3CED10U;
  const auto read_u32 = [&](std::uint32_t address) noexcept {
    return address != 0U && memory.mapped(address, 4U)
               ? memory.load_u32(address)
               : 0U;
  };
  const auto object = read_u32(kOwner + 16U);
  if (context.r3.u32 != kOwner && context.r3.u32 != object) {
    return;
  }
  const auto vtable = read_u32(context.r3.u32);
  const auto slot_address =
      vtable <= std::numeric_limits<std::uint32_t>::max() - it->second * 4U
          ? vtable + it->second * 4U
          : 0U;
  const auto slot_target = it->second == 0U ? 0U : read_u32(slot_address);
  std::fprintf(
      stderr,
      "AC6_BRANDLOGO_OWNER_VCALL tick=%llu lr=0x%08X target=0x%08X "
      "thread=%u owner=0x%08X object=0x%08X r3=0x%08X r4=0x%08X "
      "r5=0x%08X vtable=0x%08X slot=%u slot_target=0x%08X "
      "u216=0x%08X u220=0x%08X u224=0x%08X b0=0x%02X b1=0x%02X "
      "b2=0x%02X b3=0x%02X\n",
      static_cast<unsigned long long>(tick), lr, target,
      current_guest_thread_id, kOwner, object, context.r3.u32, context.r4.u32,
      context.r5.u32, vtable, it->second, slot_target,
      read_u32(kOwner + 216U), read_u32(kOwner + 220U),
      read_u32(kOwner + 224U), memory.mapped(kOwner + 212U, 1U)
          ? memory.load_u8(kOwner + 212U)
          : 0U,
      memory.mapped(kOwner + 213U, 1U) ? memory.load_u8(kOwner + 213U) : 0U,
      memory.mapped(kOwner + 214U, 1U) ? memory.load_u8(kOwner + 214U) : 0U,
      memory.mapped(kOwner + 215U, 1U) ? memory.load_u8(kOwner + 215U) : 0U);
}

static void trace_brandlogo_owner_store(const PPCContext &context,
                                        std::uint32_t address,
                                        std::uint32_t size,
                                        std::uint32_t value,
                                        const char *generated_name,
                                        std::uint32_t generated_line) noexcept {
  static const bool on =
      std::getenv("AC6_DEMO_WATCH_BRANDLOGO_OWNER_STORES") != nullptr;
  if (!on || active_bridge == nullptr || generated_name == nullptr) {
    return;
  }
  const auto tick = active_bridge->tick();
  if (tick < 220U || tick > 240U) {
    return;
  }
  constexpr std::uint32_t kOwner = 0x2E3CED10U;
  if (address < kOwner + 212U || address >= kOwner + 225U || size == 0U ||
      address > std::numeric_limits<std::uint32_t>::max() - size ||
      address + size <= kOwner + 212U) {
    return;
  }
  std::fprintf(stderr,
               "AC6_BRANDLOGO_OWNER_STORE tick=%llu address=0x%08X "
               "size=%u value=0x%08X offset=0x%X lr=0x%08X "
               "function=%s line=%u thread=%u\n",
               static_cast<unsigned long long>(tick), address, size, value,
               address - kOwner, static_cast<std::uint32_t>(context.lr),
               generated_name, generated_line, current_guest_thread_id);
}

static void trace_as_context_counter(const PPCContext &context,
                                     std::uint32_t lr,
                                     std::uint32_t target) {
  const auto tick = require_bridge().tick();
  if (std::getenv("AC6_DEMO_WATCH_AS_CONTEXT_COUNTER") == nullptr ||
      lr != 0x820D3AF0U || tick < 3000U || tick > 3030U) {
    return;
  }
  std::fprintf(stderr,
               "AC6_AS_CONTEXT_COUNTER tick=%llu target=0x%08X "
               "context=0x%08X counter_owner=0x%08X\n",
               static_cast<unsigned long long>(tick), target,
               context.r3.u32, context.r31.u32);
}

// Per load and per store: getenv must stay cached here. Uncached, perf put 45%
// of a live probe's CPU in getenv while every trace using it was disabled.
static void trace_as_context_counter_access(const PPCContext &context,
                                            std::uint32_t address,
                                            std::uint32_t value,
                                            const char *kind) {
  static const bool on =
      std::getenv("AC6_DEMO_WATCH_AS_CONTEXT_COUNTER") != nullptr;
  const auto tick = require_bridge().tick();
  if (!on || context.lr != 0x820D3AF0U || tick < 3000U || tick > 3030U) {
    return;
  }
  std::fprintf(stderr,
               "AC6_AS_CONTEXT_COUNTER_%s tick=%llu address=0x%08X "
               "value=0x%08X\n",
               kind, static_cast<unsigned long long>(tick), address, value);
}

static void trace_title_selector(const PPCContext &context, std::uint32_t lr,
                                 std::uint32_t target) {
  const auto tick = require_bridge().tick();
  if (std::getenv("AC6_DEMO_WATCH_TITLE_SELECTOR") == nullptr ||
      (lr != 0x823251B8U && lr != 0x82325250U && lr != 0x82325274U) ||
      tick < 3000U || tick > 3035U) {
    return;
  }
  std::fprintf(stderr,
               "AC6_TITLE_SELECTOR tick=%llu lr=0x%08X target=0x%08X "
               "object=0x%08X r4=0x%08X r10=0x%08X\n",
               static_cast<unsigned long long>(tick), lr, target,
               context.r3.u32, context.r4.u32, context.r10.u32);
}

static void trace_loading_consumer(const PPCContext &context, std::uint32_t lr,
                                   std::uint32_t target) {
  if (std::getenv("AC6_DEMO_WATCH_LOADING_CONSUMER") == nullptr ||
      lr != 0x8218A55CU ||
      !ac6demo::guest_bridge_detail::transition_trace_tick_allowed(
          require_bridge().tick())) {
    return;
  }
  static thread_local std::uint32_t record_count = 0U;
  if (record_count >= 256U) {
    return;
  }
  ++record_count;
  auto &memory = require_bridge().memory();
  const auto object = context.r3.u32;
  std::uint32_t vtable = 0U;
  std::uint32_t slot15 = 0U;
  const bool object_mapped = object != 0U && memory.mapped(object, 4U);
  if (object_mapped) {
    vtable = memory.load_u32(object);
    if (vtable <= std::numeric_limits<std::uint32_t>::max() - 60U &&
        memory.mapped(vtable + 60U, 4U)) {
      slot15 = memory.load_u32(vtable + 60U);
    }
  }
  std::fprintf(stderr,
               "AC6_LOADING_CONSUMER lr=0x%08X target=0x%08X "
               "tick=%llu thread=%u object=0x%08X object_mapped=%u "
               "vtable=0x%08X slot15=0x%08X r4=0x%08X\n",
               lr, target, static_cast<unsigned long long>(require_bridge().tick()),
               current_guest_thread_id, object, object_mapped ? 1U : 0U,
               vtable, slot15, context.r4.u32);
}

static void trace_swg_callback_arm(const PPCContext &context,
                                   std::uint32_t lr,
                                   std::uint32_t target) {
  if (std::getenv("AC6_DEMO_WATCH_SWG_CALLBACK") == nullptr ||
      target != 0x820CDF30U) {
    return;
  }
  static thread_local std::uint32_t record_count = 0U;
  if (record_count >= 128U) {
    return;
  }
  ++record_count;
  auto &memory = require_bridge().memory();
  const auto object = context.r3.u32;
  std::uint32_t vtable = 0U;
  std::uint32_t owner = 0U;
  std::uint32_t ready = 0U;
  std::uint32_t armed = 0U;
  const bool mapped = object != 0U && memory.mapped(object, 10U);
  if (mapped) {
    vtable = memory.load_u32(object);
    owner = memory.load_u32(object + 4U);
    ready = memory.load_u8(object + 8U);
    armed = memory.load_u8(object + 9U);
  }
  const auto global = 0x826DFC48U;
  const auto global_value = memory.mapped(global, 1U) ? memory.load_u8(global) : 0U;
  std::fprintf(stderr,
               "AC6_SWG_CALLBACK_ARM tick=%llu lr=0x%08X target=0x%08X "
               "object=0x%08X mapped=%u vtable=0x%08X owner=0x%08X "
               "ready=%u armed=%u global_826DFC48=%u\n",
               static_cast<unsigned long long>(require_bridge().tick()), lr,
               target, object, mapped ? 1U : 0U, vtable, owner, ready, armed,
               global_value);
}

extern "C" void AC6_PPC_CALL_INDIRECT(PPCContext &context, std::uint8_t *base, std::uint32_t guest_address) {
  struct QualifiedVirtualDispatchSite final {
    std::uint32_t lr;
    std::array<std::uint32_t, 6> slots;
    std::size_t slot_count;
  };
  // Exact canonical-Ghidra bctrl return addresses; never a generic LR lookup.
  constexpr std::array kQualifiedVirtualDispatchSites{
      QualifiedVirtualDispatchSite{0x8216D79CU, {13U}, 1U}, QualifiedVirtualDispatchSite{0x8216D7E8U, {14U}, 1U}, QualifiedVirtualDispatchSite{0x8219EAE8U, {1U}, 1U}, QualifiedVirtualDispatchSite{0x8219ECBCU, {3U}, 1U}, QualifiedVirtualDispatchSite{0x8219EE94U, {3U}, 1U}, QualifiedVirtualDispatchSite{0x8219ED34U, {1U}, 1U}, QualifiedVirtualDispatchSite{0x8219ED64U, {1U}, 1U}, QualifiedVirtualDispatchSite{0x8219ED78U, {5U}, 1U}, QualifiedVirtualDispatchSite{0x8219ED94U, {14U}, 1U}, QualifiedVirtualDispatchSite{0x8219EDA8U, {12U}, 1U}, QualifiedVirtualDispatchSite{0x8219EDC0U, {14U}, 1U}, QualifiedVirtualDispatchSite{0x8219EDD4U, {12U}, 1U}, QualifiedVirtualDispatchSite{0x8219EE10U, {11U}, 1U}, QualifiedVirtualDispatchSite{0x8219EE34U, {1U}, 1U}, QualifiedVirtualDispatchSite{0x8219EF4CU, {6U}, 1U}, QualifiedVirtualDispatchSite{0x8219EFA0U, {2U}, 1U}, QualifiedVirtualDispatchSite{0x8219EFC0U, {8U}, 1U}, QualifiedVirtualDispatchSite{0x8219F058U, {9U}, 1U}, QualifiedVirtualDispatchSite{0x8219F06CU, {1U}, 1U},
      QualifiedVirtualDispatchSite{0x821679B0U, {4U}, 1U},
      QualifiedVirtualDispatchSite{0x8218A3ACU, {13U}, 1U},
      QualifiedVirtualDispatchSite{0x8219F014U, {4U}, 1U}, QualifiedVirtualDispatchSite{0x8219F10CU, {2U}, 1U}, QualifiedVirtualDispatchSite{0x8219F124U, {7U}, 1U}, QualifiedVirtualDispatchSite{0x8219F13CU, {9U}, 1U}, QualifiedVirtualDispatchSite{0x8219F188U, {4U}, 1U}, QualifiedVirtualDispatchSite{0x8219F24CU, {2U}, 1U}, QualifiedVirtualDispatchSite{0x8219F264U, {7U}, 1U}, QualifiedVirtualDispatchSite{0x8219F27CU, {9U}, 1U}, QualifiedVirtualDispatchSite{0x8219F2C8U, {4U}, 1U}, QualifiedVirtualDispatchSite{0x8219F35CU, {6U}, 1U}, QualifiedVirtualDispatchSite{0x8219F3B0U, {2U}, 1U}, QualifiedVirtualDispatchSite{0x8219F3C8U, {7U}, 1U}, QualifiedVirtualDispatchSite{0x8219F3E0U, {9U}, 1U}, QualifiedVirtualDispatchSite{0x8219F42CU, {4U}, 1U}, QualifiedVirtualDispatchSite{0x8219F4C4U, {6U}, 1U}, QualifiedVirtualDispatchSite{0x8219F518U, {2U}, 1U}, QualifiedVirtualDispatchSite{0x8219F530U, {7U}, 1U}, QualifiedVirtualDispatchSite{0x8219F548U, {9U}, 1U}, QualifiedVirtualDispatchSite{0x8219F594U, {4U}, 1U},
      QualifiedVirtualDispatchSite{0x821A014CU, {1U}, 1U}, QualifiedVirtualDispatchSite{0x821A01F0U, {1U}, 1U}, QualifiedVirtualDispatchSite{0x821A02A0U, {1U}, 1U}, QualifiedVirtualDispatchSite{0x821A0324U, {1U}, 1U}, QualifiedVirtualDispatchSite{0x821A0398U, {1U}, 1U}, QualifiedVirtualDispatchSite{0x821A0410U, {1U}, 1U},
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
  trace_brandlogo_owner_vcall(context, lr, guest_address);
  trace_dynamic_object_vtable(context, lr, guest_address); trace_title_matrix_consumer(context, lr, guest_address); trace_swg_native_call(context, lr, guest_address);
  trace_movie_frame_dispatch(context, lr, guest_address); trace_as_context_counter(context, lr, guest_address); trace_title_selector(context, lr, guest_address);
  trace_loading_consumer(context, lr, guest_address);
  trace_swg_callback_arm(context, lr, guest_address);
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
    const auto resource_index_watch =
        std::getenv("AC6_DEMO_WATCH_RESOURCE_INDEX") != nullptr &&
        require_bridge().tick() >= 220U && require_bridge().tick() <= 240U &&
        (lr == 0x820D5294U || lr == 0x820D52BCU || lr == 0x820D5308U);
    const auto log_resource_index_call = [&](const char *phase) {
      if (!resource_index_watch) {
        return;
      }
      auto &memory = require_bridge().memory();
      const auto object = lr == 0x820D52BCU ? context.r4.u32 : context.r3.u32;
      const auto vtable = object != 0U && memory.mapped(object, 4U)
                              ? memory.load_u32(object)
                              : 0U;
      const auto slot = lr == 0x820D5294U ? 6U
                         : lr == 0x820D52BCU ? 21U
                                             : 23U;
      const auto slot_address =
          vtable <= std::numeric_limits<std::uint32_t>::max() - slot * 4U
              ? vtable + slot * 4U
              : 0U;
      const auto slot_target = slot_address != 0U &&
                                       memory.mapped(slot_address, 4U)
                                   ? memory.load_u32(slot_address)
                                   : 0U;
      const auto result = context.r3.u32;
      const auto result_p4 = result != 0U && memory.mapped(result + 4U, 4U)
                                 ? memory.load_u32(result + 4U)
                                 : 0U;
      const auto result_p24 = result != 0U && memory.mapped(result + 24U, 4U)
                                  ? memory.load_u32(result + 24U)
                                  : 0U;
      std::fprintf(
          stderr,
          "AC6_RESOURCE_INDEX_CALL phase=%s tick=%llu lr=0x%08X "
          "target=0x%08X object=0x%08X vtable=0x%08X slot=%u "
          "slot_target=0x%08X r3=0x%08X r4=0x%08X r5=0x%08X "
          "r29=0x%08X r31=0x%08X result=0x%08X result_p4=0x%08X "
          "result_p24=0x%08X\n",
          phase, static_cast<unsigned long long>(require_bridge().tick()), lr,
          guest_address, object, vtable, slot, slot_target, context.r3.u32,
          context.r4.u32, context.r5.u32, context.r29.u32, context.r31.u32,
          result, result_p4, result_p24);
    };
    log_resource_index_call("before");
    trace_loading_resource_poll_call(context, lr, guest_address, [&] {
      invoke_body_trace_with_swg_msgi_override(function, context, base,
                                               guest_address, lr);
    });
    log_resource_index_call("after");
    return;
  }
  throw ac6demo::RuntimeTrap("unqualified guest indirect call",
                             require_bridge().tick(), lr, guest_address);
}
#include "guest_bridge/civil_time.hpp"
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
[[nodiscard]] std::uint64_t decode_wait_deadline(GuestBridge &bridge, std::uint32_t timeout_pointer) {
  if (timeout_pointer == 0U) {
    return kNoWakeTick;
  }
  auto &memory = bridge.memory();
  if (!memory.mapped(timeout_pointer, 8U)) {
    throw ac6demo::RuntimeTrap("guest wait timeout is not mapped",
                               bridge.tick(), 0, timeout_pointer);
  }
  const auto timeout = static_cast<std::int64_t>(memory.load_u64(timeout_pointer));
  if (timeout > 0) {
    throw ac6demo::RuntimeTrap("unqualified absolute guest wait timeout",
                               bridge.tick(), 0, timeout_pointer);
  }
  const auto magnitude = timeout == std::numeric_limits<std::int64_t>::min() ? std::numeric_limits<std::uint64_t>::max()
                                                                                : static_cast<std::uint64_t>(-timeout);
  if (magnitude > kNoWakeTick - (kHundredNanosecondsPerGuestTick - 1U)) {
    return kNoWakeTick;
  }
  const auto ticks = std::max<std::uint64_t>(1U, (magnitude + kHundredNanosecondsPerGuestTick - 1U) /
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
  const auto magnitude = due_time == std::numeric_limits<std::int64_t>::min() ? std::numeric_limits<std::uint64_t>::max()
                                                                                 : static_cast<std::uint64_t>(-due_time);
  if (magnitude > kNoWakeTick - (kHundredNanosecondsPerGuestTick - 1U)) {
    return kNoWakeTick;
  }
  const auto ticks = std::max<std::uint64_t>(1U, (magnitude + kHundredNanosecondsPerGuestTick - 1U) /
                                                      kHundredNanosecondsPerGuestTick);
  if (bridge.tick() > kNoWakeTick - ticks) {
    return kNoWakeTick;
  }
  return bridge.tick() + ticks;
}
[[nodiscard]] std::uint64_t timer_period_to_ticks(std::uint32_t period_ms) noexcept {
  if (period_ms == 0U) {
    return 0U;
  }
  const auto duration = static_cast<std::uint64_t>(period_ms) * 1'000'000U;
  return std::max<std::uint64_t>(
      1U, (duration + kHundredNanosecondsPerGuestTick - 1U) /
              kHundredNanosecondsPerGuestTick);
}
[[nodiscard]] bool dispatch_import_qualified(PPCContext &context,
                                             const char *module,
                                             const char *name,
                                             std::uint16_t ordinal) {
  current_import_lr = static_cast<std::uint32_t>(context.lr);
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
#include "guest_bridge/import_journal.hpp"
} // namespace

#include "guest_bridge/title_terminal_trace_override.hpp"
#include "guest_bridge/qualified_loader_overrides.hpp"
#include "guest_key_schedule.inl"
#include "guest_mission_consumers.inl"

// clang-format off
#include "guest_bridge/kernel_data_imports.hpp"
#include "guest_bridge/constructor.hpp"
#include "guest_bridge/scheduler.hpp"
#include "guest_bridge/point_draw_trace.hpp"
#include "guest_bridge/graphics_ring.hpp"
#include "guest_bridge/graphics_mmio_cpu.hpp"
#include "guest_bridge/frontend_state_trace.hpp"
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
