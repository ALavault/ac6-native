#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6/native_vulkan_backend.h"
#include "ac6/native_guest_memory.h"
#include "ac6/native_guest_vd.h"
#include "ac6/native_shader_translator.h"
#include "ac6/native_xenos.h"

#include <cassert>
#include <cstdint>
#include <array>
#include <vector>
#include <cstring>

namespace {

using ac6::native::DrawPacket;
using ac6::native::MmioBus;
using ac6::native::Pm4Decoder;
using ac6::native::PresentPacket;
using ac6::native::ResolvePacket;
using ac6::native::VdBridge;
using ac6::native::VulkanBackend;
using ac6::native::XenosCommand;
using ac6::native::XenosState;

void endian_modes_are_distinct_and_bounded() {
  const std::array<std::uint8_t, 4> bytes{1u, 2u, 3u, 4u};
  const auto view = std::span<const std::uint8_t>(bytes);
  assert(ac6::native::load_word(view, 0u, ac6::native::Endian::kNone) ==
         0x04030201u);
  assert(ac6::native::load_word(view, 0u, ac6::native::Endian::k8In16) ==
         0x03040102u);
  assert(ac6::native::load_word(view, 0u, ac6::native::Endian::k8In32) ==
         0x01020304u);
  assert(ac6::native::load_word(view, 0u, ac6::native::Endian::k16In32) ==
         0x02010403u);
  assert(ac6::native::load_word(view, 2u, ac6::native::Endian::kNone) == 0u);
}

void decoder_commits_only_complete_packet() {
  XenosState state;
  std::vector<XenosCommand> output;
  const std::vector<std::uint32_t> truncated{
      ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 4u), 0xA5A5u};
  const auto result = Pm4Decoder::decode_one(truncated, state, output);
  assert(!result.ok());
  assert(result.error.code == ac6::native::Pm4ErrorCode::kTruncatedPacket);
  assert(state.register_value(4u) == 0u);
  assert(output.empty());

  const std::vector<std::uint32_t> valid{
      ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 4u), 0xA5A5u,
      0x5A5Au};
  assert(Pm4Decoder::decode_one(valid, state, output).ok());
  assert(state.register_value(4u) == 0xA5A5u);
  assert(state.register_value(5u) == 0x5A5Au);
}

void decoder_rejects_unknown_and_bad_wait() {
  XenosState state;
  std::vector<XenosCommand> output;
  const std::vector<std::uint32_t> unknown{
      ac6::native::pm4::type3_header(0xEEu, 1u), 0u};
  auto result = Pm4Decoder::decode_one(unknown, state, output);
  assert(!result.ok());
  assert(result.error.code == ac6::native::Pm4ErrorCode::kUnsupportedOpcode);
  assert(output.empty());
  const std::vector<std::uint32_t> bad_wait{
      ac6::native::pm4::type3_header(ac6::native::pm4::kOpcodeWaitForIdle, 1u), 3u};
  result = Pm4Decoder::decode_one(bad_wait, state, output);
  assert(!result.ok());
  assert(result.error.code == ac6::native::Pm4ErrorCode::kInvalidWait);
}

void decoder_covers_type1_and_type2_without_silent_effects() {
  XenosState state;
  std::vector<XenosCommand> output;
  const std::vector<std::uint32_t> stream{
      ac6::native::pm4::type1_header(6u, 7u),
      0x11u, 0x22u, ac6::native::pm4::header(ac6::native::pm4::kType2, 1u)};
  const auto result = Pm4Decoder::decode_stream(stream, state, output);
  assert(result.ok());
  assert(state.register_value(6u) == 0x11u);
  assert(state.register_value(7u) == 0x22u);
  assert(output.empty());
}

