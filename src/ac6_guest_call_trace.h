#pragma once

#include <cstdint>

// P2.2 control-flow trace. See ac6_guest_call_trace.cpp for why this exists and
// what it is for. Calls are no-ops unless the runtime was built with
// AC6RECOMP_TRACE_GUEST_CALLS and ac6_trace_guest_calls is set.
namespace ac6::trace {

#ifdef AC6RECOMP_TRACE_GUEST_CALLS

// Begin a capture, tagged with the raw button mask that triggered it. Ignored
// if a capture is already running, so the window always starts at the press.
void Arm(uint32_t edge_mask);

// True while entries are still being recorded.
bool IsCapturing();

// Write a completed capture to ac6-trace-<mask>-<sequence>.bin. Safe to call every frame;
// does nothing until a capture has finished filling.
void FlushIfComplete();

#else

inline void Arm(uint32_t) {}
inline bool IsCapturing() { return false; }
inline void FlushIfComplete() {}

#endif  // AC6RECOMP_TRACE_GUEST_CALLS

}  // namespace ac6::trace
