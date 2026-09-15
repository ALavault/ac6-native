#!/usr/bin/env python3
"""Materialize offline Xenon import definitions in an ignored build tree."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


IMPORT = re.compile(r"__imp__([A-Za-z0-9_]+)")
HEADER = """// Generated build-only import boundary; never install or track this file.
#include \"ppc_context.h\"
#include \"ac6/native_guest_input.h\"
#include \"ac6/native_guest_media.h\"
#include \"ac6/native_guest_vd.h\"
#include \"ac6/native_runtime.h\"
#include \"ac6/native_guest_threads.h\"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <string>
#include <string_view>
#include <thread>
#include <poll.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace {
constexpr std::uint64_t kOfflineStatus = 0xC00000BBull;

// Bounded diagnostic for the generic offline-stub fallback: which imports
// this build has no specific handling for are actually reached at runtime.
// Mirrors AC6_NATIVE_VD_TRACE's getenv-per-call pattern (native_guest_vd.cpp).
void trace_offline_import(const char* name) noexcept {
  const char* enabled = std::getenv("AC6_NATIVE_IMPORT_TRACE");
  if (enabled == nullptr || std::strcmp(enabled, "1") != 0) return;
  std::fprintf(stderr, "[offline-import] %s\\n", name);
}
// r129: strips a leading drive-letter-style prefix (e.g. "game:") the
// way this XEX's own DATA00.PAC path uses, byte-confirmed on the real
// title, and normalizes separators, leaving a path relative to
// whatever media the runtime was booted against.
std::string guest_path_to_relative(std::string_view raw) {
  std::string path(raw);
  const std::size_t colon = path.find(':');
  if (colon != std::string::npos) path.erase(0, colon + 1);
  while (!path.empty() && (path.front() == '\\\\' || path.front() == '/')) {
    path.erase(0, 1);
  }
  for (char& c : path) {
    if (c == '\\\\') c = '/';
  }
  return path;
}

std::atomic<std::uint32_t> g_next_handle{0x100u};
std::atomic<std::uint32_t> g_next_thread_stack{0x8ef00000u};
// Keep kernel virtual allocations in a deterministic non-image/non-stack
// range. The guest address space is already a complete 32-bit reservation.
std::atomic<std::uint32_t> g_next_virtual{0x10000000u};

struct EventState {
  bool signaled{};
  bool manual_reset{};
  // r285: an auto-reset event's outstanding, not-yet-claimed signals. Two
  // threads legitimately waiting on the same auto-reset key (a real,
  // observed shape -- r284 traced the retail game's own generic "kick and
  // wait for target" utility used concurrently by two threads on one
  // shared handle pair) used to race on a single `signaled` bool: whichever
  // thread reacquired g_event_mutex first consumed it, and the other saw
  // `signaled == false` and reported a false STATUS_TIMEOUT for a signal
  // that had, in fact, just fired. Counting releases instead of a single
  // flag means every set_event() call is honored by exactly one wait_event()
  // call, in whatever order they reacquire the lock -- never zero, never
  // more than one per signal, and never a spurious timeout for a signal
  // that already happened.
  std::uint64_t pending_releases{};
  // r499: per-key condvar. One process-wide cv made every set_event wake
  // every parked waiter regardless of key (12.7M set_event calls per 260 s
  // probe window against ~20 parked threads = a ~1M wake/s futex storm the
  // sampler measured as roughly half of the main thread's late-run
  // samples inside __lll_lock_wake_private, pacing the frame loop at
  // ~0.35 fps). Each key now waits on its own cv, so a signal wakes only
  // that key's waiters. shared_ptr storage keeps the cv alive for waiters
  // that copied their entry while another thread erases the key.
  std::condition_variable cv;
};
std::mutex g_event_mutex;
std::unordered_map<std::uint32_t, std::shared_ptr<EventState>> g_events;

// r116: RtlEnterCriticalSection/RtlLeaveCriticalSection were no-ops under
// the (now-disproven -- r111-r115 traced eighteen real concurrent host
// threads spawned via ExCreateThread) assumption of a single guest
// thread. Keyed by the guest RTL_CRITICAL_SECTION object's own address,
// same pattern as g_events keyed by handle. std::recursive_mutex because
// the real API allows the same thread to re-enter a critical section it
// already holds.
std::mutex g_critical_sections_mutex;
std::unordered_map<std::uint32_t, std::unique_ptr<std::recursive_mutex>>
    g_critical_sections;

std::recursive_mutex& critical_section_for(std::uint32_t key) {
  std::lock_guard lock(g_critical_sections_mutex);
  auto it = g_critical_sections.find(key);
  if (it == g_critical_sections.end()) {
    it = g_critical_sections
             .emplace(key, std::make_unique<std::recursive_mutex>())
             .first;
  }
  return *it->second;
}

// r282 (diagnostic only): a low-frequency, non-flooding view of which
// critical sections are currently held and by which host thread -- tests
// the hypothesis that a wait stub throwing GuestThreadTerminated while a
// section is held leaks it locked forever (the raw lock()/unlock() pair
// above has no RAII across that unwind, so a section left locked when its
// holder's stack unwinds through the entry lambda's catch is never
// unlocked). A per-call trace (tried first) produced over 100k
// lines/second and an unrelated host heap corruption abort before any
// useful signal appeared; this heartbeat replaces it.
std::mutex g_held_sections_mutex;
std::unordered_map<std::uint32_t, std::uint64_t> g_held_sections;

void start_held_sections_heartbeat_once() {
  static std::once_flag once;
  std::call_once(once, [] {
    std::thread([] {
      for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::lock_guard lock(g_held_sections_mutex);
        if (g_held_sections.empty()) continue;
        std::fprintf(stderr, "[held-critical-sections] count=%zu:",
                     g_held_sections.size());
        for (const auto& entry : g_held_sections) {
          std::fprintf(stderr, " key=0x%08x tid=%llu", entry.first,
                       static_cast<unsigned long long>(entry.second));
        }
        std::fprintf(stderr, "\\n");
      }
    }).detach();
  });
}

// r191: Kf/KeAcquireSpinLock*/Kf/KeReleaseSpinLock* were no-ops -- the
// same real-concurrency risk r116 already fixed for critical sections
// (r111-r115's confirmed real concurrent host threads), but for a
// pervasively-used primitive: this XEX's own real call sites number in
// the dozens to low hundreds per function, spanning most of the engine's
// address range. Keyed by the guest KSPIN_LOCK object's own address, same
// technique as critical_section_for. Plain std::mutex, not recursive: a
// real spinlock is not re-entrant either (self-reacquisition deadlocks on
// real hardware too).
std::mutex g_spin_locks_mutex;
std::unordered_map<std::uint32_t, std::unique_ptr<std::mutex>> g_spin_locks;

std::mutex& spin_lock_for(std::uint32_t key) {
  std::lock_guard lock(g_spin_locks_mutex);
  auto it = g_spin_locks.find(key);
  if (it == g_spin_locks.end()) {
    it = g_spin_locks.emplace(key, std::make_unique<std::mutex>()).first;
  }
  return *it->second;
}

// r499: resolves (or creates) the shared EventState for a key under the
// registry mutex. g_events stores shared_ptr so a waiter can keep the
// entry (and its per-key condvar) alive across a concurrent erase.
std::shared_ptr<EventState> event_state_for(std::uint32_t key) {
  auto it = g_events.find(key);
  if (it != g_events.end()) return it->second;
  auto state = std::make_shared<EventState>();
  g_events.emplace(key, state);
  return state;
}

void create_event(std::uint32_t key, bool manual_reset, bool signaled) {
  if (key == 0u) return;
  std::shared_ptr<EventState> state;
  {
    std::lock_guard lock(g_event_mutex);
    state = std::make_shared<EventState>();
    state->signaled = signaled;
    state->manual_reset = manual_reset;
    g_events[key] = state;
  }
  // No waiters can exist for a key created this instant; kept for
  // symmetry with the level-triggered contract.
  if (signaled) state->cv.notify_all();
}

bool set_event(std::uint32_t key) {
  if (key == 0u) return false;
  bool previous;
  bool manual;
  std::shared_ptr<EventState> state;
  {
    std::lock_guard lock(g_event_mutex);
    state = event_state_for(key);
    previous = state->signaled;
    state->signaled = true;
    // r285: one more release available for an auto-reset key -- see
    // EventState::pending_releases. A manual-reset event stays level-
    // triggered (every waiter sees it, none consume it), so it never
    // accrues pending releases.
    manual = state->manual_reset;
    if (!manual) ++state->pending_releases;
  }
  // r499: wake discipline. set_event fires ~12.7M times per 260 s probe
  // window on this title; with ~20 threads parked in blocking waits,
  // notify_all per signal storms every waiter with a futex wake + a
  // predicate re-check it loses (the sampler measured the MAIN thread
  // spending roughly half its late-run samples inside __lll_lock_wake_
  // private, and the frame pace degrading to ~0.35 fps). An auto-reset
  // key can satisfy exactly one waiter (one pending release), so
  // notify_one is the correct and sufficient wake; a manual-reset key
  // is level-triggered and must still wake everyone. With the r499
  // per-key condvar the wake reaches only this key's waiters either
  // way.
  if (manual) {
    state->cv.notify_all();
  } else {
    state->cv.notify_one();
  }
  return previous;
}

bool clear_event(std::uint32_t key) {
  if (key == 0u) return false;
  std::shared_ptr<EventState> state;
  {
    std::lock_guard lock(g_event_mutex);
    state = event_state_for(key);
  }
  // r499: a clear ends the level-triggered signal; wake this key's
  // waiters so their predicates re-evaluate against the cleared state
  // (the notify runs without the registry mutex, standard cv practice).
  const bool previous = state->signaled;
  {
    std::lock_guard lock(g_event_mutex);
    state->signaled = false;
    state->pending_releases = 0;  // r285: an explicit clear discards credit
  }
  state->cv.notify_all();
  return previous;
}

// A guest caller not yet signaled is expected to retry (per the offline,
// single-guest-process contract: nothing here blocks the host indefinitely).
// Blocking briefly on a condition variable instead of returning immediately
// keeps that same retry contract -- STATUS_TIMEOUT on an unsignaled wait --
// while letting an actual signal (set_event) wake the waiter promptly
// instead of every caller busy-spinning the retry as fast as the host can
// re-issue it. r91 measured that spin at ~103,000 futex ops/sec across the
// worker threads r90's fix unblocked; this bounds it without changing the
// observable single-shot return contract any existing caller depends on.
//
// r285: r91's single `signaled` bool is correct with exactly one waiter,
// but r284 traced two real, concurrent waiters on one auto-reset handle
// (the retail game's own generic "kick and wait for target" utility, used
// by two threads at once) racing to reacquire g_event_mutex after a shared
// notify_all() -- the loser saw `signaled == false` (the winner had already
// reset it) and reported a false timeout for a signal that DID fire. This
// waits for a `pending_releases` credit instead of a one-shot flag: every
// set_event() adds one, every successful wait_event() consumes one, so
// N signals correctly satisfy N waiters (in whatever order they reacquire
// the lock) instead of at most one of them, with the rest starved by a
// race they did not need to lose. Still bounded to a 2 ms attempt, same
// as before: this does not change the "never blocks indefinitely"
// contract, only who gets credited when more than one thread is waiting.
// r289 (diagnostic only): r288 showed neither wait_event() nor
// wait_mutant() ever exceeds its own ~2 ms bound (exactly 2 anomalies in
// a clean 75 s window, both single-digit ms over the threshold) -- so a
// multi-second stall is not a single call running long. It could still be
// the GUEST not calling at all for a second or more (busy elsewhere, or
// blocked on something unrelated), which this cannot distinguish from
// "calling constantly but always resolving fast" without a call-rate
// signal. A low-frequency heartbeat (same technique as r282's
// g_held_sections, not a per-call trace) reports the running call count
// once a second: a stall shows as the same count printed back-to-back;
// continuous polling shows it climbing every tick.
std::atomic<std::uint64_t> g_wait_event_calls{0};
std::atomic<std::uint64_t> g_wait_mutant_calls{0};
// r289 (same cycle): the aggregate counts above turned out to be
// swamped by hundreds of thousands of calls/sec from callers with no
// relation to the frame handshake (some other spin-polling site
// elsewhere in the engine) -- the aggregate rate never dipped even once
// across a 75 s window, which is not informative about the two specific
// threads r283-r287 have been chasing. These two counters are scoped to
// the exact handles r285/r286 already confirmed belong to that
// handshake's shared gate object: 0x121 (manual-reset event) and 0x120
// (Mutant).
std::atomic<std::uint64_t> g_wait_event_calls_gate121{0};
std::atomic<std::uint64_t> g_wait_mutant_calls_gate120{0};

void start_wait_call_heartbeat_once() {
  static std::once_flag once;
  std::call_once(once, [] {
    std::thread([] {
      std::uint64_t last_event = 0;
      std::uint64_t last_mutant = 0;
      std::uint64_t last_gate121 = 0;
      std::uint64_t last_gate120 = 0;
      for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        const std::uint64_t event_calls = g_wait_event_calls.load();
        const std::uint64_t mutant_calls = g_wait_mutant_calls.load();
        const std::uint64_t gate121_calls = g_wait_event_calls_gate121.load();
        const std::uint64_t gate120_calls = g_wait_mutant_calls_gate120.load();
        std::fprintf(stderr,
                     "[wait-call-heartbeat] wait_event_calls=%llu "
                     "(+%llu) wait_mutant_calls=%llu (+%llu) "
                     "gate121_calls=%llu (+%llu) gate120_calls=%llu (+%llu)\\n",
                     static_cast<unsigned long long>(event_calls),
                     static_cast<unsigned long long>(event_calls - last_event),
                     static_cast<unsigned long long>(mutant_calls),
                     static_cast<unsigned long long>(mutant_calls - last_mutant),
                     static_cast<unsigned long long>(gate121_calls),
                     static_cast<unsigned long long>(gate121_calls - last_gate121),
                     static_cast<unsigned long long>(gate120_calls),
                     static_cast<unsigned long long>(gate120_calls - last_gate120));
        last_event = event_calls;
        last_mutant = mutant_calls;
        last_gate121 = gate121_calls;
        last_gate120 = gate120_calls;
      }
    }).detach();
  });
}

// r530: block-trace arming -- boot blocks are noise (128 early pairs
// all resolve) while the wedge lands seconds in. Only pair blocks
// entered after this many ms of process uptime
// (AC6_NATIVE_BLOCK_TRACE_DELAY_MS, default 10000).
inline bool block_trace_armed() {
  static const auto process_start = std::chrono::steady_clock::now();
  static const long long delay_ms = [] {
    if (const char* text = std::getenv("AC6_NATIVE_BLOCK_TRACE_DELAY_MS")) {
      long long parsed = 0;
      for (const char* p = text; *p >= '0' && *p <= '9'; ++p) {
        parsed = parsed * 10 + (*p - '0');
      }
      return parsed;
    }
    return 10000ll;
  }();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - process_start)
             .count() >= delay_ms;
}

// r497 supersedes the r91 "never blocks indefinitely" bound for the wait
// modes the guest actually uses. The 360 kernel contract for
// NtWaitForSingleObjectEx/NtWaitForMultipleObjectsEx/NtSignalAndWaitFor
// SingleObjectEx is (handle, wait_mode, alertable) where wait_mode 0
// waits FOREVER and wait_mode n waits n*100 ms -- there is no millisecond
// timeout parameter at all. The r91 bound collapsed every infinite wait
// into a 2 ms poll that returns STATUS_TIMEOUT; the guest's own wait
// loops then retried, and the in-binary wait instrumentation measured
// 36M such instant infinite-mode waits (plus a ~500k/s event-claim
// storm) in one 90 s probe window, saturating g_event_mutex and pacing
// the whole engine. A wait_mode-0 wait now blocks on g_event_cv until
// the event is claimable or the shutdown stop fires (native_guest_
// threads_stop_requested, r277), and a wait_mode-n wait waits that
// bound. Bounded waits keep the r288 SLOW anomaly trace; infinite waits
// park by design, so they are exempt from it.
bool wait_event(std::uint32_t key, std::uint32_t wait_mode) {
  if (key == 0u) return false;
  // r515: shutdown observance on the instant path. Blocked waits throw
  // GuestThreadTerminated through their predicate, but a thread churning
  // instantly-satisfied waits (the game's own busy-poll design, r500:
  // ~10M calls per 25 s window) never reaches any stop check, so
  // stop_and_join() hangs joining it (the wedged-run rc=137 class:
  // hang iff wedged because the wedge decides which waits churn vs
  // block). The stop flag is set only by stop_and_join itself, so this
  // changes nothing before shutdown begins.
  if (ac6::native::native_guest_threads_stop_requested()) {
    throw ac6::native::GuestThreadTerminated{};
  }
  if (std::getenv("AC6_NATIVE_WAIT_TIMING_TRACE") != nullptr) {
    start_wait_call_heartbeat_once();
    g_wait_event_calls.fetch_add(1, std::memory_order_relaxed);
    if (key == 0x121u) {
      g_wait_event_calls_gate121.fetch_add(1, std::memory_order_relaxed);
    }
  }
  // r288 (diagnostic only): this call is supposed to be bounded to ~2 ms
  // (r91); r287 proved the guest's own frame handshake CAN complete a
  // full cycle in under 120 ms, yet every prior cycle also measured
  // multi-second gaps between cycles for reasons none of r284-r287's
  // fixes changed. Anomaly-only (prints nothing for a normal call, so
  // this cannot flood the way an unconditional per-call trace did in
  // r282) -- names which specific bounded primitive, if any, is actually
  // exceeding its own bound, and by how much.
  const auto call_start = std::chrono::steady_clock::now();
  auto try_claim = [](EventState& event) {
    if (event.manual_reset) return event.signaled;
    if (event.pending_releases == 0u) return false;
    if (--event.pending_releases == 0u) event.signaled = false;
    return true;
  };
  std::unique_lock lock(g_event_mutex);
  auto it = g_events.find(key);
  if (it == g_events.end()) return false;
  // r499: keep the entry (and its per-key condvar) alive across a
  // concurrent erase of this key by another thread.
  std::shared_ptr<EventState> state = it->second;
  bool result;
  if (try_claim(*state)) {
    result = true;
  } else {
    auto claimable = [&] {
      auto retry = g_events.find(key);
      return ac6::native::native_guest_threads_stop_requested() ||
             retry == g_events.end() ||
             (retry->second->manual_reset ? retry->second->signaled
                                         : retry->second->pending_releases > 0u);
    };
    // r530 block pairing (diagnostic gate AC6_NATIVE_THREAD_SAMPLE,
    // same convention as the in-binary r497-r500 hooks): the guest-side
    // tripwire cannot see waits that never return, so pair here at the
    // HLE layer. Armed only past the boot window (block_trace_armed);
    // log the block entry (first 512 per process) and the wake with
    // elapsed ms (first 512); an entry with no wake is a forever-parked
    // thread naming its exact unsignaled key.
    const bool block_trace =
        std::getenv("AC6_NATIVE_THREAD_SAMPLE") != nullptr &&
        block_trace_armed();
    static std::atomic<unsigned> block_enter_count{0u};
    static std::atomic<unsigned> block_exit_count{0u};
    const unsigned block_ordinal = block_trace
                                       ? block_enter_count.fetch_add(1u)
                                       : 0u;
    // Inline hash (current_thread_tid is declared later in this file).
    const std::uint64_t block_tid =
        block_trace ? static_cast<std::uint64_t>(
                          std::hash<std::thread::id>{}(
                              std::this_thread::get_id()))
                    : 0u;
    const auto block_start = std::chrono::steady_clock::now();
    if (block_trace && block_ordinal < 512u) {
      std::fprintf(stderr,
                   "r530 hlewait enter ord=%u tid=%llu key=0x%08x mode=%u\\n",
                   block_ordinal,
                   static_cast<unsigned long long>(block_tid), key,
                   wait_mode);
    }
    if (wait_mode == 0u) {
      state->cv.wait(lock, claimable);
    } else {
      state->cv.wait_for(lock, std::chrono::milliseconds(wait_mode) * 100u,
                         claimable);
    }
    if (block_trace) {
      const auto block_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - block_start)
              .count();
      if (block_exit_count.fetch_add(1u) < 512u) {
        std::fprintf(stderr,
                     "r530 hlewait exit ord=%u tid=%llu key=0x%08x "
                     "elapsed_ms=%lld\\n",
                     block_ordinal,
                     static_cast<unsigned long long>(block_tid), key,
                     static_cast<long long>(block_ms));
      }
    }
    it = g_events.find(key);
    result = it != g_events.end() && try_claim(*it->second);
    if (ac6::native::native_guest_threads_stop_requested()) {
      throw ac6::native::GuestThreadTerminated{};
    }
  }
  if (std::getenv("AC6_NATIVE_WAIT_TIMING_TRACE") != nullptr) {
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - call_start)
            .count();
    if (wait_mode != 0u && elapsed_ms > 5) {
      std::fprintf(stderr,
                   "[wait_event SLOW] key=0x%08x elapsed_ms=%lld result=%d\\n",
                   key, static_cast<long long>(elapsed_ms), result ? 1 : 0);
    }
  }
  return result;
}

// r286: a Mutant is not an event. r285's own diagnostic trace confirmed
// this project's `sub_82345C88`/`sub_82345CE0` pair (the retail game's
// generic "kick and wait for target" utility, r283/r284) treats
// `*(gate+0)` as a real Xbox 360 Mutant -- acquired by a wait, released via
// `NtReleaseMutant` (traced: `initial_owner=0`, an unowned-at-creation
// mutex) -- not an auto- or manual-reset event. `NtCreateMutant` used to
// register nothing at all in `g_events`, so any real wait on it failed
// instantly (a map-lookup miss, not the intended 2 ms bounded contention
// wait), and `NtSignalAndWaitForSingleObjectEx`'s "signal" side called the
// generic `set_event()` on it regardless -- silently fabricating a phantom
// auto-reset EventState via `g_events[key]`'s auto-vivifying `operator[]`,
// with no ownership tracking, no recursion count, and no way to reject a
// release from a thread that never acquired it. This gives Mutants their
// own single-owner model instead.
struct MutantState {
  std::mutex mutex;  // r287: protects the three fields below, per-key --
                      // see the "r287" comment on g_mutants for why.
  bool owned = false;
  std::uint64_t owner_tid = 0;      // 0 == unowned; a hashed std::thread::id
  std::uint32_t recursion_count = 0;
  // r497: infinite-mode waits (wait mode 0, the Xbox 360 contract) block
  // on this per-key condvar and release_mutant() notifies it, so a guest
  // thread waiting on an owned mutant sleeps until the owner releases
  // instead of burning the previous 2 ms bounded retry loop at
  // hundreds of thousands of calls per second (measured: 36M instant
  // infinite-mode waits in one 90 s probe window).
  std::condition_variable cv;
};
// r287: this used to share g_event_mutex (r286's own revision, made to
// avoid adding a second independently-contended lock to every single
// Nt*Wait*/Nt*Signal* call site engine-wide). That traded one problem for
// a worse one this cycle's own instrumentation caught directly: an
// unrelated third thread hammering ITS OWN, completely different Mutant
// through this same one shared lock thousands of times per second
// (traced: 6,922 acquisitions in a 15 s window, versus 15 and 3 for the
// two threads r283-r286 were actually chasing) starved them of the SAME
// lock for seconds at a time under Linux's non-FIFO futex contention --
// explaining exactly the "every step is bounded to ~2 ms yet the loop
// only advances every few seconds" mystery r286 measured but could not
// attribute. Fixed the way this project's own `critical_section_for`/
// `g_critical_sections` already model exactly this shape: one lock per
// KEY (per Mutant object), not one lock shared by every Mutant in the
// whole engine. `g_mutants_registry_mutex` protects only the structural
// map lookup/insert itself (rare: creation only, and the read-only
// lookup below is a single hash probe held for nanoseconds); it never
// serializes one Mutant's acquire/release against another's.
std::mutex g_mutants_registry_mutex;
std::unordered_map<std::uint32_t, std::unique_ptr<MutantState>> g_mutants;

