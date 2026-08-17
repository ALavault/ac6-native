// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
if (std::string_view{name} == "XamInputGetState") {
  auto &bridge = require_bridge();
  if (context.lr > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  const auto caller_lr = static_cast<std::uint32_t>(context.lr);
  const auto user_index = context.r3.u32;
  const auto flags = context.r4.u32;
  const auto output = context.r5.u32;
  context.r3.u32 = bridge.input_device().get_state(
      context.r3.u32, context.r4.u32, bridge.memory(), context.r5.u32,
      ac6demo::XamInputCallsite{bridge.tick(), current_guest_thread_id,
                                caller_lr});
  if (context.r3.u32 == 0U) {
    bridge.input_device().observe_controller_state_output(caller_lr, output);
    std::array<std::uint8_t, 16U> state16{};
    auto &memory = bridge.memory();
    if (memory.mapped(output, state16.size())) {
      const auto bytes = memory.load_bytes(output, state16.size());
      std::transform(bytes.begin(), bytes.end(), state16.begin(),
                     [](std::byte byte) {
                       return static_cast<std::uint8_t>(byte);
                     });
      // The PAL callsite passes controller+0x44 as X_INPUT_STATE output;
      // the qualified consumer subtracts that fixed offset before reading
      // the controller object.  Arm the chain on that object identity while
      // retaining the returned 16-byte state from the actual output buffer.
      if (output >= 0x44U) {
        ac6demo::guest_bridge_detail::arm_xam_return_chain(
            caller_lr, bridge.tick(), current_guest_thread_id, user_index,
            flags, output - 0x44U, context.r3.u32, state16.data());
      }
    }
  }
  return true;
}
if (std::string_view{name} == "XamInputGetCapabilities") {
  auto &bridge = require_bridge();
  context.r3.u32 = bridge.input_device().get_capabilities(
      context.r3.u32, context.r4.u32, bridge.memory(), context.r5.u32);
  return true;
}
if (std::string_view{name} == "XamInputSetState") {
  auto &bridge = require_bridge();
  context.r3.u32 = bridge.input_device().set_vibration(
      context.r3.u32, context.r4.u32, bridge.memory(), context.r5.u32);
  return true;
}
if (std::string_view{name} == "XamInputGetKeystrokeEx") {
  auto &bridge = require_bridge();
  auto &memory = bridge.memory();
  if (!memory.mapped(context.r3.u32, 4U)) {
    return false;
  }
  const auto user_index = memory.load_u32(context.r3.u32);
  context.r3.u32 = bridge.input_device().get_keystroke(
      user_index, context.r4.u32, memory, context.r5.u32);
  return true;
}
if (std::string_view{name} == "XamLoaderGetLaunchDataSize") {
  // The qualified demo is launched without a title-to-title payload.
  context.r3.u32 = 0U;
  return true;
}
if (std::string_view{name} == "XamLoaderGetLaunchData") {
  // A zero-sized launch payload leaves the caller's zero-filled buffer
  // untouched; no retail launch data is imported into the demo store.
  context.r3.u32 = 0U;
  return true;
}
if (std::string_view{name} == "XamGetSystemVersion") {
  context.r3.u32 = 0x20023200U;
  return true;
}
if (std::string_view{name} == "ExGetXConfigSetting") {
  // The reached bootstrap query is XCONFIG_USER_VIDEO_FLAGS
  // (category=3, setting=10).  This is a qualified kernel ABI value, not a
  // host display override: the guest consumes it while constructing its
  // video/Xenos state.  Keep every other XConfig request fail-closed until
  // its callsite and payload are qualified separately.
  auto &memory = memory_for(context);
  if (context.r3.u32 != 3U || context.r4.u32 != 10U || context.r6.u32 < 4U ||
      !memory.mapped(context.r5.u32, 4U) ||
      (context.r7.u32 != 0U && !memory.mapped(context.r7.u32, 2U))) {
    return false;
  }
  memory.store_u32(context.r5.u32, 0x00040000U);
  if (context.r7.u32 != 0U) {
    memory.store_u16(context.r7.u32, 4U);
  }
  context.r3.s64 = 0;
  return true;
}
if (std::string_view{name} == "ObDeleteSymbolicLink") {
  // The qualified store exposes no retail device namespace.  These title
  // cleanup calls are idempotent and their return value is not consumed by
  // the reached bootstrap path.
  context.r3.s64 = -1;
  return true;
}
