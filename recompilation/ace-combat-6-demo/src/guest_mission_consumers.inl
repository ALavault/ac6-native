void sub_82095B80(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_82095B80");
  auto &memory = memory_for(context);
  const auto state = context.r3.u32;
  const auto mode = memory.load_u32(state + 8U);
  if (mode == 2U) {
    context.r3.u32 = memory.load_u32(state + 0x0CU);
  } else if (mode == 3U) {
    context.r3.u32 = memory.load_u32(state + 0x20U);
  } else if (mode == 4U) {
    context.r3.u32 = memory.load_u32(state + 0x1CU);
  } else if (mode == 5U) {
    context.r3.u32 = memory.load_u32(state + 0x14U);
  } else {
    const auto raw_index = memory.load_u32(state + 0x206E4U);
    const auto index = raw_index <= 2U ? raw_index : 0U;
    context.r3.u32 = memory.load_u32(state + 0x6C4U + index * 0xAAB8U);
  }
}

void sub_820E9290(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_820E9290");
  auto &memory = memory_for(context);
  const auto state = context.r3.u32;
  const auto mode = memory.load_u32(state + 8U);
  if (mode == 2U) {
    context.r3.u32 = memory.load_u32(state + 0x10U);
  } else if (mode == 5U) {
    context.r3.u32 = memory.load_u32(state + 0x18U);
  } else if (mode == 4U) {
    context.r3.u32 = 2U;
  } else {
    const auto raw_index = memory.load_u32(state + 0x206E4U);
    const auto index = raw_index <= 2U ? raw_index : 0U;
    context.r3.u32 = memory.load_u32(state + 0x6D0U + index * 0xAAB8U);
  }
}

void sub_820E9300(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_820E9300");
  auto &memory = memory_for(context);
  const auto state = context.r3.u32;
  if (memory.load_u32(state + 8U) != 1U) {
    context.r3.u32 = 0U;
    return;
  }

  sub_82095B80(context, base);
  if (context.r3.u32 != 2U) {
    context.r3.u32 = 0U;
    return;
  }
  const auto raw_index = memory.load_u32(state + 0x206E4U);
  const auto index = raw_index <= 2U ? raw_index : 0U;
  context.r3.u32 = memory.load_u32(state + 0x6C8U + index * 0xAAB8U) == 0U ? 1U : 0U;
}

void sub_8218E088(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8218E088");
  auto &memory = memory_for(context);
  if (context.r3.s32 < 2) {
    const auto offset = (context.r3.u32 << 2) & 0xFFFFFFFCU;
    context.r3.u32 = memory.load_u32(0x82391D34U + offset);
  } else {
    context.r3.u32 = 0U;
  }
}

void sub_8218DF70(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8218DF70");
  auto &memory = memory_for(context);
  if (context.r3.s32 < 2) {
    const auto offset = (context.r3.u32 << 2) & 0xFFFFFFFCU;
    context.r3.u32 = memory.load_u32(0x82391CDCU + offset);
  } else {
    context.r3.u32 = 0U;
  }
}

void sub_8218EA88(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8218EA88");
  memory_for(context).store_u32(context.r3.u32 + 0x10U, context.r4.u32);
}

void sub_8219EAA0(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219EAA0");
  auto &memory = memory_for(context);
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto key = context.r3.u32;
  auto node = context.r4.u32;

  if (key != 0U) {
    while (node != 0U) {
      const auto element = memory.load_u32(node);
      if (element != 0U) {
        const auto target = memory.load_u32(memory.load_u32(element) + 4U);
        context.r3.u32 = element;
        context.lr = 0x8219EAE8U;
        AC6_PPC_CALL_INDIRECT(context, memory.raw_base(), target);
        if (context.r3.u32 == key) {
          context.r3.u32 = node;
          context.r29 = saved_r29;
          context.r30 = saved_r30;
          context.r31 = saved_r31;
          context.lr = saved_lr;
          return;
        }
      }
      node = memory.load_u32(node + 4U);
    }
  }

  context.r3.u32 = 0U;
  context.r29 = saved_r29;
  context.r30 = saved_r30;
  context.r31 = saved_r31;
  context.lr = saved_lr;
}

