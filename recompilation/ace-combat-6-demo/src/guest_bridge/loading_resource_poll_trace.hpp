#pragma once

namespace {
struct LoadingResourcePollTraceState final {
  bool active = false;
  bool mapped = false;
  bool saw_af20 = false;
  bool saw_95b50 = false;
  std::uint32_t object = 0;
  std::uint32_t state = 0;
  std::uint32_t buffer = 0;
  std::uint32_t job = 0;
  std::uint32_t resource = 0;
  std::uint8_t flag = 0;
};

thread_local LoadingResourcePollTraceState loading_resource_poll_trace;
thread_local std::uint32_t loading_resource_poll_trace_count = 0;
thread_local std::uint32_t loading_provider_task_entry_trace_count = 0;
thread_local std::uint32_t loading_provider_manager_entry_trace_count = 0;

void trace_loading_provider_entry(PPCContext &context,
                                  const char *generated_name) noexcept {
  if (std::getenv("AC6_DEMO_WATCH_LOADING_PROVIDER") == nullptr ||
      generated_name == nullptr) {
    return;
  }
  const std::string_view name{generated_name};
  const bool task = name.ends_with("sub_8218CCD0");
  const bool manager = name.ends_with("sub_8219F5D0");
  const bool target_manager = manager && static_cast<std::uint32_t>(context.lr) ==
                             0x8218CD30U;
  if (!task && !target_manager) return;
  auto &entry_count = task ? loading_provider_task_entry_trace_count
                           : loading_provider_manager_entry_trace_count;
  if (entry_count >= 16U) return;

  auto &memory = require_bridge().memory();
  const auto object = context.r3.u32;
  const auto mapped = object != 0U && memory.mapped(object, manager ? 0x30U : 0x18U);
  const auto read_u8 = [&](std::uint32_t offset) {
    return mapped && memory.mapped(object + offset, 1U)
               ? static_cast<unsigned>(memory.load_u8(object + offset))
               : 0xFFFFFFFFU;
  };
  const auto read_u32 = [&](std::uint32_t offset) {
    return mapped && memory.mapped(object + offset, 4U)
               ? memory.load_u32(object + offset)
               : 0xFFFFFFFFU;
  };
  std::fprintf(
      stderr,
      "AC6_LOADING_PROVIDER_ENTRY tick=%llu thread=%u function=%s "
      "lr=0x%08X r3=0x%08X r4=0x%08X mapped=%u "
      "task9=%u task10=%u task12=0x%08X task17=%u "
      "manager_1c=0x%08X manager_20=0x%08X manager_24=0x%08X "
      "manager_28=0x%08X manager_2c=0x%08X\n",
      static_cast<unsigned long long>(require_bridge().tick()),
      current_guest_thread_id, generated_name,
      static_cast<std::uint32_t>(context.lr), context.r3.u32,
      context.r4.u32, mapped ? 1U : 0U, read_u8(9U), read_u8(10U),
      read_u32(12U), read_u8(17U), read_u32(28U), read_u32(32U),
      read_u32(36U), read_u32(40U), read_u32(44U));
  ++entry_count;
}

void trace_loading_resource_poll_entry(const char *generated_name) noexcept {
  if (!loading_resource_poll_trace.active || generated_name == nullptr) return;
  const std::string_view name{generated_name};
  loading_resource_poll_trace.saw_af20 |= name.ends_with("sub_8219AF20");
  loading_resource_poll_trace.saw_95b50 |= name.ends_with("sub_82195B50");
}

template <typename Invoke>
void trace_loading_resource_poll_call(PPCContext &context, std::uint32_t lr,
                                      std::uint32_t target, Invoke invoke) {
  const auto tick = require_bridge().tick();
  const bool enabled =
      std::getenv("AC6_DEMO_WATCH_LOADING_RESOURCE_POLL") != nullptr &&
      !loading_resource_poll_trace.active &&
      loading_resource_poll_trace_count < 16U && tick >= 3000U &&
      lr == 0x8219F64CU && target == 0x8219DF00U;
  if (!enabled) {
    invoke();
    return;
  }

  auto &memory = require_bridge().memory();
  auto &trace = loading_resource_poll_trace;
  trace = {};
  trace.active = true;
  trace.object = context.r3.u32;
  trace.mapped = trace.object != 0U && memory.mapped(trace.object, 0x28U);
  if (trace.mapped) {
    trace.state = memory.load_u32(trace.object + 0x0CU);
    trace.buffer = memory.load_u32(trace.object + 0x14U);
    trace.job = memory.load_u32(trace.object + 0x1CU);
    trace.resource = memory.load_u32(trace.object + 0x20U);
    trace.flag = memory.load_u8(trace.object + 0x24U);
  }
  ++loading_resource_poll_trace_count;
  try {
    invoke();
  } catch (...) {
    trace = {};
    throw;
  }

  const auto snapshot = trace;
  trace = {};
  std::fprintf(
      stderr,
      "AC6_LOADING_RESOURCE_POLL tick=%llu thread=%u object=0x%08X "
      "mapped=%u state=%u buffer=0x%08X job=0x%08X resource=0x%08X "
      "flag=%u saw_af20=%u saw_95b50=%u result=%d\n",
      static_cast<unsigned long long>(require_bridge().tick()),
      current_guest_thread_id, snapshot.object, snapshot.mapped ? 1U : 0U,
      snapshot.state, snapshot.buffer, snapshot.job, snapshot.resource,
      static_cast<unsigned>(snapshot.flag), snapshot.saw_af20 ? 1U : 0U,
      snapshot.saw_95b50 ? 1U : 0U, context.r3.s32);
}
}  // namespace
