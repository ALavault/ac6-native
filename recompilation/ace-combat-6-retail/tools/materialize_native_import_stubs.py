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
#include \"ac6/native_guest_vd.h\"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
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
    if name in {"RtlEnterCriticalSection", "RtlLeaveCriticalSection",
                "RtlInitializeCriticalSection",
                "RtlInitializeCriticalSectionAndSpinCount"}:
        return "  ctx.r3.u64 = 0u;  // single guest thread until scheduler migration\n"
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
    if name == "NtCreateEvent":
        return """  const std::uint32_t handle = g_next_handle.fetch_add(1u);
  if (ctx.r3.u32 != 0u) PPC_STORE_U32(ctx.r3.u32, handle);
  create_event(handle, ctx.r6.u32 == 0u, ctx.r7.u32 != 0u);
  ctx.r3.u64 = 0u;
"""
    if name in {"NtCreateSemaphore", "NtCreateTimer", "NtCreateMutant"}:
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
    if name in {"NtReleaseMutant", "NtReleaseSemaphore"}:
        # Single guest thread until scheduler migration: no real contention
        # is modeled, so release always succeeds immediately (matches the
        # RtlEnterCriticalSection/RtlLeaveCriticalSection idiom above).
        # NtReleaseMutant(handle, PreviousCount*); NtReleaseSemaphore(handle,
        # ReleaseCount, PreviousCount*) -- both take an optional out pointer
        # for the previous count in r4.
        return """  if (ctx.r4.u32 != 0u) PPC_STORE_U32(ctx.r4.u32, 0u);
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
