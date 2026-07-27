#include "ac6_boundary_probe.h"

#include <rex/logging.h>
#include <rex/logging/api.h>

#include <cstdint>
#include <thread>

namespace {

// sub_821D4ED0 computes its critical section once, in the prologue:
//     r30 = r3 * 152 + 0x829E64A8      (r3 = thread start context, 0..3)
//     r28 = r30 + 16
// r28 and r30 are callee-saved, so the invariant r28 == r30 + 16 must hold at
// every call site in the function. Checking that invariant needs no knowledge
// of r3 -- which is just as well, since both call sites are reached through
// `mr r3,r28` and r3 no longer holds the index by then.
constexpr uint32_t kTableBase = 0x829E64A8;
constexpr uint32_t kStride = 152;
constexpr uint32_t kCsOffset = 16;

// Host thread identity, so a broken invocation can be told apart from a healthy
// one: four guest threads run this function concurrently with contexts 0..3.
uint32_t HostTid() {
    return static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFF);
}

void Report(const char* site, const PPCRegister& r28, const PPCRegister& r30) {
    const bool consistent = r28.u32 == r30.u32 + kCsOffset;
    const bool in_table = r30.u32 >= kTableBase && r30.u32 < kTableBase + 4 * kStride &&
                          (r30.u32 - kTableBase) % kStride == 0;

    REXLOG_ERROR("AC6 CS probe [{}] tid={:04x}: r30={:#010x} r28={:#010x} invariant={} index={}",
                 site, HostTid(), r30.u32, r28.u32, consistent ? "held" : "BROKEN",
                 in_table ? int32_t((r30.u32 - kTableBase) / kStride) : -1);
}

}  // namespace

void ac6CriticalSectionEntryProbe(PPCRegister& r3) {
    // At 0x821D4ED0 itself. If a broken call site reports a tid that never
    // appears here, execution reached the body without running the prologue.
    REXLOG_ERROR("AC6 CS entry tid={:04x}: r3={}", HostTid(), r3.u32);
}

void ac6CriticalSectionBoundaryProbeA(PPCRegister& r28, PPCRegister& r30) {
    Report("site A 0x821D4F34", r28, r30);
}

void ac6CriticalSectionBoundaryProbeB(PPCRegister& r28, PPCRegister& r30) {
    Report("site B 0x821D4FB8", r28, r30);
}

void ac6CriticalSectionCalleeProbe(PPCRegister& ctr, PPCRegister& r28, PPCRegister& r30) {
    REXLOG_ERROR("AC6 CS callee tid={:04x}: target={:#010x} r30_in={:#010x} r28_in={:#010x}",
                 HostTid(), ctr.u32, r30.u32, r28.u32);
}
