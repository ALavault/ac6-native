#pragma once

// The pinned XenonRecomp ppc_context.h is consumed as a generic ABI helper.
// This adapter is the AC6 boundary: generated game code sees explicit hooks,
// while the native product supplies the actual bridge to PpcRuntimeHooks.
#include "ppc_context_base.h"
#include <atomic>

extern "C" void AC6_PPC_CALL_INDIRECT(PPCContext&, std::uint8_t*,
                                        std::uint32_t);
extern "C" void AC6_PPC_FUNCTION_ENTRY_CONTEXT(PPCContext&, const char*) noexcept;
extern "C" void AC6_PPC_SET_LOAD_SITE(const char*, std::uint32_t) noexcept;
extern "C" std::uint8_t AC6_PPC_LOAD_U8(PPCContext&, std::uint8_t*,
                                         std::uint32_t);
extern "C" std::uint16_t AC6_PPC_LOAD_U16(PPCContext&, std::uint8_t*,
                                           std::uint32_t);
extern "C" std::uint32_t AC6_PPC_LOAD_U32(PPCContext&, std::uint8_t*,
                                           std::uint32_t);
extern "C" std::uint64_t AC6_PPC_LOAD_U64(PPCContext&, std::uint8_t*,
                                           std::uint32_t);
extern "C" void AC6_PPC_STORE_U8(PPCContext&, std::uint8_t*, std::uint32_t,
                                  std::uint8_t, const char*, std::uint32_t);
extern "C" void AC6_PPC_STORE_U16(PPCContext&, std::uint8_t*, std::uint32_t,
                                   std::uint16_t, const char*, std::uint32_t);
extern "C" void AC6_PPC_STORE_U32(PPCContext&, std::uint8_t*, std::uint32_t,
                                   std::uint32_t, const char*, std::uint32_t);
extern "C" void AC6_PPC_STORE_U64(PPCContext&, std::uint8_t*, std::uint32_t,
                                   std::uint64_t, const char*, std::uint32_t);
extern "C" void AC6_PPC_STORE_U128(PPCContext&, std::uint8_t*, std::uint32_t,
                                    const std::uint8_t*, const char*,
                                    std::uint32_t);

extern "C" void AC6_PPC_EIEIO(PPCContext&) noexcept;
extern "C" void AC6_PPC_SYNC(PPCContext&) noexcept;
extern "C" void AC6_PPC_LWSYNC(PPCContext&) noexcept;
extern "C" std::uint64_t AC6_PPC_READ_TIMEBASE(PPCContext&) noexcept;
extern "C" std::uint32_t AC6_PPC_LWARX(PPCContext&, std::uint32_t);
extern "C" bool AC6_PPC_STWCX(PPCContext&, std::uint32_t, std::uint32_t);
extern "C" __attribute__((noinline)) std::uint64_t
AC6_PPC_LDARX(PPCContext&, std::uint32_t);
extern "C" __attribute__((noinline)) bool
AC6_PPC_STDCX(PPCContext&, std::uint32_t, std::uint64_t);
extern "C" void AC6_PPC_VPKSWSS(PPCVRegister&, const PPCVRegister&,
                                  const PPCVRegister&) noexcept;
extern "C" void AC6_PPC_VCMPBFP(PPCVRegister&, const PPCVRegister&,
                                  const PPCVRegister&, PPCCRRegister&, bool) noexcept;
extern "C" void AC6_PPC_VREFP(PPCVRegister&, const PPCVRegister&) noexcept;
extern "C" void AC6_PPC_VRSQRTEFP(PPCVRegister&, const PPCVRegister&) noexcept;

#define PPC_EIEIO(ctx) AC6_PPC_EIEIO(ctx)
#define PPC_SYNC(ctx) AC6_PPC_SYNC(ctx)
#define PPC_LWSYNC(ctx) AC6_PPC_LWSYNC(ctx)
#define PPC_READ_TIMEBASE(ctx) AC6_PPC_READ_TIMEBASE(ctx)
#define PPC_LWARX(ctx, address) \
  (AC6_PPC_SET_LOAD_SITE(__func__, __LINE__), AC6_PPC_LWARX(ctx, address))
#define PPC_STWCX(ctx, address, value) \
  (AC6_PPC_SET_LOAD_SITE(__func__, __LINE__), \
   AC6_PPC_STWCX(ctx, address, value))
#define PPC_LDARX(ctx, address) \
  (AC6_PPC_SET_LOAD_SITE(__func__, __LINE__), AC6_PPC_LDARX(ctx, address))
#define PPC_STDCX(ctx, address, value) \
  (AC6_PPC_SET_LOAD_SITE(__func__, __LINE__), \
   AC6_PPC_STDCX(ctx, address, value))
#define PPC_VPKSWSS(dst, left, right) AC6_PPC_VPKSWSS(dst, left, right)
#define PPC_VCMPBFP(dst, left, right, cr, record) \
  AC6_PPC_VCMPBFP(dst, left, right, cr, record)
#define PPC_VREFP(dst, source) AC6_PPC_VREFP(dst, source)
#define PPC_VRSQRTEFP(dst, source) AC6_PPC_VRSQRTEFP(dst, source)

