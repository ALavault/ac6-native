#pragma once

// P2.3: replace every guest store in the recompiled program with a hook that
// records the address, the value, and the guest lr of whoever wrote it.
//
// This is the instrument cycle 440 identified as the only untried approach
// independent of code shape, and which was then never built -- twelve cycles of
// snapshot diffing were done instead, which is strictly weaker: a diff says that
// a word changed, never who changed it, and cannot see a field written back to
// the same value inside a frame.
//
// P2.2 is what makes it necessary. Function-entry tracing showed the A press
// executing a strict subset of the Right press, but every function in that
// difference turned out to be D3D -- sub_821D0E28 tail-calls into the device at
// sub_821DE920, sub_821E2400 reads a device object at +12432/+12448, and
// sub_8234F6E8 is float colour setup. Those are the *consequence* of the
// highlight moving, not the cause: A changes no state, so nothing re-renders.
// The branch that differs therefore lies inside a function both presses call,
// which entry tracing cannot resolve. Stores can.
//
// How it works: PPC_STORE_U8/16/32/64 are #ifndef-guarded in
// <rex/ppc/memory.h>, and every generated translation unit reaches them through
// generated/ac6recomp_init.h. Defining them first -- via -include on the
// generated sources only -- replaces all of them. Two properties make this
// work and are easy to miss:
//
//   * the macros expand inside a PPC_FUNC body, so ctx is in scope and ctx.lr
//     is the guest return address of the writer. That is exact attribution in
//     guest space, with no dladdr, no linker map and no PIE slide arithmetic --
//     all of which the P2.2 trace needed and got wrong twice.
//
//   * PPC_PHYS_HOST_OFFSET is declared later, in memory.h itself. That is fine:
//     macros are expanded at use, not at definition, so the reference resolves
//     by the time any generated code stores anything.
//
// Each operand is bound to a local before use. The generated corpus passes
// simple expressions today, but a macro that evaluates its arguments twice is a
// silent miscompilation waiting for the first one with a side effect.

#include <cstdint>

namespace ac6::stores {

// Defined in ac6_ppc_store_watch.cpp. Disarmed, this is one relaxed load and a
// predicted-not-taken branch -- it runs on every guest store in the program.
void Note(uint32_t address, uint64_t value, uint32_t lr) noexcept;

}  // namespace ac6::stores

#define PPC_STORE_U8(x, y)                                                          \
  ({                                                                                \
    const uint32_t _ac6_a = (uint32_t)(x);                                          \
    const uint8_t _ac6_v = (uint8_t)(y);                                            \
    ac6::stores::Note(_ac6_a, _ac6_v, (uint32_t)ctx.lr);                            \
    *(volatile uint8_t*)(base + _ac6_a + PPC_PHYS_HOST_OFFSET(_ac6_a)) = _ac6_v;    \
  })

#define PPC_STORE_U16(x, y)                                                         \
  ({                                                                                \
    const uint32_t _ac6_a = (uint32_t)(x);                                          \
    const uint16_t _ac6_v = (uint16_t)(y);                                          \
    ac6::stores::Note(_ac6_a, _ac6_v, (uint32_t)ctx.lr);                            \
    *(volatile uint16_t*)(base + _ac6_a + PPC_PHYS_HOST_OFFSET(_ac6_a)) =           \
        __builtin_bswap16(_ac6_v);                                                  \
  })

#define PPC_STORE_U32(x, y)                                                         \
  ({                                                                                \
    const uint32_t _ac6_a = (uint32_t)(x);                                          \
    const uint32_t _ac6_v = (uint32_t)(y);                                          \
    ac6::stores::Note(_ac6_a, _ac6_v, (uint32_t)ctx.lr);                            \
    *(volatile uint32_t*)(base + _ac6_a + PPC_PHYS_HOST_OFFSET(_ac6_a)) =           \
        __builtin_bswap32(_ac6_v);                                                  \
  })

#define PPC_STORE_U64(x, y)                                                         \
  ({                                                                                \
    const uint32_t _ac6_a = (uint32_t)(x);                                          \
    const uint64_t _ac6_v = (uint64_t)(y);                                          \
    ac6::stores::Note(_ac6_a, _ac6_v, (uint32_t)ctx.lr);                            \
    *(volatile uint64_t*)(base + _ac6_a + PPC_PHYS_HOST_OFFSET(_ac6_a)) =           \
        __builtin_bswap64(_ac6_v);                                                  \
  })