void sub_8219ECF8(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219ECF8");
  auto &memory = memory_for(context);
  const auto saved_r28 = context.r28;
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto element = context.r4.u32;

  if (element == 0U) {
    context.r3.u32 = 0U;
    context.r28 = saved_r28;
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
    return;
  }

  const auto element_vtable = memory.load_u32(element);
  context.r3.u32 = element;
  context.lr = 0x8219ED34U;
  AC6_PPC_CALL_INDIRECT(context, memory.raw_base(),
                        memory.load_u32(element_vtable + 4U));
  const auto key = context.r3.u32;

  context.r3.u32 = key;
  context.r4.u32 = memory.load_u32(object + 0x1CU);
  context.lr = 0x8219ED3CU;
  sub_8219EAA0(context, base);
  const auto node = context.r3.u32;
  if (node != 0U) {
    const auto node_element = memory.load_u32(node);
    if (node_element == 0U) {
      context.r3.u32 = element;
      context.lr = 0x8219ED64U;
      AC6_PPC_CALL_INDIRECT(context, memory.raw_base(),
                            memory.load_u32(memory.load_u32(element) + 4U));
      context.r3.u32 = 0U;
      context.r28 = saved_r28;
      context.r29 = saved_r29;
      context.r30 = saved_r30;
      context.r31 = saved_r31;
      context.lr = saved_lr;
      return;
    }

    context.r3.u32 = node_element;
    context.lr = 0x8219ED78U;
    AC6_PPC_CALL_INDIRECT(context, memory.raw_base(),
                          memory.load_u32(memory.load_u32(node_element) + 20U));
    if (context.r3.s32 <= 0) {
      context.r3.u32 = node_element;
      context.lr = 0x8219ED94U;
      AC6_PPC_CALL_INDIRECT(
          context, memory.raw_base(),
          memory.load_u32(memory.load_u32(node_element) + 56U));
    }
    context.r3.u32 = node_element;
    context.lr = 0x8219EDA8U;
    AC6_PPC_CALL_INDIRECT(
        context, memory.raw_base(),
        memory.load_u32(memory.load_u32(node_element) + 48U));
  } else {
    context.r3.u32 = element;
    context.lr = 0x8219EDC0U;
    AC6_PPC_CALL_INDIRECT(context, memory.raw_base(),
                          memory.load_u32(memory.load_u32(element) + 56U));
    context.r3.u32 = element;
    context.lr = 0x8219EDD4U;
    AC6_PPC_CALL_INDIRECT(context, memory.raw_base(),
                          memory.load_u32(memory.load_u32(element) + 48U));

    const auto free_head = memory.load_u32(0x827745ECU);
    if (free_head == 0U) {
      context.r3.u32 = 0U;
      context.r28 = saved_r28;
      context.r29 = saved_r29;
      context.r30 = saved_r30;
      context.r31 = saved_r31;
      context.lr = saved_lr;
      return;
    }
    const auto free_next = memory.load_u32(free_head + 4U);
    memory.store_u32(0x827745ECU, free_next);

    context.r3.u32 = element;
    context.r4.u32 = object;
    context.lr = 0x8219EE10U;
    AC6_PPC_CALL_INDIRECT(context, memory.raw_base(),
                          memory.load_u32(memory.load_u32(element) + 44U));
    memory.store_u32(free_head, element);
    memory.store_u32(free_head + 4U, memory.load_u32(object + 0x1CU));
    memory.store_u32(object + 0x1CU, free_head);
  }

  context.r3.u32 = element;
  context.lr = 0x8219EE34U;
  AC6_PPC_CALL_INDIRECT(context, memory.raw_base(),
                        memory.load_u32(memory.load_u32(element) + 4U));
  context.r28 = saved_r28;
  context.r29 = saved_r29;
  context.r30 = saved_r30;
  context.r31 = saved_r31;
  context.lr = saved_lr;
}

void sub_82311960(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_82311960");
  context.r3.u32 = memory_for(context).load_u32(context.r3.u32 + 4U);
}

void sub_820D2C60(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_820D2C60");
  context.r3.u32 = memory_for(context).load_u32(context.r3.u32 + 8U);
}

template <typename Memory>
static void ac6_mission_append_node(Memory &memory, std::uint32_t node,
                                    std::uint32_t list_address) {
  if (node == 0U) {
    return;
  }
  const auto head = memory.load_u32(list_address);
  if (head == 0U) {
    memory.store_u32(node + 4U, 0U);
    memory.store_u32(list_address, node);
    return;
  }
  auto tail = head;
  for (;;) {
    const auto next = memory.load_u32(tail + 4U);
    if (next == 0U) {
      memory.store_u32(tail + 4U, node);
      memory.store_u32(node + 4U, 0U);
      return;
    }
    tail = next;
  }
}

template <typename Memory>
static std::uint32_t ac6_mission_take_node(Memory &memory,
                                            std::uint32_t object,
                                            std::uint32_t element) {
  const auto free_head = memory.load_u32(0x827745E0U);
  if (free_head == 0U) {
    return 0U;
  }
  memory.store_u32(0x827745E0U, memory.load_u32(free_head + 4U));
  memory.store_u32(free_head, element);
  ac6_mission_append_node(memory, free_head, object + 0x20U);
  return element;
}

template <typename Memory>
static void ac6_mission_dispatch(PPCContext &context, std::uint8_t *base,
                                  Memory &memory, std::uint32_t object,
                                  std::uint32_t slot_offset,
                                  std::uint32_t return_address,
                                  std::uint32_t argument) {
  context.r3.u32 = object;
  context.r4.u32 = argument;
  context.lr = return_address;
  const auto target = memory.load_u32(memory.load_u32(object) + slot_offset);
  AC6_PPC_CALL_INDIRECT(context, memory.raw_base(), target);
}

template <typename Memory>
static void ac6_mission_call_slot1(PPCContext &context, Memory &memory,
                                    std::uint32_t object,
                                    std::uint32_t return_address) {
  context.r3.u32 = object;
  context.lr = return_address;
  const auto target = memory.load_u32(memory.load_u32(object) + 4U);
  AC6_PPC_CALL_INDIRECT(context, memory.raw_base(), target);
}

