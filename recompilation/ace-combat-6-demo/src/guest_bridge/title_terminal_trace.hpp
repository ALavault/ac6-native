#pragma once

// Observation-only probe for the one unresolved natural TitleUS boundary.
// It is disabled unless AC6_DEMO_WATCH_TITLE_TERMINAL is present.  The probe
// never writes guest memory and accepts an object-102 anchor only after the
// cursor values 0x1210 and 0x142C are both observed reaching the same
// MovieMemory::Add receiver.
namespace {

constexpr std::uint64_t kTitleTerminalTraceStartTick = 3000U;
constexpr std::uint64_t kTitleTerminalTracePreAnchorCap = 3405U;
constexpr std::uint64_t kTitleTerminalTracePostTicks = 180U;
constexpr std::uint64_t kTitleTerminalTraceAnchorCap = 400U;

struct TitleTerminalMovieState final {
  bool mapped{};
  std::uint8_t d5{};
  std::uint8_t d6{};
  std::uint32_t d8{};
  std::uint32_t dc{};
  std::uint32_t e0{};
  std::uint32_t f8{};
};

enum class TitleMovieFactoryCallsite : std::uint8_t {
  other,
  root,
  child,
};

[[nodiscard]] constexpr TitleMovieFactoryCallsite
title_movie_factory_callsite(std::uint32_t lr) noexcept {
  if (lr == 0x823223F8U) {
    return TitleMovieFactoryCallsite::root;
  }
  if (lr == 0x8232356CU) {
    return TitleMovieFactoryCallsite::child;
  }
  return TitleMovieFactoryCallsite::other;
}

static_assert(title_movie_factory_callsite(0x823223F8U) ==
              TitleMovieFactoryCallsite::root);
static_assert(title_movie_factory_callsite(0x8232356CU) ==
              TitleMovieFactoryCallsite::child);
static_assert(title_movie_factory_callsite(0U) ==
              TitleMovieFactoryCallsite::other);

struct TitleMovieFactoryJoin final {
  bool active{};
  bool ctor_seen{};
  bool ctor_qualified{};
  bool exit_qualified{};
  bool awaiting_publish{};
  bool done{};
  bool receiver_qualified{};
  bool aggregate_mapped{};
  bool old_publish_mapped{};
  std::uint64_t serial{};
  std::uint64_t tick{};
  std::uint32_t thread{};
  std::uint32_t caller_lr{};
  TitleMovieFactoryCallsite callsite{TitleMovieFactoryCallsite::other};
  std::uint32_t receiver{};
  std::uint32_t aggregate{};
  std::uint32_t old_publish{};
  std::uint32_t ctor_receiver{};
  std::uint32_t ctor_movie_memory{};
  std::uint32_t ctor_parent{};
  std::uint32_t ctor_b{};
  std::uint32_t ctor_d{};
  std::uint32_t ctor_s{};
  std::uint32_t result{};
};

struct TitleTerminalTraceState final {
  std::uint64_t order{};
  std::uint64_t current_tick{std::numeric_limits<std::uint64_t>::max()};
  std::uint64_t candidate_anchor_tick{};
  std::uint64_t anchor_tick{};
  std::uint64_t terminal_tick{};
  std::uint64_t expected_add_tick{};
  std::uint64_t terminal_exec_tick{};
  std::uint32_t candidate_owner{};
  std::uint32_t candidate_memory{};
  std::uint32_t expected_add_owner{};
  std::uint32_t expected_add_memory{};
  std::uint32_t expected_add_raw{};
  std::uint32_t owner{};
  std::uint32_t movie_memory{};
  std::uint32_t pending_exec{};
  std::uint32_t pending_receiver{};
  std::uint32_t terminal{};
  std::uint32_t expected_add_thread{};
  std::uint32_t candidate_thread{};
  std::uint32_t terminal_exec_thread{};
  std::uint32_t terminal_opcode_lr{};
  std::uint32_t candidate_stage{};
  std::uint32_t natural_stage{};
  std::uint32_t drains_this_tick{};
  std::uint32_t post_ticks{};
  std::uint32_t post_ticks_with_drain{};
  bool expected_add{};
  bool anchored{};
  bool opcode2_pending{};
  bool terminal_pending{};
  bool terminal_seen{};
  bool terminal_drain_complete{};
  bool baseline_ready{};
  bool terminal_d5_baseline_mapped{};
  bool post_state_invalid{};
  bool post_redrain{};
  bool d8_history_overflow{};
  bool dc_history_overflow{};
  bool done{};
  std::uint8_t terminal_d5_baseline{};
  std::array<std::uint32_t, 32U> d8_history{};
  std::array<std::uint32_t, 32U> dc_history{};
  std::size_t d8_history_count{};
  std::size_t dc_history_count{};
  std::uint32_t qualified_raw_mask{};
  TitleTerminalMovieState baseline{};
  TitleMovieFactoryJoin movie_factory{};
};

thread_local TitleTerminalTraceState title_terminal_trace;

[[nodiscard]] bool title_terminal_trace_enabled() noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_TITLE_TERMINAL") != nullptr;
  return enabled;
}

[[nodiscard]] bool title_terminal_name_is(const char *generated_name,
                                          std::string_view suffix) noexcept {
  return generated_name != nullptr &&
         std::string_view(generated_name).ends_with(suffix);
}

[[nodiscard]] std::uint64_t title_terminal_next_order() noexcept {
  return ++title_terminal_trace.order;
}

[[nodiscard]] std::uint32_t title_terminal_read_u32(
    GuestMemory &memory, std::uint32_t address) noexcept {
  return address != 0U && memory.mapped(address, 4U)
             ? memory.load_u32(address)
             : 0U;
}

struct TitleMappedU32 final {
  bool mapped{};
  std::uint32_t value{};
};

struct TitleChildConstructorSnapshot final {
  bool enabled{};
  std::uint32_t child{};
  std::uint32_t parent{};
  std::uint32_t b{};
  std::uint32_t d{};
};

[[nodiscard]] TitleMappedU32 title_movie_read_u32(
    GuestMemory &memory, std::uint32_t address) noexcept {
  if (address == 0U || !memory.mapped(address, 4U)) {
    return {};
  }
  return {true, memory.load_u32(address)};
}

[[nodiscard]] TitleChildConstructorSnapshot
trace_title_child_constructor_enter(const PPCContext &context) noexcept {
  if (active_bridge == nullptr ||
      std::getenv("AC6_DEMO_WATCH_TITLE_CHILD_CHAIN") == nullptr ||
      active_bridge->tick() != 3001U) {
    return {};
  }
  static std::uint32_t count = 0U;
  if (count++ >= 16U) {
    return {};
  }
  auto &memory = active_bridge->memory();
  const TitleChildConstructorSnapshot snapshot{
      true, context.r3.u32, context.r6.u32, context.r7.u32, context.r8.u32};
  std::fprintf(
      stderr,
      "AC6_TITLE_CHILD_CTOR event=entry tick=3001 child=0x%08X "
      "movie_memory=0x%08X parent=0x%08X B=0x%08X D=0x%08X s=0x%08X "
      "B0=0x%08X B38=0x%08X D0=0x%08X D4=0x%08X D8=0x%08X "
      "DC=0x%08X D10=0x%08X\n",
      snapshot.child, context.r5.u32, snapshot.parent, snapshot.b, snapshot.d,
      context.r9.u32, title_terminal_read_u32(memory, snapshot.b),
      title_terminal_read_u32(memory, snapshot.b + 0x38U),
      title_terminal_read_u32(memory, snapshot.d),
      title_terminal_read_u32(memory, snapshot.d + 4U),
      title_terminal_read_u32(memory, snapshot.d + 8U),
      title_terminal_read_u32(memory, snapshot.d + 0xCU),
      title_terminal_read_u32(memory, snapshot.d + 0x10U));
  return snapshot;
}