void decoder_enforces_hardware_predicate_and_one_register() {
  XenosState state;
  std::vector<XenosCommand> output;
  const std::vector<std::uint32_t> one_register{
      ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 0x8000u | 9u),
      0x11u, 0x22u};
  assert(Pm4Decoder::decode_one(one_register, state, output).ok());
  assert(state.register_value(9u) == 0x22u);
  const std::vector<std::uint32_t> predicated{
      ac6::native::pm4::type3_header(ac6::native::pm4::kOpcodeWaitForIdle, 1u) | 1u,
      0u};
  const auto result = Pm4Decoder::decode_one(predicated, state, output);
  assert(!result.ok());
  assert(result.error.code == ac6::native::Pm4ErrorCode::kInvalidPayload);
}

void ring_wrap_and_interrupt_are_bounded() {
  std::uint32_t interrupt = 0u;
  MmioBus bus([&](std::uint32_t value) { interrupt = value; });
  assert(bus.write(MmioBus::kRingBase, 0x1000u));
  assert(bus.write(MmioBus::kRingSize, 256u));
  assert(!bus.write(MmioBus::kRingSize, 255u));
  assert(bus.write(MmioBus::kInterrupt, 7u));
  assert(interrupt == 7u);

  std::vector<std::uint32_t> ring(64u, 0u);
  // Packet starts at dword 63 and wraps to dwords 0..3.
  ring[63] = ac6::native::pm4::type3_header(
      ac6::native::pm4::kOpcodeXeSwap, 4u);
  ring[0] = ac6::native::pm4::kSwapSignature;
  ring[1] = 0x10000u;
  ring[2] = 1280u;
  ring[3] = 720u;
  assert(bus.write(MmioBus::kRingRead, 252u));
  assert(bus.write(MmioBus::kRingWrite, 16u));
  VdBridge bridge(bus);
  bridge.set_ring_words(ring);
  XenosState state;
  std::vector<XenosCommand> output;
  const auto result = bridge.pump(state, output);
  assert(result.ok());
  assert(result.consumed == 5u);
  assert(output.size() == 1u);
  assert(std::holds_alternative<PresentPacket>(output.front()));
  assert(bus.ring_read() == 16u);
}

void indirect_buffers_expand_and_cycles_fail_closed() {
  MmioBus bus;
  assert(bus.write(MmioBus::kRingSize, 256u));
  std::vector<std::uint32_t> ring(64u, 0u);
  ring[0] = ac6::native::pm4::type3_header(
      ac6::native::pm4::kOpcodeIndirectBuffer, 2u);
  ring[1] = 0x2000u;
  ring[2] = 5u;
  assert(bus.write(MmioBus::kRingRead, 0u));
  assert(bus.write(MmioBus::kRingWrite, 12u));
  const std::vector<std::uint32_t> guest{
      ac6::native::pm4::type3_header(ac6::native::pm4::kOpcodeXeSwap, 4u),
      ac6::native::pm4::kSwapSignature, 0x10000u, 1280u, 720u};
  VdBridge bridge(bus);
  bridge.set_ring_words(ring);
  bridge.set_guest_words(0x2000u, guest);
  XenosState state;
  std::vector<XenosCommand> output;
  assert(bridge.pump(state, output).ok());
  assert(output.size() == 1u);
  assert(std::holds_alternative<PresentPacket>(output.front()));

  ring[0] = ac6::native::pm4::type3_header(
      ac6::native::pm4::kOpcodeIndirectBuffer, 2u);
  ring[1] = 0x2000u;
  ring[2] = 3u;
  assert(bus.write(MmioBus::kRingRead, 0u));
  assert(bus.write(MmioBus::kRingWrite, 12u));
  bridge.set_ring_words(ring);
  bridge.set_guest_words(0x2000u, std::span<const std::uint32_t>(ring).subspan(0u, 3u));
  const auto cycle = bridge.pump(state, output);
  assert(!cycle.ok());
  assert(cycle.error.code == ac6::native::Pm4ErrorCode::kRingCorrupt);
  assert(bus.ring_read() == 0u);
}