inline std::uint64_t current_thread_tid() {
  return static_cast<std::uint64_t>(
      std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

bool is_mutant(std::uint32_t key) {
  std::lock_guard lock(g_mutants_registry_mutex);
  return g_mutants.find(key) != g_mutants.end();
}

// One immediate attempt: acquires (or recursively re-acquires, matching
// the real API's contract that the owning thread may re-wait on its own
// mutant) if unowned or already owned by this thread; otherwise fails
// without blocking, for the caller to retry under the same bounded 2 ms
// contract every other wait stub here already uses.
bool try_acquire_mutant(std::uint32_t key) {
  MutantState* state;
  {
    std::lock_guard registry_lock(g_mutants_registry_mutex);
    auto it = g_mutants.find(key);
    if (it == g_mutants.end()) return false;
    state = it->second.get();
  }
  std::lock_guard lock(state->mutex);
  const std::uint64_t tid = current_thread_tid();
  if (state->owned && state->owner_tid != tid) return false;
  state->owned = true;
  state->owner_tid = tid;
  ++state->recursion_count;
  return true;
}

// r497: wait mode honored like wait_event()'s (0 = block forever on the
// per-key condvar until the owner releases or the shutdown stop fires;
// n = the old bounded retry, kept for bounded modes). A guest thread
// waiting on an owned mutant sleeps instead of burning 200 us retries.
bool wait_mutant(std::uint32_t key, std::uint32_t wait_mode) {
  // r515: same shutdown observance as wait_event()'s instant path (see
  // its comment): try_acquire_mutant() succeeding never reaches a stop
  // check, hanging stop_and_join() on a churning thread.
  if (ac6::native::native_guest_threads_stop_requested()) {
    throw ac6::native::GuestThreadTerminated{};
  }
  // r288 (diagnostic only, anomaly-gated -- see wait_event's own r288
  // comment above for why): names whether THIS specific bounded primitive
  // is where a slow cycle's time actually goes.
  if (std::getenv("AC6_NATIVE_WAIT_TIMING_TRACE") != nullptr) {
    start_wait_call_heartbeat_once();
    g_wait_mutant_calls.fetch_add(1, std::memory_order_relaxed);
    if (key == 0x120u) {
      g_wait_mutant_calls_gate120.fetch_add(1, std::memory_order_relaxed);
    }
  }
  const auto call_start = std::chrono::steady_clock::now();
  bool result = try_acquire_mutant(key);
  if (!result && wait_mode == 0u) {
    MutantState* state;
    {
      std::lock_guard registry_lock(g_mutants_registry_mutex);
      auto it = g_mutants.find(key);
      if (it == g_mutants.end()) return false;
      state = it->second.get();
    }
    const std::uint64_t tid = current_thread_tid();
    std::unique_lock lock(state->mutex);
    // r530 block pairing on the mutant path (same gate/bounds/delay as
    // wait_event's): a producer parked on an unreleased mutant names it
    // here; entries without wakes are forever-parked.
    const bool block_trace =
        std::getenv("AC6_NATIVE_THREAD_SAMPLE") != nullptr &&
        block_trace_armed();
    static std::atomic<unsigned> mutant_enter_count{0u};
    static std::atomic<unsigned> mutant_exit_count{0u};
    const unsigned mutant_ordinal =
        block_trace ? mutant_enter_count.fetch_add(1u) : 0u;
    const auto mutant_start = std::chrono::steady_clock::now();
    if (block_trace && mutant_ordinal < 512u) {
      std::fprintf(stderr,
                   "r530 hlewait enter ord=m%u tid=%llu key=0x%08x mode=%u\\n",
                   mutant_ordinal,
                   static_cast<unsigned long long>(tid), key, wait_mode);
    }
    state->cv.wait(lock, [&] {
      return ac6::native::native_guest_threads_stop_requested() ||
             !state->owned || state->owner_tid == tid;
    });
    if (block_trace) {
      const auto mutant_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - mutant_start)
              .count();
      if (mutant_exit_count.fetch_add(1u) < 512u) {
        std::fprintf(stderr,
                     "r530 hlewait exit ord=m%u tid=%llu key=0x%08x "
                     "elapsed_ms=%lld\\n",
                     mutant_ordinal,
                     static_cast<unsigned long long>(tid), key,
                     static_cast<long long>(mutant_ms));
      }
    }
    if (ac6::native::native_guest_threads_stop_requested()) {
      throw ac6::native::GuestThreadTerminated{};
    }
    if (!state->owned || state->owner_tid == tid) {
      state->owned = true;
      state->owner_tid = tid;
      ++state->recursion_count;
      result = true;
    }
  }
  if (!result) {
    const auto deadline = call_start + std::chrono::milliseconds(2);
    do {
      std::this_thread::sleep_for(std::chrono::microseconds(200));
      if (try_acquire_mutant(key)) {
        result = true;
        break;
      }
    } while (std::chrono::steady_clock::now() < deadline);
  }
  if (std::getenv("AC6_NATIVE_WAIT_TIMING_TRACE") != nullptr) {
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - call_start)
            .count();
    if (wait_mode != 0u && elapsed_ms > 5) {
      std::fprintf(stderr,
                   "[wait_mutant SLOW] key=0x%08x elapsed_ms=%lld result=%d\\n",
                   key, static_cast<long long>(elapsed_ms), result ? 1 : 0);
    }
  }
  return result;
}

// Only the current owner may release, and only as many times as it
// acquired (recursion): matches the real API's STATUS_MUTANT_NOT_OWNED
// contract instead of silently succeeding for any caller.
bool release_mutant(std::uint32_t key) {
  MutantState* state;
  {
    std::lock_guard registry_lock(g_mutants_registry_mutex);
    auto it = g_mutants.find(key);
    if (it == g_mutants.end()) return false;
    state = it->second.get();
  }
  bool released = false;
  {
    std::lock_guard lock(state->mutex);
    if (state->owned && state->owner_tid == current_thread_tid()) {
      if (--state->recursion_count == 0u) {
        state->owned = false;
        state->owner_tid = 0;
      }
      released = true;
    }
  }
  if (released) state->cv.notify_all();
  return released;
}

// r497: shutdown support for wait_mode-0 parkers. A guest thread parked
// in an infinite wait never returns to the stub's own r277 stop check,
// so native_guest_threads_stop_and_join() calls this to wake every
// parked waiter; the wait's own predicate then throws
// GuestThreadTerminated (r277's clean-shutdown contract).
void wake_all_kernel_waits() {
  {
    std::lock_guard lock(g_event_mutex);
    // r499: each key owns its condvar now -- wake every entry's cv.
    for (auto& entry : g_events) {
      entry.second->cv.notify_all();
    }
  }
  std::lock_guard registry_lock(g_mutants_registry_mutex);
  for (auto& entry : g_mutants) {
    entry.second->cv.notify_all();
  }
}

// r114: a suspended-created thread must block indefinitely until an
// explicit resume, unlike wait_event()'s bounded single-shot retry
// contract (designed for guest Nt*Wait* polling, not for parking a
// host std::thread before it ever runs guest code). Reuses the same
// g_events map/mutex/cv -- thread handles and event/semaphore/mutant
// handles already share one monotonic counter (g_next_handle), so
// there is no key collision between them.
// r278: the Nt/Ke wait and signal imports accept either a small integer
// handle (the NtCreateEvent family, tracked in g_events) or a guest POINTER
// to a live dispatcher object. The second form is real in this boot: the
// task dispatcher sub_821D4C20 polls NtWaitForSingleObjectEx on its own
// low-region KEVENT headers (observed 0x100046F4 / 0x10004744, ~200k+ polls
// per 3 s window) because the handle-only model returned STATUS_TIMEOUT
// forever. For pointers, model the standard NT dispatcher header the
// guest's inline KeInitializeEvent writes: SignalState at object+4,
// byte 0 = Type (0 = notification, 1 = synchronization).
constexpr std::uint32_t kMaxGuestHandle = 0x10000u;

inline bool is_guest_object_key(std::uint32_t key) {
  return key >= kMaxGuestHandle && key <= 0xFFFFFFF8u;
}

// Guest dispatcher objects (KEVENT headers in guest memory, keyed by
// their guest address) block on this shared condvar instead of spinning.
// set_guest_object writes the SignalState AND notifies, so a waiter wakes
// immediately.  The 4 ms fallback re-checks the SignalState in case the
// guest wrote it directly.  A wait for an object nobody signals blocks
// honestly -- that is a real gap in the HLE signal graph (r497/r583), not
// something to paper over with a timeout force-signal.
inline std::mutex g_guest_object_mutex;
inline std::condition_variable g_guest_object_cv;

inline bool wait_guest_object(uint8_t* base, std::uint32_t ptr) {
  if (!is_guest_object_key(ptr)) return false;
  const std::uint32_t state = PPC_LOAD_U32(ptr + 4u);
  if (state != 0u) {
    if (PPC_LOAD_U8(ptr + 0u) == 1u) PPC_STORE_U32(ptr + 4u, 0u);
    return true;
  }
  std::unique_lock<std::mutex> lock(g_guest_object_mutex);
  g_guest_object_cv.wait_for(lock, std::chrono::milliseconds(4));
  return false;
}

inline void set_guest_object(uint8_t* base, std::uint32_t ptr) {
  if (!is_guest_object_key(ptr)) return;
  if (PPC_LOAD_U8(ptr + 0u) == 1u) {
    PPC_STORE_U32(ptr + 4u, 1u);
  } else {
    const std::uint32_t state = PPC_LOAD_U32(ptr + 4u);
    if (state < 0x7FFFFFFFu) PPC_STORE_U32(ptr + 4u, state + 1u);
  }
  g_guest_object_cv.notify_all();
}

inline void clear_guest_object(uint8_t* base, std::uint32_t ptr) {
  if (is_guest_object_key(ptr)) PPC_STORE_U32(ptr + 4u, 0u);
}

inline void reset_guest_object(uint8_t* base, std::uint32_t ptr) {
  if (is_guest_object_key(ptr)) PPC_STORE_U32(ptr + 4u, 0u);
}

void park_until_resumed(std::uint32_t key) {
  if (key == 0u) return;
  std::unique_lock lock(g_event_mutex);
  auto it = g_events.find(key);
  if (it == g_events.end()) return;
  // r499: wait on this key's own condvar (shared_ptr keeps it alive
  // across a concurrent erase); the 100 ms bound still polls the
  // shutdown stop flag (r277).
  std::shared_ptr<EventState> state = it->second;
  while (!state->cv.wait_for(lock, std::chrono::milliseconds(100), [&] {
    auto retry = g_events.find(key);
    return retry == g_events.end() || retry->second->signaled;
  })) {
    if (ac6::native::native_guest_threads_stop_requested()) break;
  }
  // r277: shutdown ends parked threads through their entry lambda's
  // GuestThreadTerminated catch so the registry join can finish.
  if (ac6::native::native_guest_threads_stop_requested()) {
    throw ac6::native::GuestThreadTerminated{};
  }
}

// r216: real signature (documented, and confirmed at this XEX's own
// single real call site, traced through its wrapper) is NtSetTimerEx(
// HANDLE TimerHandle, PLARGE_INTEGER DueTime, PTIMERAPCROUTINE
// TimerApcRoutine, TIMER_TYPE TimerType, PVOID TimerContext, LONG
// Period, BOOLEAN ResumeContext, ULONG Flags) -- 8 params fit the PPC
// ABI's r3..r10 directly. The one real call site passes a NULL
// TimerApcRoutine and a negative (relative) DueTime of about -10.2ms
// with Period=0 (one-shot); this timer is waited on via
// NtWaitForSingleObjectEx (already modeled through g_events on the same
// handle), not an APC callback this project would need to invoke. Each
// timer's own cancellation flag lets NtCancelTimer suppress a pending
// fire without needing to join or interrupt the sleeping host thread.
std::mutex g_timers_mutex;
std::unordered_map<std::uint32_t, std::shared_ptr<std::atomic<bool>>>
    g_timer_cancelled;

void set_timer(std::uint32_t key, std::int64_t due_time_100ns,
               std::int32_t period_ms) {
  if (key == 0u) return;
  auto cancelled = std::make_shared<std::atomic<bool>>(false);
  {
    std::lock_guard lock(g_timers_mutex);
    g_timer_cancelled[key] = cancelled;
  }
  if (due_time_100ns >= 0) return;  // absolute due time, untraced: no-op
  const auto initial_delay =
      std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>(
          -due_time_100ns);
  std::thread([key, cancelled, initial_delay, period_ms]() mutable {
    std::this_thread::sleep_for(initial_delay);
    for (;;) {
      if (cancelled->load()) return;
      set_event(key);
      if (period_ms <= 0) return;
      std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }
  }).detach();
}

void cancel_timer(std::uint32_t key) {
  std::lock_guard lock(g_timers_mutex);
  auto it = g_timer_cancelled.find(key);
  if (it != g_timer_cancelled.end()) it->second->store(true);
}

std::uint32_t allocate_guest(uint8_t* base, std::uint32_t requested) {
  if (requested == 0u) return 0u;
  const std::uint64_t rounded =
      (static_cast<std::uint64_t>(requested) + 0xffffu) & ~0xffffull;
  if (rounded > 0x70000000ull) return 0u;
  std::uint32_t address = g_next_virtual.load(std::memory_order_relaxed);
  for (;;) {
    const std::uint64_t next = address;
    if (next + rounded > 0x7f000000ull) return 0u;
    const std::uint32_t desired =
        static_cast<std::uint32_t>(next + rounded);
    if (g_next_virtual.compare_exchange_weak(
            address, desired, std::memory_order_relaxed,
            std::memory_order_relaxed)) {
      std::memset(base + address, 0, static_cast<std::size_t>(rounded));
      return address;
    }
  }
}

// r236: shared by sprintf/_vsnprintf. Every real caller of both imports in
// this XEX was traced (r234/r235) and every format string reaching either
// one was individually decoded (scripts/DumpBytes.java) -- the full,
// exhaustively-verified specifier vocabulary across all of them is: bare
// literal text, `%s` (with an optional decimal minimum-width, e.g. the
// `%25s` this XEX itself uses), `%d`, and `%x`/`%X` (with an optional
// zero-padded decimal width, e.g. `%08x`/`%08X`). This parser implements
// exactly that vocabulary and nothing more; an unrecognized specifier is
// copied through literally (both characters, unconsumed) rather than
// guessing an argument width and risking a misaligned read of every
// argument after it -- the same "don't guess a fabricated value" discipline
// this file uses throughout (e.g. r108's MmQueryStatistics).
std::size_t guest_vprintf(uint8_t* base, std::uint32_t dest, std::size_t capacity,
                           std::uint32_t format,
                           const std::function<std::uint32_t()>& next_arg) {
  std::size_t written = 0;
  auto put = [&](char c) {
    if (written + 1 < capacity) PPC_STORE_U8(dest + written, static_cast<uint8_t>(c));
    ++written;
  };
  for (std::uint32_t i = 0;; ++i) {
    const std::uint8_t raw = PPC_LOAD_U8(format + i);
    if (raw == 0u) break;
    const char c = static_cast<char>(raw);
    if (c != '%') {
      put(c);
      continue;
    }
    ++i;
    bool zero_pad = false;
    std::uint32_t width = 0;
    char spec = static_cast<char>(PPC_LOAD_U8(format + i));
    if (spec == '0') {
      zero_pad = true;
      ++i;
      spec = static_cast<char>(PPC_LOAD_U8(format + i));
    }
    while (spec >= '0' && spec <= '9') {
      width = width * 10u + static_cast<std::uint32_t>(spec - '0');
      ++i;
      spec = static_cast<char>(PPC_LOAD_U8(format + i));
    }
    if (spec == '%') {
      put('%');
      continue;
    }
    if (spec == 's') {
      const std::uint32_t str_ptr = next_arg();
      std::uint32_t length = 0;
      if (str_ptr != 0u) {
        while (PPC_LOAD_U8(str_ptr + length) != 0u) ++length;
      }
      for (std::uint32_t pad = length; pad < width; ++pad) put(' ');
      for (std::uint32_t k = 0; k < length; ++k) {
        put(static_cast<char>(PPC_LOAD_U8(str_ptr + k)));
      }
      continue;
    }
    if (spec == 'd' || spec == 'x' || spec == 'X') {
      char format_spec[16];
      std::snprintf(format_spec, sizeof(format_spec), "%%%s%u%c",
                    zero_pad ? "0" : "", width, spec);
      char number[24];
      const int count = std::snprintf(
          number, sizeof(number), format_spec,
          spec == 'd' ? static_cast<int>(next_arg()) : next_arg());
      for (int k = 0; k < count && k < static_cast<int>(sizeof(number)); ++k) {
        put(number[k]);
      }
      continue;
    }
    // Unrecognized specifier: never observed at any traced real call site
    // (r234/r235) -- copy through rather than guess.
    put('%');
    if (spec == 0) {
      --i;  // re-read the terminator next iteration so the loop breaks cleanly
    } else {
      put(spec);
    }
  }
  if (capacity > 0u) {
    PPC_STORE_U8(dest + (written < capacity ? written : capacity - 1u), 0u);
  }
  return written;
}
}

// r497: exported (external-linkage) shutdown hook. The wait/lock helpers
// above live in an anonymous namespace, so native_guest_threads.cpp's
// stop_and_join reaches them only through this wrapper.
void ac6_native_wake_all_kernel_waits() { wake_all_kernel_waits(); }

"""


def render_body(name: str) -> str:
    if name == "NtAllocateVirtualMemory":
        return """  // Xbox 360 ABI: r3 = PVOID* base, r4 = SIZE_T* size.
  // The reserved guest address space commits pages on first touch.
  if (ctx.r3.u32 == 0u || ctx.r4.u32 == 0u) {
    ctx.r3.u64 = 0xC000000Du;  // STATUS_INVALID_PARAMETER
    return;
  }
  std::uint32_t requested = PPC_LOAD_U32(ctx.r4.u32);
  if (requested == 0u) {
    ctx.r3.u64 = 0xC0000017u;  // STATUS_NO_MEMORY
    return;
  }
  // r281: the documented NT contract for a caller-supplied BaseAddress is
  // that the region is rounded outward to page boundaries -- the base DOWN
  // to a page, the end (base + size) UP to a page -- and BOTH the rounded
  // base and the rounded size are written back. This stub rounded the base
  // down to 64 KiB and wrote the raw size back: the guest RTL heap
  // (sub_821F92B8/sub_821F9150/sub_821F8368) then read committed_end =
  // base_out + size_out short by the misalignment (traced on the US ISO:
  // commit 0x5C050 at 0x2E780000 -> committed end 0x2E7DC050; next commit
  // requested at 0x2E7DC050 came back as 0x2E7D0000 + 0x50000), its
  // uncommitted-range list and free list overlapped, and the sorted
  // free-list insert sub_821F8A00 spun forever. Page size: every heap call
  // carries 0x20000000 (the large-page bit) and the heap itself counts
  // pages as size >> 16 (sub_821F8368: NumberOfUnCommittedPages -= the
  // big-endian high half of the written-back size), so that bit selects
  // 64 KiB pages; without it the NT default 4 KiB page applies.
  const std::uint32_t allocation_type = ctx.r5.u32;
  const std::uint32_t page_size =
      (allocation_type & 0x20000000u) != 0u ? 0x10000u : 0x1000u;
  const std::uint32_t page_mask = page_size - 1u;
  std::uint32_t address = PPC_LOAD_U32(ctx.r3.u32);
  std::uint32_t region_size = 0u;
  if (address == 0u) {
    region_size = (requested + page_mask) & ~page_mask;
    address = allocate_guest(base, region_size);
    if (address == 0u) {
      ctx.r3.u64 = 0xC0000017u;  // STATUS_NO_MEMORY
      return;
    }
  } else {
    const std::uint64_t start = address & ~static_cast<std::uint64_t>(page_mask);
    const std::uint64_t end =
        (static_cast<std::uint64_t>(address) + requested + page_mask) &
        ~static_cast<std::uint64_t>(page_mask);
    if (end > 0x7f000000ull || end <= start) {
      ctx.r3.u64 = 0xC0000017u;  // STATUS_NO_MEMORY
      return;
    }
    address = static_cast<std::uint32_t>(start);
    region_size = static_cast<std::uint32_t>(end - start);
    // The 4 GiB guest reservation is already backed; a commit inside a
    // reserved range leaves existing contents alone, as NT does for pages
    // that are already committed.
  }
  PPC_STORE_U32(ctx.r3.u32, address);
  PPC_STORE_U32(ctx.r4.u32, region_size);
  ac6::native::native_guest_vd_service().register_allocation(
      base, address, region_size);
  ctx.r3.u64 = 0u;
"""
    if name == "ExAllocatePool":
        return """  const std::uint32_t address = allocate_guest(base, ctx.r3.u32);
  if (address != 0u) {
    ac6::native::native_guest_vd_service().register_allocation(
        base, address, ctx.r3.u32);
  }
  ctx.r3.u64 = address;
"""
    if name == "ExFreePool":
        return "  ctx.r3.u64 = 0u;  // pool pages are reclaimed with the guest\n"
    if name == "MmAllocatePhysicalMemoryEx":
        return """  const std::uint32_t address = allocate_guest(base, ctx.r4.u32);
  if (address != 0u) {
    ac6::native::native_guest_vd_service().register_allocation(
        base, address, ctx.r4.u32);
  }
  ctx.r3.u64 = address;
"""
    if name == "DbgPrint":
        # r91/r92: DbgPrint's two static retail call sites either pass an
        # already-formatted buffer (a caller-side vsnprintf-style helper
        # resolves %-specifiers before calling DbgPrint) or a literal
        # string with a raw extra register argument. Substituting the
        # latter's specifier ourselves would mean reimplementing a PPC
        # varargs printf, which is not verified safe against arbitrary
        # guest format strings -- so this dumps the raw guest string
        # unmodified (still fully resolved for the common pre-formatted
        # case) rather than risk misreading guest registers as va_args.
        return """  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    char message[512];
    std::size_t length = 0;
    const std::uint32_t guest_format = ctx.r3.u32;
    while (length + 1 < sizeof(message)) {
      const char byte = static_cast<char>(PPC_LOAD_U8(guest_format + length));
      if (byte == '\\0') break;
      message[length++] = byte;
    }
    message[length] = '\\0';
    std::fprintf(stderr, "[DbgPrint] %s\\n", message);
  }
  ctx.r3.u64 = 0u;
"""
    if name == "MmGetPhysicalAddress":
        return "  ctx.r3.u64 &= 0x1fffffffu;  // deterministic guest physical alias\n"
    if name in {"MmFreePhysicalMemory", "MmSetAddressProtect",
                "MmQueryAddressProtect"}:
        return "  ctx.r3.u64 = 0u;  // guest reservation owns page lifetime\n"
    if name == "NtFreeVirtualMemory":
        return "  ctx.r3.u64 = 0u;  // guest reservation is released at teardown\n"
    if name == "NtQueryVirtualMemory":
        return "  ctx.r3.u64 = 0u;  // reserved guest space has no host VM query\n"
    if name in {"RtlInitializeCriticalSection",
                "RtlInitializeCriticalSectionAndSpinCount"}:
        # r116: force the backing mutex to exist so a racing Enter never
        # finds an absent entry; the real API has no failure mode a guest
        # here would observe.
        return "  critical_section_for(ctx.r3.u32);\n  ctx.r3.u64 = 0u;\n"
    if name == "RtlEnterCriticalSection":
        # r116: real mutual exclusion -- was a no-op under a single-guest-
        # thread assumption this project's own ExCreateThread work
        # (r111-r115) has since disproven (eighteen real concurrent host
        # threads). recursive_mutex allows a thread already holding this
        # section to re-enter it, matching the real API's contract.
        # r282: a raw blocking lock() here is what turned an abandoned
        # section into a permanent shutdown hang. A wait-family stub can
        # throw GuestThreadTerminated (r213/r277's stop_requested() check)
        # while a worker holds a section entered here; that unwind skips
        # RtlLeaveCriticalSection (a separate, later guest call this host
        # call has no RAII tie to), leaking the recursive_mutex locked
        # forever -- confirmed at runtime: the g_held_sections heartbeat
        # below showed the SAME two keys held by the SAME two threads for
        # the entire remainder of every hung run. Any later thread that
        # calls lock() on one of those two keys then blocks in the kernel
        # forever, never reaching its own stop check, and
        # native_guest_threads_stop_and_join()'s join() on it never
        # returns. Bounded polling (the same technique wait_event/
        # park_until_resumed already use) makes acquisition itself
        # interruptible: the leaked mutex still exists, but nothing waits
        # on it past shutdown.
        return """  auto& section = critical_section_for(ctx.r3.u32);
  while (!section.try_lock()) {
    if (ac6::native::native_guest_threads_stop_requested()) {
      throw ac6::native::GuestThreadTerminated{};
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    start_held_sections_heartbeat_once();
    std::lock_guard lock(g_held_sections_mutex);
    g_held_sections[ctx.r3.u32] = static_cast<std::uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
  }
  ctx.r3.u64 = 0u;
"""
    if name == "RtlLeaveCriticalSection":
        return """  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    std::lock_guard lock(g_held_sections_mutex);
    g_held_sections.erase(ctx.r3.u32);
  }
  critical_section_for(ctx.r3.u32).unlock();
  ctx.r3.u64 = 0u;