void trace_title_child_constructor_exit(
    const TitleChildConstructorSnapshot &snapshot) noexcept {
  if (!snapshot.enabled || active_bridge == nullptr) {
    return;
  }
  auto &memory = active_bridge->memory();
  const auto frame_begin = title_terminal_read_u32(memory, snapshot.child + 0x28U);
  const auto frame_end = title_terminal_read_u32(memory, snapshot.child + 0x2CU);
  const auto frame_count = title_terminal_read_u32(memory, snapshot.child + 0x30U);
  const auto element_offset = title_terminal_read_u32(memory, frame_begin);
  const auto element_count = title_terminal_read_u32(memory, frame_begin + 4U);
  const auto base = title_terminal_read_u32(memory, snapshot.b);
  const auto elements = base + element_offset;
  std::fprintf(
      stderr,
      "AC6_TITLE_CHILD_CTOR event=exit tick=3001 child=0x%08X "
      "frame_begin=0x%08X frame_end=0x%08X frame_count=%u "
      "frame0_offset=0x%08X frame0_count=%u elements=0x%08X",
      snapshot.child, frame_begin, frame_end, frame_count, element_offset,
      element_count, elements);
  for (std::uint32_t index = 0U; index < 24U; ++index) {
    std::fprintf(stderr, " w%u=0x%08X", index,
                 title_terminal_read_u32(memory, elements + 4U * index));
  }
  std::fputc('\n', stderr);

  const auto element_type = title_movie_read_u32(memory, elements);
  const auto list_index = title_movie_read_u32(memory, elements + 8U);
  if (!element_type.mapped || element_type.value != 4U ||
      !list_index.mapped || list_index.value != 13U) {
    return;
  }
  const auto table = title_movie_read_u32(memory, snapshot.b + 0x38U);
  const auto descriptor = table.value + list_index.value * 8U;
  const auto descriptor_type = title_movie_read_u32(memory, descriptor);
  const auto list_offset = title_movie_read_u32(memory, descriptor + 4U);
  const auto list = base + list_offset.value;
  const auto list_count = title_movie_read_u32(memory, list);
  std::fprintf(
      stderr,
      "AC6_TITLE_CHILD_LIST tick=3001 child=0x%08X index=%u "
      "table_mapped=%u table=0x%08X descriptor=0x%08X "
      "descriptor_type_mapped=%u descriptor_type=0x%08X "
      "offset_mapped=%u offset=0x%08X list=0x%08X count_mapped=%u count=%u",
      snapshot.child, list_index.value, table.mapped ? 1U : 0U, table.value,
      descriptor, descriptor_type.mapped ? 1U : 0U, descriptor_type.value,
      list_offset.mapped ? 1U : 0U, list_offset.value, list,
      list_count.mapped ? 1U : 0U, list_count.value);
  auto record = list + 4U;
  for (std::uint32_t index = 0U;
       list_count.mapped && index < list_count.value && index < 8U; ++index) {
    const auto next = title_movie_read_u32(memory, record + 4U);
    std::fprintf(stderr,
                 " r%u_addr=0x%08X r%u_w0=0x%08X r%u_w1=0x%08X "
                 "r%u_w2=0x%08X r%u_w3=0x%08X",
                 index, record, index,
                 title_terminal_read_u32(memory, record), index,
                 title_terminal_read_u32(memory, record + 4U), index,
                 title_terminal_read_u32(memory, record + 8U), index,
                 title_terminal_read_u32(memory, record + 0xCU));
    if (!next.mapped || next.value == 0U ||
        next.value > std::numeric_limits<std::uint32_t>::max() - base) {
      break;
    }
    record = base + next.value;
  }
  std::fputc('\n', stderr);
}

[[nodiscard]] constexpr std::uint32_t title_movie_field_address(
    std::uint32_t base, std::uint32_t offset) noexcept {
  return base != 0U &&
                 base <= std::numeric_limits<std::uint32_t>::max() - offset
             ? base + offset
             : 0U;
}

[[nodiscard]] const char *title_movie_factory_callsite_name(
    TitleMovieFactoryCallsite callsite) noexcept {
  switch (callsite) {
  case TitleMovieFactoryCallsite::root:
    return "root";
  case TitleMovieFactoryCallsite::child:
    return "child";
  case TitleMovieFactoryCallsite::other:
    return "other";
  }
  return "other";
}

struct TitleMovieBoundedName final {
  bool mapped{};
  bool terminated{};
  std::array<char, 33U> text{};
};

[[nodiscard]] TitleMovieBoundedName title_movie_bounded_name(
    GuestMemory &memory, std::uint32_t address) noexcept {
  TitleMovieBoundedName result{};
  if (address == 0U || !memory.mapped(address, 1U)) {
    return result;
  }
  result.mapped = true;
  for (std::size_t index = 0U; index + 1U < result.text.size(); ++index) {
    const auto current = static_cast<std::uint64_t>(address) + index;
    if (current > std::numeric_limits<std::uint32_t>::max() ||
        !memory.mapped(static_cast<std::uint32_t>(current), 1U)) {
      break;
    }
    const auto value = memory.load_u8(static_cast<std::uint32_t>(current));
    if (value == 0U) {
      result.terminated = true;
      break;
    }
    result.text[index] = value >= 0x20U && value <= 0x7EU
                             ? static_cast<char>(value)
                             : '.';
  }
  return result;
}

[[nodiscard]] TitleTerminalMovieState title_terminal_movie_state(
    GuestMemory &memory, std::uint32_t owner) noexcept {
  TitleTerminalMovieState state{};
  if (owner == 0U || !memory.mapped(owner, 0xFCU)) {
    return state;
  }
  state.mapped = true;
  state.d5 = memory.load_u8(owner + 0xD5U);
  state.d6 = memory.load_u8(owner + 0xD6U);
  state.d8 = memory.load_u32(owner + 0xD8U);
  state.dc = memory.load_u32(owner + 0xDCU);
  state.e0 = memory.load_u32(owner + 0xE0U);
  state.f8 = memory.load_u32(owner + 0xF8U);
  return state;
}

template <std::size_t Size>
void title_terminal_remember(std::array<std::uint32_t, Size> &history,
                             std::size_t &count, bool &overflow,
                             std::uint32_t value) noexcept {
  if (std::find(history.begin(), history.begin() + count, value) !=
      history.begin() + count) {
    return;
  }
  if (count < history.size()) {
    history[count++] = value;
  } else {
    overflow = true;
  }
}

template <std::size_t Size>
[[nodiscard]] bool title_terminal_remembered(
    const std::array<std::uint32_t, Size> &history, std::size_t count,
    std::uint32_t value) noexcept {
  return std::find(history.begin(), history.begin() + count, value) !=
         history.begin() + count;
}

void title_terminal_remember_movie(
    const TitleTerminalMovieState &state) noexcept {
  auto &trace = title_terminal_trace;
  if (!state.mapped || trace.terminal_pending || trace.terminal_seen) {
    return;
  }
  title_terminal_remember(trace.d8_history, trace.d8_history_count,
                          trace.d8_history_overflow, state.d8);
  title_terminal_remember(trace.dc_history, trace.dc_history_count,
                          trace.dc_history_overflow, state.dc);
}

[[nodiscard]] std::uint32_t
title_terminal_raw_mask(std::uint32_t raw) noexcept {
  switch (raw) {
  case 0x1210U:
    return 1U << 0U;
  case 0x142CU:
    return 1U << 1U;
  case 0x146CU:
    return 1U << 2U;
  case 0x171CU:
    return 1U << 3U;
  default:
    return 0U;
  }
}

void title_terminal_print_movie(const char *event, std::uint64_t tick,
                                std::uint32_t owner,
                                const TitleTerminalMovieState &state) noexcept {
  std::fprintf(
      stderr,
      "AC6_TITLE_TERMINAL order=%llu event=%s tick=%llu thread=%u "
      "owner=0x%08X mapped=%u D5=%u D6=%u D8=0x%08X DC=0x%08X "
      "E0=0x%08X F8=0x%08X\n",
      static_cast<unsigned long long>(title_terminal_next_order()), event,
      static_cast<unsigned long long>(tick), current_guest_thread_id, owner,
      state.mapped ? 1U : 0U, static_cast<unsigned>(state.d5),
      static_cast<unsigned>(state.d6), state.d8, state.dc, state.e0,
      state.f8);
}

void title_terminal_finish(const char *verdict, const char *reason,
                           std::uint64_t tick) noexcept {
  auto &trace = title_terminal_trace;
  if (trace.done) {
    return;
  }
  std::fprintf(
      stderr,
      "AC6_TITLE_TERMINAL order=%llu event=done tick=%llu verdict=%s "
      "reason=%s anchor_tick=%llu terminal_tick=%llu owner=0x%08X "
      "movie_memory=0x%08X post_ticks=%u post_ticks_with_drain=%u "
      "post_state_invalid=%u post_redrain=%u d8_history_overflow=%u "
      "dc_history_overflow=%u\n",
      static_cast<unsigned long long>(title_terminal_next_order()),
      static_cast<unsigned long long>(tick), verdict, reason,
      static_cast<unsigned long long>(trace.anchor_tick),
      static_cast<unsigned long long>(trace.terminal_tick), trace.owner,
      trace.movie_memory, trace.post_ticks, trace.post_ticks_with_drain,
      trace.post_state_invalid ? 1U : 0U, trace.post_redrain ? 1U : 0U,
      trace.d8_history_overflow ? 1U : 0U,
      trace.dc_history_overflow ? 1U : 0U);
  trace.done = true;
}

void title_terminal_positive(const char *reason, std::uint64_t tick) noexcept {
  title_terminal_finish("POSITIVE_CAUSAL_EDGE", reason, tick);
}

