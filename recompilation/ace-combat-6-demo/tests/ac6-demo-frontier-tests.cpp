#include "ac6demo/frontier_report.hpp"
#include "ac6demo/guest_bridge.hpp"
#include "ac6demo/trace.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main() {
  ac6demo::GuestMemory memory;
  ac6demo::GuestBridge bridge(memory);
  bridge.set_tick(4U);
  bridge.record_indirect_edge(2U, 0x82100008U, 0x82102000U, {});
  bridge.record_indirect_edge(1U, 0x82100004U, 0x82101000U, {});
  const auto sorted_edges = bridge.control_flow_snapshot();
  assert(sorted_edges.size() == 2U);
  assert(sorted_edges[0].thread == 1U);
  assert(sorted_edges[1].thread == 2U);

  bridge.record_function_entry("__imp__sub_82170FCC");
  assert(bridge.function_reachability_snapshot().empty());
  bridge.enable_function_reachability(true);
  bridge.record_function_entry("__imp__sub_82170FCC");
  bridge.record_function_entry("not-a-generated-function");
  bridge.set_tick(5U);
  bridge.record_function_entry("__imp__sub_82170FCC");
  bridge.record_function_entry("__imp__sub_82185210");
  const auto reached = bridge.function_reachability_snapshot();
  assert(reached.size() == 2U);
  assert(reached[0] == (ac6demo::GuestFunctionReachability{
                           0x82170FCCU, 4U, 5U, 2U}));
  assert(reached[1] == (ac6demo::GuestFunctionReachability{
                           0x82185210U, 5U, 5U, 1U}));
  bridge.enable_function_reachability(false);
  assert(bridge.function_reachability_snapshot().empty());

  ac6demo::ProbeUntil until{};
  assert(ac6demo::parse_probe_until("frontend", &until));
  assert(until == ac6demo::ProbeUntil::Frontend);
  assert(ac6demo::parse_probe_until("mission", &until));
  assert(until == ac6demo::ProbeUntil::Mission);
  assert(ac6demo::parse_probe_until("terminal", &until));
  assert(until == ac6demo::ProbeUntil::Terminal);
  assert(!ac6demo::parse_probe_until("unknown", &until));
  assert(!ac6demo::parse_probe_until("frontend", nullptr));

  const auto root = std::filesystem::temp_directory_path();
  const auto trace = root / "ac6-demo-frontier-test.trace.jsonl";
  const auto report = root / "ac6-demo-frontier-test.report.json";
  {
    ac6demo::TraceWriter writer;
    writer.open(trace, ac6demo::GraphicsBackend::Headless);
    writer.append(0U, ac6demo::TraceDomain::Simulation,
                  "{\"state\":\"guest\"}");
    writer.close();
  }

  ac6demo::GuestControlFlowEdge edge;
  edge.kind = ac6demo::GuestControlFlowKind::Indirect;
  edge.thread = 7U;
  edge.lr = 0x82100004U;
  edge.target = 0x82101000U;
  edge.first_tick = 2U;
  edge.last_tick = 3U;
  edge.count = 4U;
  edge.virtual_dispatch =
      ac6demo::GuestVirtualDispatchSnapshot{0x10010000U, 0x8200BDD0U, 13U};
  edge.registers.r3 = 0x1234U;
  ac6demo::FrontierReportInput input;
  input.until = ac6demo::ProbeUntil::Frontend;
  input.max_ticks = 8U;
  input.trace_path = trace;
  input.outcome = "trap";
  input.session_state = "guest";
  input.completed_ticks = 3U;
  input.milestones.presents = 1U;
  input.graphics_ring.initialized = true;
  input.graphics_ring.base = 0x12340000U;
  input.graphics_ring.capacity_dwords = 1024U;
  input.graphics_ring.submissions = 2U;
  input.graphics_ring.pointer_mismatches = 1U;
  input.graphics_ring.submitted_dwords = 12U;
  input.graphics_ring.effects.counts.wait_reg_mem = 3U;
  input.graphics_ring.effects.memory_write_count = 2U;
  input.graphics_ring.effects.cpu_interrupt_count = 1U;
  input.graphics_ring.effects.last_interrupt_cpu = 2U;
  input.graphics_ring.effects.gamma_lut_sha256 = "gamma-digest";
  input.graphics_ring.effects.synchronous_effects_qualified = true;
  input.graphics_swap.calls = 3U;
  input.graphics_swap.fetch_words[0] = 0x12345678U;
  input.graphics_swap.width = 1280U;
  input.graphics_swap.height = 720U;
  input.scheduler.thread_count = 2U;
  input.scheduler.slice_exhaustions = 1U;
  input.control_flow.push_back(edge);
  input.frontier = ac6demo::FrontierSnapshot{"unqualified guest indirect call",
                                             3U, 7U, edge.lr, edge.target};
  ac6demo::write_frontier_report(report, input);

  std::ifstream stream(report, std::ios::binary);
  const std::string document{std::istreambuf_iterator<char>(stream),
                             std::istreambuf_iterator<char>()};
  assert(document.find("\"schema\": \"ac6-demo-frontier-report/v1\"") !=
         std::string::npos);
  assert(document.find("\"kind\": \"guest-indirect\"") != std::string::npos);
  assert(document.find("\"edge_count\": 1") != std::string::npos);
  assert(document.find("\"r3\":4660") != std::string::npos);
  assert(document.find(
             "\"virtual_dispatch\": "
             "{\"object\":268500992,\"vtable\":2181086672,\"slot\":13}") !=
         std::string::npos);
  assert(document.find("\"trace_sha256\":") != std::string::npos);
  assert(document.find("\"presentation_notifications\": 1") !=
         std::string::npos);
  assert(document.find("\"submitted_dwords\": 12") != std::string::npos);
  assert(document.find("\"pointer_mismatches\": 1") != std::string::npos);
  assert(document.find("\"wait_reg_mem\": 3") != std::string::npos);
  assert(document.find("\"memory_write_count\": 2") != std::string::npos);
  assert(document.find("\"last_interrupt_cpu\": 2") != std::string::npos);
  assert(document.find("\"gamma_lut_sha256\": \"gamma-digest\"") !=
         std::string::npos);
  assert(document.find("\"synchronous_effects_qualified\": true") !=
         std::string::npos);
  assert(document.find("\"vd_swap\": {\"calls\": 3") != std::string::npos);
  assert(document.find("\"fetch_words\": [305419896") != std::string::npos);

  input.until = ac6demo::ProbeUntil::Mission;
  input.outcome = "max_ticks";
  input.completed_ticks = 20U;
  input.milestones.frontend = true;
  input.scheduler.thread_count = 18U;
  input.scheduler.runnable = 1U;
  input.scheduler.blocked = 17U;
  input.scheduler.slice_exhaustions = 18U;
  input.scheduler.last_slice_activations = 256U;
  input.scheduler.wait_count = 1U;
  input.scheduler.waits[0].id = 1U;
  input.scheduler.waits[0].last_indirect_lr = 0x821A5DF8U;
  input.scheduler.waits[0].last_indirect_target = 0x82375D14U;
  input.scheduler.waits[0].indirect_call_count = 1890U;
  input.scheduler.waits[0].last_indirect_tick = 19U;
  input.frontier = ac6demo::derive_progress_frontier(
      input.until, input.completed_ticks, input.milestones, input.scheduler);
  assert(input.frontier.has_value());
  assert(input.frontier->thread == 1U);
  assert(input.frontier->lr == 0x821A5DF8U);
  assert(input.frontier->address == 0x82375D14U);
  ac6demo::write_frontier_report(report, input);
  std::ifstream liveness_stream(report, std::ios::binary);
  const std::string liveness_document{
      std::istreambuf_iterator<char>(liveness_stream),
      std::istreambuf_iterator<char>()};
  assert(liveness_document.find("\"kind\": \"scheduler\"") !=
         std::string::npos);
  assert(
      liveness_document.find(
          "scheduler activation budget exhausted before mission milestone") !=
      std::string::npos);
  assert(liveness_document.find("\"thread\": 1") != std::string::npos);

  input.completed_ticks = 320U;
  input.scheduler.waits[0].last_indirect_tick = 252U;
  input.frontier = ac6demo::derive_progress_frontier(
      input.until, input.completed_ticks, input.milestones, input.scheduler);
  assert(input.frontier.has_value());
  assert(input.frontier->thread == 1U);
  assert(input.frontier->lr == 0U);
  assert(input.frontier->address == 0U);

  std::error_code error;
  std::filesystem::remove(trace, error);
  std::filesystem::remove(report, error);
  return 0;
}
