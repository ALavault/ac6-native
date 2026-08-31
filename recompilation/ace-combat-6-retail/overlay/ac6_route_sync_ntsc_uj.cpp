#include <atomic>
#include <cstdint>

#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/ppc/function.h>

// Read-only route synchronization for the qualified NTSC-U/J default.xex.
// Each strong wrapper delegates to the generated implementation first and
// only observes guest words afterward. No guest state is written here.

PPC_EXTERN_FUNC(__imp__rex_sub_820943B0);
PPC_EXTERN_FUNC(__imp__rex_sub_82158D90);
PPC_EXTERN_FUNC(__imp__rex_sub_8218F4F0);
PPC_EXTERN_FUNC(__imp__rex_sub_82196590);
PPC_EXTERN_FUNC(__imp__rex_sub_821A6400);
PPC_EXTERN_FUNC(__imp__rex_sub_821C3800);
PPC_EXTERN_FUNC(__imp__rex_sub_821C5268);
PPC_EXTERN_FUNC(__imp__rex_sub_821C5708);
PPC_EXTERN_FUNC(__imp__rex_sub_82267160);
PPC_EXTERN_FUNC(__imp__rex_sub_82267258);
PPC_EXTERN_FUNC(__imp__rex_sub_822ED310);
PPC_EXTERN_FUNC(__imp__rex_sub_82256490);
PPC_EXTERN_FUNC(__imp__rex_sub_8226C068);

namespace {

uint32_t GuestWord(uint8_t *base, uint32_t address) {
  return address ? rex::memory::load_and_swap<uint32_t>(base + address) : 0;
}

uint8_t GuestByte(uint8_t *base, uint32_t address) {
  return address ? rex::memory::load_and_swap<uint8_t>(base + address) : 0;
}

struct SaveSnapshot {
  uint32_t state;
  uint32_t selector;
  uint32_t response;
  uint32_t type;
  uint32_t result;

  bool operator==(const SaveSnapshot &) const = default;
};

SaveSnapshot ReadSaveSnapshot(uint8_t *base, uint32_t screen) {
  return {
      GuestWord(base, screen + 40), GuestWord(base, screen + 44),
      GuestWord(base, screen + 12), GuestWord(base, screen + 28),
      GuestWord(base, screen + 36),
  };
}

struct SaveManagerSnapshot {
  uint32_t operation;
  uint8_t flag34;
  uint32_t result;
  uint32_t word3c;
  uint8_t flag40;
  uint8_t flag41;
  uint8_t flag42;
  uint8_t flag43;

  bool operator==(const SaveManagerSnapshot &) const = default;
};

SaveManagerSnapshot ReadSaveManagerSnapshot(uint8_t *base, uint32_t manager) {
  return {
      GuestWord(base, manager + 0x30), GuestByte(base, manager + 0x34),
      GuestWord(base, manager + 0x38), GuestWord(base, manager + 0x3C),
      GuestByte(base, manager + 0x40), GuestByte(base, manager + 0x41),
      GuestByte(base, manager + 0x42), GuestByte(base, manager + 0x43),
  };
}

struct Mission01SchedulerSnapshot {
  uint32_t step;
  uint32_t elapsed;
  uint32_t step_table;
  uint32_t active;

  bool operator==(const Mission01SchedulerSnapshot &) const = default;
};

Mission01SchedulerSnapshot ReadMission01SchedulerSnapshot(uint8_t *base,
                                                          uint32_t self) {
  return {
      GuestWord(base, self + 0x10), GuestWord(base, self + 0x14),
      GuestWord(base, self + 0x264), GuestWord(base, self + 0x268),
  };
}

} // namespace