[[nodiscard]] bool title_terminal_trace_open(std::uint64_t tick) noexcept {
  const auto &trace = title_terminal_trace;
  if (!title_terminal_trace_enabled() || trace.done ||
      tick < kTitleTerminalTraceStartTick) {
    return false;
  }
  if (!trace.anchored) {
    return tick <= kTitleTerminalTracePreAnchorCap;
  }
  return tick <= trace.anchor_tick + kTitleTerminalTraceAnchorCap;
}

[[nodiscard]] bool title_movie_factory_trace_open(
    std::uint64_t tick) noexcept {
  return title_terminal_trace_enabled() &&
         tick >= kTitleTerminalTraceStartTick &&
         tick <= kTitleTerminalTracePreAnchorCap &&
         !title_terminal_trace.movie_factory.done;
}

void title_movie_factory_done(const char *verdict, const char *reason,
                              std::uint64_t tick) noexcept {
  auto &join = title_terminal_trace.movie_factory;
  if (join.done) {
    return;
  }
  std::fprintf(
      stderr,
      "AC6_TITLE_MOVIE_FACTORY order=%llu event=done tick=%llu "
      "verdict=%s reason=%s serial=%llu callsite=%s caller_lr=0x%08X "
      "receiver=0x%08X aggregate=0x%08X result=0x%08X\n",
      static_cast<unsigned long long>(title_terminal_next_order()),
      static_cast<unsigned long long>(tick), verdict, reason,
      static_cast<unsigned long long>(join.serial),
      title_movie_factory_callsite_name(join.callsite), join.caller_lr,
      join.receiver, join.aggregate, join.result);
  join.active = false;
  join.awaiting_publish = false;
  join.done = true;
}

[[nodiscard]] std::uint64_t
trace_title_movie_factory_enter(PPCContext &context) noexcept {
  if (active_bridge == nullptr) {
    return 0U;
  }
  const auto tick = active_bridge->tick();
  if (!title_movie_factory_trace_open(tick)) {
    return 0U;
  }
  auto &join = title_terminal_trace.movie_factory;
  if (join.active || join.awaiting_publish) {
    title_movie_factory_done("INCONCLUSIVE", "factory_reentered", tick);
    return 0U;
  }

  join = {};
  join.active = true;
  join.serial = title_terminal_next_order();
  join.tick = tick;
  join.thread = current_guest_thread_id;
  join.caller_lr = static_cast<std::uint32_t>(context.lr);
  join.callsite = title_movie_factory_callsite(join.caller_lr);
  join.receiver = context.r3.u32;

  auto &memory = active_bridge->memory();
  const auto receiver_vtable = title_movie_read_u32(memory, join.receiver);
  join.receiver_qualified =
      receiver_vtable.mapped && receiver_vtable.value == 0x820064D8U;
  const auto aggregate = title_movie_read_u32(
      memory, title_movie_field_address(join.receiver, 4U));
  join.aggregate_mapped = aggregate.mapped;
  join.aggregate = aggregate.value;
  const auto old_publish = title_movie_read_u32(
      memory, join.aggregate_mapped
                  ? title_movie_field_address(join.aggregate, 0xF4U)
                  : 0U);
  join.old_publish_mapped = old_publish.mapped;
  join.old_publish = old_publish.value;

  const auto manager = title_movie_read_u32(memory, 0x827435F8U);
  const auto current_task = title_movie_read_u32(
      memory, manager.mapped
                  ? title_movie_field_address(manager.value, 8U)
                  : 0U);
  const auto task_vtable = title_movie_read_u32(
      memory, current_task.mapped ? current_task.value : 0U);
  const auto task_swg_vtable = title_movie_read_u32(
      memory, current_task.mapped
                  ? title_movie_field_address(current_task.value, 0x1CU)
                  : 0U);
  const auto task_swg_aggregate = title_movie_read_u32(
      memory, current_task.mapped
                  ? title_movie_field_address(current_task.value, 0x20U)
                  : 0U);

  std::fprintf(
      stderr,
      "AC6_TITLE_MOVIE_FACTORY order=%llu event=factory_entry tick=%llu "
      "thread=%u serial=%llu callsite=%s caller_lr=0x%08X "
      "r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X r7=0x%08X "
      "r8=0x%08X r9=0x%08X receiver_vtable_mapped=%u "
      "receiver_vtable=0x%08X "
      "aggregate_mapped=%u aggregate=0x%08X old_publish_mapped=%u "
      "old_publish=0x%08X manager_mapped=%u manager=0x%08X "
      "current_task_mapped=%u current_task=0x%08X "
      "task_vtable_mapped=%u task_vtable=0x%08X "
      "task_swg_vtable_mapped=%u task_swg_vtable=0x%08X "
      "task_swg_aggregate_mapped=%u task_swg_aggregate=0x%08X "
      "receiver_vtable_matches=%u task_swg_vtable_matches=%u "
      "task_swg_matches_aggregate=%u\n",
      static_cast<unsigned long long>(join.serial),
      static_cast<unsigned long long>(tick), join.thread,
      static_cast<unsigned long long>(join.serial),
      title_movie_factory_callsite_name(join.callsite), join.caller_lr,
      context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32,
      context.r7.u32, context.r8.u32, context.r9.u32,
      receiver_vtable.mapped ? 1U : 0U, receiver_vtable.value,
      aggregate.mapped ? 1U : 0U, aggregate.value,
      old_publish.mapped ? 1U : 0U, old_publish.value,
      manager.mapped ? 1U : 0U, manager.value,
      current_task.mapped ? 1U : 0U, current_task.value,
      task_vtable.mapped ? 1U : 0U, task_vtable.value,
      task_swg_vtable.mapped ? 1U : 0U, task_swg_vtable.value,
      task_swg_aggregate.mapped ? 1U : 0U, task_swg_aggregate.value,
      receiver_vtable.mapped && receiver_vtable.value == 0x820064D8U ? 1U
                                                                     : 0U,
      task_swg_vtable.mapped && task_swg_vtable.value == 0x82006438U
          ? 1U
          : 0U,
      task_swg_aggregate.mapped && aggregate.mapped &&
              task_swg_aggregate.value == aggregate.value
          ? 1U
          : 0U);
  return join.serial;
}

void trace_title_movie_factory_ctor(PPCContext &context,
                                    std::uint64_t tick) noexcept {
  auto &join = title_terminal_trace.movie_factory;
  if (!join.active) {
    return;
  }
  const auto lr = static_cast<std::uint32_t>(context.lr);
  const bool correlated = tick == join.tick &&
                          current_guest_thread_id == join.thread &&
                          lr == 0x820D1940U && !join.ctor_seen;
  if (!correlated) {
    std::fprintf(
        stderr,
        "AC6_TITLE_MOVIE_FACTORY order=%llu event=ctor_join_rejected "
        "tick=%llu thread=%u serial=%llu lr=0x%08X expected_tick=%llu "
        "expected_thread=%u ctor_seen=%u\n",
        static_cast<unsigned long long>(title_terminal_next_order()),
        static_cast<unsigned long long>(tick), current_guest_thread_id,
        static_cast<unsigned long long>(join.serial), lr,
        static_cast<unsigned long long>(join.tick), join.thread,
        join.ctor_seen ? 1U : 0U);
    title_movie_factory_done("INCONCLUSIVE", "ctor_correlation_failed", tick);
    return;
  }
  join.ctor_seen = true;
  join.ctor_receiver = context.r3.u32;
  join.ctor_movie_memory = context.r5.u32;
  join.ctor_parent = context.r6.u32;
  join.ctor_b = context.r7.u32;
  join.ctor_d = context.r8.u32;
  join.ctor_s = context.r9.u32;
  join.ctor_qualified =
      join.aggregate_mapped && context.r4.u32 == join.aggregate &&
      static_cast<std::uint64_t>(join.aggregate) + 8U == context.r7.u32;
  auto &memory = active_bridge->memory();
  const auto name = title_movie_bounded_name(memory, join.ctor_s);
  std::fprintf(
      stderr,
      "AC6_TITLE_MOVIE_FACTORY order=%llu event=ctor_join tick=%llu "
      "thread=%u serial=%llu lr=0x%08X receiver=0x%08X A=0x%08X "
      "movie_memory=0x%08X parent=0x%08X B=0x%08X D=0x%08X "
      "s=0x%08X name_mapped=%u name_terminated=%u name=%s "
      "A_matches=%u B_matches_A_plus_8=%u\n",
      static_cast<unsigned long long>(title_terminal_next_order()),
      static_cast<unsigned long long>(tick), current_guest_thread_id,
      static_cast<unsigned long long>(join.serial), lr, context.r3.u32,
      context.r4.u32, context.r5.u32, context.r6.u32, context.r7.u32,
      context.r8.u32, context.r9.u32, name.mapped ? 1U : 0U,
      name.terminated ? 1U : 0U, name.text.data(),
      join.aggregate_mapped && context.r4.u32 == join.aggregate ? 1U : 0U,
      join.aggregate_mapped &&
              static_cast<std::uint64_t>(join.aggregate) + 8U ==
                  context.r7.u32
          ? 1U
          : 0U);
}

