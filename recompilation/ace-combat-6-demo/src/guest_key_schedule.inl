// Native orchestration for the key-schedule arithmetic helper.  The memory
// fill/copy and multi-precision children stay generated until their own
// boundaries are qualified.
void sub_823273E0(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_823273E0");
  auto &memory = memory_for(context);
  const auto destination = context.r3.u32;
  const auto value = context.r4.u8;
  const auto count = context.r5.u32;
  for (std::uint32_t index = 0; index < count; ++index) {
    memory.store_u8(destination + index, value);
  }
  context.r3.u64 = destination;
}

void sub_82327D90(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_82327D90");
  auto &memory = memory_for(context);
  const auto destination = context.r3.u32;
  const auto source = context.r4.u32;
  const auto count = context.r5.u32;
  for (std::uint32_t index = 0; index < count; ++index) {
    memory.store_u8(destination + index, memory.load_u8(source + index));
  }
  context.r3.u64 = destination;
}

void sub_82278538(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_82278538");
  auto &memory = memory_for(context);
  context.r7.s64 = 0;
  if (context.r6.s32 <= 0) {
    return;
  }

  context.r11.s64 = -1;
  context.r9.u64 = context.r4.u64;
  context.r5.s64 -= context.r4.s64;
  context.r10.u64 = context.r11.u64 & 0xFFFFFFFFULL;
  do {
    context.r11.u64 =
        memory.load_u64(context.r5.u32 + context.r9.u32);
    context.r8.u64 = memory.load_u64(context.r9.u32);
    context.r11.u64 += context.r8.u64;
    if (context.r11.u64 <= context.r10.u64) {
      memory.store_u64(context.r9.u32, context.r11.u64);
    } else {
      context.r3.u64 = context.r11.u64 & 0xFFFFFFFFULL;
      context.r8.s64 = context.r9.s64 - 8;
      context.r11.s64 = context.r7.s64 - 1;
      memory.store_u64(context.r9.u32, context.r3.u64);
      context.r3.u64 = memory.load_u64(context.r8.u32);
      while (context.r3.u64 == context.r10.u64) {
        context.r3.u64 = 0;
        context.r11.s64 -= 1;
        memory.store_u64(context.r8.u32, context.r3.u64);
        context.r8.u64 =
            (std::rotl(context.r11.u32 | (context.r11.u64 << 32), 3) &
             0xFFFFFFF8ULL) +
            context.r4.u64;
        context.r3.u64 = memory.load_u64(context.r8.u32);
      }
      context.r11.u64 =
          std::rotl(context.r11.u32 | (context.r11.u64 << 32), 3) &
          0xFFFFFFF8ULL;
      context.r8.u64 = memory.load_u64(context.r11.u32 + context.r4.u32);
      context.r8.u64 += 1;
      memory.store_u64(context.r11.u32 + context.r4.u32, context.r8.u64);
    }
    context.r7.s64 += 1;
    context.r9.s64 += 8;
  } while (context.r7.s32 < context.r6.s32);
}

void sub_822785C8(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_822785C8");
  auto &memory = memory_for(context);
  context.r7.s64 = 0;
  if (context.r6.s32 <= 0) {
    return;
  }

  context.r11.u64 = context.r4.u64;
  context.r5.s64 -= context.r4.s64;
  do {
    context.r10.u64 = memory.load_u64(context.r11.u32);
    context.r9.u64 =
        memory.load_u64(context.r5.u32 + context.r11.u32);
    if (context.r10.u64 >= context.r9.u64) {
      context.r10.u64 -= context.r9.u64;
      memory.store_u64(context.r11.u32, context.r10.u64);
    } else {
      context.r12.u64 = 1;
      context.r9.u64 = context.r10.u64 - context.r9.u64;
      context.r12.u64 = std::rotl(context.r12.u64, 32);
      context.r8.s64 = context.r11.s64 - 8;
      context.r9.u64 += context.r12.u64;
      context.r10.s64 = context.r7.s64 - 1;
      memory.store_u64(context.r11.u32, context.r9.u64);
      context.r9.u64 = memory.load_u64(context.r8.u32);
      if (context.r9.u64 == 0) {
        context.r9.u64 = context.r8.u64;
        context.r8.u64 = 0xFFFFFFFFULL;
        context.r10.s64 -= 1;
        memory.store_u64(context.r9.u32, context.r8.u64);
        context.r9.u64 =
            (std::rotl(context.r10.u32 | (context.r10.u64 << 32), 3) &
             0xFFFFFFF8ULL) +
            context.r4.u64;
        context.r8.u64 = memory.load_u64(context.r9.u32);
        while (context.r8.u64 == 0) {
          context.r8.u64 = 0xFFFFFFFFULL;
          context.r10.s64 -= 1;
          memory.store_u64(context.r9.u32, context.r8.u64);
          context.r9.u64 =
              (std::rotl(context.r10.u32 | (context.r10.u64 << 32), 3) &
               0xFFFFFFF8ULL) +
              context.r4.u64;
          context.r8.u64 = memory.load_u64(context.r9.u32);
        }
      }
      context.r10.u64 =
          std::rotl(context.r10.u32 | (context.r10.u64 << 32), 3) &
          0xFFFFFFF8ULL;
      context.r9.u64 = memory.load_u64(context.r10.u32 + context.r4.u32);
      context.r9.u64 -= 1;
      memory.store_u64(context.r10.u32 + context.r4.u32, context.r9.u64);
    }
    context.r7.s64 += 1;
    context.r11.s64 += 8;
  } while (context.r7.s32 < context.r6.s32);
}

