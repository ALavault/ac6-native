#pragma once

// Native overrides for the two qualified loader helpers.  This file is
// included from guest_bridge.cpp after the generated namespace closes, so the
// definitions retain access to the translation unit's bridge helpers while
// keeping the dispatch entry point small enough for the source-size audit.
void sub_82279D08(PPCContext &context, std::uint8_t *base) {
  (void)base;
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_82279D08");
  auto &memory = memory_for(context);
  const auto state = context.r3.u32;
  const auto base_offset = memory.load_u32(state + 56U);

  for (;;) {
    const auto cursor = memory.load_u64(state + 22400U);
    const auto limit = memory.load_u64(state + 16U);
    if (cursor >= limit) {
      return;
    }
    if (memory.load_u32(state + 22388U) == 0U && limit - cursor >= 8U) {
      const auto target = static_cast<std::uint32_t>(cursor) + base_offset;
      memory.store_u64(target, memory.load_u64(target) ^
                                  memory.load_u64(state + 22380U));
      memory.store_u64(state + 22400U, cursor + 8U);
      continue;
    }

    const auto key_index = memory.load_u32(state + 22388U);
    const auto key = memory.load_u8(state + 22380U + key_index);
    const auto target = static_cast<std::uint32_t>(cursor) + base_offset;
    memory.store_u8(target, memory.load_u8(target) ^ key);
    memory.store_u32(state + 22388U, key_index + 1U);
    memory.store_u64(state + 22400U, cursor + 1U);
  }
}

// Native orchestration for the qualified key producer.  The two arithmetic
// children remain generated until their own PPC boundaries are translated.
void sub_8227A5E0(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8227A5E0");
  auto &memory = memory_for(context);
  const auto state = context.r3.u32;
  const auto caller_r1 = context.r1.u32;
  const auto frame = caller_r1 - 8528U;
  const auto saved_r28 = context.r28;
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;

  memory.store_u32(frame, caller_r1);
  context.r1.u32 = frame;
  context.r29.u64 = context.r3.u64;
  if (context.r5.s32 < 0) {
    context.r5.u64 = memory.load_u32(state + 22392U);
  }
  context.r28.u64 = context.r29.u64 + 22380U;
  if (context.r4.u32 != 0U) {
    context.r28.u64 = context.r4.u64;
  }
  context.r11.u64 = context.r5.u64;
  context.r6.s64 = 4;
  context.r30.u64 =
      std::rotl(context.r11.u32 | (context.r11.u64 << 32), 4) & 0xFF0U;
  context.r5.s64 = 5;
  context.r11.s64 = context.r30.s64 + 40;
  context.r4.s64 = context.r1.s64 + 80;
  context.r31.u64 =
      std::rotl(context.r11.u32 | (context.r11.u64 << 32), 29) & 0x1FFFFFFFU;
  context.r3.u64 = context.r29.u64;
  context.r7.u64 = context.r31.u64;
  context.lr = 0x8227A63CU;
  sub_82279D98(context, base);

  context.r7.u64 = context.r31.u64;
  context.r6.s64 = 1;
  context.r5.s64 = 239;
  context.r4.s64 = context.r1.s64 + 4288;
  context.r3.u64 = context.r29.u64;
  context.lr = 0x8227A654U;
  sub_82279D98(context, base);

  context.r6.u64 = context.r31.u64;
  context.r5.s64 = context.r1.s64 + 4288;
  context.r4.s64 = context.r1.s64 + 80;
  context.r3.u64 = context.r29.u64;
  context.lr = 0x8227A668U;
  sub_822785C8(context, base);

  context.r11.s64 = context.r31.s64 - 1;
  context.r9.s64 = 0;
  if (context.r11.s32 >= 0) {
    context.r10.u64 =
        std::rotl(context.r11.u32 | (context.r11.u64 << 32), 3) & 0xFFFFFFF8U;
    context.r8.s64 = context.r1.s64 + 80;
    context.r10.u64 += context.r8.u64;
    do {
      context.r8.u64 = memory.load_u64(context.r10.u32);
      context.r11.s64 -= 1;
      context.r8.u64 = std::rotl(context.r8.u64, 2) & 0xFFFFFFFFFFFFFFFCU;
      context.r9.u64 += context.r8.u64;
      context.r8.u64 = context.r9.u64 & 0xFFFFFFFFU;
      context.r9.u64 = std::rotl(context.r9.u64, 32) & 0xFFFFFFFFU;
      memory.store_u64(context.r10.u32, context.r8.u64);
      context.r10.s64 -= 8;
    } while (context.r11.s32 >= 0);
  }

  context.r11.s64 = context.r30.s64 + 16;
  context.r8.s64 = context.r1.s64 + 85;
  context.r11.u64 =
      std::rotl(context.r11.u32 | (context.r11.u64 << 32), 0) & 0xFFFFFFF8U;
  context.r10.s64 = context.r28.s64 + 2;
  context.r9.s64 = 2;
  context.r11.u64 += context.r8.u64;
  do {
    context.r8.u64 = memory.load_u8(context.r11.u32 - 1U);
    context.r7.s64 = -5;
    context.r6.u64 = memory.load_u8(context.r11.u32);
    context.r9.s64 -= 1;
    context.r5.u64 = memory.load_u8(context.r11.u32 + 1U);
    memory.store_u8(context.r10.u32 - 2U, context.r8.u8);
    context.r8.u64 = memory.load_u64(context.r11.u32 + context.r7.u32);
    context.r11.s64 += 8;
    memory.store_u8(context.r10.u32 - 1U, context.r6.u8);
    memory.store_u8(context.r10.u32, context.r5.u8);
    memory.store_u8(context.r10.u32 + 1U, context.r8.u8);
    context.r10.s64 += 4;
  } while (context.r9.s32 != 0);

  memory.store_u32(state + 22388U, 0U);
  context.r1.u32 = caller_r1;
  context.r28 = saved_r28;
  context.r29 = saved_r29;
  context.r30 = saved_r30;
  context.r31 = saved_r31;
  context.lr = saved_lr;
}