void sub_8219E428(PPCContext &context, std::uint8_t *base) {
  (void)base;
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219E428");
  auto &memory = memory_for(context);
  const auto node = context.r3.u32;
  memory.store_u32(node + 0x20U, context.r4.u32);
  memory.store_u32(node + 0x24U, context.r5.u32);
  memory.store_u32(node + 0x14U, context.r6.u32);
  memory.store_u32(node + 0x18U, context.r7.u32);
  memory.store_u32(node + 0x1CU, context.r8.u32);
}

void sub_8219E768(PPCContext &context, std::uint8_t *base) {
  (void)base;
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219E768");
  auto &memory = memory_for(context);
  const auto node = context.r3.u32;
  memory.store_u32(node + 0x18U, context.r4.u32);
  memory.store_u32(node + 0x1CU, context.r5.u32);
  memory.store_u32(node + 0x14U, context.r6.u32);
}

void sub_8219EA48(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219EA48");
  auto &memory = memory_for(context);
  const auto holder = context.r3.u32;
  const auto list_address = context.r4.u32;
  const auto node = memory.load_u32(holder);
  ac6_mission_append_node(memory, node, list_address);
}

void sub_8219EEE8(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219EEE8");
  auto &memory = memory_for(context);
  const auto saved_r27 = context.r27;
  const auto saved_r28 = context.r28;
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto value = context.r5.u32;
  const auto finish = [&](std::uint32_t result) {
    context.r3.u32 = result;
    context.r27 = saved_r27;
    context.r28 = saved_r28;
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
  };

  if (context.r4.s32 != 2) {
    finish(0U);
    return;
  }

  const auto global = object;
  context.r3.u32 = value;
  context.r4.u32 = 0U;
  sub_8219DE20(context, base);
  const auto key = context.r3.u32;
  context.r3.u32 = global;
  context.r4.u32 = key;
  sub_8219EE40(context, base);
  auto element = context.r3.u32;
  if (element != 0U) {
    context.r3.u32 = global;
    context.r4.u32 = element;
    sub_8219ECF8(context, base);
    ac6_mission_dispatch(context, base, memory, element, 24U, 0x8219EF4CU,
                         0U);
    if ((context.r3.u32 & 0xFFU) == 1U) {
      finish(key);
      return;
    }
  } else {
    context.r3.u32 = memory.load_u32(0x8281EB50U);
    context.r4.u32 = 0x44U;
    context.r5.u32 = 16U;
    context.r6.u32 = memory.load_u8(object + 48U);
    sub_821E1CD8(context, base);
    if (context.r3.u32 == 0U) {
      finish(0U);
      return;
    }
    sub_8219DD10(context, base);
    element = context.r3.u32;
    if (element == 0U) {
      finish(0U);
      return;
    }
    ac6_mission_dispatch(context, base, memory, element, 8U, 0x8219EFA0U,
                         key);
    context.r3.u32 = global;
    context.r4.u32 = element;
    sub_8219ECF8(context, base);
  }

  ac6_mission_dispatch(context, base, memory, element, 32U, 0x8219EFC0U,
                       0U);
  if ((context.r3.u32 & 0xFFU) == 0U) {
    context.r3.u32 = memory.load_u32(0x8281EB50U);
    context.r4.u32 = 0x28U;
    context.r5.u32 = 16U;
    context.r6.u32 = memory.load_u8(object + 48U);
    sub_821E1CD8(context, base);
    if (context.r3.u32 == 0U) {
      finish(0U);
      return;
    }
    sub_8219DEC8(context, base);
    const auto node_object = context.r3.u32;
    if (node_object == 0U) {
      finish(0U);
      return;
    }
    memory.store_u32(node_object + 16U, value);
    memory.store_u32(node_object + 24U, memory.load_u32(object + 44U));
    ac6_mission_dispatch(context, base, memory, node_object, 16U,
                         0x8219F014U, element);
    const auto node = ac6_mission_take_node(memory, object, node_object);
    if (node == 0U) {
      finish(0U);
      return;
    }
    ac6_mission_dispatch(context, base, memory, element, 36U, 0x8219F058U,
                         1U);
  }

  ac6_mission_dispatch(context, base, memory, element, 4U, 0x8219F06CU, 0U);
  finish(context.r3.u32);
}