void sub_82279D98(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_82279D98");
  auto &memory = memory_for(context);
  const auto caller_r1 = context.r1.u32;
  const auto frame = caller_r1 - 8560U;
  const auto saved_r24 = context.r24;
  const auto saved_r25 = context.r25;
  const auto saved_r26 = context.r26;
  const auto saved_r27 = context.r27;
  const auto saved_r28 = context.r28;
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;

  memory.store_u32(frame, caller_r1);
  context.r1.u32 = frame;
  context.r25.u64 = context.r3.u64;
  context.r26.u64 = context.r4.u64;
  context.r31.u64 = context.r5.u64;
  context.r5.s64 = 4200;
  context.r4.s64 = 0;
  context.r3.s64 = context.r1.s64 + 80;
  context.r24.u64 = context.r6.u64;
  context.r30.u64 = context.r7.u64;
  context.lr = 0x82279DD0U;
  sub_823273E0(context, base);

  context.r5.s64 = 4200;
  context.r4.s64 = 0;
  context.r3.s64 = context.r1.s64 + 4288;
  context.lr = 0x82279DE0U;
  sub_823273E0(context, base);

  context.r28.s64 = 1;
  context.r27.u64 = context.r31.u64 * context.r31.u64;
  memory.store_u64(context.r1.u32 + 80U, context.r28.u64);
  context.r9.s64 = 0;
  if (context.r30.s32 > 0) {
    if (context.r31.s64 <= 0) {
      throw ac6demo::RuntimeTrap("sub_82279D98: invalid seed divisor",
                                 require_bridge().tick(),
                                 static_cast<std::uint32_t>(context.lr),
                                 0x82279D98U);
    }
    context.r11.s64 = context.r1.s64 + 80;
    context.r10.u64 = context.r30.u64;
    do {
      context.r8.u64 = memory.load_u64(context.r11.u32);
      context.r9.u64 =
          std::rotl(context.r9.u64, 32) & 0xFFFFFFFF00000000ULL;
      context.r10.s64 -= 1;
      context.r9.u64 += context.r8.u64;
      context.r8.u64 = context.r9.u64 / context.r31.u64;
      context.r7.u64 = context.r8.u64 * context.r31.u64;
      memory.store_u64(context.r11.u32, context.r8.u64);
      context.r9.u64 -= context.r7.u64;
      context.r11.s64 += 8;
    } while (context.r10.s32 != 0);
  }

  context.r29.u64 =
      std::rotl(context.r30.u32 | (context.r30.u64 << 32), 3) & 0xFFFFFFF8U;
  context.r4.s64 = context.r1.s64 + 80;
  context.r3.u64 = context.r26.u64;
  context.r5.u64 = context.r29.u64;
  context.lr = 0x82279E40U;
  sub_82327D90(context, base);

  context.r31.s64 = 3;
  do {
    context.r9.s64 = 0;
    if (context.r30.s32 > 0) {
      if (context.r27.s64 <= 0) {
        throw ac6demo::RuntimeTrap("sub_82279D98: invalid square divisor",
                                   require_bridge().tick(),
                                   static_cast<std::uint32_t>(context.lr),
                                   0x82279D98U);
      }
      context.r11.s64 = context.r1.s64 + 80;
      context.r10.u64 = context.r30.u64;
      do {
        context.r8.u64 = memory.load_u64(context.r11.u32);
        context.r9.u64 =
            std::rotl(context.r9.u64, 32) & 0xFFFFFFFF00000000ULL;
        context.r10.s64 -= 1;
        context.r9.u64 += context.r8.u64;
        context.r8.u64 = context.r9.u64 / context.r27.u64;
        context.r7.u64 = context.r8.u64 * context.r27.u64;
        memory.store_u64(context.r11.u32, context.r8.u64);
        context.r9.u64 -= context.r7.u64;
        context.r11.s64 += 8;
      } while (context.r10.s32 != 0);
    }

    context.r5.u64 = context.r29.u64;
    context.r4.s64 = context.r1.s64 + 80;
    context.r3.s64 = context.r1.s64 + 4288;
    context.lr = 0x82279E94U;
    sub_82327D90(context, base);

    context.r7.u64 = context.r31.u32;
    context.r9.s64 = 0;
    if (context.r30.s32 > 0) {
      context.r11.s64 = context.r1.s64 + 4288;
      context.r10.u64 = context.r30.u64;
      do {
        context.r8.u64 = memory.load_u64(context.r11.u32);
        context.r9.u64 =
            std::rotl(context.r9.u64, 32) & 0xFFFFFFFF00000000ULL;
        context.r10.s64 -= 1;
        context.r9.u64 += context.r8.u64;
        context.r8.u64 = context.r9.u64 / context.r7.u64;
        context.r6.u64 = context.r8.u64 * context.r7.u64;
        memory.store_u64(context.r11.u32, context.r8.u64);
        context.r9.u64 -= context.r6.u64;
        context.r11.s64 += 8;
      } while (context.r10.s32 != 0);
    }

    context.r11.u64 = context.r28.u32 & 1U;
    context.r6.u64 = context.r30.u64;
    context.r5.s64 = context.r1.s64 + 4288;
    context.r4.u64 = context.r26.u64;
    context.r3.u64 = context.r25.u64;
    if (context.r11.u32 == 0U) {
      context.lr = 0x82279EF4U;
      sub_82278538(context, base);
    } else {
      context.lr = 0x82279EFCU;
      sub_822785C8(context, base);
    }

    context.r28.s64 += 1;
    context.r31.s64 += 2;
    context.r9.s64 = 0;
    if (context.r30.s32 <= 0) {
      break;
    }
    context.r10.s64 = context.r1.s64 + 4288;
    context.r11.u64 = context.r30.u64;
    do {
      context.r8.u64 = memory.load_u64(context.r10.u32);
      context.r11.s64 -= 1;
      context.r10.s64 += 8;
      context.r9.u64 |= context.r8.u64;
    } while (context.r11.s32 != 0);
    if (context.r9.u32 == 0U) {
      break;
    }
  } while (true);

  if (context.r24.s64 > 1) {
    context.r11.s64 = context.r30.s64 - 1;
    context.r9.s64 = 0;
    if (context.r11.s32 >= 0) {
      context.r10.u64 =
          std::rotl(context.r11.u32 | (context.r11.u64 << 32), 3) &
          0xFFFFFFF8U;
      context.r10.u64 += context.r26.u64;
      do {
        context.r8.u64 = memory.load_u64(context.r10.u32);
        context.r11.s64 -= 1;
        context.r8.u64 = context.r24.u64 * context.r8.u64;
        context.r9.u64 += context.r8.u64;
        context.r8.u64 = context.r9.u64 & 0xFFFFFFFFU;
        context.r9.u64 = std::rotl(context.r9.u64, 32) & 0xFFFFFFFFU;
        memory.store_u64(context.r10.u32, context.r8.u64);
        context.r10.s64 -= 8;
      } while (context.r11.s32 >= 0);
    }
  }

  context.r1.u32 = caller_r1;
  context.r24 = saved_r24;
  context.r25 = saved_r25;
  context.r26 = saved_r26;
  context.r27 = saved_r27;
  context.r28 = saved_r28;
  context.r29 = saved_r29;
  context.r30 = saved_r30;
  context.r31 = saved_r31;
  context.lr = saved_lr;
}