void trace_title_movie_factory_exit(PPCContext &context, std::uint64_t token,
                                    bool threw) noexcept {
  if (token == 0U || active_bridge == nullptr) {
    return;
  }
  auto &join = title_terminal_trace.movie_factory;
  const auto tick = active_bridge->tick();
  if (!join.active || token != join.serial || tick != join.tick ||
      current_guest_thread_id != join.thread) {
    title_movie_factory_done("INCONCLUSIVE", "factory_exit_mismatch", tick);
    return;
  }
  if (threw) {
    title_movie_factory_done("INCONCLUSIVE", "factory_exception", tick);
    return;
  }
  if (!join.ctor_seen) {
    title_movie_factory_done("INCONCLUSIVE", "ctor_missing", tick);
    return;
  }
  join.result = context.r3.u32;
  auto &memory = active_bridge->memory();
  const auto result_vtable = title_movie_read_u32(memory, join.result);
  const auto result_movie_memory = title_movie_read_u32(
      memory, title_movie_field_address(join.result, 0x10U));
  const auto result_b = title_movie_read_u32(
      memory, title_movie_field_address(join.result, 0x20U));
  const auto result_d = title_movie_read_u32(
      memory, title_movie_field_address(join.result, 0x24U));
  const auto result_s = title_movie_read_u32(
      memory, title_movie_field_address(join.result, 0xD0U));
  join.exit_qualified =
      join.receiver_qualified && join.ctor_qualified && join.result != 0U &&
      join.result == join.ctor_receiver && result_vtable.mapped &&
      result_vtable.value == 0x820304D8U && result_movie_memory.mapped &&
      result_movie_memory.value == join.ctor_movie_memory && result_b.mapped &&
      result_b.value == join.ctor_b && result_d.mapped &&
      result_d.value == join.ctor_d && result_s.mapped &&
      result_s.value == join.ctor_s;
  std::fprintf(
      stderr,
      "AC6_TITLE_MOVIE_FACTORY order=%llu event=factory_exit tick=%llu "
      "thread=%u serial=%llu callsite=%s result=0x%08X "
      "result_vtable_mapped=%u result_vtable=0x%08X "
      "result_movie_memory_mapped=%u result_movie_memory=0x%08X "
      "result_B_mapped=%u result_B=0x%08X result_D_mapped=%u "
      "result_D=0x%08X result_s_mapped=%u result_s=0x%08X "
      "result_matches_ctor_receiver=%u vtable_matches=%u "
      "movie_memory_matches=%u B_matches=%u D_matches=%u s_matches=%u "
      "qualified=%u\n",
      static_cast<unsigned long long>(title_terminal_next_order()),
      static_cast<unsigned long long>(tick), current_guest_thread_id,
      static_cast<unsigned long long>(join.serial),
      title_movie_factory_callsite_name(join.callsite), join.result,
      result_vtable.mapped ? 1U : 0U, result_vtable.value,
      result_movie_memory.mapped ? 1U : 0U, result_movie_memory.value,
      result_b.mapped ? 1U : 0U, result_b.value,
      result_d.mapped ? 1U : 0U, result_d.value,
      result_s.mapped ? 1U : 0U, result_s.value,
      join.result == join.ctor_receiver ? 1U : 0U,
      result_vtable.mapped && result_vtable.value == 0x820304D8U ? 1U : 0U,
      result_movie_memory.mapped &&
              result_movie_memory.value == join.ctor_movie_memory
          ? 1U
          : 0U,
      result_b.mapped && result_b.value == join.ctor_b ? 1U : 0U,
      result_d.mapped && result_d.value == join.ctor_d ? 1U : 0U,
      result_s.mapped && result_s.value == join.ctor_s ? 1U : 0U,
      join.exit_qualified ? 1U : 0U);

  join.active = false;
  if (!join.exit_qualified) {
    title_movie_factory_done("INCONCLUSIVE", "factory_layout_mismatch", tick);
    return;
  }
  if (join.callsite == TitleMovieFactoryCallsite::root) {
    join.awaiting_publish = true;
    return;
  }
  title_movie_factory_done("OBSERVED", "factory_result_without_root_publish",
                           tick);
}

void title_terminal_reset_candidate() noexcept {
  auto &trace = title_terminal_trace;
  trace.candidate_owner = 0U;
  trace.candidate_memory = 0U;
  trace.candidate_anchor_tick = 0U;
  trace.candidate_stage = 0U;
  trace.expected_add = false;
  trace.expected_add_owner = 0U;
  trace.expected_add_memory = 0U;
  trace.expected_add_raw = 0U;
  trace.expected_add_tick = 0U;
  trace.expected_add_thread = 0U;
  trace.candidate_thread = 0U;
}

void title_terminal_expect_add(std::uint32_t owner,
                               std::uint32_t movie_memory,
                               std::uint32_t raw,
                               std::uint64_t tick) noexcept {
  auto &trace = title_terminal_trace;
  trace.expected_add = true;
  trace.expected_add_owner = owner;
  trace.expected_add_memory = movie_memory;
  trace.expected_add_raw = raw;
  trace.expected_add_tick = tick;
  trace.expected_add_thread = current_guest_thread_id;
}

void trace_title_terminal_outer(PPCContext &context,
                                std::uint64_t tick) noexcept {
  auto &trace = title_terminal_trace;
  auto &memory = require_bridge().memory();
  const auto owner = context.r3.u32;
  const auto movie_memory = title_terminal_read_u32(memory, owner + 0x10U);
  const auto cursor = title_terminal_read_u32(memory, owner + 0xF8U);
  const auto raw = title_terminal_read_u32(memory, cursor + 4U);
  std::fprintf(
      stderr,
      "AC6_TITLE_TERMINAL order=%llu event=outer_type6 tick=%llu thread=%u "
      "owner=0x%08X movie_memory=0x%08X cursor=0x%08X raw=0x%08X "
      "lr=0x%08X anchored=%u\n",
      static_cast<unsigned long long>(title_terminal_next_order()),
      static_cast<unsigned long long>(tick), current_guest_thread_id, owner,
      movie_memory, cursor, raw, static_cast<std::uint32_t>(context.lr),
      trace.anchored ? 1U : 0U);

  if (trace.anchored) {
    return;
  }
  if (raw == 0x1210U) {
    title_terminal_reset_candidate();
    trace.candidate_owner = owner;
    trace.candidate_memory = movie_memory;
    trace.candidate_anchor_tick = tick;
    trace.candidate_thread = current_guest_thread_id;
    trace.candidate_stage = 1U;
    title_terminal_expect_add(owner, movie_memory, raw, tick);
    return;
  }
  if (trace.candidate_stage == 2U && owner == trace.candidate_owner &&
      movie_memory == trace.candidate_memory && raw == 0x142CU &&
      current_guest_thread_id == trace.candidate_thread) {
    trace.candidate_stage = 3U;
    title_terminal_expect_add(owner, movie_memory, raw, tick);
    return;
  }
  if (owner == trace.candidate_owner) {
    title_terminal_reset_candidate();
  }
}

void trace_title_terminal_add(PPCContext &context,
                              std::uint64_t tick) noexcept {
  auto &trace = title_terminal_trace;
  const auto movie_memory = context.r3.u32;
  const auto raw = context.r4.u32;
  const bool expected = trace.expected_add &&
                        movie_memory == trace.expected_add_memory &&
                        raw == trace.expected_add_raw &&
                        tick == trace.expected_add_tick &&
                        current_guest_thread_id == trace.expected_add_thread &&
                        static_cast<std::uint32_t>(context.lr) == 0x82322AB4U;
  if (trace.anchored && movie_memory != trace.movie_memory) {
    return;
  }
  if (!expected && !trace.anchored) {
    return;
  }
  std::fprintf(
      stderr,
      "AC6_TITLE_TERMINAL order=%llu event=movie_memory_add tick=%llu "
      "thread=%u movie_memory=0x%08X raw=0x%08X lr=0x%08X "
      "expected=%u expected_owner=0x%08X expected_tick=%llu "
      "expected_thread=%u\n",
      static_cast<unsigned long long>(title_terminal_next_order()),
      static_cast<unsigned long long>(tick), current_guest_thread_id,
      movie_memory, raw, static_cast<std::uint32_t>(context.lr),
      expected ? 1U : 0U, trace.expected_add_owner,
      static_cast<unsigned long long>(trace.expected_add_tick),
      trace.expected_add_thread);
  if (!expected) {
    if (trace.expected_add) {
      title_terminal_reset_candidate();
    }
    return;
  }
  trace.expected_add = false;
  if (trace.candidate_stage == 1U && raw == 0x1210U) {
    trace.candidate_stage = 2U;
    return;
  }
  if (trace.candidate_stage == 3U && raw == 0x142CU) {
    trace.anchored = true;
    trace.owner = trace.candidate_owner;
    trace.movie_memory = trace.candidate_memory;
    trace.anchor_tick = trace.candidate_anchor_tick;
    trace.candidate_stage = 4U;
    trace.qualified_raw_mask = title_terminal_raw_mask(0x1210U) |
                               title_terminal_raw_mask(0x142CU);
    std::fprintf(
        stderr,
        "AC6_TITLE_TERMINAL order=%llu event=anchor_confirmed tick=%llu "
        "thread=%u anchor_tick=%llu owner=0x%08X movie_memory=0x%08X\n",
        static_cast<unsigned long long>(title_terminal_next_order()),
        static_cast<unsigned long long>(tick), current_guest_thread_id,
        static_cast<unsigned long long>(trace.anchor_tick), trace.owner,
        trace.movie_memory);
    const auto state =
        title_terminal_movie_state(require_bridge().memory(), trace.owner);
    title_terminal_remember_movie(state);
    title_terminal_print_movie("anchor_state", tick, trace.owner, state);
  }
}