void sub_8219F080(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219F080");
  auto &memory = memory_for(context);
  const auto saved_r27 = context.r27;
  const auto saved_r28 = context.r28;
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto out = context.r4.u32;
  const auto value = context.r5.u32;
  const auto finish = [&](std::uint32_t result) {
    context.r3.u32 = result;
    context.r27 = saved_r27;
    context.r28 = saved_r28;
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
  };

  const auto global = object;
  context.r3.u32 = value;
  sub_821A0600(context, base);
  const auto key = context.r3.u32;
  context.r3.u32 = global;
  context.r4.u32 = key;
  sub_8219EE40(context, base);
  auto element = context.r3.u32;
  memory.store_u32(out, element);
  if (element == 0U) {
    context.r3.u32 = memory.load_u32(0x8281EB50U);
    context.r4.u32 = 0x1CU;
    context.r5.u32 = 16U;
    context.r6.u32 = memory.load_u8(object + 48U);
    sub_821E1CD8(context, base);
    if (context.r3.u32 == 0U) {
      finish(0U);
      return;
    }
    sub_821A05A8(context, base);
    element = context.r3.u32;
    if (element == 0U) {
      finish(0U);
      return;
    }
    memory.store_u32(element + 24U, value);
    memory.store_u32(out, element);
    ac6_mission_dispatch(context, base, memory, element, 8U, 0x8219F10CU,
                         key);
  }
  ac6_mission_dispatch(context, base, memory, element, 28U, 0x8219F124U, 0U);
  ac6_mission_dispatch(context, base, memory, element, 36U, 0x8219F13CU, 1U);
  context.r3.u32 = global;
  context.r4.u32 = element;
  sub_8219ECF8(context, base);
  context.r3.u32 = memory.load_u32(0x8281EB50U);
  context.r4.u32 = 0x28U;
  context.r5.u32 = 16U;
  context.r6.u32 = memory.load_u8(object + 48U);
  sub_821E1CD8(context, base);
  if (context.r3.u32 == 0U) {
    finish(0U);
    return;
  }
  sub_8219E440(context, base);
  const auto node_object = context.r3.u32;
  if (node_object == 0U) {
    finish(0U);
    return;
  }
  ac6_mission_dispatch(context, base, memory, node_object, 16U, 0x8219F188U,
                       memory.load_u32(out));
  const auto node = ac6_mission_take_node(memory, object, node_object);
  finish(node);
}

void sub_8219F1C0(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219F1C0");
  auto &memory = memory_for(context);
  const auto saved_r27 = context.r27;
  const auto saved_r28 = context.r28;
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto out = context.r4.u32;
  const auto value = context.r5.u32;
  const auto finish = [&](std::uint32_t result) {
    context.r3.u32 = result;
    context.r27 = saved_r27;
    context.r28 = saved_r28;
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
  };

  const auto global = object;
  context.r3.u32 = value;
  sub_821A0548(context, base);
  const auto key = context.r3.u32;
  context.r3.u32 = global;
  context.r4.u32 = key;
  sub_8219EE40(context, base);
  auto element = context.r3.u32;
  memory.store_u32(out, element);
  if (element == 0U) {
    context.r3.u32 = memory.load_u32(0x8281EB50U);
    context.r4.u32 = 0x1CU;
    context.r5.u32 = 16U;
    context.r6.u32 = memory.load_u8(object + 48U);
    sub_821E1CD8(context, base);
    if (context.r3.u32 == 0U) {
      finish(0U);
      return;
    }
    sub_821A04F0(context, base);
    element = context.r3.u32;
    if (element == 0U) {
      finish(0U);
      return;
    }
    memory.store_u32(element + 24U, value);
    memory.store_u32(out, element);
    ac6_mission_dispatch(context, base, memory, element, 8U, 0x8219F24CU,
                         key);
  }
  ac6_mission_dispatch(context, base, memory, element, 28U, 0x8219F264U, 0U);
  ac6_mission_dispatch(context, base, memory, element, 36U, 0x8219F27CU, 1U);
  context.r3.u32 = global;
  context.r4.u32 = element;
  sub_8219ECF8(context, base);
  context.r3.u32 = memory.load_u32(0x8281EB50U);
  context.r4.u32 = 0x28U;
  context.r5.u32 = 16U;
  context.r6.u32 = memory.load_u8(object + 48U);
  sub_821E1CD8(context, base);
  if (context.r3.u32 == 0U) {
    finish(0U);
    return;
  }
  sub_8219E2E0(context, base);
  const auto node_object = context.r3.u32;
  if (node_object == 0U) {
    finish(0U);
    return;
  }
  ac6_mission_dispatch(context, base, memory, node_object, 16U, 0x8219F2C8U,
                       memory.load_u32(out));
  const auto node = ac6_mission_take_node(memory, object, node_object);
  finish(node);
}

