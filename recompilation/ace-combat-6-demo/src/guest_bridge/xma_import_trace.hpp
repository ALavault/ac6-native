#pragma once

#include "ac6demo/hash.hpp"
#include "ac6demo/guest_memory.hpp"

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace ac6demo::guest_bridge_detail {

inline bool xma_slot_watch_enabled() noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_XMA_SLOT") != nullptr;
  return enabled;
}

inline bool xma_slot_overlaps(std::uint32_t address,
                              std::uint32_t width) noexcept {
  constexpr std::uint32_t kSlot = 0x17360050U;
  constexpr std::uint32_t kSlotEnd = kSlot + 4U;
  if (width == 0U || address > std::numeric_limits<std::uint32_t>::max() -
                              (width - 1U)) {
    return false;
  }
  const auto end = address + width;
  return address < kSlotEnd && end > kSlot;
}

inline std::uint32_t xma_generated_pc(const char *generated_name) noexcept {
  if (generated_name == nullptr) {
    return 0U;
  }
  constexpr std::string_view kPrefix = "__imp__sub_";
  const std::string_view name(generated_name);
  if (!name.starts_with(kPrefix)) {
    return 0U;
  }
  std::uint32_t value = 0U;
  const auto *first = name.data() + kPrefix.size();
  const auto *last = name.data() + name.size();
  const auto result = std::from_chars(first, last, value, 16);
  return result.ec == std::errc{} && result.ptr == last ? value : 0U;
}

inline bool xma_slot_function_of_interest(std::uint32_t pc) noexcept {
  switch (pc) {
  case 0x821A4B70U: // zero-fill helper
  case 0x821A3C30U: // preceding fill caller
  case 0x823273E0U: // FE fill helper
  case 0x82356528U: // XMA table caller
  case 0x82357240U: // output-slot load / import wrapper
    return true;
  default:
    return false;
  }
}

template <typename Context>
inline void trace_xma_slot_function_entry(
    const Context &context, const char *generated_name, std::uint64_t tick,
    std::uint32_t thread) noexcept {
  if (!xma_slot_watch_enabled()) {
    return;
  }
  static thread_local std::uint32_t record_count = 0U;
  if (record_count >= 256U) {
    return;
  }
  const auto pc = xma_generated_pc(generated_name);
  if (!xma_slot_function_of_interest(pc)) {
    return;
  }
  if (pc == 0x823273E0U && !xma_slot_overlaps(context.r3.u32, 1U)) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_XMA_SLOT_ENTRY pc=0x%08X lr=0x%08X tick=%llu thread=%u "
      "r1=0x%08X r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X "
      "r7=0x%08X r10=0x%08X r11=0x%08X function=%s\n",
      pc, static_cast<std::uint32_t>(context.lr),
      static_cast<unsigned long long>(tick), thread, context.r1.u32,
      context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32,
      context.r7.u32, context.r10.u32, context.r11.u32,
      generated_name == nullptr ? "" : generated_name);
}

