#include "ac6demo/frontier_report.hpp"

#include "ac6demo/content.hpp"
#include "ac6demo/hash.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef AC6_DEMO_GENERATED_GUEST
#include "ac6_demo_build_identity.hpp"
#endif

namespace ac6demo {
namespace {

[[nodiscard]] std::string json_escape(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  constexpr char hex[] = "0123456789abcdef";
  for (const unsigned char character : value) {
    switch (character) {
    case '\\':
      result += "\\\\";
      break;
    case '"':
      result += "\\\"";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (character < 0x20U) {
        result += "\\u00";
        result += hex[(character >> 4U) & 0x0fU];
        result += hex[character & 0x0fU];
      } else {
        result.push_back(static_cast<char>(character));
      }
    }
  }
  return result;
}

[[nodiscard]] std::string_view backend_name(GraphicsBackend backend) noexcept {
  return backend == GraphicsBackend::Vulkan ? "vulkan" : "headless";
}

[[nodiscard]] std::string_view
frontier_kind(std::string_view diagnostic) noexcept {
  if (diagnostic.find("indirect") != std::string_view::npos) {
    return "guest-indirect";
  }
  if (diagnostic.find("import") != std::string_view::npos) {
    return "import";
  }
  if (diagnostic.find("MMIO") != std::string_view::npos ||
      diagnostic.find("Xenos ring") != std::string_view::npos) {
    return "graphics-mmio";
  }
  if (diagnostic.find("scheduler") != std::string_view::npos ||
      diagnostic.find("guest wait") != std::string_view::npos) {
    return "scheduler";
  }
  if (diagnostic.find("VFS") != std::string_view::npos ||
      diagnostic.find("file") != std::string_view::npos) {
    return "vfs";
  }
  return "runtime";
}

void write_registers(std::ostream &output,
                     const GuestRegisterSnapshot &registers) {
  output << "{\"r1\":" << registers.r1 << ",\"r3\":" << registers.r3
         << ",\"r4\":" << registers.r4 << ",\"r5\":" << registers.r5
         << ",\"r6\":" << registers.r6 << ",\"r7\":" << registers.r7
         << ",\"r8\":" << registers.r8 << ",\"r9\":" << registers.r9
         << ",\"r10\":" << registers.r10 << ",\"r11\":" << registers.r11
         << ",\"r12\":" << registers.r12 << ",\"r13\":" << registers.r13
         << ",\"r26\":" << registers.r26 << ",\"r27\":" << registers.r27
         << ",\"r28\":" << registers.r28 << ",\"r29\":" << registers.r29
         << ",\"r30\":" << registers.r30 << ",\"r31\":" << registers.r31 << '}';
}

void write_scheduler_details(std::ostream &output,
                             const GuestSchedulerSnapshot &scheduler) {
  output << ",\"event_publications\":[";
  for (std::uint32_t index = 0U; index < scheduler.event_publication_count;
       ++index) {
    if (index != 0U) {
      output << ',';
    }
    const auto &publication = scheduler.event_publications[index];
    output << "{\"key\":" << publication.key
           << ",\"thread\":" << publication.thread
           << ",\"lr\":" << publication.lr
           << ",\"kind\":" << static_cast<unsigned>(publication.kind) << '}';
  }
  output << "],\"waits\":[";
  for (std::uint32_t index = 0U; index < scheduler.wait_count; ++index) {
    if (index != 0U) {
      output << ',';
    }
    const auto &wait = scheduler.waits[index];
    output << "{\"id\":" << wait.id << ",\"startup\":" << wait.startup
           << ",\"entry\":" << wait.entry << ",\"parameter\":" << wait.parameter
           << ",\"callback\":" << wait.callback
           << ",\"callback_parameter\":" << wait.callback_parameter
           << ",\"last_indirect_target\":" << wait.last_indirect_target
           << ",\"last_indirect_lr\":" << wait.last_indirect_lr
           << ",\"indirect_call_count\":" << wait.indirect_call_count
           << ",\"last_indirect_tick\":" << wait.last_indirect_tick
           << ",\"wait_key\":" << wait.wait_key
           << ",\"wait_lr\":" << wait.wait_lr
           << ",\"wake_tick\":" << wait.wake_tick
           << ",\"wait_kind\":" << static_cast<unsigned>(wait.wait_kind)
           << ",\"blocked\":" << (wait.blocked ? "true" : "false")
           << ",\"suspended\":" << (wait.suspended ? "true" : "false")
           << ",\"finished\":" << (wait.finished ? "true" : "false") << '}';
  }
  output << ']';
}

[[nodiscard]] bool
milestone_reached(const FrontierReportInput &input) noexcept {
  switch (input.until) {
  case ProbeUntil::Frontend:
    return input.milestones.frontend;
  case ProbeUntil::Mission:
    return input.milestones.mission;
  case ProbeUntil::Terminal:
    return input.milestones.terminal;
  }
  return false;
}

[[nodiscard]] bool
milestone_reached(ProbeUntil until,
                  const ProbeMilestones &milestones) noexcept {
  switch (until) {
  case ProbeUntil::Frontend:
    return milestones.frontend;
  case ProbeUntil::Mission:
    return milestones.mission;
  case ProbeUntil::Terminal:
    return milestones.terminal;
  }
  return false;
}

[[nodiscard]] const GuestThreadWaitSnapshot *
select_runnable_thread(const GuestSchedulerSnapshot &scheduler) noexcept {
  const GuestThreadWaitSnapshot *best = nullptr;
  for (std::uint32_t index = 0U; index < scheduler.wait_count; ++index) {
    const auto &thread = scheduler.waits[index];
    if (thread.blocked || thread.suspended || thread.finished) {
      continue;
    }
    if (best == nullptr ||
        thread.indirect_call_count > best->indirect_call_count ||
        (thread.indirect_call_count == best->indirect_call_count &&
         thread.id < best->id)) {
      best = &thread;
    }
  }
  return best;
}

[[nodiscard]] const GuestThreadWaitSnapshot *
select_blocked_thread(const GuestSchedulerSnapshot &scheduler) noexcept {
  for (std::uint32_t index = 0U; index < scheduler.wait_count; ++index) {
    if (scheduler.waits[index].blocked) {
      return &scheduler.waits[index];
    }
  }
  return nullptr;
}

void write_xenos_opcode_counts(std::ostream &output,
                               const XenosPacketCensusSnapshot &census) {
  output << "], \"type3_opcode_counts\": [";
  bool first = true;
  for (std::size_t opcode = 0U; opcode < census.type3_opcode_counts.size();
       ++opcode) {
    const auto count = census.type3_opcode_counts[opcode];
    if (count == 0U) {
      continue;
    }
    if (!first) {
      output << ',';
    }
    first = false;
    output << "{\"opcode\": " << opcode << ", \"count\": " << count << '}';
  }
}

void write_xenos_typed_commands(std::ostream &output,
                                const XenosTypedCommandSnapshot &commands) {
  output << "{\"shader_load_count\": " << commands.shader_load_count
         << ", \"shader_sha256\": [";
  for (std::uint32_t index = 0U; index < commands.shader_load_count; ++index) {
    if (index != 0U) {
      output << ',';
    }
    output << '\"' << commands.shader_sha256[index] << '\"';
  }
  output << "], \"draw_count\": " << commands.draw_count << ", \"draws\": [";
  for (std::uint32_t index = 0U; index < commands.draw_count; ++index) {
    if (index != 0U) {
      output << ',';
    }
    const auto &draw = commands.draws[index];
    output << "{\"index_count\": " << draw.index_count
           << ", \"predicated\": " << (draw.predicated ? "true" : "false")
           << ", \"vertex_shader_sha256\": \"" << draw.vertex_shader_sha256
           << "\", \"pixel_shader_sha256\": \"" << draw.pixel_shader_sha256
           << "\"}";
  }
  output << "], \"present_count\": " << commands.present_count
         << ", \"present_resource_id\": \"" << commands.present_resource_id
         << "\", \"present_format\": "
         << static_cast<unsigned>(commands.present_format)
         << ", \"present_tiled\": "
         << (commands.present_tiled ? "true" : "false")
         << ", \"present_width\": " << commands.present_width
         << ", \"present_height\": " << commands.present_height
         << ", \"renderer_relevant_semantics_qualified\": "
         << (commands.renderer_relevant_semantics_qualified ? "true" : "false")
         << '}';
}

void write_xenos_effects(std::ostream &output,
                         const XenosEffectSnapshot &effects) {
  output << "{\"scratch_writeback\": " << effects.counts.scratch_writeback
         << ", \"register_rmw\": " << effects.counts.register_rmw
         << ", \"wait_reg_mem\": " << effects.counts.wait_reg_mem
         << ", \"conditional_write\": " << effects.counts.conditional_write
         << ", \"event_write\": " << effects.counts.event_write
         << ", \"interrupt\": " << effects.counts.interrupt
         << ", \"event_write_shader_done\": "
         << effects.counts.event_write_shader_done
         << ", \"invalidate_state\": " << effects.counts.invalidate_state
         << ", \"micro_engine_init\": " << effects.counts.micro_engine_init
         << ", \"memory_write_count\": " << effects.memory_write_count
         << ", \"cpu_interrupt_count\": " << effects.cpu_interrupt_count
         << ", \"last_interrupt_cpu\": "
         << static_cast<unsigned>(effects.last_interrupt_cpu)
         << ", \"pending_interrupt_count\": " << effects.pending_interrupt_count
         << ", \"pending_wait\": " << (effects.pending_wait ? "true" : "false")
         << ", \"pending_wait_memory\": "
         << (effects.pending_wait_memory ? "true" : "false")
         << ", \"pending_wait_address\": " << effects.pending_wait_address
         << ", \"pending_wait_observed\": " << effects.pending_wait_observed
         << ", \"pending_wait_reference\": " << effects.pending_wait_reference
         << ", \"pending_wait_mask\": " << effects.pending_wait_mask
         << ", \"pending_wait_interval\": " << effects.pending_wait_interval
         << ", \"gamma_lut_sha256\": \"" << effects.gamma_lut_sha256
         << "\", \"synchronous_effects_qualified\": "
         << (effects.synchronous_effects_qualified ? "true" : "false") << '}';
}

void write_render_queue(std::ostream &output,
                        const GuestSchedulerSnapshot &scheduler) {
  output << "{\"mapped\": "
         << (scheduler.render_queue_mapped ? "true" : "false")
         << ", \"base\": " << scheduler.render_queue_base
         << ", \"producer\": " << scheduler.render_queue_producer
         << ", \"consumer\": " << scheduler.render_queue_consumer
         << ", \"producer_changes\": "
         << scheduler.render_queue_producer_changes
         << ", \"consumer_changes\": "
         << scheduler.render_queue_consumer_changes
         << ", \"last_producer_thread\": "
         << scheduler.render_queue_last_producer_thread
         << ", \"last_consumer_thread\": "
         << scheduler.render_queue_last_consumer_thread
         << ", \"last_producer_tick\": "
         << scheduler.render_queue_last_producer_tick
         << ", \"last_consumer_tick\": "
         << scheduler.render_queue_last_consumer_tick
         << ", \"max_pending\": " << scheduler.render_queue_max_pending << '}';
}

} // namespace

bool parse_probe_until(std::string_view value, ProbeUntil *result) noexcept {
  if (result == nullptr) {
    return false;
  }
  if (value == "frontend") {
    *result = ProbeUntil::Frontend;
    return true;
  }
  if (value == "mission") {
    *result = ProbeUntil::Mission;
    return true;
  }
  if (value == "terminal") {
    *result = ProbeUntil::Terminal;
    return true;
  }
  return false;
}

std::string_view probe_until_name(ProbeUntil value) noexcept {
  switch (value) {
  case ProbeUntil::Frontend:
    return "frontend";
  case ProbeUntil::Mission:
    return "mission";
  case ProbeUntil::Terminal:
    return "terminal";
  }
  return "frontend";
}

std::optional<FrontierSnapshot>
derive_progress_frontier(ProbeUntil until, std::uint64_t completed_ticks,
                         const ProbeMilestones &milestones,
                         const GuestSchedulerSnapshot &scheduler) {
  if (milestone_reached(until, milestones)) {
    return std::nullopt;
  }

  if (scheduler.slice_exhaustions != 0U && scheduler.runnable != 0U) {
    const auto *thread = select_runnable_thread(scheduler);
    const bool current_location =
        thread != nullptr &&
        (thread->last_indirect_tick == completed_ticks ||
         (completed_ticks != 0U &&
          thread->last_indirect_tick == completed_ticks - 1U));
    return FrontierSnapshot{
        "scheduler activation budget exhausted before " +
            std::string(probe_until_name(until)) + " milestone",
        completed_ticks, thread == nullptr ? 0U : thread->id,
        current_location ? thread->last_indirect_lr : 0U,
        current_location ? thread->last_indirect_target : 0U};
  }

  if (scheduler.runnable == 0U && scheduler.blocked != 0U) {
    const auto *thread = select_blocked_thread(scheduler);
    return FrontierSnapshot{
        "all started guest threads blocked before " +
            std::string(probe_until_name(until)) + " milestone",
        completed_ticks, thread == nullptr ? 0U : thread->id,
        thread == nullptr ? 0U : thread->last_indirect_lr,
        thread == nullptr ? 0U : thread->last_indirect_target};
  }

  return FrontierSnapshot{"probe tick budget exhausted before " +
                              std::string(probe_until_name(until)) +
                              " milestone",
                          completed_ticks, 0U, 0U, 0U};
}

void write_frontier_report(const std::filesystem::path &path,
                           const FrontierReportInput &input) {
  if (path.empty() || input.trace_path.empty()) {
    throw std::invalid_argument("frontier report and trace paths are required");
  }
  std::error_code error;
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      throw std::runtime_error("cannot create frontier report directory: " +
                               error.message());
    }
  }
  const auto trace_sha256 = Sha256::file(input.trace_path);

  std::ostringstream output;
  output << "{\n  \"schema\": \"ac6-demo-frontier-report/v1\",\n"
         << "  \"target\": {\"id\": \"ac6-demo-xbox360-pal\", "
            "\"xex_sha256\": \""
         << kQualifiedXexSha256 << "\", \"module\": \"Default.xex\"},\n"
         << "  \"request\": {\"until\": \"" << probe_until_name(input.until)
         << "\", \"max_ticks\": " << input.max_ticks << ", \"backend\": \""
         << backend_name(input.backend) << "\"},\n"
         << "  \"outcome\": {\"kind\": \"" << json_escape(input.outcome)
         << "\", \"completed_ticks\": " << input.completed_ticks
         << ", \"session_state\": \"" << json_escape(input.session_state)
         << "\", \"milestone_reached\": "
         << (milestone_reached(input) ? "true" : "false") << "},\n"
         << "  \"identity\": {\"trace_sha256\": \"" << trace_sha256 << "\"";