"""
    if name == "KfAcquireSpinLock":
        # r191: KIRQL KfAcquireSpinLock(PKSPIN_LOCK SpinLock) -- real call
        # site 0x821e600c protects a real queue-append (0x821e6020-0x6054)
        # between this and the matching KfReleaseSpinLock at 0x821e6060.
        # Returned "old IRQL" is never traced back to a consumer in this
        # XEX's own call sites examined so far; PASSIVE_LEVEL (0) is the
        # only value real code below DISPATCH_LEVEL can observe itself at.
        return "  spin_lock_for(ctx.r3.u32).lock();\n  ctx.r3.u64 = 0u;\n"
    if name == "KfReleaseSpinLock":
        return "  spin_lock_for(ctx.r3.u32).unlock();\n  ctx.r3.u64 = 0u;\n"
    if name == "KeAcquireSpinLockAtRaisedIrql":
        # r191: VOID KeAcquireSpinLockAtRaisedIrql(PKSPIN_LOCK SpinLock) --
        # same lock object identity as KfAcquireSpinLock, but the real API
        # assumes the caller already raised IRQL itself, so it does not
        # return one.
        return "  spin_lock_for(ctx.r3.u32).lock();\n  ctx.r3.u64 = 0u;\n"
    if name == "KeReleaseSpinLockFromRaisedIrql":
        return "  spin_lock_for(ctx.r3.u32).unlock();\n  ctx.r3.u64 = 0u;\n"
    if name == "KeRaiseIrqlToDpcLevel":
        # r191: KIRQL KeRaiseIrqlToDpcLevel(VOID) -- no lock object
        # parameter; see native_guest_threads.cpp's g_dpc_level_mutex for
        # why this is a recursive_mutex, not spin_lock_for's plain mutex.
        # Old IRQL returned as PASSIVE_LEVEL (0) for the same reason as
        # KfAcquireSpinLock: untraced to a consumer in this XEX so far.
        # r422: goes through raise_dpc_level() (native_guest_threads.h),
        # not the mutex directly, so a GuestThreadTerminated thrown before
        # the matching KfLowerIrql can be released by
        # release_residual_dpc_level() in the catch block below instead
        # of orphaning the mutex forever.
        return "  ac6::native::raise_dpc_level();\n  ctx.r3.u64 = 0u;\n"
    if name == "KfLowerIrql":
        return "  ac6::native::lower_dpc_level();\n  ctx.r3.u64 = 0u;\n"
    if name == "KeGetCurrentProcessType":
        return "  ctx.r3.u64 = 0u;  // title process\n"
    if name == "ExGetXConfigSetting":
        return """  // CRT video/region probe: retail 768-line mode, no feature bits.
  if (ctx.r5.u32 != 0u) PPC_STORE_U32(ctx.r5.u32, 0x00000300u);
  ctx.r3.u64 = 0u;
"""
    if name in {"VdRetrainEDRAM", "VdRetrainEDRAMWorker",
                "VdInitializeEngines",
                "VdInitializeScalerCommandBuffer", "VdEnableDisableClockGating",
                "VdSetSystemCommandBufferGpuIdentifierAddress",
                "VdSetGraphicsInterruptCallback", "VdCallGraphicsNotificationRoutines",
                "VdShutdownEngines"}:
        return "  ctx.r3.u64 = 0u;  // native renderer owns the Vd lifecycle\n"
    if name == "VdInitializeRingBuffer":
        return """  ac6::native::native_guest_vd_service().initialize_ring(
      base, ctx.r3.u32, ctx.r4.u32);
  ctx.r3.u64 = 0u;
"""
    if name == "VdEnableRingBufferRPtrWriteBack":
        return """  ac6::native::native_guest_vd_service().enable_readback(
      base, ctx.r3.u32);
  ctx.r3.u64 = 0u;
"""
    if name == "VdSwap":
        return """  ac6::native::native_guest_vd_service().publish_write_address(
      base, ctx.r3.u32);
  ctx.r3.u64 = 0u;
"""
    if name == "VdIsHSIOTrainingSucceeded":
        return "  ctx.r3.u64 = 1u;  // host has no Xenon HSIO training phase\n"
    if name in {"RtlInitAnsiString", "RtlInitUnicodeString"}:
        element_size = "2u" if name == "RtlInitUnicodeString" else "1u"
        return f"""  const std::uint32_t destination = ctx.r3.u32;
  const std::uint32_t source = ctx.r4.u32;
  if (destination == 0u) {{
    ctx.r3.u64 = 0xC000000Du;  // STATUS_INVALID_PARAMETER
    return;
  }}
  std::uint32_t length = 0u;
  if (source != 0u) {{
    while (length < 0xfffeu && PPC_LOAD_U{8 if element_size == '1u' else 16}(source + length) != 0u) length += {element_size};
  }}
  PPC_STORE_U16(destination + 0u, static_cast<std::uint16_t>(length));
  PPC_STORE_U16(destination + 2u, static_cast<std::uint16_t>(length + {element_size}));
  PPC_STORE_U32(destination + 4u, source);
  ctx.r3.u64 = 0u;
"""
    if name == "ExCreateThread":
        # r114: the real ExCreateThread(Handle, StackSize, ThreadId,
        # XapiThreadStartup, StartAddress, StartContext, CreationFlags)
        # takes CreationFlags in r9 (7th integer arg, PPC ABI r3..r9) --
        # this stub previously never read it, so every spawned thread ran
        # immediately regardless of what the guest requested. Traced
        # directly to two reproducible SIGSEGVs (sub_82346428,
        # sub_821D4C20): both are a null guest function pointer invoked
        # through PPC_CALL_INDIRECT_FUNC before some other, not-yet-run
        # initialization had populated it -- consistent with a thread the
        # guest meant to create suspended (CREATE_SUSPENDED, the same bit
        # value 0x4 Win32 uses; Xbox 360's XDK mirrors that convention)
        # running ahead of its own setup instead of waiting for an
        # explicit resume.
        return """  const std::uint32_t output_handle = ctx.r3.u32;
  const std::uint32_t shim_address = ctx.r6.u32;
  const std::uint32_t routine_address = ctx.r7.u32;
  const std::uint32_t routine_argument = ctx.r8.u32;
  const std::uint32_t creation_flags = ctx.r9.u32;
  const std::uint32_t handle = g_next_handle.fetch_add(1u);
  if (output_handle != 0u) PPC_STORE_U32(output_handle, handle);
  if (shim_address < PPC_CODE_BASE ||
      shim_address >= PPC_CODE_BASE + PPC_CODE_SIZE ||
      routine_address < PPC_CODE_BASE ||
      routine_address >= PPC_CODE_BASE + PPC_CODE_SIZE) {
    ctx.r3.u64 = 0xC000000Du;  // STATUS_INVALID_PARAMETER
    return;
  }
  PPCFunc* shim = PPC_LOOKUP_FUNC(base, shim_address);
  if (shim == nullptr) {
    ctx.r3.u64 = 0xC0000017u;  // STATUS_NO_MEMORY / no generated target
    return;
  }
  PPCContext worker = ctx;
  worker.r1.u32 = g_next_thread_stack.fetch_sub(0x10000u);
  worker.r3.u32 = routine_address;
  worker.r4.u32 = routine_argument;
  // r280: the kernel CREATE_SUSPENDED bit is 0x1, not the Win32 0x4 r114
  // assumed. Primary evidence: this XEX's own thread-create wrapper
  // sub_821F7A18 computes ExCreateThread's CreationFlags (r9) as
  // (game_flags >> 2) & 1 -- it maps the Win32 CREATE_SUSPENDED bit 2 down
  // to kernel bit 0 -- and both pools that pass game flag 4 (sub_821D4B30
  // for the 0x821D4C20 dispatchers, sub_821D4CD8 for the 0x821D4F20 pool)
  // create their events after the thread and only then call the
  // NtResumeThread wrapper sub_821F5990. With 0x4 those threads ran before
  // their event handles existed (traced as flags=0x00000001 suspended=0).
  constexpr std::uint32_t kCreateSuspended = 0x00000001u;
  const bool start_suspended = (creation_flags & kCreateSuspended) != 0u;
  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    std::fprintf(stderr,
                 "[ExCreateThread] handle=%u routine=0x%08x flags=0x%08x "
                 "suspended=%d\\n",
                 handle, routine_address, creation_flags,
                 start_suspended ? 1 : 0);
  }
  // r305 (diagnostic only): r304 found routine=0x823453e8 (the r283
  // worker-thread routine) is spawned FRESH, repeatedly (8x in a single
  // 15s window), not a fixed pool of 8 long-lived threads -- only 1 of
  // those 8 led to sub_8233B378 (r303). The existing trace above prints
  // the routine address but not routine_argument, so it cannot say
  // which of several 0x823453e8 threads carries which per-invocation
  // task. This is its own dedicated env var (NOT AC6_NATIVE_IMPORT_TRACE,
  // which floods -- r282/r288/r289/r304), filtered to only this one
  // routine address so it stays low-volume even over a long window.
  if (routine_address == 0x823453e8u &&
      std::getenv("AC6_NATIVE_WORKER_SPAWN_TRACE") != nullptr) {
    // r305 (same cycle): sub_823453E8's own body (read in full) is a
    // generic per-task trampoline itself -- it loads a function pointer
    // from *(routine_argument+20) and an argument from
    // *(routine_argument+24), then calls through it indirectly. THIS is
    // the real task-type discriminator, one level deeper than the
    // routine_argument pointer alone can show.
    const std::uint32_t task_function = PPC_LOAD_U32(routine_argument + 20u);
    const std::uint32_t task_argument = PPC_LOAD_U32(routine_argument + 24u);
    std::fprintf(stderr,
                 "[worker-spawn] handle=%u routine_argument=0x%08x "
                 "task_function=0x%08x task_argument=0x%08x\\n",
                 handle, routine_argument, task_function, task_argument);
  }
  if (start_suspended) create_event(handle, /*manual_reset=*/true,
                                     /*signaled=*/false);
  try {
    std::thread guest_thread([shim, worker, base, handle, start_suspended]() mutable {
      if (start_suspended) park_until_resumed(handle);
      try {
        shim(worker, base);
      } catch (const ac6::native::GuestThreadTerminated&) {
        // r213: ExTerminateThread never returns on real hardware; catch
        // it here so this thread ends cleanly instead of escaping the
        // thread entry point and invoking std::terminate() on the whole
        // process.
        // r422: this thread may have raised IRQL (KeRaiseIrqlToDpcLevel)
        // and been terminated here before its matching KfLowerIrql --
        // release whatever it still holds instead of orphaning the
        // shared g_dpc_level_mutex for every other thread forever.
        ac6::native::release_residual_dpc_level();
      }
    });
    // r277: joinable registry entry instead of detach(); shutdown joins
    // every worker so stub globals outlive their last use.
    ac6::native::native_guest_threads_register(std::move(guest_thread));
  } catch (...) {
    ctx.r3.u64 = 0xC0000017u;  // host thread creation failed
    return;
  }
  ctx.r3.u64 = 0u;
"""
    if name in {"NtResumeThread", "KeResumeThread"}:
        # r114: releases a thread ExCreateThread parked on CREATE_SUSPENDED.
        # No nested suspend count is modeled (this project spawns each
        # thread with at most one pending suspension) -- the previous
        # count is reported as 1 the first time this resumes a still-
        # parked thread, 0 on a redundant call against an already-running
        # one, matching NtResumeThread's real "previous count" contract
        # for that single-suspension case without asserting anything about
        # a nesting depth this harness never creates.
        return """  const bool was_already_running = set_event(ctx.r3.u32);
  if (ctx.r4.u32 != 0u) PPC_STORE_U32(ctx.r4.u32,
                                       was_already_running ? 0u : 1u);
  ctx.r3.u64 = 0u;
"""
    if name == "ObReferenceObjectByHandle":
        # r147: real signature (this project's own reduced Xbox 360 kernel
        # form, confirmed consistent across every call site this cycle
        # checked -- sub_821EF2xx, sub_821F3Dxx, sub_821F3Exx,
        # sub_823ADCxx) is (Handle, ObjectType, PVOID* Object). Every one
        # of those call sites treats the returned "object" purely as an
        # opaque token immediately re-passed to another kernel thread API
        # (KeSetBasePriorityThread/KeQueryBasePriorityThread/
        # ObDereferenceObject) -- never dereferenced by guest code itself
        # -- so this project's own handles (already-opaque integers) serve
        # directly as that token: write the handle back out as the object.
        #
        # The generic offline-import fallback this call previously fell
        # through to never wrote *Object at all. One call site
        # (sub_823ADE00 in the generated tree) does not even check this
        # call's own return status before immediately calling
        # KeResumeThread on that unwritten, uninitialized *Object -- the
        # real handle a matching ExCreateThread(..., CREATE_SUSPENDED)
        # parked never reaches KeResumeThread's real set_event() dispatch,
        # so that worker thread can never actually resume. Writing the
        # real handle through fixes that thread-resume path directly, not
        # just this import's own status.
        return """  if (ctx.r5.u32 != 0u) PPC_STORE_U32(ctx.r5.u32, ctx.r3.u32);
  ctx.r3.u64 = 0u;
"""
    if name == "KeSetAffinityThread":
        # r148: real signature (this project's own reduced Xbox 360 kernel
        # form; confirmed at this XEX's own single call site, reached only
        # after this cycle's ObReferenceObjectByHandle fix) is
        # (Handle, DWORD Affinity, DWORD* PreviousAffinity). Unlike an
        # NTSTATUS-returning import, the real Windows/Xbox kernel
        # contract for this call returns the *previous* affinity mask
        # itself in r3, not a status code -- confirmed by this XEX's own
        # caller, which treats r3 as a raw mask (`cntlzw`/`subfic` to find
        # the previous core index from it), not as an error code, once
        # past its own `< 0` sanity guard.
        #
        # The generic offline-import fallback this call previously fell
        # through to returned kOfflineStatus (negative as a signed mask),
        # which the caller's own `< 0` guard reads as failure every time,
        # and never wrote *PreviousAffinity, leaving the caller's own
        # bit-scan over that uninitialized stack slot to produce a
        # meaningless "previous core index". This project models no real
        # per-core thread affinity (a single host-scheduled execution
        # model, matching every other thread-management stub here), so
        # there is no genuine "previous" mask to report; core 0
        # (mask 0x1, the lowest hardware thread on this title's 6-thread
        # Xenon topology) is a safe, always-in-range default that keeps
        # the caller's own bit-scan meaningful instead of reading
        # uninitialized memory.
        return """  if (ctx.r5.u32 != 0u) PPC_STORE_U32(ctx.r5.u32, 1u);
  ctx.r3.u64 = 1u;
"""
    if name == "ObDereferenceObject":
        # r162: real signature (PVOID Object) -> LONG, the object's new
        # reference count after the decrement -- not an NTSTATUS. Checked
        # against all 18 of this XEX's own static call sites
        # (sub_821F3DA0, sub_821F3EA0, sub_821F3F30, sub_821EF308,
        # sub_823ADBD8, and the rest the campaign's own xref scan found):
        # every one of them discards r3 immediately after this call
        # (overwritten by an unrelated return value, or the call is the
        # last thing done before an unconditional `li r3,<constant>`).
        # There is no genuine "new reference count" this project's
        # opaque-handle model can report, so the return value is
        # observably inert either way -- but the generic offline-import
        # fallback this call previously fell through to still returned
        # kOfflineStatus, an NTSTATUS-shaped negative value where the
        # real contract is a small non-negative count. Matches
        # KeSetAffinityThread's own r148 precedent: fix the contract
        # shape even where no currently-traced caller depends on it, so
        # a future caller that does check it does not inherit a
        # status-shaped value from a plain-count-returning import.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "KeSetBasePriorityThread":
        # r162: real signature (PKTHREAD Thread, LONG Increment) -> LONG,
        # the *previous* base priority increment -- not an NTSTATUS.
        # Checked against all 3 of this XEX's own static call sites
        # (sub_821F3DA0, sub_821EF308, sub_823ADBD8): each discards r3
        # immediately after this call the same way ObDereferenceObject's
        # own call sites do (r3 unconditionally overwritten before the
        # function returns). Same contract-shape correction as
        # ObDereferenceObject above and KeSetAffinityThread (r148): 0 is
        # a safe, in-range "previous increment" default, not the
        # NTSTATUS-shaped kOfflineStatus sentinel a plain-LONG-returning
        # import should never produce.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "KeQueryBasePriorityThread":
        # r163: real signature (PKTHREAD Thread) -> LONG, the thread's
        # current base priority increment -- not an NTSTATUS. This XEX's
        # own single static call site (sub_821F3EA0, the only xref to
        # this import thunk) is the one KeSetBasePriorityThread/
        # ObDereferenceObject sibling that actually USES the return
        # value: it clamps it to [-16, 15] and returns that as its own
        # result. The generic offline-import fallback this call
        # previously fell through to returned kOfflineStatus
        # (0xC00000BB), a large negative value as signed LONG, which the
        # caller's own `cmpwi cr6,r31,-0x10` clamp always caught, making
        # every query of this thread's priority report the clamp floor
        # (-15) unconditionally rather than a real, in-range value. This
        # project tracks no real per-thread priority increment (matching
        # the "single host-scheduled execution model" already established
        # for KeSetAffinityThread/KeSetBasePriorityThread above), so 0
        # (baseline/normal priority) is the safe, in-range default that
        # keeps the caller's own clamp a no-op instead of always firing.
        return "  ctx.r3.u64 = 0u;\n"
    if name in {"KeEnterCriticalRegion", "KeLeaveCriticalRegion"}:
        # r164: real signature is VOID -- no return value at all. Checked
        # against this XEX's own real static call sites (2 each, thunks
        # 0x823D068C/0x823D067C): the one traced in full (sub_821F3540's
        # call to KeEnterCriticalRegion) has r3 overwritten by the very
        # next instruction before anything reads it, matching the real
        # VOID contract's own expectation that no caller ever checks a
        # return value here. The generic offline-import fallback's
        # kOfflineStatus was already harmless at this call site (nothing
        # reads it), but is still the wrong shape for a VOID-returning
        # kernel call -- fixed for the same defensive-correctness reason
        # as the ObDereferenceObject/KeSetBasePriorityThread family (r162):
        # a status-shaped sentinel should never leak out of an import
        # whose real contract has no return value to shape at all.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "RtlTryEnterCriticalSection":
        # r164: real signature is BOOLEAN (nonzero = lock acquired). This
        # XEX's own 5 real static call sites (thunk 0x823D00AC) do check
        # the return value (a zero-vs-nonzero test, e.g. sub_8233CF78's
        # `cmplwi r3,0x0 / beq <skip-critical-section>`) -- the generic
        # offline-import fallback's kOfflineStatus was already nonzero, so
        # it already evaluated as "lock acquired" under every traced
        # caller's own test, and this fix changes no observed control
        # flow. It is made anyway for the same reason as the VOID pair
        # above: an NTSTATUS-shaped sentinel is the wrong shape for a
        # BOOLEAN-returning import, and a canonical `1` is what a real
        # "lock always available" stub (this project models no real
        # per-thread contention) should actually report.
        return "  ctx.r3.u64 = 1u;\n"
    if name == "NetDll_recvfrom":
        # r583: return SOCKET_ERROR with a 1 ms kernel poll throttle.
        # Dedicated network threads poll recvfrom in a tight loop; an
        # instant return spins 7 threads at 100% CPU and starves the game
        # loop (livelock).  poll(NULL,0,1) sleeps efficiently in the kernel
        # (no busy-wait) capping the poll rate at ~1 kHz.  The game loop's
        # own once-per-frame recvfrom adds at most 1 ms/frame.
        return """  ::poll(nullptr, 0, 1);
  ctx.r3.u64 = 0xFFFFFFFFFFFFFFFFull;
"""
    if name in {"NetDll_XNetStartup", "NetDll_WSAStartup"}:
        # r167: real signature is INT (0 = success), the WinSock/XNet
        # convention, not an NTSTATUS. sub_821FCCE0/sub_821FCED0 (this
        # XEX's own two thin wrapper functions around these imports,
        # reached indirectly rather than by a literal `bl`, matching the
        # same computed-call pattern r156 found for a different import --
        # not re-derived in full this cycle, deferred as a separate
        # question) each return this call's own value completely
        # unmodified as their own result: `bl 0x823d06bc` /
        # `addi r1,r1,0x70` / `... / blr` -- r3 is never touched between
        # the call and the wrapper's own return. The generic
        # offline-import fallback's kOfflineStatus (0xC00000BB) is
        # nonzero, which a real caller checking "== 0 means success" (the
        # standard WinSock/XNet convention) would read as failure, on an
        # offline-only stub that has no real network condition to report
        # as a failure in the first place -- this project's own established
        # "Offline-only HLE boundary; no socket or host I/O side effect"
        # pattern (used elsewhere in this file) already treats an absent
        # network as something to succeed past, not fail on. 0 (success)
        # is the correct shape and the correct offline-boundary behavior.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "VdQueryVideoMode":
        # r169: real signature is VOID VdQueryVideoMode(X_VIDEO_MODE*) --
        # a struct-fill through r3, not a status return (r168 named this
        # gap and deferred implementation pending real offset derivation
        # from this XEX's own disassembly, not assumed from any other
        # project's independent reimplementation).
        #
        # Both of this XEX's own real static call sites read multiple
        # struct fields and compute on them for real:
        #   sub_821F0DB0 @ 0x821F0E78: reads struct+0x0 (u32, written to
        #     TWO separate output fields -- the classic "width" /
        #     "actual width" duplication), struct+0x4 (u32, written once),
        #     and struct+0x14 (float, added to a constant then truncated
        #     to an integer -- consistent with a refresh-rate-derived
        #     timing value).
        #   sub_821F2BC8 @ 0x821F2C00: reads struct+0x8 and runs it
        #     through a `cntlzw`/shift/`xori` sequence -- the canonical
        #     PPC "normalize nonzero to 1" boolean idiom, confirming
        #     struct+0x8 is a boolean-shaped flag, independently of the
        #     first call site.
        #
        # struct+0x0/+0x4/+0x8/+0x14 are therefore evidence-confirmed by
        # this XEX's own code, not assumed. Positionally, three more
        # 4-byte boolean-shaped fields fit exactly between +0x8 and
        # +0x14 (matching a width/height/is_interlaced/is_widescreen/
        # is_hi_def/refresh_rate field order -- cross-checked, not
        # copied, against has207/xenia-edge's own public X_VIDEO_MODE
        # field *names*, r168), but +0xC/+0x10 are not directly read by
        # either traced call site, so they are filled with the same
        # "no evidence either way" default (0) the generic fallback
        # would have left an untouched byte at, not asserted as a
        # specific confirmed value.
        #
        # display_width/display_height use 1280x720 -- not invented for
        # this fix, but this project's own pre-existing, already-qualified
        # resolution assumption used throughout its PM4/swap-packet test
        # fixtures (native/fixtures/xenos-capsule-minimal.json,
        # native/tests/native_xenos_tests.cpp) -- kept internally
        # consistent rather than picking a new, unrelated number.
        # is_interlaced=0 (progressive; correct for 720p, which is
        # non-interlaced by definition) and refresh_rate=60.0f (standard
        # NTSC 60 Hz, matching this project's own single-region NTSC-U/J
        # target) are the only two values not directly read off this XEX's
        # own bytes; both are ordinary, unremarkable production defaults
        # for those exact display parameters, not values chosen to game
        # any specific downstream comparison.
        return """  PPC_STORE_U32(ctx.r3.u32 + 0, 1280u);
  PPC_STORE_U32(ctx.r3.u32 + 4, 720u);
  PPC_STORE_U32(ctx.r3.u32 + 8, 0u);
  PPC_STORE_U32(ctx.r3.u32 + 0x14, 0x42700000u);  // 60.0f
