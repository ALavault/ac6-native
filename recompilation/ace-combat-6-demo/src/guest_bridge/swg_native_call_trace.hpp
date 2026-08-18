#pragma once

// Reimplements sub_820D4DB8/sub_820D4748/sub_820DFD28's own algorithm
// (read directly, AC6_DEMO_THE_SYMBOL_TABLE_IS_AN_MSVC_MAP_KEYED_BY_TWO_INTEGERS.md
// and AC6_DEMO_CATEGORY_IS_THE_NATIVE_FUNCTION_ID_TITLE_NEVER_EVALUATES_CATEGORY_1.md):
// an MSVC xtree lower_bound descent (node layout: _Left=+0, _Parent=+4,
// _Right=+8, _Isnil=+33, key struct at +12 with category at its own +4 and
// id at its own +12 -- node-relative +16/+24), then an equality check via
// "not(key < found)" given lower_bound's "found >= key" invariant. Ignores
// the comparator's -1-wildcard branches: never needed, no wildcard has
// been seen live, and this is a read-only diagnostic, not a copy of the
// guest's own execution path.
inline std::uint32_t find_swg_symbol_node(ac6demo::GuestMemory &memory,
                                           std::uint32_t root,
                                           std::uint32_t category,
                                           std::uint32_t id) {
  const auto less = [](std::uint32_t cat_a, std::uint32_t id_a,
                        std::uint32_t cat_b, std::uint32_t id_b) {
    return cat_a != cat_b ? cat_a < cat_b : id_a < id_b;
  };
  const auto my_head = memory.load_u32(root + 4U);
  auto candidate = my_head;
  auto node = memory.load_u32(my_head + 4U);
  for (int guard = 0; guard < 64 && memory.load_u8(node + 33U) == 0U; ++guard) {
    const auto node_category = memory.load_u32(node + 16U);
    const auto node_id = memory.load_u32(node + 24U);
    // sub_820D4DB8 calls the comparator as comparator(node_key, search_key)
    // -- node first -- not the other way around; getting this backwards
    // makes every descent go the wrong way and every lookup miss.
    if (less(node_category, node_id, category, id)) {
      node = memory.load_u32(node + 8U);
    } else {
      candidate = node;
      node = memory.load_u32(node + 0U);
    }
  }
  if (candidate == my_head) {
    return 0U;
  }
  const auto candidate_category = memory.load_u32(candidate + 16U);
  const auto candidate_id = memory.load_u32(candidate + 24U);
  return less(category, id, candidate_category, candidate_id) ? 0U : candidate;
}

// f7c4e68f's type check is [[node+28]+4] -- node+28 holds a VALUE POINTER
// (the mapped_type of this node's pair, stored right after the 16-byte key
// struct that starts at node+12), and the type tag is 4 bytes into
// *that* object, not node+32. Collapsing the pointer hop into one offset
// add was this instrument's first-draft bug; corrected here.
inline std::uint32_t resolve_swg_symbol_type_tag(ac6demo::GuestMemory &memory,
                                                   std::uint32_t node) {
  if (node == 0U) {
    return 0U;
  }
  const auto value_pointer = memory.load_u32(node + 28U);
  return value_pointer != 0U ? memory.load_u32(value_pointer + 4U) : 0U;
}

