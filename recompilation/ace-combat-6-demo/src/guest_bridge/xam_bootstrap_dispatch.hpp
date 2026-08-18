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
    // Answered from the title's own XEX_HEADER_SYSTEM_FLAGS rather than a
    // constant. The demo's Default.xex declares 0x00000600, so privilege 10 is
    // granted and 11 and 23 are not -- the three the guest asks about. Returning
    // a flat 0 made sub_821A6F78 skip its XGetAVPack block, which the retail
    // runtime enters at frame 0.
    context.r3.s64 =
        require_bridge().executable_privilege(context.r3.u32) ? 1 : 0;
    return true;
  }
  if (std::string_view{name} == "XGetAVPack") {
    // xam.xex ordinal 971, absent from the public XDK headers. It has exactly
    // one caller in this image, sub_821A6F78, which skips its block when the
    // answer is 3, 6, 8 or 4 and otherwise queries ExGetXConfigSetting(2, 2)
    // and requires (value & 0xFF00) == 0x300. This bridge's qualified AV-region
    // record for that setting is 0x00001000, whose masked value is 0x1000, so
    // the block is skipped at the next test whatever is answered here.
    //
    // The choice is therefore inert in this image, and it is written down as a
    // choice rather than dressed up as a derivation: nothing available
    // qualifies a specific AV pack, and this must be re-qualified if a later
    // path ever reads the value.
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
// XDK xam.h: XNID(Version, Area, Index) = Area << 25 | Version << 16 | Index,
// with _XNAREA_SYSTEM 0, so XN_SYS_UI = XNID(0, 0, 0x0009) and
// XN_SYS_SIGNINCHANGED = XNID(0, 0, 0x000A). XNOTIFY_SYSTEM is the area bit a
// listener asks for. Values are read from the header, not inferred from names.
constexpr std::uint32_t kNotificationSystemUi = 0x00000009U;
constexpr std::uint32_t kNotificationSystemSigninChanged = 0x0000000AU;
constexpr std::uint64_t kNotifyAreaSystem = 0x00000001U;

  if (std::string_view{name} == "XamNotifyCreateListener") {
    // XamNotifyCreateListener(qwAreas) returns a kernel listener handle.
    //
    // XDK XNotifyCreateListener: "Before the first listener is created [...]
    // four types of system notifications will be saved by the notification
    // system: XN_LIVE_INVITE_ACCEPTED, XN_SYS_UI, XN_SYS_SIGNINCHANGED,
    // XN_SYS_DISCMEDIACHANGED. These notifications are saved by category until
    // the first listener eligible to receive the notifications is created;
    // then they are immediately delivered to that first listener."
    //
    // The console therefore hands a title its initial sign-in state through a
    // notification, not only through polling. Without it a title that waits
    // for XN_SYS_SIGNINCHANGED before leaving its start-up mode waits forever.
    const auto max_version = context.r4.u32;
    if (max_version > 10U || next_notify_handle > 0xFFFFFFFCU) {
      return false;
    }
    const auto handle = next_notify_handle;
    next_notify_handle += 4U;
    const auto areas = context.r3.u64;
    GuestNotifyListener listener{areas, max_version, {}};
    if (!saved_notifications_delivered && (areas & kNotifyAreaSystem) != 0U) {
      // Of the four saved notifications only the two system ones have a
      // qualified offline value here. XN_LIVE_INVITE_ACCEPTED needs a pending
      // Live invite, which an offline demo cannot have, and
      // XN_SYS_DISCMEDIACHANGED reports a media change that never happens
      // against a fixed store, so neither is synthesized.
      //
      // XN_SYS_UI param is BOOL fShow: no system UI is over this title.
      // XN_SYS_SIGNINCHANGED param is DWORD dwValid, "bit specifying which
      // players are valid, the least-significant bit represents user zero";
      // slot 0 is signed in locally and slots 1..3 are not, which is exactly
      // what XamUserGetSigninState already reports, so the bitmask is 1.
      listener.pending.push_back(GuestNotification{kNotificationSystemUi, 0U});
      listener.pending.push_back(
          GuestNotification{kNotificationSystemSigninChanged, 1U});
      saved_notifications_delivered = true;
    }
    notify_listeners.emplace(handle, std::move(listener));
    context.r3.u32 = handle;
    return true;
  }
  if (std::string_view{name} == "XNotifyGetNext") {
    // XNotifyGetNext(handle, dwMsgFilter, pdwId, pParam). dwMsgFilter is a
    // notification id, "set to zero to retrieve any notification"; the return
    // value is "nonzero" when a notification was returned and zero when the
    // queue is empty. Output slots are cleared on the empty path so the ABI
    // stays the one the guest already relied on.
    auto& memory = require_bridge().memory();
    const auto listener = notify_listeners.find(context.r3.u32);
    const auto filter = context.r4.u32;
    const auto id_pointer = context.r5.u32;
    const auto param_pointer = context.r6.u32;
    if (listener == notify_listeners.end() || id_pointer == 0U ||
        !memory.mapped(id_pointer, 4U) ||
        (param_pointer != 0U && !memory.mapped(param_pointer, 4U))) {
      return false;
    }
    auto& pending = listener->second.pending;
    const auto match = std::find_if(
        pending.begin(), pending.end(),
        [filter](const GuestNotification& notification) {
          return filter == 0U || notification.id == filter;
        });
    if (match == pending.end()) {
      memory.store_u32(id_pointer, 0U);
      if (param_pointer != 0U) {
        memory.store_u32(param_pointer, 0U);
      }
      context.r3.u32 = 0U;
      return true;
    }
    memory.store_u32(id_pointer, match->id);
    if (param_pointer != 0U) {
      memory.store_u32(param_pointer, match->param);
    }
    pending.erase(match);
    context.r3.u32 = 1U;
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
