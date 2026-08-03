#include "generated/ac6recomp_config.h"

#include <rex/memory.h>
#include <rex/ppc.h>

#include "ac6_dialog_text_fallback.h"
#include "ac6_decimal_parse.h"

#include <array>
#include <string_view>

PPC_EXTERN_FUNC(sub_82350008);
PPC_EXTERN_FUNC(sub_8237BFD8);
PPC_EXTERN_FUNC(sub_8237D1B8);
PPC_EXTERN_FUNC(sub_82383D50);
PPC_EXTERN_FUNC(rex_sub_8238F028);
PPC_EXTERN_FUNC(__savegprlr_28);
PPC_EXTERN_FUNC(__restgprlr_28);
PPC_EXTERN_FUNC(__restgprlr_27);
PPC_EXTERN_FUNC(sub_820F62B0);
#ifndef AC6RECOMP_PROBE_GUEST_TEXT
PPC_EXTERN_FUNC(__imp__sub_820F8608);
#endif

// The PAL atoi thunk at 0x82382480 tail-calls the common strtol worker with
// endptr=null and base=10. The generated worker currently returns errno EINVAL
// (22) for every valid decimal string, corrupting SWG message ids and numeric
// UI attributes. Preserve the thunk's exact public contract locally while the
// general locale-aware strtol worker remains isolated.
PPC_FUNC_IMPL(sub_82382480) {
  PPC_FUNC_PROLOGUE();
  std::array<char, 128> input{};
  size_t length = 0;
  if (ctx.r3.u32 != 0) {
    for (; length + 1 < input.size(); ++length) {
      input[length] = static_cast<char>(PPC_LOAD_U8(ctx.r3.u32 + length));
      if (input[length] == '\0') break;
    }
  }
  ctx.r3.s64 = ac6::decimal_parse::ParseAtoi(
      std::string_view(input.data(), length));
}

// The PAL XEX's indirect callback at 0x820F6180 is a four-instruction vtable
// thunk (vtable slot +0x74) that the generated function map does not expose.
// Keep the generated corpus untouched and provide the exact thunk here; the
// app registers this guest address after the dispatcher table is initialized.
PPC_FUNC_IMPL(sub_820F6180) {
  PPC_FUNC_PROLOGUE();
  ctx.r12.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
  ctx.r11.u64 = PPC_LOAD_U32(ctx.r12.u32 + 0x74);
  ctx.ctr.u64 = ctx.r11.u64;
  PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32);
}

// The PAL formatter's continuation at 0x8238EF90 is entered from the middle
// of sub_8238EEB0. ReXGlue emitted the backward edge to 0x8238EF78 as a fatal
// unresolved branch. Preserve the guest loop locally, then run the original
// suffix and shared epilogue through the generated continuation helper.
PPC_FUNC_IMPL(rex_sub_8238EF90) {
  PPC_FUNC_PROLOGUE();

  // 0x8238EF90..0x8238EFA0, with the backward edge to 0x8238EF78 kept as a
  // native loop. The caller has already loaded r10 from [r8].
  for (;;) {
    PPC_STORE_U8(ctx.r11.u32 + 0, ctx.r10.u8);
    ctx.xer.ca = ctx.r5.u32 > 0;
    ctx.r5.s64 -= 1;
    ctx.cr0.compare<int32_t>(ctx.r5.s32, 0, ctx.xer);
    ctx.r11.s64 += 1;
    if (!ctx.cr0.gt) {
      break;
    }

    ctx.r10.u64 = PPC_LOAD_U8(ctx.r8.u32 + 0);
    ctx.cr0.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
    if (ctx.cr0.eq) {
      ctx.r10.u64 = ctx.r9.u64;
    } else {
      ctx.r8.s64 += 1;
    }
  }

  // 0x8238EFA0..0x8238F028.
  ctx.cr6.compare<int32_t>(ctx.r5.s32, 0, ctx.xer);
  PPC_STORE_U8(ctx.r11.u32 + 0, ctx.r7.u8);
  if (!ctx.cr6.lt) {
    ctx.r10.u64 = PPC_LOAD_U8(ctx.r8.u32 + 0);
    ctx.r10.s64 = ctx.r10.s8;
    ctx.cr6.compare<int32_t>(ctx.r10.s32, 53, ctx.xer);
    if (!ctx.cr6.lt) {
      // 0x8238EFC0..0x8238EFD8: carry through trailing '9' digits, then
      // increment the first non-'9' byte.
      for (;;) {
        ctx.r11.s64 -= 1;
        ctx.r10.u64 = PPC_LOAD_U8(ctx.r11.u32 + 0);
        ctx.cr6.compare<uint32_t>(ctx.r10.u32, 57, ctx.xer);
        if (!ctx.cr6.eq) {
          ctx.r10.u64 = (ctx.r10.u32 & 0xFFu) + 1;
          PPC_STORE_U8(ctx.r11.u32 + 0, ctx.r10.u8);
          break;
        }
        PPC_STORE_U8(ctx.r11.u32 + 0, ctx.r9.u8);
      }
    }
  }

  // 0x8238EFE0 onward.
  ctx.r11.u64 = PPC_LOAD_U8(ctx.r3.u32 + 0);
  ctx.cr6.compare<uint32_t>(ctx.r11.u32, 49, ctx.xer);
  if (ctx.cr6.eq) {
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r6.u32 + 4) + 1;
    PPC_STORE_U32(ctx.r6.u32 + 4, ctx.r11.u32);
    rex_sub_8238F028(ctx, base);
    return;
  }

  ctx.r11.u64 = ctx.r4.u64;
  ctx.r10.u64 = ctx.r11.u64;
  do {
    ctx.r9.u64 = PPC_LOAD_U8(ctx.r11.u32 + 0);
    ctx.r11.s64 += 1;
    ctx.cr6.compare<uint32_t>(ctx.r9.u32, 0, ctx.xer);
  } while (!ctx.cr6.eq);

  ctx.r11.u64 = ctx.r11.u64 - ctx.r10.u64 - 1;
  ctx.r5.s64 = ctx.r11.s64 + 1;
  ctx.lr = 0x8238F028;
  sub_82383D50(ctx, base);
  rex_sub_8238F028(ctx, base);
}