void sub_8219F300(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219F300");
  auto &memory = memory_for(context);
  const auto saved_r27 = context.r27;
  const auto saved_r28 = context.r28;
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto caller_r1 = context.r1.u32;
  const auto frame = caller_r1 - 128U;
  memory.store_u32(frame, caller_r1);
  context.r1.u32 = frame;
  const auto object = context.r3.u32;
  const auto out = context.r4.u32;
  const auto arg5 = context.r5.u32;
  const auto arg6 = context.r6.u32;
  const auto finish = [&](std::uint32_t result) {
    context.r3.u32 = result;
    context.r27 = saved_r27;
    context.r28 = saved_r28;
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.r1.u32 = caller_r1;
    context.lr = saved_lr;
  };

  const auto global = object;
  context.r3.u32 = arg5;
  context.r4.u32 = arg6;
  sub_821A06B8(context, base);
  const auto key = context.r3.u32;
  context.r3.u32 = global;
  context.r4.u32 = key;
  sub_8219EE40(context, base);
  auto element = context.r3.u32;
  memory.store_u32(out, element);
  if (element != 0U) {
    context.r3.u32 = global;
    context.r4.u32 = element;
    sub_8219ECF8(context, base);
    ac6_mission_dispatch(context, base, memory, element, 24U, 0x8219F35CU,
                         0U);
    if ((context.r3.u32 & 0xFFU) == 1U) {
      finish(0U);
      return;
    }
  } else {
    context.r3.u32 = memory.load_u32(0x8281EB50U);
    context.r4.u32 = 0x101CU;
    context.r5.u32 = 16U;
    context.r6.u32 = memory.load_u8(object + 48U);
    sub_821E1CD8(context, base);
    if (context.r3.u32 == 0U) {
      finish(0U);
      return;
    }
    sub_821A0660(context, base);
    element = context.r3.u32;
    if (element == 0U) {
      finish(0U);
      return;
    }
    memory.store_u32(out, element);
    ac6_mission_dispatch(context, base, memory, element, 8U, 0x8219F3B0U,
                         key);
  }
  ac6_mission_dispatch(context, base, memory, element, 28U, 0x8219F3C8U, 0U);
  ac6_mission_dispatch(context, base, memory, element, 36U, 0x8219F3E0U, 1U);
  context.r3.u32 = global;
  context.r4.u32 = element;
  sub_8219ECF8(context, base);
  context.r3.u32 = memory.load_u32(0x8281EB50U);
  context.r4.u32 = 0x20U;
  context.r5.u32 = 16U;
  context.r6.u32 = memory.load_u8(object + 48U);
  sub_821E1CD8(context, base);
  if (context.r3.u32 == 0U) {
    finish(0U);
    return;
  }
  sub_8219E550(context, base);
  const auto node_object = context.r3.u32;
  if (node_object == 0U) {
    finish(0U);
    return;
  }
  ac6_mission_dispatch(context, base, memory, node_object, 16U, 0x8219F42CU,
                       memory.load_u32(out));
  const auto node = ac6_mission_take_node(memory, object, node_object);
  finish(node);
}

void sub_8219F468(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219F468");
  auto &memory = memory_for(context);
  const auto saved_r27 = context.r27;
  const auto saved_r28 = context.r28;
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto out = context.r4.u32;
  const auto arg5 = context.r5.u32;
  const auto arg6 = context.r6.u32;
  const auto finish = [&](std::uint32_t result) {
    context.r3.u32 = result;
    context.r27 = saved_r27;
    context.r28 = saved_r28;
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
  };

  const auto global = object;
  context.r3.u32 = arg5;
  context.r4.u32 = arg6;
  sub_821A0770(context, base);
  const auto key = context.r3.u32;
  context.r3.u32 = global;
  context.r4.u32 = key;
  sub_8219EE40(context, base);
  auto element = context.r3.u32;
  memory.store_u32(out, element);
  if (element != 0U) {
    context.r3.u32 = global;
    context.r4.u32 = element;
    sub_8219ECF8(context, base);
    ac6_mission_dispatch(context, base, memory, element, 24U, 0x8219F4C4U,
                         0U);
    if ((context.r3.u32 & 0xFFU) == 1U) {
      finish(0U);
      return;
    }
  } else {
    context.r3.u32 = memory.load_u32(0x8281EB50U);
    context.r4.u32 = 0x9CU;
    context.r5.u32 = 16U;
    context.r6.u32 = memory.load_u8(object + 48U);
    sub_821E1CD8(context, base);
    if (context.r3.u32 == 0U) {
      finish(0U);
      return;
    }
    sub_821A0718(context, base);
    element = context.r3.u32;
    if (element == 0U) {
      finish(0U);
      return;
    }
    memory.store_u32(out, element);
    ac6_mission_dispatch(context, base, memory, element, 8U, 0x8219F518U,
                         key);
  }
  ac6_mission_dispatch(context, base, memory, element, 28U, 0x8219F530U, 0U);
  ac6_mission_dispatch(context, base, memory, element, 36U, 0x8219F548U, 1U);
  context.r3.u32 = global;
  context.r4.u32 = element;
  sub_8219ECF8(context, base);
  context.r3.u32 = memory.load_u32(0x8281EB50U);
  context.r4.u32 = 0x24U;
  context.r5.u32 = 16U;
  context.r6.u32 = memory.load_u8(object + 48U);
  sub_821E1CD8(context, base);
  if (context.r3.u32 == 0U) {
    finish(0U);
    return;
  }
  sub_8219E778(context, base);
  const auto node_object = context.r3.u32;
  if (node_object == 0U) {
    finish(0U);
    return;
  }
  ac6_mission_dispatch(context, base, memory, node_object, 16U, 0x8219F594U,
                       memory.load_u32(out));
  const auto node = ac6_mission_take_node(memory, object, node_object);
  finish(node);
}

void sub_821A00E8(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_821A00E8");
  auto &memory = memory_for(context);
  const auto saved_r28 = context.r28;
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto mode = context.r4.s32;
  const auto field0 = context.r5.u32;
  const auto field1 = context.r6.u32;
  const auto field2 = context.r7.u32;
  const auto value = context.r8.u32;
  const auto out = context.r1.u32 - 48U;
  const auto finish = [&](std::uint32_t result) {
    context.r3.u32 = result;
    context.r28 = saved_r28;
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
  };
  if (mode != 4 && mode != 6) {
    finish(0U);
    return;
  }
  memory.store_u32(out, 0U);
  context.r3.u32 = object;
  context.r4.u32 = out;
  context.r5.u32 = value;
  if (mode == 4) {
    sub_8219F080(context, base);
  } else {
    sub_8219F1C0(context, base);
  }
  const auto node = context.r3.u32;
  if (node == 0U) {
    finish(0U);
    return;
  }
  memory.store_u32(node + 0x10U, field0);
  memory.store_u32(node + 0x14U, field1);
  memory.store_u32(node + 0x18U, field2);
  memory.store_u32(node + 0x1CU, value);
  ac6_mission_call_slot1(context, memory, memory.load_u32(out), 0x821A014CU);
  finish(context.r3.u32);
}