template <typename Context>
inline void trace_xma_table_entry(const Context &context,
                                  const GuestMemory &memory,
                                  const char *generated_name,
                                  std::uint64_t tick,
                                  std::uint32_t thread) noexcept {
  if (!xma_slot_watch_enabled() || xma_generated_pc(generated_name) !=
                                      0x82357240U) {
    return;
  }
  const auto table = context.r3.u32;
  if (table == 0U || !memory.mapped(table, 12U)) {
    std::fprintf(stderr,
                 "AC6_XMA_TABLE mapped=0 tick=%llu thread=%u table=0x%08X\n",
                 static_cast<unsigned long long>(tick), thread, table);
    return;
  }
  const auto count = memory.load_u32(table);
  const auto flags = memory.load_u32(table + 4U);
  const auto entries = memory.load_u32(table + 8U);
  std::fprintf(
      stderr,
      "AC6_XMA_TABLE mapped=1 tick=%llu thread=%u table=0x%08X "
      "count=0x%08X flags=0x%08X entries=0x%08X\n",
      static_cast<unsigned long long>(tick), thread, table, count, flags,
      entries);
  const auto bounded_count = count < 3U ? count : 3U;
  for (std::uint32_t index = 0U; index < bounded_count; ++index) {
    const auto entry = entries + index * 96U;
    if (entry < entries || !memory.mapped(entry, 96U)) {
      std::fprintf(stderr,
                   "AC6_XMA_TABLE_ENTRY mapped=0 index=%u entry=0x%08X\n",
                   index, entry);
      continue;
    }
    std::fprintf(stderr,
                 "AC6_XMA_TABLE_ENTRY mapped=1 index=%u entry=0x%08X",
                 index, entry);
    for (std::uint32_t offset = 0U; offset < 96U; offset += 4U) {
      std::fprintf(stderr, " %08X", memory.load_u32(entry + offset));
    }
    std::fputc('\n', stderr);
    const auto first = memory.load_u32(entry + 0x1CU);
    const auto second = memory.load_u32(entry + 0x20U);
    for (const auto pointer : {first, second}) {
      if (pointer == 0U || !memory.mapped(pointer, 64U)) {
        std::fprintf(stderr,
                     "AC6_XMA_TABLE_SAMPLE mapped=0 index=%u pointer=0x%08X\n",
                     index, pointer);
        continue;
      }
      const auto sample = memory.load_bytes(pointer, 64U);
      std::fprintf(stderr,
                   "AC6_XMA_TABLE_SAMPLE mapped=1 index=%u pointer=0x%08X "
                   "sha256=%s first=0x%08X\n",
                   index, pointer, Sha256::bytes(sample).c_str(),
                   memory.load_u32(pointer));
    }
  }
}

inline void trace_xma_slot_store(std::uint32_t address, std::uint32_t width,
                                 std::uint64_t value, std::uint64_t tick,
                                 std::uint32_t thread, std::uint32_t lr,
                                 const char *generated_name,
                                 std::uint32_t generated_line) noexcept {
  static thread_local std::uint32_t record_count = 0U;
  if (!xma_slot_watch_enabled() || !xma_slot_overlaps(address, width) ||
      record_count >= 256U) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_XMA_SLOT_STORE pc=0x%08X lr=0x%08X tick=%llu thread=%u "
      "address=0x%08X size=%u value=0x%016llX function=%s line=%u\n",
      xma_generated_pc(generated_name), lr,
      static_cast<unsigned long long>(tick), thread, address, width,
      static_cast<unsigned long long>(value),
      generated_name == nullptr ? "" : generated_name, generated_line);
}

inline void trace_xma_slot_load(std::uint32_t address, std::uint32_t width,
                                std::uint64_t value, std::uint64_t tick,
                                std::uint32_t thread, std::uint32_t lr,
                                const char *generated_name,
                                std::uint32_t generated_line) noexcept {
  static thread_local std::uint32_t record_count = 0U;
  if (!xma_slot_watch_enabled() || !xma_slot_overlaps(address, width) ||
      record_count >= 256U) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_XMA_SLOT_LOAD pc=0x%08X lr=0x%08X tick=%llu thread=%u "
      "address=0x%08X size=%u value=0x%016llX function=%s line=%u\n",
      xma_generated_pc(generated_name), lr,
      static_cast<unsigned long long>(tick), thread, address, width,
      static_cast<unsigned long long>(value),
      generated_name == nullptr ? "" : generated_name, generated_line);
}

inline void trace_xma_address_store(std::uint32_t address,
                                    std::uint32_t value, std::uint64_t tick,
                                    std::uint32_t thread, std::uint32_t lr,
                                    const char *generated_name,
                                    std::uint32_t generated_line) noexcept {
  if (std::getenv("AC6_DEMO_WATCH_XMA_ADDRESS") == nullptr ||
      (address != 0x829DA52CU && address != 0x7FEA1A80U &&
       address != 0x7FEA31E0U)) {
    return;
  }
  std::fprintf(stderr,
               "AC6_XMA_ADDR_STORE tick=%llu thread=%u address=0x%08X "
               "value=0x%08X lr=0x%08X function=%s line=%u\n",
               static_cast<unsigned long long>(tick), thread, address, value,
               lr, generated_name == nullptr ? "" : generated_name,
               generated_line);
}