// The diagnostic text build already owns this strong wrapper so it can record
// the renderer boundary. Normal builds still need the exact PAL dialog-text
// fallback before entering the generated implementation.
#ifndef AC6RECOMP_PROBE_GUEST_TEXT
PPC_FUNC_IMPL(sub_820F8608) {
  PPC_FUNC_PROLOGUE();
  ac6::dialog_text::ApplyFallback(ctx, base);
  __imp__sub_820F8608(ctx, base);
}
#endif

// ReXGlue splits the first backward loop at 0x8234B9A8 and rejects the
// otherwise valid edge from 0x8234B9C0 to 0x8234B9A0.
PPC_FUNC_IMPL(sub_8234B978) {
  PPC_FUNC_PROLOGUE();

  ctx.r12.u64 = ctx.lr;
  ctx.lr = 0x8234B980;
  __savegprlr_28(ctx, base);
  const uint32_t stack = ctx.r1.u32 - 128;
  PPC_STORE_U32(stack, ctx.r1.u32);
  ctx.r1.u32 = stack;

  ctx.r30.u64 = ctx.r5.u64;
  ctx.r29.u64 = ctx.r3.u64;
  ctx.r28.u64 = ctx.r4.u64;
  PPC_STORE_U32(ctx.r30.u32, 0);

  ctx.r31.u64 = PPC_LOAD_U32(ctx.r29.u32 + 16);
  while (ctx.r31.u32 != 0) {
    ctx.r5.u64 = ctx.r30.u64;
    ctx.r4.u64 = ctx.r28.u64;
    ctx.r3.u64 = ctx.r31.u64;
    ctx.lr = 0x8234B9B0;
    sub_82350008(ctx, base);
    ctx.cr0.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
    if (!ctx.cr0.lt) {
      ctx.r3.s64 = 0;
      ctx.r1.s64 += 128;
      __restgprlr_28(ctx, base);
      return;
    }
    ctx.r31.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);
  }

  ctx.r31.u64 = PPC_LOAD_U32(ctx.r29.u32 + 20);
  while (ctx.r31.u32 != 0) {
    ctx.r5.u64 = ctx.r30.u64;
    ctx.r4.u64 = ctx.r28.u64;
    ctx.r3.u64 = ctx.r31.u64;
    ctx.lr = 0x8234B9E4;
    sub_82350008(ctx, base);
    ctx.cr0.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
    if (!ctx.cr0.lt) {
      ctx.r3.s64 = 0;
      ctx.r1.s64 += 128;
      __restgprlr_28(ctx, base);
      return;
    }
    ctx.r31.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);
  }

  ctx.r3.u64 = 0x80004005;
  ctx.r1.s64 += 128;
  __restgprlr_28(ctx, base);
}

// Entry 0x8234B9A8 is inside sub_8234B978 and loops back to 0x8234B9A0.
// ReXGlue emits it as a separate weak function, so its backward edge cannot
// be represented as a C++ label. Preserve the guest fragment as a strong
// override instead of weakening unresolved-branch validation globally.
PPC_FUNC_IMPL(rex_sub_8234B9A8) {
  PPC_FUNC_PROLOGUE();

  do {
    ctx.r3.u64 = ctx.r31.u64;
    ctx.lr = 0x8234B9B0;
    sub_82350008(ctx, base);
    ctx.cr0.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
    if (!ctx.cr0.lt) {
      ctx.r3.s64 = 0;
      break;
    }
    ctx.r31.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);
  } while (ctx.r31.u32 != 0);

  if (ctx.r3.s32 < 0) {
    ctx.r31.u64 = PPC_LOAD_U32(ctx.r29.u32 + 20);
    while (ctx.r31.u32 != 0) {
      ctx.r5.u64 = ctx.r30.u64;
      ctx.r4.u64 = ctx.r28.u64;
      ctx.r3.u64 = ctx.r31.u64;
      ctx.lr = 0x8234B9E4;
      sub_82350008(ctx, base);
      ctx.cr0.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
      if (!ctx.cr0.lt) {
        ctx.r3.s64 = 0;
        break;
      }
      ctx.r31.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);
    }
    if (ctx.r31.u32 == 0) {
      ctx.r3.u64 = 0x80004005;
    }
  }

  ctx.r1.s64 += 128;
  __restgprlr_28(ctx, base);
}

