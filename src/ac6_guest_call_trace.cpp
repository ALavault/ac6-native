/**
 * ReXGlue runtime - AC6 Recompilation project
 * Copyright (c) 2026 Tom Clay. All rights reserved.
 *
 * P2.2: a whole-program control-flow trace of the recompiled guest, armed by an
 * input edge.
 *
 * The question that has gone unanswered since cycle 421 is a control-flow one:
 * what does the guest do with the face-button mask on this screen, when it
 * handles the D-pad mask from the same X_INPUT_STATE correctly? Every cycle
 * since has asked data questions -- which word changed, at which offset -- and
 * twelve of them scanned memory for a field that two forced writes then proved
 * was not the selection at all.
 *
 * P1 and P2.1 closed that route for good: the screen object everyone modelled
 * sits idle at state 9, and its input context does not change on any press,
 * including the two that visibly move the highlight. There is no offset left to
 * guess at, because the object is not involved.
 *
 * This asks the control-flow question directly. The generated corpus is
 * ordinary C++ compiled by this project, so -finstrument-functions on those
 * translation units yields a callback at the entry of all 23 058 guest
 * functions. Recording them into a ring buffer, armed on the input edge already
 * detected in the pad wrapper, produces the execution path of a press. Diffing
 * a Right press against an A press names the function that consumes one and not
 * the other.
 *
 * Design notes, each of which is load-bearing:
 *
 * - The buffer holds raw host function pointers, not symbol names. Resolving
 *   them in-process would need -rdynamic and dladdr, and would run inside the
 *   frame loop; instead the addresses are dumped verbatim and resolved offline
 *   against the binary's symbol table with nm. Cheap in the hot path, and the
 *   resolution is exact rather than nearest-symbol.
 *
 * - Only entries are recorded. Exits double the volume and add nothing: the
 *   question is which functions run, and in what order.
 *
 * - Disarmed, the hot path is one relaxed load and a predicted-not-taken
 *   branch. Instrumenting 23 058 functions means this runs on every guest call
 *   in the program, so anything heavier would change the timing of the frames
 *   being measured.
 *
 * - Everything here is no_instrument_function. Without it the callbacks
 *   instrument themselves and the first guest call never returns.
 */

#include "ac6_guest_call_trace.h"

// The header supplies inline no-op stubs when tracing is compiled out, so the
// real definitions must not also exist in that configuration -- otherwise every
// build without AC6RECOMP_TRACE_GUEST_CALLS fails on redefinition. Only the
// dedicated instrumented build defines it.
#ifdef AC6RECOMP_TRACE_GUEST_CALLS

#include <atomic>
#include <cstdint>
#include <cstdio>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(ac6_trace_guest_calls, false, "AC6/Trace",
                    "Capture a guest control-flow trace on the next input edge and write it to "
                    "ac6-trace-<mask>-<sequence>.bin for offline symbol resolution.");
REXCVAR_DEFINE_INT32(ac6_trace_entries, 400000, "AC6/Trace",
                     "How many guest function entries to record per armed capture.");
REXCVAR_DEFINE_INT32(ac6_trace_edge_mask, 0, "AC6/Trace",
                     "Only arm the trace on edges matching this raw button mask (0 = any). "
                     "0x0008 is D-pad Right, 0x1000 is A -- the two presses the differential "
                     "compares.");

extern "C" void __cyg_profile_func_enter(void*, void*);

