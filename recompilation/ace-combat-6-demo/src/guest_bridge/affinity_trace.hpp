#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Read-only affinity evidence.  It is disabled unless explicitly requested
// and does not retain or reinterpret the raw Xenon value.
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
      "AC6_AFFINITY object=0x%08X raw_r4=0x%08X previous_ptr=0x%08X "
      "previous_value=0x%08X previous_mapped=%u result=0x%08X "
      "tick=%llu thread=%u lr=0x%08X processor=unknown\n",
      object, context.r4.u32, context.r5.u32, previous_value,
      previous_mapped ? 1U : 0U, result,
      static_cast<unsigned long long>(require_bridge().tick()),
      current_guest_thread_id, static_cast<std::uint32_t>(context.lr));
}