"""
    if name == "VdQueryVideoFlags":
        # r170: real signature is DWORD VdQueryVideoFlags(VOID) -- a flags
        # bitmask return value, not a status code. This XEX's one real call
        # site (0x821f31cc) only tests bit 0 of the return
        # (`rlwinm. r11,r3,0,31,31; bne ...`) to pick between two ways of
        # computing a display height. The generic offline-import fallback
        # previously returned `kOfflineStatus` (0xC00000BB) here -- an
        # NTSTATUS constant whose low bit happens to be 1, which forced the
        # same branch every time not because that branch is correct, but
        # because a status code was reused as if it were a flags value. The
        # real contract has no status/failure shape at all.
        # No evidence at this one call site favors either branch outcome, so
        # the fix is the neutral flags-shaped value -- no flags set -- not a
        # value picked to force a particular downstream comparison (r53's
        # precedent, reaffirmed by r169).
        return "  ctx.r3.u64 = 0u;\n"
    if name == "VdGetCurrentDisplayGamma":
        # r170: real signature is VOID VdGetCurrentDisplayGamma(DWORD* type,
        # FLOAT* value) -- two pointer outputs, not a status return. This
        # XEX's one real call site (0x821eb454) sets up `r3`/`r4` as two
        # adjacent stack slots, then reads them back as an integer
        # (`lwz r14,0x54(r1)`) and a float (`lfs f2,0x50(r1)`) and compares
        # both against a cached table entry to decide whether to rebuild a
        # gamma-correction table. That cache starts uninitialized, so the
        # rebuild path is taken on the first call regardless of the exact
        # values supplied here -- no crash or hang risk either way, and no
        # call site reads a value that would let this XEX's own disassembly
        # pin the real type/gamma constants. type=0 and gamma=2.2 are
        # ordinary production defaults (a generic curve index and a
        # standard display gamma), not read from this XEX's own bytes and
        # not chosen to force a specific downstream comparison.
        return """  PPC_STORE_U32(ctx.r3.u32, 0u);
  PPC_STORE_U32(ctx.r4.u32, 0x400ccccdu);  // 2.2f
"""
    if name == "VdGetCurrentDisplayInformation":
        # r170: struct-fill call (`r3 = &struct`), evidence-confirmed across
        # ALL THREE of this XEX's own real call sites, cross-validated
        # against r169's VdQueryVideoMode fix:
        #   0x821f0764 (struct at [r1+0x170]): reads struct+0x48/+0x4a/+0x56
        #   as three separate u16 fields and forwards them verbatim into
        #   the SAME cached output fields (0x5414/0x5418/0x541c) that
        #   VdQueryVideoMode (r169) fills from its own struct+0x00/+0x04 --
        #   independent confirmation that 0x5414=width, 0x5418=height,
        #   0x541c=actual_width are real, distinct fields (not always-equal
        #   duplicates: here width and actual_width come from two DIFFERENT
        #   struct offsets, +0x48 and +0x56).
        #   0x821ea4d8 and 0x821ea2a4 (structs at [r1+0x60] and
        #   [r1+0x1a0]) both additionally read a byte at struct+0x05 into a
        #   normalize/compare idiom, confirming a real boolean-shaped field
        #   there.
        # Values: width/actual_width=1280, height=720 -- this project's own
        # pre-existing resolution assumption (r169), not invented; no
        # evidence distinguishes width from actual_width for this project's
        # single fixed target, so the same value is used for both.
        #
        # r175: struct+0x05 traced to a real algorithm choice at BOTH call
        # sites, not a cosmetic flag. At 0x821ea4d8, `value==1` selects
        # 0x821eb778 (a genuine bilinear-style LINEAR INTERPOLATION scaler
        # -- computes a fractional blend between two neighbor lookups,
        # `subf r9,r9,r8` then scaled) over `value!=1` selecting 0x821eb6e0
        # (a simpler NEAREST-NEIGHBOR scaler -- direct per-element
        # lookup/duplicate, no blend). At 0x821ea2a4, `value!=1` sets a bit
        # in a persisted flags byte that `value==1` leaves clear -- the
        # same direction (1 = the plain/default case, not-1 = a marked
        # deviation) at both sites. A higher-quality interpolated scaler is
        # the physically sensible choice for this project's own
        # already-established HD/widescreen target (1280x720), and no
        # evidence at either site points the other way. 1u.
        return """  PPC_STORE_U16(ctx.r3.u32 + 0x48, 1280u);
  PPC_STORE_U16(ctx.r3.u32 + 0x4a, 720u);
  PPC_STORE_U16(ctx.r3.u32 + 0x56, 1280u);
  PPC_STORE_U8(ctx.r3.u32 + 0x5, 1u);
"""
    if name == "XGetVideoMode":
        # r171: real signature is VOID XGetVideoMode(XVIDEO_MODE*) -- a
        # struct-fill through r3, the same real XDK output struct
        # VdQueryVideoMode (r169) fills (XGetVideoMode is the XAM-level
        # wrapper around it on real hardware). This XEX's one real call
        # site (0x82339794-0x82339798, struct at [r1+0x60]) reads
        # struct+0x14 as a float (`lfs f0,0x74(r1)`) and uses it in REAL
        # consequential arithmetic: compared against a sentinel constant,
        # then used as the divisor of `fdivs f1,f31,f0`. Left
        # unimplemented, that divisor is whatever garbage occupies this
        # stack slot -- a real risk of a spurious near-zero divide
        # producing Inf/NaN that propagates into further guest float math.
        # This exactly matches VdQueryVideoMode's own struct+0x14
        # (refresh-rate-shaped float), corroborating the shared struct
        # type from a second, independent real call site in this XEX.
        # Only +0x14 is read at this one call site, so only it is filled
        # -- +0x00/+0x04/+0x08 are not asserted here without their own
        # reading evidence at this call site, per r169's own discipline.
        # 60.0f (`0x42700000`), the same NTSC default r169 used, not
        # invented for this fix.
        return "  PPC_STORE_U32(ctx.r3.u32 + 0x14, 0x42700000u);  // 60.0f\n"
    if name == "XGetGameRegion":
        # r172: real signature is DWORD XGetGameRegion(VOID) -- a region
        # code, not a status. All THREE of this XEX's real call sites read
        # it, and the exact numeric value matters (not just membership in
        # an accepted set):
        #
        # `0x821babdc` (sub_821BAB90): the return value is cached, then
        # compared for equality against exactly four constants --
        # `0x1ff`/`0x101`/`0x102`/`0x1fc` -- with all four branching to the
        # SAME target. Any of the four gives identical behavior here.
        #
        # `0x821f4a68` (a cached language-code helper): extracts byte1
        # (bits 8-15) of the return. `0x101` and `0x102` both have
        # byte1=0x01, taking the same branch as each other, which then
        # further distinguishes `r3==0x101` exactly (-> cached code 20)
        # from any other byte1=0x01 value including `0x102` (-> code 21).
        #
        # `0x821f4b0c` (a cached region-category helper): extracts byte2
        # (bits 16-23). `0x101`, `0x102`, `0x1ff` and `0x1fc` all have
        # byte2=0x01, taking the same branch, which then distinguishes
        # `r3==0x101` exactly (-> category 2) from any other byte2=0x01
        # value including `0x102`/`0x1ff`/`0x1fc` (-> category 7, a
        # fallback/generic-NTSC-family category rather than the primary
        # one).
        #
        # `0x101` is therefore not an arbitrary pick between two
        # equally-plausible codes: it is the ONE value this XEX's own code
        # treats as the privileged, exact-match case in both consumer
        # functions, with every other accepted code (`0x102` included)
        # falling to a secondary/fallback grouping -- independent
        # corroboration from this XEX's own control flow, not merely the
        # "-us"/"ntsc-uj" naming convention this project already targets.
        return "  ctx.r3.u64 = 0x101u;\n"
    if name == "XGetAVPack":
        # r173: real signature is DWORD XGetAVPack(VOID) -- an AV-cable
        # type code, not a status. This XEX's one real call site
        # (0x821f5d14) only checks the return for equality against four
        # specific values -- 0x3/0x6/0x8/0x4 -- all branching to the SAME
        # "skip this setup" target (0x821f5eac); the return value is never
        # stored or read again afterward. Any value outside that four-item
        # set behaves identically here (proceeds into a language-menu
        # table setup this project's Gate 2 target needs to reach). 0u is
        # the simplest such value -- not asserted to match any specific
        # real AV-pack enum meaning, only to avoid the four values this
        # XEX's own code treats specially.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XGetLanguage":
        # r174: real signature is DWORD XGetLanguage(VOID) -- a language
        # ID, not a status. This XEX's one real call site (0x821f5d9c)
        # feeds the result into a bounds check against 10 and then (beyond
        # this call site's own visible instructions) a per-language lookup
        # table, so the exact value matters unlike XGetAVPack above. 1 is
        # the real Xbox 360 XDK's own standardized `XC_LANGUAGE_ENGLISH`
        # constant -- a platform-wide protocol value, not something
        # specific to this XEX's own compiled layout (unlike a struct byte
        # offset), and consistent with this project's own NTSC-U/J target.
        return "  ctx.r3.u64 = 1u;\n"
    if name == "XamUserGetSigninState":
        # r176: real signature is DWORD XamUserGetSigninState(DWORD
        # dwUserIndex), a real enum (0=not signed in, 1=signed in locally,
        # 2=signed in to Xbox Live), not a status. This XEX's real call
        # sites gate real control flow on it, not merely a discarded read:
        #
        # `0x821f4428` (a sign-in resolution helper): loops user indices
        # 0..3 (or checks one specific index) calling this import and
        # testing `cmpwi cr6,r3,0x1` -- the FIRST index whose result is
        # EXACTLY 1 is treated as the active signed-in user (stores a
        # success code and proceeds); if none matches, it falls through to
        # a different function entirely (a sign-in prompt/fallback path).
        # The generic offline-import fallback's kOfflineStatus
        # (0xC00000BB) never equals 1 for any index, so this always fell
        # through to that fallback path -- a real, consequential bug, not
        # a cosmetic one.
        # `0x82206954` (an unrelated per-player update function): tests
        # the result against 0 (`cmpwi cr6,r29,0x0`) to gate a further
        # update -- 0 is real hardware's own "not signed in" sentinel,
        # consistent with the same enum.
        #
        # This project's own established convention throughout this file
        # is single-player, fully offline (every stub here is commented
        # "Offline-only HLE boundary; no socket or host I/O side effect"),
        # so index 0 is signed in LOCALLY (1) -- not to Live (2), which
        # would assert real network/account state this project has never
        # modeled -- and every other index is not signed in (0).
        return """  const std::uint32_t user_index = ctx.r3.u32;
  ctx.r3.u64 = (user_index == 0u) ? 1u : 0u;
"""
    if name == "XamGetSystemVersion":
        # r177: real signature is DWORD XamGetSystemVersion(VOID) -- a
        # dashboard build number, not a status. Every real call site
        # (0x821fcd04, 0x821fcef0, 0x82210ed4, 0x82210fac, and r176's own
        # 0x821f4440) compares it against a version threshold to decide
        # whether to probe for optional, newer-dashboard-only
        # functionality (via XexGetModuleHandle/XexGetProcedureAddress,
        # both still unimplemented and already safely fail-closed) or take
        # a simpler/cached fallback path -- every examined branch degrades
        # to a working path regardless of which side of the threshold is
        # taken, EXCEPT 0x821f4440 (the sign-in resolution helper r176
        # just fixed): there, `>= 0x20096b00` skips the signin-state loop
        # entirely, going straight to a different, unexamined function.
        # Returning a value BELOW every observed threshold (0x20096b00 is
        # the lowest) is therefore not just a safe default -- it is
        # required for r176's fix to actually be exercised at that call
        # site, and every other call site's own branches confirm a low
        # value degrades safely elsewhere too. 0x20000000: a plausible,
        # round dashboard-version-shaped value, not read from this XEX's
        # own bytes and clearly below every real threshold found.
        return "  ctx.r3.u64 = 0x20000000u;\n"
    if name == "XamInputGetState":
        # r567: the US caller uses the two-argument XInput wrapper at
        # 82390CE0. That thunk moves the state pointer r4 -> r5, sets r4
        # (XAM flags) to zero, then branches to the import. The import ABI
        # is therefore (r3=user, r4=flags, r5=state), unlike the wrapper.
        # Original PPC: 7C852378, 38800000, 4803FC94. The caller's 0x48F
        # comparison still supplies the disconnected error contract.
        return """  const std::uint32_t user_index = ctx.r3.u32;
  ac6::native::NativeGuestInputService::GamepadState pad{};
  if (!ac6::native::native_guest_input_service().get_state(user_index, pad)) {
    ctx.r3.u64 = 0x48fu;  // ERROR_DEVICE_NOT_CONNECTED
    return;
  }
  PPC_STORE_U32(ctx.r5.u32 + 0x0, pad.packet_number);
  PPC_STORE_U16(ctx.r5.u32 + 0x4, pad.buttons);
  PPC_STORE_U8(ctx.r5.u32 + 0x6, pad.left_trigger);
  PPC_STORE_U8(ctx.r5.u32 + 0x7, pad.right_trigger);
  PPC_STORE_U16(ctx.r5.u32 + 0x8, static_cast<std::uint16_t>(pad.thumb_lx));
  PPC_STORE_U16(ctx.r5.u32 + 0xa, static_cast<std::uint16_t>(pad.thumb_ly));
  PPC_STORE_U16(ctx.r5.u32 + 0xc, static_cast<std::uint16_t>(pad.thumb_rx));
  PPC_STORE_U16(ctx.r5.u32 + 0xe, static_cast<std::uint16_t>(pad.thumb_ry));
  ctx.r3.u64 = 0u;
"""
    if name == "XamInputSetState":
        # r180: real signature is DWORD XamInputSetState(DWORD dwUserIndex,
        # XINPUT_VIBRATION* pVibration) -- struct-read through r4 (two u16
        # motor speeds, Microsoft's own fixed ABI, same category as
        # XINPUT_STATE above), same real error contract as
        # XamInputGetState (0x48F when not connected). Best-effort: a real
        # console accepts this call for a connected pad even on hardware
        # without motors, matched by
        # NativeGuestInputService::set_vibration's own contract.
        return """  const std::uint32_t user_index = ctx.r3.u32;
  const std::uint16_t left_motor = PPC_LOAD_U16(ctx.r4.u32 + 0x0);
  const std::uint16_t right_motor = PPC_LOAD_U16(ctx.r4.u32 + 0x2);
  if (!ac6::native::native_guest_input_service().set_vibration(
          user_index, left_motor, right_motor)) {
    ctx.r3.u64 = 0x48fu;  // ERROR_DEVICE_NOT_CONNECTED
    return;
  }
  ctx.r3.u64 = 0u;
"""
    if name == "XamInputGetCapabilities":
        # r180: real signature is DWORD XamInputGetCapabilities(DWORD
        # dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCaps) --
        # struct-fill through r5. This XEX's own real call site
        # (0x82390d48) confirms two fields byte-for-byte against
        # Microsoft's own fixed XINPUT_CAPABILITIES layout: `lbz
        # r11,0x61(r1)` reads struct+0x1 (SubType) and `lhz r11,0x62(r1)`
        # reads struct+0x2 (Flags) -- exactly the real ABI's own offsets.
        # Only those two fields (plus Type, adjacent and unambiguous) are
        # filled with confirmed-shape values; the Gamepad/Vibration
        # capability sub-structs are not read by this XEX's own examined
        # call site, so they are left at 0 rather than asserted, matching
        # this project's own established discipline (r169's own
        # unconfirmed offsets).
        return """  const std::uint32_t user_index = ctx.r3.u32;
  if (!ac6::native::native_guest_input_service().is_connected(user_index)) {
    ctx.r3.u64 = 0x48fu;  // ERROR_DEVICE_NOT_CONNECTED
    return;
  }
  PPC_STORE_U8(ctx.r5.u32 + 0x0, 1u);   // XINPUT_DEVTYPE_GAMEPAD
  PPC_STORE_U8(ctx.r5.u32 + 0x1, 1u);   // XINPUT_DEVSUBTYPE_GAMEPAD
  PPC_STORE_U16(ctx.r5.u32 + 0x2, 0u);  // Flags: no FFB/wireless/voice claimed
  ctx.r3.u64 = 0u;
"""
    if name == "XamInputGetKeystrokeEx":
        # r181: real signature is DWORD XamInputGetKeystrokeEx(DWORD
        # dwUserIndex, DWORD dwFlags, PXINPUT_KEYSTROKE pKeystroke) --
        # polls a queue of menu-navigation "virtual key" events (D-pad/
        # button presses treated as keystrokes for UI navigation), a
        # different real contract from GetState/SetState/GetCapabilities
        # (r180): the normal, expected steady-state answer is
        # ERROR_EMPTY (0x4306, no new keystroke queued), not
        # ERROR_SUCCESS with populated output. This XEX's own one real
        # call site (0x82390de0, a thin any-user-index-normalizing
        # wrapper) never inspects the return value itself -- the
        # generic offline fallback's kOfflineStatus (0xC00000BB) is a
        # nonsensical NT status for this Win32-error-shaped API either
        # way, so this corrects the shape without asserting any
        # particular keystroke queue behavior this project has not
        # implemented (no press/release edge tracking exists yet --
        # naming that gap here rather than fabricating queued events).
        return "  ctx.r3.u64 = 0x4306u;  // ERROR_EMPTY\n"
    if name == "XamUserCheckPrivilege":
        # r182: real signature is DWORD XamUserCheckPrivilege(DWORD
        # dwUserIndex, DWORD dwPrivilegeType, LPBOOL pfResult) -- a
        # struct-fill (the bool) PLUS a real status return, not just a
        # discarded status. This XEX's own real call sites confirm both
        # halves of the contract are consumed:
        #   0x82206bcc/0x82206bf0 (a restriction-flag gate) check the
        #   return for equality against 0 (ERROR_SUCCESS) with
        #   `cmplwi r3,0x0; bne <treat as restricted/skip>`, then read
        #   back the bool at r5 (`lwz r11,0x60(r1)`) and again branch the
        #   SAME way when it is 1 (granted) as when the call failed --
        #   only "query succeeded AND privilege denied" takes the other
        #   path.
        #   0x821f44ac (the sign-in-resolution helper r176 fixed) returns
        #   this call's raw result as ITS OWN return value with no
        #   further check -- kOfflineStatus (0xC00000BB) is a nonsensical
        #   NT status handed straight to that function's own caller in
        #   place of a real Win32 error code.
        # Fixed: ERROR_SUCCESS (0) with the bool set to TRUE (privilege
        # granted) -- consistent with this project's own established
        # single, unrestricted offline-profile assumption (r176's
        # "signed in locally, not to Live" reasoning), not a value chosen
        # to force either branch at the one call site where the outcome
        # actually differs.
        return """  if (ctx.r5.u32 != 0u) PPC_STORE_U32(ctx.r5.u32, 1u);
  ctx.r3.u64 = 0u;
"""
    if name == "RtlImageXexHeaderField":
        # r183: real signature is PVOID RtlImageXexHeaderField(PVOID
        # XexHeaderBase, DWORD ImageFlags) -- unlike almost every other
        # import in this file, the RETURN VALUE ITSELF is the field
        # pointer (0 means "not present"), not a status code at all. This
        # XEX's own two real call sites both then DEREFERENCE that
        # pointer when it is nonzero:
        #   0x821f7d88 (field 0x20401): `cmplwi r3,0x0; beq <default>` --
        #   if nonzero, `lwz r30,0x0(r3)` dereferences it immediately.
        #   0x82390e40 (field 0x40006): the raw return is stored directly
        #   as an output field's own value and later treated as present.
        # The generic offline-import fallback's kOfflineStatus
        # (0xC00000BB) is nonzero, so BOTH call sites currently treat an
        # unimplemented, non-existent field as "found" and dereference
        # 0xC00000BB as a guest pointer -- a real crash risk, not a
        # cosmetic gap. This project has no reachable evidence that either
        # optional header field is actually present in this XEX's own
        # header (no header-field-table parser exists yet under
        # `native/`), so the honest, safe answer is "not present": both
        # call sites' own default-handling paths for that case are
        # well-defined and exercised deliberately, not merely tolerated.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XeCryptSha":
        # r184: real signature is VOID XeCryptSha(const BYTE* pbInput1,
        # DWORD cbInput1, const BYTE* pbInput2, DWORD cbInput2, const
        # BYTE* pbInput3, DWORD cbInput3, BYTE* pbDigest, DWORD
        # cbDigestSize) -- confirmed by this XEX's own real call site
        # (0x82390f04): all 8 integer argument registers (r3..r10) are
        # populated, and r10 (cbDigestSize) is 0x14 -- exactly the SHA-1
        # digest length, not a status code. Real contract's return value
        # is not checked by this XEX's own call site.
        #
        # The generic offline-import fallback previously wrote nothing
        # through pbDigest at all. At 0x82390f04, the computed digest
        # feeds a comparison (`bl 0x823d0abc` immediately after, using
        # the digest as an input) whose result gates real control flow --
        # a garbage/absent digest would fail every such comparison
        # against a real reference hash. This computes the ACTUAL SHA-1
        # (OpenSSL EVP, already linked and used the same way for this
        # file's AES-CBC XEX decode, native/src/native_xex.cpp) over
        # whatever real guest bytes are present, not a fabricated digest
        # -- the one case in this file where "real value" has no
        # ambiguity to resolve, since the algorithm computes it exactly.
        return """  EVP_MD_CTX* sha1_ctx = EVP_MD_CTX_new();
  if (sha1_ctx != nullptr && EVP_DigestInit_ex(sha1_ctx, EVP_sha1(), nullptr) == 1) {
    if (ctx.r4.u32 != 0u) {
      EVP_DigestUpdate(sha1_ctx, base + ctx.r3.u32, ctx.r4.u32);
    }
    if (ctx.r6.u32 != 0u) {
      EVP_DigestUpdate(sha1_ctx, base + ctx.r5.u32, ctx.r6.u32);
    }
    if (ctx.r8.u32 != 0u) {
      EVP_DigestUpdate(sha1_ctx, base + ctx.r7.u32, ctx.r8.u32);
    }
    std::array<std::uint8_t, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_length = 0;
    if (EVP_DigestFinal_ex(sha1_ctx, digest.data(), &digest_length) == 1) {
      const std::uint32_t copy_length =
          std::min<std::uint32_t>(ctx.r10.u32, digest_length);
      for (std::uint32_t i = 0; i < copy_length; ++i) {
        PPC_STORE_U8(ctx.r9.u32 + i, digest[i]);
      }
    }
  }
  if (sha1_ctx != nullptr) EVP_MD_CTX_free(sha1_ctx);
