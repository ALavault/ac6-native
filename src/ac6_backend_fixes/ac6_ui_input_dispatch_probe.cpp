/**
 * ReXGlue runtime - AC6 Recompilation project
 * Copyright (c) 2026 Tom Clay. All rights reserved.
 *
 * Probes on the UI input path: which object drives the dispatcher, which way it
 * branches, what the guest's own edge detector produces, and which screen is on
 * top.
 *
 * Cycles 397-408 measured the whole host side of the stuck dialog and found it
 * correct: presentation runs at 60Hz, the guest resubmits ~56 draws per frame,
 * input reaches the guest with exact button masks, no thread spins, and the
 * viewport transform is right. The screen still redraws an unchanging image and
 * acts on none of the buttons it provably receives.
 *
 * Cycle 409 traced the input path to sub_8234D510, which has no direct caller
 * anywhere in the recompiled code and is therefore reached indirectly - the
 * shape of a per-screen handler held in an object field.
 *
 * What this file used to contain, and why it does not any more:
 *
 * Cycles 441-452 added a memory scanner here that FNV-hashed 32 MB of guest
 * address space every 120 frames and re-hashed it synchronously on every
 * DPAD_LEFT edge, plus a per-frame walk of the screen object and one level of
 * its pointers, plus two cvars that force-wrote candidate "selection" words.
 * Twelve cycles of that produced two candidate blocks, and both were eliminated
 * by forced write in cycles 450 and 452. The approach is also strictly weaker
 * than the question demands: a snapshot diff says that a word changed, never
 * who changed it, and it cannot see a field written back to the same value
 * within a frame.
 *
 * It is removed rather than left switched off. It ran at frame rate whenever
 * ac6_log_ui_dispatch was set, which is exactly the configuration every future
 * experiment on this screen will use, and hashing 32 MB inside the frame loop
 * perturbs the timing of the frames those experiments measure. Dead
 * instrumentation that distorts the live instrument is worse than no
 * instrumentation.
 *
 * The eliminated addresses are recorded in the cycle journal; nothing here
 * needs to carry them.
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/ppc/function.h>

#include "../ac6_guest_call_trace.h"
#include "../ac6_guest_text_probe.h"
#include "../ac6_ppc_store_watch.h"
#include "../ac6_dialog_text_fallback.h"
#include "../ac6_campaign_resource_bridge.h"
#include "../ac6_save_dialog_input_bridge.h"

REXCVAR_DEFINE_BOOL(ac6_log_ui_dispatch, false, "AC6/Input",
                    "Log the UI input dispatcher's object pointer and r11 branch.");
REXCVAR_DEFINE_BOOL(ac6_log_ui_dispatch_verbose, false, "AC6/Input",
                    "Log high-volume first-mission timeline probes.");
REXCVAR_DEFINE_BOOL(ac6_log_loadout_dispatch, false, "AC6/Input",
                    "Log the native aircraft/weapon selection consumers on A edges.");
REXCVAR_DEFINE_BOOL(ac6_trace_mission_edges, false, "AC6/Trace",
                    "Arm the guest call trace only after the first-campaign bridge.");
REXCVAR_DEFINE_INT32(ac6_force_screen_state, -1, "AC6/Input",
                     "P1: on each A press, force the screen's state word at [screen+68] to this "
                     "value (-1 = off). Bounds the problem by driving the transition the confirm "
                     "button fails to drive.");
REXCVAR_DEFINE_BOOL(ac6_force_loadout_ready, false, "AC6/Input",
                    "Probe: after the aircraft and special-weapon A edges, expose the native "
                    "loadout-ready status expected by the first-mission state machine.");
REXCVAR_DEFINE_BOOL(ac6_force_loadout_launch, false, "AC6/Input",
                    "Probe: route the qualified third loadout A through the native launch "
                    "branch (event 49 inactive, +0x60 status 5).");
REXCVAR_DEFINE_BOOL(ac6_pause_first_mission_dispatch, false, "AC6/Trace",
                    "Pause once at the first unknown child dispatch for guest-memory inspection.");
#ifdef AC6RECOMP_PROBE_GUEST_TEXT
REXCVAR_DEFINE_BOOL(ac6_log_text_draws, false, "AC6/Text",
                    "Log ASCII strings passed to the guest text renderer with their PPC caller.");
#endif

PPC_EXTERN_FUNC(__imp__rex_sub_8234D510);
PPC_EXTERN_FUNC(__imp__rex_sub_8234D210);
PPC_EXTERN_FUNC(__imp__rex_sub_821C56F8);
PPC_EXTERN_FUNC(__imp__sub_821C5258);
PPC_EXTERN_FUNC(__imp__sub_82343928);
PPC_EXTERN_FUNC(__imp__sub_82343AD0);
PPC_EXTERN_FUNC(__imp__sub_8234D3F0);
PPC_EXTERN_FUNC(__imp__sub_8234D478);
PPC_EXTERN_FUNC(__imp__sub_821C37E0);
PPC_EXTERN_FUNC(__imp__sub_821CFE18);
PPC_EXTERN_FUNC(__imp__sub_82158DF0);
PPC_EXTERN_FUNC(__imp__sub_821CA908);
PPC_EXTERN_FUNC(__imp__sub_821CAA50);
PPC_EXTERN_FUNC(__imp__sub_821CB5F0);
PPC_EXTERN_FUNC(__imp__sub_821C59B0);
PPC_EXTERN_FUNC(__imp__sub_8214C038);
PPC_EXTERN_FUNC(__imp__sub_8214C360);
PPC_EXTERN_FUNC(__imp__sub_8214C518);
PPC_EXTERN_FUNC(__imp__sub_8214D000);
PPC_EXTERN_FUNC(__imp__sub_8214D390);
PPC_EXTERN_FUNC(__imp__sub_820F6330);
PPC_EXTERN_FUNC(__imp__sub_820F62B0);
PPC_EXTERN_FUNC(__imp__sub_8214B5F0);
PPC_EXTERN_FUNC(__imp__sub_82146DB8);
PPC_EXTERN_FUNC(__imp__sub_8218C238);
PPC_EXTERN_FUNC(__imp__sub_82144F98);
PPC_EXTERN_FUNC(__imp__sub_82144FC8);
PPC_EXTERN_FUNC(__imp__sub_82144FD8);
PPC_EXTERN_FUNC(__imp__sub_821B3870);
PPC_EXTERN_FUNC(__imp__sub_821AFCA0);
PPC_EXTERN_FUNC(__imp__sub_821AFE38);
PPC_EXTERN_FUNC(__imp__sub_821F3BA0);
PPC_EXTERN_FUNC(__imp__sub_822FA748);
PPC_EXTERN_FUNC(__imp__sub_82221A28);
PPC_EXTERN_FUNC(__imp__sub_8221E6C0);
PPC_EXTERN_FUNC(__imp__sub_82199F68);
PPC_EXTERN_FUNC(__imp__sub_82199D08);
PPC_EXTERN_FUNC(__imp__sub_82199BD8);
PPC_EXTERN_FUNC(__imp__sub_821A7A70);
PPC_EXTERN_FUNC(__imp__sub_821BBF98);
PPC_EXTERN_FUNC(__imp__sub_821A7260);
PPC_EXTERN_FUNC(__imp__sub_821A72C0);
PPC_EXTERN_FUNC(__imp__sub_821A75D0);
PPC_EXTERN_FUNC(__imp__sub_821539E0);
PPC_EXTERN_FUNC(__imp__sub_821D29B0);
PPC_EXTERN_FUNC(__imp__sub_821D2860);
PPC_EXTERN_FUNC(__imp__sub_82123E18);
PPC_EXTERN_FUNC(__imp__sub_82124990);
PPC_EXTERN_FUNC(__imp__sub_821250F8);
PPC_EXTERN_FUNC(__imp__sub_82147070);
PPC_EXTERN_FUNC(__imp__sub_821482B8);
PPC_EXTERN_FUNC(__imp__sub_820D94B8);
PPC_EXTERN_FUNC(__imp__sub_820EA068);
PPC_EXTERN_FUNC(__imp__sub_823835D0);
PPC_EXTERN_FUNC(__imp__sub_8237C4D8);
PPC_EXTERN_FUNC(__imp__sub_8237C828);
PPC_EXTERN_FUNC(__imp__sub_8237BF08);
PPC_EXTERN_FUNC(__imp__sub_82387530);
PPC_EXTERN_FUNC(__imp__sub_8213AD60);
PPC_EXTERN_FUNC(__imp__sub_821C1130);
PPC_EXTERN_FUNC(__imp__sub_8218F4C8);
PPC_EXTERN_FUNC(__imp__sub_820D8FE0);
PPC_EXTERN_FUNC(__imp__sub_8237B4D8);
PPC_EXTERN_FUNC(__imp__sub_8237CC58);
PPC_EXTERN_FUNC(__imp__sub_8237BBC8);
PPC_EXTERN_FUNC(__imp__sub_8237BFD8);
PPC_EXTERN_FUNC(__imp__sub_8237D1B8);
PPC_EXTERN_FUNC(__imp__sub_8237EFF8);
PPC_EXTERN_FUNC(__imp__sub_8237EF50);
PPC_EXTERN_FUNC(__imp__sub_8237EED0);
PPC_EXTERN_FUNC(__imp__sub_8237E418);
PPC_EXTERN_FUNC(__imp__sub_820DB628);
PPC_EXTERN_FUNC(__imp__sub_820DB1F0);
PPC_EXTERN_FUNC(__imp__sub_820DB2C8);
PPC_EXTERN_FUNC(__imp__sub_820DB368);
PPC_EXTERN_FUNC(__imp__sub_820DB408);
PPC_EXTERN_FUNC(__imp__sub_820DB4C8);
PPC_EXTERN_FUNC(__imp__sub_820DB578);
PPC_EXTERN_FUNC(__imp__sub_820DA210);
PPC_EXTERN_FUNC(__imp__sub_820DA2E8);
PPC_EXTERN_FUNC(__imp__sub_820E3720);
PPC_EXTERN_FUNC(__imp__sub_820E3790);
PPC_EXTERN_FUNC(__imp__sub_820E37C0);
PPC_EXTERN_FUNC(__imp__sub_820E37F8);
PPC_EXTERN_FUNC(__imp__sub_820E3808);
PPC_EXTERN_FUNC(__imp__sub_8237CB10);
PPC_EXTERN_FUNC(__imp__sub_823B7338);
PPC_EXTERN_FUNC(__imp__rex_sub_820943B0);
PPC_EXTERN_FUNC(__imp___vsnprintf);
#ifdef AC6RECOMP_PROBE_GUEST_TEXT
PPC_EXTERN_FUNC(__imp__sub_820F8608);
PPC_EXTERN_FUNC(__imp__sub_820D7C08);
#endif

namespace {

// The raw mask of the most recent press edge, published by the edge detector
// and consumed by the screen update on its next call. 0 means "nothing new".
std::atomic<uint32_t> g_last_edge{0};
std::atomic<uint32_t> g_binding_probe_edge{0};
std::atomic<uint32_t> g_loadout_edge{0};
std::atomic<uint64_t> g_loadout_edge_serial{0};
thread_local uint32_t g_binding_probe_active_edge = 0;
ac6::save_dialog::InputBridge g_save_dialog_input_bridge;
std::atomic<bool> g_first_mission_stage_armed{false};
std::atomic<uint32_t> g_first_mission_loadout_a_count{0};
std::atomic<bool> g_allocator_empty_logged{false};
std::atomic<bool> g_first_mission_launch_started{false};
std::atomic<uint32_t> g_guest_fallback_cursor{0xBF000000u};
std::atomic<uint32_t> g_child_slot_missing_logs{0};
std::atomic<uint32_t> g_child_dispatch_logs{0};
std::atomic<uint32_t> g_child_secondary_dispatch_logs{0};
std::atomic<uint32_t> g_timeline_prepare_logs{0};
std::atomic<uint32_t> g_timeline_reset_logs{0};
std::atomic<uint32_t> g_timeline_repeat_logs{0};
thread_local uint32_t g_first_mission_timeline_state = 0;

// Set when the guest's own edge detector reports an A press, consumed by the
// screen update on its next call.
//
// Forcing a state the instant the screen parks tests nothing: the screen parks
// about half a second after it is created, which is long before the dialog is
// on screen and interactive. The forced value then executes against a screen
// that has not yet asked anybody anything. Tying the force to the A edge asks
// the actual question instead -- "if A produced this transition, what would
// happen" -- at the moment A is pressed.
std::atomic<bool> g_confirm_pressed{false};

bool LoadoutProbeEnabled() {
  return REXCVAR_GET(ac6_log_loadout_dispatch) &&
         g_first_mission_stage_armed.load(std::memory_order_relaxed);
}

// The launch probe is only valid after the first-campaign resource bridge has
// armed the loadout flow and the guest has consumed the three expected A
// edges (aircraft, special weapon, confirmation).  Keep this independent of
// the logging edge: the state machine calls its predicates after the edge has
// already been cleared.
bool FirstMissionLaunchOverrideEnabled() {
  return REXCVAR_GET(ac6_force_loadout_launch) &&
         g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
         g_first_mission_loadout_a_count.load(std::memory_order_relaxed) >= 3u;
}

struct LoadoutEdge {
  uint32_t mask;
  uint64_t serial;
};

LoadoutEdge LastLoadoutEdge() {
  return {g_loadout_edge.load(std::memory_order_relaxed),
          g_loadout_edge_serial.load(std::memory_order_relaxed)};
}

uint32_t GuestWord(uint8_t* base, uint32_t address) {
  return address ? rex::memory::load_and_swap<uint32_t>(base + address) : 0;
}

uint32_t FirstMissionFallbackAllocate(uint8_t* base, uint32_t request) {
  if (!g_first_mission_launch_started.load(std::memory_order_relaxed) ||
      request == 0 || request > 0x01000000u) {
    return 0;
  }
  constexpr uint32_t kFallbackBegin = 0xBF000000u;
  constexpr uint32_t kFallbackEnd = 0xC0000000u;
  const uint32_t rounded = (request + 63u) & ~63u;
  const uint32_t fallback =
      g_guest_fallback_cursor.fetch_add(rounded, std::memory_order_relaxed);
  const uint64_t end = static_cast<uint64_t>(fallback) + rounded;
  if (fallback < kFallbackBegin || end > kFallbackEnd) {
    REXLOG_ERROR(
        "[ac6-guest-allocator-fallback-exhausted] request={} cursor=0x{:08X}",
        request, fallback);
    return 0;
  }
  std::memset(base + fallback, 0, rounded);
  if (!g_allocator_empty_logged.exchange(true, std::memory_order_relaxed)) {
    REXLOG_WARN(
        "[ac6-guest-allocator-fallback] request={} guest=0x{:08X} "
        "range=0x{:08X}-0x{:08X}",
        request, fallback, kFallbackBegin, kFallbackEnd);
  }
  return fallback;
}

// A saved campaign can arrive at the exact first-mission resource call with
// the package index already promoted to 1.  In that case the PAL setter has no
// 0 -> 1 result for the bridge to observe, but the task/loadout probes still
// need the same narrowly qualified stage marker.  This only publishes a
// diagnostic epoch; it does not write guest memory or alter the resource
// result.
void ArmFirstMissionStage(const char* source) {
  bool expected = false;
  if (!g_first_mission_stage_armed.compare_exchange_strong(
          expected, true, std::memory_order_relaxed)) {
    return;
  }
  g_first_mission_loadout_a_count.store(0, std::memory_order_relaxed);
  g_allocator_empty_logged.store(false, std::memory_order_relaxed);
  g_first_mission_launch_started.store(false, std::memory_order_relaxed);
  g_guest_fallback_cursor.store(0xBF000000u, std::memory_order_relaxed);
  g_child_slot_missing_logs.store(0, std::memory_order_relaxed);
  g_child_dispatch_logs.store(0, std::memory_order_relaxed);
  g_child_secondary_dispatch_logs.store(0, std::memory_order_relaxed);
  g_timeline_prepare_logs.store(0, std::memory_order_relaxed);
  g_timeline_reset_logs.store(0, std::memory_order_relaxed);
  g_timeline_repeat_logs.store(0, std::memory_order_relaxed);
  REXLOG_INFO("[ac6-first-mission-stage-arm] source={}", source);
}

// The dispatcher loads its selector from [this+8] and branches four ways. Names
// follow the generated body in generated/ac6recomp_recomp.38.cpp:23531, not the
// stale tree that misled cycles 408-411.
//
// state < -2  -> returns immediately, storing -2
// state == -1 or -2 -> sub_8234D478
// state == 0  -> sub_8234D3F0, the per-frame pad poll
// state > 0   -> returns having done nothing, storing -2
//
// In every path the callee's return value is written back to [this+8], so the
// field is the screen's state and the dispatcher advances it.
const char* BranchFor(int32_t state) {
  if (state < -2) return "below-range(no-op)";
  if (state < 0) return "sub_8234D478";
  if (state == 0) return "poll(sub_8234D3F0)";
  return "positive(NO-OP)";
}

}  // namespace

// PAL post-campaign-introduction resource selection. At this exact call site,
// level zero requests DATA entry 209, whose inner payload is the common
// `results` SWG/NTXR bundle. The immediately following constructor consumes a
// BRDB/BMAP package; entry 210 is the first package with that proven layout.
// Keep the correction local to the first campaign transition rather than
// mutating the persistent level value or the generic getter.
PPC_FUNC_IMPL(rex_sub_820943B0) {
  PPC_FUNC_PROLOGUE();
  constexpr uint32_t kResultsResourceCaller = 0x8218F3A0u;
  constexpr uint32_t kLevelRootPointer = 0x826E4EB4u;
  const uint32_t caller = ctx.lr;
  const uint32_t level_context = ctx.r3.u32;
  static std::atomic<uint32_t> resource_probe_logs{0};
  const uint32_t probe_sequence =
      (REXCVAR_GET(ac6_log_ui_dispatch) && caller == kResultsResourceCaller)
          ? resource_probe_logs.fetch_add(1, std::memory_order_relaxed) + 1
          : 0;
  const uint32_t level_root_before =
      rex::memory::load_and_swap<uint32_t>(base + kLevelRootPointer);
  const uint32_t mode_before = level_context
      ? rex::memory::load_and_swap<uint32_t>(base + level_context + 8)
      : UINT32_MAX;
  const uint32_t selector_before = level_context
      ? rex::memory::load_and_swap<uint32_t>(base + level_context + 0x243F8)
      : UINT32_MAX;
  const uint32_t level_address_before =
      level_context ? level_context + 19552u : 0;
  const uint32_t current_level_before = level_address_before
      ? rex::memory::load_and_swap<uint32_t>(base + level_address_before)
      : UINT32_MAX;
  __imp__rex_sub_820943B0(ctx, base);
  if (probe_sequence && probe_sequence <= 16u) {
    REXLOG_INFO(
        "[ac6-campaign-resource-call] sequence={} caller=0x{:08X} "
        "context=0x{:08X} root=0x{:08X} mode={} selector={} "
        "result=0x{:08X}",
        probe_sequence, caller, level_context, level_root_before,
        mode_before, selector_before, ctx.r3.u32);
  }
  const bool existing_first_mission =
      caller == kResultsResourceCaller && ctx.r3.u32 == 1u &&
      level_root_before && level_context == level_root_before + 112u &&
      mode_before == 1u && selector_before == UINT32_MAX &&
      current_level_before == 1u;
  if (existing_first_mission) {
    ArmFirstMissionStage("qualified-level1-resource");
    return;
  }
  if (caller != kResultsResourceCaller || ctx.r3.u32 != 0) {
    return;
  }

  const uint32_t level_root = level_root_before;
  if (!level_root || level_context != level_root + 112) {
    return;
  }
  const uint32_t level_mode = mode_before;
  const uint32_t level_selector = selector_before;
  if (!ac6::campaign_resource::ShouldSelectFirstCampaignMission(
          ctx.r3.u32, level_mode, level_selector)) {
    return;
  }

  ctx.r3.u64 = 1;
  ArmFirstMissionStage("level0-resource-bridge");
  REXLOG_INFO(
      "[ac6-first-campaign-level-bridge] level=0->1 mode={} selector={} "
      "mode_task_factory=0x{:08X} resource=209->210 "
      "source=brdb-bmap-constructor-contract",
      level_mode, level_selector,
      rex::memory::load_and_swap<uint32_t>(base + level_root + 16u));
}

// First mission package task. Log the exact indirect-call inputs before its
// first update so a null vtable slot can be distinguished from a bad payload
// pointer without tracing every guest call.
PPC_FUNC_IMPL(sub_820D8FE0) {
  PPC_FUNC_PROLOGUE();
  constexpr uint32_t kCampaignTaskCaller = 0x8218F5A4u;
  const uint32_t task = ctx.r3.u32;
  if (REXCVAR_GET(ac6_log_ui_dispatch) && ctx.lr == kCampaignTaskCaller &&
      task) {
    const uint32_t owner =
        rex::memory::load_and_swap<uint32_t>(base + task + 4);
    const uint32_t payload =
        rex::memory::load_and_swap<uint32_t>(base + task + 24);
    const uint32_t payload_vtable = payload
        ? rex::memory::load_and_swap<uint32_t>(base + payload)
        : 0;
    const uint32_t payload_update = payload_vtable
        ? rex::memory::load_and_swap<uint32_t>(base + payload_vtable + 116)
        : 0;
    const uint32_t model = owner
        ? rex::memory::load_and_swap<uint32_t>(base + owner + 224)
        : 0;
    const uint32_t event = owner
        ? rex::memory::load_and_swap<uint32_t>(base + owner + 236)
        : 0;
    const uint32_t model_vtable = model
        ? rex::memory::load_and_swap<uint32_t>(base + model)
        : 0;
    const uint32_t model_update = model_vtable
        ? rex::memory::load_and_swap<uint32_t>(base + model_vtable + 64)
        : 0;
    const uint32_t model_advance = model_vtable
        ? rex::memory::load_and_swap<uint32_t>(base + model_vtable + 72)
        : 0;
    const uint32_t optional =
        rex::memory::load_and_swap<uint32_t>(base + task + 28);
    const uint32_t optional_vtable = optional
        ? rex::memory::load_and_swap<uint32_t>(base + optional)
        : 0;
    const uint32_t optional_get = optional_vtable
        ? rex::memory::load_and_swap<uint32_t>(base + optional_vtable + 8)
        : 0;
    const uint8_t event_pending = event
        ? rex::memory::load_and_swap<uint8_t>(base + event + 9)
        : 0;
    const uint8_t model_latch = model
        ? rex::memory::load_and_swap<uint8_t>(base + model + 4148)
        : 0;
    const uint32_t model_state = model
        ? rex::memory::load_and_swap<uint32_t>(base + model + 4128)
        : 0;
    const uint32_t model_arg = model
        ? rex::memory::load_and_swap<uint32_t>(base + model + 4132)
        : 0;
    REXLOG_INFO(
        "[ac6-first-mission-task] task=0x{:08X} owner=0x{:08X} "
        "payload=0x{:08X} payload_vtable=0x{:08X} "
        "payload_update=0x{:08X} model=0x{:08X} "
        "model_vtable=0x{:08X} model_update=0x{:08X} "
        "model_advance=0x{:08X} model_state={} model_arg={} "
        "model_latch={} event=0x{:08X} event_pending={} "
        "optional=0x{:08X} optional_vtable=0x{:08X} optional_get=0x{:08X}",
        task, owner, payload, payload_vtable, payload_update, model,
        model_vtable, model_update, model_advance, model_state, model_arg,
        model_latch, event, event_pending, optional, optional_vtable,
        optional_get);
  }
  __imp__sub_820D8FE0(ctx, base);
}

// The first mission task reaches this timeline update once per frame. Its
// first call succeeds, while the second crosses the first integer-period
// boundary and enters sub_8237CC58/sub_8237B190. Record only the fields that
// select those branches and their direct virtual slots at the exact caller.
PPC_FUNC_IMPL(sub_8237B4D8) {
  PPC_FUNC_PROLOGUE();
  constexpr uint32_t kFirstMissionTaskCaller = 0x820D9180u;
  const uint32_t owner = ctx.r3.u32;
  if (REXCVAR_GET(ac6_log_ui_dispatch_verbose) &&
      ctx.lr == kFirstMissionTaskCaller && owner) {
    const uint32_t event =
        rex::memory::load_and_swap<uint32_t>(base + owner + 236);
    const uint32_t event_vtable = event
        ? rex::memory::load_and_swap<uint32_t>(base + event)
        : 0;
    const uint32_t clock =
        rex::memory::load_and_swap<uint32_t>(base + owner + 12);
    const uint32_t dispatch =
        rex::memory::load_and_swap<uint32_t>(base + owner + 244);
    const uint32_t dispatch_word0 = dispatch
        ? rex::memory::load_and_swap<uint32_t>(base + dispatch)
        : 0;
    REXLOG_INFO(
        "[ac6-first-mission-timeline] owner=0x{:08X} stop0={} stop1={} "
        "elapsed_bits=0x{:08X} event=0x{:08X} event_vtable=0x{:08X} "
        "event_begin=0x{:08X} event_end=0x{:08X} clock=0x{:08X} "
        "clock_scale={} dispatch=0x{:08X} dispatch_word0=0x{:08X} "
        "active={} period={} phase_bits=0x{:08X}",
        owner,
        rex::memory::load_and_swap<uint8_t>(base + owner),
        rex::memory::load_and_swap<uint8_t>(base + owner + 1),
        rex::memory::load_and_swap<uint32_t>(base + owner + 4), event,
        event_vtable,
        event_vtable
            ? rex::memory::load_and_swap<uint32_t>(base + event_vtable + 16)
            : 0,
        event_vtable
            ? rex::memory::load_and_swap<uint32_t>(base + event_vtable + 20)
            : 0,
        clock,
        clock ? rex::memory::load_and_swap<uint32_t>(base + clock + 76) : 0,
        dispatch, dispatch_word0,
        rex::memory::load_and_swap<uint32_t>(base + owner + 256),
        rex::memory::load_and_swap<uint32_t>(base + owner + 260),
        rex::memory::load_and_swap<uint32_t>(base + owner + 264));
  }
  __imp__sub_8237B4D8(ctx, base);
}

// Called only when the mission timeline crosses a whole-frame boundary. The
// routine dispatches through two type tables and an object at [state+16]; the
// values below identify every indirect target reachable before its first loop.
PPC_FUNC_IMPL(sub_8237CC58) {
  PPC_FUNC_PROLOGUE();
  constexpr uint32_t kTimelineCaller = 0x8237B5E8u;
  constexpr uint32_t kNestedTimelineCaller = 0x8237D0FCu;
  constexpr uint32_t kTypeMapA = 0x8267A1D0u;
  constexpr uint32_t kTypeMapB = 0x8267A208u;
  constexpr uint32_t kRecordTypeMap = 0x8267A27Cu;
  const uint32_t state = ctx.r3.u32;
  if (g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) &&
      (ctx.lr == kTimelineCaller || ctx.lr == kNestedTimelineCaller) && state) {
    const uint32_t candidate4 =
        rex::memory::load_and_swap<uint32_t>(base + state + 4);
    const uint32_t candidate8 =
        rex::memory::load_and_swap<uint32_t>(base + state + 8);
    const uint32_t candidate = candidate8 ? candidate8 : candidate4;
    const uint32_t type = candidate
        ? rex::memory::load_and_swap<uint32_t>(base + candidate)
        : 0;
    const uint32_t backend =
        rex::memory::load_and_swap<uint32_t>(base + state + 16);
    const uint32_t backend_vtable = backend
        ? rex::memory::load_and_swap<uint32_t>(base + backend)
        : 0;
    const uint32_t frame =
        rex::memory::load_and_swap<uint32_t>(base + state + 216);
    const uint32_t record_offsets =
        rex::memory::load_and_swap<uint32_t>(base + state + 40);
    const uint32_t record_root_holder =
        rex::memory::load_and_swap<uint32_t>(base + state + 32);
    const uint32_t record_root = record_root_holder
        ? rex::memory::load_and_swap<uint32_t>(base + record_root_holder)
        : 0;
    const uint32_t record_offset =
        frame < 0x10000 && record_offsets
            ? rex::memory::load_and_swap<uint32_t>(
                  base + record_offsets + frame * 8)
            : 0;
    const uint32_t record = record_root + record_offset;
    const uint32_t record_type = record
        ? rex::memory::load_and_swap<uint32_t>(base + record)
        : 0;
    const uint32_t pending =
        rex::memory::load_and_swap<uint32_t>(base + state + 228);
    // After the recursive child list, the generated body makes two more
    // indirect/virtual transitions.  Capture their exact guest pointers at
    // entry so a null target can be separated from a failure inside the
    // callback itself.  These fields are especially important for the first
    // type-4 child, which reaches the unresolved-branch trap after its count
    // reaches zero.
    const uint32_t callback =
        rex::memory::load_and_swap<uint32_t>(base + state + 408);
    const uint32_t callback_vtable = callback
        ? rex::memory::load_and_swap<uint32_t>(base + callback)
        : 0;
    const uint32_t callback_target = callback_vtable
        ? rex::memory::load_and_swap<uint32_t>(base + callback_vtable + 4)
        : 0;
    const uint32_t post =
        rex::memory::load_and_swap<uint32_t>(base + state + 412);
    const uint32_t post_source = post
        ? rex::memory::load_and_swap<uint32_t>(base + post + 244)
        : 0;
    const uint32_t post_base = post_source ? post_source + 64 : 0;
    REXLOG_INFO(
        "[ac6-first-mission-dispatch] caller=0x{:08X} state=0x{:08X} "
        "candidate4=0x{:08X} "
        "candidate8=0x{:08X} type={} map_a=0x{:08X} map_b=0x{:08X} "
        "backend=0x{:08X} backend_vtable=0x{:08X} slots="
        "[0x{:08X},0x{:08X},0x{:08X},0x{:08X},0x{:08X},0x{:08X}] "
        "initialized={} frame={} prior={} current={} pending=0x{:08X} "
        "pending_next=0x{:08X} record=0x{:08X} record_type={} "
        "record_repeat={} record_target=0x{:08X} "
        "callback=0x{:08X} callback_vtable=0x{:08X} "
        "callback_target=0x{:08X} post=0x{:08X} "
        "post_source=0x{:08X} post_base=0x{:08X}",
        ctx.lr, state, candidate4, candidate8, type,
        candidate && type < 256
            ? rex::memory::load_and_swap<uint32_t>(base + kTypeMapA + type * 4)
            : 0,
        candidate && type < 256
            ? rex::memory::load_and_swap<uint32_t>(base + kTypeMapB + type * 4)
            : 0,
        backend, backend_vtable,
        backend_vtable
            ? rex::memory::load_and_swap<uint32_t>(base + backend_vtable + 4)
            : 0,
        backend_vtable
            ? rex::memory::load_and_swap<uint32_t>(base + backend_vtable + 24)
            : 0,
        backend_vtable
            ? rex::memory::load_and_swap<uint32_t>(base + backend_vtable + 32)
            : 0,
        backend_vtable
            ? rex::memory::load_and_swap<uint32_t>(base + backend_vtable + 40)
            : 0,
        backend_vtable
            ? rex::memory::load_and_swap<uint32_t>(base + backend_vtable + 44)
            : 0,
        backend_vtable
            ? rex::memory::load_and_swap<uint32_t>(base + backend_vtable + 48)
            : 0,
        rex::memory::load_and_swap<uint8_t>(base + state + 212),
        frame,
        rex::memory::load_and_swap<uint32_t>(base + state + 220),
        rex::memory::load_and_swap<uint32_t>(base + state + 224),
        pending,
        pending ? rex::memory::load_and_swap<uint32_t>(base + pending + 24) : 0,
        record, record_type,
        record ? rex::memory::load_and_swap<uint32_t>(base + record + 4) : 0,
        record && record_type < 256
            ? rex::memory::load_and_swap<uint32_t>(
                  base + kRecordTypeMap + record_type * 4)
            : 0,
        callback, callback_vtable, callback_target, post, post_source,
        post_base);
  }
  const uint32_t prior_state = g_first_mission_timeline_state;
  if (g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      (ctx.lr == kTimelineCaller || ctx.lr == kNestedTimelineCaller)) {
    g_first_mission_timeline_state = state;
  }
  __imp__sub_8237CC58(ctx, base);
  g_first_mission_timeline_state = prior_state;
}

// Stage markers for the second-tick failure above. These exact return
// addresses are all inside sub_8237CC58; an entry without its matching leave
// names the callee containing the unresolved indirect branch.
#define AC6_TIMELINE_STAGE(function_name, caller_address)                       \
  PPC_FUNC_IMPL(function_name) {                                               \
    PPC_FUNC_PROLOGUE();                                                       \
    const bool traced = g_first_mission_stage_armed.load(                      \
                            std::memory_order_relaxed) &&                       \
                        REXCVAR_GET(ac6_log_ui_dispatch_verbose) &&           \
                        ctx.lr == (caller_address);                            \
    if (traced)                                                             \
      REXLOG_INFO("[ac6-first-mission-stage] state=0x{:08X} enter "          \
                  #function_name,                                            \
                  g_first_mission_timeline_state);                           \
    __imp__##function_name(ctx, base);                                         \
    if (traced)                                                             \
      REXLOG_INFO("[ac6-first-mission-stage] state=0x{:08X} leave "          \
                  #function_name,                                            \
                  g_first_mission_timeline_state);                           \
  }

AC6_TIMELINE_STAGE(sub_820E3720, 0x8237CE6Cu)
AC6_TIMELINE_STAGE(sub_820E3790, 0x8237CF20u)
AC6_TIMELINE_STAGE(sub_820E37C0, 0x8237CFBCu)
AC6_TIMELINE_STAGE(sub_820E37F8, 0x8237CFA4u)
AC6_TIMELINE_STAGE(sub_8237CB10, 0x8237CEF0u)

#undef AC6_TIMELINE_STAGE

PPC_FUNC_IMPL(sub_820E3808) {
  PPC_FUNC_PROLOGUE();
  const bool candidate =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) && ctx.lr == 0x8237CE80u;
  const uint32_t log_no = candidate
                              ? g_timeline_reset_logs.fetch_add(
                                    1, std::memory_order_relaxed)
                              : UINT32_MAX;
  const bool traced = candidate && log_no < 2u;
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-timeline-reset] phase=enter sequence={} "
        "state=0x{:08X} r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} "
        "r6=0x{:08X} object32=0x{:08X}",
        log_no + 1, g_first_mission_timeline_state, ctx.r3.u32, ctx.r4.u32,
        ctx.r5.u32, ctx.r6.u32, GuestWord(base, ctx.r3.u32 + 32));
  }
  __imp__sub_820E3808(ctx, base);
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-timeline-reset] phase=leave sequence={} "
        "state=0x{:08X} r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} "
        "r6=0x{:08X} object32=0x{:08X}",
        log_no + 1, g_first_mission_timeline_state, ctx.r3.u32, ctx.r4.u32,
        ctx.r5.u32, ctx.r6.u32, GuestWord(base, ctx.r3.u32 + 32));
  }
}

PPC_FUNC_IMPL(sub_823B7338) {
  PPC_FUNC_PROLOGUE();
  const bool traced =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) && ctx.lr == 0x8237CF58u;
  __imp__sub_823B7338(ctx, base);
  if (traced) {
    const uint32_t state = g_first_mission_timeline_state;
    REXLOG_INFO(
        "[ac6-first-mission-count] state=0x{:08X} count={} frame={} "
        "prior={} current={} record=0x{:08X}",
        state, ctx.r3.u32,
        rex::memory::load_and_swap<uint32_t>(base + state + 216),
        rex::memory::load_and_swap<uint32_t>(base + state + 220),
        rex::memory::load_and_swap<uint32_t>(base + state + 224),
        rex::memory::load_and_swap<uint32_t>(base + state + 248));
  }
}

// The generated timeline tail calls this helper after all child timelines.
// Its first operation is another virtual dispatch, so retain the exact
// object/vtable/slot before invoking the generated body.
PPC_FUNC_IMPL(sub_8237BBC8) {
  PPC_FUNC_PROLOGUE();
  const bool traced =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) && ctx.lr == 0x8237D190u;
  const uint32_t state = ctx.r3.u32;
  const uint32_t mode = ctx.r4.u32;
  const uint32_t backend = state
      ? rex::memory::load_and_swap<uint32_t>(base + state + 16)
      : 0;
  // PAL 0x8237BBC8 treats [state+16] as the dispatch object itself: it loads
  // that object's vtable at +0, then calls slot +0x20.  The earlier probe
  // added another +16 dereference here, classified the object payload as a
  // vtable, and consequently skipped every valid post callback as a heap
  // address.  Keep the observation aligned with the guest ABI so the body can
  // run when the slot is a real guest code pointer.
  const uint32_t object = backend;
  const uint32_t vtable = object
      ? rex::memory::load_and_swap<uint32_t>(base + object)
      : 0;
  const uint32_t target = vtable
      ? rex::memory::load_and_swap<uint32_t>(base + vtable + 32)
      : 0;
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-post] enter state=0x{:08X} mode={} "
        "backend=0x{:08X} object=0x{:08X} vtable=0x{:08X} target=0x{:08X}",
        state, mode, backend, object, vtable, target);
  }
  // The indirect slot is a guest code pointer.  The current PAL child graph
  // sometimes leaves it pointing into the B8 heap (or at zero); letting the
  // generated helper issue that branch turns a recoverable optional update
  // into the host's unresolved-branch trap.  Preserve valid dispatches and
  // make the invalid optional callback an explicit, observable no-op.
  constexpr uint32_t kGuestCodeBegin = 0x82000000u;
  constexpr uint32_t kGuestCodeEnd = 0x82400000u;
  if (target >= kGuestCodeBegin && target < kGuestCodeEnd) {
    __imp__sub_8237BBC8(ctx, base);
    if (traced) {
      REXLOG_INFO("[ac6-first-mission-post] leave state=0x{:08X} mode={}",
                  state, mode);
    }
  } else if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-post-skip] state=0x{:08X} mode={} "
        "invalid_target=0x{:08X}",
        state, mode, target);
  }
}

// These two helpers are reached by the callback-array loop override above.
// Keep their exact call-site boundaries visible if the next failure moves
// into an item initializer or publisher.
PPC_FUNC_IMPL(sub_8237BFD8) {
  PPC_FUNC_PROLOGUE();
  const bool traced =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) && ctx.lr == 0x820E6004u;
  const uint32_t item = ctx.r3.u32;
  const uint32_t item_vtable = item
      ? rex::memory::load_and_swap<uint32_t>(base + item + 16)
      : 0;
  const uint32_t target = item_vtable
      ? rex::memory::load_and_swap<uint32_t>(base + item_vtable + 24)
      : 0;
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-item] enter item=0x{:08X} arg={} "
        "vtable=0x{:08X} target=0x{:08X}",
        item, ctx.r4.u32, item_vtable, target);
  }
  __imp__sub_8237BFD8(ctx, base);
  if (traced) {
    REXLOG_INFO("[ac6-first-mission-item] leave item=0x{:08X}", item);
  }
}

PPC_FUNC_IMPL(sub_8237EFF8) {
  PPC_FUNC_PROLOGUE();
  const bool traced =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) && ctx.lr == 0x8237CFC8u;
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-record] enter record=0x{:08X} r4=0x{:08X} "
        "r5=0x{:08X}",
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
  }
  __imp__sub_8237EFF8(ctx, base);
  if (traced) {
    REXLOG_INFO("[ac6-first-mission-record] leave record=0x{:08X}",
                ctx.r3.u32);
  }
}

PPC_FUNC_IMPL(sub_8237EF50) {
  PPC_FUNC_PROLOGUE();
  const bool traced =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) && ctx.lr == 0x8237CFC8u;
  const uint32_t record = ctx.r3.u32;
  const uint32_t header = record
      ? rex::memory::load_and_swap<uint32_t>(base + record)
      : 0;
  const uint32_t header_state = header
      ? rex::memory::load_and_swap<uint32_t>(base + header + 12)
      : 0;
  const uint32_t header_flags = header_state
      ? rex::memory::load_and_swap<uint32_t>(base + header_state + 72)
      : 0;
  const uint32_t child = record
      ? rex::memory::load_and_swap<uint32_t>(base + record + 12)
      : 0;
  const uint32_t child_vtable = child
      ? rex::memory::load_and_swap<uint32_t>(base + child)
      : 0;
  const uint32_t child_target8 = child_vtable
      ? rex::memory::load_and_swap<uint32_t>(base + child_vtable + 8)
      : 0;
  const uint32_t child_target12 = child_vtable
      ? rex::memory::load_and_swap<uint32_t>(base + child_vtable + 12)
      : 0;
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-record] body record=0x{:08X} header=0x{:08X} "
        "header_state=0x{:08X} header_flags=0x{:08X} child=0x{:08X} "
        "child_vtable=0x{:08X} target8=0x{:08X} target12=0x{:08X}",
        record, header, header_state, header_flags, child, child_vtable,
        child_target8, child_target12);
  }
  __imp__sub_8237EF50(ctx, base);
  if (traced) {
    REXLOG_INFO("[ac6-first-mission-record] body-leave record=0x{:08X}",
                record);
  }
}

PPC_FUNC_IMPL(sub_820DA210) {
  PPC_FUNC_PROLOGUE();
  const bool traced =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) &&
      (ctx.lr == 0x8237EFC0u || ctx.lr == 0x8237EFE4u);
  const uint32_t object = ctx.r3.u32;
  const uint32_t vtable = object
      ? rex::memory::load_and_swap<uint32_t>(base + object)
      : 0;
  const uint32_t slot0 = vtable
      ? rex::memory::load_and_swap<uint32_t>(base + vtable)
      : 0;
  const uint32_t slot40 = vtable
      ? rex::memory::load_and_swap<uint32_t>(base + vtable + 40)
      : 0;
  const uint32_t slot80 = vtable
      ? rex::memory::load_and_swap<uint32_t>(base + vtable + 80)
      : 0;
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-record-target] enter target=0x820DA210 "
        "caller=0x{:08X} object=0x{:08X} vtable=0x{:08X} "
        "slot0=0x{:08X} slot40=0x{:08X} slot80=0x{:08X}",
        ctx.lr, object, vtable, slot0, slot40, slot80);
  }
  __imp__sub_820DA210(ctx, base);
  if (traced) {
    REXLOG_INFO("[ac6-first-mission-record-target] leave target=0x820DA210");
  }
}

PPC_FUNC_IMPL(sub_820DA2E8) {
  PPC_FUNC_PROLOGUE();
  const bool traced =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) &&
      (ctx.lr == 0x8237EFC0u || ctx.lr == 0x8237EFE4u);
  const uint32_t object = ctx.r3.u32;
  const uint32_t vtable = object
      ? rex::memory::load_and_swap<uint32_t>(base + object)
      : 0;
  const uint32_t slot0 = vtable
      ? rex::memory::load_and_swap<uint32_t>(base + vtable)
      : 0;
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-record-target] enter target=0x820DA2E8 "
        "caller=0x{:08X} object=0x{:08X} vtable=0x{:08X} "
        "slot0=0x{:08X}",
        ctx.lr, object, vtable, slot0);
  }
  __imp__sub_820DA2E8(ctx, base);
  if (traced) {
    REXLOG_INFO("[ac6-first-mission-record-target] leave target=0x820DA2E8");
  }
}

PPC_FUNC_IMPL(sub_8237EED0) {
  PPC_FUNC_PROLOGUE();
  const bool traced =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) && ctx.lr == 0x8237EFC8u;
  const uint32_t record = ctx.r3.u32;
  const uint32_t queue = record
      ? rex::memory::load_and_swap<uint32_t>(base + record + 20)
      : 0;
  const int32_t type = queue
      ? static_cast<int32_t>(rex::memory::load_and_swap<uint32_t>(base + queue))
      : 0;
  const uint32_t target = type > 0 && type < 256
      ? rex::memory::load_and_swap<uint32_t>(
            base + 0x8267A310u + static_cast<uint32_t>(type) * 4)
      : 0;
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-record-queue] enter record=0x{:08X} "
        "queue=0x{:08X} type={} target=0x{:08X}",
        record, queue, type, target);
  }
  __imp__sub_8237EED0(ctx, base);
  if (traced) {
    REXLOG_INFO("[ac6-first-mission-record-queue] leave record=0x{:08X}",
                record);
  }
}

PPC_FUNC_IMPL(sub_8237E418) {
  PPC_FUNC_PROLOGUE();
  const bool traced =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) && ctx.lr == 0x8237EF28u;
  const uint32_t record = ctx.r3.u32;
  const uint32_t queue = record
      ? rex::memory::load_and_swap<uint32_t>(base + record + 20)
      : 0;
  const uint32_t owner = record
      ? rex::memory::load_and_swap<uint32_t>(base + record + 12)
      : 0;
  const uint32_t vtable = owner
      ? rex::memory::load_and_swap<uint32_t>(base + owner)
      : 0;
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-record-op] enter record=0x{:08X} "
        "queue=0x{:08X} first={} next0=0x{:08X} owner=0x{:08X} "
        "vtable=0x{:08X} slots56=0x{:08X} slots60=0x{:08X} "
        "slots64=0x{:08X} slots68=0x{:08X} slots72=0x{:08X} "
        "slots76=0x{:08X} slots80=0x{:08X}",
        record, queue,
        queue ? rex::memory::load_and_swap<uint32_t>(base + queue) : 0,
        queue ? rex::memory::load_and_swap<uint32_t>(base + queue + 4) : 0,
        owner, vtable,
        vtable ? rex::memory::load_and_swap<uint32_t>(base + vtable + 56) : 0,
        vtable ? rex::memory::load_and_swap<uint32_t>(base + vtable + 60) : 0,
        vtable ? rex::memory::load_and_swap<uint32_t>(base + vtable + 64) : 0,
        vtable ? rex::memory::load_and_swap<uint32_t>(base + vtable + 68) : 0,
        vtable ? rex::memory::load_and_swap<uint32_t>(base + vtable + 72) : 0,
        vtable ? rex::memory::load_and_swap<uint32_t>(base + vtable + 76) : 0,
        vtable ? rex::memory::load_and_swap<uint32_t>(base + vtable + 80) : 0);
    if (queue) {
      REXLOG_INFO(
          "[ac6-first-mission-record-op-data] words="
          "[0x{:08X},0x{:08X},0x{:08X},0x{:08X},0x{:08X},0x{:08X},"
          "0x{:08X},0x{:08X},0x{:08X},0x{:08X},0x{:08X},0x{:08X},"
          "0x{:08X},0x{:08X}]",
          rex::memory::load_and_swap<uint32_t>(base + queue + 0),
          rex::memory::load_and_swap<uint32_t>(base + queue + 4),
          rex::memory::load_and_swap<uint32_t>(base + queue + 8),
          rex::memory::load_and_swap<uint32_t>(base + queue + 12),
          rex::memory::load_and_swap<uint32_t>(base + queue + 16),
          rex::memory::load_and_swap<uint32_t>(base + queue + 20),
          rex::memory::load_and_swap<uint32_t>(base + queue + 24),
          rex::memory::load_and_swap<uint32_t>(base + queue + 28),
          rex::memory::load_and_swap<uint32_t>(base + queue + 32),
          rex::memory::load_and_swap<uint32_t>(base + queue + 36),
          rex::memory::load_and_swap<uint32_t>(base + queue + 40),
          rex::memory::load_and_swap<uint32_t>(base + queue + 44),
          rex::memory::load_and_swap<uint32_t>(base + queue + 48),
          rex::memory::load_and_swap<uint32_t>(base + queue + 52));
    }
  }
  __imp__sub_8237E418(ctx, base);
  if (traced) {
    REXLOG_INFO("[ac6-first-mission-record-op] leave record=0x{:08X}",
                record);
  }
}

PPC_FUNC_IMPL(sub_820DB628) {
  PPC_FUNC_PROLOGUE();
  const bool traced =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) && ctx.lr == 0x8237E5ACu;
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-record-op-target] enter target=0x820DB628 "
        "object=0x{:08X} arg=0x{:08X}",
        ctx.r3.u32, ctx.r4.u32);
  }
  __imp__sub_820DB628(ctx, base);
  if (traced) {
    REXLOG_INFO("[ac6-first-mission-record-op-target] leave target=0x820DB628");
  }
}

#define AC6_RECORD_OP_TARGET(function_name)                                  \
  PPC_FUNC_IMPL(function_name) {                                             \
    PPC_FUNC_PROLOGUE();                                                      \
    const bool traced =                                                       \
        g_first_mission_stage_armed.load(std::memory_order_relaxed) &&        \
        REXCVAR_GET(ac6_log_ui_dispatch_verbose) &&                          \
        (ctx.lr == 0x8237E4D8u || ctx.lr == 0x8237E4F0u ||                    \
         ctx.lr == 0x8237E554u || ctx.lr == 0x8237E5ACu);                     \
    if (traced)                                                               \
      REXLOG_INFO("[ac6-first-mission-record-op-target] enter "              \
                  #function_name " object=0x{:08X} arg=0x{:08X}",           \
                  ctx.r3.u32, ctx.r4.u32);                                    \
    __imp__##function_name(ctx, base);                                        \
    if (traced)                                                               \
      REXLOG_INFO("[ac6-first-mission-record-op-target] leave "              \
                  #function_name);                                           \
  }

AC6_RECORD_OP_TARGET(sub_820DB1F0)
AC6_RECORD_OP_TARGET(sub_820DB2C8)
AC6_RECORD_OP_TARGET(sub_820DB368)
AC6_RECORD_OP_TARGET(sub_820DB408)
AC6_RECORD_OP_TARGET(sub_820DB4C8)
AC6_RECORD_OP_TARGET(sub_820DB578)

#undef AC6_RECORD_OP_TARGET

// Campaign-introduction transition owner. This state machine constructs the
// scene resource graph at state 0, updates it at state 1, then publishes the
// shared {1,3} mode signal at state 2. Record state changes, scene-substate
// changes and one sample every 300 calls without tracing the generated corpus.
PPC_FUNC_IMPL(sub_8218F4C8) {
  PPC_FUNC_PROLOGUE();
  const uint32_t self = ctx.r3.u32;
  const uint32_t before_state =
      self ? rex::memory::load_and_swap<uint32_t>(base + self + 12) : UINT32_MAX;
  const uint32_t before_timer =
      self ? rex::memory::load_and_swap<uint32_t>(base + self + 72) : 0;
  const uint32_t scene_object = self ? self + 624 : 0;
  const uint32_t before_scene_state = scene_object
      ? rex::memory::load_and_swap<uint32_t>(base + scene_object + 0x6080C)
      : UINT32_MAX;

  __imp__sub_8218F4C8(ctx, base);

  if (!REXCVAR_GET(ac6_log_ui_dispatch) || !self || before_state > 2) {
    return;
  }

  constexpr uint32_t kSceneControllerPointer = 0x823F9B28u;
  constexpr uint32_t kCampaignGlobalsPointer = 0x8293BA10u;
  constexpr uint32_t kCampaignMode = 0x82A21A90u;
  constexpr uint32_t kLevelRootPointer = 0x826E4EB4u;
  const uint32_t after_state =
      rex::memory::load_and_swap<uint32_t>(base + self + 12);
  const uint32_t after_timer =
      rex::memory::load_and_swap<uint32_t>(base + self + 72);
  const uint32_t after_scene_state =
      rex::memory::load_and_swap<uint32_t>(base + scene_object + 0x6080C);
  static thread_local uint32_t last_self = 0;
  static thread_local uint32_t last_owner_state = UINT32_MAX;
  static thread_local uint32_t last_scene_state = UINT32_MAX;
  static thread_local uint64_t calls = 0;
  ++calls;
  const bool should_log =
      self != last_self || before_state != after_state ||
      after_state != last_owner_state ||
      before_scene_state != after_scene_state ||
      after_scene_state != last_scene_state || calls % 300 == 0;
  last_self = self;
  last_owner_state = after_state;
  last_scene_state = after_scene_state;
  if (!should_log) {
    return;
  }
  const uint32_t scene =
      rex::memory::load_and_swap<uint32_t>(base + kSceneControllerPointer);
  const uint32_t scene_vtable =
      scene ? rex::memory::load_and_swap<uint32_t>(base + scene) : 0;
  const uint32_t campaign =
      rex::memory::load_and_swap<uint32_t>(base + kCampaignGlobalsPointer);
  const uint32_t campaign_resource =
      campaign ? rex::memory::load_and_swap<uint32_t>(base + campaign + 8) : 0;
  const uint32_t campaign_phase =
      campaign ? rex::memory::load_and_swap<uint32_t>(base + campaign + 24) : 0;
  const uint32_t campaign_step =
      campaign ? rex::memory::load_and_swap<uint32_t>(base + campaign + 28) : 0;
  const uint32_t mode =
      rex::memory::load_and_swap<uint32_t>(base + kCampaignMode);
  const uint32_t level_root =
      rex::memory::load_and_swap<uint32_t>(base + kLevelRootPointer);
  const uint32_t level_context = level_root ? level_root + 112 : 0;
  const uint32_t level_mode = level_context
      ? rex::memory::load_and_swap<uint32_t>(base + level_context + 8)
      : UINT32_MAX;
  const uint32_t level_selector = level_context
      ? rex::memory::load_and_swap<uint32_t>(base + level_context + 0x243F8)
      : UINT32_MAX;
  uint32_t level_address = 0;
  switch (level_mode) {
    case 2: level_address = level_context + 16; break;
    case 3: level_address = level_context + 24; break;
    case 4: level_address = level_context + 32; break;
    case 5: level_address = level_context + 36; break;
    default:
      if (level_context) {
        const uint32_t profile = level_selector < 3 ? level_selector : 0;
        level_address = level_context + profile * 42976u + 19552u;
      }
      break;
  }
  const uint32_t current_level = level_address
      ? rex::memory::load_and_swap<uint32_t>(base + level_address)
      : UINT32_MAX;
  const uint32_t self_vtable =
      rex::memory::load_and_swap<uint32_t>(base + self);
  const uint32_t task_owner =
      rex::memory::load_and_swap<uint32_t>(base + self + 32);
  const uint32_t task_payload =
      rex::memory::load_and_swap<uint32_t>(base + self + 52);
  const uint32_t scene_index =
      rex::memory::load_and_swap<uint32_t>(base + scene_object + 708);
  const uint32_t scene_count =
      rex::memory::load_and_swap<uint32_t>(base + scene_object + 704);
  const uint32_t scene_records =
      rex::memory::load_and_swap<uint32_t>(base + scene_object + 744);
  const uint32_t scene_record_key =
      rex::memory::load_and_swap<uint32_t>(base + scene_object + 732);
  const uint8_t scene_hold =
      rex::memory::load_and_swap<uint8_t>(base + scene_object + 0x607C0);
  const uint32_t scene_time_bits =
      rex::memory::load_and_swap<uint32_t>(base + scene_object + 0x60810);
  const uint32_t active_scene_key =
      rex::memory::load_and_swap<uint32_t>(base + scene_object + 0x6082C);
  REXLOG_INFO(
      "[ac6-campaign-transition] self=0x{:08X} state={}->{} timer={}->{} "
      "scene_state={}->{} scene_time=0x{:08X} scene_hold={} "
      "scene_index={} scene_count={} scene_records=0x{:08X} "
      "default_key=0x{:08X} active_key=0x{:08X} "
      "self_vtable=0x{:08X} task_owner=0x{:08X} task_payload=0x{:08X} "
      "controller=0x{:08X} controller_vtable=0x{:08X} campaign=0x{:08X} "
      "resource=0x{:08X} phase={} step={} mode={} level_root=0x{:08X} "
      "level_context=0x{:08X} level_mode={} level_selector={} "
      "level_address=0x{:08X} current_level={}",
      self, before_state, after_state, before_timer, after_timer,
      before_scene_state, after_scene_state, scene_time_bits, scene_hold,
      scene_index, scene_count, scene_records, scene_record_key,
      active_scene_key, self_vtable, task_owner, task_payload, scene,
      scene_vtable, campaign, campaign_resource, campaign_phase, campaign_step,
      mode, level_root, level_context, level_mode, level_selector,
      level_address, current_level);
}

// PAL XEX acc302... reaches this constructor from 0x8218F6DC with a decoded
// entry wrapper: an outer one-member FHM at r4, whose sole member at +0x1000 is
// the multi-member FHM the constructor expects. Passing the wrapper makes its
// request for member 1 return null. Unwrap only this exact registry object after
// validating the live outer and inner directories.
PPC_FUNC_IMPL(sub_8213AD60) {
  PPC_FUNC_PROLOGUE();
  constexpr uint32_t kCaller = 0x8218F6DCu;
  constexpr uint32_t kResourceVtable = 0x82067B14u;
  constexpr uint32_t kResourceKey = 0x5979A7AAu;

  const uint32_t object = ctx.r27.u32;
  const uint32_t wrapper_address = ctx.r4.u32;
  if (ctx.lr == kCaller && object && wrapper_address &&
      rex::memory::load_and_swap<uint32_t>(base + object) == kResourceVtable &&
      rex::memory::load_and_swap<uint32_t>(base + object + 4) == kResourceKey &&
      rex::memory::load_and_swap<uint32_t>(base + object + 24) ==
          wrapper_address) {
    const uint32_t registry_size =
        rex::memory::load_and_swap<uint32_t>(base + object + 28);
    const auto wrapper = ac6::campaign_resource::InspectCampaignWrapper(
        reinterpret_cast<const uint8_t*>(base + wrapper_address),
        std::size_t(registry_size) + 0x1000);
    // The registry size describes the padded outer allocation, not the inner
    // member's logical size. InspectCampaignWrapper already proves that the
    // complete inner FHM lies within that allocation.
    if (wrapper.valid &&
        wrapper_address <= UINT32_MAX - wrapper.inner_offset) {
      const uint32_t inner = wrapper_address + wrapper.inner_offset;
      REXLOG_INFO(
          "[ac6-campaign-resource-bridge] wrapper=0x{:08X} inner=0x{:08X} "
          "members={} size=0x{:X} source=live-single-member-wrapper",
          wrapper_address, inner, wrapper.inner_member_count,
          wrapper.inner_size);
      ctx.r4.u64 = inner;
    } else {
      const auto* header =
          reinterpret_cast<const uint8_t*>(base + wrapper_address);
      REXLOG_ERROR(
          "[ac6-campaign-resource-bridge] rejected wrapper=0x{:08X} "
          "registry_size=0x{:X} valid={} inner_size=0x{:X} members={} "
          "header={:02X}{:02X}{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}",
          wrapper_address, registry_size, wrapper.valid, wrapper.inner_size,
          wrapper.inner_member_count, header[0], header[1], header[2],
          header[3], header[4], header[5], header[6], header[7]);
    }
  }
  __imp__sub_8213AD60(ctx, base);
}

// Mission transition record walker. Its vtable dispatch is indirect, so the
// generated corpus has no caller edge. Keep this probe at the exact entry to
// qualify the next payload boundary reached after the campaign parent bridge.
PPC_FUNC_IMPL(sub_821C1130) {
  PPC_FUNC_PROLOGUE();
  const uint32_t payload = ctx.r4.u32;
  const uint32_t service = ctx.r3.u32;
  const uint32_t vtable =
      service ? rex::memory::load_and_swap<uint32_t>(base + service) : 0;
  if (ctx.lr == 0x821C1374u && service == 0x826A0708u &&
      vtable == 0x820674D8u && payload &&
      ac6::campaign_resource::HasEmptyNtxrDirectory(
          reinterpret_cast<const uint8_t*>(base + payload), 0x104)) {
    REXLOG_INFO(
        "[ac6-mission-record-walker] service=0x{:08X} payload=0x{:08X} "
        "source=empty-ntxr-directory result=0",
        service, payload);
    ctx.r3.u64 = 0;
    return;
  }
  if (payload) {
    const auto* header = reinterpret_cast<const uint8_t*>(base + payload);
    REXLOG_INFO(
        "[ac6-mission-record-walker] lr=0x{:08X} service=0x{:08X} "
        "vtable=0x{:08X} slots=0x{:08X},0x{:08X},0x{:08X},0x{:08X} "
        "payload=0x{:08X} type=0x{:08X} "
        "header={:02X}{:02X}{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}",
        uint32_t(ctx.lr), service, vtable,
        vtable ? rex::memory::load_and_swap<uint32_t>(base + vtable + 24) : 0,
        vtable ? rex::memory::load_and_swap<uint32_t>(base + vtable + 28) : 0,
        vtable ? rex::memory::load_and_swap<uint32_t>(base + vtable + 32) : 0,
        vtable ? rex::memory::load_and_swap<uint32_t>(base + vtable + 36) : 0,
        payload, ctx.r5.u32, header[0], header[1], header[2], header[3],
        header[4], header[5], header[6], header[7]);
  } else {
    REXLOG_ERROR(
        "[ac6-mission-record-walker] lr=0x{:08X} service=0x{:08X} "
        "payload=NULL type=0x{:08X}",
        uint32_t(ctx.lr), ctx.r3.u32, ctx.r5.u32);
  }
  __imp__sub_821C1130(ctx, base);
}

// sub_82387530 is the retail CRT formatter used behind the bounded string
// wrappers. ReXGlue split its internal parsing loop at 0x823876E4, so formats
// that contain a conversion after literal text eventually hit a generated
// unresolved-call fatal at 0x82388068. String streams are identified by the
// retail flag 66 at +12. Route only those through the runtime's existing PPC
// vararg formatter and preserve the retail stream cursor/count contract.
PPC_FUNC_IMPL(sub_82387530) {
  PPC_FUNC_PROLOGUE();
  const uint32_t stream = ctx.r3.u32;
  const uint32_t format = ctx.r4.u32;
  const uint32_t args = ctx.r6.u32;
  if (stream && format &&
      rex::memory::load_and_swap<uint32_t>(base + stream + 12) == 66) {
    const uint32_t cursor =
        rex::memory::load_and_swap<uint32_t>(base + stream + 0);
    const uint32_t remaining =
        rex::memory::load_and_swap<uint32_t>(base + stream + 4);
    if (cursor && remaining) {
      ctx.r3.u64 = cursor;
      ctx.r4.u64 = remaining;
      ctx.r5.u64 = format;
      ctx.r6.u64 = args;
      __imp___vsnprintf(ctx, base);
      const int32_t count = ctx.r3.s32;
      if (count >= 0) {
        const uint32_t advanced =
            uint32_t(count) < remaining ? uint32_t(count) : remaining;
        rex::memory::store_and_swap<uint32_t>(base + stream + 0,
                                               cursor + advanced);
        rex::memory::store_and_swap<uint32_t>(base + stream + 4,
                                               remaining - advanced);
      }
      return;
    }
  }
  __imp__sub_82387530(ctx, base);
}

// The loadout screens do not call their selection consumers directly.  They
// install them in virtual tables, so a normal caller search misses the exact
// handoff that consumes the A edge.  Keep these wrappers diagnostic-only until
// the object/field contract is qualified from a live first-mission run.
PPC_FUNC_IMPL(sub_8214C038) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t object = ctx.r3.u32;
  const uint32_t out = ctx.r5.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=8214C038 edge=0x{:04X} lr=0x{:08X} "
        "r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} r6=0x{:08X} "
        "obj0=0x{:08X} obj4=0x{:08X} obj8=0x{:08X} objC=0x{:08X} "
        "table=0x{:08X} out0=0x{:08X}",
        edge.mask, ctx.lr, object, ctx.r4.u32, out, ctx.r6.u32,
        GuestWord(base, object + 0), GuestWord(base, object + 4),
        GuestWord(base, object + 8), GuestWord(base, object + 12),
        GuestWord(base, object + 0x8608), GuestWord(base, out));
  }
  __imp__sub_8214C038(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=8214C038 leave r3=0x{:08X} "
        "out0=0x{:08X} table=0x{:08X} table+4=0x{:08X} table+8=0x{:08X}",
        ctx.r3.u32, GuestWord(base, out), GuestWord(base, object + 0x8608),
        GuestWord(base, object + 0x860C), GuestWord(base, object + 0x8610));
  }
}

PPC_FUNC_IMPL(sub_8214C360) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t object = ctx.r3.u32;
  const uint32_t out = ctx.r5.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=8214C360 edge=0x{:04X} lr=0x{:08X} "
        "r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} r6=0x{:08X} "
        "obj0=0x{:08X} obj4=0x{:08X} obj8=0x{:08X} objC=0x{:08X} "
        "table=0x{:08X} out0=0x{:08X}",
        edge.mask, ctx.lr, object, ctx.r4.u32, out, ctx.r6.u32,
        GuestWord(base, object + 0), GuestWord(base, object + 4),
        GuestWord(base, object + 8), GuestWord(base, object + 12),
        GuestWord(base, object + 0x8608), GuestWord(base, out));
  }
  __imp__sub_8214C360(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=8214C360 leave r3=0x{:08X} "
        "out0=0x{:08X} table=0x{:08X} table+4=0x{:08X} table+8=0x{:08X}",
        ctx.r3.u32, GuestWord(base, out), GuestWord(base, object + 0x8608),
        GuestWord(base, object + 0x860C), GuestWord(base, object + 0x8610));
  }
}

PPC_FUNC_IMPL(sub_8214C518) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t object = ctx.r3.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=8214C518 edge=0x{:04X} lr=0x{:08X} "
        "r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} r6=0x{:08X} "
        "obj0=0x{:08X} b35989={} b35990={} b35994={} b35995={} "
        "word35984=0x{:08X} word36208=0x{:08X}",
        edge.mask, ctx.lr, object, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32,
        GuestWord(base, object),
        rex::memory::load_and_swap<uint8_t>(base + object + 35989),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        rex::memory::load_and_swap<uint8_t>(base + object + 35994),
        rex::memory::load_and_swap<uint8_t>(base + object + 35995),
        GuestWord(base, object + 35984), GuestWord(base, object + 36208));
  }
  __imp__sub_8214C518(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=8214C518 leave edge=0x{:04X} "
        "r3=0x{:08X} r4=0x{:08X} b35989={} b35990={} word35984=0x{:08X}",
        edge.mask, ctx.r3.u32, ctx.r4.u32,
        rex::memory::load_and_swap<uint8_t>(base + object + 35989),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        GuestWord(base, object + 35984));
  }
}

PPC_FUNC_IMPL(sub_8214D000) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t object = ctx.r3.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=8214D000 edge=0x{:04X} lr=0x{:08X} "
        "r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} r6=0x{:08X} "
        "obj0=0x{:08X} b35990={} word34312=0x{:08X} word54828=0x{:08X}",
        edge.mask, ctx.lr, object, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32,
        GuestWord(base, object),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        GuestWord(base, object + 34312), GuestWord(base, object + 54828));
  }
  __imp__sub_8214D000(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=8214D000 leave edge=0x{:04X} "
        "r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} r6=0x{:08X}",
        edge.mask, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
  }
}

PPC_FUNC_IMPL(sub_82146DB8) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  static std::atomic<uint64_t> last_force_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t object = ctx.r3.u32;
  const uint32_t caller_lr = ctx.lr;
  const bool new_force_edge =
      edge.mask == 0x1000u && edge.serial && caller_lr == 0x8218C64Cu &&
      edge.serial != last_force_serial.exchange(edge.serial);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=82146DB8 edge=0x{:04X} lr=0x{:08X} "
        "r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} r6=0x{:08X} "
        "obj0=0x{:08X} obj8=0x{:08X} objC=0x{:08X} "
        "b34308={} b34312={} b35989={} b35990={} b35992={} "
        "word35984=0x{:08X}",
        edge.mask, ctx.lr, object, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32,
        GuestWord(base, object), GuestWord(base, object + 8),
        GuestWord(base, object + 12),
        rex::memory::load_and_swap<uint8_t>(base + object + 34308),
        rex::memory::load_and_swap<uint8_t>(base + object + 34312),
        rex::memory::load_and_swap<uint8_t>(base + object + 35989),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        rex::memory::load_and_swap<uint8_t>(base + object + 35992),
        GuestWord(base, object + 35984));
  }
  __imp__sub_82146DB8(ctx, base);
  if (REXCVAR_GET(ac6_force_loadout_ready) && new_force_edge &&
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      caller_lr == 0x8218C64Cu && object &&
      GuestWord(base, object) == 0x8205DAECu) {
    const uint32_t count =
        g_first_mission_loadout_a_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (count == 3) {
      // The state-1 handler calls +0x70 immediately after this method and
      // expects status 5. The generated PAL path never populates that byte
      // (it remains zero after the aircraft and weapon confirmations), so
      // expose the already-qualified launch state at the third A edge only.
      rex::memory::store_and_swap<uint8_t>(base + object + 35995, 5);
      REXLOG_WARN(
          "[ac6-loadout-ready-override] manager=0x{:08X} edge=0x{:04X} "
          "serial={} sequence={} status=0->5",
          object, edge.mask, edge.serial, count);
    }
  }
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=82146DB8 leave edge=0x{:04X} "
        "r3=0x{:08X} r4=0x{:08X} obj8=0x{:08X} objC=0x{:08X} "
        "b34308={} b34312={} b35989={} b35990={} b35992={} "
        "word35984=0x{:08X}",
        edge.mask, ctx.r3.u32, ctx.r4.u32, GuestWord(base, object + 8),
        GuestWord(base, object + 12),
        rex::memory::load_and_swap<uint8_t>(base + object + 34308),
        rex::memory::load_and_swap<uint8_t>(base + object + 34312),
        rex::memory::load_and_swap<uint8_t>(base + object + 35989),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        rex::memory::load_and_swap<uint8_t>(base + object + 35992),
        GuestWord(base, object + 35984));
  }
}

PPC_FUNC_IMPL(sub_8218C238) {
  PPC_FUNC_PROLOGUE();
  const uint32_t state_object = ctx.r3.u32;
  const bool log = REXCVAR_GET(ac6_log_loadout_dispatch) &&
                   g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
                   state_object;
  const uint32_t before =
      state_object ? GuestWord(base, state_object + 12) : 0;
  if (log) {
    REXLOG_INFO("[ac6-first-mission-state] enter object=0x{:08X} state={} "
                "ticks=0x{:08X}",
                state_object, static_cast<int32_t>(before),
                GuestWord(base, state_object + 72));
  }
  const bool log_state2_gate =
      log && before == 2u && GuestWord(base, state_object + 72) <= 1u;
  auto log_state2_context = [&](const char* phase) {
    if (!log_state2_gate) {
      return;
    }
    // C810 materializes lis r29,-32192 (0x82400000), then loads -25816.
    constexpr uint32_t kGlobalHolder = 0x823F9B28u;
    const uint32_t global = GuestWord(base, kGlobalHolder);
    constexpr uint32_t kGuestLo = 0x82000000u;
    constexpr uint32_t kGuestHi = 0xC0000000u;
    if (global < kGuestLo || global >= kGuestHi) {
      REXLOG_INFO(
          "[ac6-loadout-gate] state2 {} global_holder=0x{:08X} "
          "global=0x{:08X} invalid-global state=0x{:08X} ticks=0x{:08X}",
          phase, kGlobalHolder, global, GuestWord(base, state_object + 12),
          GuestWord(base, state_object + 72));
      return;
    }
    const uint32_t vtable = GuestWord(base, global);
    if (vtable < kGuestLo || vtable >= kGuestHi) {
      REXLOG_INFO(
          "[ac6-loadout-gate] state2 {} global_holder=0x{:08X} "
          "global=0x{:08X} vtable=0x{:08X} invalid-vtable "
          "state=0x{:08X} ticks=0x{:08X}",
          phase, kGlobalHolder, global, vtable,
          GuestWord(base, state_object + 12),
          GuestWord(base, state_object + 72));
      return;
    }
    REXLOG_INFO(
        "[ac6-loadout-gate] state2 {} global_holder=0x{:08X} "
        "global=0x{:08X} vtable=0x{:08X} slot252=0x{:08X} "
        "slot360=0x{:08X} slot456=0x{:08X} state=0x{:08X} ticks=0x{:08X}",
        phase, kGlobalHolder, global, vtable, GuestWord(base, vtable + 252),
        GuestWord(base, vtable + 360), GuestWord(base, vtable + 456),
        GuestWord(base, state_object + 12), GuestWord(base, state_object + 72));
  };
  log_state2_context("enter");
  __imp__sub_8218C238(ctx, base);
  log_state2_context("leave");
  if (log) {
    REXLOG_INFO("[ac6-first-mission-state] leave object=0x{:08X} state={} "
                "ticks=0x{:08X} return=0x{:08X}",
                state_object,
                static_cast<int32_t>(GuestWord(base, state_object + 12)),
                GuestWord(base, state_object + 72), ctx.r3.u32);
  }
}

// The generated allocator helper writes through the result of
// sub_820EA068 without checking for an exhausted arena.  Keep the generated
// behavior for normal calls, but provide a bounded guest-memory extension for
// the qualified first-mission launch so the world task can construct its
// containers instead of trapping at guest address zero.
PPC_FUNC_IMPL(sub_820D94B8) {
  PPC_FUNC_PROLOGUE();
  const uint64_t saved_r31 = ctx.r31.u64;
  ctx.r31.u64 = ctx.r4.u32 + 4u;
  const uint32_t request = ctx.r31.u32;
  ctx.r3.u64 = ctx.r31.u64;
  __imp__sub_820EA068(ctx, base);

  if (!ctx.r3.u32 &&
      g_first_mission_launch_started.load(std::memory_order_relaxed) &&
      request && request <= 0x01000000u) {
    ctx.r3.u64 = FirstMissionFallbackAllocate(base, request);
  }

  if (!ctx.r3.u32) {
    // The original helper would fault on the next store.  Returning a null
    // result preserves a recoverable failure for callers outside this narrow
    // mission bridge.
    ctx.r31.u64 = saved_r31;
    return;
  }
  PPC_STORE_U32(ctx.r3.u32, ctx.r31.u32);
  ctx.r3.s64 = ctx.r3.s64 + 4;
  ctx.r31.u64 = saved_r31;
}

// A few first-mission constructors pass an exhausted allocation straight to
// the guest memset helper.  Give that narrow path the same bounded backing
// range; normal calls, including intentional zero-length clears, stay native.
PPC_FUNC_IMPL(sub_823835D0) {
  PPC_FUNC_PROLOGUE();
  const bool candidate =
      g_first_mission_stage_armed.load(std::memory_order_relaxed) &&
      REXCVAR_GET(ac6_log_ui_dispatch_verbose) && ctx.lr == 0x8237CE6Cu;
  const uint32_t log_no = candidate
                              ? g_timeline_prepare_logs.fetch_add(
                                    1, std::memory_order_relaxed)
                              : UINT32_MAX;
  const bool traced = candidate && log_no < 2u;
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-timeline-prepare] phase=enter sequence={} "
        "state=0x{:08X} r3=0x{:08X} fill=0x{:02X} bytes={} "
        "r6=0x{:08X}",
        log_no + 1, g_first_mission_timeline_state, ctx.r3.u32,
        ctx.r4.u32 & 0xFFu, ctx.r5.u32, ctx.r6.u32);
  }
  if (!ctx.r3.u32 && ctx.r5.u32 &&
      g_first_mission_launch_started.load(std::memory_order_relaxed)) {
    ctx.r3.u64 = FirstMissionFallbackAllocate(base, ctx.r5.u32);
    if (!ctx.r3.u32) {
      return;
    }
  }
  __imp__sub_823835D0(ctx, base);
  if (traced) {
    REXLOG_INFO(
        "[ac6-first-mission-timeline-prepare] phase=leave sequence={} "
        "state=0x{:08X} r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} "
        "r6=0x{:08X}",
        log_no + 1, g_first_mission_timeline_state, ctx.r3.u32, ctx.r4.u32,
        ctx.r5.u32, ctx.r6.u32);
  }
}

// Child timeline dispatch reaches this helper through a type/index table.
// The generated body dereferences the computed table slot unconditionally;
// expose and quarantine a missing slot so the exact child contract is visible
// instead of turning into a host access at guest address +4.
PPC_FUNC_IMPL(sub_8237C4D8) {
  PPC_FUNC_PROLOGUE();
  constexpr uint32_t kGuestLo = 0x82000000u;
  constexpr uint32_t kGuestHi = 0xC0000000u;
  const uint32_t object = ctx.r3.u32;
  const uint32_t child = ctx.r5.u32;
  const uint32_t object_vtable =
      (object >= kGuestLo && object < kGuestHi) ? GuestWord(base, object + 32) : 0;
  const uint32_t child_index =
      (child >= kGuestLo && child < kGuestHi) ? GuestWord(base, child + 8) : 0;
  const uint32_t stride =
      (object_vtable >= kGuestLo && object_vtable < kGuestHi)
          ? GuestWord(base, object_vtable + 56)
          : 0;
  const uint32_t slot = stride + (child_index << 3);
  const uint32_t slot_function =
      (slot >= kGuestLo && slot < kGuestHi) ? GuestWord(base, slot + 4) : 0;
  const bool launch =
      g_first_mission_launch_started.load(std::memory_order_relaxed);
  const bool repeat_candidate =
      launch && REXCVAR_GET(ac6_log_ui_dispatch_verbose) &&
      ctx.lr == 0x8237CEF0u;
  const uint32_t repeat_no = repeat_candidate
                                 ? g_timeline_repeat_logs.fetch_add(
                                       1, std::memory_order_relaxed)
                                 : UINT32_MAX;
  if (repeat_candidate && repeat_no < 2u) {
    const uint32_t state = g_first_mission_timeline_state;
    REXLOG_INFO(
        "[ac6-first-mission-timeline-repeat] sequence={} "
        "state=0x{:08X} remaining={} r3=0x{:08X} r4=0x{:08X} "
        "r5=0x{:08X} r6=0x{:08X} frame={} prior={} current={} "
        "record=0x{:08X}",
        repeat_no + 1, state, ctx.r30.u32, ctx.r3.u32, ctx.r4.u32,
        ctx.r5.u32, ctx.r6.u32, GuestWord(base, state + 216),
        GuestWord(base, state + 220), GuestWord(base, state + 224),
        GuestWord(base, state + 248));
  }
  if (launch && (!object_vtable || !stride || !slot_function)) {
    // This call is made from a per-frame child loop.  Keep the evidence
    // bounded, but dump the complete small-object headers on the first
    // occurrence so a missing table can be repaired from its native layout
    // rather than hidden behind a null return.
    const uint32_t log_no =
        g_child_slot_missing_logs.fetch_add(1, std::memory_order_relaxed);
    if (log_no < 2u) {
      REXLOG_ERROR(
          "[ac6-first-mission-child-slot-missing] fn=8237C4D8 "
          "lr=0x{:08X} object=0x{:08X} child=0x{:08X} "
          "object_vtable=0x{:08X} index={} stride=0x{:08X} "
          "slot=0x{:08X} target=0x{:08X} "
          "obj=[{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},"
          "{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X}] "
          "vt=[{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},"
          "{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X}] "
          "child=[{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X}]",
          ctx.lr, object, child, object_vtable, child_index, stride, slot,
          slot_function,
          GuestWord(base, object + 0), GuestWord(base, object + 4),
          GuestWord(base, object + 8), GuestWord(base, object + 12),
          GuestWord(base, object + 16), GuestWord(base, object + 20),
          GuestWord(base, object + 24), GuestWord(base, object + 28),
          GuestWord(base, object + 32), GuestWord(base, object + 36),
          GuestWord(base, object + 40), GuestWord(base, object + 44),
          GuestWord(base, object + 48), GuestWord(base, object + 52),
          GuestWord(base, object + 56), GuestWord(base, object + 60),
          GuestWord(base, object_vtable + 0), GuestWord(base, object_vtable + 4),
          GuestWord(base, object_vtable + 8), GuestWord(base, object_vtable + 12),
          GuestWord(base, object_vtable + 16), GuestWord(base, object_vtable + 20),
          GuestWord(base, object_vtable + 24), GuestWord(base, object_vtable + 28),
          GuestWord(base, object_vtable + 32), GuestWord(base, object_vtable + 36),
          GuestWord(base, object_vtable + 40), GuestWord(base, object_vtable + 44),
          GuestWord(base, object_vtable + 48), GuestWord(base, object_vtable + 52),
          GuestWord(base, object_vtable + 56), GuestWord(base, object_vtable + 60),
          GuestWord(base, child + 0), GuestWord(base, child + 4),
          GuestWord(base, child + 8), GuestWord(base, child + 12),
          GuestWord(base, child + 16), GuestWord(base, child + 20),
          GuestWord(base, child + 24), GuestWord(base, child + 28));
    }

    // The first PAL child carries a valid type/index but the reconstructed
    // owner omitted its sparse slot table.  Materialise only that exact
    // 0x10004 entry in the bounded guest scratch range and route it through
    // the retail no-op callback.  This preserves the generated constructor's
    // normal bookkeeping while avoiding a speculative global table rewrite.
    if (object_vtable && !stride && child_index == 0x00010004u) {
      constexpr uint32_t kChildSlotTableBytes = 0x00080040u;
      // C4D8 adds [slot+4] to the descriptor word at [vtable+0]; it is an
      // object-relative metadata offset, not an indirect code address.
      constexpr uint32_t kRetailSlotOffset = 0u;
      const uint32_t table =
          FirstMissionFallbackAllocate(base, kChildSlotTableBytes);
      if (table) {
        const uint32_t entry = table + (child_index << 3);
        rex::memory::store_and_swap<uint32_t>(base + table, 0);
        rex::memory::store_and_swap<uint32_t>(base + entry, 0);
        rex::memory::store_and_swap<uint32_t>(base + entry + 4,
                                              kRetailSlotOffset);
        rex::memory::store_and_swap<uint32_t>(base + object_vtable + 56,
                                              table);
        REXLOG_WARN(
            "[ac6-first-mission-child-slot-fallback] object=0x{:08X} "
            "child=0x{:08X} index={} table=0x{:08X} entry=0x{:08X} "
            "slot_offset=0x{:08X}",
            object, child, child_index, table, entry, kRetailSlotOffset);
        __imp__sub_8237C4D8(ctx, base);
        // The scratch table is valid only for this exact child invocation.
        // Restore the absent native field before the next sibling is walked;
        // otherwise its unrelated index would read arbitrary scratch words
        // and turn the next bctr into a misleading jump-table failure.
        rex::memory::store_and_swap<uint32_t>(base + object_vtable + 56, 0);
        return;
      }
    }
    ctx.r3.u64 = 0;
    return;
  }
  __imp__sub_8237C4D8(ctx, base);
}

// The child-slot repair exposes the next indirect boundary in the generated
// timeline dispatcher.  The retail table at 0x8267A1D0 is six entries wide;
// log and quarantine any value outside that qualified set instead of letting
// the generated bctr abort without its inputs.
PPC_FUNC_IMPL(sub_8237C828) {
  PPC_FUNC_PROLOGUE();
  constexpr uint32_t kGuestLo = 0x82000000u;
  constexpr uint32_t kGuestHi = 0xC0000000u;
  constexpr uint32_t kTypeMap = 0x8267A1D0u;
  const uint32_t object = ctx.r3.u32;
  const uint32_t input = ctx.r4.u32;
  const auto safe_word = [&](uint32_t address) {
    return address >= kGuestLo && address < kGuestHi ? GuestWord(base, address)
                                                      : 0u;
  };
  const uint32_t object_table = safe_word(object + 32);
  const uint32_t table_base = safe_word(object_table);
  const uint32_t input_offset = safe_word(input + 8);
  const uint32_t record = table_base + input_offset;
  const uint32_t record_type = safe_word(record);
  const uint32_t target = record_type < 0x10000u
                              ? safe_word(kTypeMap + record_type * 4)
                              : 0u;
  const bool launch =
      g_first_mission_launch_started.load(std::memory_order_relaxed);
  if (launch && (target == 0u ||
                 (target != 0x8237C828u && target != 0x8237C640u &&
                  target != 0x8237C658u && target != 0x8237C670u))) {
    const uint32_t log_no =
        g_child_dispatch_logs.fetch_add(1, std::memory_order_relaxed);
    if (log_no < 4u) {
      REXLOG_ERROR(
          "[ac6-first-mission-child-dispatch-missing] lr=0x{:08X} "
          "object=0x{:08X} input=0x{:08X} object_table=0x{:08X} "
          "table_base=0x{:08X} input_offset=0x{:08X} record=0x{:08X} "
          "record_type=0x{:08X} target=0x{:08X} "
          "code=[{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X},{:08X}]",
          ctx.lr, object, input, object_table, table_base, input_offset,
          record, record_type, target, safe_word(target + 0),
          safe_word(target + 4), safe_word(target + 8), safe_word(target + 12),
          safe_word(target + 16), safe_word(target + 20), safe_word(target + 24),
          safe_word(target + 28));
    }
    if (REXCVAR_GET(ac6_pause_first_mission_dispatch) && log_no == 0u) {
      REXLOG_WARN(
          "[ac6-first-mission-child-dispatch-pause] sleeping=20s target=0x{:08X}",
          target);
      std::this_thread::sleep_for(std::chrono::seconds(20));
    }
    // The caller immediately feeds this result to sub_8237B898, which reads
    // the 0..52-byte numeric record. A null return would only move the fault
    // to that consumer; retain the bounded input record while the missing
    // type-map entry is qualified.
    ctx.r3.u64 = input;
    return;
  }
  __imp__sub_8237C828(ctx, base);
}

// The companion dispatcher at 0x8237BF30 selects the second timeline type
// table.  Keep its input visible and preserve a non-null numeric record when
// the reconstructed owner has no qualified table entry.
PPC_FUNC_IMPL(sub_8237BF08) {
  PPC_FUNC_PROLOGUE();
  constexpr uint32_t kGuestLo = 0x82000000u;
  constexpr uint32_t kGuestHi = 0xC0000000u;
  constexpr uint32_t kTypeMap = 0x8267A208u;
  const uint32_t object = ctx.r3.u32;
  const uint32_t input = ctx.r4.u32;
  const auto safe_word = [&](uint32_t address) {
    return address >= kGuestLo && address < kGuestHi ? GuestWord(base, address)
                                                      : 0u;
  };
  const uint32_t object_table = safe_word(object + 32);
  const uint32_t table_base = safe_word(object_table);
  const uint32_t input_offset = safe_word(input + 12);
  const uint32_t record = table_base + input_offset;
  const uint32_t record_type = safe_word(record);
  const uint32_t target = record_type < 0x10000u
                              ? safe_word(kTypeMap + record_type * 4)
                              : 0u;
  const bool launch =
      g_first_mission_launch_started.load(std::memory_order_relaxed);
  if (launch &&
      (target == 0u ||
       (target != 0x8237BF08u && target != 0x8237C688u &&
        target != 0x8237C878u && target != 0x8237BF38u &&
        target != 0x8237C6A0u))) {
    const uint32_t log_no = g_child_secondary_dispatch_logs.fetch_add(
        1, std::memory_order_relaxed);
    if (log_no < 4u) {
      REXLOG_ERROR(
          "[ac6-first-mission-child-secondary-missing] lr=0x{:08X} "
          "object=0x{:08X} input=0x{:08X} object_table=0x{:08X} "
          "table_base=0x{:08X} input_offset=0x{:08X} record=0x{:08X} "
          "record_type=0x{:08X} target=0x{:08X}",
          ctx.lr, object, input, object_table, table_base, input_offset,
          record, record_type, target);
    }
    ctx.r3.u64 = input;
    return;
  }
  __imp__sub_8237BF08(ctx, base);
}

// The state-1 loadout handler tests these two tiny virtual methods immediately
// after the confirmation update (+0x8C).  Their generated bodies are simple
// field accessors, so logging their live values makes the launch predicate
// explicit without changing the guest state machine.
PPC_FUNC_IMPL(sub_82144FC8) {
  PPC_FUNC_PROLOGUE();
  const LoadoutEdge edge = LastLoadoutEdge();
  const uint32_t object = ctx.r3.u32;
  const bool log = LoadoutProbeEnabled() && edge.mask && ctx.lr == 0x8218C660u;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-predicate] fn=82144FC8 enter edge=0x{:04X} "
        "lr=0x{:08X} object=0x{:08X} ready={} state=0x{:08X} "
        "b35989={} b35990={} b35992={} b35995={}",
        edge.mask, ctx.lr, object,
        rex::memory::load_and_swap<uint8_t>(base + object + 35994),
        GuestWord(base, object + 35984),
        rex::memory::load_and_swap<uint8_t>(base + object + 35989),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        rex::memory::load_and_swap<uint8_t>(base + object + 35992),
        rex::memory::load_and_swap<uint8_t>(base + object + 35995));
  }
  __imp__sub_82144FC8(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-predicate] fn=82144FC8 leave edge=0x{:04X} "
        "return={} ready={} state=0x{:08X}",
        edge.mask, ctx.r3.u32 & 0xFFu,
        rex::memory::load_and_swap<uint8_t>(base + object + 35994),
        GuestWord(base, object + 35984));
  }
}

PPC_FUNC_IMPL(sub_82144F98) {
  PPC_FUNC_PROLOGUE();
  const LoadoutEdge edge = LastLoadoutEdge();
  const uint32_t object = ctx.r3.u32;
  const bool log = LoadoutProbeEnabled() && edge.mask && ctx.lr == 0x8218C728u;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-predicate] fn=82144F98 enter edge=0x{:04X} "
        "lr=0x{:08X} object=0x{:08X} ready={} state=0x{:08X} "
        "b35989={} b35990={} b35992={} b35995={}",
        edge.mask, ctx.lr, object,
        rex::memory::load_and_swap<uint8_t>(base + object + 35994),
        GuestWord(base, object + 35984),
        rex::memory::load_and_swap<uint8_t>(base + object + 35989),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        rex::memory::load_and_swap<uint8_t>(base + object + 35992),
        rex::memory::load_and_swap<uint8_t>(base + object + 35995));
  }
  __imp__sub_82144F98(ctx, base);
  if (FirstMissionLaunchOverrideEnabled() && ctx.lr == 0x8218C728u) {
    ctx.r3.u64 = 5;
    g_first_mission_launch_started.store(true, std::memory_order_relaxed);
    REXLOG_WARN(
        "[ac6-loadout-launch-override] fn=82144F98 status=0->5");
  }
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-predicate] fn=82144F98 leave edge=0x{:04X} "
        "return=0x{:08X} ready={} state=0x{:08X}",
        edge.mask, ctx.r3.u32,
        rex::memory::load_and_swap<uint8_t>(base + object + 35994),
        GuestWord(base, object + 35984));
  }
}

PPC_FUNC_IMPL(sub_82144FD8) {
  PPC_FUNC_PROLOGUE();
  const LoadoutEdge edge = LastLoadoutEdge();
  const uint32_t object = ctx.r3.u32;
  const bool log = LoadoutProbeEnabled() && edge.mask && ctx.lr == 0x8218C6A4u;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-predicate] fn=82144FD8 enter edge=0x{:04X} "
        "lr=0x{:08X} object=0x{:08X} status={} ready={} state=0x{:08X} "
        "b35989={} b35990={} b35992={}",
        edge.mask, ctx.lr, object,
        rex::memory::load_and_swap<uint8_t>(base + object + 35995),
        rex::memory::load_and_swap<uint8_t>(base + object + 35994),
        GuestWord(base, object + 35984),
        rex::memory::load_and_swap<uint8_t>(base + object + 35989),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        rex::memory::load_and_swap<uint8_t>(base + object + 35992));
  }
  __imp__sub_82144FD8(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-predicate] fn=82144FD8 leave edge=0x{:04X} "
        "return={} status={} ready={} state=0x{:08X}",
        edge.mask, ctx.r3.u32 & 0xFFu,
        rex::memory::load_and_swap<uint8_t>(base + object + 35995),
        rex::memory::load_and_swap<uint8_t>(base + object + 35994),
        GuestWord(base, object + 35984));
  }
}

PPC_FUNC_IMPL(sub_821B3870) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t object = ctx.r3.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=821B3870 edge=0x{:04X} lr=0x{:08X} "
        "r3=0x{:08X} r4={} r5=0x{:08X} r6=0x{:08X} "
        "obj0=0x{:08X} obj8=0x{:08X} obj1C=0x{:08X} obj268=0x{:08X}",
        edge.mask, ctx.lr, object, ctx.r4.s32, ctx.r5.u32, ctx.r6.u32,
        GuestWord(base, object), GuestWord(base, object + 8),
        GuestWord(base, object + 0x1C), GuestWord(base, object + 0x268));
  }
  __imp__sub_821B3870(ctx, base);
  if (log) {
    REXLOG_INFO("[ac6-loadout-consumer] fn=821B3870 leave r3=0x{:08X} r4={} r5=0x{:08X}",
                ctx.r3.u32, ctx.r4.s32, ctx.r5.u32);
  }
}

PPC_FUNC_IMPL(sub_821AFCA0) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t object = ctx.r3.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=821AFCA0 edge=0x{:04X} lr=0x{:08X} "
        "r3=0x{:08X} r4=0x{:08X} state12={} counter72={} "
        "word24=0x{:08X} word28=0x{:08X}",
        edge.mask, ctx.lr, object, ctx.r4.u32,
        GuestWord(base, object + 12), GuestWord(base, object + 72),
        GuestWord(base, object + 24), GuestWord(base, object + 28));
  }
  __imp__sub_821AFCA0(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=821AFCA0 leave r3=0x{:08X} state12={} "
        "counter72={} word24=0x{:08X} word28=0x{:08X}",
        ctx.r3.u32, GuestWord(base, object + 12), GuestWord(base, object + 72),
        GuestWord(base, object + 24), GuestWord(base, object + 28));
  }
}

PPC_FUNC_IMPL(sub_821AFE38) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  constexpr uint32_t kInputGlobal = 0x8293BA10u;
  const uint32_t global = GuestWord(base, kInputGlobal);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=821AFE38 edge=0x{:04X} lr=0x{:08X} "
        "r3=0x{:08X} r4={} global=0x{:08X} g16=0x{:08X} g24=0x{:08X} "
        "g28=0x{:08X} g40={} g41={}",
        edge.mask, ctx.lr, ctx.r3.u32, ctx.r4.s32, global,
        GuestWord(base, global + 16), GuestWord(base, global + 24),
        GuestWord(base, global + 28),
        global ? rex::memory::load_and_swap<uint8_t>(base + global + 40) : 0,
        global ? rex::memory::load_and_swap<uint8_t>(base + global + 41) : 0);
  }
  __imp__sub_821AFE38(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=821AFE38 leave r3=0x{:08X} global=0x{:08X} "
        "g16=0x{:08X} g24=0x{:08X} g28=0x{:08X} g40={} g41={}",
        ctx.r3.u32, global, GuestWord(base, global + 16),
        GuestWord(base, global + 24), GuestWord(base, global + 28),
        global ? rex::memory::load_and_swap<uint8_t>(base + global + 40) : 0,
        global ? rex::memory::load_and_swap<uint8_t>(base + global + 41) : 0);
  }
}

PPC_FUNC_IMPL(sub_821F3BA0) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  __imp__sub_821F3BA0(ctx, base);
  if (log) {
    REXLOG_INFO("[ac6-loadout-consumer] fn=821F3BA0 edge=0x{:04X} lr=0x{:08X} "
                "return=0x{:08X}", edge.mask, ctx.lr, ctx.r3.u32);
  }
}

// The state-1 handler probes the global event-list entry 49 before deciding
// whether a confirmed loadout starts the mission or follows the menu-return
// path.  Keep this at the exact callsite so the result is not confused with
// unrelated event-list lookups elsewhere in the generated code.
PPC_FUNC_IMPL(sub_821539E0) {
  PPC_FUNC_PROLOGUE();
  const bool log = LoadoutProbeEnabled() && ctx.lr == 0x8218C6C8u &&
                   ctx.r4.u32 == 49u;
  const uint32_t object = ctx.r3.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-gate] fn=821539E0 enter lr=0x{:08X} object=0x{:08X} "
        "arg={} list=0x{:08X} tail=0x{:08X}",
        ctx.lr, object, ctx.r4.u32, GuestWord(base, object + 72),
        GuestWord(base, object + 96));
  }
  __imp__sub_821539E0(ctx, base);
  if (FirstMissionLaunchOverrideEnabled() && log) {
    ctx.r3.u64 = 0;
    REXLOG_WARN(
        "[ac6-loadout-launch-override] fn=821539E0 event=49 return=-1->0");
  }
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-gate] fn=821539E0 leave return=0x{:08X} "
        "list=0x{:08X} tail=0x{:08X}",
        ctx.r3.u32, GuestWord(base, object + 72),
        GuestWord(base, object + 96));
  }
}

// State 2 calls the resource/task graph gate once the short countdown reaches
// zero.  Its return value determines whether the native state machine can
// continue into mission setup or immediately falls back to state 0.
PPC_FUNC_IMPL(sub_821D29B0) {
  PPC_FUNC_PROLOGUE();
  const bool log = LoadoutProbeEnabled() && ctx.lr == 0x8218C85Cu;
  const uint32_t object = ctx.r3.u32;
  if (log) {
    const uint32_t vtable = GuestWord(base, object);
    REXLOG_INFO(
        "[ac6-loadout-gate] fn=821D29B0 enter lr=0x{:08X} object=0x{:08X} "
        "vtable=0x{:08X} child32=0x{:08X} child28=0x{:08X} "
        "head24=0x{:08X} flags36=0x{:08X}",
        ctx.lr, object, vtable, GuestWord(base, object + 32),
        GuestWord(base, object + 28), GuestWord(base, object + 24),
        GuestWord(base, object + 36));
    constexpr uint32_t kGuestLo = 0x82000000u;
    constexpr uint32_t kGuestHi = 0xC0000000u;
    uint32_t link = GuestWord(base, object + 28);
    for (uint32_t i = 0; i < 8 && link >= kGuestLo && link < kGuestHi;
         ++i) {
      const uint32_t child = GuestWord(base, link);
      const uint32_t child_vtable =
          child >= kGuestLo && child < kGuestHi ? GuestWord(base, child) : 0;
      const uint32_t slot12 = child_vtable >= kGuestLo &&
                                      child_vtable < kGuestHi
                                  ? GuestWord(base, child_vtable + 12)
                                  : 0;
      REXLOG_INFO(
          "[ac6-loadout-gate] state2-child index={} link=0x{:08X} "
          "child=0x{:08X} vtable=0x{:08X} slot12=0x{:08X} "
          "next=0x{:08X}",
          i, link, child, child_vtable, slot12, GuestWord(base, link + 4));
      link = GuestWord(base, link + 4);
    }
  }
  __imp__sub_821D29B0(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-gate] fn=821D29B0 leave return={} child32=0x{:08X} "
        "child28=0x{:08X} head24=0x{:08X} flags36=0x{:08X}",
        ctx.r3.s32, GuestWord(base, object + 32),
        GuestWord(base, object + 28), GuestWord(base, object + 24),
        GuestWord(base, object + 36));
  }
}

// The loadout update reaches this worker just before it inserts event 49 and
// publishes the confirmation byte.  A zero result is the observed reason the
// generated path leaves the byte at zero; retain the exact caller boundary.
PPC_FUNC_IMPL(sub_821D2860) {
  PPC_FUNC_PROLOGUE();
  const bool log = LoadoutProbeEnabled() && ctx.lr == 0x821472D8u;
  const uint32_t object = ctx.r3.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-gate] fn=821D2860 enter lr=0x{:08X} object=0x{:08X} "
        "child32=0x{:08X} child28=0x{:08X} head24=0x{:08X} flags36=0x{:08X}",
        ctx.lr, object, GuestWord(base, object + 32),
        GuestWord(base, object + 28), GuestWord(base, object + 24),
        GuestWord(base, object + 36));
  }
  __imp__sub_821D2860(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-gate] fn=821D2860 leave return={} child32=0x{:08X} "
        "child28=0x{:08X} head24=0x{:08X} flags36=0x{:08X}",
        ctx.r3.s32, GuestWord(base, object + 32),
        GuestWord(base, object + 28), GuestWord(base, object + 24),
        GuestWord(base, object + 36));
  }
}

// These are the three global transition slots reached after state-2's graph
// gate.  The first return value is the only branch that can abort that path;
// the other two mutate the transition records without returning a status.
PPC_FUNC_IMPL(sub_82124990) {
  PPC_FUNC_PROLOGUE();
  const bool log = LoadoutProbeEnabled() && ctx.lr == 0x8218C89Cu;
  const uint32_t object = ctx.r3.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-gate] fn=82124990 enter lr=0x{:08X} object=0x{:08X} "
        "arg4=0x{:08X} word800=0x{:08X} word104=0x{:08X}",
        ctx.lr, object, ctx.r4.u32, GuestWord(base, object + 800),
        GuestWord(base, object + 104));
  }
  __imp__sub_82124990(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-gate] fn=82124990 leave return={} word800=0x{:08X} "
        "word104=0x{:08X}",
        ctx.r3.s32, GuestWord(base, object + 800),
        GuestWord(base, object + 104));
  }
}

PPC_FUNC_IMPL(sub_82123E18) {
  PPC_FUNC_PROLOGUE();
  const bool log = LoadoutProbeEnabled() && ctx.lr == 0x8218C8C0u;
  const uint32_t object = ctx.r3.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-gate] fn=82123E18 enter lr=0x{:08X} object=0x{:08X} "
        "arg4={} arg5=0x{:08X}",
        ctx.lr, object, ctx.r4.u32, ctx.r5.u32);
  }
  __imp__sub_82123E18(ctx, base);
  if (log) {
    REXLOG_INFO("[ac6-loadout-gate] fn=82123E18 leave object=0x{:08X}",
                object);
  }
}

PPC_FUNC_IMPL(sub_821250F8) {
  PPC_FUNC_PROLOGUE();
  const bool log = LoadoutProbeEnabled() && ctx.lr == 0x8218C8D4u;
  const uint32_t object = ctx.r3.u32;
  if (log) {
    REXLOG_INFO("[ac6-loadout-gate] fn=821250F8 enter lr=0x{:08X} object=0x{:08X}",
                ctx.lr, object);
  }
  __imp__sub_821250F8(ctx, base);
  if (log) {
    REXLOG_INFO("[ac6-loadout-gate] fn=821250F8 leave object=0x{:08X}",
                object);
  }
}

// 8214B5F0 is the manager state setter behind its virtual transition slots.
// Record every bounded request so a missing state transition can be separated
// from a setter that ran but was later overwritten.
PPC_FUNC_IMPL(sub_8214B5F0) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> state_logs{0};
  const uint32_t object = ctx.r3.u32;
  const uint32_t requested = ctx.r4.u32;
  const uint32_t caller_lr = ctx.lr;
  const uint32_t sequence =
      state_logs.fetch_add(1, std::memory_order_relaxed) + 1;
  const bool log = LoadoutProbeEnabled() && sequence <= 128 && object;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-state] sequence={} enter manager=0x{:08X} "
        "lr=0x{:08X} requested={} current={} ready={} status={}",
        sequence, object, caller_lr, requested,
        GuestWord(base, object + 35984),
        rex::memory::load_and_swap<uint8_t>(base + object + 35994),
        rex::memory::load_and_swap<uint8_t>(base + object + 35995));
  }
  __imp__sub_8214B5F0(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-state] sequence={} leave manager=0x{:08X} "
        "requested={} current={} ready={} status={}",
        sequence, object, requested, GuestWord(base, object + 35984),
        rex::memory::load_and_swap<uint8_t>(base + object + 35994),
        rex::memory::load_and_swap<uint8_t>(base + object + 35995));
  }
}

// RTTI identifies level_root+0x36054 as CX360EffectManager. 822FA748 maps
// selectors to effect ids 202..208 through that manager; the overlap with
// CSelectAircraftManager event ids is numerical only.
PPC_FUNC_IMPL(sub_822FA748) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> command_logs{0};
  const uint32_t receiver = ctx.r3.u32;
  const uint32_t command = ctx.r4.u32;
  const uint32_t argument = ctx.r5.u32;
  const uint32_t caller_lr = ctx.lr;
  const uint32_t sequence =
      command_logs.fetch_add(1, std::memory_order_relaxed) + 1;
  const bool log = LoadoutProbeEnabled() && sequence <= 128;
  if (log) {
    const uint32_t level_root = GuestWord(base, 0x826E4EB4u);
    const uint32_t event_target =
        level_root ? GuestWord(base, level_root + 0x36054u) : 0;
    REXLOG_INFO(
        "[ac6-loadout-command] sequence={} enter receiver=0x{:08X} "
        "lr=0x{:08X} command={} argument=0x{:08X} "
        "level_root=0x{:08X} event_target=0x{:08X}",
        sequence, receiver, caller_lr, command, argument, level_root,
        event_target);
  }
  __imp__sub_822FA748(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-command] sequence={} leave receiver=0x{:08X} "
        "command={} return=0x{:08X}",
        sequence, receiver, command, ctx.r3.u32);
  }
}

// 82221A28 constructs the effect-side object whose vtable slot +0x7C is
// 822FA748. Keep this diagnostic separate from aircraft-selection dispatch.
PPC_FUNC_IMPL(sub_82221A28) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> constructor_logs{0};
  const uint32_t sequence =
      constructor_logs.fetch_add(1, std::memory_order_relaxed) + 1;
  const bool log = LoadoutProbeEnabled() && sequence <= 64;
  const uint32_t owner = ctx.r3.u32;
  const uint32_t storage = ctx.r4.u32;
  const uint32_t index = ctx.r5.u32;
  const uint32_t argument = ctx.r6.u32;
  const uint32_t caller_lr = ctx.lr;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-command-owner] sequence={} enter lr=0x{:08X} "
        "owner=0x{:08X} storage=0x{:08X} index={} argument=0x{:08X}",
        sequence, caller_lr, owner, storage, index, argument);
  }
  __imp__sub_82221A28(ctx, base);
  if (log) {
    const uint32_t object = ctx.r3.u32;
    REXLOG_INFO(
        "[ac6-loadout-command-owner] sequence={} leave object=0x{:08X} "
        "vtable=0x{:08X}",
        sequence, object, object ? GuestWord(base, object) : 0);
  }
}

// RTTI identifies 8221E6C0 as the constructor that installs the
// ACE6::CAce6ArmsManager vtable 82054E6C. Observe whether the manager itself
// exists before attributing the absent shared virtual slot-38 factory call to
// a missing manager lifecycle.
PPC_FUNC_IMPL(sub_8221E6C0) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> arms_manager_logs{0};
  const uint32_t sequence =
      arms_manager_logs.fetch_add(1, std::memory_order_relaxed) + 1;
  const bool log = LoadoutProbeEnabled() && sequence <= 16;
  const uint32_t manager = ctx.r3.u32;
  const uint32_t caller_lr = ctx.lr;
  if (log) {
    REXLOG_INFO(
        "[ac6-arms-manager] sequence={} enter lr=0x{:08X} "
        "manager=0x{:08X} prior_vtable=0x{:08X}",
        sequence, caller_lr, manager, manager ? GuestWord(base, manager) : 0);
  }
  __imp__sub_8221E6C0(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-arms-manager] sequence={} leave manager=0x{:08X} "
        "return=0x{:08X} vtable=0x{:08X}",
        sequence, manager, ctx.r3.u32, manager ? GuestWord(base, manager) : 0);
  }
}

// 82199F68 is CModeTaskGame's registered mode-dependent lifecycle callback.
// Literal PAL flow constructs and publishes the selected mission manager at
// owner+648 only for command -3; its campaign variant embeds the ArmsManager
// instances. Capture the natural command and mode before changing policy.
PPC_FUNC_IMPL(sub_82199F68) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> arms_lifecycle_logs{0};
  const uint32_t sequence =
      arms_lifecycle_logs.fetch_add(1, std::memory_order_relaxed) + 1;
  const bool log = LoadoutProbeEnabled() && sequence <= 64;
  const uint32_t owner = ctx.r3.u32;
  const int32_t command = ctx.r4.s32;
  const uint32_t caller_lr = ctx.lr;
  const uint32_t level_root = GuestWord(base, 0x826E4EB4u);
  const int32_t mode =
      level_root ? static_cast<int32_t>(GuestWord(base, level_root + 120u)) : -1;
  if (log) {
    REXLOG_INFO(
        "[ac6-arms-lifecycle] sequence={} enter lr=0x{:08X} "
        "owner=0x{:08X} command={} level_root=0x{:08X} mode={} "
        "published=0x{:08X}",
        sequence, caller_lr, owner, command, level_root, mode,
        owner ? GuestWord(base, owner + 648u) : 0);
  }
  __imp__sub_82199F68(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-arms-lifecycle] sequence={} leave owner=0x{:08X} command={} "
        "return=0x{:08X} published=0x{:08X}",
        sequence, owner, command, ctx.r3.u32,
        owner ? GuestWord(base, owner + 648u) : 0);
  }
}

// 821A7260 constructs the qualified CModeTaskLoading object (vtable 82064A54).
// Its initialization/update stages normally dispatch virtual slot +0x48,
// which resolves to 821A7A70 for this class.
PPC_FUNC_IMPL(sub_821A7260) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> mode_task_loading_ctor_logs{0};
  const uint32_t sequence = mode_task_loading_ctor_logs.fetch_add(
                                1, std::memory_order_relaxed) +
                            1;
  const bool log = LoadoutProbeEnabled() && sequence <= 16;
  const uint32_t object = ctx.r3.u32;
  const uint32_t caller_lr = ctx.lr;
  __imp__sub_821A7260(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-mode-task-loading] sequence={} lr=0x{:08X} "
        "object=0x{:08X} return=0x{:08X} vtable=0x{:08X} state={}",
        sequence, caller_lr, object, ctx.r3.u32,
        object ? GuestWord(base, object) : 0,
        object ? static_cast<int32_t>(GuestWord(base, object + 12u)) : -1);
  }
}

PPC_FUNC_IMPL(sub_821A72C0) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> mode_task_loading_init_logs{0};
  const uint32_t sequence = mode_task_loading_init_logs.fetch_add(
                                1, std::memory_order_relaxed) +
                            1;
  const bool log = LoadoutProbeEnabled() && sequence <= 16;
  const uint32_t object = ctx.r3.u32;
  const uint32_t caller_lr = ctx.lr;
  if (log) {
    REXLOG_INFO(
        "[ac6-mode-task-loading-init] sequence={} enter lr=0x{:08X} "
        "object=0x{:08X} vtable=0x{:08X} state={} phase=0x{:08X}",
        sequence, caller_lr, object, object ? GuestWord(base, object) : 0,
        object ? static_cast<int32_t>(GuestWord(base, object + 12u)) : -1,
        object ? GuestWord(base, object + 648u) : 0);
  }
  __imp__sub_821A72C0(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-mode-task-loading-init] sequence={} leave object=0x{:08X} "
        "return=0x{:08X} state={} phase=0x{:08X}",
        sequence, object, ctx.r3.u32,
        object ? static_cast<int32_t>(GuestWord(base, object + 12u)) : -1,
        object ? GuestWord(base, object + 648u) : 0);
  }
}

PPC_FUNC_IMPL(sub_821A75D0) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> mode_task_loading_update_logs{0};
  const uint32_t sequence = mode_task_loading_update_logs.fetch_add(
                                1, std::memory_order_relaxed) +
                            1;
  const bool log = LoadoutProbeEnabled() && sequence <= 32;
  const uint32_t object = ctx.r3.u32;
  const int32_t before =
      object ? static_cast<int32_t>(GuestWord(base, object + 12u)) : -1;
  __imp__sub_821A75D0(ctx, base);
  const int32_t after =
      object ? static_cast<int32_t>(GuestWord(base, object + 12u)) : -1;
  if (log && (sequence == 1 || before != after)) {
    REXLOG_INFO(
        "[ac6-mode-task-loading-update] sequence={} object=0x{:08X} "
        "vtable=0x{:08X} state={}->{} phase=0x{:08X}",
        sequence, object, object ? GuestWord(base, object) : 0, before, after,
        object ? GuestWord(base, object + 648u) : 0);
  }
}

// 821A7A70 selects one of two qualified mode-task factories and publishes it
// at level_root+16. Selector zero maps to 821BBF98, the base CModeTaskGame
// allocator. Capture both selection and publication without supplying either.
PPC_FUNC_IMPL(sub_821A7A70) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> mode_task_factory_selection_logs{0};
  const uint32_t sequence = mode_task_factory_selection_logs.fetch_add(
                                1, std::memory_order_relaxed) +
                            1;
  const bool log = LoadoutProbeEnabled() && sequence <= 16;
  const uint32_t caller_lr = ctx.lr;
  const int32_t selector = ctx.r4.s32;
  const uint32_t level_root = GuestWord(base, 0x826E4EB4u);
  const uint32_t before = level_root ? GuestWord(base, level_root + 16u) : 0;
  __imp__sub_821A7A70(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-mode-task-factory-select] sequence={} lr=0x{:08X} "
        "selector={} level_root=0x{:08X} factory=0x{:08X}->0x{:08X}",
        sequence, caller_lr, selector, level_root, before,
        level_root ? GuestWord(base, level_root + 16u) : 0);
  }
}

// 821BBF98 is the selector-zero factory. It allocates 656 bytes and invokes
// the base CModeTaskGame constructor when allocation succeeds.
PPC_FUNC_IMPL(sub_821BBF98) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> mode_task_factory_logs{0};
  const uint32_t sequence =
      mode_task_factory_logs.fetch_add(1, std::memory_order_relaxed) + 1;
  const bool log = LoadoutProbeEnabled() && sequence <= 16;
  const uint32_t caller_lr = ctx.lr;
  __imp__sub_821BBF98(ctx, base);
  if (log) {
    const uint32_t object = ctx.r3.u32;
    REXLOG_INFO(
        "[ac6-mode-task-factory] sequence={} lr=0x{:08X} "
        "return=0x{:08X} vtable=0x{:08X}",
        sequence, caller_lr, object, object ? GuestWord(base, object) : 0);
  }
}

// 82199BD8 constructs the base CModeTaskGame object. Its qualified base vtable
// is 82064384 and its callback registry starts at +0x268. This separates a
// missing mode-task construction from a constructed object whose virtual
// registration stage is skipped.
PPC_FUNC_IMPL(sub_82199BD8) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> mode_task_game_logs{0};
  const uint32_t sequence =
      mode_task_game_logs.fetch_add(1, std::memory_order_relaxed) + 1;
  const bool log = LoadoutProbeEnabled() && sequence <= 16;
  const uint32_t object = ctx.r3.u32;
  const uint32_t caller_lr = ctx.lr;
  if (log) {
    REXLOG_INFO(
        "[ac6-mode-task-game] sequence={} enter lr=0x{:08X} "
        "object=0x{:08X}",
        sequence, caller_lr, object);
  }
  __imp__sub_82199BD8(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-mode-task-game] sequence={} leave object=0x{:08X} "
        "return=0x{:08X} vtable=0x{:08X} registry_vtable=0x{:08X} "
        "published=0x{:08X}",
        sequence, object, ctx.r3.u32, object ? GuestWord(base, object) : 0,
        object ? GuestWord(base, object + 0x268u) : 0,
        object ? GuestWord(base, object + 648u) : 0);
  }
}

// 82199D08 is virtual slot 3 of the base CModeTaskGame vtable 82064384. It
// registers lifecycle callback 82199F68 on this+0x268 through 8219AAE8.
// Distinguish missing registration from a registered callback whose upstream
// dispatcher never invokes it.
PPC_FUNC_IMPL(sub_82199D08) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> arms_registration_logs{0};
  const uint32_t sequence =
      arms_registration_logs.fetch_add(1, std::memory_order_relaxed) + 1;
  const bool log = LoadoutProbeEnabled() && sequence <= 16;
  const uint32_t owner = ctx.r3.u32;
  const uint32_t caller_lr = ctx.lr;
  if (log) {
    REXLOG_INFO(
        "[ac6-mode-task-game-registration] sequence={} enter lr=0x{:08X} "
        "object=0x{:08X} registry=0x{:08X}",
        sequence, caller_lr, owner, owner ? owner + 0x268u : 0);
  }
  __imp__sub_82199D08(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-mode-task-game-registration] sequence={} leave object=0x{:08X} "
        "return=0x{:08X}",
        sequence, owner, ctx.r3.u32);
  }
}

// 820F6330 is the qualified SendMsgV export callback. Its first operation
// parses the four-byte Mddd string at *(r6), then broadcasts the numeric id to
// the 16-entry CSwgListener registry. Capture the untouched VM descriptor and
// register context; do not inject or rewrite messages.
PPC_FUNC_IMPL(sub_820F6330) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> send_logs{0};
  constexpr uint32_t kGuestLo = 0x82000000u;
  constexpr uint32_t kGuestHi = 0xC0000000u;
  const uint32_t descriptor = ctx.r6.u32;
  const bool descriptor_valid =
      descriptor >= kGuestLo && descriptor <= kGuestHi - 32u;
  const uint32_t message = descriptor_valid ? GuestWord(base, descriptor) : 0;
  const bool message_valid = message >= kGuestLo && message <= kGuestHi - 16u;
  const uint32_t sequence =
      send_logs.fetch_add(1, std::memory_order_relaxed) + 1;
  const bool log = LoadoutProbeEnabled() && sequence <= 128;
  if (log) {
    auto byte = [&](uint32_t offset) -> uint32_t {
      return message_valid
                 ? rex::memory::load_and_swap<uint8_t>(base + message + offset)
                 : 0;
    };
    REXLOG_INFO(
        "[ac6-swg-sendmsg] sequence={} lr=0x{:08X} "
        "r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} r6=0x{:08X} "
        "r7=0x{:08X} r8=0x{:08X} r9=0x{:08X} r10=0x{:08X} "
        "descriptor=0x{:08X} d0=0x{:08X} d4=0x{:08X} d8=0x{:08X} "
        "dC=0x{:08X} d10=0x{:08X} d14=0x{:08X} d18=0x{:08X} "
        "d1C=0x{:08X} message=0x{:08X} "
        "bytes={:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X} "
        "{:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X}",
        sequence, ctx.lr, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, descriptor,
        ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32, descriptor,
        descriptor_valid ? GuestWord(base, descriptor + 0x00) : 0,
        descriptor_valid ? GuestWord(base, descriptor + 0x04) : 0,
        descriptor_valid ? GuestWord(base, descriptor + 0x08) : 0,
        descriptor_valid ? GuestWord(base, descriptor + 0x0C) : 0,
        descriptor_valid ? GuestWord(base, descriptor + 0x10) : 0,
        descriptor_valid ? GuestWord(base, descriptor + 0x14) : 0,
        descriptor_valid ? GuestWord(base, descriptor + 0x18) : 0,
        descriptor_valid ? GuestWord(base, descriptor + 0x1C) : 0, message,
        byte(0), byte(1), byte(2), byte(3), byte(4), byte(5), byte(6), byte(7),
        byte(8), byte(9), byte(10), byte(11), byte(12), byte(13), byte(14),
        byte(15));
  }
  __imp__sub_820F6330(ctx, base);
}

// Pinpoint any divergence between the literal Mddd parser and the CRT decimal
// conversion it calls. These wrappers observe input/output only.
PPC_FUNC_IMPL(sub_820F62B0) {
  PPC_FUNC_PROLOGUE();
  const uint32_t input = ctx.r3.u32;
  const bool log = LoadoutProbeEnabled();
  if (log) {
    REXLOG_INFO(
        "[ac6-swg-message-parser] enter lr=0x{:08X} input=0x{:08X} "
        "bytes={:02X}{:02X}{:02X}{:02X}",
        ctx.lr, input, PPC_LOAD_U8(input + 0), PPC_LOAD_U8(input + 1),
        PPC_LOAD_U8(input + 2), PPC_LOAD_U8(input + 3));
  }
  __imp__sub_820F62B0(ctx, base);
  if (log) {
    REXLOG_INFO("[ac6-swg-message-parser] leave input=0x{:08X} result={}",
                input, ctx.r3.s32);
  }
}

// 8214D390 is CSelectAircraftManager's CSwgListener callback. It accepts
// general notifications (event 22 is observed) and dispatches ids 200..237
// through a local jump table. Capture the caller as well as state changes.
PPC_FUNC_IMPL(sub_8214D390) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> event_logs{0};
  const uint32_t object = ctx.r3.u32;
  const uint32_t event = ctx.r4.u32;
  const uint32_t caller_lr = ctx.lr;
  const uint32_t sequence =
      event_logs.fetch_add(1, std::memory_order_relaxed) + 1;
  const bool log = LoadoutProbeEnabled() && sequence <= 64;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-event] sequence={} enter lr=0x{:08X} "
        "manager=0x{:08X} event={} "
        "state=0x{:08X} b35988={} b35989={} b35990={} ready={}",
        sequence, caller_lr, object, event, GuestWord(base, object + 35984),
        rex::memory::load_and_swap<uint8_t>(base + object + 35988),
        rex::memory::load_and_swap<uint8_t>(base + object + 35989),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        rex::memory::load_and_swap<uint8_t>(base + object + 35994));
  }
  __imp__sub_8214D390(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-event] sequence={} leave manager=0x{:08X} event={} "
        "state=0x{:08X} b35988={} b35989={} b35990={} ready={}",
        sequence, object, event, GuestWord(base, object + 35984),
        rex::memory::load_and_swap<uint8_t>(base + object + 35988),
        rex::memory::load_and_swap<uint8_t>(base + object + 35989),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        rex::memory::load_and_swap<uint8_t>(base + object + 35994));
  }
}

// 82147070 is the high-level loadout update reached from the aircraft/weapon
// screens.  F3BA0 is only a getter; this wrapper captures the state object and
// the boolean hand-off through 821482B8 without touching generated code.
PPC_FUNC_IMPL(sub_82147070) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  static std::atomic<bool> manager_vtable_logged{false};
  static std::atomic<uint32_t> selection_transition_logs{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t object = ctx.r3.u32;
  auto log_fields = [&](const char* phase) {
    if (!log) {
      return;
    }
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=82147070 {} edge=0x{:04X} lr=0x{:08X} "
        "r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} r6=0x{:08X} "
        "obj0=0x{:08X} obj4=0x{:08X} obj8=0x{:08X} objC=0x{:08X} "
        "s35984=0x{:08X} b35988={} b35989={} b35990={} b35991={} "
        "b35992={} b35993={} b35994={} b35995={} "
        "w36112=0x{:08X} w36116=0x{:08X} w36120=0x{:08X} "
        "w36124=0x{:08X} s36208=0x{:08X}",
        phase, edge.mask, ctx.lr, object, ctx.r4.u32, ctx.r5.u32,
        ctx.r6.u32, GuestWord(base, object), GuestWord(base, object + 4),
        GuestWord(base, object + 8), GuestWord(base, object + 12),
        GuestWord(base, object + 35984),
        rex::memory::load_and_swap<uint8_t>(base + object + 35988),
        rex::memory::load_and_swap<uint8_t>(base + object + 35989),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        rex::memory::load_and_swap<uint8_t>(base + object + 35991),
        rex::memory::load_and_swap<uint8_t>(base + object + 35992),
        rex::memory::load_and_swap<uint8_t>(base + object + 35993),
        rex::memory::load_and_swap<uint8_t>(base + object + 35994),
        rex::memory::load_and_swap<uint8_t>(base + object + 35995),
        GuestWord(base, object + 36112), GuestWord(base, object + 36116),
        GuestWord(base, object + 36120), GuestWord(base, object + 36124),
        GuestWord(base, object + 36208));
  };
  const uint32_t selection_before = object ? GuestWord(base, object + 36120) : 0;
  log_fields("enter");
  if (log && object) {
    const uint32_t vtable = GuestWord(base, object);
    bool expected = false;
    if (vtable && manager_vtable_logged.compare_exchange_strong(expected, true)) {
      REXLOG_INFO(
          "[ac6-loadout-vtable] manager=0x{:08X} vtable=0x{:08X} "
          "known_b0=0x{:08X}",
          object, vtable, GuestWord(base, vtable + 0xB0));
      for (uint32_t offset = 0x40; offset <= 0x140; offset += 4) {
        const uint32_t target = GuestWord(base, vtable + offset);
        if (target) {
          REXLOG_INFO("[ac6-loadout-vtable] +0x{:03X}=0x{:08X}", offset,
                      target);
        }
      }
    }
  }
  __imp__sub_82147070(ctx, base);
  const uint32_t selection_after = object ? GuestWord(base, object + 36120) : 0;
  if (LoadoutProbeEnabled() && object && selection_before != selection_after) {
    const uint32_t sequence =
        selection_transition_logs.fetch_add(1, std::memory_order_relaxed) + 1;
    if (sequence <= 24) {
      const uint32_t candidate_count = GuestWord(base, object + 34304);
      REXLOG_INFO(
          "[ac6-loadout-selection-transition] sequence={} manager=0x{:08X} "
          "lr=0x{:08X} selected=0x{:08X}->0x{:08X} candidates={}",
          sequence, object, ctx.lr, selection_before, selection_after,
          candidate_count);
      for (uint32_t index = 0; index < std::min(candidate_count, 4u); ++index) {
        REXLOG_INFO(
            "[ac6-loadout-selection-candidate] sequence={} index={} "
            "a=0x{:08X} b=0x{:08X} c=0x{:08X} d=0x{:08X} "
            "primary=0x{:08X} secondary=0x{:08X} counter={}",
            sequence, index, GuestWord(base, object + 36024 + index * 4),
            GuestWord(base, object + 36032 + index * 4),
            GuestWord(base, object + 36040 + index * 4),
            GuestWord(base, object + 36048 + index * 4),
            GuestWord(base, object + 36056 + index * 4),
            GuestWord(base, object + 36064 + index * 4),
            GuestWord(base, object + 36104 + index * 4));
      }
    }
  }
  log_fields("leave");
}

PPC_FUNC_IMPL(sub_821482B8) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t object = ctx.r3.u32;
  const uint32_t selector = ctx.r4.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=821482B8 enter edge=0x{:04X} lr=0x{:08X} "
        "r3=0x{:08X} selector=0x{:08X} state=0x{:08X} slot=0x{:08X} "
        "obj35984=0x{:08X} obj35989={} obj35990={} obj36208=0x{:08X}",
        edge.mask, ctx.lr, object, selector, GuestWord(base, object + 36120),
        GuestWord(base, object + 36124), GuestWord(base, object + 35984),
        rex::memory::load_and_swap<uint8_t>(base + object + 35989),
        rex::memory::load_and_swap<uint8_t>(base + object + 35990),
        GuestWord(base, object + 36208));
  }
  __imp__sub_821482B8(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-consumer] fn=821482B8 leave edge=0x{:04X} return={} "
        "state=0x{:08X} slot=0x{:08X} obj35984=0x{:08X} obj36208=0x{:08X}",
        edge.mask, ctx.r3.u32, GuestWord(base, object + 36120),
        GuestWord(base, object + 36124), GuestWord(base, object + 35984),
        GuestWord(base, object + 36208));
  }
}

// The pad object is only the producer of the canonical edge.  These wrappers
// observe the two aggregate layers above it: the screen-state update and the
// four-child input fan-out.  They are intentionally diagnostic; the generated
// bodies remain untouched and the one-line-per-edge guards keep the probe from
// changing frame timing.
PPC_FUNC_IMPL(sub_821C5258) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t screen = ctx.r3.u32;
  const uint32_t arg4 = ctx.r4.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-screen] fn=821C5258 enter edge=0x{:04X} serial={} "
        "lr=0x{:08X} screen=0x{:08X} arg4=0x{:08X} state={} "
        "result={} mode={} type={} input=0x{:08X}",
        edge.mask, edge.serial, ctx.lr, screen, arg4,
        screen ? static_cast<int32_t>(GuestWord(base, screen + 68)) : 0,
        screen ? static_cast<int32_t>(GuestWord(base, screen + 12)) : 0,
        screen ? static_cast<int32_t>(GuestWord(base, screen + 24)) : 0,
        screen ? static_cast<int32_t>(GuestWord(base, screen + 28)) : 0,
        screen ? GuestWord(base, screen + 4) : 0);
  }
  __imp__sub_821C5258(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-screen] fn=821C5258 leave edge=0x{:04X} "
        "return={} state={} result={} mode={} type={}",
        edge.mask, ctx.r3.s32,
        screen ? static_cast<int32_t>(GuestWord(base, screen + 68)) : 0,
        screen ? static_cast<int32_t>(GuestWord(base, screen + 12)) : 0,
        screen ? static_cast<int32_t>(GuestWord(base, screen + 24)) : 0,
        screen ? static_cast<int32_t>(GuestWord(base, screen + 28)) : 0);
  }
}

PPC_FUNC_IMPL(sub_82343928) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t object = ctx.r3.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-input] fn=82343928 enter edge=0x{:04X} serial={} "
        "lr=0x{:08X} object=0x{:08X} flags=0x{:08X} "
        "vtable=0x{:08X} plus36=0x{:08X}",
        edge.mask, edge.serial, ctx.lr, object, ctx.r4.u32,
        GuestWord(base, object), GuestWord(base, object + 36));
  }
  __imp__sub_82343928(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-input] fn=82343928 leave edge=0x{:04X} "
        "return=0x{:08X} object=0x{:08X} plus36=0x{:08X}",
        edge.mask, ctx.r3.u32, object, GuestWord(base, object + 36));
  }
}

PPC_FUNC_IMPL(sub_82343AD0) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t owner = ctx.r3.u32;
  const uint32_t child0 = GuestWord(base, owner + 4);
  const uint32_t child1 = GuestWord(base, owner + 8);
  const uint32_t child2 = GuestWord(base, owner + 12);
  const uint32_t child3 = GuestWord(base, owner + 16);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-input] fn=82343AD0 enter edge=0x{:04X} serial={} "
        "lr=0x{:08X} owner=0x{:08X} flags=0x{:08X} gate1=0x{:08X} "
        "gate2=0x{:08X} children={:08X},{:08X},{:08X},{:08X} "
        "targets={:08X},{:08X},{:08X},{:08X}",
        edge.mask, edge.serial, ctx.lr, owner, ctx.r4.u32,
        GuestWord(base, owner + 644), GuestWord(base, owner + 652), child0,
        child1, child2, child3,
        GuestWord(base, GuestWord(base, child0) + 16),
        GuestWord(base, GuestWord(base, child1) + 16),
        GuestWord(base, GuestWord(base, child2) + 16),
        GuestWord(base, GuestWord(base, child3) + 16));
  }
  __imp__sub_82343AD0(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-input] fn=82343AD0 leave edge=0x{:04X} "
        "return={} owner=0x{:08X}",
        edge.mask, ctx.r3.s32, owner);
  }
}

PPC_FUNC_IMPL(sub_8234D3F0) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t self = ctx.r3.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-input] fn=8234D3F0 enter edge=0x{:04X} serial={} "
        "lr=0x{:08X} self=0x{:08X} device=0x{:08X} state={} "
        "edge_field=0x{:08X} repeat=0x{:08X} raw=0x{:08X}",
        edge.mask, edge.serial, ctx.lr, self, GuestWord(base, self + 4),
        static_cast<int32_t>(GuestWord(base, self + 8)),
        GuestWord(base, self + 20), GuestWord(base, self + 36),
        GuestWord(base, self + 132));
  }
  __imp__sub_8234D3F0(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-input] fn=8234D3F0 leave edge=0x{:04X} return={} "
        "self=0x{:08X} state={} edge_field=0x{:08X} repeat=0x{:08X} "
        "raw=0x{:08X}",
        edge.mask, ctx.r3.s32, self,
        static_cast<int32_t>(GuestWord(base, self + 8)),
        GuestWord(base, self + 20), GuestWord(base, self + 36),
        GuestWord(base, self + 132));
  }
}

PPC_FUNC_IMPL(sub_8234D478) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint64_t> last_serial{0};
  const LoadoutEdge edge = LastLoadoutEdge();
  const bool log = LoadoutProbeEnabled() && edge.mask &&
                   edge.serial != last_serial.exchange(edge.serial);
  const uint32_t self = ctx.r3.u32;
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-input] fn=8234D478 enter edge=0x{:04X} serial={} "
        "lr=0x{:08X} self=0x{:08X} device=0x{:08X} state={} "
        "edge_field=0x{:08X} repeat=0x{:08X} raw=0x{:08X}",
        edge.mask, edge.serial, ctx.lr, self, GuestWord(base, self + 4),
        static_cast<int32_t>(GuestWord(base, self + 8)),
        GuestWord(base, self + 20), GuestWord(base, self + 36),
        GuestWord(base, self + 132));
  }
  __imp__sub_8234D478(ctx, base);
  if (log) {
    REXLOG_INFO(
        "[ac6-loadout-input] fn=8234D478 leave edge=0x{:04X} return={} "
        "self=0x{:08X} state={} edge_field=0x{:08X} repeat=0x{:08X} "
        "raw=0x{:08X}",
        edge.mask, ctx.r3.s32, self,
        static_cast<int32_t>(GuestWord(base, self + 8)),
        GuestWord(base, self + 20), GuestWord(base, self + 36),
        GuestWord(base, self + 132));
  }
}

PPC_FUNC_IMPL(rex_sub_8234D510) {
  PPC_FUNC_PROLOGUE();

  // The selector screens reach this routine through a virtual input slot.  A
  // global "one line per edge" guard misses the useful object because the pad
  // owns several sibling contexts; retain one observation per object for the
  // current edge instead.  The fields are the values produced by
  // sub_8234D378/sub_8234D210 immediately before the screen consumes them.
  const uint32_t loadout_self = ctx.r3.u32;
  const LoadoutEdge loadout_edge = LastLoadoutEdge();
  bool log_loadout_dispatch = false;
  if (LoadoutProbeEnabled() && loadout_edge.mask && loadout_self) {
    static std::mutex loadout_mutex;
    static std::map<std::pair<uint64_t, uint32_t>, bool> loadout_seen;
    std::lock_guard<std::mutex> lock(loadout_mutex);
    const auto key = std::make_pair(loadout_edge.serial, loadout_self);
    if (loadout_seen.emplace(key, true).second) {
      // Bound the map to the active run; it is diagnostic state only and must
      // not grow with every frame of a long mission.
      if (loadout_seen.size() > 256) {
        for (auto it = loadout_seen.begin(); it != loadout_seen.end();) {
          if (it->first.first + 8 < loadout_edge.serial) {
            it = loadout_seen.erase(it);
          } else {
            ++it;
          }
        }
      }
      log_loadout_dispatch = true;
    }
  }
  const uint32_t state_before = GuestWord(base, loadout_self + 8);
  const uint32_t edge_before = GuestWord(base, loadout_self + 20);
  const uint32_t repeat_before = GuestWord(base, loadout_self + 36);
  const uint32_t raw_before = GuestWord(base, loadout_self + 132);
  if (log_loadout_dispatch) {
    REXLOG_INFO(
        "[ac6-loadout-dispatch] enter edge=0x{:04X} serial={} lr=0x{:08X} "
        "this=0x{:08X} state={} edge_field=0x{:08X} repeat=0x{:08X} "
        "raw=0x{:08X} r4=0x{:08X} r5=0x{:08X}",
        loadout_edge.mask, loadout_edge.serial, ctx.lr, loadout_self,
        static_cast<int32_t>(state_before), edge_before, repeat_before,
        raw_before, ctx.r4.u32, ctx.r5.u32);
  }

  if (REXCVAR_GET(ac6_log_ui_dispatch)) {
    // The selector is NOT the incoming r11. The dispatcher loads it from
    // [this+8] after its prologue, so reading ctx.r11 on entry yields the
    // caller's leftover value -- which is what made every object report one
    // uniform branch in cycle 412. Read the field from guest memory instead,
    // before the call, since the callee writes its result back to it.
    const uint32_t self = ctx.r3.u32;
    const int32_t state =
        self ? static_cast<int32_t>(rex::memory::load_and_swap<uint32_t>(base + self + 8)) : 0;

    static std::mutex mutex;
    static std::map<std::pair<uint32_t, int32_t>, uint64_t> seen;
    static uint64_t calls = 0;
    std::lock_guard<std::mutex> lock(mutex);
    ++seen[{self, state}];
    if ((++calls % 600) == 0) {
      for (const auto& [key, count] : seen) {
        REXLOG_INFO("[ac6-ui-dispatch] this=0x{:08X} state={} branch={} calls={}", key.first,
                    key.second, BranchFor(key.second), count);
      }
    }
  }

  __imp__rex_sub_8234D510(ctx, base);

  if (log_loadout_dispatch) {
    REXLOG_INFO(
        "[ac6-loadout-dispatch] leave edge=0x{:04X} serial={} this=0x{:08X} "
        "return={} state={} edge_field=0x{:08X} repeat=0x{:08X} raw=0x{:08X}",
        loadout_edge.mask, loadout_edge.serial, loadout_self, ctx.r3.s32,
        static_cast<int32_t>(GuestWord(base, loadout_self + 8)),
        GuestWord(base, loadout_self + 20), GuestWord(base, loadout_self + 36),
        GuestWord(base, loadout_self + 132));
  }
}

// Cycle 427: which field does the menu actually read?
//
// sub_8234D378 computes pressed edges into [this+20]; sub_8234D210 computes the
// auto-repeat output into [this+36]. Cycle 426's hypothesis is that navigation
// reads the repeat output and confirmation reads the edges, which would explain
// a responsive d-pad beside inert face buttons. Nothing measured said so, hence
// this.
//
// Sampled after the original runs, so both fields hold this poll's values. Only
// non-zero samples are logged, so the output is one line per actual button
// event rather than a flood at frame rate.
PPC_FUNC_IMPL(rex_sub_8234D210) {
  PPC_FUNC_PROLOGUE();
  const uint32_t self = ctx.r3.u32;
  __imp__rex_sub_8234D210(ctx, base);

  // The pad poll runs every frame for the whole run, whereas the screen update
  // only runs once a particular screen exists. Flushing from the screen meant a
  // capture armed before the dialog appeared was never written at all.
  ac6::trace::FlushIfComplete();
  ac6::stores::FlushIfComplete();

  if (self) {
    const uint32_t edge = rex::memory::load_and_swap<uint32_t>(base + self + 20);
    // XINPUT_GAMEPAD_A. Latched unconditionally, not under the logging cvar, so
    // a forcing run does not have to also enable logging to work.
    if (edge & 0x1000u) {
      g_confirm_pressed.store(true, std::memory_order_relaxed);
    }
    if (edge) {
      g_last_edge.store(edge, std::memory_order_relaxed);
      g_binding_probe_edge.store(edge, std::memory_order_relaxed);
      g_loadout_edge.store(edge, std::memory_order_relaxed);
      g_loadout_edge_serial.fetch_add(1, std::memory_order_relaxed);
      // Arm the control-flow trace here rather than in the screen update: this
      // is the earliest point at which the guest itself has decided a press
      // happened, so the capture window starts before any consumer of the press
      // has run. Arming a frame later would miss the very functions the diff is
      // looking for.
      if (!REXCVAR_GET(ac6_trace_mission_edges) ||
          g_first_mission_stage_armed.load(std::memory_order_relaxed)) {
        ac6::trace::Arm(edge);
      }
      ac6::stores::Arm(edge);
    }
  }
  if (REXCVAR_GET(ac6_log_ui_dispatch) && self) {
    const uint32_t pressed = rex::memory::load_and_swap<uint32_t>(base + self + 20);
    const uint32_t repeat = rex::memory::load_and_swap<uint32_t>(base + self + 36);
    const uint32_t current = rex::memory::load_and_swap<uint32_t>(base + self + 28);
    const uint32_t delay = rex::memory::load_and_swap<uint32_t>(base + self + 120);
    const uint32_t interval = rex::memory::load_and_swap<uint32_t>(base + self + 124);
    if (pressed || repeat) {
      REXLOG_INFO("[ac6-edges] this=0x{:08X} pressed=0x{:04X} repeat=0x{:04X} cur=0x{:04X} "
                  "delay={} interval={}",
                  self, pressed, repeat, current, delay, interval);
    }
  }
}

// Identify the screen on top, and the input context it owns.
//
// The vtable pointer names the class and its slots locate its code. [screen+4]
// is the input context: sub_821B9828, vtable slot 3 of the owner, computes
// this+96, initialises it through sub_821CE088, and stores that pointer into
// this+0x17DE4, which is screen+4. Cycles 428-436 modelled the fields at
// offsets 84-100 of that object as a screen state machine, which they are not;
// 84 reads "a system modal is up" (2 while XamShowDeviceSelectorUI is pending,
// 0 when none is) and 92-100 are an embedded X_OVERLAPPED.
PPC_FUNC_IMPL(rex_sub_821C56F8) {
  PPC_FUNC_PROLOGUE();
  const uint32_t screen = ctx.r3.u32;
  const uint32_t call_arg4 = ctx.r4.u32;
  const uint32_t call_arg5 = ctx.r5.u32;
  const uint32_t call_arg6 = ctx.r6.u32;
  const uint32_t state_before_call =
      screen ? GuestWord(base, screen + 68) : 0;
  __imp__rex_sub_821C56F8(ctx, base);

  const uint32_t edge = g_last_edge.exchange(0, std::memory_order_relaxed);
  if (edge && LoadoutProbeEnabled()) {
    REXLOG_INFO(
        "[ac6-loadout-screen-call] screen=0x{:08X} edge=0x{:04X} "
        "lr=0x{:08X} args={:08X},{:08X},{:08X} state_before={} "
        "state_after={} return={} result={} mode={} type={} input=0x{:08X}",
        screen, edge, ctx.lr, call_arg4, call_arg5, call_arg6,
        static_cast<int32_t>(state_before_call),
        screen ? static_cast<int32_t>(GuestWord(base, screen + 68)) : 0,
        ctx.r3.s32, screen ? static_cast<int32_t>(GuestWord(base, screen + 12)) : 0,
        screen ? static_cast<int32_t>(GuestWord(base, screen + 24)) : 0,
        screen ? static_cast<int32_t>(GuestWord(base, screen + 28)) : 0,
        screen ? GuestWord(base, screen + 4) : 0);
  }
  if (screen) {
    const uint32_t inner_state =
        rex::memory::load_and_swap<uint32_t>(base + screen + 68);
    const uint32_t dialog_type =
        rex::memory::load_and_swap<uint32_t>(base + screen + 28);
    const uint32_t response = g_save_dialog_input_bridge.Observe(
        edge, inner_state == 9 && dialog_type == 30, dialog_type);
    if (response) {
      rex::memory::store_and_swap<uint32_t>(base + screen + 12, response);
      REXLOG_INFO("[ac6-save-confirm] edge=0x{:04X} type={} response={}", edge,
                  dialog_type, response);
    }
  }

  // Keep the inner storage state machine distinct from the outer screen log.
  // In particular, a selector acceptance can be followed either by a later
  // type-6 response or by an asynchronous non-zero load result.  Logging only
  // the outer fields cannot distinguish those paths when they occur between
  // two frame-boundary samples.
  if (REXCVAR_GET(ac6_log_ui_dispatch_verbose) && screen) {
    struct InnerSnapshot {
      uint32_t state;
      uint32_t selector;
      uint32_t response;
      uint32_t type;
      uint32_t result;
      uint32_t mode;
      uint32_t storage_mode;
      uint32_t slot;
    };
    const InnerSnapshot now{
        rex::memory::load_and_swap<uint32_t>(base + screen + 68),
        rex::memory::load_and_swap<uint32_t>(base + screen + 44),
        rex::memory::load_and_swap<uint32_t>(base + screen + 12),
        rex::memory::load_and_swap<uint32_t>(base + screen + 28),
        rex::memory::load_and_swap<uint32_t>(base + screen + 36),
        rex::memory::load_and_swap<uint32_t>(base + screen + 388),
        rex::memory::load_and_swap<uint32_t>(base + screen + 392),
        rex::memory::load_and_swap<uint32_t>(base + screen + 396),
    };
    static std::mutex inner_mutex;
    static bool primed = false;
    static uint32_t last_screen = 0;
    static InnerSnapshot last{};
    static uint32_t lines = 0;
    std::lock_guard<std::mutex> lock(inner_mutex);
    const bool changed = !primed || screen != last_screen ||
        now.state != last.state || now.selector != last.selector ||
        now.response != last.response || now.type != last.type ||
        now.result != last.result || now.mode != last.mode ||
        now.storage_mode != last.storage_mode || now.slot != last.slot;
    if (changed && lines++ < 256) {
      REXLOG_INFO(
          "[ac6-save-inner] screen=0x{:08X} before={} after={} selector={} "
          "response={} type={} result={} mode388={} mode392={} slot396={} return={}",
          screen, state_before_call, now.state, now.selector, now.response,
          now.type, now.result, now.mode, now.storage_mode, now.slot, ctx.r3.s32);
      last = now;
      last_screen = screen;
      primed = true;
    }
  }

  // Runs on the screen update because that is a reliable once-per-frame point
  // outside the trace callback itself. Writing the file from inside the
  // callback would recurse and would stall the frame that is being captured.
  ac6::trace::FlushIfComplete();
  ac6::stores::FlushIfComplete();
  ac6::text_probe::Arm();

  if (!REXCVAR_GET(ac6_log_ui_dispatch) || !screen) {
    return;
  }

  static std::mutex mutex;
  static uint64_t calls = 0;
  std::lock_guard<std::mutex> lock(mutex);

  // The state word this screen dispatches on. sub_821C56F8 switches six ways on
  // a value 0..5, and sub_821C5258 -- the predicate gating one of those cases --
  // writes the next state here. Nothing in fifty cycles ever reported what
  // values it actually takes, which is the cheapest fact available about the
  // screen and was never collected: a field pinned at one value names the state
  // the screen is stuck in, and a field that cycles says it is alive and the
  // transition out is what is missing.
  //
  // Logged on change rather than on a timer, so the output is one line per
  // transition instead of a frame-rate flood, and a press that moves the state
  // even briefly cannot fall between two samples.
  {
    static bool primed = false;
    static uint32_t last_state = 0;
    const uint32_t state = rex::memory::load_and_swap<uint32_t>(base + screen + 68);
    if (!primed || state != last_state) {
      REXLOG_INFO("[ac6-state] screen=0x{:08X} [+68] {} -> {}", screen,
                  primed ? int32_t(last_state) : -1, int32_t(state));
      last_state = state;
      primed = true;
    }
  }

  // P2.1: does the press reach the layer the UI consumes?
  //
  // AC6 maps raw XInput -> canonical -> 32 logical slots (sub_821CE088 turns
  // raw A 0x1000 into canonical 0x20; the bindings inside sub_821BE1F8 give A
  // logical slots 0 and 23), and the UI reads logical bits. Every cycle from
  // 401 to 452 instrumented only the raw pad wrapper, so the canonical and
  // logical layers -- exactly where a defect produces "correct raw edge,
  // navigation works, confirm dead" -- were never looked at.
  //
  // [screen+4] is the input context this screen owns, initialised by
  // sub_821CE088 at owner+96. A frame-to-frame diff of it, reported only on the
  // frame an edge arrives, says whether A sets anything there. Bounded to one
  // object and 160 bytes -- this is deliberately not the 32 MB scan that cost
  // twelve cycles.
  //
  // Right and Left are the positive controls: they visibly move the highlight,
  // so they MUST show a difference. If they do not, the instrument is wrong and
  // A's silence means nothing.
  {
    const uint32_t input_ctx = rex::memory::load_and_swap<uint32_t>(base + screen + 4);
    constexpr uint32_t kWords = 40;  // 160 bytes
    static uint32_t prev[kWords] = {};
    static bool primed = false;
    if (input_ctx) {
      uint32_t now[kWords];
      for (uint32_t i = 0; i < kWords; ++i) {
        now[i] = rex::memory::load_and_swap<uint32_t>(base + input_ctx + i * 4);
      }
      if (primed && edge) {
        uint32_t changed = 0;
        for (uint32_t i = 0; i < kWords; ++i) {
          if (now[i] != prev[i]) {
            ++changed;
            REXLOG_INFO("[ac6-inputctx] edge=0x{:04X} obj=0x{:08X} +{} : 0x{:08X} -> 0x{:08X}",
                        edge, input_ctx, i * 4, prev[i], now[i]);
          }
        }
        REXLOG_INFO("[ac6-inputctx] edge=0x{:04X} --- {} of {} words changed ---", edge, changed,
                    kWords);
      }
      for (uint32_t i = 0; i < kWords; ++i) {
        prev[i] = now[i];
      }
      primed = true;
    }
  }

  // P1 of the plan: bound the problem before solving it. Nothing establishes
  // that this dialog is the only gate to mission 1, so drive the transition the
  // confirm button fails to drive and see how far the game gets.
  //
  // Fires only once the screen is actually parked, which it announces itself:
  // sub_821C56F8 dispatches six ways on 0..5, and the state observed here runs
  // 0 -> 1 -> 2 -> 5 -> 3 -> 9 within half a second of the screen appearing and
  // then never moves again. 9 is outside the switch's range, so every later
  // frame falls through the dispatcher having done nothing -- which is exactly
  // "redraws an unchanging image and acts on none of the buttons it receives".
  //
  // Waiting for state > 5 rather than firing on the first call matters: the
  // cvar is set at startup, the screen object exists long before the dialog
  // does, and forcing a value into a state machine that is still running its
  // opening sequence tests nothing.
  //
  // Note which value the sequence never visits: 4.
  {
    const int32_t forced = REXCVAR_GET(ac6_force_screen_state);
    if (forced >= 0 && g_confirm_pressed.exchange(false, std::memory_order_relaxed)) {
      const uint32_t before = rex::memory::load_and_swap<uint32_t>(base + screen + 68);
      rex::memory::store_and_swap<uint32_t>(base + screen + 68, uint32_t(forced));
      REXLOG_WARN("[ac6-force-state] A pressed: screen=0x{:08X} [+68] {} -> {} (forced)", screen,
                  int32_t(before), forced);
    }
  }

  if ((++calls % 60) != 0) {
    return;
  }

  const uint32_t vtable = rex::memory::load_and_swap<uint32_t>(base + screen + 0);
  uint32_t slot0 = 0, slot1 = 0, slot2 = 0;
  if (vtable) {
    slot0 = rex::memory::load_and_swap<uint32_t>(base + vtable + 0);
    slot1 = rex::memory::load_and_swap<uint32_t>(base + vtable + 4);
    slot2 = rex::memory::load_and_swap<uint32_t>(base + vtable + 8);
  }
  REXLOG_INFO("[ac6-screen-id] screen=0x{:08X} vtable=0x{:08X} slots={:08X},{:08X},{:08X}", screen,
              vtable, slot0, slot1, slot2);

  const uint32_t input_ctx = rex::memory::load_and_swap<uint32_t>(base + screen + 4);
  if (input_ctx) {
    REXLOG_INFO("[ac6-screen] inputctx=0x{:08X} modal={} device_id=0x{:08X} ovl_result=0x{:08X} "
                "ovl_ext=0x{:08X} ovl_len=0x{:08X}",
                input_ctx,
                int32_t(rex::memory::load_and_swap<uint32_t>(base + input_ctx + 84)),
                rex::memory::load_and_swap<uint32_t>(base + input_ctx + 88),
                rex::memory::load_and_swap<uint32_t>(base + input_ctx + 92),
                rex::memory::load_and_swap<uint32_t>(base + input_ctx + 96),
                rex::memory::load_and_swap<uint32_t>(base + input_ctx + 100));
  }
}

// UI owner above the save state machines. It publishes [save+24/+28/+32] to
// the visible modal and owns the close/selection bytes at +64..+67. Those are
// the authoritative boundary for distinguishing a wrong save branch from a
// modal that was configured correctly but never reports its choice.
PPC_FUNC_IMPL(sub_82158DF0) {
  PPC_FUNC_PROLOGUE();
  const uint32_t owner = ctx.r3.u32;
  __imp__sub_82158DF0(ctx, base);
  if (!REXCVAR_GET(ac6_log_ui_dispatch) || !owner) return;

  struct Snapshot {
    uint32_t state;
    uint32_t latched_result;
    uint32_t prior_kind;
    uint8_t active;
    uint8_t closing;
    uint8_t closed;
    uint8_t modal;
  };
  const Snapshot now = {
      rex::memory::load_and_swap<uint32_t>(base + owner + 48),
      rex::memory::load_and_swap<uint32_t>(base + owner + 56),
      rex::memory::load_and_swap<uint32_t>(base + owner + 60),
      rex::memory::load_and_swap<uint8_t>(base + owner + 64),
      rex::memory::load_and_swap<uint8_t>(base + owner + 65),
      rex::memory::load_and_swap<uint8_t>(base + owner + 66),
      rex::memory::load_and_swap<uint8_t>(base + owner + 67),
  };
  static Snapshot last = {};
  static uint32_t last_owner = 0;
  static bool primed = false;
  if (!primed || last_owner != owner || now.state != last.state ||
      now.latched_result != last.latched_result || now.prior_kind != last.prior_kind ||
      now.active != last.active || now.closing != last.closing ||
      now.closed != last.closed || now.modal != last.modal) {
    REXLOG_INFO("[ac6-save-ui] owner=0x{:08X} state48={} result56={} kind60={} "
                "active64={} closing65={} closed66={} modal67={}",
                owner, now.state, now.latched_result, now.prior_kind, now.active,
                now.closing, now.closed, now.modal);
    last = now;
    last_owner = owner;
    primed = true;
  }
}

// The save browser owns two nested state machines. The outer one creates the
// first, one-button warning (dialog type 4); only later does rex_sub_821C56F8
// enter the storage/create path that can create the YES/NO dialog (type 30).
// Log the outer fields on change so a one-frame type-4 dialog cannot disappear
// between screenshots or the once-per-second screen identity samples.
PPC_FUNC_IMPL(sub_821C37E0) {
  PPC_FUNC_PROLOGUE();
  const uint32_t screen = ctx.r3.u32;
  const uint32_t call_arg4 = ctx.r4.u32;
  const uint32_t call_arg5 = ctx.r5.u32;
  const uint32_t call_arg6 = ctx.r6.u32;
  static std::atomic<uint64_t> last_loadout_serial{0};
  const LoadoutEdge loadout_edge = LastLoadoutEdge();
  const bool log_loadout =
      LoadoutProbeEnabled() && loadout_edge.mask &&
      loadout_edge.serial != last_loadout_serial.exchange(loadout_edge.serial);
  if (log_loadout && screen) {
    REXLOG_INFO(
        "[ac6-loadout-owner] fn=821C37E0 enter edge=0x{:04X} serial={} "
        "lr=0x{:08X} screen=0x{:08X} args={:08X},{:08X},{:08X} "
        "vtable=0x{:08X} state40={} selector44={} response12={} type28={} "
        "result36={} inner68={} outer72={} mode388={} mode392={} slot396={}",
        loadout_edge.mask, loadout_edge.serial, ctx.lr, screen, call_arg4,
        call_arg5, call_arg6, GuestWord(base, screen),
        static_cast<int32_t>(GuestWord(base, screen + 40)),
        static_cast<int32_t>(GuestWord(base, screen + 44)),
        static_cast<int32_t>(GuestWord(base, screen + 12)),
        static_cast<int32_t>(GuestWord(base, screen + 28)),
        static_cast<int32_t>(GuestWord(base, screen + 36)),
        static_cast<int32_t>(GuestWord(base, screen + 68)),
        static_cast<int32_t>(GuestWord(base, screen + 72)),
        static_cast<int32_t>(GuestWord(base, screen + 388)),
        static_cast<int32_t>(GuestWord(base, screen + 392)),
        static_cast<int32_t>(GuestWord(base, screen + 396)));
  }

  // PAL default.xex
  // acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde:
  // sub_821C3BE8 state 3 reads [screen+16] before doing anything else. Feed
  // the guest-computed A edge into that retail selection boundary before the
  // outer update calls it. Writing after the call delays forever because the
  // native input path never has another producer for this non-modal selector.
  if (screen) {
    const ac6::save_dialog::FileBrowserState state{
        rex::memory::load_and_swap<uint32_t>(base + screen + 40),
        rex::memory::load_and_swap<uint32_t>(base + screen + 44),
        rex::memory::load_and_swap<uint32_t>(base + screen + 16),
        rex::memory::load_and_swap<uint32_t>(base + screen + 12),
        rex::memory::load_and_swap<uint32_t>(base + screen + 28),
        rex::memory::load_and_swap<uint32_t>(base + screen + 36),
        rex::memory::load_and_swap<uint32_t>(base + screen + 68),
        rex::memory::load_and_swap<uint32_t>(base + screen + 72),
        rex::memory::load_and_swap<uint32_t>(base + screen + 388),
    };
    if (ac6::save_dialog::IsFileBrowserWaiting(state)) {
      const uint32_t edge = g_last_edge.exchange(0, std::memory_order_relaxed);
      const uint32_t selection =
          ac6::save_dialog::FileBrowserSelection(edge, state);
      if (selection) {
        rex::memory::store_and_swap<uint32_t>(base + screen + 16, selection);
        REXLOG_INFO("[ac6-file-confirm] edge=0x{:04X} selection={}", edge,
                    selection);
      }
    }
  }
  __imp__sub_821C37E0(ctx, base);
  if (log_loadout && screen) {
    REXLOG_INFO(
        "[ac6-loadout-owner] fn=821C37E0 leave edge=0x{:04X} "
        "return={} state40={} selector44={} response12={} type28={} "
        "result36={} inner68={} outer72={} mode388={} mode392={} slot396={}",
        loadout_edge.mask, ctx.r3.s32,
        static_cast<int32_t>(GuestWord(base, screen + 40)),
        static_cast<int32_t>(GuestWord(base, screen + 44)),
        static_cast<int32_t>(GuestWord(base, screen + 12)),
        static_cast<int32_t>(GuestWord(base, screen + 28)),
        static_cast<int32_t>(GuestWord(base, screen + 36)),
        static_cast<int32_t>(GuestWord(base, screen + 68)),
        static_cast<int32_t>(GuestWord(base, screen + 72)),
        static_cast<int32_t>(GuestWord(base, screen + 388)),
        static_cast<int32_t>(GuestWord(base, screen + 392)),
        static_cast<int32_t>(GuestWord(base, screen + 396)));
  }

  // The file-create prompt in sub_821C3BE8 and the post-create notices in
  // sub_821C4FA0 no longer call rex_sub_821C56F8. Consume the same guest edge
  // here so their visible modal responses reach the save owner.
  if (screen) {
    const uint32_t outer_state =
        rex::memory::load_and_swap<uint32_t>(base + screen + 40);
    const uint32_t selector_state =
        rex::memory::load_and_swap<uint32_t>(base + screen + 44);
    const uint32_t create_state =
        rex::memory::load_and_swap<uint32_t>(base + screen + 64);
    const uint32_t dialog_type =
        rex::memory::load_and_swap<uint32_t>(base + screen + 28);
    const bool waiting_for_file_create =
        outer_state == 8 &&
        ((selector_state == 4 && dialog_type == 6) ||
         (selector_state == 7 && dialog_type == 8) ||
         (selector_state == 8 && dialog_type == 10));
    const bool waiting_for_create_notice =
        (create_state == 2 && dialog_type == 37) ||
        (create_state == 6 && dialog_type == 35);
    if (waiting_for_file_create || waiting_for_create_notice) {
      const uint32_t edge = g_last_edge.exchange(0, std::memory_order_relaxed);
      const uint32_t response =
          g_save_dialog_input_bridge.Observe(edge, true, dialog_type);
      if (response) {
        rex::memory::store_and_swap<uint32_t>(base + screen + 12, response);
      REXLOG_INFO("[ac6-save-confirm] edge=0x{:04X} type={} response={}", edge,
                  dialog_type, response);
    }
  }

}

  if (!REXCVAR_GET(ac6_log_ui_dispatch) || !screen) return;

  struct Snapshot {
    uint32_t state;
    uint32_t selector_state;
    uint32_t response;
    uint32_t dialog_type;
    uint32_t result;
    uint32_t inner_state;
    uint32_t outer_state;
    uint32_t mode;
    uint32_t storage_mode;
    uint32_t slot;
  };
  const Snapshot now = {
      rex::memory::load_and_swap<uint32_t>(base + screen + 40),
      rex::memory::load_and_swap<uint32_t>(base + screen + 44),
      rex::memory::load_and_swap<uint32_t>(base + screen + 12),
      rex::memory::load_and_swap<uint32_t>(base + screen + 28),
      rex::memory::load_and_swap<uint32_t>(base + screen + 36),
      rex::memory::load_and_swap<uint32_t>(base + screen + 68),
      rex::memory::load_and_swap<uint32_t>(base + screen + 72),
      rex::memory::load_and_swap<uint32_t>(base + screen + 388),
      rex::memory::load_and_swap<uint32_t>(base + screen + 392),
      rex::memory::load_and_swap<uint32_t>(base + screen + 396),
  };
  static std::mutex mutex;
  static bool primed = false;
  static uint32_t last_screen = 0;
  static Snapshot last = {};
  std::lock_guard<std::mutex> lock(mutex);
  if (!primed || screen != last_screen ||
      now.state != last.state || now.selector_state != last.selector_state ||
      now.response != last.response ||
      now.dialog_type != last.dialog_type || now.result != last.result ||
      now.inner_state != last.inner_state || now.outer_state != last.outer_state ||
      now.mode != last.mode || now.storage_mode != last.storage_mode || now.slot != last.slot) {
    REXLOG_INFO("[ac6-save-outer] screen=0x{:08X} state40={} selector44={} response12={} type28={} "
                "result36={} inner68={} outer72={} mode388={} mode392={} slot396={}",
                screen, now.state, now.selector_state, now.response,
                now.dialog_type, now.result,
                now.inner_state, now.outer_state, now.mode, now.storage_mode, now.slot);
    last = now;
    last_screen = screen;
    primed = true;
  }
}

// sub_821CFE18 is the exact predicate above dialog type 4. It feeds the user
// index stored in the save object to XamUserGetSigninState and returns zero for
// an invalid index or a signed-out user. Recording this contract distinguishes
// a host sign-in mismatch from an input edge that auto-accepts the warning.
PPC_FUNC_IMPL(sub_821CFE18) {
  PPC_FUNC_PROLOGUE();
  const uint32_t object = ctx.r3.u32;
  const uint32_t user_index = object
      ? rex::memory::load_and_swap<uint32_t>(base + object + 36)
      : 0xFFFFFFFFu;
  __imp__sub_821CFE18(ctx, base);
  if (REXCVAR_GET(ac6_log_ui_dispatch)) {
    static std::atomic<uint32_t> lines{0};
    if (lines.fetch_add(1, std::memory_order_relaxed) < 32) {
      REXLOG_INFO("[ac6-save-signin] object=0x{:08X} user_index={} predicate={}",
                  object, int32_t(user_index), int32_t(ctx.r3.u32));
    }
  }
}

// ---------------------------------------------------------------------------
// The confirm binding lookup, and why it never fires.
//
// sub_821CA908 zeroes a four-element table of 32 slots at 0x826EDBA0 (stride
// 160), calls sub_821CAA50 to populate it, then -- only if the byte flag at
// [this+25] is non-zero -- scans the table ANDing the mask at [this+28] against
// each entry. A hit calls sub_821CB5F0 and consumes the request by clearing
// both fields.
//
// Measured with the store watchpoint: on an A press the body runs (138 stores)
// and the populator writes the correct canonical code 0x20, but not one store
// ever carries lr=0x821CA9C4, the return address of the sub_821CB5F0 call. So
// the match path is never taken and the request is never consumed, which is why
// the same lookup repeats every frame.
//
// Two candidates remain and the store data cannot separate them: either the
// guard flag at [this+25] is zero so the scan never starts, or the scan runs
// with a mask that matches nothing. These three wrappers read the fields at the
// exact points that distinguish them -- on entry, after the populator returns
// (which is the guard's own value), and at the action itself.
// ---------------------------------------------------------------------------
namespace {
bool ConfirmProbeEnabled() {
  return REXCVAR_GET(ac6_log_ui_dispatch) && g_binding_probe_active_edge != 0;
}
}  // namespace

PPC_FUNC_IMPL(sub_821CA908) {
  PPC_FUNC_PROLOGUE();
  const uint32_t self = ctx.r3.u32;
  const uint32_t edge = g_binding_probe_edge.exchange(0, std::memory_order_relaxed);
  g_binding_probe_active_edge = edge;
  uint8_t flag =
      self ? rex::memory::load_and_swap<uint8_t>(base + self + 25) : 0;
  uint32_t mask =
      self ? rex::memory::load_and_swap<uint32_t>(base + self + 28) : 0;
  if (self && ConfirmProbeEnabled()) {
    REXLOG_INFO("[ac6-confirm] edge=0x{:04X} enter this=0x{:08X} flag[+25]={} "
                "mask[+28]=0x{:08X}", edge, self, flag, mask);
  }
  __imp__sub_821CA908(ctx, base);
  g_binding_probe_active_edge = 0;
}

PPC_FUNC_IMPL(sub_821CAA50) {
  PPC_FUNC_PROLOGUE();
  const uint32_t self = ctx.r3.u32;
  __imp__sub_821CAA50(ctx, base);
  // This is the guard's value: sub_821CA908 reads [this+25] on the instruction
  // after this call returns.
  const uint8_t flag =
      self ? rex::memory::load_and_swap<uint8_t>(base + self + 25) : 0;
  const uint32_t mask =
      self ? rex::memory::load_and_swap<uint32_t>(base + self + 28) : 0;
  const uint32_t slot0 = rex::memory::load_and_swap<uint32_t>(base + 0x826EDBA0u + 0 * 160);
  const uint32_t slot1 = rex::memory::load_and_swap<uint32_t>(base + 0x826EDBA0u + 1 * 160);
  const uint32_t slot2 = rex::memory::load_and_swap<uint32_t>(base + 0x826EDBA0u + 2 * 160);
  const uint32_t slot3 = rex::memory::load_and_swap<uint32_t>(base + 0x826EDBA0u + 3 * 160);
  if (self && ConfirmProbeEnabled()) {
    REXLOG_INFO("[ac6-confirm] edge=0x{:04X} populated this=0x{:08X} flag[+25]={} "
                "mask[+28]=0x{:08X} slots={:08X},{:08X},{:08X},{:08X} matches={}",
                g_binding_probe_active_edge, self, flag, mask, slot0, slot1, slot2, slot3,
                ((slot0 | slot1 | slot2 | slot3) & mask) != 0);
  }
}

PPC_FUNC_IMPL(sub_821CB5F0) {
  PPC_FUNC_PROLOGUE();
  // The action the match path calls. If this never logs, the binding never
  // resolved; if it logs and the dialog still does not move, the fault is
  // downstream of the binding entirely.
  if (ConfirmProbeEnabled()) {
    REXLOG_WARN("[ac6-confirm] edge=0x{:04X} ACTION sub_821CB5F0 fired this=0x{:08X}",
                g_binding_probe_active_edge, ctx.r3.u32);
  }
  __imp__sub_821CB5F0(ctx, base);
}

// The save browser enters its error-dialog state immediately after this
// asynchronous content-list poll returns true. Record the producer's status,
// not merely the later UI state, so an empty list can be distinguished from a
// failed list operation without changing guest behavior.
PPC_FUNC_IMPL(sub_821C59B0) {
  PPC_FUNC_PROLOGUE();
  const uint32_t self = ctx.r3.u32;
  __imp__sub_821C59B0(ctx, base);

  static std::atomic<uint32_t> lines{0};
  if (REXCVAR_GET(ac6_log_ui_dispatch) && self &&
      lines.fetch_add(1, std::memory_order_relaxed) < 80) {
    const uint32_t task = rex::memory::load_and_swap<uint32_t>(base + self + 8);
    const uint32_t completion =
        task ? rex::memory::load_and_swap<uint32_t>(base + task + 8) : 0;
    REXLOG_INFO(
        "[ac6-save-poll] ret={} self=0x{:08X} ui_state={} result={} dialog_type={} "
        "mode388={} mode392={} slot396={} task=0x{:08X} task_state={} task_result={} "
        "completion=0x{:08X}",
        ctx.r3.u32, self, rex::memory::load_and_swap<uint32_t>(base + self + 68),
        int32_t(rex::memory::load_and_swap<uint32_t>(base + self + 36)),
        int32_t(rex::memory::load_and_swap<uint32_t>(base + self + 28)),
        int32_t(rex::memory::load_and_swap<uint32_t>(base + self + 388)),
        int32_t(rex::memory::load_and_swap<uint32_t>(base + self + 392)),
        int32_t(rex::memory::load_and_swap<uint32_t>(base + self + 396)), task,
        task ? int32_t(rex::memory::load_and_swap<uint32_t>(base + task + 4)) : 0,
        task ? int32_t(rex::memory::load_and_swap<uint32_t>(base + task + 24996)) : 0,
        completion);
  }
}

#ifdef AC6RECOMP_PROBE_GUEST_TEXT

// sub_820F8608 is the guest text renderer. It preserves its r5 string argument
// in r27, then calls the font object's vtable getter at 0x820F8698. The broader
// entry probe observed LOAD_W_003 and M70000_222 still live in r5 at that
// getter, proving that resource-shaped names are reaching this renderer. This
// strong wrapper replaces only the generated weak alias and records the exact
// caller responsible for each printable string.
PPC_FUNC_IMPL(sub_820F8608) {
  PPC_FUNC_PROLOGUE();
  ac6::dialog_text::ApplyFallback(ctx, base);
  static std::atomic<uint32_t> lines{0};
  constexpr uint32_t kMaxLines = 600;
  const uint32_t text = ctx.r5.u32;
  if (REXCVAR_GET(ac6_log_text_draws) && text &&
      lines.load(std::memory_order_relaxed) < kMaxLines) {
    char preview[97] = {};
    uint32_t length = 0;
    bool printable = true;
    bool has_underscore = false;
    for (; length < sizeof(preview) - 1; ++length) {
      const uint8_t value = base[text + length];
      if (value == 0) break;
      if (value < 0x20 || value > 0x7E) {
        printable = false;
        break;
      }
      preview[length] = static_cast<char>(value);
      has_underscore |= value == '_';
    }
    const bool target_family =
        has_underscore && (preview[0] == 'L' || preview[0] == 'M' || preview[0] == 'H');
    if (printable && target_family && length < sizeof(preview) - 1 &&
        lines.fetch_add(1, std::memory_order_relaxed) < kMaxLines) {
      REXLOG_WARN("[ac6-text-draw] caller=0x{:08X} renderer=0x820F8608 text_ptr=0x{:08X} "
                  "r3=0x{:08X} r4=0x{:08X} r6=0x{:08X} r7=0x{:08X} text=\"{}\"",
                  static_cast<uint32_t>(ctx.lr), text, ctx.r3.u32, ctx.r4.u32,
                  ctx.r6.u32, ctx.r7.u32, preview);
    }
  }
  __imp__sub_820F8608(ctx, base);
}

// Immediate dispatcher above the renderer: it receives the text in r7 and
// invokes vtable slot 48 of the r4 object. Its own lr therefore identifies the
// UI producer that failed to resolve the resource-shaped name.
PPC_FUNC_IMPL(sub_820D7C08) {
  PPC_FUNC_PROLOGUE();
  static std::atomic<uint32_t> lines{0};
  constexpr uint32_t kMaxLines = 300;
  const uint32_t text = ctx.r7.u32;
  if (REXCVAR_GET(ac6_log_text_draws) && text &&
      lines.load(std::memory_order_relaxed) < kMaxLines) {
    char preview[97] = {};
    uint32_t length = 0;
    bool printable = true;
    bool has_underscore = false;
    for (; length < sizeof(preview) - 1; ++length) {
      const uint8_t value = base[text + length];
      if (value == 0) break;
      if (value < 0x20 || value > 0x7E) {
        printable = false;
        break;
      }
      preview[length] = static_cast<char>(value);
      has_underscore |= value == '_';
    }
    const bool target_family =
        has_underscore && (preview[0] == 'L' || preview[0] == 'M' || preview[0] == 'H');
    if (printable && target_family && length < sizeof(preview) - 1 &&
        lines.fetch_add(1, std::memory_order_relaxed) < kMaxLines) {
      REXLOG_WARN("[ac6-text-dispatch] caller=0x{:08X} dispatcher=0x820D7C08 "
                  "text_ptr=0x{:08X} r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} "
                  "r6=0x{:08X} r8=0x{:08X} text=\"{}\"",
                  static_cast<uint32_t>(ctx.lr), text, ctx.r3.u32, ctx.r4.u32,
                  ctx.r5.u32, ctx.r6.u32, ctx.r8.u32, preview);
    }
  }
  __imp__sub_820D7C08(ctx, base);
}

#endif  // AC6RECOMP_PROBE_GUEST_TEXT
