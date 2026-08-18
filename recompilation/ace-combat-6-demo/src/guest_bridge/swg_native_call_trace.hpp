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
  // Every reachable call to sub_820E8F90 (the marshaller) arrives through
  // this same indirect-call path -- it has no direct `bl` callers anywhere
  // in the image. Naming `lr` here, independent of the lr==0x820E9130 gate
  // below (that gate is for calls *out of* the marshaller, this is for
  // calls *into* it), identifies the swg VM loop itself: the standing
  // "where does the interpreter live" question, attacked live.
  if (std::getenv("AC6_DEMO_WATCH_SWG_VM_LOOP_CALL") != nullptr &&
      guest_address == 0x820E8F90U) {
    std::fprintf(stderr,
                 "AC6_SWG_VM_LOOP_CALL tick=%llu thread=%u lr=0x%08X "
                 "r1=0x%08X r3=0x%08X r31=0x%08X\n",
                 static_cast<unsigned long long>(require_bridge().tick()),
                 current_guest_thread_id, lr, context.r1.u32, context.r3.u32,
                 context.r31.u32);
  }
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

// Falsifier for AC6_DEMO_M102_RESOLVES_TO_A_QUERY_NOBODY_CURRENTLY_ANSWERS.md's
// named next step: force SendMsgI's boxed "handled" result to a chosen
// value instead of the 0 every currently-registered listener leaves it
// at, and watch live whether title's script goes on to call
// sub_820EA4A8 (the completion trigger, 642f77a4). Unlike every other
// instrument in this file, this one WRITES guest memory -- it exists to
// test a hypothesis, not to observe one, and is opt-in on its own env
// var, off by default.
inline void apply_swg_sendmsgi_override(std::uint32_t out_param_address) {
  static const char *forced_str = std::getenv("AC6_DEMO_FORCE_SWG_MSGI_RESULT");
  if (forced_str == nullptr || out_param_address == 0U) {
    return;
  }
  static const auto forced_value =
      static_cast<std::uint32_t>(std::strtoul(forced_str, nullptr, 0));
  require_bridge().memory().store_u32(out_param_address, forced_value);
  std::fprintf(stderr,
               "AC6_SWG_MSGI_FORCED tick=%llu address=0x%08X value=%u\n",
               static_cast<unsigned long long>(require_bridge().tick()),
               out_param_address, forced_value);
}

// AC6_DEMO_CORRECTING_GETCURRENTMISSION_IS_ALWAYS_16_THE_BRANCH_WAS_READ_BACKWARDS.md's
// named next step: sub_820EA550's gate (sub_820E9300) always fails in this
// run ([sub112+8] never 1), so GetCurrentMission always returns
// sub_82095B80's raw value (observed live: 0), never the gate-success
// sentinel 16. Force the boxed result to 16 -- the one value this campaign
// has never seen live -- and watch whether title's script goes on to call
// sub_820EA4A8 (the completion trigger). Same write-only, opt-in shape as
// apply_swg_sendmsgi_override.
inline void apply_swg_mission_override(std::uint32_t out_param_address) {
  static const char *forced_str = std::getenv("AC6_DEMO_FORCE_SWG_MISSION_RESULT");
  if (forced_str == nullptr || out_param_address == 0U) {
    return;
  }
  static const auto forced_value =
      static_cast<std::uint32_t>(std::strtoul(forced_str, nullptr, 0));
  require_bridge().memory().store_u32(out_param_address, forced_value);
  std::fprintf(stderr,
               "AC6_SWG_MISSION_FORCED tick=%llu address=0x%08X value=%u\n",
               static_cast<unsigned long long>(require_bridge().tick()),
               out_param_address, forced_value);
}

// SendMsgI's and GetCurrentMission's out-param pointer (both r4, per the
// marshaller's uniform box-the-result-through-r4 convention confirmed by
// direct read of sub_820EA538/sub_820EA550's own generated bodies) has to
// be captured before the call -- callees are free to clobber r4 -- and
// applied after, once the call's own store has already happened and can be
// overwritten before the marshaller reads it back for boxing.
template <typename Function>
inline void invoke_body_trace_with_swg_msgi_override(
    Function function, PPCContext &context, std::uint8_t *base,
    std::uint32_t guest_address, std::uint32_t lr) {
  const bool is_send_msg_i =
      lr == 0x820E9130U && guest_address == 0x820E9838U;
  const bool is_get_mission =
      lr == 0x820E9130U && guest_address == 0x820EA550U;
  const auto out_param_address =
      (is_send_msg_i || is_get_mission) ? context.r4.u32 : 0U;
  invoke_body_trace(function, context, base, guest_address);
  if (is_send_msg_i) {
    apply_swg_sendmsgi_override(out_param_address);
  } else if (is_get_mission) {
    apply_swg_mission_override(out_param_address);
  }
}
