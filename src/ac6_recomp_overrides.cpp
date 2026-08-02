#include <rex/memory.h>
#include <rex/ppc.h>

PPC_EXTERN_FUNC(sub_82350008);
PPC_EXTERN_FUNC(__savegprlr_28);
PPC_EXTERN_FUNC(__restgprlr_28);

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
