#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6demo/endian.hpp"
#include "ac6demo/guest_bridge.hpp"
#include "ac6demo/guest_memory.hpp"
#include "ac6demo/hash.hpp"
#include "ac6demo/ppc.hpp"
#include "ac6demo/session.hpp"

#include "../src/guest_bridge/kernel_data_imports.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <unistd.h>

namespace ac6demo {
void trace_ib_capture(const GuestMemory &memory, std::uint32_t address,
                      std::uint32_t dword_count, std::uint64_t tick);
void trace_qualified_title_shader_capture(
    const GuestMemory &memory, std::uint32_t address,
    std::span<const std::uint32_t> dwords, std::uint64_t tick);
}

namespace {

void test_ib_payload_trace_window() {
  ac6demo::GuestMemory memory;
  memory.map_zero(0x20000U, 0x1000U);
  memory.store_u32(0x20100U, 0xC0002D00U);
  memory.store_u32(0x20104U, 0x00010081U);

  assert(::setenv("AC6_DEMO_WATCH_IB_WRITERS", "1", 1) == 0);
  assert(::setenv("AC6_DEMO_WATCH_ADDR_LO", "0x20100", 1) == 0);
  assert(::setenv("AC6_DEMO_WATCH_ADDR_HI", "0x20108", 1) == 0);
  std::FILE* captured = std::tmpfile();
  assert(captured != nullptr);
  const int saved_stderr = ::dup(fileno(stderr));
  assert(saved_stderr >= 0);
  assert(::dup2(fileno(captured), fileno(stderr)) >= 0);
  ac6demo::trace_ib_capture(memory, 0x20100U, 2U, 0U);
  ac6demo::trace_ib_capture(memory, 0x20200U, 2U, 0U);
  std::fflush(stderr);
  assert(::dup2(saved_stderr, fileno(stderr)) >= 0);
  assert(::close(saved_stderr) == 0);

  assert(std::fseek(captured, 0L, SEEK_END) == 0);
  const long size = std::ftell(captured);
  assert(size >= 0);
  assert(std::fseek(captured, 0L, SEEK_SET) == 0);
  std::string output(static_cast<std::size_t>(size), '\0');
  assert(std::fread(output.data(), 1U, output.size(), captured) == output.size());
  std::fclose(captured);
  assert(::unsetenv("AC6_DEMO_WATCH_IB_WRITERS") == 0);
  assert(::unsetenv("AC6_DEMO_WATCH_ADDR_LO") == 0);
  assert(::unsetenv("AC6_DEMO_WATCH_ADDR_HI") == 0);

  assert(output.find("AC6_IB_PAYLOAD address=0x00020100 dwords=2 "
                     "values=C0002D00,00010081") != std::string::npos);
  assert(output.find("AC6_IB_PAYLOAD address=0x00020200") == std::string::npos);
}

void test_qualified_title_shader_capture() {
  ac6demo::GuestMemory memory;
  memory.map_zero(0x155FA000U, 0x1000U);
  for (std::uint32_t index = 0U; index < 111U; ++index) {
    memory.store_u32(0x155FAA00U + index * 4U, 0xA5000000U + index);
  }
  std::vector<std::uint32_t> ib(2765U);
  const std::array pixel{0xC0012700U, 0x155FAB81U, 0x0000000FU};
  const std::array constants{0xC0022F00U, 0x155FAA00U, 0x000003F0U,
                             0x00000010U};
  const std::array vertex{0xC0012700U, 0x155FAA40U, 0x00000042U};
  std::copy(pixel.begin(), pixel.end(), ib.begin() + 10U);
  std::copy(constants.begin(), constants.end(), ib.begin() + 20U);
  std::copy(vertex.begin(), vertex.end(), ib.begin() + 30U);

  assert(::setenv("AC6_DEMO_WATCH_IB_WRITERS", "1", 1) == 0);
  std::FILE *captured = std::tmpfile();
  assert(captured != nullptr);
  const int saved_stderr = ::dup(fileno(stderr));
  assert(saved_stderr >= 0);
  assert(::dup2(fileno(captured), fileno(stderr)) >= 0);
  ac6demo::trace_qualified_title_shader_capture(memory, 0x129CA640U, ib, 230U);
  ac6demo::trace_qualified_title_shader_capture(memory, 0x129CA644U, ib, 230U);
  std::fflush(stderr);
  assert(::dup2(saved_stderr, fileno(stderr)) >= 0);
  assert(::close(saved_stderr) == 0);

  assert(std::fseek(captured, 0L, SEEK_END) == 0);
  const long size = std::ftell(captured);
  assert(size >= 0);
  assert(std::fseek(captured, 0L, SEEK_SET) == 0);
  std::string output(static_cast<std::size_t>(size), '\0');
  assert(std::fread(output.data(), 1U, output.size(), captured) == output.size());
  std::fclose(captured);
  assert(::unsetenv("AC6_DEMO_WATCH_IB_WRITERS") == 0);

  assert(output.find("tag=constants ib=0x129CA640 tick=230 "
                     "address=0x155FAA00 dwords=16") != std::string::npos);
  assert(output.find("tag=vs ib=0x129CA640 tick=230 "
                     "address=0x155FAA40 dwords=66") != std::string::npos);
  assert(output.find("tag=ps ib=0x129CA640 tick=230 "
                     "address=0x155FAB80 dwords=15") != std::string::npos);
  assert(output.find("ib=0x129CA644") == std::string::npos);
}

void test_qualified_title_vertex_snapshot_size() {
  assert(ac6demo::qualified_title_vertex_snapshot_size(0U, 4U) == 208U);
  assert(ac6demo::qualified_title_vertex_snapshot_size(4U, 4U) == 416U);
  assert(ac6demo::qualified_title_vertex_snapshot_size(8U, 4U) == 624U);
  assert(!ac6demo::qualified_title_vertex_snapshot_size(1U, 4U));
  assert(!ac6demo::qualified_title_vertex_snapshot_size(12U, 4U));
  assert(!ac6demo::qualified_title_vertex_snapshot_size(4U, 3U));
}

// KeTimeStampBundle+16 is the guest's millisecond tick count, read 23,644
// times across a 12,000-tick run by sub_821A5040. The qualified profile is
// 60 Hz, the same one ppc.cpp uses for its 50 MHz timebase.
void test_ke_timestamp_bundle_tick_count() {
  namespace detail = ac6demo::guest_bridge_detail;
  assert(detail::kVdHsioTrainingSucceededResult == 1);
  assert(detail::kKeTimeStampBundleSlot == 0x82000700U);
  // (library index 1 << 16) | ordinal 173, the unpatched XEX encoding.
  assert(detail::kKeTimeStampBundleUnpatched == ((1U << 16) | 173U));
  assert(detail::kKeTimeStampBundleTickCount == 16U);
  assert(detail::tick_count_milliseconds(0U) == 0U);
  assert(detail::tick_count_milliseconds(60U) == 1000U);
  assert(detail::tick_count_milliseconds(30U) == 500U);
  assert(detail::tick_count_milliseconds(11999U) == 199983U);
  // Monotonic across the profile, and not truncated to 32 bits early.
  assert(detail::tick_count_milliseconds(12000U) >
         detail::tick_count_milliseconds(11999U));
}

#ifdef AC6_DEMO_GENERATED_GUEST
void test_xma_kick_bit_wraps_at_context_32() {
  ac6demo::GuestMemory memory;
  ac6demo::GuestBridge bridge(memory);
  bridge.map_xma_kick_window();
  std::uint32_t context{};
  for (std::uint32_t index = 0U; index <= 32U; ++index) {
    context = bridge.allocate_xma_context();
    assert(context != 0U);
    if (index == 31U) {
      bridge.observe_xma_physical_context(context);
      memory.store_u32(0x7FEA1A80U, 0x00000080U);
    }
  }
  bridge.observe_xma_physical_context(context);
  memory.store_u32(0x7FEA1A80U, 0x01000000U);
}
#endif

#ifdef AC6_DEMO_GENERATED_GUEST
// XexCheckExecutablePrivilege(n) answers bit n of the title's own
// XEX_HEADER_SYSTEM_FLAGS. The demo's Default.xex declares 0x00000600, and the
// three privileges this image asks about are 10, 11 and 23.
void test_executable_privilege_reads_system_flags() {
  ac6demo::GuestMemory memory;
  memory.map_zero(0x20000U, 0x4000U);
  ac6demo::GuestBridge bridge(memory);
  // No image prepared yet: every privilege is refused rather than assumed.
  assert(!bridge.executable_privilege(10U));
  bridge.prepare(ac6demo::GuestBridge::ThreadImage{0x20000U, 0x100U, 0x100U,
                                                   0x1000U, 0x00000600U});
  assert(bridge.executable_privilege(9U));
  assert(bridge.executable_privilege(10U));
  assert(!bridge.executable_privilege(11U));
  assert(!bridge.executable_privilege(23U));
  assert(!bridge.executable_privilege(0U));
  // Out of range is a refusal, not an out-of-bounds shift.
  assert(!bridge.executable_privilege(32U));
  assert(!bridge.executable_privilege(0xFFFFFFFFU));
}

// Guarded because the non-generated build compiles a GuestBridge with no
// thread scheduler at all; this assertion runs for real in the codegen-on
// tree, whose ctest suite covers the same file.
// XDK: XSetThreadProcessor pins to one hardware thread and
// GetCurrentProcessorNumber reports 0..5, so a one-hot affinity mask names the
// processor the guest will read back from its PCR.
void test_guest_processor_identity_is_one_hot() {
  ac6demo::GuestMemory memory;
  memory.map_zero(0x20000U, 0x2000U);
  ac6demo::GuestBridge bridge(memory);
  std::uint32_t thread_id{};
  const auto created = bridge.create_guest_thread(0x1000U, 0x20000U, 0U,
                                                  0x82000000U, 0U, 0U,
                                                  0x20004U, &thread_id);
  assert(created);
  std::uint32_t object{};
  assert(bridge.reference_guest_thread(memory.load_u32(0x20004U), &object));
  assert(bridge.guest_thread_processor(object) == 0U);
  assert(bridge.pin_guest_thread_processor(object, 0x10U));
  assert(bridge.guest_thread_processor(object) == 4U);
  assert(bridge.pin_guest_thread_processor(object, 0x20U));
  assert(bridge.guest_thread_processor(object) == 5U);
  assert(bridge.pin_guest_thread_processor(object, 0x01U));
  assert(bridge.guest_thread_processor(object) == 0U);
  // Refused shapes leave the published identity alone rather than invent one.
  assert(bridge.pin_guest_thread_processor(object, 0x04U));
  assert(!bridge.pin_guest_thread_processor(object, 0U));
  assert(!bridge.pin_guest_thread_processor(object, 0x30U));
  assert(!bridge.pin_guest_thread_processor(object, 0x40U));
  assert(!bridge.pin_guest_thread_processor(object + 4U, 0x02U));
  assert(bridge.guest_thread_processor(object) == 2U);
}

#endif

void test_xenos_ring_snapshot() {
  ac6demo::GuestMemory memory;
  memory.map_zero(0x20000U, 0x1000U);
  memory.map_zero(0x30000U, 0x3000U);
  memory.map_zero(0x40000U, 0x1000U);
  ac6demo::GuestBridge bridge(memory);
  bridge.configure_xenos_ring(0x20000U, 9U);
  bridge.enable_xenos_read_pointer_writeback(0x4003CU);
  bridge.apply_xenos_mmio_write(0x7FC80714U, 4U);
  bridge.apply_xenos_mmio_write(0x7FC80714U, 12U);
  bridge.apply_xenos_mmio_write(0x7FC80714U, 16U);
  const auto snapshot = bridge.xenos_ring_snapshot();
  assert(snapshot.initialized);
  assert(snapshot.base == 0x20000U);
  assert(snapshot.capacity_dwords == 1024U);
  assert(snapshot.read_pointer == 16U);
  assert(snapshot.write_pointer == 16U);
  assert(snapshot.owner_endpoint == 0U);
  assert(snapshot.submissions == 3U);
  assert(snapshot.pointer_mismatches == 0U);
  assert(snapshot.submitted_dwords == 16U);
  assert(snapshot.max_submission_dwords == 8U);
  assert(snapshot.recent_submission_count == 3U);
  assert(snapshot.recent_submissions[2].start_pointer == 12U);
  assert(snapshot.recent_submissions[2].end_pointer == 16U);
  assert(snapshot.recent_submissions[2].captured_dword_count == 4U);
  assert(!snapshot.recent_submissions[2].truncated);
  assert(snapshot.packet_census.packet_count == 8U);
  assert(snapshot.packet_census.decoded_dword_count == 16U);
  assert(snapshot.packet_census.type_counts[0] == 8U);
  assert(snapshot.packet_census.reached_corpus_qualified);
  assert(memory.load_u32(0x40000U) == 0U);
  assert(memory.load_u32(0x4003CU) == 16U);
}

void test_xenos_ring_accepts_wptr_while_wait_pending() {
  ac6demo::GuestMemory memory;
  memory.map_zero(0x20000U, 0x1000U);
  memory.map_zero(0x40000U, 0x1000U);
  memory.map_zero(0x16AE2000U, 0x1000U);
  ac6demo::GuestBridge bridge(memory);
  bridge.configure_xenos_ring(0x20000U, 3U);
  bridge.enable_xenos_read_pointer_writeback(0x4003CU);

  memory.store_u32(0x20000U, 0xC0043C00U);
  memory.store_u32(0x20004U, 0x13U);
  memory.store_u32(0x20008U, 0x16AE2006U);
  memory.store_u32(0x2000CU, 1U);
  memory.store_u32(0x20010U, 0xFFFFFFFFU);
  memory.store_u32(0x20014U, 0x100U);
  memory.store_u32(0x20018U, 0x20U);
  memory.store_u32(0x2001CU, 0x12345678U);

  bridge.apply_xenos_mmio_write(0x7FC80714U, 6U);
  auto snapshot = bridge.xenos_ring_snapshot();
  assert(snapshot.read_pointer == 0U);
  assert(snapshot.write_pointer == 6U);
  assert(snapshot.submissions == 0U);
  assert(snapshot.effects.pending_wait);

  bridge.apply_xenos_mmio_write(0x7FC80714U, 8U);
  snapshot = bridge.xenos_ring_snapshot();
  assert(snapshot.read_pointer == 0U);
  assert(snapshot.write_pointer == 8U);
  assert(snapshot.submissions == 0U);
  assert(snapshot.recent_submission_count == 1U);
  assert(snapshot.effects.pending_wait);

  memory.store_u32(0x16AE2004U, 1U);
  bridge.apply_xenos_mmio_write(0x7FC80714U, 8U);
  snapshot = bridge.xenos_ring_snapshot();
  assert(snapshot.read_pointer == 8U);
  assert(snapshot.write_pointer == 8U);
  assert(snapshot.submissions == 2U);
  assert(snapshot.submitted_dwords == 8U);
  assert(snapshot.max_submission_dwords == 6U);
  assert(snapshot.recent_submission_count == 2U);
  assert(snapshot.recent_submissions[1].start_pointer == 6U);
  assert(snapshot.recent_submissions[1].end_pointer == 8U);
  assert(snapshot.packet_census.type_counts[0] == 1U);
  assert(!snapshot.effects.pending_wait);
  assert(memory.load_u32(0x4003CU) == 8U);
}

void test_xenos_indirect_history_rolls_between_submissions() {
  ac6demo::GuestMemory memory;
  memory.map_zero(0x20000U, 0x1000U);
  memory.map_zero(0x40000U, 0x1000U);
  memory.map_zero(0x50000U, 0x1000U);
  ac6demo::GuestBridge bridge(memory);
  bridge.configure_xenos_ring(0x20000U, 7U);
  bridge.enable_xenos_read_pointer_writeback(0x4003CU);

  std::string second_draw_vertex_shader;
  std::string shader_58_vertex;
  for (std::uint32_t index = 0U; index < 65U; ++index) {
    const auto indirect_address = 0x50000U + index * 48U;
    const auto ring_pointer = index * 3U;
    memory.store_u32(indirect_address, 0xC0022B00U);
    memory.store_u32(indirect_address + 4U, 0U);
    memory.store_u32(indirect_address + 8U, 1U);
    memory.store_u32(indirect_address + 12U, 0x01000000U + index);
    memory.store_u32(indirect_address + 16U, 0xC0022B00U);
    memory.store_u32(indirect_address + 20U, 1U);
    memory.store_u32(indirect_address + 24U, 1U);
    memory.store_u32(indirect_address + 28U, 0x05000000U + index);
    memory.store_u32(indirect_address + 32U, 0xC0003600U);
    memory.store_u32(indirect_address + 36U, 0x00010081U);
    memory.store_u32(0x20000U + ring_pointer * 4U, 0xC0013F00U);
    memory.store_u32(0x20000U + (ring_pointer + 1U) * 4U,
                     indirect_address);
    memory.store_u32(0x20000U + (ring_pointer + 2U) * 4U, 10U);
    bridge.apply_xenos_mmio_write(0x7FC80714U, ring_pointer + 3U);
    if (index == 1U) {
      const auto commands = bridge.xenos_ring_snapshot().typed_commands;
      second_draw_vertex_shader = commands.draws[1].vertex_shader_sha256;
    } else if (index == 57U) {
      const auto commands = bridge.xenos_ring_snapshot().typed_commands;
      shader_58_vertex = commands.shader_sha256[14];
    }
  }

  const auto snapshot = bridge.xenos_ring_snapshot();
  assert(snapshot.read_pointer == 195U);
  assert(snapshot.write_pointer == 195U);
  assert(snapshot.submissions == 65U);
  assert(snapshot.indirect_buffer_count == 16U);
  assert(snapshot.indirect_buffers.front().address == 0x50930U);
  assert(snapshot.indirect_buffers.back().address == 0x50C00U);
  assert(snapshot.typed_commands.shader_load_count == 16U);
  assert(snapshot.typed_commands.shader_sha256.front() == shader_58_vertex);
  assert(snapshot.typed_commands.draw_count == 64U);
  assert(snapshot.typed_commands.draws.front().vertex_shader_sha256 ==
         second_draw_vertex_shader);
  assert(snapshot.packet_census.type3_opcode_counts[0x3FU] == 65U);
  assert(snapshot.packet_census.type3_opcode_counts[0x2BU] == 130U);
  assert(snapshot.packet_census.type3_opcode_counts[0x36U] == 65U);
}

void test_xenos_ring_capture_wraps() {
  ac6demo::GuestMemory memory;
  memory.map_zero(0x20000U, 0x1000U);
  memory.map_zero(0x30000U, 0x3000U);
  memory.map_zero(0x40000U, 0x1000U);
  ac6demo::GuestBridge bridge(memory);
  bridge.configure_xenos_ring(0x20000U, 3U);
  bridge.set_xenos_ring_owner(0x30000U);
  bridge.enable_xenos_read_pointer_writeback(0x4003CU);
  memory.store_u32(0x30000U + 10908U, 14U);
  bridge.apply_xenos_mmio_write(0x7FC80714U, 14U);
  memory.store_u32(0x20000U + 14U * 4U, 0xC0006100U);
  memory.store_u32(0x20000U + 15U * 4U, 0x00000000U);
  memory.store_u32(0x20000U, 0xC0006200U);
  memory.store_u32(0x20004U, 0x00000001U);
  memory.store_u32(0x20008U, 0xC0006300U);
  memory.store_u32(0x2000CU, 0x00000000U);
  memory.store_u32(0x30000U + 10908U, 4U);
  bridge.apply_xenos_mmio_write(0x7FC80714U, 4U);
  const auto snapshot = bridge.xenos_ring_snapshot();
  assert(snapshot.read_pointer == 4U);
  assert(snapshot.write_pointer == 4U);
  assert(snapshot.recent_submission_count == 2U);
  const auto &submission = snapshot.recent_submissions[1];
  assert(submission.start_pointer == 14U);
  assert(submission.end_pointer == 4U);
  assert(submission.dword_count == 6U);
  assert(submission.captured_dword_count == 6U);
  assert(!submission.truncated);
  assert(submission.dwords[0] == 0xC0006100U);
  assert(submission.dwords[1] == 0x00000000U);
  assert(submission.dwords[2] == 0xC0006200U);
  assert(submission.dwords[3] == 0x00000001U);
  assert(submission.dwords[4] == 0xC0006300U);
  assert(submission.dwords[5] == 0x00000000U);
  assert(snapshot.packet_census.type_counts[0] == 7U);
  assert(snapshot.packet_census.type_counts[2] == 0U);
  assert(snapshot.packet_census.type3_opcode_counts[0x61U] == 1U);
  assert(snapshot.packet_census.type3_opcode_counts[0x62U] == 1U);
  assert(snapshot.packet_census.type3_opcode_counts[0x63U] == 1U);
}

void test_xenos_unknown_packet_keeps_rptr() {
  ac6demo::GuestMemory memory;
  memory.map_zero(0x20000U, 0x1000U);
  memory.map_zero(0x30000U, 0x3000U);
  memory.map_zero(0x40000U, 0x1000U);
  ac6demo::GuestBridge bridge(memory);
  bridge.configure_xenos_ring(0x20000U, 3U);
  bridge.set_xenos_ring_owner(0x30000U);
  bridge.enable_xenos_read_pointer_writeback(0x4003CU);
  memory.store_u32(0x20000U, 0xC0007F00U);
  memory.store_u32(0x20004U, 0U);
  memory.store_u32(0x30000U + 10908U, 2U);
  bool rejected = false;
  try {
    bridge.apply_xenos_mmio_write(0x7FC80714U, 2U);
  } catch (const ac6demo::RuntimeTrap &) {
    rejected = true;
  }
  assert(rejected);
  const auto snapshot = bridge.xenos_ring_snapshot();
  assert(snapshot.read_pointer == 0U);
  assert(snapshot.write_pointer == 0U);
  assert(memory.load_u32(0x40000U) == 0U);
  assert(memory.load_u32(0x4003CU) == 0U);
}

void test_xgi_user_context_request_guard() {
  ac6demo::GuestMemory memory;
  memory.map_zero(0x5000U, 0x1000U);
  memory.store_u32(0x5020U, 0x00008001U);
  assert(ac6demo::validate_xgi_user_context_request(
      memory, 0x821A55A0U, 0xFBU, 0x000B0006U, 0U, 0x5010U, 24U));
  assert(!ac6demo::validate_xgi_user_context_request(
      memory, 0x821A55A4U, 0xFBU, 0x000B0006U, 0U, 0x5010U, 24U));
  assert(!ac6demo::validate_xgi_user_context_request(
      memory, 0x821A55A0U, 0xFCU, 0x000B0006U, 0U, 0x5010U, 24U));
  assert(!ac6demo::validate_xgi_user_context_request(
      memory, 0x821A55A0U, 0xFBU, 0x000B0007U, 0U, 0x5010U, 24U));
  assert(!ac6demo::validate_xgi_user_context_request(
      memory, 0x821A55A0U, 0xFBU, 0x000B0006U, 1U, 0x5010U, 24U));
  assert(!ac6demo::validate_xgi_user_context_request(
      memory, 0x821A55A0U, 0xFBU, 0x000B0006U, 0U, 0x5010U, 20U));
  memory.store_u32(0x5020U, 0x00008002U);
  assert(!ac6demo::validate_xgi_user_context_request(
      memory, 0x821A55A0U, 0xFBU, 0x000B0006U, 0U, 0x5010U, 24U));
  memory.store_u32(0x5020U, 0x00008001U);
  assert(!ac6demo::validate_xgi_user_context_request(
      memory, 0x821A55A0U, 0xFBU, 0x000B0006U, 0U, 0x9000U, 24U));
}

void test_xma_context_release_guard() {
  ac6demo::GuestMemory memory;
  ac6demo::GuestBridge bridge(memory);
  const auto first = bridge.allocate_xma_context();
  const auto second = bridge.allocate_xma_context();
  const auto third = bridge.allocate_xma_context();
  assert(first == 0x10000000U);
  assert(second == first + 64U);
  assert(third == second + 64U);
  for (std::uint32_t offset = 0U; offset < 64U; offset += 4U) {
    memory.store_u32(first + offset, 0xA5A50000U + offset);
    memory.store_u32(second + offset, 0x5A5A0000U + offset);
  }
  assert(!bridge.release_xma_context(first + 4U));
  assert(!bridge.release_xma_context(first - 64U));
  assert(bridge.release_xma_context(first));
  for (std::uint32_t offset = 0U; offset < 64U; offset += 4U) {
    assert(memory.load_u32(first + offset) == 0U);
    assert(memory.load_u32(second + offset) == 0x5A5A0000U + offset);
  }
  assert(!bridge.release_xma_context(first));
  assert(bridge.release_xma_context(second));
  assert(!bridge.release_xma_context(third + 64U));
  assert(bridge.release_xma_context(third));
  const auto reused = bridge.allocate_xma_context();
  assert(reused == first || reused == second || reused == third);
  assert(bridge.release_xma_context(reused));
}

} // namespace

