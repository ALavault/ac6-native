/**
 * ReXGlue runtime - AC6 Recompilation project
 * Copyright (c) 2026 Tom Clay. All rights reserved.
 *
 * P2.3: the store watchpoint. See ac6_ppc_store_hook.h for why every guest
 * store in the program routes through here and why ctx.lr is exact attribution.
 *
 * Records (address, value, writer) triples into a ring buffer armed on an input
 * edge, and writes them out for offline diffing. Comparing the stores a working
 * press performs against those a dead one performs names the write that only
 * one of them makes, and names the guest code that makes it -- which is the
 * question cycle 421 asked and nothing since has answered.
 */

#include "ac6_ppc_store_watch.h"

#ifdef AC6RECOMP_WATCH_GUEST_STORES

#include <atomic>
#include <cstdint>
#include <cstdio>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(ac6_watch_guest_stores, false, "AC6/Trace",
                    "Capture every guest store on the next input edge and write it to "
                    "ac6-stores-<mask>-<sequence>.bin.");
REXCVAR_DEFINE_INT32(ac6_watch_store_entries, 300000, "AC6/Trace",
                     "How many guest stores to record per armed capture.");
REXCVAR_DEFINE_INT32(ac6_watch_store_edge_mask, 0, "AC6/Trace",
                     "Only arm the store watch on edges matching this raw button mask (0 = any).");
REXCVAR_DEFINE_INT32(ac6_watch_store_skip_edges, 0, "AC6/Trace",
                     "Skip this many matching input edges before arming a store capture.");
REXCVAR_DEFINE_UINT32(ac6_watch_store_address, 0, "AC6/Trace",
                      "Log bounded writes to this exact guest address (0 = disabled).");
REXCVAR_DEFINE_INT32(ac6_watch_store_address_logs, 32, "AC6/Trace",
                     "Maximum exact-address store records to log.");
REXCVAR_DEFINE_BOOL(ac6_watch_store_address_changes_only, false, "AC6/Trace",
                    "Log the first exact-address store and subsequent value changes only.");

namespace ac6::stores {
namespace {

constexpr uint32_t kMaxEntries = 1u << 21;

struct Entry {
  uint32_t address;
  uint32_t lr;
  uint64_t value;
};

Entry* g_buffer = nullptr;
std::atomic<uint32_t> g_write_index{0};
std::atomic<bool> g_armed{false};
std::atomic<uint32_t> g_capacity{0};
uint32_t g_pending_mask = 0;
uint32_t g_capture_sequence = 0;
uint32_t g_matching_edge_sequence = 0;
std::atomic<uint32_t> g_address_log_sequence{0};
std::atomic<uint64_t> g_address_last_value{0};
std::atomic<bool> g_address_value_initialized{false};

}  // namespace

void Note(uint32_t address, uint64_t value, uint32_t lr) noexcept {
  // Store-instrumented builds are immutable for one process. Cache these after
  // command-line cvars have been applied so disabled address tracing remains a
  // single predictable branch on the hot path.
  static const uint32_t target_address = REXCVAR_GET(ac6_watch_store_address);
  static const int32_t configured_address_logs = REXCVAR_GET(ac6_watch_store_address_logs);
  static const bool address_changes_only =
      REXCVAR_GET(ac6_watch_store_address_changes_only);
  if (target_address != 0 && address == target_address) {
    const uint64_t previous =
        g_address_last_value.exchange(value, std::memory_order_relaxed);
    const bool had_previous =
        g_address_value_initialized.exchange(true, std::memory_order_relaxed);
    if (!address_changes_only || !had_previous || previous != value) {
      const uint32_t sequence =
          g_address_log_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
      const uint32_t limit = configured_address_logs > 0
                                 ? static_cast<uint32_t>(configured_address_logs)
                                 : 0;
      if (sequence <= limit) {
        REXLOG_WARN("[ac6-store-address] sequence={} address=0x{:08X} lr=0x{:08X} "
                    "value=0x{:016X}",
                    sequence, address, lr, value);
      }
    }
  }
  if (!g_armed.load(std::memory_order_relaxed)) {
    return;
  }
  const uint32_t index = g_write_index.fetch_add(1, std::memory_order_relaxed);
  const uint32_t capacity = g_capacity.load(std::memory_order_relaxed);
  if (index >= capacity) {
    // Full. Stop here rather than wrapping: a contiguous window starting at the
    // press is what makes two captures comparable entry by entry.
    g_armed.store(false, std::memory_order_release);
    g_write_index.store(capacity, std::memory_order_relaxed);
    return;
  }
  g_buffer[index] = Entry{address, lr, value};
}

void Arm(uint32_t edge_mask) {
  if (!REXCVAR_GET(ac6_watch_guest_stores) || g_armed.load(std::memory_order_relaxed)) {
    return;
  }
  const uint32_t want = static_cast<uint32_t>(REXCVAR_GET(ac6_watch_store_edge_mask));
  if (want != 0 && (edge_mask & want) == 0) {
    return;
  }
  const uint32_t matching_edge_sequence = ++g_matching_edge_sequence;
  const int32_t configured_skip = REXCVAR_GET(ac6_watch_store_skip_edges);
  const uint32_t skip = configured_skip > 0 ? static_cast<uint32_t>(configured_skip) : 0;
  if (matching_edge_sequence <= skip) {
    return;
  }
  if (g_pending_mask != 0) {
    return;  // an earlier capture is still waiting to be written
  }
  if (!g_buffer) {
    uint32_t entries = static_cast<uint32_t>(REXCVAR_GET(ac6_watch_store_entries));
    if (entries == 0 || entries > kMaxEntries) {
      entries = kMaxEntries;
    }
    g_buffer = new (std::nothrow) Entry[entries];
    if (!g_buffer) {
      REXLOG_ERROR("[ac6-stores] could not allocate {} entries", entries);
      return;
    }
    g_capacity.store(entries, std::memory_order_relaxed);
  }
  g_pending_mask = edge_mask;
  g_write_index.store(0, std::memory_order_relaxed);
  g_armed.store(true, std::memory_order_release);
  // No load-slide problem here, unlike the P2.2 function trace: addresses and
  // writers are both guest values, so what is recorded is what Ghidra shows.
  REXLOG_WARN("[ac6-stores] armed on edge 0x{:04X}, capturing {} stores", edge_mask,
              g_capacity.load(std::memory_order_relaxed));
}

void FlushIfComplete() {
  if (g_armed.load(std::memory_order_acquire)) {
    return;
  }
  const uint32_t count = g_write_index.load(std::memory_order_relaxed);
  if (!g_buffer || count == 0 || g_pending_mask == 0) {
    return;
  }
  char path[64];
  const uint32_t sequence = ++g_capture_sequence;
  std::snprintf(path, sizeof(path), "ac6-stores-%04X-%04u.bin", g_pending_mask,
                sequence);
  if (FILE* f = std::fopen(path, "wb")) {
    std::fwrite(g_buffer, sizeof(Entry), count, f);
    std::fclose(f);
    REXLOG_WARN("[ac6-stores] wrote {} stores to {}", count, path);
  } else {
    REXLOG_ERROR("[ac6-stores] could not open {}", path);
  }
  g_pending_mask = 0;
  g_write_index.store(0, std::memory_order_relaxed);
}

}  // namespace ac6::stores

#endif  // AC6RECOMP_WATCH_GUEST_STORES
