#pragma once

// sub_820E8F90's own native-call dispatch: mtctr r10=[r23+12]; bctrl, with
// r23 still holding the 16-byte command-table row (table_base +
// command_index*16) it was assigned to at function entry and never
// reassigned before this call. Naming the target and the row per call is
// the cheapest way to find which of this function's 9 calls (12000-tick
// atlas) is the one that resolves to sub_820EA4A8 for startup, and what
// (if anything) title's own script issues instead.
inline void trace_swg_native_call(const PPCContext &context, std::uint32_t lr,
                                   std::uint32_t guest_address) {
  if (std::getenv("AC6_DEMO_WATCH_SWG_NATIVE_CALL") == nullptr ||
      lr != 0x820E9130U) {
    return;
  }
  const auto marshaled_args = context.r26.u32;
  const auto first_arg = marshaled_args != 0
                              ? require_bridge().memory().load_u32(marshaled_args)
                              : 0U;
  // sub_820E9838 (SendMsgI) treats first_arg itself as a pointer to a
  // 4-byte tag (sub_820E9388: byte0 must be 0x4D='M', bytes 1-3 nonzero,
  // else the message is rejected before any listener runs). Dereference it
  // here so a rejected-tag call is visible without a second pass.
  char tag_bytes[5] = {0, 0, 0, 0, 0};
  if (guest_address == 0x820E9838U && first_arg != 0U) {
    for (int i = 0; i < 4; ++i) {
      const auto b = require_bridge().memory().load_u8(first_arg + i);
      tag_bytes[i] = (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.';
    }
  }
  std::fprintf(stderr,
               "AC6_SWG_NATIVE_CALL tick=%llu thread=%u target=0x%08X "
               "table_row=0x%08X context=0x%08X arg_count=%u args=0x%08X "
               "first_arg=0x%08X tag=%s\n",
               static_cast<unsigned long long>(require_bridge().tick()),
               current_guest_thread_id, guest_address, context.r23.u32,
               context.r31.u32, context.r28.u32, marshaled_args, first_arg,
               tag_bytes);
  // sub_820CE010 bounds this same array to 16 slots (base 0x826DF800,
  // 64 bytes / 4). Only two of them (startup's and title's own listener)
  // have ever been read for a slot-+0x20 implementation; dump every
  // populated slot's object and vtable pointer here so a third registrant
  // isn't missed.
  if (std::getenv("AC6_DEMO_WATCH_SWG_LISTENER_ARRAY") != nullptr &&
      guest_address == 0x820E9838U) {
    auto &memory = require_bridge().memory();
    for (std::uint32_t slot = 0; slot < 16U; ++slot) {
      const auto object = memory.load_u32(0x826DF800U + slot * 4U);
      if (object == 0U) {
        continue;
      }
      const auto vtable = memory.load_u32(object);
      std::fprintf(stderr,
                   "AC6_SWG_LISTENER_SLOT tick=%llu slot=%u object=0x%08X "
                   "vtable=0x%08X\n",
                   static_cast<unsigned long long>(require_bridge().tick()),
                   slot, object, vtable);
    }
  }
}
