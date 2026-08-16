#pragma once

#ifdef AC6_DEMO_GENERATED_GUEST

#include "ac6demo/guest_memory.hpp"
#include "ppc_context_base.h"

#include <array>
#include <cstdint>
#include <limits>
#include <ranges>

namespace ac6demo::guest_bridge_detail {

// This is the only reached callable entry that Ghidra classifies as a branch
// delay slot.  Its four PAL words and the vtable offset are checked at runtime;
// the caller supplies the normal qualified function lookup/invocation path.
template <typename Lookup, typename Invoke>
bool dispatch_reached_branch_delay_thunk(
    PPCContext &context, std::uint8_t *base, std::uint32_t guest_address,
    GuestMemory &memory, std::uint64_t tick, std::uint32_t lr, Lookup &&lookup,
    Invoke &&invoke) {
  constexpr std::uint32_t kThunk = 0x820D3310U;
  if (guest_address != kThunk) {
    return false;
  }
  constexpr std::array<std::uint32_t, 4U> kWords{
      0x81830000U, 0x816C0064U, 0x7D6903A6U, 0x4E800420U};
  if (!memory.mapped(kThunk, 16U) ||
      !std::ranges::equal(
          kWords, std::array<std::uint32_t, 4U>{memory.load_u32(kThunk),
                                                memory.load_u32(kThunk + 4U),
                                                memory.load_u32(kThunk + 8U),
                                                memory.load_u32(kThunk + 12U)})) {
    throw RuntimeTrap("qualified branch-delay thunk bytes changed", tick, lr,
                      guest_address);
  }
  const auto object = context.r3.u32;
  if (object == 0U || !memory.mapped(object, 4U)) {
    throw RuntimeTrap("qualified branch-delay thunk object is not mapped", tick,
                      lr, object);
  }
  const auto vtable = memory.load_u32(object);
  if (vtable == 0U ||
      vtable > std::numeric_limits<std::uint32_t>::max() - 0x64U ||
      !memory.mapped(vtable + 0x64U, 4U)) {
    throw RuntimeTrap("qualified branch-delay thunk vtable slot is not mapped",
                      tick, lr, vtable);
  }
  const auto slot_target = memory.load_u32(vtable + 0x64U);
  if (slot_target == 0U || slot_target == kThunk || lookup(slot_target) == nullptr) {
    throw RuntimeTrap("qualified branch-delay thunk target is not qualified", tick,
                      lr, slot_target);
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