void sub_821A0180(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_821A0180");
  auto &memory = memory_for(context);
  const auto saved_r27 = context.r27;
  const auto saved_r28 = context.r28;
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto mode = context.r4.s32;
  const auto field0 = context.r5.u32;
  const auto field1 = context.r6.u32;
  const auto field2 = context.r7.u32;
  const auto field3 = context.r8.u32;
  const auto value = context.r9.u32;
  const auto out = context.r1.u32 - 64U;
  const auto finish = [&](std::uint32_t result) {
    context.r3.u32 = result;
    context.r27 = saved_r27;
    context.r28 = saved_r28;
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
  };
  if (mode != 4 && mode != 6) {
    finish(0U);
    return;
  }
  memory.store_u32(out, 0U);
  context.r3.u32 = object;
  context.r4.u32 = out;
  context.r5.u32 = value;
  if (mode == 4) {
    sub_8219F080(context, base);
  } else {
    sub_8219F1C0(context, base);
  }
  if (context.r3.u32 == 0U) {
    finish(0U);
    return;
  }
  context.r4.u32 = field0;
  context.r5.u32 = field1;
  context.r6.u32 = field2;
  context.r7.u32 = field3;
  context.r8.u32 = value;
  sub_8219E428(context, base);
  ac6_mission_call_slot1(context, memory, memory.load_u32(out), 0x821A01F0U);
  finish(context.r3.u32);
}

void sub_821A0240(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_821A0240");
  auto &memory = memory_for(context);
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto value = context.r4.u32;
  const auto field = context.r7.u32;
  const auto out = context.r1.u32 - 32U;
  const auto finish = [&](std::uint32_t result) {
    context.r3.u32 = result;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
  };
  if (value == 0U) {
    finish(0U);
    return;
  }
  memory.store_u32(out, 0U);
  context.r3.u32 = object;
  context.r4.u32 = out;
  sub_8219F300(context, base);
  if (context.r3.u32 != 0U) {
    memory.store_u32(context.r3.u32 + 0x10U, value);
    memory.store_u32(context.r3.u32 + 0x14U, field);
  }
  const auto element = memory.load_u32(out);
  if (element == 0U) {
    finish(0U);
    return;
  }
  ac6_mission_call_slot1(context, memory, element, 0x821A02A0U);
  finish(context.r3.u32);
}

void sub_821A02C0(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_821A02C0");
  auto &memory = memory_for(context);
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto value = context.r4.u32;
  const auto field0 = context.r6.u32;
  const auto field1 = context.r7.u32;
  const auto out = context.r1.u32 - 48U;
  const auto finish = [&](std::uint32_t result) {
    context.r3.u32 = result;
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
  };
  if (value == 0U) {
    finish(0U);
    return;
  }
  memory.store_u32(out, 0U);
  context.r3.u32 = object;
  context.r4.u32 = out;
  sub_8219F300(context, base);
  if (context.r3.u32 != 0U) {
    context.r4.u32 = value;
    context.r5.u32 = field0;
    context.r6.u32 = field1;
    sub_8219E768(context, base);
  }
  const auto element = memory.load_u32(out);
  if (element == 0U) {
    finish(0U);
    return;
  }
  ac6_mission_call_slot1(context, memory, element, 0x821A0324U);
  finish(context.r3.u32);
}

void sub_821A0338(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_821A0338");
  auto &memory = memory_for(context);
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto value = context.r4.u32;
  const auto field0 = context.r6.u32;
  const auto field1 = context.r7.u32;
  const auto out = context.r1.u32 - 48U;
  const auto finish = [&](std::uint32_t result) {
    context.r3.u32 = result;
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
  };
  if (value == 0U) {
    finish(0U);
    return;
  }
  memory.store_u32(out, 0U);
  context.r3.u32 = object;
  context.r4.u32 = out;
  sub_8219F468(context, base);
  if (context.r3.u32 != 0U) {
    memory.store_u32(context.r3.u32 + 0x14U, value);
    memory.store_u32(context.r3.u32 + 0x18U, field0);
    memory.store_u32(context.r3.u32 + 0x1CU, field1);
  }
  const auto element = memory.load_u32(out);
  if (element == 0U) {
    finish(0U);
    return;
  }
  ac6_mission_call_slot1(context, memory, element, 0x821A0398U);
  finish(context.r3.u32);
}

