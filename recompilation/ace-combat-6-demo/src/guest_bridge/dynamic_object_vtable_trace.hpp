#pragma once

thread_local bool ac6_body_trace_active = false;

inline void trace_body_store(const PPCContext &context, std::uint32_t address,
                             std::uint32_t value, const char *generated_name,
                             std::uint32_t generated_line) noexcept {
  if (!ac6_body_trace_active ||
      std::getenv("AC6_DEMO_WATCH_BODY_STATE") == nullptr ||
      address < 0x82934000U || address >= 0x82935000U) {
    return;
  }
  std::fprintf(stderr,
               "AC6_BODY_STORE address=0x%08X value=0x%08X tick=%llu "
               "thread=%u lr=0x%08X function=%s generated_line=%u\n",
               address, value,
               static_cast<unsigned long long>(require_bridge().tick()),
               current_guest_thread_id, static_cast<std::uint32_t>(context.lr),
               generated_name == nullptr ? "" : generated_name,
               generated_line);
}

template <typename Function>
static void invoke_body_trace(Function function, PPCContext &context,
                               std::uint8_t *base,
                               std::uint32_t guest_address) {
  const bool previous = ac6_body_trace_active;
  ac6_body_trace_active =
      std::getenv("AC6_DEMO_WATCH_BODY_STATE") != nullptr &&
      guest_address == 0x822F8848U;
  try {
    function(context, base);
  } catch (...) {
    ac6_body_trace_active = previous;
    throw;
  }
  ac6_body_trace_active = previous;
}

static void trace_dynamic_object_vtable(const PPCContext &context,
                                        std::uint32_t lr,
                                        std::uint32_t guest_address) {
  if (std::getenv("AC6_DEMO_WATCH_INDIRECT_OBJECT") == nullptr ||
      lr != 0x822E559CU || guest_address != 0x822F8848U) {
    return;
  }
  auto &memory = require_bridge().memory();
  const auto object = context.r3.u32;
  std::uint32_t vtable = 0U;
  std::uint32_t slot4 = 0U;
  const bool object_mapped = object != 0U && memory.mapped(object, 4U);
  bool vtable_mapped = false;
  bool slot4_mapped = false;
  if (object_mapped) {
    vtable = memory.load_u32(object);
    vtable_mapped = vtable != 0U && memory.mapped(vtable, 20U);
    if (vtable_mapped) {
      slot4_mapped = memory.mapped(vtable + 16U, 4U);
      if (slot4_mapped) {
        slot4 = memory.load_u32(vtable + 16U);
      }
    }
  }
  std::fprintf(
      stderr,
      "AC6_INDIRECT_TARGET lr=0x%08X target=0x%08X tick=%llu "
      "thread=%u object=0x%08X object_mapped=%u vtable=0x%08X "
      "vtable_mapped=%u slot=4 slot_mapped=%u slot_target=0x%08X "
      "r4=0x%08X r5=0x%08X r6=0x%08X r7=0x%08X\n",
      lr, guest_address,
      static_cast<unsigned long long>(require_bridge().tick()),
      current_guest_thread_id, object, object_mapped ? 1U : 0U, vtable,
      vtable_mapped ? 1U : 0U, slot4_mapped ? 1U : 0U, slot4,
      context.r4.u32, context.r5.u32, context.r6.u32, context.r7.u32);
}