// Read-only probe for the later PAL aperture families identified by the
// cycle-1718 static join.  This deliberately logs the attempted access before
// guest_memory_access() can trap; it never maps, supplies, or consumes a
// value.  The two ranges cover every possible u16 index in
// ((n >> 5) << 2), while the two singleton addresses are the fixed PAL
// accesses.  Keep this separate from the production XMA address hook so the
// default route remains byte-identical.
inline bool xma_late_address_overlaps(std::uint32_t address,
                                      std::uint32_t width) noexcept {
  if (width == 0U || address > std::numeric_limits<std::uint32_t>::max() -
                                (width - 1U)) {
    return false;
  }
  const auto end = address + width;
  const auto overlaps = [address, end](std::uint32_t first,
                                        std::uint32_t last) noexcept {
    return address < last && end > first;
  };
  return overlaps(0x7FEA1A40U, 0x7FEA3A40U) ||
         overlaps(0x7FEA1940U, 0x7FEA3940U) ||
         overlaps(0x7FEA1804U, 0x7FEA1808U) ||
         overlaps(0x7FEA1818U, 0x7FEA181CU);
}

inline void trace_xma_late_access(
    const char *operation, std::uint32_t address, std::uint32_t width,
    bool has_value, std::uint64_t value, std::uint64_t tick,
    std::uint32_t thread, std::uint32_t lr, const char *generated_name,
    std::uint32_t generated_line) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_XMA_LATE") != nullptr;
  static thread_local std::uint32_t record_count = 0U;
  if (!enabled || !xma_late_address_overlaps(address, width) ||
      record_count >= 512U) {
    return;
  }
  ++record_count;
  const auto pc = xma_generated_pc(generated_name);
  std::fprintf(
      stderr,
      "AC6_XMA_LATE_ACCESS op=%s tick=%llu thread=%u address=0x%08X "
      "size=%u value=%s pc=0x%08X lr=0x%08X function=%s line=%u\n",
      operation == nullptr ? "unknown" : operation,
      static_cast<unsigned long long>(tick), thread, address, width,
      has_value ? "present" : "unknown", pc, lr,
      generated_name == nullptr ? "" : generated_name, generated_line);
  if (has_value) {
    std::fprintf(stderr, "AC6_XMA_LATE_VALUE value=0x%016llX\n",
                 static_cast<unsigned long long>(value));
  }
}

// Read-only, opt-in evidence hook for the first XMA context constructor.  It
// deliberately records the bounded guest descriptor instead of implementing
// the import: the descriptor ABI and the packet source must be qualified from
// PAL bytes before any XMA effect is admitted.
template <typename Context>
inline void trace_xma_create_import(const Context &context, GuestMemory &memory,
                                    const char *module, const char *name,
                                    std::uint16_t ordinal, std::uint64_t tick,
                                    std::uint32_t thread) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_XMA_CREATE") != nullptr;
  static thread_local std::uint32_t record_count = 0U;
  if (!enabled || module == nullptr || name == nullptr ||
      std::string_view(module) != "xboxkrnl.exe" ||
      std::string_view(name) != "XMACreateContext" || ordinal != 548U ||
      record_count >= 32U) {
    return;
  }
  ++record_count;
  const auto descriptor = context.r3.u32;
  std::fprintf(
      stderr,
      "AC6_XMA_CREATE tick=%llu thread=%u lr=0x%08X r3=0x%08X "
      "r4=0x%08X r5=0x%08X r6=0x%08X r7=0x%08X\n",
      static_cast<unsigned long long>(tick), thread,
      static_cast<std::uint32_t>(context.lr), descriptor, context.r4.u32,
      context.r5.u32, context.r6.u32, context.r7.u32);
  if (descriptor == 0U || !memory.mapped(descriptor, 96U)) {
    std::fprintf(stderr,
                 "AC6_XMA_CREATE_DESCRIPTOR mapped=0 address=0x%08X\n",
                 descriptor);
    return;
  }
  std::fprintf(stderr, "AC6_XMA_CREATE_DESCRIPTOR mapped=1 address=0x%08X",
               descriptor);
  for (std::uint32_t offset = 0U; offset < 96U; offset += 4U) {
    std::fprintf(stderr, " %08X", memory.load_u32(descriptor + offset));
  }
  std::fputc('\n', stderr);
}

} // namespace ac6demo::guest_bridge_detail
