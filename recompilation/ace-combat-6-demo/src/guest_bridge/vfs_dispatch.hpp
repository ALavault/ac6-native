// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
  if (std::string_view{name} == "NtClose") {
    const auto erased = events.erase(context.r3.u32) +
                        notify_listeners.erase(context.r3.u32) +
                        timers.erase(context.r3.u32) +
                        mutants.erase(context.r3.u32) + semaphores.erase(context.r3.u32);
    const auto thread_erased = require_bridge().close_guest_thread(context.r3.u32);
    const auto file_erased = require_bridge().close_guest_file(context.r3.u32);
    context.r3.s64 = (erased == 1U || thread_erased || file_erased) ? 0 : -1;
    return true;
  }
  if (std::string_view{name} == "NtQueryFullAttributesFile") {
    auto& bridge = require_bridge();
    auto& memory = bridge.memory();
    constexpr std::uint32_t kStatusInvalidParameter = 0xC000000DU;
    std::string xbox_path;
    if (!ac6demo::read_guest_object_attributes_path(
            memory, context.r3.u32, &xbox_path) ||
        !memory.mapped(context.r4.u32, 56U)) {
      context.r3.u32 = kStatusInvalidParameter;
      return true;
    }
    std::uint32_t handle{};
    const auto status = bridge.open_guest_file(xbox_path, 0U, 1U, &handle);
    if (status != 0U) {
      context.r3.u32 = status;
      return true;
    }
    std::uint64_t size{};
    const bool queried = bridge.guest_file_size(handle, &size);
    const bool closed = bridge.close_guest_file(handle);
    if (!queried || !closed ||
        !ac6demo::write_guest_file_network_open_information(
            memory, context.r4.u32, 56U, size)) {
      return false;
    }
    context.r3.u32 = 0U;
    return true;
  }
  if (std::string_view{name} == "NtCreateFile") {
    auto& bridge = require_bridge();
    auto& memory = bridge.memory();
    constexpr std::uint32_t kStatusInvalidParameter = 0xC000000DU;
    const auto handle_pointer = context.r3.u32;
    const auto object_attributes = context.r5.u32;
    const auto io_status = context.r6.u32;
    if (!memory.mapped(handle_pointer, 4U) || !memory.mapped(io_status, 8U)) {
      return false;
    }
    std::string xbox_path;
    if (!ac6demo::read_guest_object_attributes_path(
            memory, object_attributes, &xbox_path)) {
      memory.store_u32(io_status, kStatusInvalidParameter);
      memory.store_u32(io_status + 4U, 0U);
      context.r3.u32 = kStatusInvalidParameter;
      return true;
    }
    std::uint32_t handle{};
    const auto status = bridge.open_guest_file(xbox_path, context.r4.u32,
                                               context.r10.u32, &handle);
    if (status == 0U) {
      memory.store_u32(handle_pointer, handle);
    }
    memory.store_u32(io_status, status);
    memory.store_u32(io_status + 4U, status == 0U ? 1U : 0U);
    context.r3.u32 = status;
    return true;
  }
  if (std::string_view{name} == "NtQueryInformationFile") {
    auto& bridge = require_bridge();
    auto& memory = bridge.memory();
    constexpr std::uint32_t kStatusSuccess = 0U;
    constexpr std::uint32_t kStatusInvalidHandle = 0xC0000008U;
    constexpr std::uint32_t kStatusInvalidInfoClass = 0xC0000003U;
    constexpr std::uint32_t kFileNetworkOpenInformation = 34U;
    const auto io_status = context.r4.u32;
    if (!memory.mapped(io_status, 8U)) {
      return false;
    }
    std::uint64_t size{};
    std::uint32_t status = kStatusSuccess;
    if (!bridge.guest_file_size(context.r3.u32, &size)) {
      status = kStatusInvalidHandle;
    } else if (context.r7.u32 != kFileNetworkOpenInformation) {
      status = kStatusInvalidInfoClass;
    } else if (!ac6demo::write_guest_file_network_open_information(
                   memory, context.r5.u32, context.r6.u32, size)) {
      return false;
    }
    memory.store_u32(io_status, status);
    memory.store_u32(io_status + 4U, status == 0U ? 56U : 0U);
    context.r3.u32 = status;
    return true;
  }
  if (std::string_view{name} == "NtSetInformationFile") {
    auto& bridge = require_bridge();
    auto& memory = bridge.memory();
    constexpr std::uint32_t kStatusSuccess = 0U;
    constexpr std::uint32_t kStatusInvalidHandle = 0xC0000008U;
    constexpr std::uint32_t kStatusInvalidInfoClass = 0xC0000003U;
    constexpr std::uint32_t kStatusInvalidParameter = 0xC000000DU;
    constexpr std::uint32_t kFilePositionInformation = 14U;
    const auto io_status = context.r4.u32;
    if (!memory.mapped(io_status, 8U)) {
      return false;
    }
    std::uint64_t ignored_size{};
    std::uint32_t status = kStatusSuccess;
    if (!bridge.guest_file_size(context.r3.u32, &ignored_size)) {
      status = kStatusInvalidHandle;
    } else if (context.r7.u32 != kFilePositionInformation) {
      status = kStatusInvalidInfoClass;
    } else if (context.r6.u32 < 8U || !memory.mapped(context.r5.u32, 8U)) {
      return false;
    } else {
      const auto position = memory.load_u64(context.r5.u32);
      if (static_cast<std::int64_t>(position) < 0) {
        status = kStatusInvalidParameter;
      } else if (!bridge.set_guest_file_position(context.r3.u32, position)) {
        status = kStatusInvalidHandle;
      }
    }
    memory.store_u32(io_status, status);
    memory.store_u32(io_status + 4U, 0U);
    context.r3.u32 = status;
    return true;
  }
  if (std::string_view{name} == "NtReadFile") {
    auto& bridge = require_bridge();
    auto& memory = bridge.memory();
    constexpr std::uint32_t kStatusSuccess = 0U;
    const auto io_status = context.r7.u32;
    const auto buffer = context.r8.u32;
    const auto length = context.r9.u32;
    const auto event_handle = context.r4.u32 & ~std::uint32_t{1U};
    if (context.r5.u32 != 0U ||
        (context.r6.u32 != 0U && context.r6.u32 != io_status)) {
      throw ac6demo::RuntimeTrap(
          "unqualified NtReadFile APC completion mode event=" +
              std::to_string(context.r4.u32) + " apc=" +
              std::to_string(context.r5.u32) + " context=" +
              std::to_string(context.r6.u32) + " iosb=" +
              std::to_string(io_status),
          bridge.tick(), static_cast<std::uint32_t>(context.lr), context.r3.u32);
    }
    if (event_handle != 0U && events.find(event_handle) == events.end()) {
      throw ac6demo::RuntimeTrap(
          "NtReadFile references an unknown event handle",
          bridge.tick(), static_cast<std::uint32_t>(context.lr), event_handle);
    }
    if (!memory.mapped(io_status, 8U) ||
        (length != 0U && !memory.mapped(buffer, length))) {
      return false;
    }
    std::optional<std::uint64_t> offset;
    if (context.r10.u32 != 0U) {
      if (!memory.mapped(context.r10.u32, 8U)) {
        return false;
      }
      offset = memory.load_u64(context.r10.u32);
      if (static_cast<std::int64_t>(*offset) < 0) {
        return false;
      }
    }
    std::vector<std::byte> bytes(length);
    std::uint32_t bytes_read{};
    const auto status = bridge.read_guest_file(context.r3.u32, offset, bytes,
                                               &bytes_read);
    if (status == kStatusSuccess) {
      for (std::uint32_t index = 0U; index < bytes_read; ++index) {
        memory.store_u8(buffer + index,
                        static_cast<std::uint8_t>(bytes[index]));
      }
    }
    memory.store_u32(io_status, status);
    memory.store_u32(io_status + 4U, bytes_read);
    if (event_handle != 0U) {
      publish_guest_event(bridge, event_handle, events.at(event_handle),
                          static_cast<std::uint32_t>(context.lr));
    }
    context.r3.u32 = status;
    return true;
  }