void sub_821A03A8(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_821A03A8");
  auto &memory = memory_for(context);
  const auto saved_r28 = context.r28;
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto value = context.r4.u32;
  const auto field0 = context.r6.u32;
  const auto field1 = context.r7.u32;
  const auto field2 = context.r8.u32;
  const auto out = context.r1.u32 - 48U;
  const auto finish = [&](std::uint32_t result) {
    context.r3.u32 = result;
    context.r28 = saved_r28;
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
  };
  if (value == 0U) {
    finish(0U);
    return;
  }
  memory.store_u32(out, 0U);
  context.r3.u32 = object;
  context.r4.u32 = out;
  sub_8219F468(context, base);
  if (context.r3.u32 != 0U) {
    memory.store_u32(context.r3.u32 + 0x14U, value);
    memory.store_u32(context.r3.u32 + 0x18U, field0);
    memory.store_u32(context.r3.u32 + 0x1CU, field2);
    memory.store_u32(context.r3.u32 + 0x20U, field1);
  }
  const auto element = memory.load_u32(out);
  if (element == 0U) {
    finish(0U);
    return;
  }
  ac6_mission_call_slot1(context, memory, element, 0x821A0410U);
  finish(context.r3.u32);
}

void sub_8219EC88(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219EC88");
  auto &memory = memory_for(context);
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  auto node = memory.load_u32(object + 0x1CU);
  memory.store_u32(object + 0x24U, 1U);

  while (node != 0U) {
    const auto element = memory.load_u32(node);
    if (element != 0U) {
      const auto target = memory.load_u32(memory.load_u32(element) + 12U);
      context.r3.u32 = element;
      context.lr = 0x8219ECBCU;
      AC6_PPC_CALL_INDIRECT(context, memory.raw_base(), target);
      if (context.r3.s32 == 1) {
        const auto recurse = memory.load_u32(node);
        if (recurse != 0U) {
          context.r3.u32 = recurse;
          sub_8219EC88(context, base);
        }
      }
    }
    node = memory.load_u32(node + 4U);
  }

  context.r29 = saved_r29;
  context.r30 = saved_r30;
  context.r31 = saved_r31;
  context.lr = saved_lr;
}

void sub_8219EE40(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8219EE40");
  auto &memory = memory_for(context);
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;
  const auto key = context.r4.u32;

  if (std::getenv("AC6_DEMO_WATCH_LOADING_RESOLVER") != nullptr &&
      require_bridge().tick() >= 4100U && require_bridge().tick() <= 4140U) {
    static std::uint32_t records = 0U;
    if (records++ < 256U) {
      std::fprintf(stderr,
                   "AC6_LOADING_RESOLVER phase=enter tick=%llu lr=0x%08X "
                   "object=0x%08X key=0x%08X link=0x%08X fields="
                   "[%08X,%08X,%08X,%08X]\n",
                   static_cast<unsigned long long>(require_bridge().tick()),
                   static_cast<std::uint32_t>(saved_lr), object, key,
                   memory.load_u32(object + 0x1CU),
                   memory.load_u32(object + 0x20U),
                   memory.load_u32(object + 0x24U),
                   memory.load_u32(object + 0x28U));
    }
  }

  context.r3.u32 = key;
  context.r4.u32 = memory.load_u32(object + 0x1CU);
  sub_8219EAA0(context, base);
  if (context.r3.u32 != 0U) {
    context.r3.u32 = memory.load_u32(context.r3.u32);
    if (std::getenv("AC6_DEMO_WATCH_LOADING_RESOLVER") != nullptr &&
        require_bridge().tick() >= 4100U && require_bridge().tick() <= 4140U) {
      static std::uint32_t records = 0U;
      if (records++ < 256U) {
        std::fprintf(stderr,
                     "AC6_LOADING_RESOLVER phase=fast_return tick=%llu "
                     "result=0x%08X\n",
                     static_cast<unsigned long long>(require_bridge().tick()),
                     context.r3.u32);
      }
    }
    context.r29 = saved_r29;
    context.r30 = saved_r30;
    context.r31 = saved_r31;
    context.lr = saved_lr;
    return;
  }

  auto node = memory.load_u32(object + 0x1CU);
  while (node != 0U) {
    const auto element = memory.load_u32(node);
    if (element != 0U) {
      const auto target = memory.load_u32(memory.load_u32(element) + 12U);
      context.r3.u32 = element;
      context.lr = 0x8219EE94U;
      AC6_PPC_CALL_INDIRECT(context, memory.raw_base(), target);
      if (context.r3.s32 == 1) {
        const auto recurse = memory.load_u32(node);
        if (recurse != 0U) {
          context.r3.u32 = recurse;
          context.r4.u32 = key;
          context.lr = 0x8219EEB0U;
          sub_8219EE40(context, base);
          if (context.r3.u32 != 0U) {
            context.r29 = saved_r29;
            context.r30 = saved_r30;
            context.r31 = saved_r31;
            context.lr = saved_lr;
            return;
          }
        }
      }
    }
    node = memory.load_u32(node + 4U);
  }

  context.r3.u32 = 0U;
  context.r29 = saved_r29;
  context.r30 = saved_r30;
  context.r31 = saved_r31;
  context.lr = saved_lr;
}