PPC_FUNC_IMPL(rex_sub_820943B0) {
  PPC_FUNC_PROLOGUE();
  const uint32_t profile = ctx.r3.u32;
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);
  const uint32_t selector = profile ? GuestWord(base, profile + 8) : UINT32_MAX;
  __imp__rex_sub_820943B0(ctx, base);

  const uint32_t value = ctx.r3.u32;
  static thread_local uint32_t last_profile = 0;
  static thread_local uint32_t last_selector = UINT32_MAX;
  static thread_local uint32_t last_value = UINT32_MAX;
  static thread_local bool primed = false;
  static std::atomic<uint32_t> lines{0};
  if ((!primed || profile != last_profile || selector != last_selector ||
       value != last_value) &&
      lines.fetch_add(1, std::memory_order_relaxed) < 64) {
    REXLOG_INFO(
        "[ac6-current-level] profile=0x{:08X} selector={} value={} lr=0x{:08X}",
        profile, static_cast<int32_t>(selector), static_cast<int32_t>(value),
        caller);
    last_profile = profile;
    last_selector = selector;
    last_value = value;
    primed = true;
  }
}

PPC_FUNC_IMPL(rex_sub_82158D90) {
  PPC_FUNC_PROLOGUE();
  const uint32_t manager = ctx.r3.u32;
  const SaveManagerSnapshot before =
      manager ? ReadSaveManagerSnapshot(base, manager) : SaveManagerSnapshot{};
  __imp__rex_sub_82158D90(ctx, base);
  if (!manager) {
    return;
  }

  const SaveManagerSnapshot after = ReadSaveManagerSnapshot(base, manager);
  static thread_local uint32_t last_manager = 0;
  static thread_local SaveManagerSnapshot last{};
  static thread_local bool primed = false;
  static std::atomic<uint32_t> lines{0};
  if ((!primed || manager != last_manager || !(after == last) ||
       !(before == after)) &&
      lines.fetch_add(1, std::memory_order_relaxed) < 256) {
    REXLOG_INFO("[ac6-save-manager] manager=0x{:08X} "
                "op30={}->{} flag34={}->{} result38={}->{} word3c={}->{} "
                "flag40={}->{} flag41={}->{} flag42={}->{} flag43={}->{}",
                manager, before.operation, after.operation,
                static_cast<uint32_t>(before.flag34),
                static_cast<uint32_t>(after.flag34), before.result,
                after.result, before.word3c, after.word3c,
                static_cast<uint32_t>(before.flag40),
                static_cast<uint32_t>(after.flag40),
                static_cast<uint32_t>(before.flag41),
                static_cast<uint32_t>(after.flag41),
                static_cast<uint32_t>(before.flag42),
                static_cast<uint32_t>(after.flag42),
                static_cast<uint32_t>(before.flag43),
                static_cast<uint32_t>(after.flag43));
    last_manager = manager;
    last = after;
    primed = true;
  }
}

PPC_FUNC_IMPL(rex_sub_82196590) {
  PPC_FUNC_PROLOGUE();
  const uint32_t profile = ctx.r3.u32;
  const uint32_t value = ctx.r4.u32;
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);
  const uint32_t selector = profile ? GuestWord(base, profile + 8) : UINT32_MAX;
  __imp__rex_sub_82196590(ctx, base);

  if (caller == 0x821A64D0 || caller == 0x821A64E8) {
    REXLOG_INFO("[ac6-current-level-set] profile=0x{:08X} selector={} value={} "
                "lr=0x{:08X}",
                profile, static_cast<int32_t>(selector),
                static_cast<int32_t>(value), caller);
  }
}

PPC_FUNC_IMPL(rex_sub_821A6400) {
  PPC_FUNC_PROLOGUE();
  const uint32_t self = ctx.r3.u32;
  static std::atomic<uint32_t> lines{0};
  if (lines.fetch_add(1, std::memory_order_relaxed) < 16) {
    REXLOG_INFO(
        "[ac6-post-mission] task=InterMissionSelect self=0x{:08X} event=enter",
        self);
  }
  __imp__rex_sub_821A6400(ctx, base);
}

