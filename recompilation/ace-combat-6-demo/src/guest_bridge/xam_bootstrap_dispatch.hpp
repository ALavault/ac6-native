// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
  if (std::string_view{name} == "NtAllocateVirtualMemory") {
    return dispatch_allocate(context);
  }
  if (std::string_view{name} == "KeGetCurrentProcessType") {
    // A title runs in the user process class. The guest stores this byte in
    // its process records and compares it against this kernel result.
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "XexCheckExecutablePrivilege") {
    // The qualified title has no elevated privilege requirement for this
    // bootstrap probe; the guest branches on the documented boolean result.
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "ExGetXConfigSetting") {
    // The demo queries the PAL XConfig values used for language, country,
    // video flags, and AV region during bootstrap.  These are fixed guest
    // platform records, not host preferences or retail state.
    auto& memory = require_bridge().memory();
    const auto category = static_cast<std::uint16_t>(context.r3.u32);
    const auto setting = static_cast<std::uint16_t>(context.r4.u32);
    std::uint32_t value = 0U;
    std::uint16_t value_size = 0U;
    if (category == 2U && setting == 2U) {
      value = 0x00001000U;  // PAL/NTSC AV-region-compatible platform value
      value_size = 4U;
    } else if (category == 3U) {
      switch (setting) {
        case 1U:
        case 2U:
        case 3U:
        case 4U:
        case 5U:
        case 6U:
        case 7U:
        case 12U:
          value = 0U;
          value_size = 4U;
          break;
        case 9U:
          value = 1U;  // English guest language
          value_size = 4U;
          break;
        case 10U:
          value = 0x00040000U;
          value_size = 4U;
          break;
        case 14U:
          value = 103U;  // PAL/US-compatible guest country code
          value_size = 1U;
          break;
        default:
          return false;
      }
    } else {
      return false;
    }
    const auto buffer = context.r5.u32;
    const auto buffer_size = context.r6.u32;
    const auto required_size = context.r7.u32;
    if (required_size != 0U && !memory.mapped(required_size, 2U)) {
      return false;
    }
    if (buffer == 0U) {
      if (buffer_size != 0U) {
        context.r3.u32 = 0xC000000DU;  // STATUS_INVALID_PARAMETER
        return true;
      }
      if (required_size != 0U) {
        memory.store_u16(required_size, value_size);
      }
      context.r3.u32 = 0U;
      return true;
    }
    if (buffer_size < value_size || !memory.mapped(buffer, value_size)) {
      context.r3.u32 = 0xC0000023U;  // STATUS_BUFFER_TOO_SMALL
      return true;
    }
    if (value_size == 1U) {
      memory.store_u8(buffer, static_cast<std::uint8_t>(value));
    } else {
      memory.store_u32(buffer, value);
    }
    if (required_size != 0U) {
      memory.store_u16(required_size, value_size);
    }
    context.r3.u32 = 0U;
    return true;
  }
  if (std::string_view{name} == "XamNotifyCreateListener") {
    // XamNotifyCreateListener(mask, max_version) returns a kernel listener
    // handle.  Keep only the qualified offline queue: no host notifications
    // are injected, so XNotifyGetNext deterministically reports an empty
    // queue until a title-reached broadcast contract is qualified.
    const auto max_version = context.r4.u32;
    if (max_version > 10U || next_notify_handle > 0xFFFFFFFCU) {
      return false;
    }
    const auto handle = next_notify_handle;
    next_notify_handle += 4U;
    notify_listeners.emplace(handle,
                             GuestNotifyListener{context.r3.u64, max_version});
    context.r3.u32 = handle;
    return true;
  }
  if (std::string_view{name} == "XNotifyGetNext") {
    // XNotifyGetNext(handle, match_id, id*, param*) clears the output slots
    // and returns zero when the listener has no pending notification.  The
    // demo's strict offline profile does not synthesize sign-in or network
    // notifications, but it does preserve the listener ABI and close path.
    auto& memory = require_bridge().memory();
    const auto listener = notify_listeners.find(context.r3.u32);
    const auto id_pointer = context.r5.u32;
    const auto param_pointer = context.r6.u32;
    if (listener == notify_listeners.end() || id_pointer == 0U ||
        !memory.mapped(id_pointer, 4U) ||
        (param_pointer != 0U && !memory.mapped(param_pointer, 4U))) {
      return false;
    }
    memory.store_u32(id_pointer, 0U);
    if (param_pointer != 0U) {
      memory.store_u32(param_pointer, 0U);
    }
    context.r3.u32 = 0U;
    return true;
  }
  if (std::string_view{name} == "XNotifyPositionUI") {
    // XNotifyPositionUI(position) is void. The reached callsite loads the
    // qualified value 6 into r3 and does not consume a return value.
    return require_bridge().set_notify_ui_position(context.r3.u32);
  }
  if (std::string_view{name} == "XamUserGetSigninState") {
    // The reached XAM seam enumerates the four hardware user slots. Expose
    // one explicit offline-local profile and report the remaining slots as
    // unsigned; do not promote any slot to an Xbox Live identity.
    const auto signin_state =
        require_bridge().xam_user_signin_state(context.r3.u32);
    if (!signin_state.has_value()) {
      return false;
    }
    context.r3.u32 = *signin_state;
    return true;
  }
  if (std::string_view{name} == "XamUserGetName") {
    // Reached ABI: user zero, 16-byte buffer, zero on success. Use the same
    // explicit offline profile as XamUserGetSigninState and preserve bytes
    // beyond the terminating NUL.
    if (!require_bridge().write_xam_user_name(
            context.r3.u32, context.r4.u32, context.r5.u32)) {
      return false;
    }
    context.r3.u32 = 0U;
    return true;
  }
  if (std::string_view{name} == "XamUserReadProfileSettings") {
    // The reached AC6 path is the first, size-only profile query.  Preserve
    // the XAM sizing contract without fabricating profile contents; a later
    // materialization call must be qualified separately if the demo reaches
    // one.
    auto& memory = require_bridge().memory();
    const auto setting_count = context.r7.u32;
    const auto setting_ids = context.r8.u32;
    const auto size_pointer = context.r9.u32;
    const auto buffer = context.r10.u32;
    if (setting_count == 0U || setting_count > 32U || size_pointer == 0U ||
        !memory.mapped(size_pointer, 4U) ||
        (context.r5.u32 != 0U && context.r6.u32 == 0U) ||
        !memory.mapped(setting_ids,
                       static_cast<std::size_t>(setting_count) * 4U)) {
      return false;
    }

    std::uint64_t needed_size = 8U +
                                static_cast<std::uint64_t>(setting_count) * 40U;
    for (std::uint32_t index = 0U; index < setting_count; ++index) {
      const auto setting_id = memory.load_u32(setting_ids + index * 4U);
      const auto setting_type = setting_id >> 28U;
      if (setting_type == 4U || setting_type == 6U) {
        needed_size += (setting_id >> 16U) & 0x0FFFU;
      }
    }
    if (needed_size > std::numeric_limits<std::uint32_t>::max()) {
      return false;
    }
    const auto available_size = memory.load_u32(size_pointer);
    if (available_size == 0U) {
      memory.store_u32(size_pointer, static_cast<std::uint32_t>(needed_size));
      context.r3.u32 = 122U;  // ERROR_INSUFFICIENT_BUFFER
      return true;
    }
    if (buffer == 0U || available_size < needed_size ||
        !memory.mapped(buffer, static_cast<std::size_t>(available_size))) {
      context.r3.u32 = 122U;  // ERROR_INSUFFICIENT_BUFFER
      return true;
    }
    return false;
  }
  if (std::string_view{name} == "XMsgStartIORequest" && ordinal == 503U) {
    // PAL-observed XGI user-context request at tick 4254. Xenia/ReXGlue use
    // the same five-register ABI and their XgiApp handler only validates/logs
    // this message before returning X_E_SUCCESS. Keep this boundary read-only
    // and accept only the exact guest tuple captured at LR 0x821A55A0.
    if (!ac6demo::validate_xgi_user_context_request(
            require_bridge().memory(), static_cast<std::uint32_t>(context.lr),
            context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32,
            context.r7.u32)) {
      return false;
    }
    context.r3.u32 = 0U; // X_E_SUCCESS
    return true;
  }
  if (std::string_view{name} == "NetDll_listen") {
    // The demo has no network endpoint. Preserve the Winsock failure path;
    // returning success here would manufacture title state.
    network_error = 10045;  // WSAEOPNOTSUPP
    context.r3.s64 = -1;
    return true;
  }
  if (std::string_view{name} == "NetDll_socket") {
    // The qualified demo has no network transport.  Keep socket creation on
    // its documented failure path so later code can select the offline mode;
    // never create a host socket or return a fabricated guest handle.
    network_error = 10050;  // WSAENETDOWN
    context.r3.s64 = -1;
    return true;
  }
  if (std::string_view{name} == "NetDll_bind") {
    // The preceding offline socket creation returns an invalid handle.  Keep
    // the Winsock error contract coherent and do not inspect or modify the
    // caller's sockaddr buffer.
    network_error = 10038;  // WSAENOTSOCK
    context.r3.s64 = -1;
    return true;
  }
  if (std::string_view{name} == "NetDll_recvfrom") {
    // static_exact: 0x821CA128 remaps the wrapper arguments to the XAM ABI;
    // 0x82134008 branches on -1 before consuming either receive buffer.
    // bridge_observed: the offline socket seam reaches this call with caller
    // 1, socket -1, length 1273, flags 0 and two non-null address outputs.
    // Preserve the invalid-socket failure without opening a host socket or
    // modifying guest buffers. Any other receive shape remains unqualified.
    auto& memory = require_bridge().memory();
    if (context.r3.u32 != 1U || context.r4.u32 != 0xFFFFFFFFU ||
        context.r6.u32 != 1273U || context.r7.u32 != 0U ||
        context.r5.u32 == 0U || context.r8.u32 == 0U ||
        context.r9.u32 == 0U || !memory.mapped(context.r5.u32, 1273U) ||
        !memory.mapped(context.r8.u32, 16U) ||
        !memory.mapped(context.r9.u32, 4U)) {
      return false;
    }
    network_error = 10038;  // WSAENOTSOCK
    context.r3.s64 = -1;
    return true;
  }
  if (std::string_view{name} == "NetDll_WSAGetLastError") {
    context.r3.s64 = network_error;
    return true;
  }
  if (std::string_view{name} == "NetDll_XNetStartup") {
    // Networking is outside the qualified demo boundary; keep the title's
    // offline/error path observable instead of exposing a fake service.
    network_error = 10050;  // WSAENETDOWN
    context.r3.s64 = -1;
    return true;
  }
  if (std::string_view{name} == "NetDll_WSAStartup") {
    network_error = 10050;  // WSAENETDOWN
    context.r3.s64 = -1;
    return true;
  }
  if (std::string_view{name} == "NetDll_WSACleanup") {
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "NetDll_XNetCleanup") {
    context.r3.s64 = 0;
    return true;
  }
  if (std::string_view{name} == "NetDll_XNetGetTitleXnAddr") {
    // XNADDR is a 0x24-byte big-endian guest record.  The demo is qualified
    // for an offline run, so expose the documented NONE state and a zero
    // address rather than consulting host networking or inventing a title
    // address.  The caller treats any non-pending status as terminal.
    auto& memory = require_bridge().memory();
    const auto address = context.r4.u32;
    constexpr std::size_t kXnaddrSize = 0x24U;
    if (address == 0U || !memory.mapped(address, kXnaddrSize)) {
      return false;
    }
    for (std::size_t offset = 0U; offset < kXnaddrSize; offset += 4U) {
      memory.store_u32(address + static_cast<std::uint32_t>(offset), 0U);
    }
    context.r3.u32 = 1U;  // XNET_GET_XNADDR_NONE
    return true;
  }
