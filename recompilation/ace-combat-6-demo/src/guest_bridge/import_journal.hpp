#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Read-only ordered import journal. record_import_edge aggregates per
// (thread, lr, module, ordinal) and keeps only the last register snapshot, so
// it cannot answer "in what order were the imports called". This prints one
// line per call and one per return; it is opt-in, writes only to stderr and
// touches no guest state, so an enabled run stays byte-identical on replay.
[[nodiscard]] bool dispatch_import(PPCContext &context, const char *module,
                                   const char *name, std::uint16_t ordinal) {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_IMPORTS") != nullptr;
  if (!enabled) {
    return dispatch_import_qualified(context, module, name, ordinal);
  }
  const auto thread = current_guest_thread_id;
  std::fprintf(stderr,
               "AC6_IMPORT_CALL tick=%llu thread=%u lr=0x%08X module=%s "
               "ordinal=%u name=%s r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X "
               "r7=0x%08X r8=0x%08X r9=0x%08X r10=0x%08X\n",
               static_cast<unsigned long long>(require_bridge().tick()), thread,
               static_cast<std::uint32_t>(context.lr), module,
               static_cast<unsigned>(ordinal), name, context.r3.u32,
               context.r4.u32, context.r5.u32, context.r6.u32, context.r7.u32,
               context.r8.u32, context.r9.u32, context.r10.u32);
  const bool handled =
      dispatch_import_qualified(context, module, name, ordinal);
  // The return tick differs from the call tick whenever the import blocked,
  // which is the shape a wait diff is looking for.
  std::fprintf(stderr,
               "AC6_IMPORT_RETURN tick=%llu thread=%u ordinal=%u name=%s "
               "handled=%d r3=0x%08X\n",
               static_cast<unsigned long long>(require_bridge().tick()), thread,
               static_cast<unsigned>(ordinal), name, handled ? 1 : 0,
               context.r3.u32);
  return handled;
}
