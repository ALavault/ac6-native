// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
  if (std::string_view{name} == "KeTlsAlloc") {
    const auto slot = next_tls_slot++;
    if (slot >= 64U) {
      context.r3.s64 = -1;
      return true;
    }
    tls_slots[slot] = true;
    context.r3.u32 = slot;
    return true;
  }
  if (std::string_view{name} == "KeTlsGetValue") {
    const auto found = tls_slots.find(context.r3.u32);
    if (found == tls_slots.end() || !found->second) {
      return false;
    }
    const auto value = tls_values.find(context.r3.u32);
    context.r3.u32 = value == tls_values.end() ? 0U : value->second;
    return true;
  }
  if (std::string_view{name} == "KeTlsSetValue") {
    const auto found = tls_slots.find(context.r3.u32);
    if (found == tls_slots.end() || !found->second) {
      return false;
    }
    tls_values[context.r3.u32] = context.r4.u32;
    context.r3.u32 = 1U;
    return true;
  }
  if (std::string_view{name} == "KeTlsFree") {
    const auto found = tls_slots.find(context.r3.u32);
    if (found == tls_slots.end() || !found->second) {
      return false;
    }
    found->second = false;
    tls_values.erase(context.r3.u32);
    context.r3.u32 = 1U;
    return true;
  }
  if (std::string_view{name} == "KeEnterCriticalRegion") {
    // The deterministic guest scheduler has no asynchronous APC delivery,
    // but the nesting state remains observable for the matching kernel path.
    ++kernel_critical_region_depth;
    return true;
  }
  if (std::string_view{name} == "KeLeaveCriticalRegion") {
    if (kernel_critical_region_depth == 0U) {
      return false;
    }
    --kernel_critical_region_depth;
    return true;
  }
  if (std::string_view{name} == "VdRetrainEDRAMWorker") {
    // No EDRAM backing is exposed by the bounded Vulkan HLE.  Returning the
    // documented worker failure keeps the caller on its explicit fallback
    // path instead of manufacturing a completed retrain.
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "VdRetrainEDRAM") {
    // The demo's caller checks this result and handles the unavailable EDRAM
    // path.  Do not write synthetic addresses or claim a retrain succeeded.
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "VdIsHSIOTrainingSucceeded") {
    // The HLE exposes no Xenon HSIO/EDRAM training state; report the actual
    // bounded state so the title takes its guarded fallback branch.
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "_vsnprintf") {
    auto& memory = require_bridge().memory();
    std::string output;
    if (!format_guest_string(memory, context.r3.u32, context.r4.u32, context.r5.u32,
                             context.r6.u32, &output)) {
      return false;
    }
    const bool truncated = context.r4.u32 != 0U && output.size() >= context.r4.u32;
    context.r3.s64 = truncated ? -1 : static_cast<std::int64_t>(output.size());
    return true;
  }
  if (std::string_view{name} == "DbgPrint") {
    std::string message;
    if (!load_guest_string(require_bridge().memory(), context.r3.u32, 4096U, &message)) {
      return false;
    }
    context.r3.s64 = static_cast<std::int64_t>(message.size());
    return true;
  }
  auto& memory = require_bridge().memory();
  if (std::string_view{name} == "RtlInitAnsiString") {
    return ac6demo::initialize_guest_ansi_string(memory, context.r3.u32,
                                                 context.r4.u32);
  }
  if (std::string_view{name} == "RtlFillMemoryUlong") {
    const auto address = context.r3.u32;
    const auto length = context.r4.u32;
    if ((length & 3U) != 0U || !memory.mapped(address, length)) {
      return false;
    }
    for (std::uint32_t offset = 0U; offset < length; offset += 4U) {
      memory.store_u32(address + offset, context.r5.u32);
    }
    return true;
  }
  const auto critical_address = context.r3.u32;
  if (std::string_view{name} == "RtlInitializeCriticalSection" ||
      std::string_view{name} == "RtlInitializeCriticalSectionAndSpinCount") {
    if (!memory.mapped(critical_address, 28U)) {
      return false;
    }
    for (std::uint32_t offset = 0U; offset < 16U; offset += 4U) {
      memory.store_u32(critical_address + offset, 0U);
    }
    memory.store_u32(critical_address + 16U, 0xFFFFFFFFU);
    memory.store_u32(critical_address + 20U, 0U);
    memory.store_u32(critical_address + 24U, 0U);
    critical_sections[critical_address] = GuestCriticalSection{};
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "RtlEnterCriticalSection") {
    if (!memory.mapped(critical_address, 28U)) {
      return false;
    }
    for (;;) {
      auto& section = critical_sections[critical_address];
      if (section.owner == 0U || section.owner == current_guest_thread_id) {
        section.owner = current_guest_thread_id;
        ++section.recursion;
        memory.store_u32(critical_address + 16U, section.recursion - 1U);
        memory.store_u32(critical_address + 20U, section.recursion);
        memory.store_u32(critical_address + 24U, section.owner);
        break;
      }
      (void)require_bridge().block_current_guest_thread(
          kWaitCriticalSection, critical_address, kNoWakeTick);
    }
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "RtlTryEnterCriticalSection") {
    if (!memory.mapped(critical_address, 28U)) {
      return false;
    }
    auto& section = critical_sections[critical_address];
    if (section.owner != 0U && section.owner != current_guest_thread_id) {
      context.r3.s64 = 0;
      return true;
    }
    section.owner = current_guest_thread_id;
    ++section.recursion;
    memory.store_u32(critical_address + 16U, section.recursion - 1U);
    memory.store_u32(critical_address + 20U, section.recursion);
    memory.store_u32(critical_address + 24U, section.owner);
    context.r3.s64 = 1;
    return true;
  }
  if (std::string_view{name} == "RtlLeaveCriticalSection") {
    if (!memory.mapped(critical_address, 28U)) {
      return false;
    }
    const auto found = critical_sections.find(critical_address);
    if (found == critical_sections.end() || found->second.recursion == 0U ||
        found->second.owner != current_guest_thread_id) {
      return false;
    }
    --found->second.recursion;
    if (found->second.recursion == 0U) {
      found->second.owner = 0U;
      memory.store_u32(critical_address + 16U, 0xFFFFFFFFU);
      memory.store_u32(critical_address + 20U, 0U);
      memory.store_u32(critical_address + 24U, 0U);
      if (require_bridge().wake_guest_waiters(kWaitCriticalSection,
                                              critical_address)) {
        require_bridge().yield_current_guest_thread();
      }
    } else {
      memory.store_u32(critical_address + 16U, found->second.recursion - 1U);
      memory.store_u32(critical_address + 20U, found->second.recursion);
    }
    context.r3.s64 = 0;
    return true;
  }