// The generated body for 0x820E5F38 is a small range walk over the callback's
// object array at +48..+52.  Its final back-edge (0x820E6018 -> 0x820E5F74)
// was emitted as an unresolved call, so the first type-4 mission child traps
// after the array reaches its end even though its vtable target is valid.
// Keep the generated corpus untouched and provide the exact loop as a strong
// override.  The two guest helpers retain the original per-item side effects:
// initialize an item whose +420 flag is clear, then publish it when set.
PPC_FUNC_IMPL(sub_820E5F38) {
  PPC_FUNC_PROLOGUE();
  uint32_t ea{};

  ctx.r12.u64 = ctx.lr;
  ctx.lr = 0x820E5F40;
  __savegprlr_28(ctx, base);
  ea = ctx.r1.u32 - 128;
  PPC_STORE_U32(ea, ctx.r1.u32);
  ctx.r1.u32 = ea;

  ctx.r29.u64 = ctx.r3.u64;
  ctx.r30.s64 = ctx.r29.s64 + 44;
  ctx.r31.u64 = PPC_LOAD_U32(ctx.r30.u32 + 4);
  const uint32_t end = PPC_LOAD_U32(ctx.r30.u32 + 8);

  if (ctx.r31.u32 <= end) {
    while (ctx.r31.u32 != end) {
      const uint32_t item = PPC_LOAD_U32(ctx.r31.u32);
      if (item) {
        ctx.r3.u64 = item;
        if (PPC_LOAD_U8(item + 420) == 0) {
          ctx.r4.u64 = item;
          ctx.r3.u64 = PPC_LOAD_U32(ctx.r29.u32 + 36);
          ctx.lr = 0x820E5FD0;
          sub_8237D1B8(ctx, base);
        }

        // The guest reloads the item after initialization.  Do the same so a
        // helper that replaces the array slot is observed by the publish path.
        const uint32_t current = PPC_LOAD_U32(ctx.r31.u32);
        if (current && PPC_LOAD_U8(current + 420) != 0) {
          ctx.r3.u64 = current;
          ctx.r4.s64 = 0;
          ctx.lr = 0x820E6004;
          sub_8237BFD8(ctx, base);
        }
      }
      ctx.r31.s64 += 4;
    }
  }

  ctx.r28.u64 = end;
  ctx.r31.u64 = end;
  ctx.r1.s64 += 128;
  __restgprlr_28(ctx, base);
}

// The generated 0x820F6920 loop splits the non-empty-table path at
// 0x820F69C8, whose branch back to 0x820F6990 was emitted as an unresolved
// call.  0x6990 is the body that invokes one non-null slot and resumes the
// scan.  Re-enter that body here with the registers established by the
// generated prologue, preserving the 16-entry table walk and its epilogue.
PPC_FUNC_IMPL(rex_sub_820F69C8) {
  PPC_FUNC_PROLOGUE();

  // The split edge arrives after the search found the slot at r10 and set the
  // callback-present flag in r11.
  ctx.r31.u64 = ctx.r10.u64;
  ctx.r11.s64 = 1;

  for (;;) {
    ctx.r11.u64 &= 0xFFu;
    if (ctx.r11.u32 == 0) {
      break;
    }

    const uint32_t slot_offset = (ctx.r31.u32 << 2) & 0xFFFFFFFCu;
    ctx.r6.u64 = ctx.r29.u64;
    ctx.r5.u64 = ctx.r1.u32 + 80;
    ctx.r4.u64 = ctx.r28.u64;
    ctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + slot_offset);
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
    ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 36);
    ctx.ctr.u64 = ctx.r11.u64;
    PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32);
    ctx.r31.s64 += 1;

    // Reproduce 0x820F6958..0x820F698C: find the next non-null slot.
    ctx.r10.u64 = ctx.r31.u64;
    bool found = false;
    for (; ctx.r10.s32 < 16; ++ctx.r10.s32) {
      const uint32_t offset = (ctx.r10.u32 << 2) & 0xFFFFFFFCu;
      if (PPC_LOAD_U32(ctx.r30.u32 + offset) != 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      ctx.r11.u64 = 0;
      break;
    }
    ctx.r31.u64 = ctx.r10.u64;
    ctx.r11.u64 = 1;
  }

  ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 80);
  PPC_STORE_U32(ctx.r27.u32 + 0, ctx.r11.u32);
  ctx.r1.s64 += 144;
  __restgprlr_27(ctx, base);
}