[[nodiscard]] std::uint32_t title_terminal_follow_chain(
    GuestMemory &memory, std::uint32_t receiver, std::uint32_t *depth) noexcept {
  auto current = receiver;
  *depth = 0U;
  while (current != 0U && *depth < 32U &&
         memory.mapped(current + 0x19CU, 4U)) {
    const auto link = memory.load_u32(current + 0x19CU);
    if (link == 0U) {
      return current;
    }
    if (!memory.mapped(link + 0xF4U, 4U)) {
      return 0U;
    }
    current = memory.load_u32(link + 0xF4U);
    ++*depth;
  }
  return 0U;
}

[[nodiscard]] bool title_terminal_receiver_in_chain(
    GuestMemory &memory, std::uint32_t owner,
    std::uint32_t receiver) noexcept {
  auto current = owner;
  for (std::uint32_t depth = 0U; current != 0U && depth < 32U; ++depth) {
    if (current == receiver) {
      return true;
    }
    if (!memory.mapped(current + 0x19CU, 4U)) {
      return false;
    }
    const auto link = memory.load_u32(current + 0x19CU);
    if (link == 0U || !memory.mapped(link + 0xF4U, 4U)) {
      return false;
    }
    current = memory.load_u32(link + 0xF4U);
  }
  return false;
}

struct TitleTerminalFunctionTarget final {
  std::string_view suffix;
  std::string_view role;
  enum class Receiver : std::uint8_t {
    log_only,
    title,
    title_listener,
    manager,
    current_task,
    manager_factory_call,
  } receiver{};
  bool positive_after_terminal;
  bool invalidates_negative;
};

void trace_title_terminal_named_target(PPCContext &context,
                                       const char *generated_name,
                                       std::uint64_t tick) noexcept {
  constexpr std::array<TitleTerminalFunctionTarget, 18U> targets{{
      {"sub_8217C890", "title_listener_slot54",
       TitleTerminalFunctionTarget::Receiver::title_listener, false, true},
      {"sub_8218AB98", "title_slot48",
       TitleTerminalFunctionTarget::Receiver::title, false, true},
      {"sub_8218AA30", "title_slot4c",
       TitleTerminalFunctionTarget::Receiver::title, false, true},
      {"sub_8218A7A8", "title_update",
       TitleTerminalFunctionTarget::Receiver::title, false, true},
      {"sub_8218EA88", "manager_factory_setter",
       TitleTerminalFunctionTarget::Receiver::manager, false, true},
      {"sub_821929A8", "manager_update",
       TitleTerminalFunctionTarget::Receiver::manager, false, true},
      {"sub_82190B18", "manager_apply_factory",
       TitleTerminalFunctionTarget::Receiver::manager, true, true},
      {"sub_821926C0", "loading_global_factory",
       TitleTerminalFunctionTarget::Receiver::log_only, false, false},
      {"sub_8217E890", "loading_slot48",
       TitleTerminalFunctionTarget::Receiver::current_task, true, true},
      {"sub_82192780", "game_demo_global_factory",
       TitleTerminalFunctionTarget::Receiver::log_only, false, false},
      {"sub_820E45F8", "movie_step_forward_callback",
       TitleTerminalFunctionTarget::Receiver::log_only, false, false},
      {"sub_820E4648", "movie_rearm_callback",
       TitleTerminalFunctionTarget::Receiver::log_only, false, false},
      {"sub_820E4698", "movie_step_back_callback",
       TitleTerminalFunctionTarget::Receiver::log_only, false, false},
      {"sub_820E4800", "movie_clear_callback",
       TitleTerminalFunctionTarget::Receiver::log_only, false, false},
      {"sub_820E50D8", "movie_select_rearm_callback",
       TitleTerminalFunctionTarget::Receiver::log_only, false, false},
      {"sub_820E5140", "movie_select_callback",
       TitleTerminalFunctionTarget::Receiver::log_only, false, false},
      {"sub_82324930", "movie_opcode34_handler",
       TitleTerminalFunctionTarget::Receiver::log_only, false, false},
      {"sub_82323808", "movie_controller_constructor",
       TitleTerminalFunctionTarget::Receiver::log_only, false, false},
  }};
  const auto found = std::find_if(
      targets.begin(), targets.end(), [&](const auto &target) {
        return title_terminal_name_is(generated_name, target.suffix);
      });
  if (found == targets.end()) {
    return;
  }
  auto &memory = require_bridge().memory();
  const auto manager = title_terminal_read_u32(memory, 0x827435F8U);
  const auto current_task =
      manager == 0U ? 0U : title_terminal_read_u32(memory, manager + 8U);
  bool qualified = false;
  switch (found->receiver) {
  case TitleTerminalFunctionTarget::Receiver::log_only:
    qualified = true;
    break;
  case TitleTerminalFunctionTarget::Receiver::title:
    qualified = current_task != 0U && context.r3.u32 == current_task;
    break;
  case TitleTerminalFunctionTarget::Receiver::title_listener:
    qualified = current_task != 0U &&
                context.r3.u32 == current_task + 0x68U;
    break;
  case TitleTerminalFunctionTarget::Receiver::manager:
    qualified = manager != 0U && context.r3.u32 == manager;
    break;
  case TitleTerminalFunctionTarget::Receiver::current_task:
    qualified = current_task != 0U && context.r3.u32 == current_task;
    break;
  case TitleTerminalFunctionTarget::Receiver::manager_factory_call:
    qualified = static_cast<std::uint32_t>(context.lr) == 0x82190BE4U;
    break;
  }
  std::fprintf(
      stderr,
      "AC6_TITLE_TERMINAL order=%llu event=target_entry tick=%llu thread=%u "
      "function=%s role=%.*s lr=0x%08X r3=0x%08X r4=0x%08X "
      "r5=0x%08X r6=0x%08X manager=0x%08X current_task=0x%08X "
      "qualified=%u\n",
      static_cast<unsigned long long>(title_terminal_next_order()),
      static_cast<unsigned long long>(tick), current_guest_thread_id,
      generated_name, static_cast<int>(found->role.size()), found->role.data(),
      static_cast<std::uint32_t>(context.lr), context.r3.u32, context.r4.u32,
      context.r5.u32, context.r6.u32, manager, current_task,
      qualified ? 1U : 0U);
  if (!title_terminal_trace.terminal_seen || !qualified) {
    return;
  }
  if (found->invalidates_negative) {
    title_terminal_trace.post_state_invalid = true;
  }
  if (found->positive_after_terminal) {
    title_terminal_positive(found->role.data(), tick);
  }
}