"""
    if name in {"NtCreateFile", "NtOpenFile"}:
        # r122/r123/r129 (NtCreateFile); r200 extends this to NtOpenFile
        # (9 real call sites, e.g. 0x821f7308) after confirming its real
        # 6-arg signature (PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES,
        # PIO_STATUS_BLOCK, ULONG ShareAccess, ULONG OpenOptions) puts
        # FileHandle/ObjectAttributes/IoStatusBlock at the exact same
        # r3/r5/r6 positions NtCreateFile already uses -- this project's
        # media service never distinguishes create-vs-open (it is
        # read-only, so "open" is the only real operation either import
        # performs), so the identical body is a direct consequence of the
        # identical register contract, not an assumption.
        #
        # The real 9-arg NT signature for NtCreateFile, but this XEX's own
        # call sites only ever populate the first 8 (r3..r10) --
        # CreateOptions (the 9th, stack-passed) is unread here, matching
        # every other stub in this file. ObjectAttributes (r5) is the
        # reduced 3-field Xbox 360 layout r123 confirmed byte-by-byte:
        # {RootDirectory:u32, ObjectName:ptr-to-ANSI_STRING, Attributes:u32}.
        # The ANSI_STRING's own {Length:u16, MaximumLength:u16, Buffer:ptr}
        # layout is also byte-confirmed (r123) from a real "\Device\..."
        # string read directly off the XEX. r129 found a real title path
        # ("game:\DATA00.PAC") flows through this exact structure shape,
        # and confirmed that file genuinely exists on this project's own
        # already-qualified retail ISO -- this is not fabricated content,
        # it opens whatever media the runtime was actually booted against
        # (native_guest_media_service(), bound once at NativeRuntime::boot()
        # to the same MediaInput every other boot-time read already uses).
        return f"""  const std::uint32_t object_attributes = ctx.r5.u32;
  const std::uint32_t object_name = object_attributes != 0u
      ? PPC_LOAD_U32(object_attributes + 4u) : 0u;
  std::uint32_t status = 0xC0000034u;  // STATUS_OBJECT_NAME_NOT_FOUND
  std::uint32_t handle = 0u;
  if (object_name != 0u) {{
    const std::uint16_t length = PPC_LOAD_U16(object_name + 0u);
    const std::uint32_t buffer = PPC_LOAD_U32(object_name + 4u);
    if (buffer != 0u) {{
      const std::string_view raw(
          reinterpret_cast<const char*>(base + buffer), length);
      const std::string relative = guest_path_to_relative(raw);
      const std::optional<std::uint32_t> opened =
          ac6::native::native_guest_media_service().open_file(relative);
      if (opened.has_value()) {{
        handle = *opened;
        status = 0u;  // STATUS_SUCCESS
      }}
      if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {{
        std::fprintf(stderr, "[{name}] \\"%s\\" -> %s\\n",
                      relative.c_str(), opened.has_value() ? "ok" : "not found");
      }}
    }}
  }}
  if (ctx.r3.u32 != 0u) PPC_STORE_U32(ctx.r3.u32, handle);
  if (ctx.r6.u32 != 0u) {{
    PPC_STORE_U32(ctx.r6.u32 + 0u, status);
    PPC_STORE_U32(ctx.r6.u32 + 4u, 0u);
  }}
  ctx.r3.u64 = status;
"""
    if name == "NtReadFile":
        # r124: real signature confirmed live from the one guest caller
        # this project has instrumented -- r3=Handle, r7=&IoStatusBlock,
        # r8=Buffer, r9=Length, r10=&ByteOffset (a 64-bit big-endian pair).
        # Completes synchronously with a real STATUS_SUCCESS/STATUS_END_OF_FILE
        # rather than STATUS_PENDING -- r125/r126 named the risk of an
        # unconditional STATUS_PENDING stub (the caller's retry counter
        # only decrements on a recognized failure, never on pending, so a
        # stub that never truly completes would hang instead of crash);
        # this harness has no real DMA to model asynchronously, so the
        # honest offline behavior is to finish the read immediately.
        # r240/r241: the PAC path still fails with handle=0x829xxxxx (image
        # FILE_OBJECT, not small HANDLE) and offset=0xfefefefe (stack
        # garbage, not 0). Static trace (r241) proved the two direct
        # `bl NtReadFile` sites in this XEX (0x82391020 in Function_82390F48,
        # 0x82391b58 in Function_82391A40) both pass r4=r5=r6=0, r7/r8/r10 =
        # stack buffers, r9=0x400, ByteOffset 0x800 -- confirmed in the
        # active generated code too. A call with r4=0x134/r8=0/r9=0x40000
        # therefore comes from no direct site; the lr/r1 trace below names
        # the true caller on the next honest probe instead of guessing.
        # No fallback: a fabricated STATUS_SUCCESS with a wrong buffer or
        # offset would corrupt PAC data or write to the wrong place.
        return """  std::uint64_t offset = 0u;
  if (ctx.r10.u32 != 0u) {
    const std::uint32_t hi = PPC_LOAD_U32(ctx.r10.u32 + 0u);
    const std::uint32_t lo = PPC_LOAD_U32(ctx.r10.u32 + 4u);
    offset = (static_cast<std::uint64_t>(hi) << 32) | lo;
  }
  std::uint32_t bytes_read = 0u;
  std::uint8_t* dest = ctx.r8.u32 != 0u ? (base + ctx.r8.u32) : nullptr;
  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    std::fprintf(stderr, "[NtReadFile] r1=0x%08x r3=0x%08x r4=0x%08x r5=0x%08x r6=0x%08x r7=0x%08x r8=0x%08x r9=0x%08x r10=0x%08x offset=%llu tid=%ld\\n",
                 ctx.r1.u32,
                 ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32,
                 static_cast<unsigned long long>(offset),
                 static_cast<long>(gettid()));
    if (ctx.r3.u32 != 0u && ctx.r3.u32 >= 0x82000000u && ctx.r3.u32 < 0x83000000u) {
      std::fprintf(stderr, "[NtReadFile] *r3 dump:");
      for (int i=0;i<16;i++) std::fprintf(stderr, " %08x", PPC_LOAD_U32(ctx.r3.u32 + i*4));
      std::fprintf(stderr, "\\n");
    }
    if (ctx.r10.u32 != 0u && ctx.r10.u32 >= 0x82000000u && ctx.r10.u32 < 0x90000000u) {
      std::fprintf(stderr, "[NtReadFile] *r10 dump: %08x %08x\\n",
                   PPC_LOAD_U32(ctx.r10.u32), PPC_LOAD_U32(ctx.r10.u32+4));
    }
    // r275: name the caller without ctx.lr (PPC_CONFIG_SKIP_LR is defined).
    // The guest frame chain keeps the return address at *(r1+4) and the
    // parent frame at *(r1); dump three levels so the true NtReadFile
    // caller on the entry thread is attributed from evidence, not guessed.
    std::uint32_t frame = ctx.r1.u32;
    for (int depth = 0; depth < 3 && frame != 0u && frame >= 0x82000000u &&
                        frame < 0x90000000u; ++depth) {
      std::fprintf(stderr, "[NtReadFile] backchain[%d] lr=0x%08x next_frame=0x%08x\\n",
                   depth, PPC_LOAD_U32(frame + 4u), PPC_LOAD_U32(frame));
      frame = PPC_LOAD_U32(frame);
    }
    // r542: the guest backchain above reads null (generated code calls
    // with native returns, no guest LR kept) -- resolve the HOST return
    // address instead; generated functions carry __imp__sub_XXXXXXXX
    // symbols, naming the true caller directly. Bounded: IMPORT_TRACE
    // gate only, one frame, no allocation.
    {
      void* host_caller = __builtin_return_address(0);
      Dl_info caller_info{};
      if (dladdr(host_caller, &caller_info) != 0 &&
          caller_info.dli_sname != nullptr) {
        std::fprintf(stderr, "[NtReadFile] host_caller=%s+0x%llx\\n",
                     caller_info.dli_sname,
                     static_cast<unsigned long long>(
                         static_cast<char*>(host_caller) -
                         static_cast<char*>(caller_info.dli_saddr)));
      } else {
        std::fprintf(stderr, "[NtReadFile] host_caller=%p unresolved\\n",
                     host_caller);
      }
    }
    // r275: the PAC read pipeline's producers. 0x82778EB0 = the arena global
    // written by sub_821D5F48 (r24-29008) before it allocates the queue
    // buffer; 0x8293B930/94C/938 = the pool cluster read by sub_821CC508;
    // the queue header window is derived from the async block (r7-22860).
    std::fprintf(stderr, "[NtReadFile] globals: arena=0x%08x pool_obj=0x%08x pool=0x%08x gate=%u initcnt=%u\\n",
                 PPC_LOAD_U32(0x82758EB0u), PPC_LOAD_U32(0x8293B930u),
                 PPC_LOAD_U32(0x8293B94Cu), PPC_LOAD_U32(0x8293B938u) & 0xFFu,
                 PPC_LOAD_U32(0x8293B950u));
    if (ctx.r7.u32 >= 22860u && ctx.r7.u32 < 0x90000000u) {
      const std::uint32_t q = ctx.r7.u32 - 22860u;
      std::fprintf(stderr, "[NtReadFile] queue@0x%08x: r316=%08x r320=%08x r324=%08x r328=%08x r332=%08x r340=%08x\\n",
                   q, PPC_LOAD_U32(q + 316), PPC_LOAD_U32(q + 320), PPC_LOAD_U32(q + 324),
                   PPC_LOAD_U32(q + 328), PPC_LOAD_U32(q + 332), PPC_LOAD_U32(q + 340));
    }
  }
  const bool known_handle = ac6::native::native_guest_media_service().read_file(
      ctx.r3.u32, offset, dest, ctx.r9.u32, bytes_read);
  std::uint32_t status = 0xC0000008u;  // STATUS_INVALID_HANDLE
  if (known_handle) {
    status = (bytes_read == 0u && ctx.r9.u32 != 0u)
        ? 0xC0000011u   // STATUS_END_OF_FILE
        : 0u;           // STATUS_SUCCESS
  } else if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr ||
             std::getenv("AC6_NATIVE_NTREADFILE_ERR") != nullptr) {
    // r587: dedicated, rate-limited invalid-handle trace so the PAC
    // handle (r240/r241: 0x829xxxxx) can be captured in a short run
    // without the 73 GB IMPORT_TRACE flood. r4 = Event handle (advisor
    // note: a non-NULL Event should be signaled on completion).
    static std::atomic<unsigned> err_count{0u};
    if (err_count.fetch_add(1u) < 64u) {
      std::fprintf(stderr,
                   "[NtReadFile] invalid handle=0x%08x r4=0x%08x offset=%llu len=%u tid=%ld\\n",
                   ctx.r3.u32, ctx.r4.u32,
                   static_cast<unsigned long long>(offset), ctx.r9.u32,
                   static_cast<long>(gettid()));
    }
  }
  if (ctx.r7.u32 != 0u) {
    PPC_STORE_U32(ctx.r7.u32 + 0u, status);
    PPC_STORE_U32(ctx.r7.u32 + 4u, bytes_read);
  }
  ctx.r3.u64 = status;
"""
    if name == "NtCreateEvent":
        return """  const std::uint32_t handle = g_next_handle.fetch_add(1u);
  if (ctx.r3.u32 != 0u) PPC_STORE_U32(ctx.r3.u32, handle);
  const bool manual_reset = ctx.r6.u32 == 0u;
  create_event(handle, manual_reset, ctx.r7.u32 != 0u);
  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    // r285 (diagnostic only): creation is rare (not a hot loop), so this
    // is safe to print unconditionally under the trace gate -- confirms
    // whether a specific handle is manual- or auto-reset without needing
    // to inspect g_events by hand.
    std::fprintf(stderr, "[NtCreateEvent] handle=0x%08x manual_reset=%d initial=%d\\n",
                 handle, manual_reset ? 1 : 0, ctx.r7.u32 != 0u ? 1 : 0);
  }
  ctx.r3.u64 = 0u;
"""
    if name == "NtCreateSemaphore":
        # r145: real signature (documented NT API, confirmed against this
        # XEX's own call site sub_821F5798, disassembled directly) is
        # NtCreateSemaphore(OUT PHANDLE, IN POBJECT_ATTRIBUTES OPTIONAL,
        # IN LONG InitialCount, IN LONG MaximumCount) -- r5=InitialCount,
        # r6=MaximumCount. The generic handle-allocation stub this import
        # previously shared with NtCreateTimer/NtCreateMutant never
        # registered the handle in g_events, so any NtWaitForSingleObjectEx
        # on a semaphore was a guaranteed, deterministic STATUS_TIMEOUT
        # regardless of real NtReleaseSemaphore activity elsewhere in the
        # guest -- traced live to sub_82338388's own wait (r142) always
        # timing out on exactly this handle class. A semaphore's wait
        # contract (one permit consumed per successful wait) matches this
        # project's existing auto-reset event model directly.
        return """  const std::uint32_t handle = g_next_handle.fetch_add(1u);
  if (ctx.r3.u32 != 0u) PPC_STORE_U32(ctx.r3.u32, handle);
  create_event(handle, /*manual_reset=*/false, /*signaled=*/ctx.r5.s32 > 0);
  ctx.r3.u64 = 0u;
"""
    if name == "NtCreateMutant":
        # r286: real signature is NtCreateMutant(OUT PHANDLE, IN
        # POBJECT_ATTRIBUTES OPTIONAL, IN BOOLEAN InitialOwner) -- r5 is
        # InitialOwner. Confirmed live (diagnostic trace, kept -- creation
        # is rare, not a hot loop) that this XEX's own real call site
        # passes InitialOwner=FALSE (unowned at creation); registers in
        # g_mutants either way. Previously this stub registered nothing at
        # all, so a real wait on it (NtWaitForSingleObjectEx or the wait
        # side of NtSignalAndWaitForSingleObjectEx) failed instantly (a
        # map-lookup miss) instead of correctly contending for ownership,
        # and NtReleaseMutant/the signal side of NtSignalAndWaitForSingle-
        # ObjectEx had nothing to act on.
        return """  const std::uint32_t handle = g_next_handle.fetch_add(1u);
  if (ctx.r3.u32 != 0u) PPC_STORE_U32(ctx.r3.u32, handle);
  {
    // r287: per-key MutantState, registered under the registry lock only
    // (held briefly, for the insert) -- see g_mutants above.
    auto mutant = std::make_unique<MutantState>();
    if (ctx.r5.u32 != 0u) {
      mutant->owned = true;
      mutant->owner_tid = current_thread_tid();
      mutant->recursion_count = 1u;
    }
    std::lock_guard lock(g_mutants_registry_mutex);
    g_mutants[handle] = std::move(mutant);
  }
  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    std::fprintf(stderr, "[NtCreateMutant] handle=0x%08x initial_owner=%d\\n",
                 handle, ctx.r5.u32 != 0u ? 1 : 0);
  }
  ctx.r3.u64 = 0u;
"""
    if name == "NtCreateTimer":
        # r216: registers the handle in g_events (auto-reset, matching
        # this XEX's own real NtSetTimerEx call site forcing
        # TimerType=SynchronizationTimer=1) so NtWaitForSingleObjectEx
        # on it actually observes NtSetTimerEx's real fire via
        # set_timer()/set_event() -- previously shared the generic
        # handle-only stub with NtCreateMutant, which never registered
        # anything, so a real wait on a real timer was a guaranteed
        # STATUS_TIMEOUT regardless of the timer ever firing.
        return """  const std::uint32_t handle = g_next_handle.fetch_add(1u);
  if (ctx.r3.u32 != 0u) PPC_STORE_U32(ctx.r3.u32, handle);
  create_event(handle, /*manual_reset=*/false, /*signaled=*/false);
  ctx.r3.u64 = 0u;
"""
    if name == "NtSetEvent":
        return """  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    std::fprintf(stderr, "[NtSetEvent] target=0x%08x r4=0x%08x\\n",
                 ctx.r3.u32, ctx.r4.u32);
  }
  if (is_guest_object_key(ctx.r3.u32)) {
    set_guest_object(base, ctx.r3.u32);
    ctx.r3.u64 = 0u;
    return;
  }
  const bool previous = set_event(ctx.r3.u32);
  if (ctx.r4.u32 != 0u) PPC_STORE_U32(ctx.r4.u32, previous ? 1u : 0u);
  ctx.r3.u64 = 0u;
"""
    if name == "NtClearEvent":
        return """  clear_event(ctx.r3.u32);
  ctx.r3.u64 = 0u;
"""
    if name == "NtPulseEvent":
        return """  const bool previous = set_event(ctx.r3.u32);
  clear_event(ctx.r3.u32);
  if (ctx.r4.u32 != 0u) PPC_STORE_U32(ctx.r4.u32, previous ? 1u : 0u);
  ctx.r3.u64 = 0u;
"""
    if name == "NtClose":
        return """  // r499: wake this key's waiters before erasing -- their predicates
  // see the erased key as claimable and return; the shared_ptr they
  // copied keeps the condvar alive until they are done with it.
  {
    std::lock_guard lock(g_event_mutex);
    auto it = g_events.find(ctx.r3.u32);
    if (it != g_events.end()) {
      it->second->cv.notify_all();
      g_events.erase(it);
    }
  }
  ctx.r3.u64 = 0u;
"""
    if name == "KeResetEvent":
        return """  const bool previous = clear_event(ctx.r3.u32);
  ctx.r3.u64 = previous ? 1u : 0u;
"""
    if name == "KeSetEvent":
        return """  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    std::fprintf(stderr, "[KeSetEvent] target=0x%08x r4=0x%08x\\n",
                 ctx.r3.u32, ctx.r4.u32);
  }
  if (is_guest_object_key(ctx.r3.u32)) {
    set_guest_object(base, ctx.r3.u32);
    ctx.r3.u64 = 0u;
    return;
  }
  const bool previous = set_event(ctx.r3.u32);
  ctx.r3.u64 = previous ? 1u : 0u;
"""
    if name == "NtSignalAndWaitForSingleObjectEx":
        # r284 (diagnostic addition): trace the signal/wait handle pair and
        # thread id -- bounded by the same AC6_NATIVE_IMPORT_TRACE gate as
        # every other stub; this import's own guest-side retry loop caps
        # call volume the same way NtWaitForSingleObjectEx's does.
        #
        # r286: the "signal" side previously called the generic
        # `set_event()` on ANY handle, silently fabricating a phantom
        # auto-reset EventState (via `g_events[key]`'s auto-vivifying
        # `operator[]`) for a real Mutant this project's own r283/r284/r285
        # tracing confirmed is used here -- with no ownership check, no
        # STATUS_MUTANT_NOT_OWNED, and no actual release semantics. A
        # Mutant must be released, not "set"; and the wait side must
        # acquire it, not just observe a boolean.
        return """  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    std::fprintf(stderr, "[NtSignalAndWaitForSingleObjectEx] signal=0x%08x wait=0x%08x tid=%zu\\n",
                 ctx.r3.u32, ctx.r4.u32,
                 std::hash<std::thread::id>{}(std::this_thread::get_id()));
  }
  if (is_mutant(ctx.r3.u32)) {
    // r286 (revised, same cycle): this XEX's own guest code never checks
    // this call's return status before proceeding straight to the wait
    // side regardless (traced: sub_82345CE0 falls through to its plain
    // wait unconditionally) -- an early return here on a failed release
    // would silently skip that wait, a real behavior change from the
    // prior always-succeeds model that empirically made no observable
    // difference to the traced stall either way. release_mutant() is
    // still attempted (real bookkeeping when this thread IS the tracked
    // owner) but never blocks reaching the wait side.
    release_mutant(ctx.r3.u32);
  } else if (is_guest_object_key(ctx.r3.u32)) {
    set_guest_object(base, ctx.r3.u32);
  } else {
    set_event(ctx.r3.u32);
  }
  if (is_mutant(ctx.r4.u32)) {
    ctx.r3.u64 = wait_mutant(ctx.r4.u32, ctx.r5.u32) ? 0u : 0x102u;
    return;
  }
  if (is_guest_object_key(ctx.r4.u32)) {
    ctx.r3.u64 = wait_guest_object(base, ctx.r4.u32) ? 0u : 0x102u;
    return;
  }
  if (!wait_event(ctx.r4.u32, ctx.r5.u32)) {
    ctx.r3.u64 = 0x102u;  // STATUS_TIMEOUT; offline event pair not signaled
    return;
  }
  ctx.r3.u64 = 0u;
"""
    if name == "NtReleaseMutant":
        # r286: NtReleaseMutant(handle, PreviousCount*) -- r4 is the
        # optional out pointer for the previous count. The "single guest
        # thread, no real contention" premise this stub used to document
        # was already disproven engine-wide by r111-r115's confirmed real
        # concurrent host threads; for THIS specific handle class it was
        # additionally moot, since NtCreateMutant never registered
        # anything for this stub to act on. Now releases the real,
        # tracked Mutant (see g_mutants above): only the current owner may
        # release, matching the real STATUS_MUTANT_NOT_OWNED contract
        # instead of always succeeding regardless of caller.
        return """  const bool released = release_mutant(ctx.r3.u32);
  if (released) {
    if (ctx.r4.u32 != 0u) PPC_STORE_U32(ctx.r4.u32, 0u);
    ctx.r3.u64 = 0u;
  } else {
    ctx.r3.u64 = 0xC0000046u;  // STATUS_MUTANT_NOT_OWNED
  }
"""
    if name == "NtReleaseSemaphore":
        # r145: the real signature (documented NT API) is
        # NtReleaseSemaphore(HANDLE, LONG ReleaseCount, PLONG PreviousCount)
        # -- r4 is ReleaseCount, an integer, NOT a pointer; r5 is the actual
        # PreviousCount* out pointer. The previous, shared stub with
        # NtReleaseMutant wrote through r4 for both, which for a semaphore
        # meant writing PPC_STORE_U32 to whatever small integer ReleaseCount
        # happened to be -- a real, separate bug from the missing g_events
        # registration this cycle also fixes (NtCreateSemaphore, above).
        # This project's existing auto-reset event model has no notion of a
        # semaphore's real count beyond signaled/not, so PreviousCount is
        # still reported as 0 -- unchanged in spirit from the prior stub,
        # just through the correct register.
        return """  set_event(ctx.r3.u32);
  if (ctx.r5.u32 != 0u) PPC_STORE_U32(ctx.r5.u32, 0u);
  ctx.r3.u64 = 0u;
"""
    if name in {"NtWaitForSingleObjectEx", "NtWaitForMultipleObjectsEx",
                "KeWaitForSingleObject", "KeWaitForMultipleObjects"}:
        # r280: the four imports share one body but must trace under their
        # own names -- r278/r279 read "[NtWaitForSingleObjectEx]" lines that
        # this shared label also printed for the three other imports.
        return ("""  // r277: shutdown stop -- end this worker through its entry catch.
  if (ac6::native::native_guest_threads_stop_requested()) {
    throw ac6::native::GuestThreadTerminated{};
  }
  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    // r284: thread id added to attribute a flood of retries (STATUS_TIMEOUT
    // -> guest-side retry loop) to a specific thread without a per-thread
    // filter -- the print volume is unchanged, only the format string grew.
    std::fprintf(stderr, "[NtWaitForSingleObjectEx] handle=0x%08x r4=0x%08x tid=%zu\\n",
                 ctx.r3.u32, ctx.r4.u32,
                 std::hash<std::thread::id>{}(std::this_thread::get_id()));
  }
  // r286: a Mutant must be ACQUIRED by a wait, not merely observed as a
  // signaled/unsignaled boolean -- see g_mutants above.
  if (is_mutant(ctx.r3.u32)) {
    ctx.r3.u64 = wait_mutant(ctx.r3.u32, ctx.r4.u32) ? 0u : 0x102u;
    return;
  }
  if (is_guest_object_key(ctx.r3.u32)) {
    ctx.r3.u64 = wait_guest_object(base, ctx.r3.u32) ? 0u : 0x102u;
    return;
  }
  if (!wait_event(ctx.r3.u32, ctx.r4.u32)) {
    ctx.r3.u64 = 0x102u;  // STATUS_TIMEOUT; offline, non-blocking
    return;
  }
  ctx.r3.u64 = 0u;