void sub_8216D760(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8216D760");
  auto &memory = memory_for(context);
  const auto saved_r29 = context.r29;
  const auto saved_r30 = context.r30;
  const auto saved_r31 = context.r31;
  const auto saved_lr = context.lr;
  const auto object = context.r3.u32;

  context.r3.u32 = 0U;
  sub_8218DF70(context, base);
  const auto table_value = context.r3.u32;
  const auto global = memory.load_u32(0x827435F8U);
  context.r3.u32 = global;
  context.r4.u32 = table_value;
  sub_8218EA88(context, base);

  const auto vtable = memory.load_u32(object);
  const auto first_target = memory.load_u32(vtable + 52U);
  context.r3.u32 = object;
  context.lr = 0x8216D79CU;
  AC6_PPC_CALL_INDIRECT(context, memory.raw_base(), first_target);

  const auto state = memory.load_u32(0x823C27E0U) + 112U;
  context.r3.u32 = state;
  sub_82095B80(context, base);
  const auto mission = context.r3.u32;
  context.r3.u32 = state;
  sub_820E9300(context, base);
  const auto value = (context.r3.u32 & 0xFFU) != 0U ? 223U : mission + 207U;

  const auto second_vtable = memory.load_u32(object);
  const auto second_target = memory.load_u32(second_vtable + 56U);
  context.r3.u32 = object;
  context.r4.u32 = value;
  context.r5.u32 = 2U;
  context.lr = 0x8216D7E8U;
  AC6_PPC_CALL_INDIRECT(context, memory.raw_base(), second_target);

  const auto selected_global = memory.load_u32(0x827435F8U);
  const auto table_offset = memory.load_u32(selected_global + 0x48U) != 0U
                                ? 0x44U
                                : 0x40U;
  context.r3.u32 = 0x827745F0U;
  context.r4.u32 = memory.load_u32(selected_global + table_offset);
  sub_8219EE40(context, base);
  sub_8219EC88(context, base);

  memory.store_u32(object + 12U, 0U);
  memory.store_u32(object + 68U, 4U);
  memory.store_u32(object + 0x60930U, 0U);

  context.r29 = saved_r29;
  context.r30 = saved_r30;
  context.r31 = saved_r31;
  context.lr = saved_lr;
}

void sub_8216DB10(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_8216DB10");
  auto &memory = memory_for(context);
  const auto caller = context.r3.u32;
  const auto state = memory.load_u32(0x823C27E0U) + 0x70U;

  context.r3.u32 = state;
  sub_820E9300(context, base);
  const auto selected = (context.r3.u32 & 0xFFU) != 0U ? 1U : 0U;
  context.r3.u32 = selected;
  sub_8218DF70(context, base);
  const auto table_value = context.r3.u32;

  const auto global = memory.load_u32(0x827435F8U);
  context.r3.u32 = global;
  context.r4.u32 = table_value;
  sub_8218EA88(context, base);

  memory.store_u32(caller - 0x24U, 3U);
  memory.store_u32(caller - 0x5CU, 2U);
}

void sub_821714C0(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_821714C0");
  auto &memory = memory_for(context);
  const auto state = memory.load_u32(0x823C27E0U) + 0x70U;

  context.r3.u32 = state;
  sub_820E9300(context, base);
  const auto valid = (context.r3.u32 & 0xFFU) != 0U;
  std::uint32_t selected = 0U;
  if (valid) {
    const auto raw_index = memory.load_u32(state + 0x206E4U);
    auto index = static_cast<std::int32_t>(raw_index);
    if (index < 0 || index > 2) {
      index = 0;
    }
    memory.store_u32(state + 0x6C8U + static_cast<std::uint32_t>(index) *
                                      0xAAB8U,
                     1U);

    const auto global = memory.load_u32(0x827435F8U);
    if (memory.load_u32(global + 0x20U) != 2U) {
      memory.store_u32(global + 0x20U, 1U);
    }
    selected = 1U;
  }

  context.r3.u32 = selected;
  sub_8218E088(context, base);
  const auto table_value = context.r3.u32;
  const auto global = memory.load_u32(0x827435F8U);
  context.r3.u32 = global;
  context.r4.u32 = table_value;
  sub_8218EA88(context, base);
}

void sub_820EA550(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_820EA550");
  auto &memory = memory_for(context);
  const auto out_param_address = context.r4.u32;
  const auto state_address = memory.load_u32(0x823C27E0U) + 0x70U;

  context.r3.u32 = state_address;
  sub_82095B80(context, base);
  const auto mission = context.r3.u32;
  context.r3.u32 = state_address;
  sub_820E9300(context, base);
  const auto value = (context.r3.u32 & 0xFFU) != 0U ? 16U : mission;
  memory.store_u32(out_param_address, value);
}

void sub_820EA598(PPCContext &context, std::uint8_t *base) {
  AC6_PPC_FUNCTION_ENTRY_CONTEXT(context, "sub_820EA598");
  auto &memory = memory_for(context);
  const auto out_param_address = context.r4.u32;
  const auto state_address = memory.load_u32(0x823C27E0U) + 0x70U;

  context.r3.u32 = state_address;
  sub_820E9290(context, base);
  auto value = context.r3.u32;
  if (value == 7U) {
    value = 6U;
  } else if (value == 6U) {
    value = 7U;
  }
  context.r3.u32 = value;
  memory.store_u32(out_param_address, value);
}
