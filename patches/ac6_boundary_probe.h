#pragma once

#include <rex/ppc/types.h>

// Cycle 310 boundary probe.
//
// sub_821D4ED0 computes its critical section once, as
//     r30 = r3 * 152 + 0x829E64A8
//     r28 = r30 + 16
// with r3 the thread start context, measured to be 0..3. r28 can therefore only
// be one of four addresses near 0x829E64B8, yet RtlEnterCriticalSection receives
// 0x826A19B0. This hook reads r28 and r30 at the call site itself, which decides
// where the divergence happens:
//
//   r28 already wrong here -> corruption precedes the call, and the
//                             callee-saved-register hypothesis stands;
//   r28 correct here       -> the divergence is in argument passing.
void ac6CriticalSectionBoundaryProbeA(PPCRegister& r28, PPCRegister& r30);
void ac6CriticalSectionBoundaryProbeB(PPCRegister& r28, PPCRegister& r30);
void ac6CriticalSectionCalleeProbe(PPCRegister& ctr, PPCRegister& r28, PPCRegister& r30);
void ac6CriticalSectionEntryProbe(PPCRegister& r3);