void trace_title_terminal_function_entry(PPCContext &context,
                                         const char *generated_name) noexcept {
  if (active_bridge == nullptr || generated_name == nullptr) {
    return;
  }
  const auto tick = active_bridge->tick();
  if (title_terminal_name_is(generated_name, "sub_82323808")) {
    trace_title_movie_factory_ctor(context, tick);
  }
  if (!title_terminal_trace_open(tick)) {
    return;
  }
  if (title_terminal_trace.expected_add &&
      !title_terminal_name_is(generated_name, "sub_820D0FD8")) {
    title_terminal_reset_candidate();
  }
  if (title_terminal_trace.opcode2_pending &&
      !title_terminal_name_is(generated_name, "sub_82322CD8")) {
    title_terminal_finish("INCONCLUSIVE", "terminal_helper_not_immediate",
                          tick);
    return;
  }
  trace_title_terminal_named_target(context, generated_name, tick);
  if (title_terminal_trace.done) {
    return;
  }
  if (title_terminal_name_is(generated_name, "sub_82322A80")) {
    trace_title_terminal_outer(context, tick);
    return;
  }
  if (title_terminal_name_is(generated_name, "sub_820D0FD8")) {
    trace_title_terminal_add(context, tick);
    return;
  }

  auto &trace = title_terminal_trace;
  if (!trace.anchored) {
    return;
  }
  auto &memory = active_bridge->memory();
  if (title_terminal_name_is(generated_name, "sub_82323BB8") &&
      context.r3.u32 == trace.owner) {
    ++trace.drains_this_tick;
    const auto state = title_terminal_movie_state(memory, trace.owner);
    title_terminal_remember_movie(state);
    title_terminal_print_movie("drain_entry", tick, trace.owner, state);
    return;
  }

  constexpr std::array<std::string_view, 6U> setters{
      "sub_82322CD8", "sub_82322CF8", "sub_82322D28",
      "sub_82322D68", "sub_82322DB0", "sub_82322DF0"};
  const auto setter = std::find_if(setters.begin(), setters.end(),
                                   [&](std::string_view suffix) {
                                     return title_terminal_name_is(
                                         generated_name, suffix);
                                   });
  if (setter != setters.end()) {
    const bool qualified = title_terminal_receiver_in_chain(
        memory, trace.owner, context.r3.u32);
    std::fprintf(
        stderr,
        "AC6_TITLE_TERMINAL order=%llu event=movie_state_helper tick=%llu "
        "thread=%u function=%s lr=0x%08X r3=0x%08X r4=0x%08X "
        "qualified=%u\n",
        static_cast<unsigned long long>(title_terminal_next_order()),
        static_cast<unsigned long long>(tick), current_guest_thread_id,
        generated_name, static_cast<std::uint32_t>(context.lr), context.r3.u32,
        context.r4.u32, qualified ? 1U : 0U);
    if (trace.terminal_seen && qualified && *setter != "sub_82322CD8") {
      trace.post_state_invalid = true;
      title_terminal_positive("qualified_navigation_setter", tick);
      return;
    }
  }

  if (title_terminal_name_is(generated_name, "sub_823251E0")) {
    const auto exec = context.r3.u32;
    const auto raw = context.r4.u32;
    const auto owner_link = title_terminal_read_u32(memory, exec + 4U);
    std::fprintf(
        stderr,
        "AC6_TITLE_TERMINAL order=%llu event=execute_raw tick=%llu thread=%u "
        "exec=0x%08X raw=0x%08X owner_link=0x%08X lr=0x%08X\n",
        static_cast<unsigned long long>(title_terminal_next_order()),
        static_cast<unsigned long long>(tick), current_guest_thread_id, exec,
        raw, owner_link, static_cast<std::uint32_t>(context.lr));
    if (owner_link != trace.owner) {
      return;
    }
    const auto prior_raw_bit = title_terminal_raw_mask(raw);
    if (trace.terminal_seen) {
      if (prior_raw_bit != 0U &&
          (trace.qualified_raw_mask & prior_raw_bit) != 0U) {
        trace.post_redrain = true;
        title_terminal_positive("earlier_raw_redrained", tick);
      }
      return;
    }
    if (trace.terminal_pending) {
      if (prior_raw_bit != 0U || raw == 0x1788U) {
        title_terminal_finish("INCONCLUSIVE", "raw_before_terminal_store",
                              tick);
      }
      return;
    }
    if (prior_raw_bit != 0U) {
      trace.qualified_raw_mask |= prior_raw_bit;
    }
    bool sequence_broken = false;
    if (raw == 0x146CU) {
      if (trace.natural_stage == 0U) {
        trace.natural_stage = 1U;
      } else {
        sequence_broken = true;
      }
    } else if (raw == 0x171CU) {
      if (trace.natural_stage == 1U) {
        trace.natural_stage = 2U;
      } else {
        sequence_broken = true;
      }
    } else if (raw == 0x1788U) {
      if (trace.natural_stage == 2U) {
        trace.natural_stage = 3U;
        trace.pending_exec = exec;
        trace.pending_receiver = 0U;
        trace.terminal_exec_tick = tick;
        trace.terminal_exec_thread = current_guest_thread_id;
        trace.opcode2_pending = false;
      } else {
        sequence_broken = true;
      }
    } else if (trace.natural_stage != 0U) {
      sequence_broken = true;
    }
    if (sequence_broken) {
      title_terminal_finish("INCONCLUSIVE", "natural_raw_sequence_broken",
                            tick);
    }
    return;
  }
  if (title_terminal_name_is(generated_name, "sub_82324248")) {
    const auto exec = context.r3.u32;
    const auto receiver = title_terminal_read_u32(memory, exec + 0x18U);
    std::fprintf(
        stderr,
        "AC6_TITLE_TERMINAL order=%llu event=opcode2 tick=%llu thread=%u "
        "exec=0x%08X receiver=0x%08X lr=0x%08X qualified=%u\n",
        static_cast<unsigned long long>(title_terminal_next_order()),
        static_cast<unsigned long long>(tick), current_guest_thread_id, exec,
        receiver, static_cast<std::uint32_t>(context.lr),
        exec == trace.pending_exec && tick == trace.terminal_exec_tick &&
                current_guest_thread_id == trace.terminal_exec_thread &&
                receiver == trace.owner
            ? 1U
            : 0U);
    if (exec == trace.pending_exec && tick == trace.terminal_exec_tick &&
        current_guest_thread_id == trace.terminal_exec_thread &&
        receiver == trace.owner) {
      trace.pending_receiver = receiver;
      trace.opcode2_pending = true;
      trace.terminal_opcode_lr = static_cast<std::uint32_t>(context.lr);
    } else if (exec == trace.pending_exec) {
      title_terminal_finish("INCONCLUSIVE", "opcode2_correlation_failed",
                            tick);
    }
    return;
  }
  if (title_terminal_name_is(generated_name, "sub_82322CD8") &&
      trace.opcode2_pending) {
    if (tick != trace.terminal_exec_tick ||
        current_guest_thread_id != trace.terminal_exec_thread ||
        context.r3.u32 != trace.pending_receiver ||
        static_cast<std::uint32_t>(context.lr) != trace.terminal_opcode_lr) {
      title_terminal_finish("INCONCLUSIVE", "terminal_helper_correlation_failed",
                            tick);
      return;
    }
    std::uint32_t depth{};
    const auto terminal =
        title_terminal_follow_chain(memory, context.r3.u32, &depth);
    const bool terminal_mapped =
        terminal != 0U && memory.mapped(terminal + 0xD5U, 1U);
    const auto terminal_d5 =
        terminal_mapped ? memory.load_u8(terminal + 0xD5U) : 0U;
    trace.terminal = terminal;
    trace.terminal_pending = terminal_mapped;
    trace.terminal_tick = tick;
    trace.pending_exec = 0U;
    trace.opcode2_pending = false;
    trace.terminal_opcode_lr = 0U;
    std::fprintf(
        stderr,
        "AC6_TITLE_TERMINAL order=%llu event=terminal_chain tick=%llu "
        "thread=%u receiver=0x%08X terminal=0x%08X depth=%u D5_before=%u\n",
        static_cast<unsigned long long>(title_terminal_next_order()),
        static_cast<unsigned long long>(tick), current_guest_thread_id,
        context.r3.u32, terminal, depth, static_cast<unsigned>(terminal_d5));
    if (!terminal_mapped) {
      title_terminal_finish("INCONCLUSIVE", "terminal_chain_unmapped", tick);
    }
  }
}

[[nodiscard]] bool trace_title_terminal_wrap_drain(
    std::uint32_t owner) noexcept {
  return active_bridge != nullptr &&
         title_terminal_trace_open(active_bridge->tick()) &&
         title_terminal_trace.anchored && owner == title_terminal_trace.owner;
}

