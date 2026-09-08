#pragma once

#include <thread>

namespace ac6::native {

// r277: guest worker threads spawned through the ExCreateThread import are
// registered here instead of being detached. Their guest loops are
// wait-driven, so the wait/delay/park stubs poll stop_requested() and end
// each worker through its existing GuestThreadTerminated catch. shutdown()
// calls stop_and_join() so the import stubs' globals and the guest address
// space outlive their last use -- without the join, still-running workers
// used stub globals process exit had already destroyed (observed as heap
// corruption in glibc malloc on the main thread after the probe window).
void native_guest_threads_register(std::thread&& thread) noexcept;

[[nodiscard]] bool native_guest_threads_stop_requested() noexcept;

void native_guest_threads_stop_and_join() noexcept;

// r422: KeRaiseIrqlToDpcLevel/KfLowerIrql (r191) emulate the single-core
// Xbox 360 semantic that raising IRQL alone excludes concurrent DPC-level
// code with one process-wide std::recursive_mutex, since this project runs
// guest code on real concurrent host threads. That mutex's lock()/unlock()
// pair is necessarily raw (the two kernel calls are separately-generated
// PPC functions, so no single C++ scope can hold a lock_guard across both).
// If GuestThreadTerminated (this same file's clean-shutdown mechanism) is
// thrown on a thread between its Raise and matching Lower -- e.g. it raised
// IRQL then entered a wait -- stack unwinding skips the unlock() and
// permanently orphans the mutex, hanging every other thread that ever
// raises IRQL again, including native_guest_threads_stop_and_join()'s own
// join() (see reports/ac6-retail-native-codegen-gate2-r421-hang-thread-
// pinned-live-blocked-on-non-raii-global-irql-mutex-20260908.md). These
// three functions are the only way the two stubs touch the mutex, so every
// raise is tracked and every GuestThreadTerminated catch can release
// whatever this thread still holds.
void raise_dpc_level() noexcept;
void lower_dpc_level() noexcept;
void release_residual_dpc_level() noexcept;

}  // namespace ac6::native