// AC6_DEMO_CATEGORY_IS_THE_NATIVE_FUNCTION_ID_TITLE_NEVER_EVALUATES_CATEGORY_1.md
// left open whether category 1 (the completion trigger) is absent from
// title's script entirely, or present but unreached -- this answers it
// directly by walking every entry the map actually holds, independent of
// what the guest happens to evaluate. In-order traversal, explicit stack
// (guest trees are small; this is a diagnostic bound, not a guess at real
// depth). Also dumps the value object's own [+0]/[+8]/[+12] fields --
// sub_820E8F90 derives r23 (table_base + command_index*16) from this same
// value object, so these fields are the concrete next step to naming every
// registered category by its real command-table row, not just its type.
inline void dump_swg_symbol_table(ac6demo::GuestMemory &memory,
                                   std::uint32_t root, std::uint32_t context,
                                   std::uint32_t context_vtable) {
  const auto my_head = memory.load_u32(root + 4U);
  std::array<std::uint32_t, 64> stack{};
  std::size_t depth = 0;
  auto node = memory.load_u32(my_head + 4U);
  int visited = 0;
  while ((depth > 0U || memory.load_u8(node + 33U) == 0U) && visited < 256) {
    while (memory.load_u8(node + 33U) == 0U && depth < stack.size()) {
      stack[depth++] = node;
      node = memory.load_u32(node + 0U);
    }
    if (depth == 0U) {
      break;
    }
    node = stack[--depth];
    const auto category = memory.load_u32(node + 16U);
    const auto id = memory.load_u32(node + 24U);
    const auto value_pointer = memory.load_u32(node + 28U);
    const auto type_tag =
        value_pointer != 0U ? memory.load_u32(value_pointer + 4U) : 0U;
    const auto value0 =
        value_pointer != 0U ? memory.load_u32(value_pointer + 0U) : 0U;
    const auto value8 =
        value_pointer != 0U ? memory.load_u32(value_pointer + 8U) : 0U;
    const auto value12 =
        value_pointer != 0U ? memory.load_u32(value_pointer + 12U) : 0U;
    std::fprintf(stderr,
                 "AC6_SWG_SYMBOL_TABLE_ENTRY context=0x%08X "
                 "context_vtable=0x%08X node=0x%08X category=0x%08X "
                 "id=0x%08X type_tag=0x%08X value0=0x%08X value8=0x%08X "
                 "value12=0x%08X\n",
                 context, context_vtable, node, category, id, type_tag,
                 value0, value8, value12);
    ++visited;
    node = memory.load_u32(node + 8U);
  }
}

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
  // AC6_DEMO_THE_SYMBOL_TABLE_IS_AN_MSVC_MAP_KEYED_BY_TWO_INTEGERS.md's
  // named next step: sub_820DFFB8 (the AST-node evaluator) passes its own
  // r6 as the map lookup key -- a pointer to a struct whose [+4] (category,
  // wildcard -1) and [+12] (id) fields are the only ones the comparator
  // reads. Log both fields directly rather than the raw pointer, so this
  // is the swg vocabulary itself, not another address to chase. Gated to a
  // window covering the press and a long post-press tail, matching
  // AC6_DEMO_FORCING_GETCURRENTMISSION_TO_16_ALSO_DOES_NOT_TRIGGER_COMPLETION.md's
  // 8000-tick precedent for "does anything resume later" -- ungated this
  // fires every AST node the attract movie evaluates, every frame.
  const auto tick = require_bridge().tick();
  const bool in_watched_window = tick >= 2990ULL && tick <= 8000ULL;
  if (guest_address == 0x820DFFB8U &&
      (std::getenv("AC6_DEMO_WATCH_SWG_LOOKUP_KEY") != nullptr ||
       std::getenv("AC6_DEMO_DUMP_SWG_SYMBOL_TABLE") != nullptr)) {
    auto &memory = require_bridge().memory();
    const auto key_pointer = context.r6.u32;
    const auto category = key_pointer != 0U ? memory.load_u32(key_pointer + 4U) : 0U;
    const auto id = key_pointer != 0U ? memory.load_u32(key_pointer + 12U) : 0U;
    if (std::getenv("AC6_DEMO_WATCH_SWG_LOOKUP_KEY") != nullptr && in_watched_window) {
      const auto node = find_swg_symbol_node(memory, context.r4.u32 + 36U, category, id);
      const auto type_tag = resolve_swg_symbol_type_tag(memory, node);
      std::fprintf(stderr,
                   "AC6_SWG_LOOKUP_KEY tick=%llu thread=%u lr=0x%08X "
                   "key_pointer=0x%08X category=0x%08X id=0x%08X node=0x%08X "
                   "type_tag=0x%08X\n",
                   static_cast<unsigned long long>(tick), current_guest_thread_id,
                   lr, key_pointer, category, id, node, type_tag);
    }
    // AC6_DEMO_CATEGORY_1_IS_STRUCTURALLY_ABSENT_FROM_TITLES_SYMBOL_TABLE.md's
    // named control: startup's context (only ever seen at ticks 1045/2425,
    // outside the lookup-line tick window above) has never actually had
    // its table dumped, so the method that found category 1 missing from
    // title's table has never been shown to find category 1 present where
    // it demonstrably is. This dump is gated separately, on every call,
    // not the lookup-line window -- cheap, since it's once per distinct
    // context (a fixed small seen-set).
    if (std::getenv("AC6_DEMO_DUMP_SWG_SYMBOL_TABLE") != nullptr) {
      static std::array<std::uint32_t, 4> dumped_contexts{};
      static std::size_t dumped_count = 0;
      const auto context_address = context.r4.u32;
      bool already_dumped = false;
      for (std::size_t i = 0; i < dumped_count; ++i) {
        already_dumped = already_dumped || dumped_contexts[i] == context_address;
      }
      if (!already_dumped && dumped_count < dumped_contexts.size()) {
        dumped_contexts[dumped_count++] = context_address;
        const auto context_vtable = memory.load_u32(context_address);
        dump_swg_symbol_table(memory, context_address + 36U, context_address,
                               context_vtable);
      }
    }
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