""").replace("[NtWaitForSingleObjectEx]", "[" + name + "]")
    if name == "KeTryToAcquireSpinLockAtRaisedIrql":
        # r192: BOOLEAN KeTryToAcquireSpinLockAtRaisedIrql(PKSPIN_LOCK
        # SpinLock) -- same lock object identity as KfAcquireSpinLock
        # (r191), non-blocking variant. Real call site 0x823a8bf4 masks
        # the return through rlwinm to its low 8 bits, confirming a
        # BOOLEAN, not a status.
        return "  ctx.r3.u64 = spin_lock_for(ctx.r3.u32).try_lock() ? 1u : 0u;\n"
    if name == "KeInitializeSemaphore":
        # r192: VOID KeInitializeSemaphore(PKSEMAPHORE Semaphore, LONG
        # Count, LONG Limit) -- real call site 0x823add7c initializes a
        # standard KSEMAPHORE dispatcher-object header (self-referential
        # list at Semaphore+0x8) before this call. Same auto-reset event
        # model r145 already uses for NtCreateSemaphore, keyed by the
        # Semaphore object's own guest address like KeSetEvent/KeResetEvent
        # (Ke* variants operate on the object directly, not a handle).
        return """  create_event(ctx.r3.u32, /*manual_reset=*/false,
               /*signaled=*/ctx.r4.s32 > 0);
  ctx.r3.u64 = 0u;
"""
    if name == "KeReleaseSemaphore":
        # r192: LONG KeReleaseSemaphore(PKSEMAPHORE Semaphore, KPRIORITY
        # Increment, LONG Adjustment, BOOLEAN Wait) -- real call sites
        # 0x823ad268/0x823ad8d4 confirm r3=Semaphore, r4=Increment=1.
        # Unlike NtReleaseSemaphore (r145), the previous count is the
        # function's own return value (r3), not an out-pointer. Same
        # "previous count unmodeled, always 0" precedent as r145: this
        # project's event model has no notion of a semaphore's real count
        # beyond signaled/not.
        return """  set_event(ctx.r3.u32);
  ctx.r3.u64 = 0u;
"""
    if name == "KeBugCheck":
        # r193: VOID KeBugCheck(ULONG BugCheckCode) -- documented, never
        # returns (a real hardware halt). 6 real calls plus 1 real tail
        # jump across this XEX (0x8238329c, 0x82383344, 0x821ed328,
        # 0x821ed47c, 0x82389a88, 0x82386cd0, 0x823831dc). The generic
        # offline no-op previously returned normally with kOfflineStatus,
        # letting the recompiled guest fall through into code the real
        # console would never reach after a bugcheck -- a genuine
        # execute-past-fatal risk, not a status-shape cosmetic. Aborting
        # here instead matches real "never returns" semantics rather than
        # inventing a continuation this XEX's own compiled code does not
        # expect on real hardware.
        return """  std::fprintf(stderr, "[KeBugCheck] fatal stop code=0x%08x\\n",
               static_cast<unsigned>(ctx.r3.u32));
  std::abort();
"""
    if name == "KeBugCheckEx":
        # r193: VOID KeBugCheckEx(ULONG BugCheckCode, ULONG_PTR Parameter1,
        # ULONG_PTR Parameter2, ULONG_PTR Parameter3, ULONG_PTR Parameter4)
        # -- same never-returns contract as KeBugCheck, 4 real call sites
        # (0x821fa75c, 0x821f9e74, 0x821faa4c, 0x821f90f4).
        return """  std::fprintf(stderr,
               "[KeBugCheckEx] fatal stop code=0x%08x p1=0x%08x p2=0x%08x "
               "p3=0x%08x p4=0x%08x\\n",
               static_cast<unsigned>(ctx.r3.u32),
               static_cast<unsigned>(ctx.r4.u32),
               static_cast<unsigned>(ctx.r5.u32),
               static_cast<unsigned>(ctx.r6.u32),
               static_cast<unsigned>(ctx.r7.u32));
  std::abort();
"""
    if name == "KeDelayExecutionThread":
        # r194: NTSTATUS KeDelayExecutionThread(KPROCESSOR_MODE WaitMode,
        # BOOLEAN Alertable, PLARGE_INTEGER Interval) -- single real call
        # site 0x821f74e8, inside a wrapper (0x821f7498) that converts a
        # millisecond count to a negative (relative) 100ns LARGE_INTEGER
        # itself before this call, or a fixed 0x8000000000000000 sentinel
        # for an infinite/"forever" wait when passed -1ms. That caller
        # normalizes every return value to 0 or 0xC0 (STATUS_USER_APC),
        # and the generic offline no-op already produced 0 there by
        # accident (kOfflineStatus matches neither compared constant) --
        # so the observable status was never the bug. The real bug is
        # timing: the no-op returned instantly instead of actually
        # delaying, collapsing a real wait into zero elapsed time. Only
        # the relative (negative) form is handled, since that is the only
        # form this XEX's own real call site produces; a positive
        # (absolute) Interval is left unhandled rather than guessed.
        return """  if (ac6::native::native_guest_threads_stop_requested()) {
    throw ac6::native::GuestThreadTerminated{};
  }
  if (ctx.r5.u32 != 0u) {
    const std::int64_t interval =
        static_cast<std::int64_t>(PPC_LOAD_U64(ctx.r5.u32));
    if (interval < 0) {
      // r516: sliced sleep so shutdown stop is observed mid-delay. The
      // previous single sleep_for() never checked stop: the -1ms
      // sentinel (INT64_MIN, ~29 kyr) slept past shutdown and hung
      // stop_and_join() whenever a worker parked here (wedged-run
      // rc=137 class — the hang sits in worker join, located r515).
      // Slices preserve the delay length; stop throws out of it.
      // r543: delay-site attribution (THREAD_SAMPLE gate, bounded).
      // Sleeps are the last uninstrumented parking class: HLE waits
      // pair (r530), spins show in the sampler (r539), exits persist
      // threads. A worker parked here never appears in any of those.
      // Count per duration bucket; name the caller via the host return
      // address (same dladdr pattern as the r542 NtReadFile caller).
      constexpr std::int64_t kSliceMs = 100;
      std::int64_t remaining_ms =
          interval == (std::numeric_limits<std::int64_t>::min)()
              ? (std::numeric_limits<std::int64_t>::max)()
              : (-interval) / 10000;
      if (std::getenv("AC6_NATIVE_THREAD_SAMPLE") != nullptr) {
        static std::atomic<unsigned> delay_calls{0u};
        const unsigned ord = delay_calls.fetch_add(1u);
        const char* bucket = remaining_ms >= 3600000ll
                                 ? "long"
                                 : (remaining_ms >= 1000ll ? "medium"
                                                           : "short");
        if (ord < 16u || (ord % 2000u) == 0u) {
          void* delay_caller = __builtin_return_address(0);
          Dl_info delay_info{};
          // r544: the 5 ms loop in sub_821EA2F8 polls the shared quantum
          // counter at 0x826EB740 for EXACTLY 6144 (equality, not >=).
          // Sample it here so a miss (0? jumped past?) is visible live.
          const std::uint32_t quantum = PPC_LOAD_U32(0x826EB740u);
          // r544b: the worker-drain loop in sub_82379938 polls flag
          // [0x82915F64]==0 to exit (5 ms slices). Sample it alongside.
          const std::uint32_t drain_flag = PPC_LOAD_U32(0x82915F64u);
          // r546: the 7 persistent work items polled by sub_82379938
          // live at 0x82913B50 (816 B stride); their type words at +140
          // name the registered job types. Dump all 7 (bounded call
          // sites only — same gate as the rest of this line).
          std::uint32_t item_types[7] = {0u, 0u, 0u, 0u, 0u, 0u, 0u};
          for (unsigned item = 0u; item < 7u; ++item) {
            item_types[item] =
                PPC_LOAD_U32(0x82913B50u + item * 816u + 140u);
          }
          if (dladdr(delay_caller, &delay_info) != 0 &&
              delay_info.dli_sname != nullptr) {
            std::fprintf(stderr,
                         "r543 delay call=%u tid=%ld ms=%lld bucket=%s "
                         "quantum=0x%08x drainflag=0x%08x items=%08x,%08x,%08x,%08x,%08x,%08x,%08x caller=%s+0x%llx\\n",
                         ord, static_cast<long>(gettid()), remaining_ms,
                         bucket, quantum, drain_flag, item_types[0],
                         item_types[1], item_types[2], item_types[3],
                         item_types[4], item_types[5], item_types[6],
                         delay_info.dli_sname,
                         static_cast<unsigned long long>(
                             static_cast<char*>(delay_caller) -
                             static_cast<char*>(delay_info.dli_saddr)));
          } else {
            std::fprintf(stderr,
                         "r543 delay call=%u tid=%ld ms=%lld bucket=%s "
                         "quantum=0x%08x drainflag=0x%08x items=%08x,%08x,%08x,%08x,%08x,%08x,%08x caller=%p unresolved\\n",
                         ord, static_cast<long>(gettid()), remaining_ms,
                         bucket, quantum, drain_flag, item_types[0],
                         item_types[1], item_types[2], item_types[3],
                         item_types[4], item_types[5], item_types[6],
                         delay_caller);
          }
        }
      }
      while (remaining_ms > 0) {
        const std::int64_t slice =
            remaining_ms < kSliceMs ? remaining_ms : kSliceMs;
        std::this_thread::sleep_for(std::chrono::milliseconds(slice));
        if (ac6::native::native_guest_threads_stop_requested()) {
          throw ac6::native::GuestThreadTerminated{};
        }
        remaining_ms -= slice;
      }
    }
  }
  ctx.r3.u64 = 0u;
"""
    if name == "XamAlloc":
        # r195: DWORD XamAlloc(DWORD Type, SIZE_T Size, PVOID* pAddress) --
        # a Win32-style DWORD status (0 = ERROR_SUCCESS), not an NTSTATUS.
        # Real call site 0x821fd440 confirms r3=Type, r4=Size, r5=pAddress
        # (out): treats the return as signed and branches to its own error
        # path only when negative, so kOfflineStatus (0xC00000BB, negative
        # as signed 32-bit) always took that error path -- every one of
        # this XEX's 3 real call sites was a guaranteed allocation
        # failure. Reuses the existing allocate_guest bump allocator, same
        # as ExAllocatePool/MmAllocatePhysicalMemoryEx above.
        return """  const std::uint32_t address = allocate_guest(base, ctx.r4.u32);
  if (ctx.r5.u32 != 0u) PPC_STORE_U32(ctx.r5.u32, address);
  ctx.r3.u64 = address != 0u ? 0u : 0xeu;  // ERROR_OUTOFMEMORY
"""
    if name == "XamFree":
        # r195: DWORD XamFree(PVOID pAddress) -- same "guest reservation
        # lifetime owned by the bump allocator, never freed individually"
        # precedent as ExFreePool/RtlFreeAnsiString above.
        return "  ctx.r3.u64 = 0u;  // guest reservation is released at teardown\n"
    if name in {"ObCreateSymbolicLink", "ObDeleteSymbolicLink"}:
        # r196: NTSTATUS ObCreateSymbolicLink(POBJECT_STRING
        # SymbolicLinkName, POBJECT_STRING DeviceName)/NTSTATUS
        # ObDeleteSymbolicLink(POBJECT_STRING SymbolicLinkName) -- device
        # drive-letter mount/unmount registration during boot. Real call
        # site 0x821ea034 (inside Function_821E9F50) is a mount-candidate
        # retry loop: the generic offline no-op's kOfflineStatus (negative)
        # took its error branch unconditionally, so this loop could never
        # observe a successful mount through this path -- a real
        # boot-sequence blocker, not a cosmetic status. This project's own
        # path resolution (guest_path_to_relative, used by
        # NtCreateFile/NtOpenFile) never consults a registered symlink
        # table, so there is no real lookup this fix needs to back --
        # same "native side owns this subsystem's lifecycle" precedent as
        # VdRetrainEDRAM and friends above.
        return "  ctx.r3.u64 = 0u;\n"
    if name in {"KeLockL2", "KeUnlockL2"}:
        # r197: real hardware L2-cache-way locking, irrelevant to a
        # host-side interpreter (no cache-way partitioning to emulate).
        # Both real call sites (0x821eded0, 0x821eea94) discard the
        # return value outright -- the caller falls straight into the
        # next instruction with no check at all, so this is not even a
        # status-shape question, just noise from the generic fallback.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "KiApcNormalRoutineNop":
        # r197: literally a no-op by its own documented name/purpose --
        # the default "NormalRoutine" callback for a kernel APC that has
        # no real user-mode routine. Its single real call site
        # (0x821e6908) also discards the return value, confirming the
        # generic fallback's only actual defect here was the traced
        # kOfflineStatus noise, not observable behavior.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XamTaskShouldExit":
        # r198: BOOLEAN XamTaskShouldExit(VOID) -- no parameters (this
        # XEX's own real call site 0x82391950, inside Function_823917F8,
        # passes no explicit argument before the call). Queries whether
        # the calling XamTask worker should abort early. This project has
        # no XamTaskSchedule-driven worker execution yet (see r198's
        # report for why that stays a generic no-op), so there is no real
        # exit signal to report; kOfflineStatus previously read as
        # nonzero/"should exit", making that call site's worker loop
        # abort immediately on every iteration. FALSE (keep working) is
        # the honest default absent any real signaling mechanism.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "NtFlushBuffersFile":
        # r199: NTSTATUS NtFlushBuffersFile(HANDLE FileHandle,
        # PIO_STATUS_BLOCK IoStatusBlock). Both of this XEX's real call
        # sites (0x82392848, 0x8239130c) pass a handle and an
        # IoStatusBlock output pointer that neither call site reads back
        # afterward -- one checks only success/failure (`blt`), the other
        # discards the return entirely. This project's guest media is
        # read-only (r189/r190/r197): there is never a pending write to
        # flush, so unconditional success is the honest contract, not a
        # guess -- matching the read-only-media precedent already used
        # for NtQueryFullAttributesFile/NtQueryVolumeInformationFile.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XNotifyGetNext":
        # r200: BOOL XNotifyGetNext(HANDLE hNotification, DWORD
        # dwMsgFilter, PDWORD pdwId, PULARGE_INTEGER pParam) -- 4 real
        # call sites (0x82165868, 0x8215ca64, 0x821ce1c0, 0x82204590).
        # Real call site 0x82165868 confirms the contract: `cmpwi r3,0x0;
        # beq skip` treats zero as "no notification pending" and only
        # reads *pdwId when nonzero. kOfflineStatus is nonzero, so this
        # XEX's own real code was reading a notification id from a stack
        # slot this project's stub never wrote -- uninitialized memory
        # driving a real branch, on every single call, not just a
        # cosmetic status mismatch. This project has no real
        # notification queue to drain, so "no notification pending" (0)
        # is the honest, safe default -- same class of fix as r198's
        # XamTaskShouldExit.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XNotifyPositionUI":
        # r200: VOID XNotifyPositionUI(DWORD Position) -- cosmetic
        # (repositions the system notification popup on screen). Real
        # call site 0x821ce0ec discards the return value outright.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XAudioGetVoiceCategoryVolumeChangeMask":
        # r203: HRESULT XAudioGetVoiceCategoryVolumeChangeMask(DWORD
        # Handle, PDWORD pChangeMask) -- real call site 0x823ad1fc
        # confirms r3=Handle, r4=&pChangeMask; on success, the caller
        # loops over 2 volume categories testing bits of *pChangeMask to
        # decide whether to re-query each one via
        # XAudioGetVoiceCategoryVolume. This project has no real
        # volume-mixer subsystem to report changes from, so "nothing
        # changed" (mask 0) is the honest default -- and, as a direct
        # consequence, the per-category re-query this gates almost never
        # fires, which is the correct behavior absent a real change
        # source, not a workaround.
        return """  if (ctx.r4.u32 != 0u) PPC_STORE_U32(ctx.r4.u32, 0u);
  ctx.r3.u64 = 0u;
"""
    if name == "XAudioGetVoiceCategoryVolume":
        # r203: HRESULT XAudioGetVoiceCategoryVolume(DWORD CategoryIndex,
        # float* pVolume) -- real call site 0x823ad22c, gated by the
        # change-mask above. Full volume (1.0f) is the honest default
        # absent a real per-category mixer to read from.
        return """  if (ctx.r4.u32 != 0u) {
    constexpr float kFullVolume = 1.0f;
    std::uint32_t bits;
    std::memcpy(&bits, &kFullVolume, sizeof(bits));
    PPC_STORE_U32(ctx.r4.u32, bits);
  }
  ctx.r3.u64 = 0u;
"""
    if name in {"IoDismountVolume", "IoDismountVolumeByFileHandle"}:
        # r204: NTSTATUS IoDismountVolume(...)/NTSTATUS
        # IoDismountVolumeByFileHandle(HANDLE FileHandle). Real call
        # sites (IoDismountVolume: 0x82391984, 0x823919b8, inside
        # Function_823917F8 alongside r198's XamTaskShouldExit fix;
        # IoDismountVolumeByFileHandle: 0x82392d6c, the unconditional
        # cleanup tail of r202's traced save-write function, reached on
        # both the success and failure path) all discard the return
        # value outright -- no check at all, same class of fix as
        # r197's KeLockL2/KeUnlockL2/KiApcNormalRoutineNop.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XamNotifyCreateListener":
        # r205: HANDLE XamNotifyCreateListener(ULONGLONG qwAreas) -- the
        # real API returns a HANDLE, not an NTSTATUS. Real call site
        # 0x82204f08 checks the result with `cmplwi r3,0x0; beq
        # <retry-path>` (zero/invalid handle triggers retry), so
        # kOfflineStatus (nonzero, since it is a status code being
        # misread as a handle) was masquerading as a *valid* handle --
        # dishonest even though r200's XNotifyGetNext fix already made
        # any consumer of that handle harmless (it ignores its own
        # handle argument entirely now). Allocates a real handle from
        # the same counter NtCreateTimer/NtCreateMutant already use.
        return "  ctx.r3.u64 = g_next_handle.fetch_add(1u);\n"
    if name == "XAudioRegisterRenderDriverClient":
        # r206: real call site 0x823a667c (inside Function_823A6620)
        # confirms r4 as an output handle pointer -- the same
        # storage slot XAudioUnregisterRenderDriverClient's own real
        # call site (0x823a664c, same function) reads the handle back
        # from. This project has no real audio-render-driver pipeline
        # to back a handle with (XAudioSubmitRenderDriverFrame's own
        # return is discarded by its one real call site, confirming
        # nothing downstream needs real audio output), so this is pure
        # handle bookkeeping -- allocate one from the same counter
        # NtCreateTimer/NtCreateMutant already use, matching that
        # precedent.
        return """  if (ctx.r4.u32 != 0u) PPC_STORE_U32(ctx.r4.u32, g_next_handle.fetch_add(1u));
  ctx.r3.u64 = 0u;
"""
    if name == "XAudioUnregisterRenderDriverClient":
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XAudioSubmitRenderDriverFrame":
        # r206: real call site 0x823a68c0 discards the return value
        # entirely -- same class of fix as r197's KeLockL2/KeUnlockL2.
        # No real audio output device exists to render the submitted
        # frame to; accepting and discarding it is honest, not a
        # fabricated success.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XamVoiceHeadsetPresent":
        # r207: BOOL XamVoiceHeadsetPresent(HANDLE hVoice) -- a plain
        # boolean, not an NTSTATUS. Real call site 0x82206900 compares
        # the result directly against zero (`cmpwi cr6,r3,0x0`, no
        # signed status check), so kOfflineStatus (nonzero) was
        # currently read as TRUE ("headset present") on every call --
        # this project has no real microphone/headset device, so FALSE
        # is the honest report, not a guess. The caller's own
        # change-detection logic (comparing this call's result against
        # its last stored reading) is stable under a constant FALSE: it
        # never spuriously reports "presence changed" once the first
        # reading settles.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XamVoiceClose":
        # r208: VOID XamVoiceClose(HANDLE hVoice) -- all 3 real call
        # sites (0x82207528, 0x82207694, 0x82206fa0) discard the return
        # value outright, no check at all -- same class of fix as
        # r197's KeLockL2/KeUnlockL2/KiApcNormalRoutineNop.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XamLoaderTerminateTitle":
        # r209: VOID XamLoaderTerminateTitle(VOID) -- documented, never
        # returns (tears down the running title). Confirmed at this
        # XEX's own second real call site (0x821f608c): the very next
        # instruction (0x821f6090) is a *different function's own
        # prologue* -- the compiler emitted no epilogue at all after this
        # call, meaning it never expected control to return here. Same
        # never-returns class as r193's KeBugCheck, but this is a normal
        # title-exit path, not a fault, so a clean std::exit(0) is the
        # honest match rather than std::abort().
        return "  std::exit(0);\n"
    if name == "XMACreateContext":
        # r210: real call site 0x823aec8c confirms r3 as an output
        # handle pointer -- reads back the same storage slot immediately
        # after success to feed a follow-up query call. Return checked
        # signed (blt error), so kOfflineStatus (negative) currently
        # blocks all XMA (compressed audio) context setup unconditionally.
        # Same handle-bookkeeping precedent as XAudioRegisterRenderDriverClient
        # (r206): allocate from g_next_handle, write through the output
        # pointer, succeed.
        return """  if (ctx.r3.u32 != 0u) PPC_STORE_U32(ctx.r3.u32, g_next_handle.fetch_add(1u));
  ctx.r3.u64 = 0u;
"""
    if name == "XMAReleaseContext":
        # r210: real call site 0x823ae37c discards the return value
        # outright -- same class of fix as r197's KeLockL2/KeUnlockL2.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "ExTerminateThread":
        # r213: VOID ExTerminateThread(DWORD ExitCode) -- documented,
        # never returns. Confirmed at both of this XEX's real call sites
        # (0x821f8060, 0x82390b38): the instruction immediately after the
        # second is a different function's own prologue, the same
        # never-returns evidence r193/r209 already established for
        # KeBugCheck/XamLoaderTerminateTitle. Unlike those two, this ends
        # only the CALLING thread, not the whole process (real guest
        # threads here run as their own std::thread, per ExCreateThread);
        # throws GuestThreadTerminated, caught by that thread's own entry
        # point (ExCreateThread's lambda, or the main entry probe in
        # ac6recomp_main.cpp) so it unwinds cleanly instead of invoking
        # std::terminate() on the whole process.
        return "  throw ac6::native::GuestThreadTerminated{};\n"
    if name == "ExRegisterTitleTerminateNotification":
        # r213: real signature registers a title-terminate cleanup
        # callback. 9 real call sites, all checked following the same
        # shape (0x821f1190, 0x821f1620 traced directly): the return
        # value is discarded outright by every one, falling straight
        # into the next instruction with no check -- same class of fix
        # as r197's KeLockL2/KeUnlockL2. This project has nowhere to
        # invoke an arbitrary registered callback from later, but since
        # nothing reads the return here, that gap is not observable.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "HalReturnToFirmware":
        # r214: VOID HalReturnToFirmware(HAL_RETURN_TYPE Routine) --
        # documented to never return (reboots/halts the console). This
        # XEX's single real call site (0x821f7e6c, r3=1) is conditionally
        # gated and the compiler did emit a normal epilogue after it --
        # weaker evidence than r193/r209/r213's "no epilogue at all"
        # cases, since a compiler unaware a callee never returns still
        # generates one defensively. Real documented semantics still
        # govern: this is a whole-console exit, not per-thread, so
        # std::exit(0) (r209's XamLoaderTerminateTitle precedent) is the
        # honest match rather than falling through to whatever the
        # compiler's own (never-reached-on-real-hardware) epilogue does.
        return "  std::exit(0);\n"
    if name == "XMsgCancelIORequest":
        # r215: cancels an in-flight XMsg IO request. All 3 real call
        # sites (0x821f47b8, 0x82206efc, 0x822075c0) discard the return
        # value outright, no check at all -- same class of fix as r197's
        # KeLockL2/KeUnlockL2. This project's XMsg transport itself is
        # deferred (r203, XMsgStartIORequest), so there is never a real
        # in-flight request for this to cancel; unconditional success is
        # honest regardless.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "NtSetTimerEx":
        # r216: real call site traced through its wrapper at
        # 0x82204c54/0x82390ab0: r3=TimerHandle, r4=DueTime,
        # r8=Period (masked to a byte by the wrapper; 0 at this call
        # site = one-shot). TimerApcRoutine (r5) is NULL here -- no APC
        # to invoke.
        return """  std::int64_t due_time = 0;
  if (ctx.r4.u32 != 0u) {
    due_time = static_cast<std::int64_t>(PPC_LOAD_U64(ctx.r4.u32));
  }
  set_timer(ctx.r3.u32, due_time, static_cast<std::int32_t>(ctx.r8.u32));
  ctx.r3.u64 = 0u;