void trace_title_terminal_drain_exit(std::uint32_t owner,
                                     bool threw) noexcept {
  if (active_bridge == nullptr || !title_terminal_trace_enabled()) {
    return;
  }
  auto &trace = title_terminal_trace;
  if (trace.done) {
    return;
  }
  const auto tick = active_bridge->tick();
  const auto state = title_terminal_movie_state(active_bridge->memory(), owner);
  title_terminal_remember_movie(state);
  std::fprintf(
      stderr,
      "AC6_TITLE_TERMINAL order=%llu event=drain_exit tick=%llu thread=%u "
      "owner=0x%08X threw=%u mapped=%u D5=%u D6=%u D8=0x%08X "
      "DC=0x%08X E0=0x%08X F8=0x%08X\n",
      static_cast<unsigned long long>(title_terminal_next_order()),
      static_cast<unsigned long long>(tick), current_guest_thread_id, owner,
      threw ? 1U : 0U, state.mapped ? 1U : 0U,
      static_cast<unsigned>(state.d5), static_cast<unsigned>(state.d6),
      state.d8, state.dc, state.e0, state.f8);
  if (threw) {
    trace.post_state_invalid = true;
    title_terminal_finish("INCONCLUSIVE", "drain_body_exception", tick);
    return;
  }
  if (trace.terminal_pending) {
    title_terminal_finish("INCONCLUSIVE", "terminal_d5_store_missing", tick);
    return;
  }
  if (trace.terminal_seen && !trace.terminal_drain_complete) {
    trace.baseline = state;
    trace.baseline_ready = state.mapped;
    trace.terminal_d5_baseline_mapped =
        trace.terminal != 0U &&
        active_bridge->memory().mapped(trace.terminal + 0xD5U, 1U);
    if (trace.terminal_d5_baseline_mapped) {
      trace.terminal_d5_baseline =
          active_bridge->memory().load_u8(trace.terminal + 0xD5U);
    }
    trace.terminal_drain_complete = trace.baseline_ready &&
                                    trace.terminal_d5_baseline_mapped &&
                                    trace.terminal_d5_baseline == 0U;
    if (!trace.terminal_drain_complete) {
      title_terminal_finish("INCONCLUSIVE", "terminal_baseline_invalid",
                            tick);
      return;
    }
    std::fprintf(
        stderr,
        "AC6_TITLE_TERMINAL order=%llu event=terminal_committed tick=%llu "
        "owner=0x%08X terminal=0x%08X terminal_D5=%u\n",
        static_cast<unsigned long long>(title_terminal_next_order()),
        static_cast<unsigned long long>(tick), trace.owner, trace.terminal,
        static_cast<unsigned>(trace.terminal_d5_baseline));
  }
}

struct TitleTerminalWatchedField final {
  std::uint32_t address{};
  std::uint8_t size{};
  const char *name{};
};

struct TitleTerminalStoreFieldObservation final {
  TitleTerminalWatchedField field{};
  std::uint32_t old_value{};
};

struct TitleTerminalStoreObservation final {
  bool active{};
  std::uint64_t tick{};
  std::uint32_t thread{};
  std::uint32_t lr{};
  std::uint32_t address{};
  std::uint32_t size{};
  const char *generated_name{};
  std::uint32_t generated_line{};
  std::array<TitleTerminalStoreFieldObservation, 20U> fields{};
  std::size_t count{};
};

[[nodiscard]] std::uint32_t title_terminal_field_value(
    GuestMemory &memory, std::uint32_t address, std::uint8_t size) noexcept {
  std::uint32_t value{};
  for (std::uint8_t index = 0U; index < size; ++index) {
    value = (value << 8U) | memory.load_u8(address + index);
  }
  return value;
}

[[nodiscard]] TitleTerminalStoreObservation
trace_title_terminal_prepare_store(const PPCContext &context,
                                   std::uint32_t address,
                                   std::uint32_t size,
                                   const char *generated_name,
                                   std::uint32_t generated_line) noexcept {
  TitleTerminalStoreObservation observation{};
  if (active_bridge == nullptr || size == 0U) {
    return observation;
  }
  const auto tick = active_bridge->tick();
  const bool terminal_open = title_terminal_trace_open(tick);
  const bool factory_publish_open =
      title_terminal_trace.movie_factory.awaiting_publish &&
      title_movie_factory_trace_open(tick);
  if (!terminal_open && !factory_publish_open) {
    return observation;
  }
  auto &trace = title_terminal_trace;
  auto &memory = active_bridge->memory();
  std::array<TitleTerminalWatchedField, 20U> fields{};
  std::size_t count{};
  const auto add = [&](std::uint32_t field_address, std::uint8_t size,
                       const char *name) {
    if (field_address != 0U && count < fields.size() &&
        memory.mapped(field_address, size)) {
      fields[count++] = {field_address, size, name};
    }
  };
  if (terminal_open) {
    if (trace.anchored) {
      add(trace.owner + 0xD5U, 1U, "owner_D5");
      add(trace.owner + 0xD6U, 1U, "owner_D6");
      add(trace.owner + 0xD8U, 4U, "owner_D8");
      add(trace.owner + 0xDCU, 4U, "owner_DC");
      add(trace.owner + 0xE0U, 4U, "owner_E0");
      add(trace.owner + 0xF8U, 4U, "owner_F8");
    }
    if (trace.terminal != 0U && trace.terminal != trace.owner) {
      add(trace.terminal + 0xD5U, 1U, "terminal_D5");
    }
    const auto manager = title_terminal_read_u32(memory, 0x827435F8U);
    const auto title =
        manager == 0U ? 0U : title_terminal_read_u32(memory, manager + 8U);
    if (manager != 0U) {
      add(manager + 0x08U, 4U, "manager_08");
      add(manager + 0x10U, 4U, "manager_10");
      add(manager + 0x14U, 4U, "manager_14");
      add(manager + 0x18U, 4U, "manager_18");
      add(manager + 0x28U, 4U, "manager_28");
      add(manager + 0x2CU, 4U, "manager_2C");
      add(manager + 0x5DU, 1U, "manager_5D");
      add(manager + 0x5FU, 1U, "manager_5F");
    }
    if (title != 0U) {
      add(title + 0x0CU, 4U, "title_0C");
      add(title + 0x44U, 4U, "title_44");
      add(title + 0x70U, 4U, "title_70");
    }
  }
  if (factory_publish_open && trace.movie_factory.aggregate_mapped) {
    add(title_movie_field_address(trace.movie_factory.aggregate, 0xF4U), 4U,
        "movie_factory_A_F4");
  }

  observation.tick = tick;
  observation.thread = current_guest_thread_id;
  observation.lr = static_cast<std::uint32_t>(context.lr);
  observation.address = address;
  observation.size = size;
  observation.generated_name = generated_name;
  observation.generated_line = generated_line;
  const auto store_end = static_cast<std::uint64_t>(address) + size;
  for (std::size_t index = 0U; index < count; ++index) {
    const auto &field = fields[index];
    const auto field_end = static_cast<std::uint64_t>(field.address) + field.size;
    if (address >= field_end || store_end <= field.address) {
      continue;
    }
    observation.fields[observation.count++] = {
        field, title_terminal_field_value(memory, field.address, field.size)};
  }
  observation.active = observation.count != 0U;
  return observation;
}