int main() {
#ifdef AC6_DEMO_GENERATED_GUEST
  test_xma_kick_bit_wraps_at_context_32();
#endif
  test_ib_payload_trace_window();
  test_qualified_title_shader_capture();
  test_qualified_title_vertex_snapshot_size();
  constexpr char abc[] = "abc";
  assert(ac6demo::Sha256::bytes(std::span<const std::byte>(
             reinterpret_cast<const std::byte *>(abc), 3U)) ==
         "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  std::array<std::byte, 8> bytes{};
  ac6demo::write_be64(bytes, 0U, 0x0123456789abcdefULL);
  assert(ac6demo::read_be64(bytes, 0U) == 0x0123456789abcdefULL);

  ac6demo::GuestMemory memory;
  memory.map_zero(0x1000U, 0x2000U);
  assert(memory.committed_page_count() == 2U);
  assert(memory.protection(0x1000U) == 4U);
  memory.set_protection(0x1000U, 0x1000U, 2U);
  assert(memory.protection(0x1000U) == 2U);
  assert(memory.load_u32(0x1ffcU) == 0U);
  bool protected_write = false;
  try {
    memory.store_u32(0x1ffcU, 1U);
  } catch (const ac6demo::RuntimeTrap &) {
    protected_write = true;
  }
  assert(protected_write);
  memory.set_protection(0x1000U, 0x1000U, 4U);
  memory.map_zero(0x1800U, 0x1000U);
  assert(memory.committed_page_count() == 2U);
  memory.store_u32(0x1ffcU, 0xfeedbeefU);
  assert(memory.load_u32(0x1ffcU) == 0xfeedbeefU);
  assert(memory.raw_base()[0x1ffcU] == 0xfeU);
  assert(memory.raw_base()[0x1fffU] == 0xefU);
  bool unmapped = false;
  try {
    (void)memory.load_u32(0x9000U);
  } catch (const ac6demo::RuntimeTrap &) {
    unmapped = true;
  }
  assert(unmapped);

  memory.map_zero(0x4000U, 0x1000U);
  constexpr char guest_text[] = "game:\\data";
  for (std::uint32_t index = 0U; index < sizeof(guest_text); ++index) {
    memory.store_u8(0x4100U + index,
                    static_cast<std::uint8_t>(guest_text[index]));
  }
  assert(ac6demo::initialize_guest_ansi_string(memory, 0x4000U, 0x4100U));
  assert(memory.load_u16(0x4000U) == sizeof(guest_text) - 1U);
  assert(memory.load_u16(0x4002U) == sizeof(guest_text));
  assert(memory.load_u32(0x4004U) == 0x4100U);
  assert(ac6demo::initialize_guest_ansi_string(memory, 0x4008U, 0U));
  assert(memory.load_u16(0x4008U) == 0U);
  assert(memory.load_u16(0x400AU) == 0U);
  assert(memory.load_u32(0x400CU) == 0U);
  assert(!ac6demo::initialize_guest_ansi_string(memory, 0x5000U, 0x4100U));
  assert(!ac6demo::initialize_guest_ansi_string(memory, 0x4010U, 0x5000U));
  assert(ac6demo::write_guest_file_network_open_information(memory, 0x4020U,
                                                            56U, 0x12345U));
  assert(memory.load_u64(0x4040U) == 0x13000U);
  assert(memory.load_u64(0x4048U) == 0x12345U);
  assert(memory.load_u32(0x4050U) == 0x80U);
  assert(!ac6demo::write_guest_file_network_open_information(memory, 0x4020U,
                                                             55U, 1U));
  assert(!ac6demo::write_guest_file_network_open_information(memory, 0x5000U,
                                                             56U, 1U));
  memory.store_u32(0x4060U, 0xFFFFFFFDU);
  memory.store_u32(0x4064U, 0x4000U);
  memory.store_u32(0x4068U, 0x40U);
  std::string object_path;
  assert(ac6demo::read_guest_object_attributes_path(memory, 0x4060U,
                                                    &object_path));
  assert(object_path == "game:\\data");
  memory.store_u32(0x4068U, 0x41U);
  assert(!ac6demo::read_guest_object_attributes_path(memory, 0x4060U,
                                                     &object_path));

  test_xgi_user_context_request_guard();
  test_xma_context_release_guard();

  {
    ac6demo::GuestBridge bridge(memory);
    assert(!bridge.notify_ui_position().has_value());
    assert(!bridge.set_notify_ui_position(5U));
    assert(!bridge.notify_ui_position().has_value());
    assert(bridge.set_notify_ui_position(6U));
    assert(bridge.notify_ui_position() == 6U);
    assert(bridge.xam_user_signin_state(0U) == 1U);
    assert(bridge.xam_user_signin_state(1U) == 0U);
    assert(bridge.xam_user_signin_state(2U) == 0U);
    assert(bridge.xam_user_signin_state(3U) == 0U);
    assert(!bridge.xam_user_signin_state(4U).has_value());
    for (std::uint32_t index = 0U; index < 16U; ++index) {
      memory.store_u8(0x4200U + index, 0xA5U);
    }
    assert(!bridge.write_xam_user_name(1U, 0x4200U, 16U));
    assert(!bridge.write_xam_user_name(0U, 0x4200U, 15U));
    assert(bridge.write_xam_user_name(0U, 0x4200U, 16U));
    assert(memory.load_u8(0x4200U) == 'U');
    assert(memory.load_u8(0x4203U) == 'r');
    assert(memory.load_u8(0x4204U) == 0U);
    assert(memory.load_u8(0x4205U) == 0xA5U);
    std::uint32_t audio_handle{};
    assert(bridge.register_xaudio_client(0x82001000U, 0x1234U, &audio_handle));
    assert(audio_handle == 0xE4000000U);
    assert(!bridge.register_xaudio_client(0x82002000U, 0U, &audio_handle));
    assert(!bridge.unregister_xaudio_client(0xE4000004U));
    assert(bridge.unregister_xaudio_client(audio_handle));
  }

  {
    ac6demo::GuestMemory allocation_memory;
    ac6demo::GuestBridge bridge(allocation_memory);
    bridge.record_allocation(0x20000U, 0x2000U);
    bridge.record_allocation(0x22000U, 0x3000U);
    assert(bridge.owns_allocation(0x20000U, 0x5000U));
    assert(!bridge.owns_allocation(0x1f000U, 0x6000U));
    bridge.record_allocation(0x25000U, 0x1000U);
    assert(bridge.owns_allocation(0x20000U, 0x6000U));

    allocation_memory.map_zero(0xADF22000U, 0x4000U);
    bridge.record_allocation(0xADF22000U, 0x4000U);
    assert(bridge.resolve_physical_alias(0x0DF22000U, 0x4000U) ==
           0xADF22000U);
    assert(!bridge.resolve_physical_alias(0x0DF22000U, 0x5000U).has_value());
    allocation_memory.map_zero(0xEDF22000U, 0x4000U);
    bridge.record_allocation(0xEDF22000U, 0x4000U);
    assert(!bridge.resolve_physical_alias(0x0DF22000U, 0x4000U).has_value());

    ac6demo::GuestMemory crossing_memory;
    crossing_memory.map_zero(0x17327000U, 0x174D8000U);
    ac6demo::GuestBridge crossing_bridge(crossing_memory);
    crossing_bridge.record_allocation(0x17327000U, 0x174D8000U);
    assert(crossing_bridge.resolve_physical_alias(0x0DF22000U, 0x4000U) ==
           0x2DF22000U);
  }

  test_xenos_ring_snapshot();
  test_xenos_ring_accepts_wptr_while_wait_pending();
  test_xenos_indirect_history_rolls_between_submissions();
  test_xenos_ring_capture_wraps();
  test_xenos_unknown_packet_keeps_rptr();

  ac6demo::GuestMemory mmio_memory;
  bool wrote_mmio = false;
  mmio_memory.map_mmio(
      0xf0000000U, 4U,
      [](std::uint32_t, std::size_t) { return std::uint64_t{0x12345678U}; },
      [&wrote_mmio](std::uint32_t, std::uint64_t value, std::size_t length) {
        wrote_mmio = value == 0xabcdef01U && length == 4U;
      });
  assert(mmio_memory.load_u32(0xf0000000U) == 0x12345678U);
  mmio_memory.store_u32(0xf0000000U, 0xabcdef01U);
  assert(wrote_mmio);
  bool overlapping_mmio = false;
  try {
    mmio_memory.map_zero(0xf0000000U, 0x1000U);
    mmio_memory.map_mmio(0xf0000000U, 4U, {}, {});
  } catch (const std::invalid_argument &) {
    overlapping_mmio = true;
  }
  assert(overlapping_mmio);

  ac6demo::PpcVector a;
  ac6demo::PpcVector b;
  a.set_s32(0U, -100000);
  a.set_s32(1U, 4);
  a.set_s32(2U, 100000);
  a.set_s32(3U, 7);
  b.set_s32(0U, -32769);
  b.set_s32(1U, 32768);
  b.set_s32(2U, -1);
  b.set_s32(3U, 1);
  const auto packed = ac6demo::vpkswss(a, b);
  assert(static_cast<std::int16_t>(packed.u32(0U) >> 16U) == -32768);
  assert(static_cast<std::int16_t>(packed.u32(1U) >> 16U) == 4);
  assert(static_cast<std::int16_t>(packed.u32(2U) >> 16U) == 32767);
  assert(static_cast<std::int16_t>(packed.u32(3U) >> 16U) == 7);
  assert(static_cast<std::int16_t>(packed.u32(0U) & 0xffffU) == -32768);
  assert(static_cast<std::int16_t>(packed.u32(1U) & 0xffffU) == 32767);
  assert(static_cast<std::int16_t>(packed.u32(2U) & 0xffffU) == -1);
  assert(static_cast<std::int16_t>(packed.u32(3U) & 0xffffU) == 1);

  ac6demo::PpcVector bounds;
  bounds.set_f32(0U, 2.0F);
  bounds.set_f32(1U, 2.0F);
  bounds.set_f32(2U, 2.0F);
  bounds.set_f32(3U, 2.0F);
  ac6demo::PpcVector values;
  values.set_f32(0U, -3.0F);
  values.set_f32(1U, 0.0F);
  values.set_f32(2U, 3.0F);
  values.set_f32(3U, 1.0F);
  const auto compared = ac6demo::vcmpbfp(values, bounds);
  assert(compared.value.u32(0U) == 0x80000000U);
  assert(compared.value.u32(2U) == 0x40000000U);
  assert(!compared.cr6.lt && !compared.cr6.gt && !compared.cr6.eq);
  values.set_f32(0U, -2.0F);
  values.set_f32(2U, 2.0F);
  values.set_f32(3U, 2.0F);
  assert(ac6demo::vcmpbfp(values, bounds).cr6.eq);
  values.set_f32(3U, std::numeric_limits<float>::quiet_NaN());
  const auto unordered = ac6demo::vcmpbfp(values, bounds);
  assert(unordered.value.u32(3U) == 0xC0000000U);
  assert(!unordered.cr6.eq);

  ac6demo::PpcRuntimeHooks hooks(memory);
  ac6demo::PpcContext context;
  const auto loaded = hooks.lwarx(context, 0x1000U);
  assert(loaded == 0U);
  assert(hooks.stwcx(context, 0x1000U, 0x12345678U));
  assert(memory.load_u32(0x1000U) == 0x12345678U);
  assert(!hooks.stwcx(context, 0x1000U, 0U));
  (void)hooks.lwarx(context, 0x1000U);
  memory.store_u32(0x1000U, 0xabcdef01U);
  assert(!hooks.stwcx(context, 0x1000U, 0U));
  hooks.set_tick(60U);
  assert(hooks.read_timebase(context) == 50'000'000ULL);
  assert(std::isfinite(ac6demo::xenon_reciprocal_estimate(3.0F)));
  assert(std::isfinite(ac6demo::xenon_rsqrt_estimate(3.0F)));

  test_ke_timestamp_bundle_tick_count();
#ifdef AC6_DEMO_GENERATED_GUEST
  test_guest_processor_identity_is_one_hot();
  test_executable_privilege_reads_system_flags();
#endif

  std::cout << "ac6-demo-core-tests: ok\n";
  return 0;
}
