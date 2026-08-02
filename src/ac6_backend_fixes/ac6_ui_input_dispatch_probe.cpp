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
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>
#include <utility>
#include <cstdint>
#include <map>
#include <mutex>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/ppc/function.h>

#include "../ac6_guest_call_trace.h"
#include "../ac6_ppc_store_watch.h"

REXCVAR_DEFINE_BOOL(ac6_log_ui_dispatch, false, "AC6/Input",
                    "Log the UI input dispatcher's object pointer and r11 branch.");
REXCVAR_DEFINE_INT32(ac6_force_screen_state, -1, "AC6/Input",
                     "P1: on each A press, force the screen's state word at [screen+68] to this "
                     "value (-1 = off). Bounds the problem by driving the transition the confirm "
                     "button fails to drive.");

PPC_EXTERN_FUNC(__imp__rex_sub_8234D510);
PPC_EXTERN_FUNC(__imp__rex_sub_8234D210);
PPC_EXTERN_FUNC(__imp__rex_sub_821C56F8);
PPC_EXTERN_FUNC(__imp__sub_821CA908);
PPC_EXTERN_FUNC(__imp__sub_821CAA50);
PPC_EXTERN_FUNC(__imp__sub_821CB5F0);

void Ac6ScanGuestStrings(uint8_t* base);