PPC_FUNC_IMPL(rex_sub_82267160) {
  PPC_FUNC_PROLOGUE();
  const uint32_t self = ctx.r3.u32;
  const Mission01SchedulerSnapshot before =
      self ? ReadMission01SchedulerSnapshot(base, self)
           : Mission01SchedulerSnapshot{};
  __imp__rex_sub_82267160(ctx, base);
  const Mission01SchedulerSnapshot after =
      self ? ReadMission01SchedulerSnapshot(base, self)
           : Mission01SchedulerSnapshot{};
  const uint32_t result = ctx.r3.u32;

  static thread_local Mission01SchedulerSnapshot last{};
  static thread_local uint32_t last_self = 0;
  static thread_local bool primed = false;
  static std::atomic<uint32_t> lines{0};
  if ((result != 0 || !primed || self != last_self || !(after == last)) &&
      lines.fetch_add(1, std::memory_order_relaxed) < 256) {
    REXLOG_INFO(
        "[ac6-m01-scheduler] self=0x{:08X} step={}->{} elapsed={}->{} "
        "table=0x{:08X} active=0x{:08X} result={}",
        self, before.step, after.step, before.elapsed, after.elapsed,
        after.step_table, after.active, static_cast<int32_t>(result));
    last = after;
    last_self = self;
    primed = true;
  }
}

PPC_FUNC_IMPL(rex_sub_82267258) {
  PPC_FUNC_PROLOGUE();
  const uint32_t counter_index = ctx.r4.u32;
  const uint32_t value_record = ctx.r5.u32;
  __imp__rex_sub_82267258(ctx, base);

  static std::atomic<uint32_t> lines{0};
  if (counter_index != 0xFFFF && counter_index < 339 &&
      lines.fetch_add(1, std::memory_order_relaxed) < 256) {
    REXLOG_INFO(
        "[ac6-m01-counter] index={} value_record=0x{:08X}",
        counter_index, value_record);
  }
}

PPC_FUNC_IMPL(rex_sub_822ED310) {
  PPC_FUNC_PROLOGUE();
  const uint32_t owner = ctx.r3.u32;
  const uint32_t object = ctx.r4.u32;
  const int32_t signal = ctx.r5.s32;
  const uint32_t context =
      object ? GuestWord(base, object + 0x820) : 0;
  __imp__rex_sub_822ED310(ctx, base);
  const uint32_t result = ctx.r3.u32;

  if (signal != -2) {
    return;
  }
  static thread_local uint32_t last_owner = 0;
  static thread_local uint32_t last_object = 0;
  static thread_local uint32_t last_context = 0;
  static thread_local uint32_t last_result = UINT32_MAX;
  static std::atomic<uint32_t> lines{0};
  if ((owner != last_owner || object != last_object || context != last_context ||
       result != last_result) &&
      lines.fetch_add(1, std::memory_order_relaxed) < 256) {
    REXLOG_INFO(
        "[ac6-m01-signal] owner=0x{:08X} object=0x{:08X} signal={} "
        "context820=0x{:08X} result=0x{:08X}",
        owner, object, signal, context, result);
    last_owner = owner;
    last_object = object;
    last_context = context;
    last_result = result;
  }
}

PPC_FUNC_IMPL(rex_sub_82256490) {
  PPC_FUNC_PROLOGUE();
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);
  const uint32_t r3 = ctx.r3.u32;
  const uint32_t r4 = ctx.r4.u32;
  __imp__rex_sub_82256490(ctx, base);

  // The US unit/object driver is reached through a native dispatch table and
  // has no explicit guest pointer argument in the qualified body. Keep this
  // as a bounded call-presence diagnostic; no guest state is written or read.
  static std::atomic<uint32_t> lines{0};
  if (lines.fetch_add(1, std::memory_order_relaxed) < 64) {
    REXLOG_INFO(
        "[ac6-m01-objective-driver] enter lr=0x{:08X} r3=0x{:08X} "
        "r4=0x{:08X}",
        caller, r3, r4);
  }
}

PPC_FUNC_IMPL(rex_sub_8226C068) {
  PPC_FUNC_PROLOGUE();
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);
  __imp__rex_sub_8226C068(ctx, base);

  // This evaluator reads the active scenario/counter graph and writes its
  // native objective state. Only call presence is observed here, capped for
  // a single diagnostic window; the wrapper never touches that state.
  static std::atomic<uint32_t> lines{0};
  if (lines.fetch_add(1, std::memory_order_relaxed) < 64) {
    REXLOG_INFO("[ac6-m01-objective-evaluator] enter lr=0x{:08X}", caller);
  }
}