"""
    if name == "NtCancelTimer":
        # r216: single real call site passes r4=0 (CurrentState* not
        # requested) -- no output to write.
        return """  cancel_timer(ctx.r3.u32);
  ctx.r3.u64 = 0u;
"""
    if name == "VdSetDisplayMode":
        # r217: real call site 0x821f075c discards the return value
        # outright -- falls straight into the next call's own setup with
        # no check at all. Same class of fix as VdRetrainEDRAM and
        # friends above: the native renderer owns the Vd lifecycle.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XamShowMessageBoxUIEx":
        # r222: real 9-arg signature fully resolved (r221) against this
        # XEX's single real call site (0x821f5c30): r10=pMessageBoxResult,
        # and the 9th arg (pOverlapped, stack-passed) sits at the
        # caller's own r1+0x54 -- confirmed directly from that call
        # site's own `stw r5,0x54(r1)` (r5 = &(local OVERLAPPED-shaped
        # buffer)), not assumed from ABI convention alone: native stubs
        # receive `ctx` unchanged from the caller (no frame push), so
        # ctx.r1.u32 IS the caller's own r1 at the `bl`, and
        # ctx.r1.u32+0x54 reads exactly what that `stw` wrote.
        #
        # The caller only invokes the async completion-wait helper
        # (r220) when this call returns exactly 997
        # (ERROR_IO_PENDING); any other return skips straight to reading
        # the final button-pressed result at pOverlapped+0x14 (this
        # XEX's own `lwz r3,0x7c(r1)`, i.e. (r1+0x68)+0x14). No real UI
        # exists to ask the user anything, so "button 0" is the honest
        # default -- completing synchronously (not 997) means the
        # caller never touches pOverlapped's Internal/InternalHigh
        # fields at all on this path.
        return """  if (ctx.r10.u32 != 0u) PPC_STORE_U32(ctx.r10.u32, 0u);
  const std::uint32_t overlapped = PPC_LOAD_U32(ctx.r1.u32 + 0x54);
  if (overlapped != 0u) PPC_STORE_U32(overlapped + 0x14, 0u);
  ctx.r3.u64 = 0u;
"""
    if name == "XamUserReadProfileSettings":
        # r223: r219 found this import's own caller checks for specific
        # return codes (0x7a/122, 0x3e5/997) rather than any-nonzero, and
        # r220 first flagged this as needing the same async-completion
        # care as XamShowMessageBoxUIEx. Traced both of the caller's own
        # "not that specific code" branch targets this cycle
        # (0x821ce6dc, 0x821ce8c8): both are the caller's own clean,
        # successful return path (one plain `return 0`, one setting an
        # internal flag byte and returning 1) -- neither is an error
        # path, so any return value other than those two specific codes
        # is already handled safely by this caller. The traced call site
        # requests zero settings (dwNumSettingIds=0, pdwSettingIds=NULL),
        # so STATUS_SUCCESS is the honest match; this cycle did not
        # confirm which register holds pcbResults/pResults for this
        # import's own (still-ambiguous beyond 7 parameters) real
        # signature closely enough to write through it, so only the
        # return status is fixed, matching this project's own "don't
        # assert an unconfirmed field" discipline.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XamUserGetName":
        # r226: two real call sites in this XEX, both independently
        # confirming the documented XamUserGetName(DWORD dwUserIndex,
        # LPSTR szUserName, DWORD cchUserName) contract --
        # Function_82161B08 (0x82161bb8) calls func_0x821f4410 with a
        # literal cchUserName=0x10 and NEVER checks the return value
        # before using the buffer downstream, and Function_821CFD50
        # (0x821cfd98) independently calls it with the same literal
        # 0x10 and returns the buffer pointer only when the return is
        # exactly 0. The generic offline default only sets ctx.r3 (a
        # status) and never touches ctx.r4's buffer -- for the first
        # caller specifically that is a real uninitialized-read risk
        # (r183's class of bug: unread buffer treated as valid
        # regardless of status), not a cosmetic gap. Writes a short,
        # explicitly-synthetic ASCII name (no real gamertag exists
        # offline) truncated/null-terminated to the confirmed 16-byte
        # buffer, and returns STATUS_SUCCESS so both callers' own
        # success paths are taken honestly.
        return """  const std::uint32_t buffer = ctx.r4.u32;
  const std::uint32_t length = ctx.r5.u32;
  static constexpr char kOfflinePlayerName[] = "Player";
  if (length != 0u) {
    std::uint32_t copy_len = static_cast<std::uint32_t>(sizeof(kOfflinePlayerName) - 1u);
    if (copy_len > length - 1u) copy_len = length - 1u;
    for (std::uint32_t i = 0; i < copy_len; ++i) {
      PPC_STORE_U8(buffer + i, static_cast<std::uint8_t>(kOfflinePlayerName[i]));
    }
    PPC_STORE_U8(buffer + copy_len, 0u);
  }
  ctx.r3.u64 = 0u;
"""
    if name == "XamUserGetSigninInfo":
        # r227: r211 verified this import without confirming a fix; this
        # cycle traced its single wrapper (Function_821F5190 at
        # 0x821f5190 -- confirmed via raw disassembly to be a pure
        # register-forwarding passthrough: its prologue never touches
        # r3/r4/r5 before `bl` at 0x821f519c) and, through it, 6 real
        # callers of the wrapper. Every traced caller passes the same
        # 3-argument shape -- (dwUserIndex, dwFlags=0 literal,
        # pSigninInfo) -- matching the documented
        # XamUserGetSigninInfo(DWORD, DWORD, PXUSER_SIGNIN_INFO)
        # contract, and every one of them reads exactly one field back:
        # a bit at offset +8 (`>> 1 & 1`), gating whether the caller's
        # own real-profile logic runs at all (bit set == skip). The
        # generic offline default never writes that field, so it always
        # read stale stack bytes as this gate -- the same uninitialized
        # -read-as-control-flow risk class as r226/r183, but for a bit
        # that decides whether real per-player state gets initialized at
        # all, not merely a display string. Fills only the two fields
        # this evidence confirms (XUID at +0..+7, dwInfoFlags at +8) for
        # the same offline user 0 this file's own XamUserGetSigninState
        # (r176) already treats as signed in locally -- XUID zero (no
        # real Xbox Live identity exists offline) and the gating bit
        # clear (so every traced caller's real-profile path runs, not
        # its skip path). Other user indices keep the prior offline
        # failure this file already used everywhere, matching r176's own
        # "only index 0 is signed in" convention; nothing beyond offset
        # +8 was read by any traced caller, so nothing beyond it is
        # written.
        return """  const std::uint32_t user_index = ctx.r3.u32;
  const std::uint32_t info = ctx.r5.u32;
  if (user_index != 0u) {
    trace_offline_import("XamUserGetSigninInfo");
    ctx.r3.u64 = kOfflineStatus;
  } else {
    if (info != 0u) {
      PPC_STORE_U32(info + 0u, 0u);
      PPC_STORE_U32(info + 4u, 0u);
      PPC_STORE_U32(info + 8u, 0u);
    }
    ctx.r3.u64 = 0u;
  }
"""
    if name == "XamUserGetXUID":
        # r228: r211 verified this import without confirming a fix. Its
        # single wrapper (Function_821F4618 at 0x821f4618, the SAME
        # address r225 found bracketing the r224/r225 dispatch-table
        # region -- an unrelated coincidence of unlabeled code layout,
        # not a connection to that closed thread) was traced by raw
        # disassembly: `or r5,r4,r4` then `li r4,0x7` immediately before
        # `bl 0x823cfedc` -- the wrapper takes (dwUserIndex, pXuid) and
        # remaps to the real 3-argument
        # XamUserGetXUID(DWORD dwUserIndex, DWORD dwFlags, PXUID pXuid)
        # contract with a literal dwFlags=7, the same "wrapper remaps a
        # reduced argument list onto the real signature" pattern already
        # resolved for NtSetTimerEx (r216) and XamShowMessageBoxUIEx
        # (r221/r222). 6 real callers of the wrapper found; 3 traced
        # confirm an 8-byte XUID output (Function_821CFCE0 loops
        # dwUserIndex 0..3 comparing each retrieved XUID against a
        # caller-supplied one; Function_821CFDD8 returns the 8-byte
        # value directly as its own return; Function_821CE9A0 copies it
        # unconditionally into a struct field with no status check at
        # all -- the same "buffer read regardless of status" risk class
        # as r226/r227). The generic offline default never wrote this
        # buffer. Fills 8 bytes of zero (no real Xbox Live XUID exists
        # offline, same convention as r227's XUID field) for the same
        # user 0 this file already treats as signed in locally (r176),
        # and returns STATUS_SUCCESS; other indices keep the existing
        # offline failure.
        return """  const std::uint32_t user_index = ctx.r3.u32;
  const std::uint32_t xuid = ctx.r5.u32;
  if (user_index != 0u) {
    trace_offline_import("XamUserGetXUID");
    ctx.r3.u64 = kOfflineStatus;
  } else {
    if (xuid != 0u) {
      PPC_STORE_U32(xuid + 0u, 0u);
      PPC_STORE_U32(xuid + 4u, 0u);
    }
    ctx.r3.u64 = 0u;
  }
"""
    if name == "XamTaskSchedule":
        # r583: execute the guest callback on a detached thread so that
        # callers waiting on the task handle's completion event unblock.
        # ABI: r3=callback, r4=context, r5=flags, r6=pTaskHandle.
        # The callback signature is void callback(void* context) where
        # context is passed in r3.  The task handle is an auto-reset
        # event that the caller waits on; signalling it after the
        # callback returns unblocks NtWaitForSingleObjectEx.
        return """  const std::uint32_t callback_addr = ctx.r3.u32;
  const std::uint32_t context_val = ctx.r4.u32;
  const std::uint32_t handle_ptr = ctx.r6.u32;
  const std::uint32_t handle = g_next_handle.fetch_add(1u);
  if (handle_ptr != 0u) PPC_STORE_U32(handle_ptr, handle);
  {
    std::lock_guard<std::mutex> lock(g_event_mutex);
    g_events.try_emplace(handle);
  }
  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    std::fprintf(stderr, "[XamTaskSchedule] callback=0x%08x context=0x%08x handle=%u\\n",
                 callback_addr, context_val, handle);
  }
  std::thread([callback_addr, context_val, handle, base]() {
    PPCContext task_ctx{};
    task_ctx.fpscr.loadFromHost();
    task_ctx.r1.u32 = 0x8ef80000u;
    task_ctx.r3.u32 = context_val;
    auto* fn = PPC_LOOKUP_FUNC(base, callback_addr);
    if (fn != nullptr) {
      fn(task_ctx, base);
    }
    set_event(handle);
    if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
      std::fprintf(stderr, "[XamTaskSchedule] callback=0x%08x completed, handle=%u signalled\\n",
                   callback_addr, handle);
    }
  }).detach();
  ctx.r3.u64 = 0u;
"""
    if name == "XamTaskCloseHandle":
        # r230: r198 deferred both XamTaskSchedule and XamTaskCloseHandle
        # together, reasoning that fixing either needs a real guest
        # -callback execution subsystem. That is true for
        # XamTaskSchedule (it schedules a guest callback this project
        # never invokes) but not for this import: its one real call site
        # (0x82391e00, inside Function_82391A40's large save-content-scan
        # path) calls it as `func_0x823d09bc(auStack_4b0[0])` on a
        # sibling line immediately after a successful XamTaskSchedule
        # call, and DISCARDS the return value outright -- no check of any
        # kind, the same "retour ignoré par tous les appelants réels"
        # shape already fixed for KeLockL2/KeUnlockL2 (r197),
        # IoDismountVolume (r204), XamVoiceClose (r208) and
        # XMsgCancelIORequest (r215). Unconditional success is honest
        # regardless of whether the handle being closed is real.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "RtlNtStatusToDosError":
        # r125: a real, documented, stateless Win32 API -- converts an
        # NTSTATUS (r3) to the equivalent Win32 error code (returned in
        # r3). Previously fell through to the generic offline stub,
        # which returns kOfflineStatus regardless of input; traced
        # (r121) as this build's single most-called unimplemented
        # import (43 calls in one probe run). r125 traced its specific
        # role in the GATE2 crash chain: sub_821F75B8 calls this on a
        # failed NtReadFile status before caching the *converted*
        # value in a per-thread field a retry loop (sub_821CC508) polls
        # for exactly 997 (== the real ERROR_IO_PENDING this function
        # is supposed to produce from STATUS_PENDING) -- with the
        # generic stub, that conversion never happens, so the loop can
        # never recognize "still pending" and exhausts its retries.
        #
        # Only the two values this project has directly verified as
        # relevant this cycle are mapped explicitly (STATUS_SUCCESS and
        # STATUS_PENDING); anything else falls back to
        # ERROR_MR_MID_NOT_FOUND (317), the real Windows NT behavior
        # for a status with no explicit table entry -- not a guess at
        # what an unverified code *should* mean, but this API's own
        # documented default. Traced (AC6_NATIVE_IMPORT_TRACE-gated)
        # so a future cycle can see which other NTSTATUS values this
        # title actually converts before adding more entries.
        return """  const std::uint32_t status = ctx.r3.u32;
  std::uint32_t dos_error;
  switch (status) {
    case 0x00000000u: dos_error = 0u; break;      // STATUS_SUCCESS -> ERROR_SUCCESS
    case 0x00000103u: dos_error = 997u; break;     // STATUS_PENDING -> ERROR_IO_PENDING
    default:
      if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
        std::fprintf(stderr,
                      "[RtlNtStatusToDosError] unmapped status=0x%08x\\n",
                      status);
      }
      dos_error = 317u;  // ERROR_MR_MID_NOT_FOUND: real default for no mapping
      break;
  }
  ctx.r3.u64 = dos_error;
"""
    if name == "KeTlsAlloc":
        return """  static thread_local std::uint32_t next = 0u;
  if (next >= g_tls_values.size()) {
    ctx.r3.u64 = 0xffffffffu;
    return;
  }
  ctx.r3.u64 = next++;
"""
    if name == "KeTlsGetValue":
        return """  const std::uint32_t index = ctx.r3.u32;
  ctx.r3.u64 = index < g_tls_values.size() ? g_tls_values[index] : 0u;
"""
    if name == "KeTlsSetValue":
        return """  const std::uint32_t index = ctx.r3.u32;
  if (index >= g_tls_values.size()) {
    ctx.r3.u64 = 0u;
    return;
  }
  g_tls_values[index] = ctx.r4.u32;
  ctx.r3.u64 = 1u;
"""
    if name == "KeTlsFree":
        return """  const std::uint32_t index = ctx.r3.u32;
  if (index < g_tls_values.size()) {
    g_tls_values[index] = 0u;
    ctx.r3.u64 = 1u;
  } else {
    ctx.r3.u64 = 0u;
  }
"""
    if name == "KeQueryPerformanceFrequency":
        return "  ctx.r3.u64 = 50000000u;\n"
    if name == "KeQuerySystemTime":
        # r179: real signature is VOID KeQuerySystemTime(PLARGE_INTEGER
        # SystemTime) -- a struct-fill through r3 (a single 64-bit FILETIME
        # tick count, 100ns units since 1601-01-01), not a status. The
        # generic offline-import fallback previously wrote nothing through
        # that pointer. All FOUR of this XEX's real call sites read the
        # result back and do real work with it, not a discarded read:
        #   two feed it straight into RtlTimeToTimeFields (0x821f4bb4,
        #   0x821f5abc) to populate a real year/month/day/hour/min/sec
        #   struct -- a fixed small constant would render as a nonsensical
        #   date (e.g. 1601 or 1970), not a real one;
        #   0x821f7f00 computes an elapsed-time delta between two saved
        #   timestamps to drive what reads as a real timer/animation gate
        #   -- a fixed constant would freeze that delta at zero forever;
        #   0x82392cf8 takes the low 32 bits as what reads as a
        #   session/seed value -- a fixed constant would make it
        #   constant across runs instead of unique.
        # All four need a real, changing wall-clock value, not an
        # arbitrary fixed one. Uses the host's own current time (fully
        # deterministic per real run, exactly matching what real hardware
        # provides, not a synthetic value chosen to force any specific
        # comparison outcome).
        return """  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const std::int64_t hundred_ns =
      std::chrono::duration_cast<std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>>(now).count();
  constexpr std::int64_t kFiletimeEpochOffset = 116444736000000000LL;  // 1601-1970 in 100ns units
  PPC_STORE_U64(ctx.r3.u32, static_cast<std::uint64_t>(hundred_ns + kFiletimeEpochOffset));
"""
    if name == "RtlTimeToTimeFields":
        # r185: real signature is VOID RtlTimeToTimeFields(PLARGE_INTEGER
        # Time, PTIME_FIELDS TimeFields) -- the real companion this XEX's
        # own two of r179's four KeQuerySystemTime call sites (0x821f4bb4,
        # 0x821f5abc) feed straight into, to populate a real calendar
        # struct. Left as the generic offline fallback, r179's own fix
        # was still incomplete: the FILETIME it computes was being handed
        # to a no-op that never wrote the TIME_FIELDS output at all, so
        # the calendar struct stayed uninitialized regardless. This XEX's
        # own real call sites read back struct+0x0/+0x2/+0x4/+0x6/+0x8/
        # +0xa/+0xc/+0xe as Year/Month/Day/Hour/Minute/Second/
        # Millisecond/Weekday -- the real, standard Win32 TIME_FIELDS
        # layout, confirmed byte-for-byte at this XEX's own call sites,
        # not assumed. Uses C++20 <chrono>'s own calendar conversion (the
        # real, standard Gregorian algorithm -- no ambiguity to resolve,
        # the same category as r184's real SHA-1) rather than a
        # hand-rolled reimplementation.
        return """  const std::int64_t hundred_ns = static_cast<std::int64_t>(PPC_LOAD_U64(ctx.r3.u32));
  constexpr std::int64_t kFiletimeEpochOffset = 116444736000000000LL;  // 1601-1970 in 100ns units
  const std::chrono::system_clock::time_point time_point{
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>(
              hundred_ns - kFiletimeEpochOffset))};
  const auto day_point = std::chrono::floor<std::chrono::days>(time_point);
  const std::chrono::year_month_day ymd{day_point};
  const std::chrono::weekday weekday{day_point};
  const std::chrono::hh_mm_ss<std::chrono::milliseconds> time_of_day{
      std::chrono::duration_cast<std::chrono::milliseconds>(time_point - day_point)};
  PPC_STORE_U16(ctx.r4.u32 + 0x0, static_cast<std::uint16_t>(static_cast<int>(ymd.year())));
  PPC_STORE_U16(ctx.r4.u32 + 0x2, static_cast<std::uint16_t>(static_cast<unsigned>(ymd.month())));
  PPC_STORE_U16(ctx.r4.u32 + 0x4, static_cast<std::uint16_t>(static_cast<unsigned>(ymd.day())));
  PPC_STORE_U16(ctx.r4.u32 + 0x6, static_cast<std::uint16_t>(time_of_day.hours().count()));
  PPC_STORE_U16(ctx.r4.u32 + 0x8, static_cast<std::uint16_t>(time_of_day.minutes().count()));
  PPC_STORE_U16(ctx.r4.u32 + 0xa, static_cast<std::uint16_t>(time_of_day.seconds().count()));
  PPC_STORE_U16(ctx.r4.u32 + 0xc, static_cast<std::uint16_t>(time_of_day.subseconds().count()));
  PPC_STORE_U16(ctx.r4.u32 + 0xe, static_cast<std::uint16_t>(weekday.c_encoding()));
"""
    if name == "RtlTimeFieldsToTime":
        # r185: the real inverse of RtlTimeToTimeFields above --
        # `BOOLEAN RtlTimeFieldsToTime(PTIME_FIELDS TimeFields,
        # PLARGE_INTEGER Time)`. This XEX's own real call site
        # (0x821fb3a4) confirms the same TIME_FIELDS field offsets
        # (writing them before the call) and the return contract: `rlwinm.
        # r11,r3,0,0x18,0x1f; beq <treat as failure>` -- the low byte of
        # the return is the real BOOLEAN (nonzero = TRUE = fields were
        # valid), matching the real XDK contract exactly.
        return """  const int year = static_cast<int>(PPC_LOAD_U16(ctx.r3.u32 + 0x0));
  const unsigned month = PPC_LOAD_U16(ctx.r3.u32 + 0x2);
  const unsigned day = PPC_LOAD_U16(ctx.r3.u32 + 0x4);
  const unsigned hour = PPC_LOAD_U16(ctx.r3.u32 + 0x6);
  const unsigned minute = PPC_LOAD_U16(ctx.r3.u32 + 0x8);
  const unsigned second = PPC_LOAD_U16(ctx.r3.u32 + 0xa);
  const unsigned millisecond = PPC_LOAD_U16(ctx.r3.u32 + 0xc);
  const std::chrono::year_month_day ymd{std::chrono::year{year},
                                        std::chrono::month{month},
                                        std::chrono::day{day}};
  if (!ymd.ok()) {
    ctx.r3.u64 = 0u;  // FALSE: invalid fields, matching the real contract
    return;
  }
  const std::chrono::sys_days day_point{ymd};
  const auto time_since_midnight = std::chrono::hours{hour} + std::chrono::minutes{minute} +
      std::chrono::seconds{second} + std::chrono::milliseconds{millisecond};
  const auto time_point = day_point + time_since_midnight;
  constexpr std::int64_t kFiletimeEpochOffset = 116444736000000000LL;  // 1601-1970 in 100ns units
  const std::int64_t unix_hundred_ns = std::chrono::duration_cast<
      std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>>(
      time_point.time_since_epoch()).count();
  PPC_STORE_U64(ctx.r4.u32, static_cast<std::uint64_t>(unix_hundred_ns + kFiletimeEpochOffset));
  ctx.r3.u64 = 1u;  // TRUE
"""
    if name == "RtlFillMemoryUlong":
        # r186: real signature is VOID RtlFillMemoryUlong(PVOID
        # Destination, ULONG Length, ULONG Pattern) -- a standard,
        # publicly documented NT RTL primitive with one fixed algorithm
        # (fill `Length / 4` ULONGs with `Pattern`), the same
        # no-ambiguity-to-resolve category as r184's SHA-1 and r185's
        # calendar conversion, not a guessed value. This XEX's one real
        # call site (0x821f3354) fills a local buffer with pattern
        # `0x80000000` -- previously left as whatever stack garbage was
        # present, since the generic offline fallback never wrote through
        # the pointer at all.
        return """  const std::uint32_t word_count = ctx.r4.u32 / 4u;
  for (std::uint32_t i = 0; i < word_count; ++i) {
    PPC_STORE_U32(ctx.r3.u32 + i * 4u, ctx.r5.u32);
  }
"""
    if name == "RtlCompareMemoryUlong":
        # r186: real signature is ULONG RtlCompareMemoryUlong(PVOID
        # Source, ULONG Length, ULONG Pattern) -- the standard NT RTL
        # companion to RtlFillMemoryUlong above: compares `Length / 4`
        # ULONGs against `Pattern` and returns the byte count of the
        # longest matching prefix from the start (stopping at the first
        # mismatched ULONG). Same fixed-algorithm, no-ambiguity category.
        return """  const std::uint32_t word_count = ctx.r4.u32 / 4u;
  std::uint32_t matched_words = 0u;
  for (; matched_words < word_count; ++matched_words) {
    if (PPC_LOAD_U32(ctx.r3.u32 + matched_words * 4u) != ctx.r5.u32) break;
  }
  ctx.r3.u64 = matched_words * 4u;