void vulkan_boundary_fails_closed() {
  XenosState state;
  VulkanBackend backend;
  const std::vector<XenosCommand> unsupported{
      ac6::native::IndirectBufferPacket{0x2000u, 2u}};
  assert(!backend.submit(state, unsupported));
  assert(!backend.error().empty());
  assert(backend.present_count() == 0u);
  const std::vector<XenosCommand> valid{
      DrawPacket{1u, 3u, 3u, 0x1000u, 0x2000u, 16u, 1u, 1u},
      ResolvePacket{0u, 0u, 1280u, 720u, 1u, ac6::native::Endian::kNone, false},
      PresentPacket{0u, 1280u, 720u, 0u}};
  assert(backend.submit(state, valid));
  assert(backend.draw_count() == 1u);
  assert(backend.resolve_count() == 1u);
  assert(backend.present_count() == 1u);
  const std::vector<XenosCommand> malformed{
      ResolvePacket{0u, 0u, 1280u, 720u, 3u, ac6::native::Endian::kNone, false}};
  assert(!backend.submit(state, malformed));
  ac6::native::VulkanCapabilities no_textures;
  no_textures.texture_2d = false;
  VulkanBackend texture_backend(no_textures);
  DrawPacket texture_draw{1u, 3u, 3u, 0x1000u, 0x2000u, 16u, 1u, 1u};
  texture_draw.texture_dimension = 1u;
  const std::vector<XenosCommand> textured{texture_draw};
  assert(!texture_backend.submit(state, textured));
}

void shader_boundary_accepts_only_valid_spirv() {
  const std::vector<std::uint32_t> valid{
      0x07230203u, 0x00010500u, 0u, 8u, 0u};
  const auto translated = ac6::native::ShaderTranslator::translate(
      ac6::native::ShaderFormat::kSpirv, valid);
  assert(translated.ok());
  assert(translated.spirv == valid);
  const auto unknown = ac6::native::ShaderTranslator::translate(
      ac6::native::ShaderFormat::kXenosMicrocode, valid);
  assert(!unknown.ok());
  assert(unknown.spirv.empty());
  const std::vector<std::uint32_t> malformed{0x07230203u, 0x00020000u, 0u, 8u,
                                             0u};
  assert(!ac6::native::ShaderTranslator::translate(
                         ac6::native::ShaderFormat::kSpirv, malformed)
              .ok());
}

void guest_vd_service_drains_published_dword_index() {
  ac6::native::GuestAddressSpace guest;
  assert(guest.valid());
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  ac6::native::XenosState state;
  ac6::native::VulkanBackend backend;
  ac6::native::NativeGuestVdService service;
  service.bind(guest.base(), bus, bridge, state, backend);
  constexpr std::uint32_t ring = 0x10000u;
  service.register_allocation(guest.base(), ring, 256u);
  service.initialize_ring(guest.base(), ring, 5u);  // 1 << (5 + 3) bytes
  const std::array<std::uint32_t, 5> words{
      ac6::native::pm4::type3_header(ac6::native::pm4::kOpcodeXeSwap, 4u),
      ac6::native::pm4::kSwapSignature, 0x10000u, 1280u, 720u};
  for (std::size_t index = 0u; index < words.size(); ++index) {
    const std::uint32_t encoded = __builtin_bswap32(words[index]);
    std::memcpy(guest.base() + ring + index * 4u, &encoded, sizeof(encoded));
  }
  service.publish_write_address(guest.base(), ring + 20u);
  assert(backend.present_count() == 1u);
  assert(bus.ring_read() == 20u);
}

