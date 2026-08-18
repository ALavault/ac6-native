#pragma once

#ifdef AC6_DEMO_GENERATED_GUEST

#include "ac6demo/guest_memory.hpp"
#include "ppc_context_base.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <ranges>

namespace ac6demo::guest_bridge_detail {

// A family of 16-byte virtual-call trampolines, which Ghidra classifies as
// branch delay slots rather than function starts.
//
// This is now a fallback rather than the mechanism. 478e31ed declared 195
// interior entries, and of the 28 trampolines in the image 13 became emitted
// functions, so lookup_guest_function resolves them and they never reach
// here. The remaining 15 are executed by neither this port nor the oracle, so
// in practice nothing dispatches through this path today. It is kept because
// it is a shape check rather than an address list -- it costs four word
// comparisons and it catches a trampoline the boundary set ever stops
// declaring, which is exactly the anomaly worth failing closed on.
//
// Each is literally four words:
//
//     lwz   r12,0(rX)        ; 0x81830000 (r3) or 0x81840000 (r4)
//     lwz   r11,OFF(r12)     ; 0x816C0000 | OFF
//     mtctr r11              ; 0x7D6903A6
//     bctr                   ; 0x4E800420
//
// so the object register and the vtable offset are read out of the bytes at
// the entry rather than listed here. Three neighbours in one Ghidra chunk
// make the point: 0x820D3230 is r4 slot 0x54, 0x820D32D0 is r3 slot 0x70,
// 0x820D3310 is r3 slot 0x64. This used to qualify 0x820D3310 alone, with
// its offset written in as a constant; pressing START during the title
// screen reaches 0x820D32D0 and would have needed a second copy, and the
// next screen a third. What is qualified is the shape, and every check the
// single-address version made is still made: the four words are verified at
// dispatch, the object and vtable must be mapped, and the slot target must
// be a function this build knows.
template <typename Lookup, typename Invoke>
bool dispatch_reached_branch_delay_thunk(
    PPCContext &context, std::uint8_t *base, std::uint32_t guest_address,
    GuestMemory &memory, std::uint64_t tick, std::uint32_t lr, Lookup &&lookup,
    Invoke &&invoke) {
  if ((guest_address & 3U) != 0U || !memory.mapped(guest_address, 16U)) {
    return false;
  }
  const auto load_object = memory.load_u32(guest_address);
  const auto load_slot = memory.load_u32(guest_address + 4U);
  if ((load_object != 0x81830000U && load_object != 0x81840000U) ||
      (load_slot & 0xFFFF0000U) != 0x816C0000U ||
      memory.load_u32(guest_address + 8U) != 0x7D6903A6U ||
      memory.load_u32(guest_address + 12U) != 0x4E800420U) {
    return false;
  }
  const auto slot_offset = load_slot & 0xFFFFU;
  const auto object =
      load_object == 0x81830000U ? context.r3.u32 : context.r4.u32;
  if (object == 0U || !memory.mapped(object, 4U)) {
    throw RuntimeTrap("qualified branch-delay thunk object is not mapped", tick,
                      lr, object);
  }
  const auto vtable = memory.load_u32(object);
  if (vtable == 0U ||
      vtable > std::numeric_limits<std::uint32_t>::max() - slot_offset - 4U ||
      !memory.mapped(vtable + slot_offset, 4U)) {
    throw RuntimeTrap("qualified branch-delay thunk vtable slot is not mapped",
                      tick, lr, vtable);
  }
  const auto slot_target = memory.load_u32(vtable + slot_offset);
  if (slot_target == 0U || slot_target == guest_address ||
      lookup(slot_target) == nullptr) {
    throw RuntimeTrap("qualified branch-delay thunk target is not qualified",
                      tick, lr, slot_target);
  }
  // Opt-in: which object, which slot, which target. A trampoline dispatch is
  // where a virtual call becomes visible to this runtime, and the START press
  // during the title screen arrives through one.
  if (std::getenv("AC6_DEMO_WATCH_THUNK") != nullptr) {
    std::fprintf(stderr,
                 "AC6_THUNK tick=%llu thunk=0x%08X object=0x%08X "
                 "vtable=0x%08X slot=0x%02X target=0x%08X lr=0x%08X\n",
                 static_cast<unsigned long long>(tick), guest_address, object,
                 vtable, slot_offset, slot_target, lr);
  }
  context.r12.u64 = vtable;
  context.r11.u64 = slot_target;
  context.ctr.u64 = slot_target;
  invoke(context, base, slot_target);
  return true;
}

// The PAL image contains two callable entries in one 16-byte Ghidra chunk.
// This helper qualifies only the reached second entry; it deliberately does
// not create or imply a new static function boundary.
template <typename Lookup, typename Invoke>
bool dispatch_reached_chunk_entry(
    PPCContext &context, std::uint8_t *base, std::uint32_t guest_address,
    GuestMemory &memory, std::uint64_t tick, std::uint32_t lr, Lookup &&lookup,
    Invoke &&invoke) {
  constexpr std::uint32_t kChunk = 0x820E7E00U;
  constexpr std::uint32_t kEntry = 0x820E7E08U;
  constexpr std::uint32_t kBranchTarget = 0x820E1F78U;
  if (guest_address != kEntry) {
    return false;
  }
  constexpr std::array<std::uint32_t, 4U> kWords{
      0x38A00001U, 0x4BFFA174U, 0x38A00000U, 0x4BFFA16CU};
  if (!memory.mapped(kChunk, 16U) ||
      !std::ranges::equal(
          kWords, std::array<std::uint32_t, 4U>{
                      memory.load_u32(kChunk), memory.load_u32(kChunk + 4U),
                      memory.load_u32(kChunk + 8U),
                      memory.load_u32(kChunk + 12U)})) {
    throw RuntimeTrap("qualified chunk entry bytes changed", tick, lr,
                      guest_address);
  }
  const auto function = lookup(kBranchTarget);
  if (function == nullptr) {
    throw RuntimeTrap("qualified chunk entry branch target is not qualified",
                      tick, lr, kBranchTarget);
  }
  context.r5.u64 = 0U;
  invoke(context, base, kBranchTarget);
  return true;
}

} // namespace ac6demo::guest_bridge_detail

#endif
