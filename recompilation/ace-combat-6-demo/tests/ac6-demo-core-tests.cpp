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
#include <iostream>
#include <limits>

namespace {

// KeTimeStampBundle+16 is the guest's millisecond tick count, read 23,644
// times across a 12,000-tick run by sub_821A5040. The qualified profile is
// 60 Hz, the same one ppc.cpp uses for its 50 MHz timebase.
void test_ke_timestamp_bundle_tick_count() {
  namespace detail = ac6demo::guest_bridge_detail;
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
  bridge.set_xenos_ring_owner(0x30000U);
  bridge.enable_xenos_read_pointer_writeback(0x4003CU);
  memory.store_u32(0x30000U + 10908U, 4U);
  bridge.apply_xenos_mmio_write(0x7FC80714U, 4U);
  memory.store_u32(0x30000U + 10908U, 12U);
  bridge.apply_xenos_mmio_write(0x7FC80714U, 12U);
  bridge.apply_xenos_mmio_write(0x7FC80714U, 16U);
  const auto snapshot = bridge.xenos_ring_snapshot();
  assert(snapshot.initialized);
  assert(snapshot.base == 0x20000U);
  assert(snapshot.capacity_dwords == 1024U);
  assert(snapshot.read_pointer == 12U);
  assert(snapshot.write_pointer == 16U);
  assert(snapshot.owner_endpoint == 12U);
  assert(snapshot.submissions == 3U);
  assert(snapshot.pointer_mismatches == 1U);
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
  memory.store_u32(0x20000U + 14U * 4U, 0x80000001U);
  memory.store_u32(0x20000U + 15U * 4U, 0x80000002U);
  memory.store_u32(0x20000U, 0x80000003U);
  memory.store_u32(0x20004U, 0x80000004U);
  memory.store_u32(0x30000U + 10908U, 2U);
  bridge.apply_xenos_mmio_write(0x7FC80714U, 2U);
  const auto snapshot = bridge.xenos_ring_snapshot();
  assert(snapshot.read_pointer == 2U);
  assert(snapshot.write_pointer == 2U);
  assert(snapshot.recent_submission_count == 2U);
  const auto &submission = snapshot.recent_submissions[1];
  assert(submission.start_pointer == 14U);
  assert(submission.end_pointer == 2U);
  assert(submission.dword_count == 4U);
  assert(submission.captured_dword_count == 4U);
  assert(!submission.truncated);
  assert(submission.dwords[0] == 0x80000001U);
  assert(submission.dwords[1] == 0x80000002U);
  assert(submission.dwords[2] == 0x80000003U);
  assert(submission.dwords[3] == 0x80000004U);
  assert(snapshot.packet_census.type_counts[0] == 7U);
  assert(snapshot.packet_census.type_counts[2] == 4U);
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
}

} // namespace

int main() {
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
  }

  test_xenos_ring_snapshot();
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
#endif

  std::cout << "ac6-demo-core-tests: ok\n";
  return 0;
}
