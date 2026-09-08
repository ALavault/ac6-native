#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6/native_guest_threads.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

namespace {

// r421/r422 regression: a worker that raises IRQL (possibly nested, the
// same recursive pattern KeRaiseIrqlToDpcLevel's real call sites use) and
// is torn down before its matching lower_dpc_level() -- exactly what
// GuestThreadTerminated unwinding through a wait stub does on real
// hardware-observed call shapes -- must not permanently orphan
// g_dpc_level_mutex for every other thread that ever raises IRQL again.
// Before r422 this deadlocked NativeRuntime::shutdown()'s own
// native_guest_threads_stop_and_join() (see reports/ac6-retail-native-
// codegen-gate2-r421-hang-thread-pinned-live-blocked-on-non-raii-global-
// irql-mutex-20260908.md).
void residual_irql_is_released_on_simulated_termination() {
  using namespace ac6::native;

  std::thread worker([] {
    raise_dpc_level();
    raise_dpc_level();  // nested raise -- real IRQL is per-thread state,
                        // so the same thread legitimately raises twice.
    // No matching lower_dpc_level() here: this stands in for
    // GuestThreadTerminated unwinding past the pending Lower, same as
    // the real catch block in the generated ExCreateThread stub and in
    // ac6recomp_main.cpp's entry thread.
    release_residual_dpc_level();
  });
  worker.join();

  // If the mutex were left locked, this would hang forever. Run it on
  // its own thread and join it so a regression here fails as a CTest
  // TIMEOUT (see native/CMakeLists.txt) instead of hanging every other
  // suite that runs after this one in the same ctest invocation.
  std::atomic<bool> acquired{false};
  std::thread acquirer([&] {
    raise_dpc_level();
    acquired.store(true, std::memory_order_release);
    lower_dpc_level();
  });
  acquirer.join();
  assert(acquired.load(std::memory_order_acquire));
}

// Ordinary Raise/Lower pairing (no termination involved) must still
// provide the real cross-thread mutual exclusion r191 built
// g_dpc_level_mutex for: a second thread's raise_dpc_level() must not
// proceed until the first thread releases via lower_dpc_level(). This is
// the guarantee r422's fix must not weaken while closing the residual-
// IRQL leak above.
void raise_dpc_level_excludes_concurrent_threads() {
  using namespace ac6::native;

  raise_dpc_level();
  std::atomic<bool> second_raised{false};
  std::thread second([&] {
    raise_dpc_level();
    second_raised.store(true, std::memory_order_release);
    lower_dpc_level();
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  assert(!second_raised.load(std::memory_order_acquire));
  lower_dpc_level();
  second.join();
  assert(second_raised.load(std::memory_order_acquire));
}

}  // namespace

int main() {
  residual_irql_is_released_on_simulated_termination();
  raise_dpc_level_excludes_concurrent_threads();
  return 0;
}
