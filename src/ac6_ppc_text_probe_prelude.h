#pragma once

// Force-included into the selected generated UI translation units by the dedicated
// diagnostic build. Redefining the generated-function prologue preserves the
// base alignment assumption and exposes the live PPC argument registers
// without editing generated recompiler output.
// The generated config must precede context.h. Without it context.h installs
// its library-mode PPC_CALL_INDIRECT_FUNC debugtrap fallback, and #pragma once
// prevents ac6recomp_init.h from replacing that fallback later.
#include "ac6recomp_config.h"
#include <rex/ppc/context.h>

#include "ac6_guest_text_probe.h"

#undef PPC_FUNC_PROLOGUE
#undef PPC_CALL_INDIRECT_FUNC

#define PPC_CALL_INDIRECT_FUNC(x)                                                   \
  ::ac6::text_probe::CallIndirect(uint32_t(x), PPC_LOOKUP_FUNC(base, x), ctx, base)

#if defined(__clang__)
#define PPC_FUNC_PROLOGUE()                                      \
  do {                                                           \
    __builtin_assume(((size_t)base & 0x1F) == 0);                \
    ::ac6::text_probe::Note(__func__, ctx, base);                 \
  } while (0)
#elif defined(__GNUC__)
#define PPC_FUNC_PROLOGUE()                                      \
  do {                                                           \
    if (((size_t)base & 0x1F) != 0) __builtin_unreachable();     \
    ::ac6::text_probe::Note(__func__, ctx, base);                 \
  } while (0)
#else
#define PPC_FUNC_PROLOGUE() ::ac6::text_probe::Note(__func__, ctx, base)
#endif