namespace ac6::trace {
namespace {

// 400k entries x 8 bytes = 3.2 MB by default. Sized so a capture spans several
// frames of a title that issues tens of thousands of guest calls per frame,
// without the dump itself becoming a stall.
constexpr uint32_t kMaxEntries = 1u << 21;  // 2M hard ceiling

void** g_buffer = nullptr;
void** g_callsite_buffer = nullptr;
std::atomic<uint32_t> g_write_index{0};
std::atomic<bool> g_armed{false};
std::atomic<uint32_t> g_capacity{0};
uint32_t g_pending_mask = 0;
uint32_t g_capture_sequence = 0;

}  // namespace

__attribute__((no_instrument_function)) void Arm(uint32_t edge_mask) {
  if (!REXCVAR_GET(ac6_trace_guest_calls) || g_armed.load(std::memory_order_relaxed)) {
    return;
  }
  // Without a filter the first edge of the run wins, and the first edge is
  // always Start leaving the title -- so the capture is spent long before the
  // dialog exists, and because arming is one-shot until the buffer fills,
  // nothing later can re-arm. Select the press the differential is about.
  const uint32_t want = static_cast<uint32_t>(REXCVAR_GET(ac6_trace_edge_mask));
  if (want != 0 && (edge_mask & want) == 0) {
    return;
  }
  // A previous capture that has not been written yet must not be overwritten.
  if (g_pending_mask != 0) {
    return;
  }
  if (!g_buffer) {
    uint32_t want = static_cast<uint32_t>(REXCVAR_GET(ac6_trace_entries));
    if (want == 0 || want > kMaxEntries) {
      want = kMaxEntries;
    }
    g_buffer = new (std::nothrow) void*[want];
    g_callsite_buffer = new (std::nothrow) void*[want];
    if (!g_buffer || !g_callsite_buffer) {
      REXLOG_ERROR("[ac6-trace] could not allocate {} entries", want);
      delete[] g_buffer;
      delete[] g_callsite_buffer;
      g_buffer = nullptr;
      g_callsite_buffer = nullptr;
      return;
    }
    g_capacity.store(want, std::memory_order_relaxed);
  }
  g_pending_mask = edge_mask;
  g_write_index.store(0, std::memory_order_relaxed);
  g_armed.store(true, std::memory_order_release);
  // The executable is position-independent, so the pointers recorded here are
  // load-base + link address while nm reports link addresses. Without a
  // reference point every entry resolves to whatever symbol happens to sit
  // lowest, which is silent and total: the first attempt at this diff resolved
  // all 800 000 entries in both traces to "data_start" and reported the two
  // execution paths as identical. Publishing one known runtime address lets the
  // offline tool subtract the slide exactly.
  REXLOG_WARN("[ac6-trace] armed on edge 0x{:04X}, capturing {} entries, "
              "reference __cyg_profile_func_enter at runtime 0x{:016X}",
              edge_mask, g_capacity.load(std::memory_order_relaxed),
              reinterpret_cast<uintptr_t>(&__cyg_profile_func_enter));
}

__attribute__((no_instrument_function)) bool IsCapturing() {
  return g_armed.load(std::memory_order_relaxed);
}

__attribute__((no_instrument_function)) void FlushIfComplete() {
  if (g_armed.load(std::memory_order_acquire)) {
    return;  // still filling
  }
  const uint32_t count = g_write_index.load(std::memory_order_relaxed);
  if (!g_buffer || count == 0 || g_pending_mask == 0) {
    return;
  }
  char path[64];
  const uint32_t sequence = ++g_capture_sequence;
  std::snprintf(path, sizeof(path), "ac6-trace-%04X-%04u.bin", g_pending_mask,
                sequence);
  if (FILE* f = std::fopen(path, "wb")) {
    std::fwrite(g_buffer, sizeof(void*), count, f);
    std::fclose(f);
    REXLOG_WARN("[ac6-trace] wrote {} entries to {}", count, path);
  } else {
    REXLOG_ERROR("[ac6-trace] could not open {}", path);
  }
  char callers_path[80];
  std::snprintf(callers_path, sizeof(callers_path), "ac6-trace-%04X-%04u.callers.bin",
                g_pending_mask, sequence);
  if (FILE* f = std::fopen(callers_path, "wb")) {
    std::fwrite(g_callsite_buffer, sizeof(void*), count, f);
    std::fclose(f);
  } else {
    REXLOG_ERROR("[ac6-trace] could not open {}", callers_path);
  }
  g_pending_mask = 0;
  g_write_index.store(0, std::memory_order_relaxed);
}

}  // namespace ac6::trace

extern "C" {

// Called at the entry of every function in the instrumented translation units,
// which is the whole generated corpus and nothing else.
__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void* this_fn,
                                                                     void* call_site) {
  if (!ac6::trace::g_armed.load(std::memory_order_relaxed)) {
    return;
  }
  const uint32_t index = ac6::trace::g_write_index.fetch_add(1, std::memory_order_relaxed);
  const uint32_t capacity = ac6::trace::g_capacity.load(std::memory_order_relaxed);
  if (index >= capacity) {
    // Full: stop recording and let the frame loop notice and write the file.
    // Disarming here rather than wrapping keeps the capture a contiguous window
    // starting at the press, which is what makes two captures comparable.
    ac6::trace::g_armed.store(false, std::memory_order_release);
    ac6::trace::g_write_index.store(capacity, std::memory_order_relaxed);
    return;
  }
  ac6::trace::g_buffer[index] = this_fn;
  ac6::trace::g_callsite_buffer[index] = call_site;
}

__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void* this_fn,
                                                                    void* call_site) {
  (void)this_fn;
  (void)call_site;
}

}  // extern "C"

#endif  // AC6RECOMP_TRACE_GUEST_CALLS
