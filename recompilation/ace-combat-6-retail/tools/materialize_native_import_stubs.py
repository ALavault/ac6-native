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
#include \"ac6/native_guest_media.h\"
#include \"ac6/native_guest_vd.h\"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

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
};
std::mutex g_event_mutex;
std::condition_variable g_event_cv;
std::unordered_map<std::uint32_t, EventState> g_events;

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

void create_event(std::uint32_t key, bool manual_reset, bool signaled) {
  if (key == 0u) return;
  {
    std::lock_guard lock(g_event_mutex);
    g_events[key] = EventState{signaled, manual_reset};
  }
  if (signaled) g_event_cv.notify_all();
}

bool set_event(std::uint32_t key) {
  if (key == 0u) return false;
  bool previous;
  {
    std::lock_guard lock(g_event_mutex);
    EventState& event = g_events[key];
    previous = event.signaled;
    event.signaled = true;
  }
  g_event_cv.notify_all();
  return previous;
}

bool clear_event(std::uint32_t key) {
  if (key == 0u) return false;
  std::lock_guard lock(g_event_mutex);
  EventState& event = g_events[key];
  const bool previous = event.signaled;
  event.signaled = false;
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
bool wait_event(std::uint32_t key) {
  if (key == 0u) return false;
  std::unique_lock lock(g_event_mutex);
  auto it = g_events.find(key);
  if (it == g_events.end()) return false;
  if (!it->second.signaled) {
    g_event_cv.wait_for(lock, std::chrono::milliseconds(2), [&] {
      auto retry = g_events.find(key);
      return retry == g_events.end() || retry->second.signaled;
    });
    it = g_events.find(key);
    if (it == g_events.end() || !it->second.signaled) return false;
  }
  if (!it->second.manual_reset) it->second.signaled = false;
  return true;
}

// r114: a suspended-created thread must block indefinitely until an
// explicit resume, unlike wait_event()'s bounded single-shot retry
// contract (designed for guest Nt*Wait* polling, not for parking a
// host std::thread before it ever runs guest code). Reuses the same
// g_events map/mutex/cv -- thread handles and event/semaphore/mutant
// handles already share one monotonic counter (g_next_handle), so
// there is no key collision between them.
void park_until_resumed(std::uint32_t key) {
  if (key == 0u) return;
  std::unique_lock lock(g_event_mutex);
  g_event_cv.wait(lock, [&] {
    auto it = g_events.find(key);
    return it == g_events.end() || it->second.signaled;
  });
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
}

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
  std::uint32_t address = PPC_LOAD_U32(ctx.r3.u32);
  if (address == 0u) {
    address = allocate_guest(base, requested);
    if (address == 0u) {
      ctx.r3.u64 = 0xC0000017u;  // STATUS_NO_MEMORY
      return;
    }
  } else {
    address &= 0xffff0000u;
    if (static_cast<std::uint64_t>(address) + requested > 0x7f000000ull) {
      ctx.r3.u64 = 0xC0000017u;  // STATUS_NO_MEMORY
      return;
    }
  }
  PPC_STORE_U32(ctx.r3.u32, address);
  PPC_STORE_U32(ctx.r4.u32, requested);
  ac6::native::native_guest_vd_service().register_allocation(
      base, address, requested);
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
        return "  critical_section_for(ctx.r3.u32).lock();\n  ctx.r3.u64 = 0u;\n"
    if name == "RtlLeaveCriticalSection":
        return "  critical_section_for(ctx.r3.u32).unlock();\n  ctx.r3.u64 = 0u;\n"
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
  constexpr std::uint32_t kCreateSuspended = 0x00000004u;
  const bool start_suspended = (creation_flags & kCreateSuspended) != 0u;
  if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
    std::fprintf(stderr,
                  "[ExCreateThread] handle=%u routine=0x%08x flags=0x%08x "
                  "suspended=%d\\n",
                  handle, routine_address, creation_flags,
                  start_suspended ? 1 : 0);
  }
  if (start_suspended) create_event(handle, /*manual_reset=*/true,
                                     /*signaled=*/false);
  try {
    std::thread([shim, worker, base, handle, start_suspended]() mutable {
      if (start_suspended) park_until_resumed(handle);
      shim(worker, base);
    }).detach();
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
    if name == "NtCreateFile":
        # r122/r123/r129: the real 9-arg NT signature, but this XEX's own
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
        return """  const std::uint32_t object_attributes = ctx.r5.u32;
  const std::uint32_t object_name = object_attributes != 0u
      ? PPC_LOAD_U32(object_attributes + 4u) : 0u;
  std::uint32_t status = 0xC0000034u;  // STATUS_OBJECT_NAME_NOT_FOUND
  std::uint32_t handle = 0u;
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
        handle = *opened;
        status = 0u;  // STATUS_SUCCESS
      }
      if (std::getenv("AC6_NATIVE_IMPORT_TRACE") != nullptr) {
        std::fprintf(stderr, "[NtCreateFile] \\"%s\\" -> %s\\n",
                      relative.c_str(), opened.has_value() ? "ok" : "not found");
      }
    }
  }
  if (ctx.r3.u32 != 0u) PPC_STORE_U32(ctx.r3.u32, handle);
  if (ctx.r6.u32 != 0u) {
    PPC_STORE_U32(ctx.r6.u32 + 0u, status);
    PPC_STORE_U32(ctx.r6.u32 + 4u, 0u);
  }
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
        return """  std::uint64_t offset = 0u;
  if (ctx.r10.u32 != 0u) {
    const std::uint32_t hi = PPC_LOAD_U32(ctx.r10.u32 + 0u);
    const std::uint32_t lo = PPC_LOAD_U32(ctx.r10.u32 + 4u);
    offset = (static_cast<std::uint64_t>(hi) << 32) | lo;
  }
  std::uint32_t bytes_read = 0u;
  std::uint8_t* dest = ctx.r8.u32 != 0u ? (base + ctx.r8.u32) : nullptr;
  const bool known_handle = ac6::native::native_guest_media_service().read_file(
      ctx.r3.u32, offset, dest, ctx.r9.u32, bytes_read);
  std::uint32_t status = 0xC0000008u;  // STATUS_INVALID_HANDLE
  if (known_handle) {
    status = (bytes_read == 0u && ctx.r9.u32 != 0u)
        ? 0xC0000011u   // STATUS_END_OF_FILE
        : 0u;           // STATUS_SUCCESS
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
  create_event(handle, ctx.r6.u32 == 0u, ctx.r7.u32 != 0u);
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
    if name in {"NtCreateTimer", "NtCreateMutant"}:
        return """  if (ctx.r3.u32 != 0u) PPC_STORE_U32(ctx.r3.u32,
                                             g_next_handle.fetch_add(1u));
  ctx.r3.u64 = 0u;
"""
    if name == "NtSetEvent":
        return """  const bool previous = set_event(ctx.r3.u32);
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
        return """  { std::lock_guard lock(g_event_mutex); g_events.erase(ctx.r3.u32); }
  ctx.r3.u64 = 0u;
"""
    if name == "KeResetEvent":
        return """  const bool previous = clear_event(ctx.r3.u32);
  ctx.r3.u64 = previous ? 1u : 0u;
"""
    if name == "KeSetEvent":
        return """  const bool previous = set_event(ctx.r3.u32);
  ctx.r3.u64 = previous ? 1u : 0u;
"""
    if name == "NtSignalAndWaitForSingleObjectEx":
        return """  set_event(ctx.r3.u32);
  if (!wait_event(ctx.r4.u32)) {
    ctx.r3.u64 = 0x102u;  // STATUS_TIMEOUT; offline event pair not signaled
    return;
  }
  ctx.r3.u64 = 0u;
"""
    if name == "NtReleaseMutant":
        # NtReleaseMutant(handle, PreviousCount*) -- r4 is the optional out
        # pointer for the previous count. Single guest thread until
        # scheduler migration: no real contention is modeled, so release
        # always succeeds immediately (matches the
        # RtlEnterCriticalSection/RtlLeaveCriticalSection idiom above).
        return """  if (ctx.r4.u32 != 0u) PPC_STORE_U32(ctx.r4.u32, 0u);
  ctx.r3.u64 = 0u;
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
        return """  if (!wait_event(ctx.r3.u32)) {
    ctx.r3.u64 = 0x102u;  // STATUS_TIMEOUT; offline, non-blocking
    return;
  }
  ctx.r3.u64 = 0u;
"""
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
