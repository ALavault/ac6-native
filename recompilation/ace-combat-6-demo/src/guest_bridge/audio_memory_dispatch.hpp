// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
  if (std::string_view{name} == "XMACreateContext") {
    // XDK XMADecoder.h: STDAPI XMACreateContext(PXMACONTEXT *ppContext).
    // Exactly one parameter, an out-pointer; r4..r7 are not arguments and
    // whatever the caller left in them is stale. The earlier opt-in
    // experiment pinned that whole register tuple and three specific output
    // addresses because those were what one observed run happened to carry;
    // neither is part of the contract, and guest-side storage is not the
    // kernel's business. What remains fail-closed is the reached callsite,
    // that the out-pointer is writable, and the allocator's own bound.
    auto &bridge = require_bridge();
    auto &memory = bridge.memory();
    const auto output_slot = context.r3.u32;
    if (static_cast<std::uint32_t>(context.lr) != 0x82357298U ||
        output_slot == 0U || !memory.mapped(output_slot, 4U)) {
      return false;
    }
    const auto context_pointer = bridge.allocate_xma_context();
    memory.store_u32(output_slot, context_pointer);
    // S_OK, or the exhaustion status the opt-in experiment already used. The
    // allocator holds 320 contexts and the demo creates three, so this error
    // path is unreached and its exact value stays unqualified.
    context.r3.u32 = context_pointer == 0U ? 0xC0000017U : 0U;
    return true;
  }
  if (std::string_view{name} == "XMAReleaseContext") {
    // XDK: VOID XMAReleaseContext(PXMACONTEXT pContext), one parameter and no
    // return value; it "does not actually free memory; it merely places the
    // context on the free list". release_xma_context already validates the
    // pointer against the array this runtime handed out, which is the real
    // check the three pinned addresses were standing in for.
    const auto context_pointer = context.r3.u32;
    if (static_cast<std::uint32_t>(context.lr) != 0x82356820U ||
        !require_bridge().release_xma_context(context_pointer)) {
      return false;
    }
    // The function is VOID, so the caller must not read r3; it is written
    // deterministically rather than left carrying the argument.
    context.r3.u32 = 0U;
    return true;
  }
  if (std::string_view{name} == "XAudioSubmitRenderDriverFrame") {
    // xboxkrnl.exe ordinal 501, an export of the official xboxkrnl.lib that
    // no SDK header declares. The call shape comes from the call itself:
    // r3 is the handle this bridge issued, and r4 is a guest buffer of the
    // exact byte count the client announced at registration (0x1800 = 6144,
    // which is XAUDIOFRAMESIZE_NATIVE 256 samples of six 32-bit channels).
    //
    // This is a sink and nothing more. The frame is counted and its buffer
    // checked; not one sample is decoded, mixed, resampled or handed to a
    // host audio device, and no audible output is claimed anywhere. Its only
    // purpose is to let the guest's own render loop complete a frame instead
    // of trapping halfway through it.
    auto &bridge = require_bridge();
    auto &memory = bridge.memory();
    const auto frame_pointer = context.r4.u32;
    const auto frame_bytes = bridge.xaudio_frame_bytes();
    if (context.r3.u32 != bridge.xaudio_client_handle() || frame_bytes == 0U ||
        frame_pointer == 0U || !memory.mapped(frame_pointer, frame_bytes)) {
      return false;
    }
    bridge.count_xaudio_frame_submission();
    context.r3.u32 = 0U;
    return true;
  }
  if (std::string_view{name} == "XAudioGetVoiceCategoryVolumeChangeMask") {
    // xboxkrnl.exe ordinal 503. The name is authoritative -- it is an export
    // of the official xboxkrnl.lib in every SDK version here -- but no SDK
    // header declares it and no doc page describes it, so the shape comes
    // from the call itself: r3 is 0xE4000000, which is the very handle this
    // bridge returned from XAudioRegisterRenderDriverClient, and r4 is a
    // mapped guest pointer, so this is (client handle, out mask).
    //
    // The mask says which voice categories changed volume. Nothing in this
    // runtime ever changes one and the guest has never called
    // XAudioSetVoiceCategoryVolume, so the only answer consistent with this
    // runtime's own state is "none". A different handle fails closed.
    auto &bridge = require_bridge();
    auto &memory = bridge.memory();
    const auto mask_pointer = context.r4.u32;
    if (context.r3.u32 != bridge.xaudio_client_handle() ||
        mask_pointer == 0U || !memory.mapped(mask_pointer, 4U)) {
      return false;
    }
    memory.store_u32(mask_pointer, 0U);
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
    // r5 carries the render frame size in bytes; the submit path validates
    // its buffer against exactly this number rather than a constant.
    bridge.set_xaudio_frame_bytes(context.r5.u32);
    if (std::getenv("AC6_DEMO_WATCH_IMPORTS") != nullptr) {
      std::fprintf(stderr,
                   "AC6_XAUDIO_REGISTER tick=%llu descriptor=0x%08X "
                   "callback=0x%08X context=0x%08X frame_bytes=0x%X "
                   "r6=0x%08X r7=0x%08X r8=0x%08X\n",
                   static_cast<unsigned long long>(bridge.tick()), descriptor,
                   callback, callback_context, context.r5.u32, context.r6.u32,
                   context.r7.u32, context.r8.u32);
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
    // Pure observation, no effect on the returned address: the kick window
    // checks that the context being kicked is the one whose physical address
    // the guest just asked for. Gating it on the experiment left that state
    // at zero on the nominal path, so every kick was refused.
    bridge.observe_xma_physical_context(address);
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