// XenonRecomp emits direct SIMDe loads for vector memory operations instead of
// a PPC_LOAD_U128 macro.  Keep generated C++ untouched and route the raw load
// through the native boundary; it rejects non-guest pointers.  The hook is
// skipped entirely unless the relevant process-lifetime fast flag was enabled
// during guest entry; XAM and post-resume flags remain independent and the
// hook itself remains the final guest-mapping guard.
extern "C" std::atomic_bool AC6_PPC_POST_RESUME_VECTOR_FAST_ENABLED;
extern "C" std::atomic_bool AC6_PPC_XAM_RETURN_CHAIN_VECTOR_FAST_ENABLED;
extern "C" void AC6_PPC_RECORD_POST_RESUME_VECTOR_READ(
    const void *, const char *, std::uint32_t) noexcept;
extern "C" void AC6_PPC_RECORD_XAM_RETURN_CHAIN_VECTOR_READ(
    const void *, const void *, const char *, std::uint32_t) noexcept;
#ifdef AC6_DEMO_ENABLE_VECTOR_READ_TRACE
// Existing opt-in frontbuffer diagnostics remain independent of the probe.
extern "C" void
AC6_PPC_RECORD_VECTOR_READ(std::uint32_t, const void*) noexcept;
extern "C" void AC6_PPC_VECTOR_CONTEXT(PPCContext&, const char*,
                                         std::uint64_t, std::uint32_t,
                                         std::uintptr_t) noexcept;
extern "C" std::uintptr_t AC6_PPC_GUEST_RAW_BASE;
#endif
static inline simde__m128i
AC6_PPC_LOAD_VECTOR_INLINE(const simde__m128i *pointer,
                           const char *generated_name,
                           std::uint32_t generated_line) noexcept {
  const auto value = simde_mm_loadu_si128(pointer);
#ifdef AC6_DEMO_ENABLE_VECTOR_READ_TRACE
  const auto raw = AC6_PPC_GUEST_RAW_BASE;
  const auto address = reinterpret_cast<std::uintptr_t>(pointer) - raw;
  if (raw != 0U && reinterpret_cast<std::uintptr_t>(pointer) >= raw &&
      address < 0x1'0000'0000ULL &&
      address < 0x13AE2000ULL && address + sizeof(value) > 0x1374A000ULL) {
    AC6_PPC_RECORD_VECTOR_READ(static_cast<std::uint32_t>(address), &value);
  }
#endif
  if (AC6_PPC_POST_RESUME_VECTOR_FAST_ENABLED.load(std::memory_order_relaxed)) {
    AC6_PPC_RECORD_POST_RESUME_VECTOR_READ(pointer, generated_name,
                                            generated_line);
  }
  if (AC6_PPC_XAM_RETURN_CHAIN_VECTOR_FAST_ENABLED.load(
          std::memory_order_relaxed)) {
    AC6_PPC_RECORD_XAM_RETURN_CHAIN_VECTOR_READ(
        pointer, &value, generated_name, generated_line);
  }
  return value;
}
#undef simde_mm_load_si128
#define simde_mm_load_si128(pointer) \
  AC6_PPC_LOAD_VECTOR_INLINE(static_cast<const simde__m128i *>(pointer), \
                              __func__, __LINE__)

#undef PPC_FUNC_PROLOGUE
#define PPC_FUNC_PROLOGUE()                                                   \
  do {                                                                        \
    __builtin_assume(((size_t)base & 0x1F) == 0);                             \
    AC6_PPC_FUNCTION_ENTRY_CONTEXT(ctx, __func__);                             \
  } while (false)

#undef PPC_LOAD_U8
#define PPC_LOAD_U8(x) \
  (AC6_PPC_SET_LOAD_SITE(__func__, __LINE__), AC6_PPC_LOAD_U8(ctx, base, (x)))
#undef PPC_LOAD_U16
#define PPC_LOAD_U16(x) \
  (AC6_PPC_SET_LOAD_SITE(__func__, __LINE__), AC6_PPC_LOAD_U16(ctx, base, (x)))
#undef PPC_LOAD_U32
#define PPC_LOAD_U32(x) \
  (AC6_PPC_SET_LOAD_SITE(__func__, __LINE__), AC6_PPC_LOAD_U32(ctx, base, (x)))
#undef PPC_LOAD_U64
#define PPC_LOAD_U64(x) \
  (AC6_PPC_SET_LOAD_SITE(__func__, __LINE__), AC6_PPC_LOAD_U64(ctx, base, (x)))
#undef PPC_STORE_U8
#define PPC_STORE_U8(x, y)                                                 \
  AC6_PPC_STORE_U8(ctx, base, (x), (y), __func__, __LINE__)
#undef PPC_STORE_U16
#define PPC_STORE_U16(x, y)                                                \
  AC6_PPC_STORE_U16(ctx, base, (x), (y), __func__, __LINE__)
#undef PPC_STORE_U32
#define PPC_STORE_U32(x, y)                                                \
  AC6_PPC_STORE_U32(ctx, base, (x), (y), __func__, __LINE__)
#undef PPC_STORE_U64
#define PPC_STORE_U64(x, y)                                                \
  AC6_PPC_STORE_U64(ctx, base, (x), (y), __func__, __LINE__)
#define PPC_STORE_U128(x, y)                                               \
  AC6_PPC_STORE_U128(ctx, base, (x), (y).u8, __func__, __LINE__)
#undef PPC_MM_STORE_U32
#define PPC_MM_STORE_U32(x, y)                                             \
  AC6_PPC_STORE_U32(ctx, base, (x), (y), __func__, __LINE__)

// XenonRecomp's default macro dereferences the indirect table before it can
// report an unknown target.  Route it through the product boundary so an
// unqualified callback becomes a RuntimeTrap instead of a host SIGSEGV.
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(x) AC6_PPC_CALL_INDIRECT(ctx, base, (x))