void trace_title_terminal_commit_store(
    const TitleTerminalStoreObservation &observation) noexcept {
  if (!observation.active || active_bridge == nullptr) {
    return;
  }
  auto &trace = title_terminal_trace;
  if (trace.done && !trace.movie_factory.awaiting_publish) {
    return;
  }
  const auto tick = active_bridge->tick();
  if (tick != observation.tick ||
      current_guest_thread_id != observation.thread) {
    if (trace.movie_factory.awaiting_publish) {
      title_movie_factory_done("INCONCLUSIVE", "publish_context_changed",
                               tick);
    }
    if (!trace.done) {
      title_terminal_finish("INCONCLUSIVE", "store_commit_context_changed",
                            tick);
    }
    return;
  }
  auto &memory = active_bridge->memory();
  for (std::size_t index = 0U; index < observation.count; ++index) {
    const auto &field_observation = observation.fields[index];
    const auto &field = field_observation.field;
    const auto old_value = field_observation.old_value;
    const auto new_value =
        title_terminal_field_value(memory, field.address, field.size);
    const auto field_name = std::string_view(field.name);
    if (field_name == "movie_factory_A_F4") {
      auto &join = trace.movie_factory;
      const bool qualified =
          join.awaiting_publish && tick == join.tick &&
          observation.thread == join.thread &&
          observation.address == field.address && observation.size == 4U &&
          observation.lr == 0x823223F8U &&
          title_terminal_name_is(observation.generated_name,
                                 "sub_82322300") &&
          join.old_publish_mapped && old_value == join.old_publish &&
          new_value == join.result;
      std::fprintf(
          stderr,
          "AC6_TITLE_MOVIE_FACTORY order=%llu event=factory_publish "
          "tick=%llu thread=%u serial=%llu A=0x%08X "
          "field_address=0x%08X store_address=0x%08X store_size=%u "
          "old=0x%08X new=0x%08X expected_old=0x%08X "
          "expected_result=0x%08X lr=0x%08X function=%s "
          "generated_line=%u qualified=%u\n",
          static_cast<unsigned long long>(title_terminal_next_order()),
          static_cast<unsigned long long>(tick), observation.thread,
          static_cast<unsigned long long>(join.serial), join.aggregate,
          field.address, observation.address, observation.size, old_value,
          new_value, join.old_publish, join.result, observation.lr,
          observation.generated_name == nullptr ? ""
                                                : observation.generated_name,
          observation.generated_line, qualified ? 1U : 0U);
      title_movie_factory_done(qualified ? "OBSERVED" : "INCONCLUSIVE",
                               qualified ? "root_publish_joined"
                                         : "root_publish_mismatch",
                               tick);
      continue;
    }
    std::fprintf(
        stderr,
        "AC6_TITLE_TERMINAL order=%llu event=watched_store_committed tick=%llu "
        "thread=%u field=%s field_address=0x%08X store_address=0x%08X "
        "store_size=%u old=0x%08X new=0x%08X lr=0x%08X "
        "function=%s generated_line=%u\n",
        static_cast<unsigned long long>(title_terminal_next_order()),
        static_cast<unsigned long long>(tick), observation.thread, field.name,
        field.address, observation.address, observation.size, old_value,
        new_value, observation.lr,
        observation.generated_name == nullptr ? "" : observation.generated_name,
        observation.generated_line);

    if (!trace.terminal_seen && field_name == "owner_D8") {
      title_terminal_remember(trace.d8_history, trace.d8_history_count,
                              trace.d8_history_overflow, old_value);
      title_terminal_remember(trace.d8_history, trace.d8_history_count,
                              trace.d8_history_overflow, new_value);
    } else if (!trace.terminal_seen && field_name == "owner_DC") {
      title_terminal_remember(trace.dc_history, trace.dc_history_count,
                              trace.dc_history_overflow, old_value);
      title_terminal_remember(trace.dc_history, trace.dc_history_count,
                              trace.dc_history_overflow, new_value);
    }
    const bool qualifying_terminal_store =
        trace.terminal_pending &&
        field.address == trace.terminal + 0xD5U && old_value != 0U &&
        new_value == 0U &&
        tick == trace.terminal_exec_tick &&
        observation.thread == trace.terminal_exec_thread &&
        title_terminal_name_is(observation.generated_name, "sub_82322CE4");
    if (qualifying_terminal_store) {
      trace.terminal_pending = false;
      trace.terminal_seen = true;
      std::fprintf(
          stderr,
          "AC6_TITLE_TERMINAL order=%llu event=terminal_store_qualified "
          "tick=%llu thread=%u terminal=0x%08X old_D5=%u new_D5=%u "
          "function=%s generated_line=%u\n",
          static_cast<unsigned long long>(title_terminal_next_order()),
          static_cast<unsigned long long>(tick), observation.thread,
          trace.terminal, old_value, new_value,
          observation.generated_name == nullptr ? ""
                                                : observation.generated_name,
          observation.generated_line);
      continue;
    }
    if (!trace.terminal_seen) {
      continue;
    }
    const bool movie_field = field_name.starts_with("owner_") ||
                             field_name == "terminal_D5";
    const bool task_field = field_name.starts_with("manager_") ||
                            field_name.starts_with("title_");
    const bool changed = old_value != new_value;
    if (task_field || (movie_field && changed)) {
      trace.post_state_invalid = true;
    }
    if (!changed) {
      continue;
    }
    if (field_name == "owner_D6" && old_value == 0U && new_value == 1U) {
      title_terminal_positive("owner_D6_rearmed", tick);
    } else if (field_name == "owner_D5" && old_value == 0U &&
               new_value == 1U) {
      title_terminal_positive("owner_D5_rearmed", tick);
    } else if (field_name == "terminal_D5" && old_value == 0U &&
               new_value == 1U) {
      title_terminal_positive("terminal_D5_rearmed", tick);
    } else if (field_name == "owner_D8" &&
               title_terminal_remembered(trace.d8_history,
                                          trace.d8_history_count, new_value)) {
      title_terminal_positive("D8_returned_to_preterminal_value", tick);
    } else if (field_name == "owner_DC" &&
               title_terminal_remembered(trace.dc_history,
                                          trace.dc_history_count, new_value)) {
      title_terminal_positive("DC_returned_to_preterminal_value", tick);
    } else if (field_name == "title_0C" &&
               new_value == 2U) {
      title_terminal_positive("title_state_2", tick);
    } else if (field_name == "manager_18" && old_value == 0U &&
               new_value == 1U) {
      title_terminal_positive("manager_18_armed", tick);
    } else if ((field_name == "manager_10" ||
                field_name == "manager_14") &&
               new_value != 0U) {
      title_terminal_positive("manager_factory_published", tick);
    } else if (field_name == "manager_08" && new_value != 0U) {
      title_terminal_positive("manager_task_replaced", tick);
    }
    if (trace.done) {
      return;
    }
  }
}

void trace_title_terminal_tick(GuestBridge &bridge,
                               std::uint64_t new_tick) noexcept {
  if (!title_terminal_trace_enabled()) {
    return;
  }
  auto &trace = title_terminal_trace;
  if (trace.current_tick == new_tick) {
    return;
  }
  auto &factory = trace.movie_factory;
  if (factory.active && new_tick != factory.tick) {
    title_movie_factory_done("INCONCLUSIVE", "factory_crossed_tick",
                             new_tick);
  } else if (factory.awaiting_publish && new_tick != factory.tick) {
    title_movie_factory_done("INCONCLUSIVE", "root_publish_missing",
                             new_tick);
  } else if (!factory.done && factory.serial == 0U &&
             new_tick > kTitleTerminalTracePreAnchorCap) {
    title_movie_factory_done("INCONCLUSIVE", "factory_missing", new_tick);
  }
  if (trace.expected_add && new_tick != trace.expected_add_tick) {
    title_terminal_reset_candidate();
  }
  if (trace.current_tick != std::numeric_limits<std::uint64_t>::max() &&
      trace.terminal_drain_complete && !trace.done) {
    const auto state = title_terminal_movie_state(bridge.memory(), trace.owner);
    if (trace.current_tick > trace.terminal_tick &&
               trace.current_tick <=
                   trace.terminal_tick + kTitleTerminalTracePostTicks) {
      ++trace.post_ticks;
      if (trace.drains_this_tick != 0U) {
        ++trace.post_ticks_with_drain;
      }
      const bool terminal_d5_mapped =
          trace.terminal != 0U &&
          bridge.memory().mapped(trace.terminal + 0xD5U, 1U);
      const auto terminal_d5 = terminal_d5_mapped
                                   ? bridge.memory().load_u8(trace.terminal +
                                                             0xD5U)
                                   : 0U;
      const bool stable = trace.baseline_ready && state.mapped &&
                          state.d5 == trace.baseline.d5 &&
                          state.d6 == 0U &&
                          state.d8 == trace.baseline.d8 &&
                          state.dc == trace.baseline.dc &&
                          state.e0 == trace.baseline.e0 &&
                          state.f8 == trace.baseline.f8 &&
                          terminal_d5_mapped &&
                          terminal_d5 == trace.terminal_d5_baseline;
      trace.post_state_invalid |= !stable;
      std::fprintf(
          stderr,
          "AC6_TITLE_TERMINAL order=%llu event=post_tick_close tick=%llu "
          "owner=0x%08X drains=%u stable=%u D5=%u D6=%u D8=0x%08X "
          "DC=0x%08X E0=0x%08X F8=0x%08X terminal_D5=%u\n",
          static_cast<unsigned long long>(title_terminal_next_order()),
          static_cast<unsigned long long>(trace.current_tick), trace.owner,
          trace.drains_this_tick, stable ? 1U : 0U,
          static_cast<unsigned>(state.d5), static_cast<unsigned>(state.d6),
          state.d8, state.dc, state.e0, state.f8,
          static_cast<unsigned>(terminal_d5));
    }
  }
  trace.current_tick = new_tick;
  trace.drains_this_tick = 0U;
  if (trace.done) {
    return;
  }
  if (!trace.anchored && new_tick > kTitleTerminalTracePreAnchorCap) {
    title_terminal_finish("INCONCLUSIVE", "anchor_missing", new_tick);
    return;
  }
  if ((trace.terminal_pending ||
       (trace.terminal_seen && !trace.terminal_drain_complete)) &&
      new_tick > trace.terminal_tick) {
    title_terminal_finish("INCONCLUSIVE", "terminal_drain_incomplete",
                          new_tick);
    return;
  }
  if (trace.terminal_drain_complete &&
      new_tick > trace.terminal_tick + kTitleTerminalTracePostTicks) {
    const bool negative = trace.post_ticks == kTitleTerminalTracePostTicks &&
                          trace.post_ticks_with_drain == trace.post_ticks &&
                          !trace.post_state_invalid && !trace.post_redrain &&
                          !trace.d8_history_overflow &&
                          !trace.dc_history_overflow;
    title_terminal_finish(negative ? "NEGATIVE_BOUNDED" : "INCONCLUSIVE",
                          negative ? "stable_drain_window"
                                   : "post_window_conditions_failed",
                          new_tick);
    return;
  }
  if (trace.anchored &&
      new_tick > trace.anchor_tick + kTitleTerminalTraceAnchorCap) {
    title_terminal_finish("INCONCLUSIVE", "anchor_hard_cap", new_tick);
  }
}

} // namespace