void indirect_buffer_decode_error_reports_real_hex_address() {
  // r94/r95: the IB/header values were previously formatted with
  // std::to_string() (decimal) under a literal "0x" prefix, so a genuine,
  // in-range guest address (0x125c0000) printed as "0x308019200" -- the
  // decimal digits of 308019200 -- appearing to exceed 32 bits when it did
  // not. Confirm the error now reports the address in real hex.
  MmioBus bus;
  assert(bus.write(MmioBus::kRingSize, 256u));
  std::vector<std::uint32_t> ring(64u, 0u);
  constexpr std::uint32_t ib_address = 0x125c0000u;
  ring[0] = ac6::native::pm4::type3_header(
      ac6::native::pm4::kOpcodeIndirectBuffer, 2u);
  ring[1] = ib_address;
  ring[2] = 1u;
  assert(bus.write(MmioBus::kRingRead, 0u));
  assert(bus.write(MmioBus::kRingWrite, 12u));
  // A TYPE0 packet whose base register (0x4800) exceeds
  // XenosState::kRegisterCount (0x4000): the same real, out-of-range
  // register write r94 found live, not a fabricated case.
  const std::vector<std::uint32_t> guest{
      ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x4800u), 0u};
  VdBridge bridge(bus);
  bridge.set_ring_words(ring);
  bridge.set_guest_words(ib_address, guest);
  XenosState state;
  std::vector<XenosCommand> output;
  const auto result = bridge.pump(state, output);
  assert(!result.ok());
  assert(result.error.code == ac6::native::Pm4ErrorCode::kInvalidRegister);
  assert(result.error.detail.find("0x125c0000") != std::string::npos);
  assert(result.error.detail.find("0x00004800") != std::string::npos);
  assert(result.error.detail.find("0x308019200") == std::string::npos);
}

void guest_vd_service_event_write_applies_single_endian_swap() {
  // r94: EVENT_WRITE_SHD delivery previously ran the value through
  // gpu_swap() (emulating the GPU's own byte-lane swap unit -- its result
  // is already the final guest-visible byte pattern) and then through
  // store_guest_word() (which applies its own bswap for a normal
  // host-native logical value), compounding two swaps into a corrupted
  // result. Verified against live retail traces before the fix: a fence
  // value of 5 with Endian::k8In32 was landing in guest memory as bytes
  // 05 00 00 00 (big-endian 0x05000000) instead of the correct 00 00 00 05.
  ac6::native::GuestAddressSpace guest;
  assert(guest.valid());
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  ac6::native::XenosState state;
  ac6::native::VulkanBackend backend;
  ac6::native::NativeGuestVdService service;
  service.bind(guest.base(), bus, bridge, state, backend);
  constexpr std::uint32_t ring = 0x10000u;
  constexpr std::uint32_t fence_target = 0x30000u;
  service.register_allocation(guest.base(), ring, 256u);
  service.register_allocation(guest.base(), fence_target, 16u);
  service.initialize_ring(guest.base(), ring, 5u);  // 1 << (5 + 3) bytes
  const std::array<std::uint32_t, 4> words{
      ac6::native::pm4::type3_header(ac6::native::pm4::kOpcodeEventWriteShd,
                                     3u),
      0u,                    // initiator: must satisfy (& ~0x8000003Fu) == 0
      fence_target | 0x2u,   // address | Endian::k8In32 in the low 2 bits
      5u};                   // value
  for (std::size_t index = 0u; index < words.size(); ++index) {
    const std::uint32_t encoded = __builtin_bswap32(words[index]);
    std::memcpy(guest.base() + ring + index * 4u, &encoded, sizeof(encoded));
  }
  service.publish_write_address(guest.base(), ring + words.size() * 4u);
  std::uint32_t stored = 0u;
  std::memcpy(&stored, guest.base() + fence_target, sizeof(stored));
  const std::uint32_t stored_be = __builtin_bswap32(stored);
  assert(stored_be == 5u);
}

}  // namespace

int main() {
  endian_modes_are_distinct_and_bounded();
  decoder_commits_only_complete_packet();
  decoder_rejects_unknown_and_bad_wait();
  decoder_covers_type1_and_type2_without_silent_effects();
  decoder_enforces_hardware_predicate_and_one_register();
  ring_wrap_and_interrupt_are_bounded();
  indirect_buffers_expand_and_cycles_fail_closed();
  indirect_buffer_decode_error_reports_real_hex_address();
  vulkan_boundary_fails_closed();
  shader_boundary_accepts_only_valid_spirv();
  guest_vd_service_drains_published_dword_index();
  guest_vd_service_event_write_applies_single_endian_swap();
  return 0;
}