namespace {

// The raw mask of the most recent press edge, published by the edge detector
// and consumed by the screen update on its next call. 0 means "nothing new".
std::atomic<uint32_t> g_last_edge{0};

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

PPC_FUNC_IMPL(rex_sub_8234D510) {
  PPC_FUNC_PROLOGUE();

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
      // Arm the control-flow trace here rather than in the screen update: this
      // is the earliest point at which the guest itself has decided a press
      // happened, so the capture window starts before any consumer of the press
      // has run. Arming a frame later would miss the very functions the diff is
      // looking for.
      ac6::trace::Arm(edge);
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
  __imp__rex_sub_821C56F8(ctx, base);

  // Runs on the screen update because that is a reliable once-per-frame point
  // outside the trace callback itself. Writing the file from inside the
  // callback would recurse and would stall the frame that is being captured.
  ac6::trace::FlushIfComplete();
  ac6::stores::FlushIfComplete();

  // Only meaningful once this screen exists, which is the point at which the
  // dialog's strings would have had to be resolved.
  if (screen) {
    Ac6ScanGuestStrings(base);
  }

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
    const uint32_t edge = g_last_edge.exchange(0, std::memory_order_relaxed);
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
std::atomic<uint32_t> g_confirm_probe_lines{0};
constexpr uint32_t kConfirmProbeMaxLines = 120;

bool ConfirmProbeShouldLog() {
  if (!REXCVAR_GET(ac6_log_ui_dispatch)) return false;
  return g_confirm_probe_lines.fetch_add(1, std::memory_order_relaxed) < kConfirmProbeMaxLines;
}
}  // namespace

PPC_FUNC_IMPL(sub_821CA908) {
  PPC_FUNC_PROLOGUE();
  const uint32_t self = ctx.r3.u32;
  if (self && ConfirmProbeShouldLog()) {
    REXLOG_INFO("[ac6-confirm] enter sub_821CA908 this=0x{:08X} flag[+25]={} mask[+28]=0x{:08X}", self,
                rex::memory::load_and_swap<uint8_t>(base + self + 25),
                rex::memory::load_and_swap<uint32_t>(base + self + 28));
  }
  __imp__sub_821CA908(ctx, base);
}

PPC_FUNC_IMPL(sub_821CAA50) {
  PPC_FUNC_PROLOGUE();
  const uint32_t self = ctx.r3.u32;
  __imp__sub_821CAA50(ctx, base);
  // This is the guard's value: sub_821CA908 reads [this+25] on the instruction
  // after this call returns.
  if (self && ConfirmProbeShouldLog()) {
    REXLOG_INFO("[ac6-confirm] after populator this=0x{:08X} flag[+25]={} mask[+28]=0x{:08X} "
                "slot0=0x{:08X}",
                self, rex::memory::load_and_swap<uint8_t>(base + self + 25),
                rex::memory::load_and_swap<uint32_t>(base + self + 28),
                rex::memory::load_and_swap<uint32_t>(base + 0x826EDBA0u));
  }
}

PPC_FUNC_IMPL(sub_821CB5F0) {
  PPC_FUNC_PROLOGUE();
  // The action the match path calls. If this never logs, the binding never
  // resolved; if it logs and the dialog still does not move, the fault is
  // downstream of the binding entirely.
  if (ConfirmProbeShouldLog()) {
    REXLOG_WARN("[ac6-confirm] ACTION sub_821CB5F0 fired this=0x{:08X}", ctx.r3.u32);
  }
  __imp__sub_821CB5F0(ctx, base);
}

// ---------------------------------------------------------------------------
// Where do the dialog's strings come from?
//
// The modal renders with fallback YES / NO labels and, for a moment, the font's
// own character inventory as its body text -- while "PLEASE WAIT" renders
// perfectly in the same frame. So the glyph path is sound and the dialog is
// simply handed the wrong text. Under Xenia the same modal reads "A gamer
// profile has not been selected. ... Continue anyway?" with the buttons
// "Select a gamer profile." and "Continue anyway.", and upstream issue #11
// confirms those labels work there.
//
// Neither the strings nor their English source exist in the XEX -- searched as
// ASCII, UTF-16LE and UTF-16BE -- so they arrive from game data, which means
// there are exactly two possibilities and they need different fixes:
//
//   * the strings ARE in guest memory and the dialog does not use them, so the
//     defect is in selection or binding;
//   * the strings are NOT there, so the resource carrying them never loaded or
//     never parsed, and the dialog is correctly falling back.
//
// Searching guest memory for them answers that directly. Scanned ranges are the
// two proven safe by earlier probes: the image and its static data, and the
// heap where this screen's objects live. Needles are checked as plain ASCII and
// as UTF-16 in both byte orders, because a title that renders wide text may
// hold either.
// ---------------------------------------------------------------------------
REXCVAR_DEFINE_STRING(ac6_find_strings, "", "AC6/Input",
                      "Comma-separated needles to search for in guest memory once the dialog "
                      "screen is up. Empty disables the scan.");

namespace {

// The guest window is 4 GB of reserved address space of which only parts are
// mapped, and hardcoding "the ranges that matter" got the previous version of
// this scan a clean NOT FOUND for "PLEASE WAIT" and "YES" -- both of which were
// on screen at that moment. A scan that cannot find text it is looking at is
// reporting on itself, so the ranges are no longer guessed: /proc/self/maps is
// the authority on what is readable, and every readable mapping inside the
// guest window is searched.
struct HostRange {
  const uint8_t* begin;
  const uint8_t* end;
};

std::vector<HostRange> ReadableGuestMappings(const uint8_t* base) {
  std::vector<HostRange> out;
  const uintptr_t lo = reinterpret_cast<uintptr_t>(base);
  const uintptr_t hi = lo + (uintptr_t(4) << 30);  // the 4 GB guest window
  std::ifstream maps("/proc/self/maps");
  std::string line;
  while (std::getline(maps, line)) {
    uintptr_t a = 0, b = 0;
    char perms[8] = {};
    if (std::sscanf(line.c_str(), "%lx-%lx %7s", &a, &b, perms) != 3) continue;
    if (perms[0] != 'r') continue;
    if (b <= lo || a >= hi) continue;
    out.push_back({reinterpret_cast<const uint8_t*>(std::max(a, lo)),
                   reinterpret_cast<const uint8_t*>(std::min(b, hi))});
  }
  return out;
}

void ScanForNeedle(uint8_t* base, const std::string& needle,
                   const std::vector<HostRange>& ranges) {
  std::string ascii = needle;
  std::string wide_le, wide_be;
  for (char c : needle) {
    wide_le.push_back(c);
    wide_le.push_back('\0');
    wide_be.push_back('\0');
    wide_be.push_back(c);
  }
  const std::pair<const std::string*, const char*> forms[] = {
      {&ascii, "ascii"}, {&wide_le, "utf16le"}, {&wide_be, "utf16be"}};

  uint32_t total = 0;
  for (const HostRange& range : ranges) {
    for (const auto& [form, form_name] : forms) {
      const uint8_t* at = range.begin;
      uint32_t hits = 0;
      while (at < range.end && hits < 3 && total < 24) {
        const void* found = memmem(at, size_t(range.end - at), form->data(), form->size());
        if (!found) break;
        const uintptr_t off =
            reinterpret_cast<uintptr_t>(found) - reinterpret_cast<uintptr_t>(base);
        REXLOG_WARN("[ac6-strings] \"{}\" as {} at guest 0x{:08X}", needle, form_name,
                    uint32_t(off));
        ++hits;
        ++total;
        at = static_cast<const uint8_t*>(found) + 1;
      }
    }
  }
  if (total == 0) {
    REXLOG_WARN("[ac6-strings] \"{}\" NOT FOUND in any readable guest mapping", needle);
  }
}

}  // namespace

void Ac6ScanGuestStrings(uint8_t* base) {
  const std::string spec = REXCVAR_GET(ac6_find_strings);
  if (spec.empty()) {
    return;
  }
  static std::atomic<bool> done{false};
  if (done.exchange(true)) {
    return;
  }
  const std::vector<HostRange> ranges = ReadableGuestMappings(base);
  size_t bytes = 0;
  for (const HostRange& r : ranges) bytes += size_t(r.end - r.begin);
  REXLOG_WARN("[ac6-strings] scanning {} readable mappings ({} MB) for: {}", ranges.size(),
              bytes >> 20, spec);
  size_t start = 0;
  while (start <= spec.size()) {
    const size_t comma = spec.find(',', start);
    const std::string needle = spec.substr(start, comma - start);
    if (!needle.empty()) {
      ScanForNeedle(base, needle, ranges);
    }
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  REXLOG_WARN("[ac6-strings] scan complete");
}