#ifdef AC6_DEMO_GENERATED_GUEST
  output << ", \"codegen_manifest_sha256\": \"" << kCodegenManifestSha256
         << "\", \"ghidra_manifest_sha256\": \"" << kGhidraManifestSha256
         << "\", \"boundary_config_sha256\": \"" << kBoundaryConfigSha256
         << "\"";
#else
  output << ", \"codegen_manifest_sha256\": null, "
            "\"ghidra_manifest_sha256\": null, "
            "\"boundary_config_sha256\": null";
#endif
  output << "},\n  \"frontier\": ";
  if (!input.frontier.has_value()) {
    output << "null";
  } else {
    const auto &frontier = *input.frontier;
    output << "{\"kind\": \"" << frontier_kind(frontier.diagnostic)
           << "\", \"diagnostic\": \"" << json_escape(frontier.diagnostic)
           << "\", \"tick\": " << frontier.tick
           << ", \"thread\": " << frontier.thread << ", \"lr\": " << frontier.lr
           << ", \"address\": " << frontier.address << '}';
  }
  output << ",\n  \"milestones\": {\"presents\": " << input.milestones.presents
         << ", \"frontend\": " << (input.milestones.frontend ? "true" : "false")
         << ", \"mission\": " << (input.milestones.mission ? "true" : "false")
         << ", \"terminal\": " << (input.milestones.terminal ? "true" : "false")
         << "},\n  \"graphics\": {\"presentation_notifications\": "
         << input.milestones.presents << ", \"ring\": {\"initialized\": "
         << (input.graphics_ring.initialized ? "true" : "false")
         << ", \"base\": " << input.graphics_ring.base
         << ", \"size_log2\": " << input.graphics_ring.size_log2
         << ", \"capacity_dwords\": " << input.graphics_ring.capacity_dwords
         << ", \"read_pointer\": " << input.graphics_ring.read_pointer
         << ", \"write_pointer\": " << input.graphics_ring.write_pointer
         << ", \"owner_endpoint\": " << input.graphics_ring.owner_endpoint
         << ", \"submissions\": " << input.graphics_ring.submissions
         << ", \"pointer_mismatches\": "
         << input.graphics_ring.pointer_mismatches
         << ", \"submitted_dwords\": " << input.graphics_ring.submitted_dwords
         << ", \"max_submission_dwords\": "
         << input.graphics_ring.max_submission_dwords
         << ", \"recent_submissions\": [";
  for (std::uint32_t submission_index = 0U;
       submission_index < input.graphics_ring.recent_submission_count;
       ++submission_index) {
    const auto &submission =
        input.graphics_ring.recent_submissions[submission_index];
    if (submission_index != 0U) {
      output << ',';
    }
    output << "{\"start_pointer\": " << submission.start_pointer
           << ", \"end_pointer\": " << submission.end_pointer
           << ", \"dword_count\": " << submission.dword_count
           << ", \"captured_dword_count\": " << submission.captured_dword_count
           << ", \"truncated\": " << (submission.truncated ? "true" : "false")
           << ", \"dwords\": [";
    for (std::uint32_t word_index = 0U;
         word_index < submission.captured_dword_count; ++word_index) {
      if (word_index != 0U) {
        output << ',';
      }
      output << submission.dwords[word_index];
    }
    output << "]}";
  }
  output << "], \"indirect_buffers\": [";
  for (std::uint32_t buffer_index = 0U;
       buffer_index < input.graphics_ring.indirect_buffer_count;
       ++buffer_index) {
    const auto &buffer = input.graphics_ring.indirect_buffers[buffer_index];
    if (buffer_index != 0U) {
      output << ',';
    }
    output << "{\"address\": " << buffer.address
           << ", \"dword_count\": " << buffer.dword_count
           << ", \"captured_dword_count\": " << buffer.captured_dword_count
           << ", \"truncated\": " << (buffer.truncated ? "true" : "false")
           << ", \"byte_sha256\": \"" << buffer.byte_sha256.data()
           << "\", \"dwords\": [";
    for (std::uint32_t word_index = 0U;
         word_index < buffer.captured_dword_count; ++word_index) {
      if (word_index != 0U) {
        output << ',';
      }
      output << buffer.dwords[word_index];
    }
    output << "]}";
  }
  output << "], \"packet_census\": {\"packet_count\": "
         << input.graphics_ring.packet_census.packet_count
         << ", \"decoded_dword_count\": "
         << input.graphics_ring.packet_census.decoded_dword_count
         << ", \"type_counts\": [";
  for (std::size_t type = 0U;
       type < input.graphics_ring.packet_census.type_counts.size(); ++type) {
    if (type != 0U) {
      output << ',';
    }
    output << input.graphics_ring.packet_census.type_counts[type];
  }
  write_xenos_opcode_counts(output, input.graphics_ring.packet_census);
  output << "], \"reached_corpus_qualified\": "
         << (input.graphics_ring.packet_census.reached_corpus_qualified
                 ? "true"
                 : "false")
         << "}, \"typed_commands\": ";
  write_xenos_typed_commands(output, input.graphics_ring.typed_commands);
  output << ", \"effects\": ";
  write_xenos_effects(output, input.graphics_ring.effects);
  output << "}, \"vd_swap\": {\"calls\": " << input.graphics_swap.calls
         << ", \"tick\": " << input.graphics_swap.tick
         << ", \"output_buffer\": " << input.graphics_swap.output_buffer
         << ", \"fetch_address\": " << input.graphics_swap.fetch_address
         << ", \"swap_state\": " << input.graphics_swap.swap_state
         << ", \"system_header\": " << input.graphics_swap.system_header
         << ", \"fetch_words\": [";
  for (std::size_t index = 0U; index < input.graphics_swap.fetch_words.size();
       ++index) {
    if (index != 0U) {
      output << ',';
    }
    output << input.graphics_swap.fetch_words[index];
  }
  output << "], \"width\": " << input.graphics_swap.width
         << ", \"height\": " << input.graphics_swap.height
         << ", \"frontbuffer_address\": "
         << input.graphics_swap.frontbuffer_address
         << ", \"texture_format\": " << input.graphics_swap.texture_format
         << ", \"color_space\": " << input.graphics_swap.color_space
         << "}},\n  \"scheduler\": {\"threads\": "
         << input.scheduler.thread_count
         << ", \"runnable\": " << input.scheduler.runnable
         << ", \"blocked\": " << input.scheduler.blocked
         << ", \"finished\": " << input.scheduler.finished
         << ", \"slice_exhaustions\": " << input.scheduler.slice_exhaustions
         << ", \"last_slice_activations\": "
         << input.scheduler.last_slice_activations
         << ", \"render_queue\": ";
  write_render_queue(output, input.scheduler);
  write_scheduler_details(output, input.scheduler);
  output
      << "},\n"
      << "  \"control_flow\": {\"scope\": [\"guest-indirect\", \"imports\"], "
         "\"direct_calls\": \"qualified-static-ghidra-only\", \"edge_count\": "
      << input.control_flow.size() << ", \"edges\": [";
  for (std::size_t index = 0U; index < input.control_flow.size(); ++index) {
    const auto &edge = input.control_flow[index];
    if (index != 0U) {
      output << ',';
    }
    output << "\n    {\"kind\": \""
           << (edge.kind == GuestControlFlowKind::Import ? "import"
                                                         : "indirect")
           << "\", \"thread\": " << edge.thread << ", \"lr\": " << edge.lr
           << ", \"target\": " << edge.target << ", \"module\": \""
           << json_escape(edge.module) << "\", \"name\": \""
           << json_escape(edge.name) << "\", \"ordinal\": " << edge.ordinal
           << ", \"first_tick\": " << edge.first_tick
           << ", \"last_tick\": " << edge.last_tick
           << ", \"count\": " << edge.count << ", \"virtual_dispatch\": ";
    if (!edge.virtual_dispatch.has_value()) {
      output << "null";
    } else {
      const auto &dispatch = *edge.virtual_dispatch;
      output << "{\"object\":" << dispatch.object
             << ",\"vtable\":" << dispatch.vtable
             << ",\"slot\":" << dispatch.slot << '}';
    }
    output << ", \"last_registers\": ";
    write_registers(output, edge.registers);
    output << '}';
  }
  if (!input.control_flow.empty()) {
    output << '\n';
  }
  output << "  ]}\n}\n";

  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    throw std::runtime_error("cannot open frontier report: " + path.string());
  }
  stream << output.str();
  if (!stream) {
    throw std::runtime_error("cannot write frontier report: " + path.string());
  }
}

} // namespace ac6demo
