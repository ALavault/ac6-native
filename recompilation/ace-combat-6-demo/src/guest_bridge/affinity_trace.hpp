#pragma once

#include "ac6demo/xenon_affinity_contract.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Legacy read-only trace retained for the byte-exact original dispatch
// fragment. Its previous_ptr fields describe the old bridge interpretation,
// not the Xbox ABI.
inline void trace_affinity_call(const PPCContext &context,
                                std::uint32_t object,
                                std::uint32_t previous_value,
                                bool previous_mapped,
                                std::uint32_t result) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_AFFINITY") != nullptr;
  static std::uint32_t record_count = 0U;
  if (!enabled || record_count >= 4096U) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_AFFINITY_LEGACY object=0x%08X raw_r4=0x%08X stale_r5=0x%08X "
      "legacy_value=0x%08X legacy_mapped=%u result=0x%08X "
      "tick=%llu thread=%u lr=0x%08X\n",
      object, context.r4.u32, context.r5.u32, previous_value,
      previous_mapped ? 1U : 0U, result,
      static_cast<unsigned long long>(require_bridge().tick()),
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr));
}

inline void trace_affinity_transition(
    const PPCContext &context, std::uint32_t object,
    const ac6demo::XenonAffinityTransition &transition,
    std::uint32_t result) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_AFFINITY") != nullptr;
  static std::uint32_t record_count = 0U;
  if (!enabled || record_count >= 4096U) {
    return;
  }
  ++record_count;
  std::fprintf(
      stderr,
      "AC6_AFFINITY_TRANSITION object=0x%08X requested_mask=0x%08X "
      "previous_mask=0x%08X previous_processor=%u requested_processor=%u "
      "stale_r5=0x%08X result=0x%08X tick=%llu thread=%u lr=0x%08X\n",
      object, transition.requested_mask, transition.previous_mask,
      static_cast<unsigned int>(transition.previous_processor),
      static_cast<unsigned int>(transition.requested_processor), context.r5.u32,
      result, static_cast<unsigned long long>(require_bridge().tick()),
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr));
}
