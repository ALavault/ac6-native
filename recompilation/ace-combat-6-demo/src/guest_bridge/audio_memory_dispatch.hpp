// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
  if (std::string_view{name} == "XMACreateContext") {
    // This is a bounded experiment, never enabled by play/replay. It joins
    // the exact PAL callsite/registers to the generic XMA output-pointer ABI
    // without broadening the import surface or fabricating audio packets.
    constexpr std::uint32_t kFirstOutputSlot = 0x17360050U;
    constexpr std::uint32_t kOutputSlotStride = 0x60U;
    constexpr std::uint32_t kOutputSlotCount = 3U;
    const auto output_slot = context.r3.u32;
    const auto slot_delta = output_slot - kFirstOutputSlot;
    const auto slot_is_qualified =
        output_slot >= kFirstOutputSlot &&
        slot_delta / kOutputSlotStride < kOutputSlotCount &&
        slot_delta % kOutputSlotStride == 0U;
    if (std::getenv("AC6_DEMO_EXPERIMENTAL_XMA_CREATE") == nullptr ||
        static_cast<std::uint32_t>(context.lr) != 0x82357298U ||
        !slot_is_qualified || context.r4.u32 != 0U ||
        context.r5.u32 != 0x6180U || context.r6.u32 != 0U ||
        context.r7.u32 != 1U) {
      return false;
    }
    auto &bridge = require_bridge();
    auto &memory = bridge.memory();
    if (!memory.mapped(output_slot, 4U)) {
      return false;
    }
    const auto context_pointer = bridge.allocate_xma_context();
    memory.store_u32(output_slot, context_pointer);
    context.r3.u32 = context_pointer == 0U ? 0xC0000017U : 0U;
    std::fprintf(stderr,
                 "AC6_XMA_EXPERIMENTAL_CREATE tick=%llu thread=%u "
                 "slot=0x%08x context=0x%08x status=0x%08x\n",
                 static_cast<unsigned long long>(bridge.tick()),
                 current_guest_thread_id, output_slot, context_pointer,
                 context.r3.u32);
    return true;
  }
  if (std::string_view{name} == "XMAReleaseContext") {
    // The reached PAL release loop visits the same three contiguous contexts
    // created by the opt-in experiment. ReXGlue/Xenia generically clear the
    // 64-byte context and release its allocation bit. Preserve only those
    // three reached slots; every other caller, pointer and tuple traps.
    constexpr std::uint32_t kFirstContext = 0x2E800000U;
    constexpr std::uint32_t kContextBytes = 64U;
    constexpr std::uint32_t kContextCount = 3U;
    const auto context_pointer = context.r3.u32;
    const auto context_delta = context_pointer - kFirstContext;
    const auto context_is_qualified =
        context_pointer >= kFirstContext &&
        context_delta / kContextBytes < kContextCount &&
        context_delta % kContextBytes == 0U;
    if (std::getenv("AC6_DEMO_EXPERIMENTAL_XMA_CREATE") == nullptr ||
        static_cast<std::uint32_t>(context.lr) != 0x82356820U ||
        !context_is_qualified || context.r4.u32 != 1U ||
        context.r5.u32 != 0U || context.r6.u32 != 0U ||
        context.r7.u32 != 0U ||
        !require_bridge().release_xma_context(context_pointer)) {
      return false;
    }
    context.r3.u32 = 0U;
    return true;
  }
  if (std::string_view{name} == "XAudioRegisterRenderDriverClient") {
    auto& bridge = require_bridge();
    auto& memory = bridge.memory();
    const auto descriptor = context.r3.u32;
    const auto handle_pointer = context.r4.u32;
    if (!memory.mapped(descriptor, 8U) ||
        !memory.mapped(handle_pointer, 4U)) {
      return false;
    }
    const auto callback = memory.load_u32(descriptor);
    const auto callback_context = memory.load_u32(descriptor + 4U);
    if (callback < PPC_CODE_BASE || callback >= PPC_CODE_BASE + PPC_CODE_SIZE ||
        !memory.mapped(callback, 4U)) {
      return false;
    }
    std::uint32_t handle{};
    if (!bridge.register_xaudio_client(callback, callback_context, &handle)) {
      context.r3.u32 = 0xC000009AU;
      return true;
    }
    memory.store_u32(handle_pointer, handle);
    context.r3.u32 = 0U;
    return true;
  }
  if (std::string_view{name} == "XAudioUnregisterRenderDriverClient") {
    context.r3.u32 = require_bridge().unregister_xaudio_client(context.r3.u32)
                         ? 0U
                         : 0xC0000008U;
    return true;
  }
  if (std::string_view{name} == "NtResumeThread") {
    auto& bridge = require_bridge();
    const auto thread_handle = context.r3.u32;
    const auto previous_count_pointer = context.r4.u32;
    if (previous_count_pointer == 0U ||
        !memory_for(context).mapped(previous_count_pointer, 4U)) {
      return false;
    }
    std::uint32_t previous_count{};
    const auto resumed = require_bridge().resume_guest_thread(
        context.r3.u32, previous_count_pointer, &previous_count);
    memory_for(context).store_u32(previous_count_pointer, previous_count);
    context.r3.s64 = resumed ? 0 : -1;
    ac6demo::guest_bridge_detail::trace_event_handoff(
        "resume_thread", thread_handle, 0U, kWaitThread,
        current_guest_thread_id, bridge.tick(),
        static_cast<std::uint32_t>(context.lr), previous_count, 0U,
        resumed ? 0U : 0xFFFFFFFFU);
    return true;
  }
  if (std::string_view{name} == "MmAllocatePhysicalMemoryEx") {
    auto& bridge = require_bridge();
    auto& memory = bridge.memory();
    const auto requested_size = context.r4.u32;
    if (requested_size == 0U) {
      context.r3.u32 = 0U;
      return true;
    }
    const auto allocation = bridge.allocate_address(requested_size);
    std::uint32_t mapped_address{};
    std::size_t mapped_size{};
    if (allocation == 0U ||
        !checked_page_range(allocation, requested_size, &mapped_address, &mapped_size)) {
      context.r3.u32 = 0U;
      return true;
    }
    memory.map_zero(mapped_address, mapped_size);
    bridge.record_allocation(mapped_address, mapped_size);
    context.r3.u32 = allocation;
    return true;
  }
  if (std::string_view{name} == "MmFreePhysicalMemory") {
    // Allocation lifetime is tracked by the guest bridge.  Keep mappings
    // reserved after release so stale guest pointers cannot alias a later
    // allocation; the kernel operation itself remains observable as a
    // success only for an address issued by this bridge.
    auto& bridge = require_bridge();
    context.r3.s64 = context.r4.u32 == 0U || bridge.owns_allocation(context.r4.u32, 1U)
                         ? 0
                         : -1;
    return true;
  }
  if (std::string_view{name} == "MmGetPhysicalAddress") {
    // The bridge uses one deterministic, page-aligned physical heap for the
    // allocations issued by MmAllocatePhysicalMemoryEx.  Preserve the guest
    // offset inside that allocation and reject arbitrary virtual addresses;
    // callers must not be given a fabricated physical address.
    auto& bridge = require_bridge();
    const auto address = context.r3.u32;
    if (!bridge.memory().mapped(address, 1U) ||
        !bridge.owns_allocation(address, 1U)) {
      return false;
    }
    context.r3.u32 = address;
    if (std::getenv("AC6_DEMO_EXPERIMENTAL_XMA_CREATE") != nullptr) {
      bridge.observe_xma_physical_context(address);
    }
    if (std::getenv("AC6_DEMO_WATCH_XMA_ADDRESS") != nullptr) {
      std::fprintf(stderr,
                   "AC6_XMA_PHYSICAL tick=%llu thread=%u input=0x%08X "
                   "physical=0x%08X lr=0x%08X\n",
                   static_cast<unsigned long long>(bridge.tick()),
                   current_guest_thread_id, address, context.r3.u32,
                   static_cast<std::uint32_t>(context.lr));
    }
    return true;
  }
  if (std::string_view{name} == "MmQueryStatistics") {
    auto& memory = require_bridge().memory();
    const auto output = context.r3.u32;
    if (output == 0U || !memory.mapped(output, 4U)) {
      context.r3.u32 = 0xC000000DU;  // STATUS_INVALID_PARAMETER
      return true;
    }
    constexpr std::uint32_t kStatisticsSize = 104U;
    if (memory.load_u32(output) != kStatisticsSize ||
        !memory.mapped(output, kStatisticsSize)) {
      context.r3.u32 = 0xC0000023U;  // STATUS_BUFFER_TOO_SMALL
      return true;
    }
    constexpr std::uint32_t kTotalPhysicalPages = 0x20000U;
    constexpr std::uint32_t kKernelPages = 0x100U;
    constexpr std::uint32_t kTitleVirtualBytes = 0x2FFE0000U;
    const auto committed =
        std::min(memory.committed_page_count(),
                 kTotalPhysicalPages - kKernelPages);
    const auto available = kTotalPhysicalPages - kKernelPages - committed;
    const auto committed_bytes = static_cast<std::uint64_t>(committed) *
                                 ac6demo::kGuestPageBytes;
    for (std::uint32_t offset = 0U; offset < kStatisticsSize; offset += 4U) {
      memory.store_u32(output + offset, 0U);
    }
    memory.store_u32(output + 0U, kStatisticsSize);
    memory.store_u32(output + 4U, kTotalPhysicalPages);
    memory.store_u32(output + 8U, kKernelPages);
    memory.store_u32(output + 12U, available);
    memory.store_u32(output + 16U, kTitleVirtualBytes);
    memory.store_u32(
        output + 20U,
        static_cast<std::uint32_t>(std::min<std::uint64_t>(
            committed_bytes, kTitleVirtualBytes)));
    memory.store_u32(output + 24U, committed);
    memory.store_u32(output + 100U, kTotalPhysicalPages - 1U);
    context.r3.u32 = 0U;
    return true;
  }
  if (std::string_view{name} == "MmSetAddressProtect") {
    auto& memory = require_bridge().memory();
    const auto address = context.r3.u32;
    const auto length = context.r4.u32;
    const auto protection = context.r5.u32;
    const bool qualified_protection =
        protection == 1U || protection == 2U || protection == 4U ||
        protection == 0x10U || protection == 0x20U || protection == 0x40U;
    if (length == 0U || !qualified_protection ||
        !memory.mapped(address, length)) {
      return false;
    }
    memory.set_protection(address, length, protection);
    return true;
  }
  if (std::string_view{name} == "MmQueryAddressProtect") {
    context.r3.u32 = require_bridge().memory().protection(context.r3.u32);
    return true;
  }
  if (std::string_view{name} == "KeQueryPerformanceFrequency") {
    context.r3.u64 = 50'000'000ULL;
    return true;
  }
  if (std::string_view{name} == "KeQuerySystemTime") {
    auto& memory = memory_for(context);
    if (!memory.mapped(context.r3.u32, 8U)) {
      return false;
    }
    // 100-ns units, driven solely by the invited 60-Hz tick; no host wall
    // clock may enter a replay.
    memory.store_u64(context.r3.u32,
                     (require_bridge().tick() * 10'000'000ULL) / 60ULL);
    return true;
  }
  if (std::string_view{name} == "RtlTimeToTimeFields") {
    auto& memory = memory_for(context);
    if (!memory.mapped(context.r3.u32, 8U) || !memory.mapped(context.r4.u32, 16U)) {
      return false;
    }
    constexpr std::uint64_t kHundredsOfNsPerSecond = 10'000'000ULL;
    constexpr std::uint64_t kHundredsOfNsPerDay = 864'000'000'000ULL;
    constexpr std::int64_t kFileTimeToUnixDays = 134'774;
    const auto file_time = memory.load_u64(context.r3.u32);
    const auto days = file_time / kHundredsOfNsPerDay;
    const auto day_remainder = file_time % kHundredsOfNsPerDay;
    const auto date = civil_from_unix_days(
        static_cast<std::int64_t>(days) - kFileTimeToUnixDays);
    const auto seconds = day_remainder / kHundredsOfNsPerSecond;
    const auto remainder = day_remainder % kHundredsOfNsPerSecond;
    memory.store_u16(context.r4.u32 + 0U, static_cast<std::uint16_t>(date.year));
    memory.store_u16(context.r4.u32 + 2U, date.month);
    memory.store_u16(context.r4.u32 + 4U, date.day);
    memory.store_u16(context.r4.u32 + 6U, static_cast<std::uint16_t>(seconds / 3600U));
    memory.store_u16(context.r4.u32 + 8U,
                     static_cast<std::uint16_t>((seconds / 60U) % 60U));
    memory.store_u16(context.r4.u32 + 10U,
                     static_cast<std::uint16_t>(seconds % 60U));
    memory.store_u16(context.r4.u32 + 12U,
                     static_cast<std::uint16_t>(remainder / 10'000U));
    memory.store_u16(context.r4.u32 + 14U,
                     static_cast<std::uint16_t>((days + 1U) % 7U));
    return true;
  }
