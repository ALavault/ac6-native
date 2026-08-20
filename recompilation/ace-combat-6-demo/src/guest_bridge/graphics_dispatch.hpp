// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
if (std::string_view{name} == "VdGetSystemCommandBuffer") {
  // The qualified call passes two guest output buffers.  Keep the exact
  // 0x94-byte command-buffer header shape and sentinel values used by the
  // title's subsequent VdSwap call; no host pointer crosses this boundary.
  auto &bridge = require_bridge();
  auto &memory = bridge.memory();
  if (!memory.mapped(context.r3.u32, 0x94U) ||
      !memory.mapped(context.r4.u32, 4U)) {
    return false;
  }
  for (std::uint32_t offset = 0U; offset < 0x94U; offset += 4U) {
    memory.store_u32(context.r3.u32 + offset, 0U);
  }
  memory.store_u32(context.r3.u32, 0xBEEF0000U);
  memory.store_u32(context.r4.u32, 0xBEEF0001U);
  return true;
}
if (std::string_view{name} == "VdSwap") {
  // This is the only reached system-buffer presentation entry in the
  // qualified bootstrap.  Keep all pointers guest-owned and retain the
  // six dwords of the Xenos fetch without exposing a host address.
  auto &bridge = require_bridge();
  auto &memory = bridge.memory();
  const auto stack = context.r1.u32;
  if (stack > std::numeric_limits<std::uint32_t>::max() - 96U ||
      !memory.mapped(context.r3.u32, 256U) ||
      !memory.mapped(context.r4.u32, 24U) ||
      !memory.mapped(context.r5.u32, 4U) ||
      !memory.mapped(context.r6.u32, 0x94U) || context.r7.u32 != 0xBEEF0001U ||
      !memory.mapped(context.r8.u32, 4U) ||
      !memory.mapped(context.r9.u32, 4U) ||
      !memory.mapped(context.r10.u32, 4U) || !memory.mapped(stack + 84U, 4U) ||
      !memory.mapped(stack + 92U, 4U)) {
    return false;
  }
  const auto width_pointer = memory.load_u32(stack + 84U);
  const auto height_pointer = memory.load_u32(stack + 92U);
  if (!memory.mapped(width_pointer, 4U) || !memory.mapped(height_pointer, 4U)) {
    return false;
  }
  ac6demo::VdSwapSnapshot snapshot;
  snapshot.output_buffer = context.r3.u32;
  snapshot.fetch_address = context.r4.u32;
  snapshot.swap_state = context.r5.u32;
  snapshot.system_header = context.r6.u32;
  snapshot.frontbuffer_address = memory.load_u32(context.r8.u32);
  snapshot.texture_format = memory.load_u32(context.r9.u32);
  snapshot.color_space = memory.load_u32(context.r10.u32);
  snapshot.width = memory.load_u32(width_pointer);
  snapshot.height = memory.load_u32(height_pointer);
  for (std::uint32_t index = 0U; index < 6U; ++index) {
    snapshot.fetch_words[index] = memory.load_u32(context.r4.u32 + index * 4U);
  }
  const auto fetch_base = snapshot.fetch_words[1] & 0xFFFFF000U;
  const auto fetch_width = (snapshot.fetch_words[2] & 0x1FFFU) + 1U;
  const auto fetch_height = ((snapshot.fetch_words[2] >> 13U) & 0x1FFFU) + 1U;
  if (snapshot.frontbuffer_address != fetch_base ||
      snapshot.color_space != 0U || snapshot.width != fetch_width ||
      snapshot.height != fetch_height ||
      !bridge.owns_allocation(snapshot.frontbuffer_address, 1U)) {
    return false;
  }
  const auto packet = ac6demo::make_xenos_swap_packet(
      snapshot.fetch_words, snapshot.frontbuffer_address, snapshot.width,
      snapshot.height);
  for (std::uint32_t index = 0U; index < 64U; ++index) {
    memory.store_u32(context.r3.u32 + index * 4U, packet[index]);
  }
  bridge.record_graphics_present(snapshot);
  return true;
}
if (std::string_view{name} == "VdInitializeRingBuffer") {
  // The reached title path passes the physical address returned by
  // MmGetPhysicalAddress and a log2 size.  The bridge's physical heap is
  // deterministic and identity-mapped, so validate both values before
  // accepting the bounded command-buffer boundary.
  auto &bridge = require_bridge();
  if (context.r4.u32 < 12U || context.r4.u32 > 19U ||
      !bridge.memory().mapped(context.r3.u32, 1U) ||
      !bridge.owns_allocation(context.r3.u32, 1U)) {
    return false;
  }
  bridge.configure_xenos_ring(context.r3.u32, context.r4.u32);
  bridge.set_xenos_ring_owner(context.r31.u32);
  return true;
}
if (std::string_view{name} == "VdQueryVideoMode") {
  auto &memory = require_bridge().memory();
  const auto mode = context.r3.u32;
  if (!memory.mapped(mode, 48U)) {
    return false;
  }
  memory.store_u32(mode + 0U, 1280U);
  memory.store_u32(mode + 4U, 720U);
  memory.store_u32(mode + 8U, 0U);
  memory.store_u32(mode + 12U, 1U);
  memory.store_u32(mode + 16U, 1U);
  memory.store_u32(mode + 20U, 0x42700000U); // 60.0f
  memory.store_u32(mode + 24U, 1U);
  memory.store_u32(mode + 28U, 0x4AU);
  memory.store_u32(mode + 32U, 1U);
  for (std::uint32_t offset = 36U; offset < 48U; offset += 4U) {
    memory.store_u32(mode + offset, 0U);
  }
  context.r3.s64 = 0;
  return true;
}
if (std::string_view{name} == "VdGetCurrentDisplayGamma") {
  auto &memory = require_bridge().memory();
  if (!memory.mapped(context.r3.u32, 4U) ||
      !memory.mapped(context.r4.u32, 4U)) {
    return false;
  }
  memory.store_u32(context.r3.u32, 2U); // BT.709/HDTV
  memory.store_u32(context.r4.u32, std::bit_cast<std::uint32_t>(2.22222233F));
  context.r3.s64 = 0;
  return true;
}
if (std::string_view{name} == "VdQueryVideoFlags") {
  context.r3.u32 = 0x3U; // widescreen | >=1024, with no 1920-wide mode
  return true;
}
if (std::string_view{name} == "VdSetDisplayMode") {
  // The title computes this legacy mode word before querying the fixed
  // qualified 1280x720 profile.  Do not let it mutate the strict profile.
  context.r3.s64 = 0;
  return true;
}
if (std::string_view{name} == "VdGetCurrentDisplayInformation") {
  auto &memory = require_bridge().memory();
  const auto info = context.r3.u32;
  if (!memory.mapped(info, 0x58U)) {
    return false;
  }
  for (std::uint32_t offset = 0U; offset < 0x58U; offset += 4U) {
    memory.store_u32(info + offset, 0U);
  }
  memory.store_u16(info + 0x00U, 1280U);
  memory.store_u16(info + 0x02U, 720U);
  memory.store_u32(info + 0x08U, 0U);
  memory.store_u32(info + 0x0CU, 0U);
  memory.store_u32(info + 0x10U, 1280U);
  memory.store_u32(info + 0x14U, 720U);
  memory.store_u32(info + 0x18U, 1U);
  memory.store_u32(info + 0x28U, 1U);
  memory.store_u16(info + 0x40U, 320U);
  memory.store_u16(info + 0x42U, 180U);
  memory.store_u16(info + 0x44U, 320U);
  memory.store_u16(info + 0x46U, 180U);
  memory.store_u16(info + 0x48U, 1280U);
  memory.store_u16(info + 0x4AU, 720U);
  memory.store_u32(info + 0x4CU, 0x42700000U);
  memory.store_u16(info + 0x56U, 1280U);
  return true;
}
if (std::string_view{name} == "VdInitializeScalerCommandBuffer") {
  // The reached title call supplies the destination pointer and word count
  // in the PPC stack parameter area.  Emit only the qualified Xenon NOP
  // fill; unknown scaler commands remain outside this boundary.
  auto &memory = require_bridge().memory();
  const auto stack = context.r1.u32;
  if (stack > std::numeric_limits<std::uint32_t>::max() - 112U ||
      !memory.mapped(stack + 84U, 4U) || !memory.mapped(stack + 92U, 4U) ||
      !memory.mapped(stack + 100U, 4U) || !memory.mapped(stack + 108U, 4U)) {
    return false;
  }
  const auto destination = memory.load_u32(stack + 100U);
  const auto words = memory.load_u32(stack + 108U);
  if (words > 0x10000U ||
      words > std::numeric_limits<std::uint32_t>::max() / 4U ||
      !memory.mapped(destination, static_cast<std::size_t>(words) * 4U)) {
    return false;
  }
  for (std::uint32_t index = 0U; index < words; ++index) {
    memory.store_u32(destination + index * 4U, 0x80000000U);
  }
  context.r3.u32 = words;
  return true;
}
if (std::string_view{name} == "VdPersistDisplay") {
  // Display persistence is outside the demo's public XAM store.  Preserve
  // the kernel's unavailable-resource path and never manufacture a guest
  // allocation that could be mistaken for a save or savestate.
  auto &memory = require_bridge().memory();
  if (context.r4.u32 != 0U && !memory.mapped(context.r4.u32, 4U)) {
    return false;
  }
  if (context.r4.u32 != 0U) {
    memory.store_u32(context.r4.u32, 0U);
  }
  context.r3.s64 = 0;
  return true;
}
if (std::string_view{name} == "VdCallGraphicsNotificationRoutines") {
  auto &memory = require_bridge().memory();
  if (context.r3.u32 != 1U ||
      (context.r4.u32 != 0U && !memory.mapped(context.r4.u32, 8U))) {
    return false;
  }
  if (graphics_interrupt_callback == 0U) {
    context.r3.s64 = -1;
    return true;
  }
  const auto notification = context.r4.u32;
  context.r5.u32 = notification;
  context.r4.u32 = graphics_interrupt_context;
  AC6_PPC_CALL_INDIRECT(context, memory.raw_base(),
                        graphics_interrupt_callback);
  context.r3.s64 = 0;
  return true;
}
if (std::string_view{name} == "VdEnableRingBufferRPtrWriteBack") {
  // The title supplies a write-back address inside the qualified ring
  // allocation and a block-size exponent.  Keep the state boundary
  // deterministic while refusing an address that is not guest-backed.
  auto &bridge = require_bridge();
  if (context.r4.u32 < 6U || context.r4.u32 > 19U ||
      !bridge.memory().mapped(context.r3.u32, 4U) ||
      !bridge.owns_allocation(context.r3.u32, 4U)) {
    return false;
  }
  bridge.enable_xenos_read_pointer_writeback(context.r3.u32);
  bridge.set_xenos_ring_owner(context.r31.u32);
  return true;
}
if (std::string_view{name} == "VdInitializeEngines") {
  // The runtime owns the D3D/Vulkan boundary; no Xenon ring is exposed.
  context.r3.s64 = 0;
  return true;
}
if (std::string_view{name} == "VdSetSystemCommandBufferGpuIdentifierAddress") {
  // The title passes a null identifier before entering its own bounded
  // command-buffer setup.  Keep the address out of the guest/MMIO model.
  context.r3.s64 = 0;
  return true;
}
if (std::string_view{name} == "KiApcNormalRoutineNop" ||
    std::string_view{name} == "KiApcNormalRoutineNop_0") {
  // The import has no guest-visible result on this startup path.
  return true;
}
if (std::string_view{name} == "VdShutdownEngines") {
  // The bounded HLE owns no Xenon engine handles, so shutdown is an
  // idempotent teardown marker only.
  context.r3.s64 = 0;
  return true;
}
if (std::string_view{name} == "VdSetGraphicsInterruptCallback") {
  if (context.r3.u32 != 0U && context.r4.u32 != 0U &&
      !require_bridge().memory().mapped(context.r4.u32, 1U)) {
    return false;
  }
  graphics_interrupt_callback = context.r3.u32;
  graphics_interrupt_context = context.r3.u32 == 0U ? 0U : context.r4.u32;
  ac6demo::guest_bridge_detail::trace_graphics_interrupt_registration(
      graphics_interrupt_callback, graphics_interrupt_context,
      require_bridge().tick(), current_guest_thread_id);
  context.r3.s64 = 0;
  return true;
}
if (std::string_view{name} == "ExRegisterTitleTerminateNotification") {
  context.r3.s64 = 0;
  return true;
}
