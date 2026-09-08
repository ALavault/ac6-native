#include "ac6/native_guest_threads.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace ac6::native {

namespace {

std::atomic<bool> g_stop{false};
std::mutex g_mutex;
std::vector<std::thread> g_threads;

// r191: KeRaiseIrqlToDpcLevel/KfLowerIrql (88-110 real call sites each)
// are unpaired with a specific lock object -- on real single-core
// hardware, raising to DISPATCH_LEVEL alone is sufficient mutual
// exclusion (no DPC or lower-IRQL code can preempt), but that guarantee
// does not hold across this project's own real concurrent host threads
// (r111-r115) without an explicit lock. One global mutex emulates "no
// other DPC-level-or-higher code runs concurrently" -- the real semantic
// these two functions provide, not a per-object one since none exists at
// this call shape. recursive_mutex, unlike a plain mutex: real IRQL is
// per-thread state, not an object identity, so the same thread
// legitimately raises to DPC level while already there (a nested
// Raise/Lower pair is not a self-reacquisition of the same object the
// way a real spinlock forbids -- it is routine kernel control flow).
//
// r422: this mutex itself stays a plain, unguarded std::recursive_mutex
// -- KeRaiseIrqlToDpcLevel/KfLowerIrql are separate generated PPC
// functions, so lock()/unlock() can never share a C++ scope a
// lock_guard could span. g_dpc_level_depth is thread_local specifically
// so release_residual_dpc_level() can tell, per-thread, how many raises
// this thread still owes an unlock() for -- recursive_mutex itself does
// not expose that count. See raise_dpc_level()/lower_dpc_level()/
// release_residual_dpc_level() below and native_guest_threads.h for why
// this exists: without it, a GuestThreadTerminated thrown between a
// Raise and its matching Lower orphans the mutex forever (see reports/
// ac6-retail-native-codegen-gate2-r421-hang-thread-pinned-live-blocked-
// on-non-raii-global-irql-mutex-20260908.md).
std::recursive_mutex g_dpc_level_mutex;
thread_local int g_dpc_level_depth{0};

}  // namespace

void native_guest_threads_register(std::thread&& thread) noexcept {
  std::thread local(std::move(thread));
  try {
    std::lock_guard lock(g_mutex);
    g_threads.push_back(std::move(local));
  } catch (...) {
    // Registration is best-effort: a failed push leaves the thread detached,
    // which was the pre-r277 behavior for every worker.
    if (local.joinable()) local.detach();
  }
}

bool native_guest_threads_stop_requested() noexcept {
  return g_stop.load(std::memory_order_acquire);
}

void native_guest_threads_stop_and_join() noexcept {
  g_stop.store(true, std::memory_order_release);
  std::vector<std::thread> threads;
  {
    std::lock_guard lock(g_mutex);
    threads.swap(g_threads);
  }
  for (auto& thread : threads) {
    if (thread.joinable()) thread.join();
  }
}

void raise_dpc_level() noexcept {
  g_dpc_level_mutex.lock();
  ++g_dpc_level_depth;
}

void lower_dpc_level() noexcept {
  --g_dpc_level_depth;
  g_dpc_level_mutex.unlock();
}

void release_residual_dpc_level() noexcept {
  while (g_dpc_level_depth > 0) {
    --g_dpc_level_depth;
    g_dpc_level_mutex.unlock();
  }
}

}  // namespace ac6::native
