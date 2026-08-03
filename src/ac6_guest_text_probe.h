#pragma once

#include <cstdint>

struct PPCContext;
using PPCFunc = void(PPCContext& ctx, uint8_t* base);

// Targeted guest-text diagnostic. The generated UI translation units call
// Note() only in an AC6RECOMP_PROBE_GUEST_TEXT build; normal builds retain an
// inline no-op Arm().
namespace ac6::text_probe {

#ifdef AC6RECOMP_PROBE_GUEST_TEXT

void Arm();
void Note(const char* function_name, PPCContext& ctx, uint8_t* base);

// Diagnostic replacement for PPC_CALL_INDIRECT_FUNC. It is a transparent
// pass-through except at the one UI text-preparation call site qualified for
// the PAL executable.
void CallIndirect(uint32_t target, PPCFunc* function, PPCContext& ctx, uint8_t* base);

#else

inline void Arm() {}

#endif

}  // namespace ac6::text_probe