"""
    if name == "RtlUnicodeToMultiByteN":
        # r187: real signature is NTSTATUS
        # RtlUnicodeToMultiByteN(PCHAR MultiByteString, ULONG
        # MaxBytesInMultiByteString, PULONG BytesInMultiByteString, PCWCH
        # UnicodeString, ULONG BytesInUnicodeString). This XEX's one real
        # call site (0x821f4758) confirms the argument shape (r3=dest,
        # r4=maxDestBytes, r5=&bytesWritten [NULL here], r6=srcUTF16,
        # r7=srcByteLen) and the real NTSTATUS success contract:
        # `cmpwi r3,0x0; bge <success>` -- any non-negative return is
        # success (matching NTSTATUS convention), not a discarded status;
        # a negative return branches into RtlNtStatusToDosError (already
        # implemented, r126).
        # Converts each UTF-16 code unit to its low byte for codepoints
        # <= 0xFF (a real, standard Latin-1-shaped mapping) and the
        # conventional '?' (0x3F) replacement for anything higher --
        # ordinary NT default-unmappable-character behavior, not a value
        # chosen to force a particular result. This project's own guest
        # strings are file paths/titles, not general Unicode text.
        return """  const std::uint32_t max_dest_bytes = ctx.r4.u32;
  const std::uint32_t char_count =
      std::min<std::uint32_t>(ctx.r7.u32 / 2u, max_dest_bytes);
  std::uint32_t written = 0u;
  for (; written < char_count; ++written) {
    const std::uint16_t code_unit = PPC_LOAD_U16(ctx.r6.u32 + written * 2u);
    PPC_STORE_U8(ctx.r3.u32 + written,
                 code_unit <= 0xffu ? static_cast<std::uint8_t>(code_unit) : 0x3fu);
  }
  if (ctx.r5.u32 != 0u) PPC_STORE_U32(ctx.r5.u32, written);
  ctx.r3.u64 = 0u;  // STATUS_SUCCESS
"""
    if name == "RtlUnicodeStringToAnsiString":
        # r188: real signature is NTSTATUS
        # RtlUnicodeStringToAnsiString(PANSI_STRING DestinationString,
        # PCUNICODE_STRING SourceString, BOOLEAN
        # AllocateDestinationString). This project's own r122/r123
        # already confirmed the real ANSI_STRING layout {Length@0,
        # MaximumLength@2, Buffer@4} for this XEX; UNICODE_STRING is the
        # same fixed, Microsoft-published shape with a WCHAR* buffer --
        # external protocol knowledge, not a guessed offset. This XEX's
        # one real call site (0x823927f0) confirms the argument shape
        # (r3=Destination, r4=Source, r5=AllocateDestinationString=1) and
        # the real NTSTATUS success contract (>= 0 is success, matching
        # RtlUnicodeToMultiByteN's own confirmed contract, r187); on
        # success it later calls RtlFreeAnsiString on the same
        # destination, confirming a real allocation is expected.
        # AllocateDestinationString=0 (caller-supplied buffer) is also
        # handled, respecting the destination's own MaximumLength, though
        # not exercised by this XEX's own traced call site.
        return """  const std::uint16_t source_length = PPC_LOAD_U16(ctx.r4.u32 + 0x0);
  const std::uint32_t source_buffer = PPC_LOAD_U32(ctx.r4.u32 + 0x4);
  const std::uint32_t char_count = source_length / 2u;
  const std::uint32_t needed_bytes = char_count + 1u;  // + NUL terminator
  std::uint32_t dest_buffer = 0u;
  if (ctx.r5.u32 != 0u) {
    dest_buffer = allocate_guest(base, needed_bytes);
    if (dest_buffer == 0u) {
      ctx.r3.u64 = 0xc0000017u;  // STATUS_NO_MEMORY
      return;
    }
    PPC_STORE_U16(ctx.r3.u32 + 0x2, static_cast<std::uint16_t>(needed_bytes));
  } else {
    dest_buffer = PPC_LOAD_U32(ctx.r3.u32 + 0x4);
    const std::uint16_t max_length = PPC_LOAD_U16(ctx.r3.u32 + 0x2);
    if (dest_buffer == 0u || needed_bytes > max_length) {
      ctx.r3.u64 = 0x80000005u;  // STATUS_BUFFER_OVERFLOW
      return;
    }
  }
  std::uint32_t written = 0u;
  for (; written < char_count; ++written) {
    const std::uint16_t code_unit = PPC_LOAD_U16(source_buffer + written * 2u);
    PPC_STORE_U8(dest_buffer + written,
                 code_unit <= 0xffu ? static_cast<std::uint8_t>(code_unit) : 0x3fu);
  }
  PPC_STORE_U8(dest_buffer + written, 0u);
  PPC_STORE_U16(ctx.r3.u32 + 0x0, static_cast<std::uint16_t>(written));
  PPC_STORE_U32(ctx.r3.u32 + 0x4, dest_buffer);
  ctx.r3.u64 = 0u;  // STATUS_SUCCESS
"""
    if name == "RtlFreeAnsiString":
        # r188: real signature is VOID RtlFreeAnsiString(PANSI_STRING
        # AnsiString) -- releases the buffer RtlUnicodeStringToAnsiString
        # allocated above. This project's own established convention
        # (ExFreePool, this file) is that guest pool pages are never
        # individually reclaimed -- `allocate_guest`'s bump allocator has
        # no free path -- so this clears the ANSI_STRING fields (the real,
        # observable part of "freeing" from the caller's perspective) and
        # leaves the actual page reclaimed with the guest, matching
        # ExFreePool's own precedent exactly rather than inventing a
        # dangling-pointer risk with a fake per-allocation free.
        return """  PPC_STORE_U16(ctx.r3.u32 + 0x0, 0u);
  PPC_STORE_U16(ctx.r3.u32 + 0x2, 0u);
  PPC_STORE_U32(ctx.r3.u32 + 0x4, 0u);
"""
    if name == "NtQueryFullAttributesFile":
        # r189: real signature is NTSTATUS
        # NtQueryFullAttributesFile(POBJECT_ATTRIBUTES ObjectAttributes,
        # PFILE_NETWORK_OPEN_INFORMATION FileInformation) -- struct-fill
        # through r4 plus a real NTSTATUS return, not a discarded status.
        # Reuses the exact ObjectAttributes/ANSI_STRING path-extraction
        # shape r122/r123 already confirmed for NtCreateFile. Both of
        # this XEX's real call sites confirm the standard
        # FILE_NETWORK_OPEN_INFORMATION layout (52 bytes: four
        # LARGE_INTEGER timestamps, AllocationSize, EndOfFile, then a
        # ULONG FileAttributes) by reading FileAttributes at struct+0x30
        # -- exactly the real, Microsoft-published offset -- and, at the
        # second site, all seven fields. Matching NtCreateFile's own
        # discipline: a real file that does not exist in the bound media
        # is a normal, expected failure
        # (STATUS_OBJECT_NAME_NOT_FOUND), not a fabricated error.
        # Adds NativeGuestMediaService::file_size() (native_guest_media.h)
        # -- a small accessor on the existing service, not a new
        # subsystem -- since no import needed a file's real size without
        # a full read until now.
        return """  const std::uint32_t object_attributes = ctx.r3.u32;
  const std::uint32_t object_name = object_attributes != 0u
      ? PPC_LOAD_U32(object_attributes + 4u) : 0u;
  std::uint32_t status = 0xC0000034u;  // STATUS_OBJECT_NAME_NOT_FOUND
  if (object_name != 0u) {
    const std::uint16_t length = PPC_LOAD_U16(object_name + 0u);
    const std::uint32_t buffer = PPC_LOAD_U32(object_name + 4u);
    if (buffer != 0u) {
      const std::string_view raw(
          reinterpret_cast<const char*>(base + buffer), length);
      const std::string relative = guest_path_to_relative(raw);
      const std::optional<std::uint32_t> opened =
          ac6::native::native_guest_media_service().open_file(relative);
      if (opened.has_value()) {
        const std::optional<std::uint64_t> size =
            ac6::native::native_guest_media_service().file_size(*opened);
        ac6::native::native_guest_media_service().close_file(*opened);
        if (ctx.r4.u32 != 0u) {
          for (std::uint32_t offset = 0u; offset < 0x30u; offset += 4u) {
            PPC_STORE_U32(ctx.r4.u32 + offset, 0u);  // timestamps: unread by this XEX's own call sites
          }
          const std::uint64_t real_size = size.value_or(0u);
          PPC_STORE_U64(ctx.r4.u32 + 0x20, real_size);  // AllocationSize
          PPC_STORE_U64(ctx.r4.u32 + 0x28, real_size);  // EndOfFile
          PPC_STORE_U32(ctx.r4.u32 + 0x30, 0x80u);      // FILE_ATTRIBUTE_NORMAL
        }
        status = 0u;  // STATUS_SUCCESS
      }
    }
  }
  ctx.r3.u64 = status;
"""
    if name == "NtQueryVolumeInformationFile":
        # r190: real signature is NTSTATUS
        # NtQueryVolumeInformationFile(HANDLE FileHandle, PIO_STATUS_BLOCK
        # IoStatusBlock, PVOID FsInformation, ULONG Length,
        # FS_INFORMATION_CLASS FsInformationClass) -- a struct-fill
        # through r5, not a discarded status. ALL THREE of this XEX's
        # real call sites pass FsInformationClass=3
        # (FileFsSizeInformation) and Length=0x18 (24, exactly
        # sizeof(FILE_FS_SIZE_INFORMATION): two LARGE_INTEGERs + two
        # ULONGs). One call site computes real free/total byte counts
        # from the queried fields (SectorsPerAllocationUnit *
        # BytesPerSector * {Available,Total}AllocationUnits) and reports
        # them to its own caller -- a real disk-space check, not a
        # discarded read.
        #
        # Only FileFsSizeInformation is implemented; any other requested
        # class returns STATUS_INVALID_INFO_CLASS rather than a guessed
        # struct shape this XEX has no traced call site for.
        #
        # Values: SectorsPerAllocationUnit=0x20, BytesPerSector=0x200 --
        # a 0x4000 (16KiB) allocation unit, the real Xbox 360 FATX
        # filesystem's own documented default cluster size for large
        # partitions, not invented for this fix. Total/available space:
        # 8 GiB, an ordinary generous default (this project's own guest
        # media is read-only and does not yet support real writes, so
        # "plenty of free space" avoids a false disk-full block without
        # asserting a specific real console's exact partition size).
        # A second call site compares the computed bytes-per-unit against
        # a caller-supplied expected value not traced by this cycle; this
        # fix cannot guarantee that comparison passes and does not assert
        # that it does.
        return """  if (ctx.r7.u32 != 3u) {
    ctx.r3.u64 = 0xc0000003u;  // STATUS_INVALID_INFO_CLASS
    return;
  }
  constexpr std::uint32_t kSectorsPerAllocationUnit = 0x20u;
  constexpr std::uint32_t kBytesPerSector = 0x200u;
  constexpr std::uint64_t kAllocationUnitBytes =
      static_cast<std::uint64_t>(kSectorsPerAllocationUnit) * kBytesPerSector;
  constexpr std::uint64_t kTotalBytes = 8ull * 1024ull * 1024ull * 1024ull;
  constexpr std::uint64_t kTotalAllocationUnits = kTotalBytes / kAllocationUnitBytes;
  PPC_STORE_U64(ctx.r5.u32 + 0x0, kTotalAllocationUnits);   // TotalAllocationUnits
  PPC_STORE_U64(ctx.r5.u32 + 0x8, kTotalAllocationUnits);   // AvailableAllocationUnits
  PPC_STORE_U32(ctx.r5.u32 + 0x10, kSectorsPerAllocationUnit);
  PPC_STORE_U32(ctx.r5.u32 + 0x14, kBytesPerSector);
  if (ctx.r4.u32 != 0u) {
    PPC_STORE_U32(ctx.r4.u32 + 0u, 0u);   // IoStatusBlock.Status
    PPC_STORE_U32(ctx.r4.u32 + 4u, 0x18u);  // IoStatusBlock.Information
  }
  ctx.r3.u64 = 0u;  // STATUS_SUCCESS
"""
    if name == "MmQueryStatistics":
        # r108: the generic offline-import default below only sets ctx.r3
        # (a status code) and never touches the guest output buffer the
        # caller passed in ctx.r3 -- but MmQueryStatistics's real contract is
        # to fill that buffer. r108 traced this precisely: sub_821F4820
        # (generated/ppc_recomp.27.cpp) hands this stub a 104-byte buffer
        # (its own Length field, set by the caller before the call) and,
        # after this call returns, reads fields at +4 and +12 unconditionally
        # -- no status check gates the read. Left unwritten, those fields
        # carry whatever stale bytes already occupied that stack slot, which
        # is exactly r105/r106's "uninitialized" 0x400000 at
        # Function_821D5F48's frame+108 (== sub_821F4820's own +12,
        # PPC_STORE_U32(r31.u32 + 12, ...) in ppc_recomp.27.cpp): a
        # rlwinm-by-12 (pages-to-bytes) transform of this field, consumed a
        # few instructions later as an available-memory figure the caller
        # subtracts 0x800000 (8MiB) from -- which underflows when the field
        # is left at zero-or-small garbage.
        #
        # Real hardware never returns uninitialized kernel memory here,
        # so this fills the two fields sub_821F4820 actually reads with a
        # deterministic pair derived from the Xbox 360's well-documented
        # unified 512MiB (0x20000000-byte) physical memory, at the 4KiB page
        # granularity the caller's own rlwinm-12 shift already establishes:
        # total = 0x20000 pages, available = 0x18000 pages (384MiB), leaving
        # headroom for the OS/kernel reservation without asserting its exact
        # real figure. The two fields consumed nowhere else in the traced
        # call graph (+16, +20) are zeroed rather than guessed at, matching
        # this stub's own no-fabrication discipline for anything unread.
        return """  constexpr std::uint32_t kTotalPhysicalPages = 0x20000u;
  constexpr std::uint32_t kAvailablePhysicalPages = 0x18000u;
  const std::uint32_t buffer = ctx.r3.u32;
  PPC_STORE_U32(buffer + 4, kTotalPhysicalPages);
  PPC_STORE_U32(buffer + 12, kAvailablePhysicalPages);
  PPC_STORE_U32(buffer + 16, 0u);
  PPC_STORE_U32(buffer + 20, 0u);
  ctx.r3.u64 = 0u;
"""
    if name == "sprintf":
        # r236: r234/r235 traced all 7 real call sites and decoded every
        # format string they use (scripts/DumpBytes.java) -- the closed
        # specifier set is %s/%d/%x/%X (optional width, %x/%X optionally
        # zero-padded), no case needs more than 2 varargs (register-only,
        # r5/r6 -- no stack-spilled argument beyond r10 is ever needed at
        # any traced site). Uses the shared guest_vprintf parser (r236,
        # this file's HEADER) rather than a per-format special case, since
        # the parser itself is now exhaustively verified against every
        # real caller instead of guessed. A defensive 0x2000-byte cap
        # bounds the write regardless of guest input, since real sprintf's
        # own contract has no size argument to bound it with.
        return """  const std::array<std::uint32_t, 6> args = {
      ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32};
  int next_index = 0;
  const std::function<std::uint32_t()> next_arg = [&]() {
    return next_index < static_cast<int>(args.size()) ? args[next_index++] : 0u;
  };
  const std::size_t written =
      guest_vprintf(base, ctx.r3.u32, 0x2000u, ctx.r4.u32, next_arg);
  ctx.r3.u64 = static_cast<std::uint32_t>(written);
"""
    if name == "_vsnprintf":
        # r236: the only 2 real call sites are inside internal wrapper
        # functions (Function_821EF4E0/Function_821EF458, r235) that spill
        # their own incoming varargs (r5-r10) as sequential 8-byte
        # doublewords onto their own stack (confirmed via raw disassembly
        # -- `std r5,0x20(r1)` through `std r10,0x48(r1)`, then the va_list
        # pointer handed to _vsnprintf points at the first of those slots)
        # before calling _vsnprintf with that pointer as the 4th argument
        # (r6). Each slot's low 32 bits (offset +4, big-endian) hold the
        # actual 32-bit value. Every format string reaching either wrapper
        # was decoded (r235) and uses the same closed specifier set as
        # sprintf. Honors the real `size` argument (r4) as the write cap,
        # matching this import's actual bounded contract.
        return """  std::uint32_t offset = 0u;
  const std::uint32_t va_list_ptr = ctx.r6.u32;
  const std::function<std::uint32_t()> next_arg = [&]() {
    const std::uint32_t value = PPC_LOAD_U32(va_list_ptr + offset + 4u);
    offset += 8u;
    return value;
  };
  const std::size_t written =
      guest_vprintf(base, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, next_arg);
  ctx.r3.u64 = static_cast<std::uint32_t>(written);
"""
    if name == "XexCheckExecutablePrivilege":
        # r239: single real call site observed at startup (0x821f5ed0's
        # early init). The generic offline fallback returned
        # kOfflineStatus (0xC00000BB, STATUS_NOT_SUPPORTED) which is
        # negative; the guest's own entry path then immediately
        # terminated the title via HalReturnToFirmware/XamLoaderTerminateTitle
        # (observed: entry called XexCheckExecutablePrivilege then exited
        # via std::exit(0) before any other import). No evidence at this
        # site distinguishes which privilege ID is being checked, but the
        # only honest offline answer that lets a retail title continue
        # past its own startup privilege gate is "granted": return
        # STATUS_SUCCESS (0). This matches the single-offline-profile
        # assumption already used for XamUserCheckPrivilege (r182) and
        # the "only index 0 is signed in" convention (r176).
        return "  ctx.r3.u64 = 0u;  // STATUS_SUCCESS, privilege granted\n"
    if name == "NtQueryInformationFile":
        # r240: 9 real sites? Same shape as NtSetInformationFile. The probe
        # showed DATA.TBL open followed by NtQueryInformationFile returning
        # kOfflineStatus, causing the file-open sequence to be treated as
        # failure and routed to the dirty-disc path. Return success and
        # zero-fill the information buffer; the status check that gates the
        # dirty-disc branch passes.
        # r276: the missing producer is traced -- sub_821CC008 (the PAC read
        # pipeline's queue creator) opens each catalog file and derives the
        # shared read-buffer size from the sum of the queried file sizes;
        # sub_821F4E08 reads *(IoStatusBlock+4) as the size. The r240
        # zero-fill collapsed every size to 0, so the queue buffer got a
        # 0-byte allocation (probe evidence: queue+332 = 0, buffer = 0 at
        # NtReadFile) and the entry read loop failed downstream. Fill the
        # real size for the size-bearing information classes (r7):
        # 5 = FileStandardInformation (AllocationSize@0, EndOfFile@8),
        # 20 = FileEndOfFileInformation (EndOfFile@0),
        # 34 = FileNetworkOpenInformation (AllocationSize@32, EndOfFile@40;
        # matches the retail wrapper's Length=56 at sub_821F5630).
        # IoStatusBlock Information = the size per the observed retail
        # guest convention (sub_821F4E08). Unknown handles/classes keep the
        # r240 zero-fill (no fabricated content for unbound files).
        return """  std::uint64_t size = 0u;
  bool known = false;
  if (const auto real = ac6::native::native_guest_media_service().file_size(ctx.r3.u32)) {
    size = *real;
    known = true;
  }
  const std::uint32_t info_class = ctx.r7.u32;
  if (ctx.r5.u32 != 0u && ctx.r6.u32 != 0u) {
    for (std::uint32_t i = 0; i < ctx.r6.u32; ++i) {
      PPC_STORE_U8(ctx.r5.u32 + i, 0u);
    }
  }
  if (known && ctx.r5.u32 != 0u) {
    if (info_class == 5u) {
      PPC_STORE_U32(ctx.r5.u32 + 0u, static_cast<std::uint32_t>(size >> 32));
      PPC_STORE_U32(ctx.r5.u32 + 4u, static_cast<std::uint32_t>(size));
      PPC_STORE_U32(ctx.r5.u32 + 8u, static_cast<std::uint32_t>(size >> 32));
      PPC_STORE_U32(ctx.r5.u32 + 12u, static_cast<std::uint32_t>(size));
    } else if (info_class == 20u) {
      PPC_STORE_U32(ctx.r5.u32 + 0u, static_cast<std::uint32_t>(size >> 32));
      PPC_STORE_U32(ctx.r5.u32 + 4u, static_cast<std::uint32_t>(size));
    } else if (info_class == 34u) {
      PPC_STORE_U32(ctx.r5.u32 + 32u, static_cast<std::uint32_t>(size >> 32));
      PPC_STORE_U32(ctx.r5.u32 + 36u, static_cast<std::uint32_t>(size));
      PPC_STORE_U32(ctx.r5.u32 + 40u, static_cast<std::uint32_t>(size >> 32));
      PPC_STORE_U32(ctx.r5.u32 + 44u, static_cast<std::uint32_t>(size));
    }
  }
  if (ctx.r4.u32 != 0u) {
    PPC_STORE_U32(ctx.r4.u32 + 0u, 0u);
    PPC_STORE_U32(ctx.r4.u32 + 4u, known ? static_cast<std::uint32_t>(size) : 0u);
  }
  ctx.r3.u64 = 0u;
"""
    if name == "NtSetInformationFile":
        # r240: companion to NtQueryInformationFile. Real call site after
        # DATA.TBL open sets file position or similar. Returning success
        # lets the boot path continue. No buffer is read back on success.
        return """  if (ctx.r4.u32 != 0u) {
    PPC_STORE_U32(ctx.r4.u32 + 0u, 0u);
    PPC_STORE_U32(ctx.r4.u32 + 4u, 0u);
  }
  ctx.r3.u64 = 0u;
"""
    if name == "VdGetSystemCommandBuffer":
        # r240: hardware returns a GPU command buffer pointer. The generic
        # offline stub returned kOfflineStatus as a fake pointer value.
        # Allocate a small guest buffer so a non-null pointer is returned
        # without dereferencing garbage. The native renderer owns the Vd
        # lifecycle, so the content is not interpreted beyond being non-zero.
        return """  const std::uint32_t buffer = allocate_guest(base, 0x1000u);
  if (buffer != 0u) {
    ac6::native::native_guest_vd_service().register_allocation(base, buffer, 0x1000u);
  }
  ctx.r3.u64 = buffer;
"""
    if name == "VdPersistDisplay":
        # r240: VdPersistDisplay was fail-closed. The probe shows it is
        # called before file access; returning success avoids a spurious
        # error path while the native renderer owns the display.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XamShowDirtyDiscErrorUI":
        # r240: shown when file open fails. Returning synchronously with
        # success and no UI lets the boot retry path be avoided once files
        # are actually present. The probe showed this was invoked after
        # DATA.TBL failures.
        return "  ctx.r3.u64 = 0u;\n"
    if name == "XamLoaderLaunchTitle":
        # r240: XamLoaderLaunchTitle would reboot the title on dirty disc.
        # Return success without launching to keep the current title running
        # when files are now available.
        return "  ctx.r3.u64 = 0u;\n"
    return f"""  trace_offline_import("{name}");
  ctx.r3.u64 = kOfflineStatus;
"""


def render(mapping: Path, output: Path) -> int:
    source = mapping.read_text(encoding="utf-8")
    names = sorted(set(IMPORT.findall(source)))
    if not names:
        raise ValueError("mapping contains no imports")
    lines = [HEADER]
    if any(name.startswith("KeTls") for name in names):
        lines.append("namespace {\n")
        lines.append("thread_local std::array<std::uint32_t, 128> g_tls_values{};\n")
        lines.append("}\n\n")
    for name in names:
        lines.append(
            # XenonRecomp declares PPC_EXTERN_FUNC without extern "C".  Keep
            # C++ linkage so generated call sites resolve the same signature;
            # the previous C-linkage form silently left mapping references
            # undefined until the mapping table was forced into a link.
            f'void __imp__{name}(PPCContext& ctx, uint8_t* base) {{\n'
            "  // Offline-only HLE boundary; no socket or host I/O side effect.\n"
            + render_body(name)
            + "}\n"
        )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("".join(lines), encoding="utf-8")
    return len(names)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mapping", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    count = render(args.mapping, args.output)
    print(json.dumps({"import_count": count, "output": str(args.output.resolve())},
                     sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