PPC_FUNC_IMPL(rex_sub_821C3800) {
  PPC_FUNC_PROLOGUE();
  const uint32_t screen = ctx.r3.u32;
  __imp__rex_sub_821C3800(ctx, base);
  if (!screen) {
    return;
  }

  const SaveSnapshot snapshot = ReadSaveSnapshot(base, screen);
  static thread_local uint32_t last_screen = 0;
  static thread_local SaveSnapshot last{};
  static thread_local bool primed = false;
  static std::atomic<uint32_t> lines{0};
  if ((!primed || screen != last_screen || !(snapshot == last)) &&
      lines.fetch_add(1, std::memory_order_relaxed) < 256) {
    REXLOG_INFO("[ac6-save-route] screen=0x{:08X} state40={} selector44={} "
                "response12={} type28={} result36={}",
                screen, static_cast<int32_t>(snapshot.state),
                static_cast<int32_t>(snapshot.selector),
                static_cast<int32_t>(snapshot.response),
                static_cast<int32_t>(snapshot.type),
                static_cast<int32_t>(snapshot.result));
    last_screen = screen;
    last = snapshot;
    primed = true;
  }
}

PPC_FUNC_IMPL(rex_sub_821C5268) {
  PPC_FUNC_PROLOGUE();
  const uint32_t screen = ctx.r3.u32;
  const SaveSnapshot before =
      screen ? ReadSaveSnapshot(base, screen) : SaveSnapshot{};
  const uint32_t inner_before = screen ? GuestWord(base, screen + 68) : 0;
  __imp__rex_sub_821C5268(ctx, base);
  if (!screen) {
    return;
  }

  const SaveSnapshot after = ReadSaveSnapshot(base, screen);
  const uint32_t inner_after = GuestWord(base, screen + 68);
  if (!(before == after) || inner_before != inner_after) {
    REXLOG_INFO(
        "[ac6-save-task] screen=0x{:08X} inner68={}->{} selector44={}->{} "
        "response12={}->{} type28={}->{} result36={}->{} return={}",
        screen, inner_before, inner_after, before.selector, after.selector,
        before.response, after.response, before.type, after.type, before.result,
        after.result, ctx.r3.s32);
  }
}

PPC_FUNC_IMPL(rex_sub_821C5708) {
  PPC_FUNC_PROLOGUE();
  const uint32_t screen = ctx.r3.u32;
  const SaveSnapshot before =
      screen ? ReadSaveSnapshot(base, screen) : SaveSnapshot{};
  const uint32_t inner_before = screen ? GuestWord(base, screen + 68) : 0;
  __imp__rex_sub_821C5708(ctx, base);
  if (!screen) {
    return;
  }

  const SaveSnapshot after = ReadSaveSnapshot(base, screen);
  const uint32_t inner_after = GuestWord(base, screen + 68);
  if (!(before == after) || inner_before != inner_after) {
    REXLOG_INFO(
        "[ac6-save-state] screen=0x{:08X} inner68={}->{} selector44={}->{} "
        "response12={}->{} type28={}->{} result36={}->{} return={}",
        screen, inner_before, inner_after, before.selector, after.selector,
        before.response, after.response, before.type, after.type, before.result,
        after.result, ctx.r3.s32);
  }
}

PPC_FUNC_IMPL(rex_sub_8218F4F0) {
  PPC_FUNC_PROLOGUE();
  const uint32_t self = ctx.r3.u32;
  const uint32_t before_state = self ? GuestWord(base, self + 12) : UINT32_MAX;
  const uint32_t before_timer = self ? GuestWord(base, self + 72) : 0;
  __imp__rex_sub_8218F4F0(ctx, base);
  if (!self) {
    return;
  }

  const uint32_t after_state = GuestWord(base, self + 12);
  const uint32_t after_timer = GuestWord(base, self + 72);
  if (before_state != after_state) {
    REXLOG_INFO(
        "[ac6-campaign-transition] self=0x{:08X} state={}->{} timer={}->{}",
        self, before_state, after_state, before_timer, after_timer);
  }
}
