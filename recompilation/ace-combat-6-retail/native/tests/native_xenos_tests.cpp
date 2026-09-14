#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6/native_vulkan_backend.h"
#include "ac6/native_vulkan_device.h"
#include "ac6/native_guest_memory.h"
#include "ac6/native_guest_vd.h"
#include "ac6/native_pinned_shaders.h"
#include "ac6/native_shader_translator.h"
#include "ac6/native_xenos.h"

#include <cassert>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <array>
#include <map>
#include <tuple>
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

void decoder_accepts_verified_retail_opcodes_0x45_and_0x46() {
  // r98: both opcodes verified against real retail construction code
  // (sub_821EB8B8/sub_821EBB40) -- see kOpcodeSetGammaOrDitherTable's and
  // kOpcodeInvalidateStateExtended's definitions for what was confirmed.
  // Reproduces the exact real packets captured from guest 0x125c0000.
  XenosState state;
  std::vector<XenosCommand> output;
  const std::vector<std::uint32_t> table_entry{
      ac6::native::pm4::header(ac6::native::pm4::kType3, 6u,
                               ac6::native::pm4::kOpcodeSetGammaOrDitherTable
                                   << 8u),
      0x7u, 0x1925u, 0x30088u, 0xffffffffu, 0x1922u, 0x1u};
  auto result = Pm4Decoder::decode_stream(table_entry, state, output);
  assert(result.ok());
  assert(output.empty());

  const std::vector<std::uint32_t> wrong_count{
      ac6::native::pm4::header(ac6::native::pm4::kType3, 1u,
                               ac6::native::pm4::kOpcodeSetGammaOrDitherTable
                                   << 8u),
      0u};
  assert(!Pm4Decoder::decode_stream(wrong_count, state, output).ok());

  const std::vector<std::uint32_t> extended_invalidate{
      ac6::native::pm4::header(
          ac6::native::pm4::kType3, 1u,
          ac6::native::pm4::kOpcodeInvalidateStateExtended << 8u),
      0xfu};
  result = Pm4Decoder::decode_stream(extended_invalidate, state, output);
  assert(result.ok());
  assert(output.empty());
}

void decoder_enforces_hardware_one_register() {
  XenosState state;
  std::vector<XenosCommand> output;
  const std::vector<std::uint32_t> one_register{
      ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 0x8000u | 9u),
      0x11u, 0x22u};
  assert(Pm4Decoder::decode_one(one_register, state, output).ok());
  assert(state.register_value(9u) == 0x22u);
}

// r100: bit 0 (predicate-enable) no longer rejects decode -- see the policy
// comment above native_xenos.cpp's TYPE3 switch for the evidence chain
// (r96/r99). A predicated packet must decode exactly like its unpredicated
// twin, opcode and payload untouched by the bit.
void decoder_decodes_predicated_type3_like_unpredicated() {
  XenosState state;
  std::vector<XenosCommand> unpredicated_output;
  const std::vector<std::uint32_t> unpredicated{
      ac6::native::pm4::type3_header(ac6::native::pm4::kOpcodeWaitForIdle, 1u),
      0u};
  const auto unpredicated_result =
      Pm4Decoder::decode_one(unpredicated, state, unpredicated_output);
  assert(unpredicated_result.ok());

  std::vector<XenosCommand> predicated_output;
  const std::vector<std::uint32_t> predicated{
      ac6::native::pm4::type3_header(ac6::native::pm4::kOpcodeWaitForIdle, 1u) | 1u,
      0u};
  const auto predicated_result =
      Pm4Decoder::decode_one(predicated, state, predicated_output);
  assert(predicated_result.ok());
  assert(predicated_output.size() == unpredicated_output.size());
}

// The two real captured predicated headers from r99's construction-site
// trace (DRAW_INDX_2 `0xC0003601`, WAIT_REG_MEM `0xC0043C01`) must decode
// successfully rather than reject.
void decoder_accepts_real_captured_predicated_headers() {
  XenosState state;
  std::vector<XenosCommand> output;
  const std::vector<std::uint32_t> draw_indx_2{0xC0003601u, 0x00030088u};
  assert(Pm4Decoder::decode_one(draw_indx_2, state, output).ok());

  // count field of 0xC0043C01 is 5: function/poll-addr/ref/mask/interval.
  const std::vector<std::uint32_t> wait_reg_mem{
      0xC0043C01u, 0x00000013u, 0x16530002u, 0u, 0xffffffffu, 0u};
  assert(Pm4Decoder::decode_one(wait_reg_mem, state, output).ok());
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

void draw_state_follows_nested_pm4_order() {
  using namespace ac6::native;
  const auto write = pm4::header(pm4::kType0, 1u, 0x2200u);
  const auto load = pm4::type3_header(pm4::kOpcodeImLoadImmediate, 3u);
  const auto draw = pm4::type3_header(pm4::kOpcodeDrawIndx2, 1u);
  const auto ib = pm4::type3_header(pm4::kOpcodeIndirectBuffer, 2u);
  const auto triangle = 4u | (2u << 6u) | (3u << 16u);
  // The nested draw inherits B, writes C, and returns to a parent draw
  // before the parent's final D write. Flattening must preserve this order.
  const std::vector<std::uint32_t> inner{
      draw, triangle, write, 3u, load, 0u, 1u, 0xC0u, draw, triangle};
  const std::vector<std::uint32_t> outer{
      write, 2u, load, 0u, 1u, 0xB0u,
      ib, 0x2100u, static_cast<std::uint32_t>(inner.size()), draw, triangle};
  std::vector<std::uint32_t> ring{
      write, 1u, load, 0u, 1u, 0xA0u, draw, triangle,
      ib, 0x2000u, static_cast<std::uint32_t>(outer.size()), draw, triangle,
      write, 4u, load, 0u, 1u, 0xD0u, draw, triangle};
  const auto written = static_cast<std::uint32_t>(ring.size() * 4u);
  ring.resize(64u);
  std::vector<std::uint32_t> guest(64u + inner.size());
  std::copy(outer.begin(), outer.end(), guest.begin());
  std::copy(inner.begin(), inner.end(), guest.begin() + 64u);
  MmioBus bus;
  assert(bus.write(MmioBus::kRingSize, 256u));
  assert(bus.write(MmioBus::kRingRead, 0u));
  assert(bus.write(MmioBus::kRingWrite, written));
  VdBridge bridge(bus);
  bridge.set_ring_words(ring);
  bridge.set_guest_words(0x2000u, guest);
  XenosState state;
  std::vector<XenosCommand> commands;
  assert(bridge.pump(state, commands).ok());
  assert(bus.ring_read() == written);
  assert(state.register_value(0x2200u) == 4u);
  assert(state.active_shader(0u)[0] == 0xD0u);
  // A later submission cannot alter the retained draws either.
  assert(state.set_register(0x2200u, 99u));
  const std::uint32_t expected[] = {1u, 2u, 3u, 3u, 3u, 4u};
  std::size_t index = 0u;
  for (const auto& command : commands) {
    if (const auto* packet = std::get_if<DrawPacket>(&command)) {
      assert(index < std::size(expected));
      assert(packet->state_snapshot);
      const auto& snapshot = *packet->state_snapshot;
      assert(snapshot.register_value(0x2200u) == expected[index]);
      assert(snapshot.active_shader(0u)[0] == 0x90u + expected[index] * 0x10u);
      assert(snapshot.generation() == packet->state_generation);
      ++index;
    }
  }
  assert(index == std::size(expected));
  // Failure in a nested buffer leaves the published ring, caller state
  // and output unchanged, including draws staged before the failing IB.
  guest[64u] = pm4::type3_header(0x7Fu, 1u);
  bridge.set_guest_words(0x2000u, guest);
  assert(bus.write(MmioBus::kRingRead, 0u));
  const auto count = commands.size();
  assert(!bridge.pump(state, commands).ok());
  assert(bus.ring_read() == 0u);
  assert(state.register_value(0x2200u) == 99u);
  assert(commands.size() == count);
}

// r517 prefix-commit: a repeated IDENTICAL failure with a sane complete
// prefix commits the prefix (bus cursor, state, output) while still
// reporting the error. First failure drops everything (transients heal
// with zero behavior change); only the confirmed repeat advances, and
// only through fully-decoded top-level packets.
void failed_decode_commits_sane_prefix_on_identical_repeat() {
  using namespace ac6::native;
  const auto draw = pm4::type3_header(pm4::kOpcodeDrawIndx2, 1u);
  const auto triangle = 4u | (2u << 6u) | (3u << 16u);
  const auto ib = pm4::type3_header(pm4::kOpcodeIndirectBuffer, 2u);
  const auto bad = pm4::type3_header(0x7Fu, 1u);
  // Prefix: 1 draw + 4 register writes of 8 dwords each = 5 packets,
  // 34 dwords (above the 4-packet / 32-word sanity gates).
  std::vector<std::uint32_t> ring{draw, triangle};
  for (std::uint32_t reg = 0u; reg < 4u; ++reg) {
    ring.push_back(pm4::header(pm4::kType0, 7u, 0x2200u + reg * 0x10u));
    for (std::uint32_t value = 0u; value < 7u; ++value) {
      ring.push_back(0x100u + reg * 0x10u + value);
    }
  }
  assert(ring.size() == 34u);
  ring.push_back(ib);
  ring.push_back(0x3000u);
  ring.push_back(4u);
  const auto written = static_cast<std::uint32_t>(ring.size() * 4u);
  ring.resize(64u);
  // Nested body: one good register write, then an unsupported opcode.
  // The nested write must roll back with the failing IB (its cursor
  // cannot advance mid-packet).
  const std::vector<std::uint32_t> guest{
      pm4::header(pm4::kType0, 1u, 0x2300u), 0xDEADu, bad, 0u};
  MmioBus bus;
  assert(bus.write(MmioBus::kRingSize, 256u));
  assert(bus.write(MmioBus::kRingRead, 0u));
  assert(bus.write(MmioBus::kRingWrite, written));
  VdBridge bridge(bus);
  bridge.set_ring_words(ring);
  bridge.set_guest_words(0x3000u, guest);
  XenosState state;
  assert(state.set_register(0x2300u, 0x5A5Au));
  std::vector<XenosCommand> output;
  // First identical failure: exact rollback, as before r517.
  auto first = bridge.pump(state, output);
  assert(!first.ok());
  assert(first.consumed == 0u);
  assert(bus.ring_read() == 0u);
  assert(output.empty());
  assert(state.register_value(0x2300u) == 0x5A5Au);
  // Second identical failure: prefix commits, error still reports.
  auto second = bridge.pump(state, output);
  assert(!second.ok());
  assert(second.consumed == 34u);
  assert(bus.ring_read() == 34u * 4u);
  assert(output.size() == 1u);
  assert(std::holds_alternative<DrawPacket>(output.front()));
  for (std::uint32_t reg = 0u; reg < 4u; ++reg) {
    assert(state.register_value(0x2200u + reg * 0x10u) == 0x100u + reg * 0x10u);
  }
  assert(state.register_value(0x2300u) == 0x5A5Au);
  // Third failure starts at the IB packet itself: zero prefix, drop.
  auto third = bridge.pump(state, output);
  assert(!third.ok());
  assert(third.consumed == 0u);
  assert(bus.ring_read() == 34u * 4u);
  assert(output.size() == 1u);
}

void failed_decode_keeps_drop_path_below_sanity_gates() {
  using namespace ac6::native;
  const auto bad = pm4::type3_header(0x7Fu, 1u);
  // Failure at the first packet, repeated: zero prefix, never commits.
  std::vector<std::uint32_t> ring{bad, 0u};
  ring.resize(64u);
  MmioBus bus;
  assert(bus.write(MmioBus::kRingSize, 256u));
  assert(bus.write(MmioBus::kRingRead, 0u));
  assert(bus.write(MmioBus::kRingWrite, 8u));
  VdBridge bridge(bus);
  bridge.set_ring_words(ring);
  XenosState state;
  std::vector<XenosCommand> output;
  for (int attempt = 0; attempt < 3; ++attempt) {
    const auto result = bridge.pump(state, output);
    assert(!result.ok());
    assert(result.consumed == 0u);
    assert(bus.ring_read() == 0u);
    assert(output.empty());
  }
}

// r532 causal-intervention experiment: with AC6_NATIVE_PREFIX_ADVANCE_TEST
// set, a repeated identical nested failure with a below-gates prefix and
// stable IB content (double-read retries==0) consumes the ring window
// (lossy, diagnostic only) instead of dropping; without the env var the
// drop path above applies unchanged. Streak>=3 rules out transients:
// pumps 1-2 drop, pump 3 advances.
void failed_decode_advance_test_consumes_window_on_third_identical_repeat() {
  using namespace ac6::native;
  const auto ib = pm4::type3_header(pm4::kOpcodeIndirectBuffer, 2u);
  const auto bad = pm4::type3_header(0x7Fu, 1u);
  // Zero-prefix window: the failing IB packet is first (below the
  // 4-packet/32-word sanity gates, mirroring the frozen-window wedge).
  std::vector<std::uint32_t> ring{ib, 0x1000u, 4u};
  const auto written = static_cast<std::uint32_t>(ring.size() * 4u);
  ring.resize(64u);
  // Nested body: one good register write, then an unsupported opcode at
  // nested offset 2 (deterministic failure signature across pumps).
  const std::uint32_t nested[4] = {
      pm4::header(pm4::kType0, 1u, 0x2300u), 0xDEADu, bad, 0u};
  std::vector<std::uint8_t> memory(0x1000u + 16u, 0u);
  for (std::size_t i = 0u; i < 4u; ++i) {
    const std::uint32_t be = __builtin_bswap32(nested[i]);
    std::memcpy(memory.data() + 0x1000u + i * 4u, &be, 4u);
  }
  MmioBus bus;
  assert(bus.write(MmioBus::kRingSize, 256u));
  assert(bus.write(MmioBus::kRingRead, 0u));
  assert(bus.write(MmioBus::kRingWrite, written));
  VdBridge bridge(bus);
  bridge.set_ring_words(ring);
  bridge.set_guest_memory(memory.data());
  XenosState state;
  std::vector<XenosCommand> output;
  assert(setenv("AC6_NATIVE_PREFIX_ADVANCE_TEST", "1", 1) == 0);
  // Pumps 1-2: streak 1-2, drop path (no cursor move).
  for (int attempt = 0; attempt < 2; ++attempt) {
    const auto result = bridge.pump(state, output);
    assert(!result.ok());
    assert(result.consumed == 0u);
    assert(bus.ring_read() == 0u);
    assert(output.empty());
  }
  // Pump 3: streak hits 3 with stable content -> experimental advance
  // consumes the window; the error still reports and no work is staged.
  const auto third = bridge.pump(state, output);
  assert(!third.ok());
  assert(third.consumed == 3u);
  assert(bus.ring_read() == written);
  assert(output.empty());
  assert(unsetenv("AC6_NATIVE_PREFIX_ADVANCE_TEST") == 0);
}

// r533 non-lossy nested-prefix commit: a repeated identical depth-1
// nested failure with a below-gates TOP prefix but 32+ valid nested
// words commits the nested prefix (state, output) and consumes the
// failing top-level packet whole on the SECOND identical failure --
// only the cut packet's work is lost (still reported), nothing guessed.
// First failure drops (transients heal with zero behavior change).
void failed_decode_commits_nested_prefix_on_identical_repeat() {
  using namespace ac6::native;
  const auto draw = pm4::type3_header(pm4::kOpcodeDrawIndx2, 1u);
  const auto triangle = 4u | (2u << 6u) | (3u << 16u);
  const auto ib = pm4::type3_header(pm4::kOpcodeIndirectBuffer, 2u);
  const auto bad = pm4::type3_header(0x7Fu, 1u);
  // Zero top-level prefix (below the 4-packet/32-word gates): the failing
  // IB packet is first, mirroring the frozen-window wedge class.
  std::vector<std::uint32_t> ring{ib, 0x1000u, 38u};
  const auto written = static_cast<std::uint32_t>(ring.size() * 4u);
  ring.resize(64u);
  // Nested body: 1 draw + 4 register writes of 8 dwords (5 packets,
  // 34 words >= 32), then an unsupported opcode at nested offset 34.
  std::vector<std::uint32_t> nested{draw, triangle};
  for (std::uint32_t reg = 0u; reg < 4u; ++reg) {
    nested.push_back(pm4::header(pm4::kType0, 7u, 0x2200u + reg * 0x10u));
    for (std::uint32_t value = 0u; value < 7u; ++value) {
      nested.push_back(0x100u + reg * 0x10u + value);
    }
  }
  assert(nested.size() == 34u);
  nested.push_back(bad);
  nested.push_back(0u);
  assert(nested.size() == 36u);
  std::vector<std::uint8_t> memory(0x1000u + 38u * 4u, 0u);
  for (std::size_t i = 0u; i < nested.size(); ++i) {
    const std::uint32_t be = __builtin_bswap32(nested[i]);
    std::memcpy(memory.data() + 0x1000u + i * 4u, &be, 4u);
  }
  // Staged count covers the body (36) plus 2 trailing words: the cut
  // packet's header is present, its payload is not.
  MmioBus bus;
  assert(bus.write(MmioBus::kRingSize, 256u));
  assert(bus.write(MmioBus::kRingRead, 0u));
  assert(bus.write(MmioBus::kRingWrite, written));
  VdBridge bridge(bus);
  bridge.set_ring_words(ring);
  bridge.set_guest_memory(memory.data());
  XenosState state;
  std::vector<XenosCommand> output;
  // First identical failure: exact rollback, as before r533.
  auto first = bridge.pump(state, output);
  assert(!first.ok());
  assert(first.consumed == 0u);
  assert(bus.ring_read() == 0u);
  assert(output.empty());
  // Second identical failure: nested prefix commits (cursor past the
  // 3-word top-level IB packet), nested draw staged, registers applied.
  auto second = bridge.pump(state, output);
  assert(!second.ok());
  assert(second.consumed == 3u);
  assert(bus.ring_read() == 3u * 4u);
  assert(output.size() == 1u);
  assert(std::holds_alternative<DrawPacket>(output.front()));
  for (std::uint32_t reg = 0u; reg < 4u; ++reg) {
    assert(state.register_value(0x2200u + reg * 0x10u) == 0x100u + reg * 0x10u);
  }
}

void failed_decode_resets_on_changed_signature() {  using namespace ac6::native;
  const auto draw = pm4::type3_header(pm4::kOpcodeDrawIndx2, 1u);
  const auto triangle = 4u | (2u << 6u) | (3u << 16u);
  const auto ib = pm4::type3_header(pm4::kOpcodeIndirectBuffer, 2u);
  const auto bad = pm4::type3_header(0x7Fu, 1u);
  std::vector<std::uint32_t> ring{draw, triangle};
  for (std::uint32_t reg = 0u; reg < 4u; ++reg) {
    ring.push_back(pm4::header(pm4::kType0, 7u, 0x2200u + reg * 0x10u));
    for (std::uint32_t value = 0u; value < 7u; ++value) {
      ring.push_back(0u);
    }
  }
  ring.push_back(ib);
  ring.push_back(0x3000u);
  ring.push_back(4u);
  const auto written = static_cast<std::uint32_t>(ring.size() * 4u);
  ring.resize(64u);
  const std::vector<std::uint32_t> guest{
      pm4::header(pm4::kType0, 1u, 0x2300u), 0xDEADu, bad, 0u};
  MmioBus bus;
  assert(bus.write(MmioBus::kRingSize, 256u));
  assert(bus.write(MmioBus::kRingRead, 0u));
  assert(bus.write(MmioBus::kRingWrite, written));
  VdBridge bridge(bus);
  bridge.set_ring_words(ring);
  bridge.set_guest_words(0x3000u, guest);
  XenosState state;
  std::vector<XenosCommand> output;
  // First failure records the signature and drops.
  assert(!bridge.pump(state, output).ok());
  assert(bus.ring_read() == 0u);
  // A different failure (unmapped IB address: range error, not an
  // unsupported opcode) does not chain onto the recorded one.
  ring[35] = 0x9000u;
  bridge.set_ring_words(ring);
  const auto changed = bridge.pump(state, output);
  assert(!changed.ok());
  assert(changed.consumed == 0u);
  assert(bus.ring_read() == 0u);
  assert(output.empty());
}

void draw_indx_checks_length_before_index_fields() {
  using namespace ac6::native;
  XenosState state;
  std::vector<XenosCommand> commands;
  const auto header = pm4::type3_header(pm4::kOpcodeDrawIndx, 2u);
  const std::vector<std::uint32_t> short_indexed{header, 0u, 4u | (3u << 16u)};
  assert(!Pm4Decoder::decode_one(short_indexed, state, commands).ok());
  assert(commands.empty());
  const std::vector<std::uint32_t> auto_indexed{
      header, 0u, 4u | (2u << 6u) | (3u << 16u)};
  assert(Pm4Decoder::decode_one(auto_indexed, state, commands).ok());
  const auto& packet = std::get<DrawPacket>(commands.front());
  assert(packet.index_address == 0u && packet.index_format == 0u);
  assert(packet.vertex_stride == 1u && packet.state_snapshot);
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

void vulkan_xcb_visible_present() {
  // Visible proof path: XCB window under Xvfb, surface, swapchain, red
  // frame presented, then a dwell for an EXTERNAL screenshot capture
  // (the pixels are also verified in-GPU via readback_image).
  const char* display = std::getenv("DISPLAY");
  if (display == nullptr || display[0] == '\0') {
    std::fprintf(stderr, "SKIP vulkan_xcb_visible_present: no DISPLAY\n");
    return;
  }
  ac6::native::VulkanXcbWindow window(1280u, 720u);
  if (!window.valid()) {
    std::fprintf(stderr, "SKIP vulkan_xcb_visible_present: %s\n",
                 window.error().c_str());
    return;
  }
  ac6::native::VulkanDeviceDesc desc;
  desc.enable_xcb_surface = true;
  ac6::native::VulkanDevice device(desc);
  if (!device.valid()) {
    std::fprintf(stderr, "SKIP vulkan_xcb_visible_present: %s\n",
                 device.error().c_str());
    return;
  }
  auto create_surface = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
      vkGetInstanceProcAddr(device.instance(), "vkCreateXcbSurfaceKHR"));
  if (create_surface == nullptr) {
    std::fprintf(stderr, "SKIP vulkan_xcb_visible_present: no xcb surface\n");
    return;
  }
  VkXcbSurfaceCreateInfoKHR surface_info{};
  surface_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  surface_info.connection = window.connection();
  surface_info.window = window.window();
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (create_surface(device.instance(), &surface_info, nullptr, &surface) !=
          VK_SUCCESS ||
      surface == VK_NULL_HANDLE) {
    std::fprintf(stderr, "SKIP vulkan_xcb_visible_present: surface failed\n");
    return;
  }
  bool ok = false;
  {
    ac6::native::VulkanSwapchain chain(device, surface, 2u);
    if (chain.valid()) {
      ac6::native::VulkanOffscreenTarget frame(device);
      if (frame.valid() && frame.clear(1.0f, 0.0f, 0.0f, 1.0f)) {
        std::uint32_t shown = 99u;
        if (chain.acquire(shown) && chain.present_offscreen(frame, shown)) {
          const std::vector<std::uint8_t> shown_pixels =
              chain.readback_image(shown);
          std::uint64_t shown_sum = 0u;
          for (const std::uint8_t byte : shown_pixels) {
            shown_sum += byte;
          }
          if (shown_pixels.size() == 1280u * 720u * 4u &&
              shown_sum == 1280ull * 720ull * 510ull) {
            ok = true;
            const char* dwell = std::getenv("AC6_XCB_DWELL_MS");
            unsigned long wait_ms = 3000ul;
            if (dwell != nullptr && dwell[0] != '\0') {
              wait_ms = std::strtoul(dwell, nullptr, 10);
            }
            std::fprintf(stderr, "XCB visible red frame dwelling %lums\n",
                         wait_ms);
            struct timespec pause{};
            pause.tv_sec = static_cast<time_t>(wait_ms / 1000ul);
            pause.tv_nsec =
                static_cast<long>((wait_ms % 1000ul) * 1000ul * 1000ul);
            nanosleep(&pause, nullptr);
          }
        }
      }
      if (!ok) {
        std::fprintf(stderr, "SKIP vulkan_xcb_visible_present: %s\n",
                     chain.error().c_str());
      }
    } else {
      std::fprintf(stderr, "SKIP vulkan_xcb_visible_present: %s\n",
                   chain.error().c_str());
    }
  }
  vkDestroySurfaceKHR(device.instance(), surface, nullptr);
  // A skipped visible proof is honest only with no DISPLAY/driver support;
  // here the window, device, surface and swapchain all qualified, so a
  // failed frame is a hard failure, not a skip.
  assert(ok);
}

void vulkan_swapchain_acquire_present_cycles() {
  // Fail-closed without a device or with fewer than two images.
  ac6::native::VulkanDeviceDesc bad_desc;
  bad_desc.width = 0u;
  const ac6::native::VulkanDevice bad_device(bad_desc);
  ac6::native::VulkanSwapchain bad_chain(bad_device);
  assert(!bad_chain.valid());
  assert(!bad_chain.error().empty());
  std::uint32_t bad_index = 0u;
  assert(!bad_chain.acquire(bad_index));
  assert(!bad_chain.present(0u));

  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr, "SKIP vulkan_swapchain_acquire_present_cycles\n");
    return;
  }
  ac6::native::VulkanSwapchain single(device, 1u);
  assert(!single.valid());
  ac6::native::VulkanSwapchain chain(device, 2u);
  if (!chain.valid()) {
    // Present support is driver-dependent (headless surface); a clean
    // refusal skips instead of failing the suite.
    std::fprintf(stderr, "SKIP vulkan_swapchain_acquire_present_cycles: %s\n",
                 chain.error().c_str());
    return;
  }
  const std::uint32_t count = chain.image_count();
  assert(count >= 2u);
  std::uint32_t previous = count;
  const std::uint32_t rounds = count * 2u;
  for (std::uint32_t round = 0u; round < rounds; ++round) {
    std::uint32_t index = count;
    assert(chain.acquire(index));
    assert(index < count);
    assert(index != previous);
    previous = index;
    assert(chain.present(index));
  }
  assert(chain.presents() == rounds);
  assert(!chain.present(count));
  assert(!chain.error().empty());

  // Presented pixels carry the offscreen frame: blit red, present, read the
  // swapchain image back. The byte SUM is channel-order invariant, so it
  // holds regardless of the driver's swapchain format (e.g. BGRA8).
  ac6::native::VulkanOffscreenTarget frame(device);
  assert(frame.valid());
  assert(frame.clear(1.0f, 0.0f, 0.0f, 1.0f));
  std::uint32_t shown = count;
  assert(chain.acquire(shown));
  assert(shown < count);
  assert(chain.present_offscreen(frame, shown));
  const std::vector<std::uint8_t> shown_pixels = chain.readback_image(shown);
  assert(shown_pixels.size() == 1280u * 720u * 4u);
  std::uint64_t shown_sum = 0u;
  for (const std::uint8_t byte : shown_pixels) {
    shown_sum += byte;
  }
  assert(shown_sum == 1280ull * 720ull * 510ull);
  assert(chain.presents() == rounds + 1u);
}

void vulkan_device_offscreen_clear_readback() {  // Fail-closed without a device: no GPU needed for this half.
  ac6::native::VulkanDeviceDesc bad_desc;
  bad_desc.width = 0u;
  const ac6::native::VulkanDevice bad_device(bad_desc);
  assert(!bad_device.valid());
  assert(!bad_device.error().empty());
  ac6::native::VulkanOffscreenTarget bad_target(bad_device);
  assert(!bad_target.valid());
  assert(!bad_target.error().empty());
  assert(bad_target.readback().empty());

  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    // Headless CI without any Vulkan ICD: skip loudly, pass vacuously.
    std::fprintf(stderr, "SKIP vulkan_device_offscreen_clear_readback: %s\n",
                 device.error().c_str());
    return;
  }
  assert(device.width() == 1280u);
  assert(device.height() == 720u);
  assert(!device.device_name().empty());
  std::fprintf(stderr, "Vulkan device: %s\n", device.device_name().c_str());
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  assert(target.clear(0.0f, 0.0f, 0.0f, 1.0f));
  const std::vector<std::uint8_t> black = target.readback();
  assert(black.size() == 1280u * 720u * 4u);
  std::uint64_t black_sum = 0u;
  for (const std::uint8_t byte : black) {
    black_sum += byte;
  }
  assert(black_sum == 1280ull * 720ull * 255ull);

  // Real present seam: validation + GPU execution + readable pixels.
  ac6::native::VulkanBackend backend;
  const ac6::native::PresentPacket present{0u, 1280u, 720u, 0u};
  assert(backend.present_to_offscreen(target, present, 1.0f, 0.0f, 0.0f, 1.0f));
  assert(backend.present_count() == 1u);
  const std::vector<std::uint8_t> red = target.readback();
  assert(red.size() == black.size());
  std::uint64_t red_sum = 0u;
  for (const std::uint8_t byte : red) {
    red_sum += byte;
  }
  assert(red_sum == 1280ull * 720ull * 510ull);

  // Fail-closed: bad surface, dimension mismatch, invalid target.
  const ac6::native::PresentPacket bad_surface{16u, 1280u, 720u, 0u};
  assert(!backend.present_to_offscreen(target, bad_surface, 0.0f, 0.0f, 0.0f,
                                       1.0f));
  assert(!backend.error().empty());
  const ac6::native::PresentPacket wrong_size{0u, 640u, 480u, 0u};
  assert(!backend.present_to_offscreen(target, wrong_size, 0.0f, 0.0f, 0.0f,
                                       1.0f));
  assert(backend.present_count() == 1u);
  assert(!backend.present_to_offscreen(bad_target, present, 0.0f, 0.0f, 0.0f,
                                       1.0f));
}

void immediate_shader_packet_retains_microcode() {
  ac6::native::MmioBus bus;
  assert(bus.write(ac6::native::MmioBus::kRingSize, 256u));
  const std::vector<std::uint32_t> ucode{0xDEAD0001u, 0xDEAD0002u, 0xDEAD0003u};
  // NOTE: start must be 0: the qualified envelope rule rejects nonzero
  // start until an observed packet proves otherwise (r218 census: always 0).
  std::vector<std::uint32_t> ring{
      ac6::native::pm4::type3_header(
          ac6::native::pm4::kOpcodeImLoadImmediate,
          static_cast<std::uint32_t>(2u + ucode.size())),
      0u, static_cast<std::uint32_t>(ucode.size())};
  ring.insert(ring.end(), ucode.begin(), ucode.end());
  ring.resize(64u, 0u);
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  // One header + 2 envelope + 3 microcode dwords = 6 dwords = 24 bytes.
  assert(bus.write(ac6::native::MmioBus::kRingWrite, 24u));
  ac6::native::VdBridge bridge(bus);
  bridge.set_ring_words(ring);
  ac6::native::XenosState state;
  std::vector<ac6::native::XenosCommand> output;
  assert(bridge.pump(state, output).ok());
  assert(output.size() == 1u);
  assert(std::holds_alternative<ac6::native::ImmediateShaderPacket>(
      output.front()));
  const auto packet =
      std::get<ac6::native::ImmediateShaderPacket>(output.front());
  assert(packet.shader_type == 0u);
  assert(packet.start == 0u);
  assert(packet.dword_count == ucode.size());
  assert(packet.microcode == ucode);

  // Malformed envelope still rejected: count < 2.
  std::vector<std::uint32_t> short_ring{
      ac6::native::pm4::type3_header(
          ac6::native::pm4::kOpcodeImLoadImmediate, 1u),
      0u};
  short_ring.resize(64u, 0u);
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, 8u));
  ac6::native::VdBridge short_bridge(bus);
  short_bridge.set_ring_words(short_ring);
  std::vector<ac6::native::XenosCommand> short_output;
  assert(!short_bridge.pump(state, short_output).ok());
  assert(short_output.empty());
}

void ucode_registry_matches_pinned_only() {
  using ac6::native::ShaderTranslator;
  using ac6::native::UcodeFetchSignature;
  const std::vector<std::uint32_t> ucode{0xA5A50001u, 0xA5A50002u};
  // Empty registry refuses everything.
  assert(!ShaderTranslator::translate_ucode(0u, 0u, ucode).ok());
  // Registration with a wrong digest is rejected without touching state.
  UcodeFetchSignature bad{0u, 0u, static_cast<std::uint32_t>(ucode.size()),
                           0x12345678u};
  assert(!ShaderTranslator::register_pinned(
      bad, ucode, std::vector<std::uint32_t>{0x07230203u, 1u, 2u, 3u, 4u}));
  assert(!ShaderTranslator::translate_ucode(0u, 0u, ucode).ok());
  // Empty SPIR-V is rejected too.
  UcodeFetchSignature empty_spv{
      0u, 0u, static_cast<std::uint32_t>(ucode.size()),
      ShaderTranslator::digest_words(ucode)};
  assert(!ShaderTranslator::register_pinned(empty_spv, ucode,
                                            std::vector<std::uint32_t>{}));
  // Valid pinning then activates exactly one fetch.
  const std::vector<std::uint32_t> spirv{0x07230203u, 0x00010500u, 0u, 8u, 0u};
  UcodeFetchSignature good{
      0u, 0u, static_cast<std::uint32_t>(ucode.size()),
      ShaderTranslator::digest_words(ucode)};
  assert(ShaderTranslator::register_pinned(good, ucode, spirv));
  const auto hit = ShaderTranslator::translate_ucode(0u, 0u, ucode);
  assert(hit.ok());
  assert(hit.spirv == spirv);
  // Near misses refuse: wrong type, start, length, one flipped word.
  assert(!ShaderTranslator::translate_ucode(1u, 0u, ucode).ok());
  assert(!ShaderTranslator::translate_ucode(0u, 1u, ucode).ok());
  const std::vector<std::uint32_t> short_ucode{0xA5A50001u};
  assert(!ShaderTranslator::translate_ucode(0u, 0u, short_ucode).ok());
  const std::vector<std::uint32_t> flipped{0xA5A50001u, 0xA5A50003u};
  assert(!ShaderTranslator::translate_ucode(0u, 0u, flipped).ok());
}

// r517: drain-level prefix commit. A repeated identical short-count IB
// failure commits the complete top-level prefix through the full drain
// (backend submit + bus cursor + state) while the nested fragment rolls
// back; a third identical failure starts at the IB itself (zero prefix)
// and drops. Deterministic: no threads, no Vulkan device.
void guest_vd_service_drain_commits_prefix_on_repeated_wedge() {
  using namespace ac6::native;
  ac6::native::GuestAddressSpace guest;
  assert(guest.valid());
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  ac6::native::XenosState state;
  ac6::native::VulkanBackend backend;
  ac6::native::NativeGuestVdService service;
  service.bind(guest.base(), bus, bridge, state, backend);
  constexpr std::uint32_t ring = 0x10000u;
  constexpr std::uint32_t ib_body = 0x12000u;
  service.register_allocation(guest.base(), ring, 256u);
  service.initialize_ring(guest.base(), ring, 5u);  // 1 << (5 + 3) bytes
  assert(state.set_register(0x2300u, 0x5A5Au));
  // Prefix: 1 present + 4 eight-dword register writes = 5 packets,
  // 37 dwords (above the 4-packet / 32-word sanity gates).
  std::vector<std::uint32_t> words{
      ac6::native::pm4::type3_header(ac6::native::pm4::kOpcodeXeSwap, 4u),
      ac6::native::pm4::kSwapSignature, 0x10000u, 1280u, 720u};
  for (std::uint32_t reg = 0u; reg < 4u; ++reg) {
    words.push_back(ac6::native::pm4::header(pm4::kType0, 7u, 0x2200u + reg * 0x10u));
    for (std::uint32_t value = 0u; value < 7u; ++value) {
      words.push_back(0x100u + reg * 0x10u + value);
    }
  }
  assert(words.size() == 37u);
  words.push_back(ac6::native::pm4::type3_header(pm4::kOpcodeIndirectBuffer, 2u));
  words.push_back(ib_body);
  words.push_back(8u);
  assert(words.size() == 40u);
  for (std::size_t index = 0u; index < words.size(); ++index) {
    const std::uint32_t encoded = __builtin_bswap32(words[index]);
    std::memcpy(guest.base() + ring + index * 4u, &encoded, sizeof(encoded));
  }
  // Nested body: one good write, then a 770-dword TYPE0 block of which
  // only 5 payload dwords are staged (short count, r510 shape).
  const std::array<std::uint32_t, 8> body{
      ac6::native::pm4::header(pm4::kType0, 1u, 0x2300u), 0xDEADu,
      0x03000100u, 0u, 0u, 0u, 0u, 0u};
  for (std::size_t index = 0u; index < body.size(); ++index) {
    const std::uint32_t encoded = __builtin_bswap32(body[index]);
    std::memcpy(guest.base() + ib_body + index * 4u, &encoded, sizeof(encoded));
  }
  // First identical failure: exact rollback, cursor unmoved.
  service.publish_write_address(guest.base(), ring + 160u);
  assert(bus.ring_read() == 0u);
  assert(state.register_value(0x2300u) == 0x5A5Au);
  // Second identical failure: prefix commits through the drain.
  service.publish_write_address(guest.base(), ring + 160u);
  assert(bus.ring_read() == 37u * 4u);
  for (std::uint32_t reg = 0u; reg < 4u; ++reg) {
    assert(state.register_value(0x2200u + reg * 0x10u) == 0x100u + reg * 0x10u);
  }
  assert(state.register_value(0x2300u) == 0x5A5Au);
  // Third failure starts at the IB packet itself: zero prefix, drop,
  // cursor unmoved (no free chaining).
  service.publish_write_address(guest.base(), ring + 160u);
  assert(bus.ring_read() == 37u * 4u);
  service.unbind();
}

void guest_vd_service_drains_published_dword_index() {  ac6::native::GuestAddressSpace guest;
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

void guest_vd_service_present_executes_offscreen() {
  // End-to-end synthetic frame without a guest: ring XE_SWAP 1280x720 is
  // decoded, validated, executed on the GPU target, and readable back.
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr, "SKIP guest_vd_service_present_executes_offscreen\n");
    return;
  }
  ac6::native::GuestAddressSpace guest;
  assert(guest.valid());
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  ac6::native::XenosState state;
  ac6::native::VulkanBackend backend;
  ac6::native::NativeGuestVdService service;
  service.bind(guest.base(), bus, bridge, state, backend);
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  service.bind_offscreen(&target);
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
  // submit() counts the accepted PRESENT once and the executed present
  // counts it a second time.
  assert(backend.present_count() == 2u);
  assert(bus.ring_read() == 20u);
  const std::vector<std::uint8_t> pixels = target.readback();
  assert(pixels.size() == 1280u * 720u * 4u);
  std::uint64_t sum = 0u;
  for (const std::uint8_t byte : pixels) {
    sum += byte;
  }
  assert(sum == 1280ull * 720ull * 255ull);

  // Fail-closed: dimension mismatch leaves the read pointer unadvanced.
  // Placed immediately after the first packet so decode deterministically
  // reaches it (no zero padding in the drained range).
  const std::array<std::uint32_t, 5> small{
      ac6::native::pm4::type3_header(ac6::native::pm4::kOpcodeXeSwap, 4u),
      ac6::native::pm4::kSwapSignature, 0x10000u, 640u, 480u};
  for (std::size_t index = 0u; index < small.size(); ++index) {
    const std::uint32_t encoded = __builtin_bswap32(small[index]);
    std::memcpy(guest.base() + ring + 20u + index * 4u, &encoded,
                sizeof(encoded));
  }
  service.publish_write_address(guest.base(), ring + 40u);
  // submit() accepts the 640x480 PRESENT (count 3, pump already advanced
  // the bus read pointer to 40 as with any accepted decode) but execution
  // fails on the dimension mismatch: no pixels change.
  assert(backend.present_count() == 3u);
  assert(bus.ring_read() == 40u);
  const std::vector<std::uint8_t> still = target.readback();
  assert(still == pixels);
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
  // r100: the IB dword-count field (payload[1] of INDIRECT_BUFFER) must
  // cover the whole 4-dword guest packet below (header + 3 payload words)
  // or the decoder truncates before ever reaching the register-range
  // check this test means to exercise. This was pre-existing since the
  // test's introduction (r95/r96) -- a fixed count of 1u always yielded
  // kTruncatedPacket, never kInvalidRegister, independent of anything
  // this cycle touched (verified: still failed with r99's code checked
  // out unmodified).
  ring[2] = 4u;
  assert(bus.write(MmioBus::kRingRead, 0u));
  assert(bus.write(MmioBus::kRingWrite, 12u));
  // r96 widened kRegisterCount to 0x8000 (TYPE0's own 15-bit base-register
  // field width), so a base of 0x4800 -- the real register r94 originally
  // found live -- is in range now. Use a count that overruns the top of
  // the (still finite) register file instead, to keep exercising this
  // exact rejection path and its hex formatting.
  const std::vector<std::uint32_t> guest{
      ac6::native::pm4::header(ac6::native::pm4::kType0, 3u, 0x7ffeu), 0u, 0u,
      0u};
  VdBridge bridge(bus);
  bridge.set_ring_words(ring);
  bridge.set_guest_words(ib_address, guest);
  XenosState state;
  std::vector<XenosCommand> output;
  const auto result = bridge.pump(state, output);
  assert(!result.ok());
  assert(result.error.code == ac6::native::Pm4ErrorCode::kInvalidRegister);
  assert(result.error.detail.find("0x125c0000") != std::string::npos);
  assert(result.error.detail.find("0x00027ffe") != std::string::npos);
  assert(result.error.detail.find("0x308019200") == std::string::npos);
}

void register_count_covers_type0_full_field_width() {
  // r96: register 0x4800 -- rejected under the old 0x4000 bound, the
  // exact live content r94 found -- is now accepted. r95's own decode
  // (header 0x00054800, base 0x4800, count 6) is the real packet this
  // reproduces.
  XenosState state;
  std::vector<XenosCommand> output;
  const std::array<std::uint32_t, 7> stream{
      ac6::native::pm4::header(ac6::native::pm4::kType0, 6u, 0x4800u),
      0u, 0u, 0u, 0u, 0u, 0u};
  const auto result = Pm4Decoder::decode_stream(stream, state, output);
  assert(result.ok());
  assert(output.empty());
  assert(state.register_value(0x4800u) == 0u);
  assert(state.register_value(0x4805u) == 0u);
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

void bundled_pinned_registry_fully_hits() {
  using ac6::native::ShaderTranslator;
  // r255: the qualified US capsule (retail-us-native-r254-ucode-frontier)
  // originally carried 271 (microcode, runtime modification, SPIR-V)
  // translations of the oracle's own engine; r470-r471 extended it to 320
  // with a real `world=1` flight-gameplay capture (255 distinct fetch
  // signatures, 65 signatures with more than one modification). Loading it
  // must register all of them and activate every qualified fetch;
  // everything else still refuses.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  assert(entries.size() == 323u);
  assert(ShaderTranslator::pinned_count() >= entries.size());
  // 321 runtime modifications over 255 distinct fetch signatures: some
  // shaders were translated under 2-3 modifications. The legacy
  // ShaderTranslator::translate_ucode() lookup resolves a duplicate to the
  // first registered entry (capsule order is deterministic); the r260/r271
  // selective lookup (translate_ucode_variant(), used by draw_pinned())
  // instead filters by the state-derived modification's high dword first.
  std::size_t vertex_count = 0u;
  std::size_t pixel_count = 0u;
  using SignatureKey =
      std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t>;
  std::map<SignatureKey, std::vector<std::uint32_t>> first_spirv;
  // ShaderTranslator::translate_ucode() (the legacy, no-modification-arg
  // overload used below) tries an exact modification == 0 match FIRST
  // (via the modification-aware overload with modification=0), and only
  // falls back to "first registered entry" legacy resolution when no
  // modification == 0 entry exists for the digest -- so the expected
  // SPIR-V per signature must mirror that same two-tier rule, not plain
  // insertion order (r472: the 320-entry merge introduced digests where a
  // later-registered modification == 0 variant exists for a signature
  // whose first-registered entry has a non-zero modification).
  std::map<SignatureKey, std::vector<std::uint32_t>> zero_mod_spirv;
  for (const auto& entry : entries) {
    assert(entry.signature.shader_type <= 1u);
    assert(entry.signature.start == 0u);
    (entry.signature.shader_type == 0u ? vertex_count : pixel_count) += 1u;
    const SignatureKey key{entry.signature.shader_type, entry.signature.start,
                           entry.signature.dword_count, entry.signature.digest};
    first_spirv.emplace(key, entry.spirv);
    if (entry.modification == 0u) {
      zero_mod_spirv.emplace(key, entry.spirv);
    }
  }
  assert(vertex_count == 130u);
  assert(pixel_count == 193u);
  for (const auto& entry : entries) {
    const auto hit = ShaderTranslator::translate_ucode(
        entry.signature.shader_type, entry.signature.start, entry.microcode);
    assert(hit.ok());
    // Every fetch resolves deterministically: an exact modification == 0
    // registration wins, otherwise the first registered variant.
    const SignatureKey key{entry.signature.shader_type, entry.signature.start,
                           entry.signature.dword_count, entry.signature.digest};
    const auto zero_mod_it = zero_mod_spirv.find(key);
    const std::vector<std::uint32_t>& expected =
        zero_mod_it != zero_mod_spirv.end() ? zero_mod_it->second
                                            : first_spirv.at(key);
    assert(hit.spirv == expected);
    // Near misses still refuse inside the fully pinned registry.
    assert(!ShaderTranslator::translate_ucode(entry.signature.shader_type ^ 1u,
                                              entry.signature.start,
                                              entry.microcode)
                .ok());
    assert(!ShaderTranslator::translate_ucode(entry.signature.shader_type,
                                              entry.signature.start + 1u,
                                              entry.microcode)
                .ok());
  }
  assert(vertex_count == 130u);
  assert(pixel_count == 193u);
  // Idempotent reload: the registry does not grow.
  const std::size_t pinned_before = ShaderTranslator::pinned_count();
  assert(ac6::native::register_bundled_pinned_shader_registry());
  assert(ShaderTranslator::pinned_count() == pinned_before);
  // Any corruption fails closed without touching the registry.
  std::vector<std::uint8_t> corrupt(capsule.begin(), capsule.end());
  corrupt[corrupt.size() / 2u] = static_cast<std::uint8_t>(corrupt[corrupt.size() / 2u] ^ 0x40u);
  assert(!ac6::native::register_pinned_shader_capsule(corrupt));
  assert(!ac6::native::register_pinned_shader_capsule(
      std::span<const std::uint8_t>(capsule.data(), capsule.size() - 4u)));
  std::vector<std::uint8_t> bad_magic(corrupt);
  bad_magic[0] = static_cast<std::uint8_t>('X');
  assert(!ac6::native::register_pinned_shader_capsule(bad_magic));
  assert(ShaderTranslator::pinned_count() == pinned_before);
}


void pinned_shaders_execute_a_real_frame() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr, "SKIP pinned_shaders_execute_a_real_frame: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr, "SKIP pinned_shaders_execute_a_real_frame: %s\n",
                 runtime.error().c_str());
    return;
  }
  // The qualified US capsule carries both shaders; find them by their exact
  // fetch digests (computed from the r254 oracle evidence; the registry
  // lookup then enforces byte-exact equality anyway).
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xe74686edb2236614ull;  // 6034746566624A37
  constexpr std::uint64_t kPixelDigest = 0x240522311d02461bull;   // AB5C776B8479A29A
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  // Real PM4 stream: IM_LOAD_IMMEDIATE (VS, PS), SET_CONSTANT fetch vf0 +
  // float c0, DRAW_INDX_2 (auto-indexed triangle list), XE_SWAP 1280x720.
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  // Vertex shader load (the staged count must equal the entry's microcode
  // size: IM_LOAD_IMMEDIATE stages exactly payload[1] dwords).
  {
    std::vector<std::uint32_t> payload{
        0u, static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // Pixel shader load.
  {
    std::vector<std::uint32_t> payload{
        1u, static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // Fetch constant vf0 via TYPE0 register writes (the qualified register
  // path). The pinned SPIR-V reads this fetch constant's two raw words at
  // UBO dwords 190/191 = registers 0x48BE/0x48BF (the oracle register-file
  // layout: vertex fetch constant v lives at 2*v from 0x4800, and the ucode
  // "vf0" operand maps to slot 95). type kVertex (3), address 0x1000 dwords,
  // 16 words.
  {
    const std::uint32_t base = 0x48BEu;
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, base));
    ring.push_back(3u | (0x1000u << 2u));  // dword0: type | address
    // dword1: endian k8in32 (the guest stores big-endian 32-bit float words;
    // verified against the pinned SPIR-V's endian swap paths: with endian 2
    // the shader's sequential byte-swap recovers the host float bits exactly)
    // | size. 36-byte records: the full VS fetches 9-dword records (the
    // r273 derivation fixture).
    ring.push_back(2u | (36u << 2u));
  }
  // Pixel float constant c0 = 0: the pinned PS exports (c0.x, 1, 1, 1).
  // The pixel float bank starts at 0x4400.
  {
    const std::uint32_t base = 0x4400u;
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 4u, base));
    ring.push_back(0u);
    ring.push_back(0u);
    ring.push_back(0u);
    ring.push_back(0u);
  }
  // Vertex float constants: the full VS's compact slots map to guest
  // c130/c168-c171/c212-c215/c218-c221 (the derivation fixture: the
  // identity matrix rows c218-c221 and the w/half constants). Without
  // these the fetched positions collapse to (0,0,0,0).
  {
    const std::uint32_t vc[13][5] = {
        {130u, 0u, 0u, 0u, 0u},
        {168u, 0u, 0u, 0u, 0u},
        {169u, 0u, 0u, 0u, 0x3F800000u},
        {170u, 0u, 0u, 0u, 0u},
        {171u, 0u, 0u, 0u, 0x3F800000u},
        {212u, 0u, 0u, 0u, 0u},
        {213u, 0u, 0u, 0u, 0u},
        {214u, 0u, 0u, 0u, 0u},
        {215u, 0x3F800000u, 0u, 0u, 0u},
        {218u, 0x3F800000u, 0u, 0u, 0u},
        {219u, 0u, 0x3F800000u, 0u, 0u},
        {220u, 0u, 0u, 0x3F800000u, 0u},
        {221u, 0u, 0u, 0u, 0x3F800000u}};
    for (const auto& cst : vc) {
      ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 4u,
                                              0x4000u + 4u * cst[0]));
      ring.push_back(cst[1]);
      ring.push_back(cst[2]);
      ring.push_back(cst[3]);
      ring.push_back(cst[4]);
    }
  }
  // Register-driven EDRAM render target (r257 contract):
  // - RB_MODECONTROL = EdramMode::kColorDepth (4): color draws write EDRAM.
  // - RB_SURFACE_INFO: pitch 1280 pixels, MSAA 1x.
  // - RB_COLOR_INFO: color base 16 tiles (the second tile row at pitch
  //   1280: 2048 tiles / 16 tiles per row -> image 1280x2048; base tile 16
  //   maps to pixel origin (0, 16)), format k_8_8_8_8 (0), exp bias 0.
  // - PA_SC_SCREEN_SCISSOR TL (0,0) / BR (1280,720): the drawable region.
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u,
                                            0x2208u));
    ring.push_back(4u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u,
                                            0x2000u));
    ring.push_back(1280u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u,
                                            0x2001u));
    ring.push_back(16u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u,
                                            0x200Eu));
    ring.push_back(0u);              // TL (0, 0)
    ring.push_back(720u << 16u | 1280u);  // BR (1280, 720)
  }
  // RB_COLOR_MASK: RT0 writes all four components (bits 0:3).
  ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u,
                                          0x2104u));
  ring.push_back(0xFu);
  // Vertex index range (the guest draw state; the pinned VS clamps the
  // per-vertex index to [VGT_MIN_VTX_INDX, VGT_MAX_VTX_INDX]).
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 0x2100u));
    ring.push_back(0xFFFFFFFFu);  // VGT_MAX_VTX_INDX
    ring.push_back(0u);           // VGT_MIN_VTX_INDX
  }
  // Viewport (PA_CL_VPORT X..Z scale/offset floats + PA_CL_VTE_CNTL enables
  // with D3D-style -Y): a full 1280x720 guest viewport.
  {
    const std::uint32_t bits[6] = {
        0x44200000u,  // XSCALE  = 640
        0x44200000u,  // XOFFSET = 640
        0xC3B40000u,  // YSCALE  = -360
        0x43B40000u,  // YOFFSET = 360
        0x3F800000u,  // ZSCALE  = 1
        0x00000000u,  // ZOFFSET = 0
    };
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 6u, 0x210Fu));
    for (const std::uint32_t value : bits) ring.push_back(value);
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, 1u, 0x2206u));
    ring.push_back(0x3Fu);  // VPORT_X/Y/Z_SCALE/OFFSET_ENA
  }
  // Triangle list, 3 vertices, auto-indexed (DRAW_INDX_2 source 2).
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  // XE_SWAP 1280x720.
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});

  // The MMIO ring contract is a power-of-two fetch window (256 B .. 1 MiB);
  // this stream (register-driven EDRAM setup added in r257) exceeds 64
  // dwords, so the fixture configures the valid 512-byte window.
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  assert(bridge.pump(state, output).ok());
  // Two shader loads, one draw, one present; the TYPE0 register writes
  // commit state without emitting commands.
  assert(output.size() == 4u);
  assert(state.active_shader(0u).size() == vertex_entry->microcode.size());
  assert(state.active_shader(1u).size() == pixel_entry->microcode.size());

  // The submitted draw owns its earlier shader/register state. Poisoning
  // the final state must not change this test's real pixel readback.
  const std::array<std::uint32_t, 1> later_shader{0xDEADBEEFu};
  assert(state.stage_shader(0u, later_shader));
  assert(state.set_register(0x2200u, 0u));

  // Raw guest bytes (big-endian): 9-dword records at dword address 0x1000,
  // 36-byte stride (the full VS's fetch; the r273 derivation fixture).
  // Positions (-0.5,-0.5), (0.5,-0.5), (0, 0.5) in clip space.
  const std::uint32_t vertex_rec[3][9] = {
      {0xBF000000u, 0xBF000000u, 0x00000000u, 0x3C000000u, 0x40000000u,
       0x3E800000u, 0x3F000000u, 0x3F400000u, 0x3E800000u},
      {0x3F000000u, 0xBF000000u, 0x00000000u, 0x3C000000u, 0x40000000u,
       0x3E800000u, 0x3F000000u, 0x3F400000u, 0x3E800000u},
      {0x00000000u, 0x3F000000u, 0x00000000u, 0x3C000000u, 0x40000000u,
       0x3E800000u, 0x3F000000u, 0x3F400000u, 0x3E800000u}};
  std::vector<std::uint8_t> vertex_bytes(3u * 36u, 0u);
  for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
    for (std::uint32_t word = 0u; word < 9u; ++word) {
      const std::uint32_t swapped = __builtin_bswap32(vertex_rec[vertex][word]);
      std::memcpy(vertex_bytes.data() + vertex * 36u + word * 4u, &swapped,
                  sizeof(swapped));
    }
  }
  assert(runtime.write_shared_memory(0x1000u, vertex_bytes));

  assert(runtime.execute_frame(target, state, output));
  assert(runtime.draw_count() == 1u);
  assert(runtime.present_count() == 1u);
  // The XE_SWAP resolved the EDRAM render-target surface into the readable
  // image (base tile 16 -> EDRAM rows 16..736, resolved 1:1 at the region
  // size).
  assert(runtime.edram_resolves() == 1u);
  const auto pixels = target.readback();
  assert(pixels.size() == 1280u * 720u * 4u);
  // Center pixel is covered: (c0.x, 1, 1, 1) = (0, 255, 255, 255).
  const std::size_t center =
      (360u * 1280u + 640u) * 4u;
  assert(pixels[center + 0u] == 0u);
  assert(pixels[center + 1u] == 255u);
  assert(pixels[center + 2u] == 255u);
  assert(pixels[center + 3u] == 255u);
  // Corner inside the resolved region stays the EDRAM clear color
  // (0, 0, 0, 0): real EDRAM invalidates to zero, and only covered pixels
  // carry the pixel-shader export.
  const std::size_t corner = (5u * 1280u + 5u) * 4u;
  assert(pixels[corner + 0u] == 0u);
  assert(pixels[corner + 1u] == 0u);
  assert(pixels[corner + 2u] == 0u);
  assert(pixels[corner + 3u] == 0u);
}

void pinned_shader_samples_a_real_texture(bool registered_aliases,
                                         bool oversized_scissor = false) {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr, "SKIP pinned_shader_samples_a_real_texture: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr, "SKIP pinned_shader_samples_a_real_texture: %s\n",
                 runtime.error().c_str());
    return;
  }
  // The pinned shader pair: vertex 645F413EAFC41025 (digest
  // 0x8ff8fbdbe488f0cd, 21 dwords) and pixel 08DE923E15A1A964 (digest
  // 0xa82b05755cee587b, 18 dwords) whose export is
  // sat(tfetch2D(t0, r1.xy).rgb * c255.x) - the export depends on a real
  // tfetch of texture t0 (set 3: texture0_2d_u/s at bindings 0/1,
  // sampler0_fff at 2). The oracle compacts pixel float constants by usage;
  // this shader uses exactly one constant, so compact slot 0 = the first
  // vec4 of the raw guest bank (c0) - the test writes c0 = (1,0,0,0).
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0x8ff8fbdbe488f0cdull;  // 645F413EAFC41025
  constexpr std::uint64_t kPixelDigest = 0xa82b05755cee587bull;  // 08DE923E15A1A964
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // Texture fetch constant t0 (registers 0x4800..0x4805): type kTexture,
  // unsigned signs, clamp-to-edge x/y, pitch 256 pixels (>> 5 = 8), linear,
  // format k_8_8_8_8 (6), endianness k8in32, guest base 0x100000, 256x256,
  // identity swizzle, single mip, 2D.
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 6u,
                                            0x4800u));
    ring.push_back(2u | (2u << 10u) | (2u << 13u) | (8u << 22u));
    ring.push_back(6u | (2u << 6u) | (0x100u << 12u));
    ring.push_back(255u | (255u << 13u));
    ring.push_back((83u << 1u) | (1u << 19u) | (1u << 21u) | (1u << 23u));
    ring.push_back(0u);
    ring.push_back(1u << 9u);
  }
  // Vertex fetch constant vf0 (group 47, registers 0x48BE/0x48BF; does not
  // collide with texture fetch group 0): address dword 0x1000, endian none.
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u,
                                            0x48BEu));
    ring.push_back(3u | (0x1000u << 2u));
    ring.push_back(2u | (16u << 2u));
  }
  // Pixel float constant c255.x = 1 (the pixel bank starts at 0x4400, so
  // c255 = 0x4400 + 255*4 = 0x47FC): the shader's single used constant (its
  // map has exactly one bit, block 3 bit 63); the compacted slot 0 now
  // carries the REAL guest register value, so the export is
  // sat(sample.rgb * 1).
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 4u,
                                            0x47FCu));
    ring.push_back(0x3F800000u);
    ring.push_back(0u);
    ring.push_back(0u);
    ring.push_back(0u);
  }
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 0x2100u));
    ring.push_back(0xFFFFFFFFu);
    ring.push_back(0u);
  }
  {
    const std::uint32_t bits[6] = {
        0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u, 0x3F800000u, 0u,
    };
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 6u, 0x210Fu));
    for (const std::uint32_t value : bits) ring.push_back(value);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2206u));
    ring.push_back(0x3Fu);
  }
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2208u));
    ring.push_back(4u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2000u));
    ring.push_back(1280u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2001u));
    ring.push_back(16u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2104u));
    ring.push_back(0xFu);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 0x200Eu));
    ring.push_back(0u);
    ring.push_back(oversized_scissor ? 0x20002000u : (720u << 16u | 1280u));
  }
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);
  // Vertex data as in the pinned real-frame test (big-endian, k8in32).
  const float vertex_xy[3][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
  std::vector<std::uint8_t> vertex_bytes(3u * 16u, 0u);
  for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
    for (std::uint32_t component = 0u; component < 2u; ++component) {
      std::uint32_t bits = 0u;
      std::memcpy(&bits, &vertex_xy[vertex][component], sizeof(bits));
      const std::uint32_t swapped = __builtin_bswap32(bits);
      std::memcpy(vertex_bytes.data() + vertex * 16u + component * 4u, &swapped,
                  sizeof(swapped));
    }
  }
  ac6::native::GuestAddressSpace guest;
  assert(guest.valid());
  ac6::native::VulkanBackend backend;
  ac6::native::NativeGuestVdService service;
  constexpr std::uint32_t guest_ring = 0x20010000u;
  if (registered_aliases) {
    // r569 regression: ring and texture lie in the wrapped part of one
    // allocation. Its alias also covers the vertex address; a later low
    // registration must restore these vertices after the high alias copy.
    service.bind(guest.base(), bus, bridge, state, backend);
    service.bind_pinned(&runtime);
    service.bind_offscreen(&target);
    service.register_allocation(guest.base(), 0x1ffff000u, 0x141000u);
    std::memcpy(guest.base() + 0x4000u, vertex_bytes.data(), vertex_bytes.size());
    service.register_allocation(guest.base(), 0x4000u, static_cast<std::uint32_t>(vertex_bytes.size()));
    for (std::size_t i = 0u; i < ring.size(); ++i) {
      const auto word = __builtin_bswap32(ring[i]);
      std::memcpy(guest.base() + guest_ring + i * 4u, &word, 4u);
    }
    service.initialize_ring(guest.base(), 0x10000u, 7u);  // 1024 bytes
  } else {
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  }

  // The texture: 256x256 k_8_8_8_8 at physical byte address 0x100000, stored
  // big-endian (k8in32). Frame 1: solid (64,255,128,255); frame 2: solid
  // (255,0,0,255). The shader samples a clamped edge texel of the solid
  // texture, so every fragment receives exactly the texel color.
  const std::uint32_t texture_dwords = 256u * 256u;
  const auto make_texture = [](std::uint8_t r, std::uint8_t g, std::uint8_t b,
                               std::uint8_t a) {
    std::vector<std::uint8_t> bytes(texture_dwords * 4u);
    for (std::uint32_t i = 0u; i < texture_dwords; ++i) {
      // Guest bytes are stored reversed (k8in32): a, b, g, r.
      bytes[i * 4u + 0u] = a;
      bytes[i * 4u + 1u] = b;
      bytes[i * 4u + 2u] = g;
      bytes[i * 4u + 3u] = r;
    }
    return bytes;
  };
  auto run_frame = [&](const std::vector<std::uint8_t>& texture_bytes) {
    if (registered_aliases) {
      std::memcpy(guest.base() + 0x20100000u, texture_bytes.data(), texture_bytes.size());
      assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
      assert(bus.write(ac6::native::MmioBus::kRingWrite, 0u));
      service.publish_write_address(guest.base(), guest_ring + ring_bytes);
      assert(bus.ring_read() == ring_bytes);
      // Sync changes only GPU memory, never materializes over the low guest view.
      assert(guest.base()[0x100000u] == 0u);
      return target.readback();
    }
    assert(runtime.write_shared_memory(0x100000u / 4u, texture_bytes));
    const bool ok = runtime.execute_frame(target, state, output);
    if (!ok) {
      std::fprintf(stderr, "TEX FRAME ERR: %s\n", runtime.error().c_str());
    }
    assert(ok);
    return target.readback();
  };
  const auto pixels_a = run_frame(make_texture(64u, 255u, 128u, 255u));
  const auto pixels_b = run_frame(make_texture(255u, 0u, 0u, 255u));
  assert(pixels_a.size() == 1280u * 720u * 4u);
  assert(pixels_b.size() == 1280u * 720u * 4u);
  assert(runtime.draw_count() == 2u);
  assert(runtime.edram_resolves() == 2u);
  const std::size_t center = (360u * 1280u + 640u) * 4u;
  // The export must depend on the real tfetch: sat(sample.rgb * 1) with the
  // covered texel - exact for both solid textures (the covered pixel is the
  // first triangle pixel).
  const std::size_t probe = (361u * 1280u + 639u) * 4u;
  assert(pixels_a[probe + 0u] == 64u);
  assert(pixels_a[probe + 1u] == 255u);
  assert(pixels_a[probe + 2u] == 128u);
  assert(pixels_a[probe + 3u] == 255u);
  assert(pixels_b[probe + 0u] == 255u);
  assert(pixels_b[probe + 1u] == 0u);
  assert(pixels_b[probe + 2u] == 0u);
  assert(pixels_b[probe + 3u] == 255u);
  if (registered_aliases) {
    // The retail gate requires this marker, so a missing Vulkan device cannot
    // silently turn the pixel regression into a successful skipped test.
    std::fprintf(stderr, "r569 wrapped-alias pixel regression PASS\n");
  }
  if (oversized_scissor) {
    // A 2048-row EDRAM image must not stretch the 720-row viewport. This
    // point lies below the triangle; the existing probe lies inside it.
    const std::size_t outside = (600u * 1280u + 640u) * 4u;
    for (unsigned channel = 0u; channel < 3u; ++channel) {
      assert(pixels_a[outside + channel] == 0u);
      assert(pixels_b[outside + channel] == 0u);
    }
    std::fprintf(stderr, "r571 viewport pixel regression PASS\n");
  }
}

void pinned_texture_shader_uses_compacted_constants() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_texture_shader_uses_compacted_constants: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_texture_shader_uses_compacted_constants: %s\n",
                 runtime.error().c_str());
    return;
  }
  // Pixel 031177CD1B2446A4 (digest 0xc8addfdcf7222907, 54 dwords): a gamma
  // decode whose ucode consumes SEVEN pixel float constants
  // (c101/c102/c103/c104/c105/c106/c255) plus one tfetch2D. Its pinned
  // SPIR-V indexes the COMPACTED float layout (compact slots 0..6); with
  // the constants written to the REAL guest registers the export is
  // (sample.g, sample.g, sample.b, 1) for the qualified constants below.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0x8ff8fbdbe488f0cdull;  // 645F413EAFC41025
  constexpr std::uint64_t kPixelDigest = 0xc8addfdcf7222907ull;   // 031177CD1B2446A4
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);
  // The pinned entry's map: exactly 7 used float constants.
  {
    std::uint32_t bits = 0u;
    for (const std::uint64_t block : pixel_entry->constant_map.float_bitmap) {
      bits += static_cast<std::uint32_t>(__builtin_popcountll(block));
    }
    assert(bits == 7u);
    assert(pixel_entry->constant_map.float_count == 7u);
    // c103 (block 1 bit 39), c255 (block 3 bit 63) must be mapped.
    assert((pixel_entry->constant_map.float_bitmap[1] >> 39u) & 1u);
    assert((pixel_entry->constant_map.float_bitmap[3] >> 63u) & 1u);
  }

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // Texture fetch constant t0: same bounded setup as the r258 e2e.
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 6u, 0x4800u));
    ring.push_back(2u | (2u << 10u) | (2u << 13u) | (8u << 22u));
    ring.push_back(6u | (2u << 6u) | (0x100u << 12u));
    ring.push_back(255u | (255u << 13u));
    ring.push_back((83u << 1u) | (1u << 19u) | (1u << 21u) | (1u << 23u));
    ring.push_back(0u);
    ring.push_back(1u << 9u);
  }
  // Vertex fetch vf0 (group 47).
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 0x48BEu));
    ring.push_back(3u | (0x1000u << 2u));
    ring.push_back(2u | (16u << 2u));
  }
  // The REAL guest float constants the ucode consumes (pixel bank 0x4400):
  // c103 = c104 = c106 = 1 (registers 0x459C/0x45A0/0x45A8), c101 = c102 =
  // c105 = 0 (default), c255 = (2, 1, 1, 1) (register 0x47FC).
  {
    const std::uint32_t one = 0x3F800000u;
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 4u, 0x459Cu));
    for (std::uint32_t i = 0u; i < 4u; ++i) ring.push_back(one);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 4u, 0x45A0u));
    for (std::uint32_t i = 0u; i < 4u; ++i) ring.push_back(one);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 4u, 0x45A8u));
    for (std::uint32_t i = 0u; i < 4u; ++i) ring.push_back(one);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 4u, 0x47FCu));
    ring.push_back(0x40000000u);
    for (std::uint32_t i = 0u; i < 3u; ++i) ring.push_back(one);
  }
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 0x2100u));
    ring.push_back(0xFFFFFFFFu);
    ring.push_back(0u);
  }
  {
    const std::uint32_t bits[6] = {
        0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u, 0x3F800000u, 0u,
    };
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 6u, 0x210Fu));
    for (const std::uint32_t value : bits) ring.push_back(value);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2206u));
    ring.push_back(0x3Fu);
  }
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2208u));
    ring.push_back(4u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2000u));
    ring.push_back(1280u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2001u));
    ring.push_back(16u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2104u));
    ring.push_back(0xFu);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 0x200Eu));
    ring.push_back(0u);
    ring.push_back(720u << 16u | 1280u);
  }
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  assert(bridge.pump(state, output).ok());
  assert(output.size() == 4u);

  const float vertex_xy[3][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
  std::vector<std::uint8_t> vertex_bytes(3u * 16u, 0u);
  for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
    for (std::uint32_t component = 0u; component < 2u; ++component) {
      std::uint32_t bits = 0u;
      std::memcpy(&bits, &vertex_xy[vertex][component], sizeof(bits));
      const std::uint32_t swapped = __builtin_bswap32(bits);
      std::memcpy(vertex_bytes.data() + vertex * 16u + component * 4u, &swapped,
                  sizeof(swapped));
    }
  }
  assert(runtime.write_shared_memory(0x1000u, vertex_bytes));

  const std::uint32_t texture_dwords = 256u * 256u;
  const auto make_texture = [](std::uint8_t r, std::uint8_t g, std::uint8_t b,
                               std::uint8_t a) {
    std::vector<std::uint8_t> bytes(texture_dwords * 4u);
    for (std::uint32_t i = 0u; i < texture_dwords; ++i) {
      bytes[i * 4u + 0u] = a;
      bytes[i * 4u + 1u] = b;
      bytes[i * 4u + 2u] = g;
      bytes[i * 4u + 3u] = r;
    }
    return bytes;
  };
  const auto run_frame = [&](const std::vector<std::uint8_t>& texture_bytes) {
    assert(runtime.write_shared_memory(0x100000u / 4u, texture_bytes));
    assert(runtime.execute_frame(target, state, output));
    return target.readback();
  };
  // Texture A (64,255,128,255): sample.g = 1, sample.b = 128/255 ->
  // export = (sample.g, sample.g, sample.b, 1) = (255, 255, 128, 255).
  // Texture B (255,0,0,255): export = (0, 0, 0, 255).
  const auto pixels_a = run_frame(make_texture(64u, 255u, 128u, 255u));
  const auto pixels_b = run_frame(make_texture(255u, 0u, 0u, 255u));
  assert(pixels_a.size() == 1280u * 720u * 4u);
  assert(runtime.draw_count() == 2u);
  const std::size_t probe = (361u * 1280u + 639u) * 4u;
  // With the constants at their real registers the gamma-decode chain
  // reconstructs the sampled texel exactly (recorded exact values,
  // deterministic across runs): non-zero AND texture-dependent.
  assert(pixels_a[probe + 0u] == 64u);
  assert(pixels_a[probe + 1u] == 255u);
  assert(pixels_a[probe + 2u] == 128u);
  assert(pixels_a[probe + 3u] == 255u);
  assert(pixels_b[probe + 0u] == 255u);
  assert(pixels_b[probe + 1u] == 0u);
  assert(pixels_b[probe + 2u] == 0u);
  assert(pixels_b[probe + 3u] == 255u);
}

}  // namespace

void pinned_shader_selects_modification_by_draw_state() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_shader_selects_modification_by_draw_state: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_shader_selects_modification_by_draw_state: %s\n",
                 runtime.error().c_str());
    return;
  }
  // The multi-modification pinned pair: vertex 645F413EAFC41025 (digest
  // 0x8ff8fbdbe488f0cd) and pixel A08446C8BF5844A2 (digest
  // 0x277cef29c95619ae, 15 dwords, two variants differing ONLY in the
  // depth-stencil mode: 0x0000000000000001 = kNoModifiers vs
  // 0x0000400000000001 = kEarlyHint). The pixel shader is
  //   tfetch2D r0, r0.xy, tf0; max oC0, r0, r0
  // - the export is the sample itself, so the variants are color-equivalent
  // by design and the test verifies STATE-DRIVEN SELECTION (the selected
  // modification flips with RB_COLORCONTROL alpha state) plus exact pixels.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0x8ff8fbdbe488f0cdull;  // 645F413EAFC41025
  constexpr std::uint64_t kPixelDigest = 0x277cef29c95619aeull;  // A08446C8BF5844A2
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  std::uint32_t pixel_variants = 0u;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest &&
        entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest &&
        entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
      ++pixel_variants;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);
  assert(pixel_variants == 2u);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // Texture fetch constant t0 (registers 0x4800..0x4805): as in the texture
  // test (linear, k_8_8_8_8, k8in32, guest base 0x100000, 256x256).
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 6u,
                                            0x4800u));
    ring.push_back(2u | (2u << 10u) | (2u << 13u) | (8u << 22u));
    ring.push_back(6u | (2u << 6u) | (0x100u << 12u));
    ring.push_back(255u | (255u << 13u));
    ring.push_back((83u << 1u) | (1u << 19u) | (1u << 21u) | (1u << 23u));
    ring.push_back(0u);
    ring.push_back(1u << 9u);
  }
  // Vertex fetch constant vf0 (registers 0x48BE/0x48BF).
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u,
                                            0x48BEu));
    ring.push_back(3u | (0x1000u << 2u));
    ring.push_back(2u | (16u << 2u));
  }
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 0x2100u));
    ring.push_back(0xFFFFFFFFu);
    ring.push_back(0u);
  }
  {
    const std::uint32_t bits[6] = {
        0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u, 0x3F800000u, 0u,
    };
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 6u, 0x210Fu));
    for (const std::uint32_t value : bits) ring.push_back(value);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2206u));
    ring.push_back(0x3Fu);
  }
  {
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2208u));
    ring.push_back(4u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2000u));
    ring.push_back(1280u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2001u));
    ring.push_back(16u);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2104u));
    ring.push_back(0xFu);
    ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 2u, 0x200Eu));
    ring.push_back(0u);
    ring.push_back(720u << 16u | 1280u);
  }
  // Frame 1: RB_COLORCONTROL = 0 (alpha test OFF) -> coverage does not
  // depend on alpha -> the derived modification is kEarlyHint.
  ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2202u));
  ring.push_back(0u);
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);
  const float vertex_xy[3][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
  std::vector<std::uint8_t> vertex_bytes(3u * 16u, 0u);
  for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
    for (std::uint32_t component = 0u; component < 2u; ++component) {
      std::uint32_t bits = 0u;
      std::memcpy(&bits, &vertex_xy[vertex][component], sizeof(bits));
      const std::uint32_t swapped = __builtin_bswap32(bits);
      std::memcpy(vertex_bytes.data() + vertex * 16u + component * 4u, &swapped,
                  sizeof(swapped));
    }
  }
  assert(runtime.write_shared_memory(0x1000u, vertex_bytes));

  // One solid texture for both frames: the export is the clamped edge texel
  // exactly, in BOTH variants (color-equivalent by design).
  const std::uint32_t texture_dwords = 256u * 256u;
  std::vector<std::uint8_t> texture_bytes(texture_dwords * 4u);
  for (std::uint32_t i = 0u; i < texture_dwords; ++i) {
    // Guest bytes are stored reversed (k8in32): a, b, g, r.
    texture_bytes[i * 4u + 0u] = 255u;  // a
    texture_bytes[i * 4u + 1u] = 128u;  // b
    texture_bytes[i * 4u + 2u] = 255u;  // g
    texture_bytes[i * 4u + 3u] = 64u;   // r
  }
  assert(runtime.write_shared_memory(0x100000u / 4u, texture_bytes));

  const bool ok = runtime.execute_frame(target, state, output);
  if (!ok) {
    std::fprintf(stderr, "MOD FRAME ERR: %s\n", runtime.error().c_str());
  }
  assert(ok);
  const auto pixels_a = target.readback();
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  // Frame 1: alpha test OFF -> kEarlyHint (dword1 bit 14 set).
  std::fprintf(stderr, "R260 mod frame 1 = 0x%016llx (expect 0x0000400000000001)\n",
               static_cast<unsigned long long>(runtime.last_pixel_modification()));
  assert(runtime.last_pixel_modification() == 0x0000400000000001ull);
  assert(!pixels_a.empty());
  const std::size_t probe = (361u * 1280u + 639u) * 4u;
  assert(pixels_a[probe + 0u] == 64u);
  assert(pixels_a[probe + 1u] == 255u);
  assert(pixels_a[probe + 2u] == 128u);
  assert(pixels_a[probe + 3u] == 255u);
  // Frame 2: RB_COLORCONTROL alpha test ON (func = always = 7 -> 0xF) ->
  // coverage depends on alpha -> kNoModifiers. The decoded register state
  // is the pump-final snapshot, so frame 2 is a separate pump phase.
  ring.clear();
  ring.push_back(ac6::native::pm4::header(ac6::native::pm4::kType0, 1u, 0x2202u));
  ring.push_back(0xFu);
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes_2 = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes_2));
  std::vector<ac6::native::XenosCommand> output_2;
  auto pump_result_2 = bridge.pump(state, output_2);
  assert(pump_result_2.ok());
  assert(output_2.size() == 2u);
  const bool ok2 = runtime.execute_frame(target, state, output_2);
  if (!ok2) {
    std::fprintf(stderr, "MOD FRAME2 ERR: %s\n", runtime.error().c_str());
  }
  assert(ok2);
  const auto pixels_b = target.readback();
  assert(runtime.draw_count() == 2u);
  assert(runtime.edram_resolves() == 2u);
  // Frame 2: alpha test ON -> kNoModifiers.
  std::fprintf(stderr, "R260 mod frame 2 = 0x%016llx (expect 0x0000000000000001)\n",
               static_cast<unsigned long long>(runtime.last_pixel_modification()));
  assert(runtime.last_pixel_modification() == 0x0000000000000001ull);
  assert(pixels_b[probe + 0u] == 64u);
  assert(pixels_b[probe + 1u] == 255u);
  assert(pixels_b[probe + 2u] == 128u);
  assert(pixels_b[probe + 3u] == 255u);
}

void pinned_edram_draws_accumulate_and_reconfig_recovers() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_edram_draws_accumulate_and_reconfig_recovers: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_edram_draws_accumulate_and_reconfig_recovers: %s\n",
                 runtime.error().c_str());
    return;
  }
  // The same pinned pair as the real-frame test: VS 6034746566624A37 +
  // PS AB5C776B8479A29A, whose export is (c255.x, 1, 1, 1) - the compacted
  // slot 0 is the REAL pixel register c255 (0x47FC), so the per-draw color
  // varies with the guest register value.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xe74686edb2236614ull;  // 6034746566624A37
  constexpr std::uint64_t kPixelDigest = 0x240522311d02461bull;   // AB5C776B8479A29A
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  // Phase 1 ring: shader loads + the full r257 register-driven render
  // target setup (base tile 16 -> region rows 16..736) + c255.x = 1 +
  // draw triangle V1 (covers screen pixel (640, 360)) + swap.
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (16u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});  // c255.x = 1 -> white
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  auto pump_and_execute = [&](std::uint32_t expected_commands) {
    const std::uint32_t ring_bytes =
        static_cast<std::uint32_t>(ring.size()) * 4u;
    ring.resize(128u, 0u);
    bridge.set_ring_words(ring);
    assert(bus.write(ac6::native::MmioBus::kRingSize, 512u));
    assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
    assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
    std::vector<ac6::native::XenosCommand> output;
    auto pump_result = bridge.pump(state, output);
    assert(pump_result.ok());
    assert(output.size() == expected_commands);
    const bool ok = runtime.execute_frame(target, state, output);
    if (!ok) {
      std::fprintf(stderr, "EDRAM FRAME ERR: %s\n", runtime.error().c_str());
    }
    assert(ok);
    ring.clear();
    return target.readback();
  };
  // Vertex data: 3 big-endian float pairs at dword address 0x1000.
  auto write_vertices = [&](const float xy[3][2]) {
    std::vector<std::uint8_t> vertex_bytes(3u * 16u, 0u);
    for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
      for (std::uint32_t component = 0u; component < 2u; ++component) {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &xy[vertex][component], sizeof(bits));
        const std::uint32_t swapped = __builtin_bswap32(bits);
        std::memcpy(vertex_bytes.data() + vertex * 16u + component * 4u,
                    &swapped, sizeof(swapped));
      }
    }
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  };
  const float v1[3][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
  // V2 = V1 shifted +500 guest pixels (+500/640 in clip X): covers screen
  // pixel (1140, 360), disjoint from V1's (640, 360).
  const float v2[3][2] = {{0.28125f, -0.5f}, {1.28125f, -0.5f},
                          {0.78125f, 0.5f}};

  // Phase 1: base tile 16, white at (640, 360). Fresh surface: CLEAR.
  write_vertices(v1);
  const auto pixels_1 = pump_and_execute(4u);
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  const std::size_t p1 = (360u * 1280u + 640u) * 4u;
  assert(pixels_1[p1 + 0u] == 255u);
  assert(pixels_1[p1 + 1u] == 255u);
  assert(pixels_1[p1 + 2u] == 255u);
  assert(pixels_1[p1 + 3u] == 255u);

  // Phase 2: SAME render-target registers (base tile 16) -> the surface
  // must LOAD (accumulate), not clear. Gray at (1140, 360).
  write_vertices(v2);
  push_type0(0x47FCu, {0x3F000000u, 0u, 0u, 0u});  // c255.x = 0.5 -> 127
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const auto pixels_2 = pump_and_execute(2u);
  assert(runtime.draw_count() == 2u);
  assert(runtime.edram_resolves() == 2u);
  const std::size_t p2 = (360u * 1280u + 1140u) * 4u;
  // The phase-1 white pixel SURVIVED the phase-2 draw (LOAD pass, not
  // CLEAR): EDRAM accumulates disjoint draws on the same configuration.
  assert(pixels_2[p1 + 0u] == 255u);
  assert(pixels_2[p1 + 1u] == 255u);
  assert(pixels_2[p1 + 2u] == 255u);
  assert(pixels_2[p1 + 3u] == 255u);
  // The float->unorm8 conversion truncates: 0.5 * 255 = 127.5 -> 127.
  assert(pixels_2[p2 + 0u] == 127u);
  assert(pixels_2[p2 + 1u] == 255u);
  assert(pixels_2[p2 + 2u] == 255u);
  assert(pixels_2[p2 + 3u] == 255u);

  // Phase 3: reconfigure (base tile 32) -> a NEW surface: CLEAR, then the
  // V1 triangle (same screen position) is drawn into the new surface
  // (rows 32..752) with c255.x still 0.5 -> gray.
  write_vertices(v1);
  push_type0(0x2001u, {32u});
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const auto pixels_3 = pump_and_execute(2u);
  assert(runtime.draw_count() == 3u);
  assert(runtime.edram_resolves() == 3u);
  // c255.x is still 0.5 (unchanged registers), so the phase-3 triangle is
  // GRAY (127). The clear-on-reconfig proof: image row 200 (target row
  // 168) carried phase-1 WHITE at this exact tile (the tile-32 surface is
  // the tile-16 surface shifted by 16 image rows, so the phase-1 triangle
  // apex residue sits at rows 196..211 while the phase-3 triangle starts
  // at row 212) - only a fresh CLEAR wipes it to 0.
  assert(pixels_3[p1 + 0u] == 127u);
  assert(pixels_3[p1 + 1u] == 255u);
  assert(pixels_3[p1 + 2u] == 255u);
  assert(pixels_3[p1 + 3u] == 255u);
  const std::size_t residue = (168u * 1280u + 640u) * 4u;
  assert(pixels_3[residue + 0u] == 0u);
  assert(pixels_3[residue + 1u] == 0u);
  assert(pixels_3[residue + 2u] == 0u);
  assert(pixels_3[residue + 3u] == 0u);

  // Phase 4: reconfigure BACK to base tile 16 -> rt differs from the
  // active tile-32 target -> a NEW CLEAR of the tile-16 surface. Draw V1
  // with c255.x = 0.25 (dark red 64: observed float->unorm8 rounding).
  // If the stale LOAD content survived (bug), p2 would still be gray; the
  // re-CLEAR wipes it to the clear color.
  push_type0(0x2001u, {16u});
  push_type0(0x47FCu, {0x3E800000u, 0u, 0u, 0u});  // c255.x = 0.25 -> 64
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const auto pixels_4 = pump_and_execute(2u);
  assert(runtime.draw_count() == 4u);
  assert(runtime.edram_resolves() == 4u);
  assert(pixels_4[p1 + 0u] == 64u);
  assert(pixels_4[p1 + 1u] == 255u);
  assert(pixels_4[p1 + 2u] == 255u);
  assert(pixels_4[p1 + 3u] == 255u);
  // The phase-2 gray was wiped by the re-CLEAR (p2 is outside V1).
  assert(pixels_4[p2 + 0u] == 0u);
  assert(pixels_4[p2 + 1u] == 0u);
  assert(pixels_4[p2 + 2u] == 0u);
  assert(pixels_4[p2 + 3u] == 0u);

  // r572 reproduces the US transition: a draw with color writes and depth,
  // then draws with no attachment writes. None may erase the color image.
  // Keep executing every draw and test the reverse transition as well.
  const auto draw_and_swap = [&] {
    push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
                {4u | (2u << 6u) | (3u << 16u)});
    push_packet(ac6::native::pm4::kOpcodeXeSwap,
                {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
    return pump_and_execute(2u);
  };
  write_vertices(v1);
  push_type0(0x2002u, {0x000102D0u});  // observed D24FS8 at depth tile720
  push_type0(0x2200u, {0x00700764u});
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});
  const auto pixels_5 = draw_and_swap();
  assert(pixels_5[p1] == 255u && pixels_5[p1 + 1u] == 255u &&
         pixels_5[p1 + 2u] == 255u && pixels_5[p1 + 3u] == 255u);

  push_type0(0x2104u, {0u});
  push_type0(0x2200u, {0u});
  const auto pixels_6 = draw_and_swap();
  assert(pixels_6 == pixels_5);

  push_type0(0x2200u, {0x00700764u});
  const auto pixels_7 = draw_and_swap();
  assert(pixels_7 == pixels_5);

  // Restoring the color mask must LOAD too. Draw a disjoint triangle;
  // its new color appears while the earlier white triangle survives.
  write_vertices(v2);
  push_type0(0x2104u, {0xFu});
  push_type0(0x47FCu, {0x3F000000u, 0u, 0u, 0u});
  const auto pixels_8 = draw_and_swap();
  assert(pixels_8[p1] == 255u && pixels_8[p1 + 1u] == 255u &&
         pixels_8[p1 + 2u] == 255u && pixels_8[p1 + 3u] == 255u);
  assert(pixels_8[p2] == 127u && pixels_8[p2 + 1u] == 255u &&
         pixels_8[p2 + 2u] == 255u && pixels_8[p2 + 3u] == 255u);
  assert(runtime.draw_count() == 8u && runtime.edram_resolves() == 8u);
  std::fprintf(stderr, "r572 color preservation pixel regression PASS\n");
}

void pinned_indexed_draw_renders_guest_index_order() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_indexed_draw_renders_guest_index_order: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_indexed_draw_renders_guest_index_order: %s\n",
                 runtime.error().c_str());
    return;
  }
  // The same pinned pair (VS 60347465 + PS AB5C776B, export
  // (c255.x, 1, 1, 1)); base tile 16 as in the accumulation test.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xe74686edb2236614ull;
  constexpr std::uint64_t kPixelDigest = 0x240522311d02461bull;
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  // Phase 1 setup: shader loads, vf0, render target (base tile 16),
  // scissor, color mask, vertex range, viewport, c255.x = 1 (white).
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (16u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});  // c255.x = 1 -> white
  // DRAW_INDX (source 0 = guest index address in the packet): payload =
  // [prim | source<<6 | count<<16, index_address, stride | index_type<<11].
  auto push_draw_indx = [&](std::uint32_t count, std::uint32_t index_address,
                            std::uint32_t index_type_32bit) {
    // DRAW_INDX (source 0 = guest index address): the decoder models the
    // payload as [leading dword, VGT_DRAW_INITIATOR, index address,
    // stride | index type << 11] - four payload dwords.
    push_packet(ac6::native::pm4::kOpcodeDrawIndx,
                {0u, 4u | (count << 16u), index_address,
                 1u | (index_type_32bit << 11u)});
  };
  // Vertex data: 4 vertices - v0 (320,540), v1 (960,540), v2 (640,180),
  // v3 (960,180) in screen coordinates. Triangle A = (0,1,2) covers the
  // center (640,360); triangle B = (1,3,2) covers (853,300) and reuses
  // vertices 1 and 2 - a NON-sequential, shared-vertex index order.
  const float vertices[4][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f},
                                {0.5f, 0.5f}};
  {
    std::vector<std::uint8_t> vertex_bytes(4u * 16u, 0u);
    for (std::uint32_t vertex = 0u; vertex < 4u; ++vertex) {
      for (std::uint32_t component = 0u; component < 2u; ++component) {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &vertices[vertex][component], sizeof(bits));
        const std::uint32_t swapped = __builtin_bswap32(bits);
        std::memcpy(vertex_bytes.data() + vertex * 16u + component * 4u,
                    &swapped, sizeof(swapped));
      }
    }
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  }
  const std::size_t p_center = (360u * 1280u + 640u) * 4u;
  const std::size_t p_right = (300u * 1280u + 853u) * 4u;

  // Phase 1: 32-bit big-endian guest indices [0,1,2, 1,3,2] at dword
  // 0x2000 (guest byte address 0x8000). The pinned VS loads each index
  // dword from shared memory itself (kSysFlag_VertexIndexLoad path).
  {
    const std::uint32_t indices[6] = {0u, 1u, 2u, 1u, 3u, 2u};
    std::vector<std::uint8_t> index_bytes(6u * 4u);
    for (std::uint32_t i = 0u; i < 6u; ++i) {
      const std::uint32_t swapped = __builtin_bswap32(indices[i]);
      std::memcpy(index_bytes.data() + i * 4u, &swapped, sizeof(swapped));
    }
    assert(runtime.write_shared_memory(0x2000u, index_bytes));
  }
  push_draw_indx(6u, 0x8000u, 1u);
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);  // 2 shader loads + 1 draw + 1 present
  const bool ok = runtime.execute_frame(target, state, output);
  if (!ok) {
    std::fprintf(stderr, "IDX FRAME ERR: %s\n", runtime.error().c_str());
  }
  assert(ok);
  const auto pixels_1 = target.readback();
  assert(runtime.draw_count() == 1u);
  assert(runtime.present_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  // Triangle A at the center; triangle B on the right - BOTH from the ONE
  // indexed draw. Auto-indexed execution would render only triangle A
  // (indices 0..5 clamp beyond vertex 3), so the right pixel PROVES the
  // guest index order was really consumed.
  assert(pixels_1[p_center + 0u] == 255u);
  assert(pixels_1[p_center + 1u] == 255u);
  assert(pixels_1[p_center + 2u] == 255u);
  assert(pixels_1[p_center + 3u] == 255u);
  assert(pixels_1[p_right + 0u] == 255u);
  assert(pixels_1[p_right + 1u] == 255u);
  assert(pixels_1[p_right + 2u] == 255u);
  assert(pixels_1[p_right + 3u] == 255u);

  // Phase 2: the SAME triangles via 16-bit big-endian guest indices at
  // dword 0x2100 (byte 0x8400) - the host expansion + vkCmdDrawIndexed
  // path. New render target (base tile 32) so the readback is fresh.
  ring.clear();
  {
    const std::uint16_t indices[6] = {0, 1, 2, 1, 3, 2};
    std::vector<std::uint8_t> index_bytes(6u * 2u);
    for (std::uint32_t i = 0u; i < 6u; ++i) {
      const std::uint16_t swapped = __builtin_bswap16(indices[i]);
      std::memcpy(index_bytes.data() + i * 2u, &swapped, sizeof(swapped));
    }
    assert(runtime.write_shared_memory(0x2100u, index_bytes));
  }
  push_type0(0x2001u, {32u});
  push_draw_indx(6u, 0x8400u, 0u);
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes_2 =
      static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes_2));
  std::vector<ac6::native::XenosCommand> output_2;
  auto pump_result_2 = bridge.pump(state, output_2);
  assert(pump_result_2.ok());
  assert(output_2.size() == 2u);
  const bool ok_2 = runtime.execute_frame(target, state, output_2);
  if (!ok_2) {
    std::fprintf(stderr, "IDX FRAME2 ERR: %s\n", runtime.error().c_str());
  }
  assert(ok_2);
  const auto pixels_2 = target.readback();
  assert(runtime.draw_count() == 2u);
  assert(runtime.edram_resolves() == 2u);
  // The base tile 32 region resolves 1:1 (image rows 32..752 -> target
  // rows 0..719), so the same screen positions land at the same target
  // coordinates as phase 1.
  const std::size_t p_center_2 = p_center;
  const std::size_t p_right_2 = p_right;
  assert(pixels_2[p_center_2 + 0u] == 255u);
  assert(pixels_2[p_center_2 + 1u] == 255u);
  assert(pixels_2[p_center_2 + 2u] == 255u);
  assert(pixels_2[p_center_2 + 3u] == 255u);
  assert(pixels_2[p_right_2 + 0u] == 255u);
  assert(pixels_2[p_right_2 + 1u] == 255u);
  assert(pixels_2[p_right_2 + 2u] == 255u);
  assert(pixels_2[p_right_2 + 3u] == 255u);
}

void pinned_indexed_draw_honors_primitive_restart() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_indexed_draw_honors_primitive_restart: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_indexed_draw_honors_primitive_restart: %s\n",
                 runtime.error().c_str());
    return;
  }
  // The same pinned pair (VS 60347465 + PS AB5C776B, export
  // (c255.x, 1, 1, 1)); one indexed draw with a primitive restart in the
  // middle: [0,1,2, RESTART, 3,4,5]. Triangle A (0,1,2) covers the
  // center (640,360); triangle B (3,4,5) covers (1120,440). Without the
  // restart, the bridging triangle (2,3,4) = (960,540),(980,540),
  // (1120,300) would cover (975,520); with the restart honored, that
  // pixel stays at the clear color.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xe74686edb2236614ull;
  constexpr std::uint64_t kPixelDigest = 0x240522311d02461bull;
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  // Full setup: shader loads, vf0, render target (base tile 16), scissor,
  // color mask, vertex range, viewport, c255.x = 1 (white).
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (16u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});  // c255.x = 1 -> white
  auto push_draw_indx = [&](std::uint32_t count, std::uint32_t index_address,
                            std::uint32_t index_type_32bit) {
    push_packet(ac6::native::pm4::kOpcodeDrawIndx,
                {0u, 4u | (count << 16u), index_address,
                 1u | (index_type_32bit << 11u)});
  };
  // Six vertices: triangle A = (320,540),(640,180),(960,540); triangle B
  // = (980,540),(1120,300),(1260,540).
  const float vertices[6][2] = {{-0.5f, -0.5f}, {0.0f, 0.5f}, {0.5f, -0.5f},
                                {0.53125f, -0.5f}, {0.75f, 0.16667f},
                                {0.96875f, -0.5f}};
  {
    std::vector<std::uint8_t> vertex_bytes(6u * 16u, 0u);
    for (std::uint32_t vertex = 0u; vertex < 6u; ++vertex) {
      for (std::uint32_t component = 0u; component < 2u; ++component) {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &vertices[vertex][component], sizeof(bits));
        const std::uint32_t swapped = __builtin_bswap32(bits);
        std::memcpy(vertex_bytes.data() + vertex * 16u + component * 4u,
                    &swapped, sizeof(swapped));
      }
    }
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  }
  const std::size_t p_a = (360u * 1280u + 640u) * 4u;
  const std::size_t p_b = (440u * 1280u + 1120u) * 4u;
  // The bridging pixel: inside the hypothetical triangle (2,3,4), outside
  // both A and B.
  const std::size_t p_bridge = (520u * 1280u + 975u) * 4u;

  auto run_phase = [&](std::uint32_t expected_commands) {
    const std::uint32_t ring_bytes =
        static_cast<std::uint32_t>(ring.size()) * 4u;
    ring.resize(128u, 0u);
    bridge.set_ring_words(ring);
    assert(bus.write(ac6::native::MmioBus::kRingSize, 512u));
    assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
    assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
    std::vector<ac6::native::XenosCommand> output;
    auto pump_result = bridge.pump(state, output);
    assert(pump_result.ok());
    assert(output.size() == expected_commands);
    const bool ok = runtime.execute_frame(target, state, output);
    if (!ok) {
      std::fprintf(stderr, "RESTART FRAME ERR: %s\n", runtime.error().c_str());
    }
    assert(ok);
    ring.clear();
    return target.readback();
  };

  // Phase 1: 32-bit indices [0,1,2, 0xFFFFFFFF, 3,4,5] (big-endian
  // dwords at guest byte 0x8000). The restart splits the stream: only
  // triangles A and B render; the bridging triangle must NOT.
  {
    const std::uint32_t indices[7] = {0u, 1u, 2u, 0xFFFFFFFFu, 3u, 4u, 5u};
    std::vector<std::uint8_t> index_bytes(7u * 4u);
    for (std::uint32_t i = 0u; i < 7u; ++i) {
      const std::uint32_t swapped = __builtin_bswap32(indices[i]);
      std::memcpy(index_bytes.data() + i * 4u, &swapped, sizeof(swapped));
    }
    assert(runtime.write_shared_memory(0x2000u, index_bytes));
  }
  push_draw_indx(7u, 0x8000u, 1u);
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const auto pixels_1 = run_phase(4u);
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  assert(pixels_1[p_a + 0u] == 255u);
  assert(pixels_1[p_a + 1u] == 255u);
  assert(pixels_1[p_a + 2u] == 255u);
  assert(pixels_1[p_a + 3u] == 255u);
  assert(pixels_1[p_b + 0u] == 255u);
  assert(pixels_1[p_b + 1u] == 255u);
  assert(pixels_1[p_b + 2u] == 255u);
  assert(pixels_1[p_b + 3u] == 255u);
  // The bridging triangle (2,3,4) would cover (975,520); with the
  // restart honored it stays cleared.
  assert(pixels_1[p_bridge + 0u] == 0u);
  assert(pixels_1[p_bridge + 1u] == 0u);
  assert(pixels_1[p_bridge + 2u] == 0u);
  assert(pixels_1[p_bridge + 3u] == 0u);

  // Phase 2: the same stream with 16-bit big-endian indices and 0xFFFF
  // restarts, at guest byte 0x8400, new render target (base tile 32).
  ring.clear();
  {
    const std::uint16_t indices[7] = {0, 1, 2, 0xFFFF, 3, 4, 5};
    std::vector<std::uint8_t> index_bytes(7u * 2u);
    for (std::uint32_t i = 0u; i < 7u; ++i) {
      const std::uint16_t swapped = __builtin_bswap16(indices[i]);
      std::memcpy(index_bytes.data() + i * 2u, &swapped, sizeof(swapped));
    }
    assert(runtime.write_shared_memory(0x2100u, index_bytes));
  }
  push_type0(0x2001u, {32u});
  push_draw_indx(7u, 0x8400u, 0u);
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const auto pixels_2 = run_phase(2u);
  assert(runtime.draw_count() == 2u);
  assert(runtime.edram_resolves() == 2u);
  assert(pixels_2[p_a + 0u] == 255u);
  assert(pixels_2[p_a + 1u] == 255u);
  assert(pixels_2[p_a + 2u] == 255u);
  assert(pixels_2[p_a + 3u] == 255u);
  assert(pixels_2[p_b + 0u] == 255u);
  assert(pixels_2[p_b + 1u] == 255u);
  assert(pixels_2[p_b + 2u] == 255u);
  assert(pixels_2[p_b + 3u] == 255u);
  assert(pixels_2[p_bridge + 0u] == 0u);
  assert(pixels_2[p_bridge + 1u] == 0u);
  assert(pixels_2[p_bridge + 2u] == 0u);
  assert(pixels_2[p_bridge + 3u] == 0u);
}

void pinned_shader_samples_two_textures() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr, "SKIP pinned_shader_samples_two_textures: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr, "SKIP pinned_shader_samples_two_textures: %s\n",
                 runtime.error().c_str());
    return;
  }
  // The pinned pair: vertex 645F413EAFC41025 (exports interpolator 0 =
  // (1,1,1)) and pixel 21BBF77A724F041E (digest 0xd9e6558b3f27cb41, 21
  // dwords): tfetch2D r1.xyz, UV, tf1; tfetch2D r0, UV, tf0;
  // oC0 = (r1.xyz - r0.xyz)*r0.w + r0.xyz with w = 1 - the export is
  // texture t1's RGB when t0 is opaque. The r264 contract: TWO fetch
  // constants (t0 at registers 0x4800..0x4805, t1 at 0x4806..0x480B) and
  // two resident textures (guest byte 0x100000 and 0x180000).
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0x8ff8fbdbe488f0cdull;  // 645F413EAFC41025
  constexpr std::uint64_t kPixelDigest = 0xd9e6558b3f27cb41ull;  // 21BBF77A724F041E
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // Vertex fetch constant vf0 (registers 0x48BE/0x48BF; group 47 does not
  // collide with the texture fetch groups 0/1).
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (16u << 2u)});
  // Texture fetch constants t0 and t1: clamp-to-edge, pitch 256 pixels,
  // linear, k_8_8_8_8, k8in32, bases 0x100000 / 0x180000, 256x256.
  const auto push_tfetch = [&](std::uint32_t base_register,
                               std::uint32_t base_page) {
    push_type0(base_register,
               {2u | (2u << 10u) | (2u << 13u) | (8u << 22u),
                6u | (2u << 6u) | (base_page << 12u),
                255u | (255u << 13u),
                (83u << 1u) | (1u << 19u) | (1u << 21u) | (1u << 23u), 0u,
                1u << 9u});
  };
  push_tfetch(0x4800u, 0x100u);  // t0: guest byte 0x100000
  push_tfetch(0x4806u, 0x180u);  // t1: guest byte 0x180000
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);  // 2 shader loads + 1 draw + 1 present
  const float vertex_xy[3][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
  std::vector<std::uint8_t> vertex_bytes(3u * 16u, 0u);
  for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
    for (std::uint32_t component = 0u; component < 2u; ++component) {
      std::uint32_t bits = 0u;
      std::memcpy(&bits, &vertex_xy[vertex][component], sizeof(bits));
      const std::uint32_t swapped = __builtin_bswap32(bits);
      std::memcpy(vertex_bytes.data() + vertex * 16u + component * 4u, &swapped,
                  sizeof(swapped));
    }
  }
  assert(runtime.write_shared_memory(0x1000u, vertex_bytes));

  // Two solid textures (big-endian k8in32 guest bytes): t0 = opaque
  // (10,20,30,255), t1 frame 1 = (200,100,50,255), frame 2 = (1,2,3,255).
  const std::uint32_t texture_dwords = 256u * 256u;
  const auto make_texture = [](std::uint8_t r, std::uint8_t g, std::uint8_t b,
                               std::uint8_t a) {
    std::vector<std::uint8_t> bytes(texture_dwords * 4u);
    for (std::uint32_t i = 0u; i < texture_dwords; ++i) {
      // Guest bytes are stored reversed (k8in32): a, b, g, r.
      bytes[i * 4u + 0u] = a;
      bytes[i * 4u + 1u] = b;
      bytes[i * 4u + 2u] = g;
      bytes[i * 4u + 3u] = r;
    }
    return bytes;
  };
  auto run_frame = [&](const std::vector<std::uint8_t>& t1_bytes) {
    assert(runtime.write_shared_memory(0x100000u / 4u, make_texture(10u, 20u, 30u, 255u)));
    assert(runtime.write_shared_memory(0x180000u / 4u, t1_bytes));
    const bool ok = runtime.execute_frame(target, state, output);
    if (!ok) {
      std::fprintf(stderr, "TEX2 FRAME ERR: %s\n", runtime.error().c_str());
    }
    assert(ok);
    return target.readback();
  };
  const auto pixels_a = run_frame(make_texture(200u, 100u, 50u, 255u));
  const auto pixels_b = run_frame(make_texture(1u, 2u, 3u, 255u));
  assert(pixels_a.size() == 1280u * 720u * 4u);
  assert(runtime.draw_count() == 2u);
  assert(runtime.edram_resolves() == 2u);
  const std::size_t probe = (361u * 1280u + 639u) * 4u;
  // Frame 1: (B - A)*1 + A = B = (200,100,50,255). Frame 2: only t1
  // changed -> (1,2,3,255). If the second texture were not really
  // resident and bound (e.g. both bindings pointing at t0), frame 2
  // would stay (10,20,30,255).
  assert(pixels_a[probe + 0u] == 200u);
  assert(pixels_a[probe + 1u] == 100u);
  assert(pixels_a[probe + 2u] == 50u);
  assert(pixels_a[probe + 3u] == 255u);
  assert(pixels_b[probe + 0u] == 1u);
  assert(pixels_b[probe + 1u] == 2u);
  assert(pixels_b[probe + 2u] == 3u);
  assert(pixels_b[probe + 3u] == 255u);
}

void pinned_point_list_expands_to_triangle_strips() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_point_list_expands_to_triangle_strips: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_point_list_expands_to_triangle_strips: %s\n",
                 runtime.error().c_str());
    return;
  }
  // r265: a rectangle-list draw (guest primitive 0x08) with a pinned VS
  // whose qualified variant is host type 9 (kPointListAsTriangleStrip,
  // digest 0x9dd1c7cddae1141, 24 dwords, mod 0x900000000). The runtime
  // draws the oracle's two-triangle-strip builtin index buffer ((i<<2)+0..3
  // per primitive, primitive restarts between strips); the pinned VS loads
  // the guest index per primitive (k8in32, 32-bit) and expands a
  // constant-position quad (NDC (0,0)) by point_constant_diameter
  // (PA_SU_POINT_SIZE * 2/16 = 320 px) * point_screen_diameter_to_ndc_radius
  // (1/1280, 1/720) per strip corner. PS AB5C776B: export = (c255.x, 1, 1, 1)
  // with c255.x = 1 -> white. The quad spans x 480..800, y 200..520.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0x9dd1c7cddae1141ull;
  constexpr std::uint64_t kPixelDigest = 0x240522311d02461bull;  // AB5C776B
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // Vertex fetch constant vf0 (declared, unused by this ucode).
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (16u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});  // c255.x = 1 -> white
  // PA_SU_POINT_SIZE: width/height raw 2560 (12.4 fixed) = 320 px - the
  // constant quad diameter. PA_SU_POINT_MINMAX: min 0, max 0xFFFF.
  push_type0(0x2280u, {0x0A000A00u});
  push_type0(0x2281u, {0xFFFF0000u});
  // DRAW_INDX source 0, PRIMITIVE 0x01 (point list), 6 guest
  // vertices (6 points), 32-bit guest indices at guest byte 0x8000.
  {
    const std::uint32_t indices[6] = {0u, 1u, 2u, 3u, 4u, 5u};
    std::vector<std::uint8_t> index_bytes(6u * 4u);
    for (std::uint32_t i = 0u; i < 6u; ++i) {
      const std::uint32_t swapped = __builtin_bswap32(indices[i]);
      std::memcpy(index_bytes.data() + i * 4u, &swapped, sizeof(swapped));
    }
    assert(runtime.write_shared_memory(0x2000u, index_bytes));
  }
  push_packet(ac6::native::pm4::kOpcodeDrawIndx,
              {0u, 1u | (6u << 16u), 0x8000u, 1u | (1u << 11u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);  // 2 shader loads + 1 draw + 1 present
  const bool ok = runtime.execute_frame(target, state, output);
  if (!ok) {
    std::fprintf(stderr, "RECT FRAME ERR: %s\n", runtime.error().c_str());
  }
  assert(ok);
  const auto pixels = target.readback();
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  // Each point expands to a 320x320 px quad centered at (640, 360)
  // (the six strips overlap). Inside = white; outside = clear.
  const auto expect = [&](std::uint32_t x, std::uint32_t y, std::uint8_t v) {
    const std::size_t off = (y * 1280u + x) * 4u;
    assert(pixels[off + 0u] == v);
    assert(pixels[off + 1u] == v);
    assert(pixels[off + 2u] == v);
    assert(pixels[off + 3u] == (v == 0u ? 0u : 255u));
  };
  expect(640u, 360u, 255u);   // quad center
  expect(500u, 360u, 255u);   // inside left
  expect(780u, 360u, 255u);   // inside right
  expect(470u, 360u, 0u);     // outside left
  expect(810u, 360u, 0u);     // outside right
  expect(640u, 190u, 0u);     // outside top
  expect(640u, 530u, 0u);     // outside bottom
}


void pinned_rectangle_reconstructs_three_vertices() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_rectangle_reconstructs_three_vertices: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_rectangle_reconstructs_three_vertices: %s\n",
                 runtime.error().c_str());
    return;
  }
  // Three guest positions define an axis-aligned rectangle. Sampling the
  // opposite corner distinguishes reconstruction from drawing one triangle
  // or expanding a point using PA_SU_POINT_SIZE.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0x57b8e5f14b93cff4ull;
  constexpr std::uint64_t kPixelDigest = 0x240522311d02461bull;  // AB5C776B
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u && entry.modification == 0xA00000000ull) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // The qualified SPIR-V reads fetch words 0/1 (cmap bitmap bit 0).
  // The guest disassembly name vf95 is remapped by the translator.
  push_type0(0x4800u, {3u | (0x1000u << 2u), 2u | (21u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});  // c255.x = 1 -> white
  push_type0(0x2280u, {0x00100010u});  // tiny points must not define rectangle size
  push_type0(0x2281u, {0xFFFF0000u});
  const float vertices[3][7] = {
      {-0.5f, -0.5f, 0.5f, 1.f, 1.f, 1.f, 1.f},
      { 0.5f, -0.5f, 0.5f, 1.f, 1.f, 1.f, 1.f},
      {-0.5f,  0.5f, 0.5f, 1.f, 1.f, 1.f, 1.f},
  };
  std::vector<std::uint8_t> vertex_bytes(sizeof(vertices));
  for (std::size_t i = 0; i < sizeof(vertices) / 4u; ++i) {
    std::uint32_t word;
    std::memcpy(&word, reinterpret_cast<const std::uint8_t*>(vertices) + i * 4u, 4u);
    word = __builtin_bswap32(word);
    std::memcpy(vertex_bytes.data() + i * 4u, &word, 4u);
  }
  assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2, {8u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);  // 2 shader loads + 1 draw + 1 present
  const bool ok = runtime.execute_frame(target, state, output);
  if (!ok) {
    std::fprintf(stderr, "RECT FRAME ERR: %s\n", runtime.error().c_str());
  }
  assert(ok);
  const auto pixels = target.readback();
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  const auto expect = [&](std::uint32_t x, std::uint32_t y, std::uint8_t v) {
    const std::size_t off = (y * 1280u + x) * 4u;
    if (pixels[off] != v) {
      std::uint32_t min_x = 1280u, min_y = 720u, max_x = 0u, max_y = 0u;
      std::size_t lit = 0u;
      for (std::uint32_t py = 0u; py < 720u; ++py) {
        for (std::uint32_t px = 0u; px < 1280u; ++px) {
          if (pixels[(py * 1280u + px) * 4u] != 0u) {
            ++lit;
            min_x = std::min(min_x, px); min_y = std::min(min_y, py);
            max_x = std::max(max_x, px); max_y = std::max(max_y, py);
          }
        }
      }
      std::fprintf(stderr, "rectangle pixel (%u,%u) expected=%u actual=%u; lit=%zu bounds=(%u,%u)-(%u,%u)\n",
                   x, y, unsigned(v), unsigned(pixels[off]), lit, min_x, min_y, max_x, max_y);
    }
    assert(pixels[off + 0u] == v);
    assert(pixels[off + 1u] == v);
    assert(pixels[off + 2u] == v);
    assert(pixels[off + 3u] == (v == 0u ? 0u : 255u));
  };
  expect(640u, 360u, 255u);
  expect(900u, 220u, 255u);  // fourth-corner region outside the source triangle
  expect(380u, 500u, 255u);
  expect(300u, 360u, 0u);
  expect(980u, 360u, 0u);
  expect(640u, 150u, 0u);
  expect(640u, 560u, 0u);
}

// r490: the rectangle frame parameterized over the EDRAM surface and depth
// configuration, so the qualified pitch/MSAA mapping and the depth subset
// are exercised end-to-end against the same pinned shaders. expect_ok
// false means execute_frame must fail closed with a recorded error.
static void run_rectangle_surface_frame(std::uint32_t surface_info,
                                        std::uint32_t color_base,
                                        std::uint32_t depth_control,
                                        std::uint32_t depth_info,
                                        bool expect_ok,
                                        std::uint64_t pixel_digest =
                                            0x240522311d02461bull) {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr, "SKIP run_rectangle_surface_frame: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr, "SKIP run_rectangle_surface_frame: %s\n",
                 runtime.error().c_str());
    return;
  }
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0x57b8e5f14b93cff4ull;
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest &&
        entry.signature.shader_type == 0u &&
        entry.modification == 0xA00000000ull) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == pixel_digest &&
        entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  push_type0(0x4800u, {3u | (0x1000u << 2u), 2u | (21u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {surface_info});
  push_type0(0x2001u, {color_base});
  push_type0(0x2002u, {depth_info});
  push_type0(0x2200u, {depth_control});
  push_type0(0x210Du, {0x000000FFu});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});
  push_type0(0x2280u, {0x00100010u});
  push_type0(0x2281u, {0xFFFF0000u});
  const float vertices[3][7] = {
      {-0.5f, -0.5f, 0.5f, 1.f, 1.f, 1.f, 1.f},
      { 0.5f, -0.5f, 0.5f, 1.f, 1.f, 1.f, 1.f},
      {-0.5f,  0.5f, 0.5f, 1.f, 1.f, 1.f, 1.f},
  };
  std::vector<std::uint8_t> vertex_bytes(sizeof(vertices));
  for (std::size_t i = 0; i < sizeof(vertices) / 4u; ++i) {
    std::uint32_t word;
    std::memcpy(&word, reinterpret_cast<const std::uint8_t*>(vertices) + i * 4u, 4u);
    word = __builtin_bswap32(word);
    std::memcpy(vertex_bytes.data() + i * 4u, &word, 4u);
  }
  assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2, {8u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);
  const bool ok = runtime.execute_frame(target, state, output);
  if (!expect_ok) {
    assert(!ok);
    assert(!runtime.error().empty());
    std::fprintf(stderr, "rectangle surface frame failed closed as expected: %s\n",
                 runtime.error().c_str());
    return;
  }
  if (!ok) {
    std::fprintf(stderr, "RECT SURFACE FRAME ERR: %s\n",
                 runtime.error().c_str());
  }
  assert(ok);
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  const auto pixels = target.readback();
  assert(pixels.size() == static_cast<std::size_t>(1280u) * 720u * 4u);
  std::size_t lit = 0u;
  std::uint32_t min_x = 1280u, min_y = 720u, max_x = 0u, max_y = 0u;
  for (std::uint32_t y = 0u; y < 720u; ++y) {
    for (std::uint32_t x = 0u; x < 1280u; ++x) {
      const std::size_t off = (y * 1280u + x) * 4u;
      if (pixels[off] != 0u) {
        ++lit;
        min_x = std::min(min_x, x); min_y = std::min(min_y, y);
        max_x = std::max(max_x, x); max_y = std::max(max_y, y);
      }
    }
  }
  const auto expect = [&](std::uint32_t x, std::uint32_t y, std::uint8_t v) {
    const std::size_t off = (y * 1280u + x) * 4u;
    if (pixels[off] != v) {
      std::fprintf(stderr, "rect surface pixel (%u,%u) expected=%u actual=%u lit=%zu bounds=(%u,%u)-(%u,%u)\n",
                   x, y, unsigned(v), unsigned(pixels[off]), lit, min_x, min_y,
                   max_x, max_y);
    }
    assert(pixels[off] == v);
  };
  // Robust interior/exterior sample points: they must not flip under the
  // surface origin or the MSAA sample pattern (edges are far away).
  expect(640u, 360u, 255u);
  expect(700u, 300u, 255u);
  expect(100u, 100u, 0u);
  expect(1180u, 620u, 0u);
  assert(lit > 10000u);
  assert(min_x >= 100u && max_x <= 1180u && min_y >= 100u && max_y <= 620u);
}

void pinned_rectangle_msaa4_pitch_matches_swap_dims() {
  // r490: pitch 640 pixels at 4x MSAA maps to a 1280-sample-wide EDRAM
  // image (the oracle's GetSurfacePitchTiles), so the 1280x720 swap
  // resolves the drawn frame 1:1 instead of squashing a 640x4096 region.
  run_rectangle_surface_frame(640u | (2u << 16u), 0u, 0u, 0u, true);
}

void pinned_rectangle_depth_always_writes_d24s8() {
  // r490: the retail entry-path depth subset (RB_DEPTHCONTROL 0x8777:
  // z test + z write + stencil test with ALWAYS funcs; D24S8 at the color
  // base) renders the same pixels with the depth attachment bound.
  run_rectangle_surface_frame(1280u, 16u, 0x8777u, 16u, true);
}

void pinned_rectangle_d24fs8_depth_below_color_frame() {
  // r491: the retail r490 configuration -- D24FS8 (format bit 1) at depth
  // base 0x2d0 with 4x MSAA pitch 640, i.e. the depth surface sits directly
  // below the 720-row color frame once the oracle's pitch mapping applies.
  // The depth-only second pass must place depth content there while color
  // pixels stay unchanged.
  run_rectangle_surface_frame(640u | (2u << 16u), 0u, 0x8777u,
                              0x2d0u | (1u << 16u), true);
}

// r495: the exact retail early-boot vertex bytes (r495 probe, draw 2 at
// vf_addr 0x126c0138, three 7-dword records: v0=(-0.5,-0.5,1.0) black,
// v1=v2=(1.0,639.5) red-transparent) render ZERO fragments through the
// corrected pairing -- the guest's own early-boot rect quads are
// degenerate (v1+v2-v0 synthesis with coincident v1/v2), consistent with
// the r476 oracle evidence that this window shows a diagnostic screen,
// not title content. The visible-content gate is the guest's main-thread
// livelock (draw-54 plateau), not the renderer.
static void run_retail_vertex_bytes_diagnostic() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr, "SKIP run_retail_vertex_bytes_diagnostic: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr, "SKIP run_retail_vertex_bytes_diagnostic: %s\n",
                 runtime.error().c_str());
    return;
  }
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0x57b8e5f14b93cff4ull;
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest &&
        entry.signature.shader_type == 0u &&
        entry.modification == 0xA00000001ull) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == 0xe41b4b062083e5bfull &&
        entry.signature.shader_type == 1u &&
        entry.modification == 0x0000400000010001ull) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  push_type0(0x4800u, {3u | (0x1000u << 2u), 2u | (21u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  // Retail probe state (r495): vte=0x300 (screen-space passthrough),
  // vport 640/640/-464/368 (unused with VTX_*_FMT), scissor 8192x8192.
  push_type0(0x2206u, {0x300u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x200Eu, {0u, 0x20002000u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});
  push_type0(0x2280u, {0x00100010u});
  push_type0(0x2281u, {0xFFFF0000u});
  // The exact retail vertex dwords (big-endian words as stored in guest
  // memory): v0, v1, v2 = three 7-dword records.
  const std::uint32_t retail_words[21] = {
      0xbf000000u, 0xbf000000u, 0x3f800000u, 0x00000000u, 0x00000000u,
      0x00000000u, 0x00000000u, 0x3f800000u, 0x441fe000u, 0xbf000000u,
      0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u,
      0x441fe000u, 0x43b3c000u, 0x3f800000u, 0x00000000u, 0x00000000u,
      0x00000000u,
  };
  // Guest stores big-endian words; write_shared_memory takes the byte
  // stream as-is at dword address 0x1000>>2.
  std::vector<std::uint8_t> vertex_bytes(sizeof(retail_words));
  for (std::size_t i = 0; i < 21u; ++i) {
    const std::uint32_t word = __builtin_bswap32(retail_words[i]);
    std::memcpy(vertex_bytes.data() + i * 4u, &word, 4u);
  }
  assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2, {8u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  const bool ok = runtime.execute_frame(target, state, output);
  if (!ok) {
    std::fprintf(stderr, "RETAIL BYTES FRAME ERR: %s\n",
                 runtime.error().c_str());
  }
  assert(ok);
  const auto pixels = target.readback();
  std::size_t lit = 0u;
  std::uint32_t min_x = 1280u, min_y = 720u, max_x = 0u, max_y = 0u;
  for (std::uint32_t y = 0u; y < 720u; ++y) {
    for (std::uint32_t x = 0u; x < 1280u; ++x) {
      const std::size_t off = (y * 1280u + x) * 4u;
      if (pixels[off] != 0u || pixels[off + 1u] != 0u || pixels[off + 2u] != 0u) {
        ++lit;
        min_x = std::min(min_x, x); min_y = std::min(min_y, y);
        max_x = std::max(max_x, x); max_y = std::max(max_y, y);
      }
    }
  }
  std::fprintf(stderr,
               "r495 retail-bytes diagnostic: lit=%zu bounds=(%u,%u)-(%u,%u)\n",
               lit, min_x, min_y, max_x, max_y);
  assert(lit == 0u);
}

void pinned_rectangle_pair_exports_interpolator_color() {
  // r495: the retail title pairing (rect VS 0A6D + pinned PS e41b, which
  // exports interpolator_0 * exp_bias) needs the VS modification's
  // interpolator mask from the paired PS -- with mask 0 the type-10
  // translation declared no xe_out_interpolator_0 and the screen stayed
  // black. The fetched vertex color (1,1,1,1) must now reach the target.
  run_rectangle_surface_frame(1280u, 16u, 0u, 0u, true, 0xe41b4b062083e5bfull);
}

void pinned_depth_unqualified_configs_fail_closed() {
  // Backface stencil state enabled.
  run_rectangle_surface_frame(1280u, 16u, 0x8777u | 0x80u, 16u, false);
  // Depth region exceeding the tile-pitched surface (base 0xff0 maps below
  // the 2048-row image at 1x pitch 1280).
  run_rectangle_surface_frame(1280u, 16u, 0x8777u, 0xff0u, false);
}

void pinned_edram_format_2_10_10_10_accepted_and_reconfigures() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_edram_format_2_10_10_10_accepted_and_reconfigures: "
                 "no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_edram_format_2_10_10_10_accepted_and_reconfigures: "
                 "%s\n",
                 runtime.error().c_str());
    return;
  }
  // r266: RB_COLOR_INFO color format k_2_10_10_10 (2) is qualified
  // alongside k_8_8_8_8 (0); the EdramRenderTarget carries the format, so
  // a format change is a reconfiguration (fresh CLEAR), exactly like a
  // base-tile change. Same pinned pair as the accumulation test.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xe74686edb2236614ull;
  constexpr std::uint64_t kPixelDigest = 0x240522311d02461bull;
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  auto push_draw = [&]() {
    push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
                {4u | (2u << 6u) | (3u << 16u)});
    push_packet(ac6::native::pm4::kOpcodeXeSwap,
                {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  };
  auto run_phase = [&](std::uint32_t expected_commands) {
    const std::uint32_t ring_bytes =
        static_cast<std::uint32_t>(ring.size()) * 4u;
    ring.resize(256u, 0u);
    bridge.set_ring_words(ring);
    assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
    assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
    assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
    std::vector<ac6::native::XenosCommand> output;
    auto pump_result = bridge.pump(state, output);
    assert(pump_result.ok());
    assert(output.size() == expected_commands);
    const bool ok = runtime.execute_frame(target, state, output);
    if (!ok) {
      std::fprintf(stderr, "FMT FRAME ERR: %s\n", runtime.error().c_str());
    }
    assert(ok);
    ring.clear();
    return target.readback();
  };

  // Phase 1: format k_8_8_8_8 (0), base tile 16, c255.x = 1, triangle V1
  // covering (640, 360).
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (16u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});  // c255.x = 1 -> white
  push_draw();
  const float v1[3][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
  const float v2[3][2] = {{0.28125f, -0.5f}, {1.28125f, -0.5f},
                          {0.78125f, 0.5f}};
  const auto write_vertices = [&](const float xy[3][2]) {
    std::vector<std::uint8_t> vertex_bytes(3u * 16u, 0u);
    for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
      for (std::uint32_t component = 0u; component < 2u; ++component) {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &xy[vertex][component], sizeof(bits));
        const std::uint32_t swapped = __builtin_bswap32(bits);
        std::memcpy(vertex_bytes.data() + vertex * 16u + component * 4u,
                    &swapped, sizeof(swapped));
      }
    }
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  };
  write_vertices(v1);
  const auto pixels_1 = run_phase(4u);
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  const std::size_t p1 = (360u * 1280u + 640u) * 4u;
  assert(pixels_1[p1 + 0u] == 255u);
  assert(pixels_1[p1 + 3u] == 255u);

  // Phase 2: SAME base tile, SAME registers otherwise, but the color
  // format changes to k_2_10_10_10 (2) -> a NEW render target (fresh
  // CLEAR). Triangle V2 covers (1140, 360); the phase-1 white pixel at
  // (640, 360) must be wiped to the clear color.
  write_vertices(v2);
  push_type0(0x2001u, {16u | (2u << 16u)});  // base 16, format 2
  push_draw();
  const auto pixels_2 = run_phase(2u);
  assert(runtime.draw_count() == 2u);
  assert(runtime.edram_resolves() == 2u);
  const std::size_t p2 = (360u * 1280u + 1140u) * 4u;
  assert(pixels_2[p2 + 0u] == 255u);
  assert(pixels_2[p2 + 3u] == 255u);
  assert(pixels_2[p1 + 0u] == 0u);
  assert(pixels_2[p1 + 3u] == 0u);
}

void pinned_cube_texture_samples_expected_face_texels() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_cube_texture_samples_expected_face_texels: no "
                 "device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_cube_texture_samples_expected_face_texels: %s\n",
                 runtime.error().c_str());
    return;
  }
  // r270: the pinned cube-texture path end-to-end. The pixel shader
  // (digest 0x6668fb603e255007, mod 0x40007) samples fetch constant t0
  // (a cube: dword5 bits 9:10 = 3) through a 2D-array image with the six
  // faces stacked sequentially in guest memory, and t1 (2D) through the
  // same slot machinery. Guest k8in32 texture bytes are stored reversed
  // per dword (the sampled texel = the byte-swapped memory dword), so
  // the cube memory bytes (255, 64, 0, 255) sample as (1, 0, 64/255, 1)
  // and the 2D memory bytes (0, 255, 128, 255) sample as (1, 128/255,
  // 1, 0). Derived export at the probe pixel (the full SPIR-V chain,
  // fc0 = fc1 = 0, fc2 = (0,2,0,0), fc3 = 1): B = (2*t1y*texel0.x,
  // 2*t1z*texel0.y, 0) = (1.0039, 0, 0); C = B * interp2.xyz with
  // interp2 = (1, 128/255, 0, 1) = (1.0039, 0, 0); the export predicate
  // (max(C) <= fc2.y) holds, so export = (clamp(C), interp2.w * A) with
  // A = max(texel0.w, fc0.x) = 1 -> (255, 0, 0, 255).
  // The vertex shader (0xf5de355f7c3cf70, mod 0x7) exports the three
  // interpolators the pixel shader consumes from 10-dword vertex records
  // (in-shader index load, k8in32): position (f0,f1,f2) through the
  // identity matrix rows c218-c221, D3DCOLOR bytes at d5 -> interpolator
  // 2, floats at d6-d9 -> interpolators 0.xy / 1.xy.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xf5de355f7c3cf70ull;
  constexpr std::uint64_t kPixelDigest = 0x6668fb603e255007ull;
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u &&
        entry.modification == 0x7ull) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u &&
        entry.modification == 0x40007ull) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  auto write_f32 = [](std::uint32_t* dst, float value) {
    std::uint32_t bits = 0u;
    std::memcpy(&bits, &value, sizeof(bits));
    const std::uint32_t swapped = __builtin_bswap32(bits);
    std::memcpy(dst, &swapped, sizeof(swapped));
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // t0 = cube fetch constant (dimension 3 in dword5 bits 9:10, 4x4 faces,
  // pitch 32 pixels, k_8_8_8_8, k8in32, base 0x10000, point filters, no
  // lod/exp bias); t1 = 2D (dimension 1, base 0x11000). Same word3 as the
  // qualified 2D path (swizzle 83, linear filters, mip point).
  const std::uint32_t fetch_word3 =
      (83u << 1u) | (1u << 19u) | (1u << 21u) | (1u << 23u);
  push_type0(0x4800u, {2u | (1u << 22u), 6u | (2u << 6u) | (0x10u << 12u),
                       3u | (3u << 13u), fetch_word3, 0u, 3u << 9u});
  push_type0(0x4806u, {2u | (1u << 22u), 6u | (2u << 6u) | (0x11u << 12u),
                       3u | (3u << 13u), fetch_word3, 0u, 1u << 9u});
  // Vertex fetch constant 31 (the in-shader record descriptor): base
  // 0x1000 dwords, endian k8in32, size 10 dwords.
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (10u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});  // VGT max / min vertex index
  push_type0(0x2102u, {0u});               // VGT_INDX_OFFSET
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  // Vertex float constants c218-c221 = the identity matrix rows.
  // The full family's remaining compact-slot constants (the w/half
  // carriers: c169/c171 = (0,0,0,1) give the fetched positions w = 1,
  // c215 = (1,0,0,0); the rest are neutral zeros) - without them the
  // fetched w collapses to 0 and the draw clips away.
  push_type0(0x4000u + 130u * 4u, {0u, 0u, 0u, 0u});
  push_type0(0x4000u + 168u * 4u, {0u, 0u, 0u, 0u});
  push_type0(0x4000u + 169u * 4u, {0u, 0u, 0u, 0x3F800000u});
  push_type0(0x4000u + 170u * 4u, {0u, 0u, 0u, 0u});
  push_type0(0x4000u + 171u * 4u, {0u, 0u, 0u, 0x3F800000u});
  push_type0(0x4000u + 212u * 4u, {0u, 0u, 0u, 0u});
  push_type0(0x4000u + 213u * 4u, {0u, 0u, 0u, 0u});
  push_type0(0x4000u + 214u * 4u, {0u, 0u, 0u, 0u});
  push_type0(0x4000u + 215u * 4u, {0x3F800000u, 0u, 0u, 0u});
  push_type0(0x4000u + 218u * 4u, {0x3F800000u, 0u, 0u, 0u});
  push_type0(0x4000u + 219u * 4u, {0u, 0x3F800000u, 0u, 0u});
  push_type0(0x4000u + 220u * 4u, {0u, 0u, 0x3F800000u, 0u});
  push_type0(0x4000u + 221u * 4u, {0u, 0u, 0u, 0x3F800000u});
  // Pixel float constants (the compacted rows 0-3 = c128/c129/c254/c255):
  // c128 = c129 = 0, c254 = (0, 2, 0, 0) (the export predicate bound),
  // c255 = (1, 1, 1, 1) (the export scale).
  push_type0(0x4400u + 128u * 4u, {0u, 0u, 0u, 0u});
  push_type0(0x4400u + 129u * 4u, {0u, 0u, 0u, 0u});
  push_type0(0x4400u + 254u * 4u, {0u, 0x40000000u, 0u, 0u});
  push_type0(0x4400u + 255u * 4u, {0x3F800000u, 0x3F800000u, 0x3F800000u,
                                   0x3F800000u});
  // Vertex records (10 dwords each, big-endian k8in32) at byte 0x4000:
  // d0-d2 = the NDC position, d5 = the D3DCOLOR bytes (255,128,0,255),
  // d6/d7 = interpolator 0.xy (the cube UV), d8/d9 = interpolator 1.xy
  // (the t1 UV).
  const float positions[3][3] = {{0.0f, 0.5f, 0.0f},
                                 {0.5f, -0.5f, 0.0f},
                                 {-0.5f, -0.5f, 0.0f}};
  {
    std::vector<std::uint8_t> vertex_bytes(3u * 40u, 0u);
    for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
      std::uint32_t* words =
          reinterpret_cast<std::uint32_t*>(vertex_bytes.data() + vertex * 40u);
      write_f32(words + 0u, positions[vertex][0]);
      write_f32(words + 1u, positions[vertex][1]);
      write_f32(words + 2u, positions[vertex][2]);
      words[5] = 0xFF8000FFu;  // bytes (255, 128, 0, 255)
      write_f32(words + 6u, 0.25f);
      write_f32(words + 7u, 0.25f);
      write_f32(words + 8u, 0.25f);
      write_f32(words + 9u, 0.25f);
    }
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  }
  // Guest index buffer: [0, 1, 2] big-endian 32-bit at byte 0x6000.
  {
    const std::uint32_t indices[3] = {0u, 1u, 2u};
    std::vector<std::uint8_t> index_bytes(3u * 4u);
    for (std::uint32_t i = 0u; i < 3u; ++i) {
      const std::uint32_t swapped = __builtin_bswap32(indices[i]);
      std::memcpy(index_bytes.data() + i * 4u, &swapped, sizeof(swapped));
    }
    assert(runtime.write_shared_memory(0x6000u / 4u, index_bytes));
  }
  // Cube texels: six 4x4 faces (pitch 32 px = 128 B/row), all texels
  // (64, 128, 192, 255). 2D texels at 0x11000: (0, 128, 128, 255).
  {
    std::vector<std::uint8_t> cube(6u * 4u * 128u);
    for (std::size_t i = 0u; i < cube.size(); i += 4u) {
      // Guest k8in32: memory bytes reversed -> sampled (255, 0, 64, 255).
      cube[i + 0u] = 255u;
      cube[i + 1u] = 64u;
      cube[i + 2u] = 0u;
      cube[i + 3u] = 255u;
    }
    assert(runtime.write_shared_memory(0x10000u / 4u, cube));
    std::vector<std::uint8_t> flat(4u * 128u);
    for (std::size_t i = 0u; i < flat.size(); i += 4u) {
      // Sampled as (255, 128, 255, 0).
      flat[i + 0u] = 0u;
      flat[i + 1u] = 255u;
      flat[i + 2u] = 128u;
      flat[i + 3u] = 255u;
    }
    assert(runtime.write_shared_memory(0x11000u / 4u, flat));
  }
  // DRAW_INDX source 0: triangle list, 3 vertices, 32-bit guest indices
  // (the pinned VS loads each index itself and reads the 10-dword record).
  push_packet(ac6::native::pm4::kOpcodeDrawIndx,
              {0u, 4u | (3u << 16u), 0x6000u, 1u | (1u << 11u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);  // 2 shader loads + 1 draw + 1 present
  const bool ok = runtime.execute_frame(target, state, output);
  if (!ok) {
    std::fprintf(stderr, "CUBE FRAME ERR: %s\n", runtime.error().c_str());
  }
  assert(ok);
  const auto pixels = target.readback();
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  const std::size_t probe = (360u * 1280u + 640u) * 4u;
  assert(pixels[probe + 0u] == 255u);   // clamp(2 * (128/255) * 1) = 1
  assert(pixels[probe + 1u] == 0u);     // 2 * 1 * 0 = 0
  assert(pixels[probe + 2u] == 0u);     // r1.w = 0
  assert(pixels[probe + 3u] == 255u);   // interp2.w * max(texel0.w, 0) = 1
  // Outside the triangle: the clear color.
  const std::size_t outside = (300u * 1280u + 100u) * 4u;
  assert(pixels[outside + 0u] == 0u);
  assert(pixels[outside + 3u] == 0u);
}

void pinned_edram_format_8_8_8_8_gamma_renders_like_8_8_8_8() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_edram_format_8_8_8_8_gamma_renders_like_8_8_"
                 "8_8: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_edram_format_8_8_8_8_gamma_renders_like_8_8_"
                 "8_8: %s\n",
                 runtime.error().c_str());
    return;
  }
  // r269: RB_COLOR_INFO color format k_8_8_8_8_GAMMA (1) is qualified
  // (the oracle's GetColorVulkanFormat maps it to VK_FORMAT_R8G8B8A8_UNORM,
  // byte-identical storage with format 0, so the copy resolve path
  // applies). Phase 1 renders format 0; phase 2 switches to format 1
  // (a fresh CLEAR, like every reconfiguration). Phase 3 proves a
  // non-qualified format (5 = k_16_16_16_16) still fails closed.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xe74686edb2236614ull;
  constexpr std::uint64_t kPixelDigest = 0x240522311d02461bull;
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  auto push_draw = [&]() {
    push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
                {4u | (2u << 6u) | (3u << 16u)});
    push_packet(ac6::native::pm4::kOpcodeXeSwap,
                {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  };
  auto run_phase = [&](std::uint32_t expected_commands) {
    const std::uint32_t ring_bytes =
        static_cast<std::uint32_t>(ring.size()) * 4u;
    ring.resize(256u, 0u);
    bridge.set_ring_words(ring);
    assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
    assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
    assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
    std::vector<ac6::native::XenosCommand> output;
    auto pump_result = bridge.pump(state, output);
    assert(pump_result.ok());
    assert(output.size() == expected_commands);
    const bool ok = runtime.execute_frame(target, state, output);
    ring.clear();
    return std::pair<bool, std::vector<std::uint8_t>>(ok, ok ? target.readback() : std::vector<std::uint8_t>());
  };

  // Shader loads and the shared state.
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (16u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});  // c255.x = 1 -> white
  const float v1[3][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
  const float v2[3][2] = {{0.28125f, -0.5f}, {1.28125f, -0.5f},
                          {0.78125f, 0.5f}};
  const auto write_vertices = [&](const float xy[3][2]) {
    std::vector<std::uint8_t> vertex_bytes(3u * 16u, 0u);
    for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
      for (std::uint32_t component = 0u; component < 2u; ++component) {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &xy[vertex][component], sizeof(bits));
        const std::uint32_t swapped = __builtin_bswap32(bits);
        std::memcpy(vertex_bytes.data() + vertex * 16u + component * 4u,
                    &swapped, sizeof(swapped));
      }
    }
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  };
  write_vertices(v1);
  push_draw();
  const auto phase_1 = run_phase(4u);
  assert(phase_1.first);
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  const std::size_t p1 = (360u * 1280u + 640u) * 4u;
  assert(phase_1.second[p1 + 0u] == 255u);
  assert(phase_1.second[p1 + 3u] == 255u);

  // Phase 2: format k_8_8_8_8_GAMMA (1) at the same base tile -> a NEW
  // render target (fresh CLEAR). Triangle V2 covers (1140, 360); the
  // phase-1 white pixel at (640, 360) is wiped. The byte-identical
  // storage means the copy resolve path renders exact white exactly
  // like format 0.
  write_vertices(v2);
  push_type0(0x2001u, {16u | (1u << 16u)});  // base 16, format 1
  push_draw();
  const auto phase_2 = run_phase(2u);
  assert(phase_2.first);
  assert(runtime.draw_count() == 2u);
  assert(runtime.edram_resolves() == 2u);
  const std::size_t p2 = (360u * 1280u + 1140u) * 4u;
  assert(phase_2.second[p2 + 0u] == 255u);
  assert(phase_2.second[p2 + 3u] == 255u);
  assert(phase_2.second[p1 + 0u] == 0u);
  assert(phase_2.second[p1 + 3u] == 0u);

  // Phase 3: format 5 (k_16_16_16_16, not qualified) must fail closed.
  push_type0(0x2001u, {16u | (5u << 16u)});
  push_draw();
  const auto phase_3 = run_phase(2u);
  assert(!phase_3.first);
  assert(!runtime.error().empty());
}

void pinned_edram_format_16_16_16_16_float_renders_and_resolves() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_edram_format_16_16_16_16_float_renders_and_"
                 "resolves: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_edram_format_16_16_16_16_float_renders_and_"
                 "resolves: %s\n",
                 runtime.error().c_str());
    return;
  }
  // r268: RB_COLOR_INFO color format k_16_16_16_16_FLOAT (7) is
  // qualified: the EDRAM surface becomes VK_FORMAT_R16G16B16A16_SFLOAT
  // (the oracle's GetColorVulkanFormat mapping) with its own clear/load
  // render-pass pair and pipelines (float formats are not render-pass
  // compatible with the UNORM target), and the XE_SWAP resolve converts
  // via a blit instead of a byte copy. Phase 1 renders format 0; phase 2
  // switches to format 7 (a fresh CLEAR, like every reconfiguration) and
  // must still present exact white through the float surface + blit.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xe74686edb2236614ull;
  constexpr std::uint64_t kPixelDigest = 0x240522311d02461bull;
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  auto push_draw = [&]() {
    push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
                {4u | (2u << 6u) | (3u << 16u)});
    push_packet(ac6::native::pm4::kOpcodeXeSwap,
                {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  };
  auto run_phase = [&](std::uint32_t expected_commands) {
    const std::uint32_t ring_bytes =
        static_cast<std::uint32_t>(ring.size()) * 4u;
    ring.resize(256u, 0u);
    bridge.set_ring_words(ring);
    assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
    assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
    assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
    std::vector<ac6::native::XenosCommand> output;
    auto pump_result = bridge.pump(state, output);
    assert(pump_result.ok());
    assert(output.size() == expected_commands);
    const bool ok = runtime.execute_frame(target, state, output);
    if (!ok) {
      std::fprintf(stderr, "F16 FRAME ERR: %s\n", runtime.error().c_str());
    }
    assert(ok);
    ring.clear();
    return target.readback();
  };

  // Shader loads and the shared state (base tile 16, c255.x = 1, the
  // screen-to-NDC scale, triangle V1 / V2 vertex data as in the format
  // reconfiguration test).
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (16u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});  // c255.x = 1 -> white
  const float v1[3][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
  const float v2[3][2] = {{0.28125f, -0.5f}, {1.28125f, -0.5f},
                          {0.78125f, 0.5f}};
  const auto write_vertices = [&](const float xy[3][2]) {
    std::vector<std::uint8_t> vertex_bytes(3u * 16u, 0u);
    for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
      for (std::uint32_t component = 0u; component < 2u; ++component) {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &xy[vertex][component], sizeof(bits));
        const std::uint32_t swapped = __builtin_bswap32(bits);
        std::memcpy(vertex_bytes.data() + vertex * 16u + component * 4u,
                    &swapped, sizeof(swapped));
      }
    }
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  };
  write_vertices(v1);
  push_draw();
  const auto pixels_1 = run_phase(4u);
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  const std::size_t p1 = (360u * 1280u + 640u) * 4u;
  assert(pixels_1[p1 + 0u] == 255u);
  assert(pixels_1[p1 + 3u] == 255u);

  // Phase 2: format k_16_16_16_16_FLOAT (7) at the same base tile -> a
  // NEW surface in VK_FORMAT_R16G16B16A16_SFLOAT (fresh CLEAR). Triangle
  // V2 covers (1140, 360); the phase-1 pixel at (640, 360) is wiped.
  // The phase-2 white pixel must survive the float surface AND the blit
  // conversion at resolve exactly.
  write_vertices(v2);
  push_type0(0x2001u, {16u | (7u << 16u)});  // base 16, format 7
  push_draw();
  const auto pixels_2 = run_phase(2u);
  assert(runtime.draw_count() == 2u);
  assert(runtime.edram_resolves() == 2u);
  const std::size_t p2 = (360u * 1280u + 1140u) * 4u;
  assert(pixels_2[p2 + 0u] == 255u);
  assert(pixels_2[p2 + 3u] == 255u);
  assert(pixels_2[p1 + 0u] == 0u);
  assert(pixels_2[p1 + 3u] == 0u);
}

void pinned_point_loop_variant_expands_per_vertex_quads() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_point_loop_variant_expands_per_vertex_quads: no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_point_loop_variant_expands_per_vertex_quads: %s\n",
                 runtime.error().c_str());
    return;
  }
  // r267: the loop-variant pinned point-expansion VS (host type 9,
  // digest 0xec556475b428778f, 51 dwords, mod 0x900010001) exercised
  // end-to-end: 12-byte vertex records ([position scalar], [diameter
  // source], [packed rgb]), the compacted float slots c104 (position
  // scale (-2, -2)), c106 (predicate, w = 0) and c255, the diameter
  // clamp against PA_SU_POINT_MINMAX, and the per-corner expansion by
  // the clamped diameter * (1/1280, 1/720). PS AB5C776B: export =
  // (c255.x, 1, 1, 1) with c255.x = 1 -> white.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xec556475b428778full;
  constexpr std::uint64_t kPixelDigest = 0x240522311d02461bull;  // AB5C776B
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // Vertex fetch constant vf0: type kVertex, address 0x1000 dwords,
  // endian k8in32, size 12 dwords (the ucode fetches 12-byte records).
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (12u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  // The pinned VS's compacted float slots (vertex bank, cN at
  // 0x4000 + 4N): slot 0 = c104 = (-2, -2, -) (position scale),
  // slot 1 = c106 = 0 (predicate false), slot 2 = c255 = 0.
  push_type0(0x4000u + 104u * 4u, {0xBF000000u, 0xBF000000u});  // (-2, -2)
  push_type0(0x4000u + 106u * 4u, {0u});
  push_type0(0x4000u + 255u * 4u, {0u});
  // PA_SU_POINT_SIZE (unused: the diameter comes from the vertex data)
  // and PA_SU_POINT_MINMAX (the clamp: min 0, max 8191.875 px).
  push_type0(0x2280u, {0x0A000A00u});
  push_type0(0x2281u, {0xFFFF0000u});
  // Pixel c255.x = 1 (white export).
  push_type0(0x47FCu, {0x3F800000u, 0u, 0u, 0u});
  // Vertex data: 6 x 12-byte records at dword 0x1000, all identical:
  // position scalar 0.0 (quad center = NDC (0,0) = (640, 360)), diameter
  // source 160.0 (2 * 160 = 320 px after the ucode doubling), packed rgb.
  {
    const std::uint32_t words[3] = {0x00000000u,     // 0.0f
                                    0x43200000u,     // 160.0f
                                    0x00FFFFFFu};    // packed rgb
    std::vector<std::uint8_t> vertex_bytes(6u * 12u, 0u);
    for (std::uint32_t vertex = 0u; vertex < 6u; ++vertex) {
      for (std::uint32_t word = 0u; word < 3u; ++word) {
        const std::uint32_t swapped = __builtin_bswap32(words[word]);
        std::memcpy(vertex_bytes.data() + vertex * 12u + word * 4u, &swapped,
                    sizeof(swapped));
      }
    }
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  }
  // Guest index buffer: [0,1,2, 3,4,5] as big-endian 32-bit words at
  // guest byte 0x8000.
  {
    const std::uint32_t indices[6] = {0u, 1u, 2u, 3u, 4u, 5u};
    std::vector<std::uint8_t> index_bytes(6u * 4u);
    for (std::uint32_t i = 0u; i < 6u; ++i) {
      const std::uint32_t swapped = __builtin_bswap32(indices[i]);
      std::memcpy(index_bytes.data() + i * 4u, &swapped, sizeof(swapped));
    }
    assert(runtime.write_shared_memory(0x2000u, index_bytes));
  }
  // DRAW_INDX source 0, PRIMITIVE 0x01 (point list), 6 guest
  // vertices, 32-bit guest indices.
  push_packet(ac6::native::pm4::kOpcodeDrawIndx,
              {0u, 1u | (6u << 16u), 0x8000u, 1u | (1u << 11u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);  // 2 shader loads + 1 draw + 1 present
  const bool ok = runtime.execute_frame(target, state, output);
  if (!ok) {
    std::fprintf(stderr, "RECTLOOP FRAME ERR: %s\n", runtime.error().c_str());
  }
  assert(ok);
  const auto pixels = target.readback();
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  // Each point expands to a 320x320 px quad centered at (640, 360)
  // (the six strips overlap). Inside = white; outside = clear.
  const auto expect = [&](std::uint32_t x, std::uint32_t y, std::uint8_t v) {
    const std::size_t off = (y * 1280u + x) * 4u;
    assert(pixels[off + 0u] == v);
    assert(pixels[off + 1u] == v);
    assert(pixels[off + 2u] == v);
    assert(pixels[off + 3u] == (v == 0u ? 0u : 255u));
  };
  expect(640u, 360u, 255u);   // quad center
  expect(500u, 360u, 255u);   // inside left
  expect(780u, 360u, 255u);   // inside right
  expect(470u, 360u, 0u);     // outside left
  expect(810u, 360u, 0u);     // outside right
  expect(640u, 190u, 0u);     // outside top
  expect(640u, 530u, 0u);     // outside bottom
}

// r523: tiled DXT4_5 untile matches the cited 2D layout. 8x8 texels =
// 2x2 blocks at pitch 32 blocks: hand-computed cited offsets are
// (0,0)->0, (1,0)->32, (0,1)->16, (1,1)->48, so tiled order is
// [b0, b2, b1, b3] and linear row-major order is [b0, b1, b2, b3].
// r524: block geometry + conservative footprint for the refused boot
// descriptor (64x64 texels, pitch 128): 16x16 blocks, pitch 32 blocks,
// 4,096 linear bytes in one 16 KiB tile. Fail-closed on the same
// bounds the untile helper enforces.
void tiled_dxt45_layout_bounds_footprint() {
  std::uint32_t wb = 0u, hb = 0u, pb = 0u;
  std::uint64_t linear = 0u, footprint = 0u;
  assert(ac6::native::tiled_dxt45_layout(64u, 64u, 128u, wb, hb, pb, linear,
                                         footprint));
  assert(wb == 16u);
  assert(hb == 16u);
  assert(pb == 32u);
  assert(linear == 4096u);
  assert(footprint == 16384u);
  assert(!ac6::native::tiled_dxt45_layout(0u, 64u, 128u, wb, hb, pb, linear,
                                          footprint));
  assert(!ac6::native::tiled_dxt45_layout(64u, 64u, 64u, wb, hb, pb, linear,
                                          footprint));
  assert(!ac6::native::tiled_dxt45_layout(66u, 64u, 128u, wb, hb, pb, linear,
                                          footprint));
  assert(!ac6::native::tiled_dxt45_layout(64u, 64u, 36u, wb, hb, pb, linear,
                                          footprint));
  assert(!ac6::native::tiled_dxt45_layout(8192u, 64u, 8192u, wb, hb, pb, linear,
                                          footprint));
}

// r526: quad-list index expansion ([a,b,c,d] -> [a,b,c,a,c,d] per
// quad, single run). Triangle lists have no bridging primitives.
void expand_quad_list_auto_produces_two_triangles() {
  std::vector<std::uint32_t> out{99u};
  assert(ac6::native::expand_quad_list_auto(8u, out));
  const std::vector<std::uint32_t> expected{0u, 1u, 2u, 0u, 2u, 3u,
                                            4u, 5u, 6u, 4u, 6u, 7u};
  assert(out == expected);
  assert(!ac6::native::expand_quad_list_auto(0u, out));
  assert(!ac6::native::expand_quad_list_auto(6u, out));
  assert(!ac6::native::expand_quad_list_auto((((1u << 20u) + 1u) * 4u), out));
}

void expand_quad_list_words_remaps_dma_and_refuses_restart() {
  const std::vector<std::uint32_t> guest{10u, 11u, 12u, 13u,
                                         20u, 21u, 22u, 23u};
  std::vector<std::uint32_t> out;
  assert(ac6::native::expand_quad_list_words(guest, out));
  const std::vector<std::uint32_t> expected{10u, 11u, 12u, 10u, 12u, 13u,
                                            20u, 21u, 22u, 20u, 22u, 23u};
  assert(out == expected);
  const std::vector<std::uint32_t> restarted{10u, 11u, 0xFFFFFFFFu, 13u};
  assert(!ac6::native::expand_quad_list_words(restarted, out));
  assert(out.empty());
  const std::vector<std::uint32_t> empty;
  assert(!ac6::native::expand_quad_list_words(empty, out));
}

void untile_tiled_dxt45_matches_documented_layout() {  std::vector<std::uint8_t> tiled(64u, 0u);
  for (std::uint32_t block = 0u; block < 4u; ++block) {
    // Tiled placement per the offsets above.
    const std::size_t at = block == 0u ? 0u : (block == 1u ? 32u : (block == 2u ? 16u : 48u));
    for (std::uint32_t byte = 0u; byte < 16u; ++byte) {
      tiled[at + byte] = static_cast<std::uint8_t>(block * 16u + byte);
    }
  }
  std::vector<std::uint8_t> linear;
  assert(ac6::native::untile_tiled_dxt45_2d(
      tiled.data(), tiled.size(), 2u, 2u, 32u, 0u, linear));
  assert(linear.size() == 64u);
  for (std::uint32_t block = 0u; block < 4u; ++block) {
    for (std::uint32_t byte = 0u; byte < 16u; ++byte) {
      assert(linear[block * 16u + byte] == static_cast<std::uint8_t>(block * 16u + byte));
    }
  }
  // k8in16 endianness pair-swaps each block on the way out (cited
  // CopySwapBlock): store swapped, expect unswapped linear.
  std::vector<std::uint8_t> tiled_swapped(64u, 0u);
  for (std::uint32_t block = 0u; block < 4u; ++block) {
    const std::size_t at = block == 0u ? 0u : (block == 1u ? 32u : (block == 2u ? 16u : 48u));
    for (std::uint32_t pair = 0u; pair < 8u; ++pair) {
      tiled_swapped[at + pair * 2u] = static_cast<std::uint8_t>(block * 16u + pair * 2u + 1u);
      tiled_swapped[at + pair * 2u + 1u] = static_cast<std::uint8_t>(block * 16u + pair * 2u);
    }
  }
  std::vector<std::uint8_t> linear_swapped;
  assert(ac6::native::untile_tiled_dxt45_2d(
      tiled_swapped.data(), tiled_swapped.size(), 2u, 2u, 32u, 1u, linear_swapped));
  assert(linear_swapped == linear);
  // Fail-closed: null, zero dims, pitch below width or off the
  // 32-block grid, unsupported endianness, short source.
  std::vector<std::uint8_t> out;
  assert(!ac6::native::untile_tiled_dxt45_2d(
      nullptr, 64u, 2u, 2u, 32u, 0u, out));
  assert(!ac6::native::untile_tiled_dxt45_2d(
      tiled.data(), tiled.size(), 0u, 2u, 32u, 0u, out));
  assert(!ac6::native::untile_tiled_dxt45_2d(
      tiled.data(), tiled.size(), 2u, 2u, 1u, 0u, out));
  assert(!ac6::native::untile_tiled_dxt45_2d(
      tiled.data(), tiled.size(), 2u, 2u, 4u, 0u, out));
  assert(!ac6::native::untile_tiled_dxt45_2d(
      tiled.data(), tiled.size(), 2u, 2u, 32u, 3u, out));
  assert(!ac6::native::untile_tiled_dxt45_2d(
      tiled.data(), 48u, 2u, 2u, 32u, 0u, out));
}

void tiled_dxt1_layout_bounds_footprint() {
  std::uint32_t wb = 0u, hb = 0u, pb = 0u;
  std::uint64_t linear = 0u, footprint = 0u;
  assert(ac6::native::tiled_dxt1_layout(64u, 64u, 128u, wb, hb, pb, linear,
                                        footprint));
  assert(wb == 16u);
  assert(hb == 16u);
  assert(pb == 32u);
  assert(linear == 2048u);
  assert(footprint == 8192u);
  assert(!ac6::native::tiled_dxt1_layout(0u, 64u, 128u, wb, hb, pb, linear,
                                         footprint));
  assert(!ac6::native::tiled_dxt1_layout(64u, 64u, 64u, wb, hb, pb, linear,
                                         footprint));
  assert(!ac6::native::tiled_dxt1_layout(66u, 64u, 128u, wb, hb, pb, linear,
                                         footprint));
  assert(!ac6::native::tiled_dxt1_layout(64u, 64u, 36u, wb, hb, pb, linear,
                                         footprint));
}

void untile_tiled_dxt1_matches_documented_layout() {
  // 2x2 blocks (8x8 texels), pitch 32 blocks, 8 bytes per block.
  // tiled_offset_2d with log2=3: (0,0)->0, (1,0)->8, (0,1)->16, (1,1)->24.
  std::vector<std::uint8_t> tiled(32u, 0u);
  for (std::uint32_t block = 0u; block < 4u; ++block) {
    const std::size_t at = block == 0u ? 0u : (block == 1u ? 8u : (block == 2u ? 16u : 24u));
    for (std::uint32_t byte = 0u; byte < 8u; ++byte) {
      tiled[at + byte] = static_cast<std::uint8_t>(block * 8u + byte);
    }
  }
  std::vector<std::uint8_t> linear;
  assert(ac6::native::untile_tiled_dxt1_2d(
      tiled.data(), tiled.size(), 2u, 2u, 32u, 0u, linear));
  assert(linear.size() == 32u);
  for (std::uint32_t block = 0u; block < 4u; ++block) {
    for (std::uint32_t byte = 0u; byte < 8u; ++byte) {
      assert(linear[block * 8u + byte] == static_cast<std::uint8_t>(block * 8u + byte));
    }
  }
  // k8in16 endianness pair-swaps each block.
  std::vector<std::uint8_t> tiled_swapped(32u, 0u);
  for (std::uint32_t block = 0u; block < 4u; ++block) {
    const std::size_t at = block == 0u ? 0u : (block == 1u ? 8u : (block == 2u ? 16u : 24u));
    for (std::uint32_t pair = 0u; pair < 4u; ++pair) {
      tiled_swapped[at + pair * 2u] = static_cast<std::uint8_t>(block * 8u + pair * 2u + 1u);
      tiled_swapped[at + pair * 2u + 1u] = static_cast<std::uint8_t>(block * 8u + pair * 2u);
    }
  }
  std::vector<std::uint8_t> linear_swapped;
  assert(ac6::native::untile_tiled_dxt1_2d(
      tiled_swapped.data(), tiled_swapped.size(), 2u, 2u, 32u, 1u, linear_swapped));
  assert(linear_swapped == linear);
  // Fail-closed: null, zero dims, pitch below width, off 32-block grid,
  // unsupported endianness, short source.
  std::vector<std::uint8_t> out;
  assert(!ac6::native::untile_tiled_dxt1_2d(
      nullptr, 32u, 2u, 2u, 32u, 0u, out));
  assert(!ac6::native::untile_tiled_dxt1_2d(
      tiled.data(), tiled.size(), 0u, 2u, 32u, 0u, out));
  assert(!ac6::native::untile_tiled_dxt1_2d(
      tiled.data(), tiled.size(), 2u, 2u, 1u, 0u, out));
  assert(!ac6::native::untile_tiled_dxt1_2d(
      tiled.data(), tiled.size(), 2u, 2u, 4u, 0u, out));
  assert(!ac6::native::untile_tiled_dxt1_2d(
      tiled.data(), tiled.size(), 2u, 2u, 32u, 3u, out));
  assert(!ac6::native::untile_tiled_dxt1_2d(
      tiled.data(), 24u, 2u, 2u, 32u, 0u, out));
}

void pinned_deswizzle_identity_renders_restore_pass() {  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_deswizzle_identity_renders_restore_pass: no "
                 "device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_deswizzle_identity_renders_restore_pass: %s\n",
                 runtime.error().c_str());
    return;
  }
  // r272: the AC6 deswizzle payload (KEEP decision r271) verified
  // end-to-end. The pinned pixel shader 0x7d22894002d16018 (mod
  // 0x100000000 = PsParamGen enabled + kNoModifiers) is a tiled restore
  // pass whose fetches were translated with the neutralize-deswizzle
  // identity: the sample UV is (FragCoord.x / width, FragCoord.y /
  // height) read from the fetch constant instead of the guest tile math.
  // With solid 4x4 clamp-to-edge textures every fragment therefore
  // samples the same texel. The traced export = (texel1.rgb, 0) (the
  // second texture only feeds the fragment depth). The draw enables the
  // alpha test (function ALWAYS) so the state-derived depth mode is
  // kNoModifiers and the r271 PsParamGen candidate matches the
  // modification. t1 samples as (64, 128, 192, 255) -> export
  // (64, 128, 192, 0).
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xe74686edb2236614ull;
  constexpr std::uint64_t kPixelDigest = 0x3fc7583b029d63b5ull;
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u &&
        entry.modification == 0x10000000000ull) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // Texture fetch constants t1 (registers 0x4806) and t2 (0x480C):
  // 2D, clamp-to-edge, pitch 256 px, linear, k_8_8_8_8, k8in32, no
  // lod/exp bias (the export exponent = 1), bases 0x100000 / 0x180000,
  // 4x4. The identity UV = FragCoord / (4, 4) clamps to the edge texel.
  const std::uint32_t fetch_word0 =
      2u | (2u << 10u) | (2u << 13u) | (8u << 22u);
  const std::uint32_t fetch_word3 =
      (83u << 1u) | (1u << 19u) | (1u << 21u) | (1u << 23u);
  push_type0(0x4806u, {fetch_word0, 6u | (2u << 6u) | (0x100u << 12u),
                       3u | (3u << 13u), fetch_word3, 0u, 1u << 9u});
  push_type0(0x480Cu, {fetch_word0, 6u | (2u << 6u) | (0x180u << 12u),
                       3u | (3u << 13u), fetch_word3, 0u, 1u << 9u});
  // Vertex fetch constant 31 (the in-shader record descriptor): base
  // 0x1000 dwords, endian k8in32, size 16 bytes.
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (16u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  // Alpha test ENABLED with function ALWAYS (RB_COLORCONTROL bit 3 + the
  // function 7): the pinned shader's kill passes and the state-derived
  // depth_stencil_mode is kNoModifiers, matching the pinned
  // modification's PsParamGen candidate (r271).
  push_type0(0x2206u, {0xFu});
  // Textures: t1 samples as (64, 128, 192, 255), t2 as (10, 20, 30, 40)
  // (guest k8in32 dwords are byte-reversed). Rows are 256 px wide; only
  // the first 4 texels of each row are reachable.
  {
    std::vector<std::uint8_t> tex1(4u * 1024u);
    for (std::size_t i = 0u; i < tex1.size(); i += 4u) {
      tex1[i + 0u] = 255u;  // a
      tex1[i + 1u] = 192u;  // b
      tex1[i + 2u] = 128u;  // g
      tex1[i + 3u] = 64u;   // r
    }
    assert(runtime.write_shared_memory(0x100000u / 4u, tex1));
    std::vector<std::uint8_t> tex2(4u * 1024u);
    for (std::size_t i = 0u; i < tex2.size(); i += 4u) {
      tex2[i + 0u] = 40u;
      tex2[i + 1u] = 30u;
      tex2[i + 2u] = 20u;
      tex2[i + 3u] = 10u;
    }
    assert(runtime.write_shared_memory(0x180000u / 4u, tex2));
  }
  // Triangle V1 (as in the qualified EDRAM tests) covers (640, 360).
  const float v1[3][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
  {
    std::vector<std::uint8_t> vertex_bytes(3u * 16u, 0u);
    for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
      for (std::uint32_t component = 0u; component < 2u; ++component) {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &v1[vertex][component], sizeof(bits));
        const std::uint32_t swapped = __builtin_bswap32(bits);
        std::memcpy(vertex_bytes.data() + vertex * 16u + component * 4u,
                    &swapped, sizeof(swapped));
      }
    }
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  }
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(256u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 1024u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);  // 2 shader loads + 1 draw + 1 present
  const bool ok = runtime.execute_frame(target, state, output);
  if (!ok) {
    std::fprintf(stderr, "DESWZ FRAME ERR: %s\n", runtime.error().c_str());
  }
  assert(ok);
  const auto pixels = target.readback();
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  // The identity-UV sample of the solid t1 texture at every covered
  // fragment = (64, 128, 192, 255); the export = (rgb, 0).
  const std::size_t probe = (360u * 1280u + 640u) * 4u;
  assert(pixels[probe + 0u] == 64u);
  assert(pixels[probe + 1u] == 128u);
  assert(pixels[probe + 2u] == 192u);
  assert(pixels[probe + 3u] == 0u);   // the export alpha = 0 by the trace
  // A second covered pixel: the identity holds everywhere in the tile.
  const std::size_t probe2 = (300u * 1280u + 630u) * 4u;
  assert(pixels[probe2 + 0u] == 64u);
  assert(pixels[probe2 + 1u] == 128u);
  assert(pixels[probe2 + 2u] == 192u);
  // Outside the triangle: the clear color.
  const std::size_t outside = (100u * 1280u + 100u) * 4u;
  assert(pixels[outside + 0u] == 0u);
  assert(pixels[outside + 3u] == 0u);
}

// r274: the second named deswizzle payload (oracle hash 17E5E4AC3E713245,
// capsule digest 0xb10dc382bef32209, mod 0x0000410000000000 = PsParamGen +
// early hint, scoped to fetch slot 4 only) verified end-to-end. The pinned
// shader is a tile-restore pipeline: fetch slot 4 is neutralized to the
// host identity UV (FragCoord / (width, height) read from the fetch
// constant's dword2) while the guest tile math still runs and feeds the
// other fetches (t13, t1, t0). The traced executed path (SPIR-V
// interpreter, p17.dis):
//   - r0 = the t4 texel x Ldexp(1, fc4 dword4 bits 13:19) = the texel with
//     exponent 0 (the qualified fetch word4 = 0);
//   - the first tile-iteration gate (c251.x == 0 && r0.w != 0) holds with
//     c251 = (0,1,1,1) -> t13 fetched through the guest tile coordinates;
//   - the t0 gate needs r1.z != 0 (the interpreter init) so t0 is skipped
//     in the first iteration; the t1 gate (r1.w == 0 && r1.z == 0) holds
//     with c252.w != t4.a -> t1 fetched through the guest coordinates;
//   - the cf exits to the countdown stages (22 -> 25 -> 26 -> 29 -> 30)
//     and the export = r0 = (t4.rgb, t4.a) x color_exp_bias (1) with the
//     gamma bit clear.
// With the solid 4x4 clamp-to-edge textures the guest-math coordinates are
// finite and clamp to the same texel, so the export = the t4 texel exactly:
// t4 memory (a,b,g,r) = (255,180,100,50) samples (50,100,180,255)/255 and
// exports (50,100,180,255). The draw runs with RB_COLORCONTROL = 0 (no
// alpha test) so the derived depth mode is kEarlyHint and the r271
// PsParamGen+early-hint candidate matches the pinned modification.
void pinned_deswizzle_slot4_payload_renders_tile_restore() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_deswizzle_slot4_payload_renders_tile_restore:"
                 " no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_deswizzle_slot4_payload_renders_tile_restore:"
                 " %s\n",
                 runtime.error().c_str());
    return;
  }
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xe74686edb2236614ull;
  constexpr std::uint64_t kPixelDigest = 0xb10dc382bef32209ull;
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest && entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest && entry.signature.shader_type == 1u &&
        entry.modification == 0x410000000000ull) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // Texture fetch constants t0 (0x4800), t1 (0x4806), t4 (0x4818) and
  // t13 (0x484E): 2D, clamp-to-edge, pitch 256 px, linear, k_8_8_8_8,
  // k8in32, no lod/exp bias (the export exponent = 1), 4x4, distinct
  // shared-memory bases. The t4 identity UV = FragCoord / (4, 4) clamps
  // to the edge texel; the t13/t1 guest tile-math coordinates are finite
  // and clamp to their edge texels.
  const std::uint32_t fetch_word0 =
      2u | (2u << 10u) | (2u << 13u) | (8u << 22u);
  const std::uint32_t fetch_word3 =
      (83u << 1u) | (1u << 19u) | (1u << 21u) | (1u << 23u);
  const struct { std::uint32_t reg; std::uint32_t base; } tex[4] = {
      {0x4800u, 0x80u}, {0x4806u, 0xC0u}, {0x4818u, 0x100u},
      {0x484Eu, 0x140u}};
  for (const auto& t : tex) {
    push_type0(t.reg, {fetch_word0, 6u | (2u << 6u) | (t.base << 12u),
                       3u | (3u << 13u), fetch_word3, 0u, 1u << 9u});
  }
  // Vertex fetch constant vf0: base 0x1000 dwords, endian k8in32,
  // 16-byte records.
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (16u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFu});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  // RB_COLORCONTROL = 0: alpha test AND alpha-to-coverage disabled -> the
  // derived depth mode is kEarlyHint (dword1 bit 14) and the r271
  // PsParamGen+early-hint candidate matches the pinned modification
  // 0x410000000000.
  push_type0(0x2202u, {0u});
  // Vertex float constants: the identity matrix rows c218-c221 plus the
  // w/half carriers (the full VS family needs them; zeros collapse the
  // fetched w to 0).
  {
    const std::uint32_t vc[13][5] = {
        {130u, 0u, 0u, 0u, 0u},
        {168u, 0u, 0u, 0u, 0u},
        {169u, 0u, 0u, 0u, 0x3F800000u},
        {170u, 0u, 0u, 0u, 0u},
        {171u, 0u, 0u, 0u, 0x3F800000u},
        {212u, 0u, 0u, 0u, 0u},
        {213u, 0u, 0u, 0u, 0u},
        {214u, 0u, 0u, 0u, 0u},
        {215u, 0x3F800000u, 0u, 0u, 0u},
        {218u, 0x3F800000u, 0u, 0u, 0u},
        {219u, 0u, 0x3F800000u, 0u, 0u},
        {220u, 0u, 0u, 0x3F800000u, 0u},
        {221u, 0u, 0u, 0u, 0x3F800000u}};
    for (const auto& cst : vc) {
      push_type0(0x4000u + 4u * cst[0], {cst[1], cst[2], cst[3], cst[4]});
    }
  }
  // Pixel float constants (compact slots 0..6 -> guests c249..c255, pixel
  // bank cN at 0x4400 + 4N). All (1,1,1,1) except c251 = (0,1,1,1) (the
  // tile-iteration gate: c251.x == 0 && r0.w != 0 continues past the t4
  // exit) and c252 = (1,1,1,0) (c252.w != t4.a makes r1.z = 0 so the t1
  // gate holds; the traced cf = 0 -> 22 -> 25 -> 26 -> 29 -> 30).
  {
    const struct { std::uint32_t c; float xyzw[4]; } pconst[7] = {
        {249u, {1.0f, 1.0f, 1.0f, 1.0f}},
        {250u, {1.0f, 1.0f, 1.0f, 1.0f}},
        {251u, {0.0f, 1.0f, 1.0f, 1.0f}},
        {252u, {1.0f, 1.0f, 1.0f, 0.0f}},
        {253u, {1.0f, 1.0f, 1.0f, 1.0f}},
        {254u, {1.0f, 1.0f, 1.0f, 1.0f}},
        {255u, {1.0f, 1.0f, 1.0f, 1.0f}}};
    for (const auto& cst : pconst) {
      std::uint32_t words[4];
      for (std::uint32_t j = 0u; j < 4u; ++j) {
        std::memcpy(&words[j], &cst.xyzw[j], sizeof(float));
      }
      push_type0(0x4400u + 4u * cst.c,
                 {words[0], words[1], words[2], words[3]});
    }
  }
  // Vertex records: 3 x 4 dwords (big-endian) at dword 0x1000:
  // positions (-0.5,0.5), (0.5,0.5), (0,-0.5) through the signed YScale.
  {
    const float records[3][4] = {{-0.5f, 0.5f, 0.25f, 0.6f},
                                 {0.5f, 0.5f, 0.5f, 0.6f},
                                 {0.0f, -0.5f, 0.75f, 0.6f}};
    std::vector<std::uint8_t> vertex_bytes(3u * 16u, 0u);
    for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
      for (std::uint32_t word = 0u; word < 4u; ++word) {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &records[vertex][word], sizeof(float));
        const std::uint32_t swapped = __builtin_bswap32(bits);
        std::memcpy(vertex_bytes.data() + vertex * 16u + word * 4u, &swapped,
                    4u);
      }
    }
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  }
  // Textures: solid 4x4 clamp-to-edge blocks. Guest k8in32 texture bytes
  // are stored reversed per dword (bytes per dword = a, b, g, r).
  // t4 = the exported tile texel: (a,b,g,r) = (255,180,100,50) samples as
  // (50,100,180,255)/255. t0/t1/t13 = solid placeholders (the guest-math
  // coordinates clamp to their edge texels; their values do not reach the
  // export on the traced path).
  const struct { std::uint32_t base; std::uint8_t rgba[4]; } textures[4] = {
      {0x80000u, {255u, 10u, 20u, 30u}},
      {0xC0000u, {255u, 40u, 50u, 60u}},
      {0x100000u, {255u, 180u, 100u, 50u}},
      {0x140000u, {255u, 70u, 80u, 90u}}};
  for (const auto& t : textures) {
    std::vector<std::uint8_t> bytes(4u * 1024u);
    for (std::size_t i = 0u; i < bytes.size(); i += 4u) {
      bytes[i + 0u] = t.rgba[0];
      bytes[i + 1u] = t.rgba[1];
      bytes[i + 2u] = t.rgba[2];
      bytes[i + 3u] = t.rgba[3];
    }
    assert(runtime.write_shared_memory(t.base / 4u, bytes));
  }
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(1024u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 4096u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);  // 2 shader loads + 1 draw + 1 present
  const bool ok = runtime.execute_frame(target, state, output);
  if (!ok) {
    std::fprintf(stderr, "SLOT4 FRAME ERR: %s\n", runtime.error().c_str());
  }
  assert(ok);
  assert(runtime.last_pixel_modification() == 0x410000000000ull);
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  const auto pixels = target.readback();
  const auto expect = [&](std::uint32_t x, std::uint32_t y,
                          std::uint8_t r, std::uint8_t g, std::uint8_t b,
                          std::uint8_t a) {
    const std::size_t off = (y * 1280u + x) * 4u;
    assert(pixels[off + 0u] == r);
    assert(pixels[off + 1u] == g);
    assert(pixels[off + 2u] == b);
    assert(pixels[off + 3u] == a);
  };
  expect(640u, 360u, 50u, 100u, 180u, 255u);
  expect(630u, 300u, 50u, 100u, 180u, 255u);
  expect(700u, 400u, 50u, 100u, 180u, 255u);
  // Outside the triangle: the clear color.
  expect(100u, 100u, 0u, 0u, 0u, 0u);
}

void pinned_hoisted_gradients_payload_renders_via_interpolator() {
  ac6::native::VulkanDevice device;
  if (!device.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_hoisted_gradients_payload_renders_via_interpolator:"
                 " no device\n");
    return;
  }
  ac6::native::VulkanOffscreenTarget target(device);
  assert(target.valid());
  ac6::native::PinnedShaderRuntime runtime(device);
  if (!runtime.valid()) {
    std::fprintf(stderr,
                 "SKIP pinned_hoisted_gradients_payload_renders_via_interpolator:"
                 " %s\n",
                 runtime.error().c_str());
    return;
  }
  // r273: the AC6 hoisted-gradients payload (KEEP decision r271) verified
  // end-to-end. The pinned pixel shader (capsule digest
  // 0x58712ec6f35bd960, oracle hash 35A80CB48A481624, mod
  // 0x000040000021003f = depth mode kEarlyHint) hoists its fetch
  // gradients from interpolator 4 (DPdx/DPdy computed once at entry) and
  // fetches six textures (t0/t1/t11/t12/t14 2D + t15 cube) whose sample
  // results feed tile math, sign decodes and the cube face select. The
  // pinned VS (digest 0xe74686edb2236614, 16-byte records through the
  // descriptor path at registers 0x48BE/0x48BF) writes interpolators
  // N = (record d2, record d3, d2, d3) with position (d0, -d1 through the
  // signed YScale, d2): record d2 varies per vertex (real, nonzero
  // hoisted gradients), record d3 = 0.6 is constant so interpolator 3.w
  // = 0.6 keeps the shader's 1/w reciprocal finite (the pinned VS family
  // writes w = 0 for its interpolators, which drove this shader's
  // reciprocal to +inf and undefined GPU sampling - the rejected pairing
  // is named in the r273 derivation) and the export alpha =
  // interpolator 5.w = 0.6 is constant. The traced export = (0, 0, 0,
  // 0.6) at EVERY covered fragment (7 sample sites: t0 querylod+sample,
  // t12, t11 _u/_s with the hoisted gradients, t14, t15 cube) -> (0, 0,
  // 0, 153) 8-bit; uncovered fragments stay at the clear color. The draw
  // leaves alpha test disabled so the state-derived pixel modification
  // high is kEarlyHint and the first r271 candidate matches the pinned
  // modification exactly.
  assert(ac6::native::register_bundled_pinned_shader_registry());
  const auto capsule = ac6::native::bundled_pinned_shader_registry();
  const auto entries = ac6::native::parse_pinned_shader_capsule(capsule);
  constexpr std::uint64_t kVertexDigest = 0xe74686edb2236614ull;
  constexpr std::uint64_t kPixelDigest = 0x58712ec6f35bd960ull;
  constexpr std::uint64_t kPixelModification = 0x000040000021003full;
  const ac6::native::PinnedShaderEntry* vertex_entry = nullptr;
  const ac6::native::PinnedShaderEntry* pixel_entry = nullptr;
  for (const auto& entry : entries) {
    if (entry.signature.digest == kVertexDigest &&
        entry.signature.shader_type == 0u) {
      vertex_entry = &entry;
    }
    if (entry.signature.digest == kPixelDigest &&
        entry.signature.shader_type == 1u &&
        entry.modification == kPixelModification) {
      pixel_entry = &entry;
    }
  }
  assert(vertex_entry != nullptr && pixel_entry != nullptr);

  ac6::native::XenosState state;
  ac6::native::MmioBus bus;
  ac6::native::VdBridge bridge(bus);
  std::vector<std::uint32_t> ring;
  auto push_packet = [&ring](std::uint32_t opcode,
                             std::vector<std::uint32_t> payload) {
    ring.push_back(ac6::native::pm4::type3_header(
        opcode, static_cast<std::uint32_t>(payload.size())));
    ring.insert(ring.end(), payload.begin(), payload.end());
  };
  auto push_type0 = [&ring](std::uint32_t base,
                            std::vector<std::uint32_t> values) {
    ring.push_back(ac6::native::pm4::header(
        ac6::native::pm4::kType0, static_cast<std::uint32_t>(values.size()),
        base));
    ring.insert(ring.end(), values.begin(), values.end());
  };
  {
    std::vector<std::uint32_t> payload{0u,
                                       static_cast<std::uint32_t>(vertex_entry->microcode.size())};
    payload.insert(payload.end(), vertex_entry->microcode.begin(),
                   vertex_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  {
    std::vector<std::uint32_t> payload{1u,
                                       static_cast<std::uint32_t>(pixel_entry->microcode.size())};
    payload.insert(payload.end(), pixel_entry->microcode.begin(),
                   pixel_entry->microcode.end());
    push_packet(ac6::native::pm4::kOpcodeImLoadImmediate, payload);
  }
  // Texture fetch constants t0/t1/t11/t12/t14 (2D) and t15 (cube): the
  // qualified r272 shape (clamp-to-edge, 4x4, k_8_8_8_8, linear, k8in32,
  // exponent 0), distinct shared-memory bases. tN at 0x4800 + 6N.
  const std::uint32_t fetch_word0 =
      2u | (2u << 10u) | (2u << 13u) | (8u << 22u);
  const std::uint32_t fetch_word3 =
      (83u << 1u) | (1u << 19u) | (1u << 21u) | (1u << 23u);
  const struct { std::uint32_t n; std::uint32_t base; bool cube; } tex[6] = {
      {0u, 0x100000u, false}, {1u, 0x110000u, false},
      {11u, 0x120000u, false}, {12u, 0x130000u, false},
      {14u, 0x140000u, false}, {15u, 0x150000u, true}};
  for (const auto& t : tex) {
    const std::uint32_t dimension = t.cube ? 3u : 1u;
    push_type0(0x4800u + 6u * t.n,
               {fetch_word0, 6u | (2u << 6u) | ((t.base >> 12u) << 12u),
                3u | (3u << 13u), fetch_word3, 0u, dimension << 9u});
  }
  // Vertex fetch constant 47 (the descriptor path words, registers
  // 0x48BE/0x48BF): base 0x1000 dwords, endian k8in32, 16-byte records.
  push_type0(0x48BEu, {3u | (0x1000u << 2u), 2u | (16u << 2u)});
  push_type0(0x2208u, {4u});
  push_type0(0x2000u, {1280u});
  push_type0(0x2001u, {16u});
  push_type0(0x200Eu, {0u, 720u << 16u | 1280u});
  push_type0(0x2104u, {0xFFFFFFFFu, 0u});
  push_type0(0x2100u, {0xFFFFFFFFu, 0u});
  push_type0(0x210Fu, {0x44200000u, 0x44200000u, 0xC3B40000u, 0x43B40000u,
                       0x3F800000u, 0u});
  push_type0(0x2206u, {0x3Fu});
  // RB_COLORCONTROL = 0: alpha test AND alpha-to-coverage disabled -> the
  // derived depth-stencil mode is kEarlyHint (dword1 bit 14), matching the
  // pinned modification 0x000040000021003f.
  push_type0(0x2202u, {0u});
  // Vertex float constants: this VS's compact slots map to guest
  // c100/c101/c102/c255 and the traced configuration uses zeros (the
  // register defaults) - the record dwords carry the interpolator data.
  // Pixel float constants (compact slots -> guest c0/c80/c82/c84/c129/
  // c138/c139/c142/c160/c250/c251/c252/c253/c254/c255; pixel bank cN at
  // 0x4400 + 4N). All (1,1,1,1) except c252 = (0, 1e38, 0, 0) (compact
  // float slot 11 drives the predicate that enables the hoisted-gradient
  // t11 fetches; the traced comparison is c252.y >= r6.y with r6.y = 1.0
  // in the interpreter model, but the GPU's real queryLod chain yields a
  // much larger r6.y, so the margin is 1e38 to force the traced long
  // path) and c254 = (0, 0, -1, 0) (drives the t0 mip predicate). Traced
  // values: the executed path yields the constant export (0, 0, 0, 0.6).
  const struct { std::uint32_t c; float xyzw[4]; } pconst[15] = {
      {0u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {80u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {82u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {84u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {129u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {138u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {139u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {142u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {160u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {250u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {251u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {252u, {0.0f, 1e38f, 0.0f, 0.0f}},
      {253u, {1.0f, 1.0f, 1.0f, 1.0f}},
      {254u, {0.0f, 0.0f, -1.0f, 0.0f}},
      {255u, {1.0f, 1.0f, 1.0f, 1.0f}}};
  for (const auto& cst : pconst) {
    std::uint32_t words[4];
    for (std::uint32_t j = 0u; j < 4u; ++j) {
      std::memcpy(&words[j], &cst.xyzw[j], sizeof(float));
    }
    push_type0(0x4400u + 4u * cst.c, {words[0], words[1], words[2], words[3]});
  }
  // Vertex records: 3 x 4 dwords (big-endian) at dword 0x1000.
  // d0 = position x, d1 = position y (the VS applies the signed YScale,
  // so d1 = +0.5 is the screen top), d2 = the per-vertex interpolator
  // carrier (varies: real hoisted gradients; also the vertex z), and
  // d3 = 0.6 constant (interpolator 3.w / interpolator 5.w: finite 1/w
  // and the constant export alpha 0.6 -> 153).
  {
    const float records[3][4] = {{-0.5f, 0.5f, 0.25f, 0.6f},
                                 {0.5f, 0.5f, 0.5f, 0.6f},
                                 {0.0f, -0.5f, 0.75f, 0.6f}};
    std::vector<std::uint8_t> vertex_bytes(3u * 16u, 0u);
    for (std::uint32_t vertex = 0u; vertex < 3u; ++vertex) {
      for (std::uint32_t word = 0u; word < 4u; ++word) {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &records[vertex][word], sizeof(float));
        const std::uint32_t swapped = __builtin_bswap32(bits);
        std::memcpy(vertex_bytes.data() + vertex * 16u + word * 4u, &swapped,
                    4u);
      }
    }
    assert(runtime.write_shared_memory(0x1000u, vertex_bytes));
  }
  // Textures: solid 4x4 clamp-to-edge blocks (rows of 256 px; only the
  // first 4 texels of each row are reachable). Guest dwords are
  // byte-reversed k8in32: bytes per dword = a, b, g, r.
  const struct { std::uint32_t base; std::uint8_t rgba[4]; bool cube; } texels[6] = {
      {0x100000u, {255u, 48u, 32u, 16u}, false},
      {0x110000u, {255u, 10u, 40u, 200u}, false},
      {0x120000u, {255u, 150u, 100u, 5u}, false},
      {0x130000u, {255u, 0u, 250u, 250u}, false},
      {0x140000u, {255u, 255u, 255u, 0u}, false},
      {0x150000u, {255u, 30u, 60u, 90u}, true}};
  for (const auto& t : texels) {
    const std::uint32_t layers = t.cube ? 6u : 1u;
    std::vector<std::uint8_t> bytes(layers * 4u * 1024u);
    for (std::size_t i = 0u; i < bytes.size(); i += 4u) {
      bytes[i + 0u] = t.rgba[0];
      bytes[i + 1u] = t.rgba[1];
      bytes[i + 2u] = t.rgba[2];
      bytes[i + 3u] = t.rgba[3];
    }
    assert(runtime.write_shared_memory(t.base / 4u, bytes));
  }
  push_packet(ac6::native::pm4::kOpcodeDrawIndx2,
              {4u | (2u << 6u) | (3u << 16u)});
  push_packet(ac6::native::pm4::kOpcodeXeSwap,
              {ac6::native::pm4::kSwapSignature, 0u, 1280u, 720u});
  const std::uint32_t ring_bytes = static_cast<std::uint32_t>(ring.size()) * 4u;
  ring.resize(1024u, 0u);
  bridge.set_ring_words(ring);
  assert(bus.write(ac6::native::MmioBus::kRingSize, 4096u));
  assert(bus.write(ac6::native::MmioBus::kRingRead, 0u));
  assert(bus.write(ac6::native::MmioBus::kRingWrite, ring_bytes));
  std::vector<ac6::native::XenosCommand> output;
  auto pump_result = bridge.pump(state, output);
  assert(pump_result.ok());
  assert(output.size() == 4u);  // 2 shader loads + 1 draw + 1 present
  const bool ok = runtime.execute_frame(target, state, output);
  if (!ok) {
    std::fprintf(stderr, "HOIST FRAME ERR: %s\n", runtime.error().c_str());
  }
  assert(ok);
  // The r271 candidate search matched the pinned modification exactly.
  assert(runtime.last_pixel_modification() == kPixelModification);
  const auto pixels = target.readback();
  assert(runtime.draw_count() == 1u);
  assert(runtime.edram_resolves() == 1u);
  // Every covered fragment samples the solid textures through the payload
  // path (hoisted gradients, cube face select) and exports (0, 0, 0, 0.6).
  const auto expect = [&](std::uint32_t x, std::uint32_t y, std::uint8_t a) {
    const std::size_t off = (y * 1280u + x) * 4u;
    assert(pixels[off + 0u] == 0u);
    assert(pixels[off + 1u] == 0u);
    assert(pixels[off + 2u] == 0u);
    assert(pixels[off + 3u] == a);
  };
  expect(640u, 360u, 153u);
  expect(630u, 300u, 153u);
  expect(500u, 300u, 153u);
  expect(700u, 400u, 153u);
  // Outside the triangle: the clear color.
  expect(100u, 100u, 0u);
}

// Each Vulkan test creates and destroys its own VkDevice; empirically a
// device destroy/create cycle can leave the shared physical queue in a
// state that zeroes the NEXT test's draws (r273). The production runtime
// uses one device for the whole process, so this is a test-harness
// artifact, not a product path - isolate every test in a child process so
// each one sees a fresh driver context, like ctest isolation would.
static void run_isolated(const char* name, void (*fn)()) {
  pid_t pid = fork();
  if (pid == 0) {
    fn();
    _exit(0);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::fprintf(stderr, "ISOLATED FAIL %s (exit %d)\n", name,
                 WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    assert(false);
  }
}

int main() {
  run_isolated("draw_state_follows_nested_pm4_order", draw_state_follows_nested_pm4_order);
  run_isolated("failed_decode_commits_sane_prefix_on_identical_repeat", failed_decode_commits_sane_prefix_on_identical_repeat);
  run_isolated("failed_decode_keeps_drop_path_below_sanity_gates", failed_decode_keeps_drop_path_below_sanity_gates);
  run_isolated("failed_decode_advance_test_consumes_window_on_third_identical_repeat", failed_decode_advance_test_consumes_window_on_third_identical_repeat);
  run_isolated("failed_decode_commits_nested_prefix_on_identical_repeat", failed_decode_commits_nested_prefix_on_identical_repeat);
  run_isolated("failed_decode_resets_on_changed_signature", failed_decode_resets_on_changed_signature);
  run_isolated("draw_indx_checks_length_before_index_fields", draw_indx_checks_length_before_index_fields);

  run_isolated("pinned_shader_samples_two_textures", pinned_shader_samples_two_textures);
  run_isolated("pinned_point_list_expands_to_triangle_strips", pinned_point_list_expands_to_triangle_strips);
  run_isolated("pinned_rectangle_reconstructs_three_vertices", pinned_rectangle_reconstructs_three_vertices);
  run_isolated("pinned_rectangle_msaa4_pitch_matches_swap_dims", pinned_rectangle_msaa4_pitch_matches_swap_dims);
  run_isolated("pinned_rectangle_depth_always_writes_d24s8", pinned_rectangle_depth_always_writes_d24s8);
  run_isolated("pinned_rectangle_d24fs8_depth_below_color_frame", pinned_rectangle_d24fs8_depth_below_color_frame);
  run_isolated("pinned_rectangle_pair_exports_interpolator_color", pinned_rectangle_pair_exports_interpolator_color);
  run_isolated("run_retail_vertex_bytes_diagnostic", run_retail_vertex_bytes_diagnostic);
  run_isolated("pinned_depth_unqualified_configs_fail_closed", pinned_depth_unqualified_configs_fail_closed);
  run_isolated("endian_modes_are_distinct_and_bounded", endian_modes_are_distinct_and_bounded);
  run_isolated("decoder_commits_only_complete_packet", decoder_commits_only_complete_packet);
  run_isolated("decoder_rejects_unknown_and_bad_wait", decoder_rejects_unknown_and_bad_wait);
  run_isolated("decoder_covers_type1_and_type2_without_silent_effects", decoder_covers_type1_and_type2_without_silent_effects);
  run_isolated("decoder_accepts_verified_retail_opcodes_0x45_and_0x46", decoder_accepts_verified_retail_opcodes_0x45_and_0x46);
  run_isolated("decoder_enforces_hardware_one_register", decoder_enforces_hardware_one_register);
  run_isolated("decoder_decodes_predicated_type3_like_unpredicated", decoder_decodes_predicated_type3_like_unpredicated);
  run_isolated("decoder_accepts_real_captured_predicated_headers", decoder_accepts_real_captured_predicated_headers);
  run_isolated("ring_wrap_and_interrupt_are_bounded", ring_wrap_and_interrupt_are_bounded);
  run_isolated("indirect_buffers_expand_and_cycles_fail_closed", indirect_buffers_expand_and_cycles_fail_closed);
  run_isolated("indirect_buffer_decode_error_reports_real_hex_address", indirect_buffer_decode_error_reports_real_hex_address);
  run_isolated("register_count_covers_type0_full_field_width", register_count_covers_type0_full_field_width);
  run_isolated("vulkan_boundary_fails_closed", vulkan_boundary_fails_closed);
  run_isolated("vulkan_device_offscreen_clear_readback", vulkan_device_offscreen_clear_readback);
  run_isolated("vulkan_swapchain_acquire_present_cycles", vulkan_swapchain_acquire_present_cycles);
  run_isolated("vulkan_xcb_visible_present", vulkan_xcb_visible_present);
  run_isolated("immediate_shader_packet_retains_microcode", immediate_shader_packet_retains_microcode);
  run_isolated("ucode_registry_matches_pinned_only", ucode_registry_matches_pinned_only);
  run_isolated("shader_boundary_accepts_only_valid_spirv", shader_boundary_accepts_only_valid_spirv);
  run_isolated("guest_vd_service_drains_published_dword_index", guest_vd_service_drains_published_dword_index);
  run_isolated("guest_vd_service_drain_commits_prefix_on_repeated_wedge", guest_vd_service_drain_commits_prefix_on_repeated_wedge);
  run_isolated("guest_vd_service_present_executes_offscreen", guest_vd_service_present_executes_offscreen);
  run_isolated("guest_vd_service_event_write_applies_single_endian_swap", guest_vd_service_event_write_applies_single_endian_swap);
  run_isolated("bundled_pinned_registry_fully_hits", bundled_pinned_registry_fully_hits);
  run_isolated("pinned_shaders_execute_a_real_frame", pinned_shaders_execute_a_real_frame);
  run_isolated("pinned_shader_samples_a_real_texture", [] { pinned_shader_samples_a_real_texture(false); });
  run_isolated("guest_vd_wrapped_alias_texture_and_latest_identity", [] { pinned_shader_samples_a_real_texture(true); });
  run_isolated("pinned_viewport_independent_of_oversized_scissor", [] { pinned_shader_samples_a_real_texture(false, true); });
  run_isolated("pinned_texture_shader_uses_compacted_constants", pinned_texture_shader_uses_compacted_constants);
  run_isolated("pinned_shader_selects_modification_by_draw_state", pinned_shader_selects_modification_by_draw_state);
  run_isolated("pinned_edram_draws_accumulate_and_reconfig_recovers", pinned_edram_draws_accumulate_and_reconfig_recovers);
  run_isolated("pinned_indexed_draw_renders_guest_index_order", pinned_indexed_draw_renders_guest_index_order);
  run_isolated("pinned_indexed_draw_honors_primitive_restart", pinned_indexed_draw_honors_primitive_restart);
  run_isolated("pinned_shader_samples_two_textures", pinned_shader_samples_two_textures);
  run_isolated("pinned_point_list_expands_to_triangle_strips", pinned_point_list_expands_to_triangle_strips);
  run_isolated("pinned_rectangle_reconstructs_three_vertices", pinned_rectangle_reconstructs_three_vertices);
  run_isolated("pinned_rectangle_msaa4_pitch_matches_swap_dims", pinned_rectangle_msaa4_pitch_matches_swap_dims);
  run_isolated("pinned_rectangle_depth_always_writes_d24s8", pinned_rectangle_depth_always_writes_d24s8);
  run_isolated("pinned_rectangle_d24fs8_depth_below_color_frame", pinned_rectangle_d24fs8_depth_below_color_frame);
  run_isolated("pinned_rectangle_pair_exports_interpolator_color", pinned_rectangle_pair_exports_interpolator_color);
  run_isolated("run_retail_vertex_bytes_diagnostic", run_retail_vertex_bytes_diagnostic);
  run_isolated("pinned_depth_unqualified_configs_fail_closed", pinned_depth_unqualified_configs_fail_closed);
  run_isolated("pinned_edram_format_2_10_10_10_accepted_and_reconfigures", pinned_edram_format_2_10_10_10_accepted_and_reconfigures);
  run_isolated("pinned_point_loop_variant_expands_per_vertex_quads", pinned_point_loop_variant_expands_per_vertex_quads);
  run_isolated("pinned_edram_format_16_16_16_16_float_renders_and_resolves", pinned_edram_format_16_16_16_16_float_renders_and_resolves);
  run_isolated("pinned_edram_format_8_8_8_8_gamma_renders_like_8_8_8_8", pinned_edram_format_8_8_8_8_gamma_renders_like_8_8_8_8);
  run_isolated("pinned_cube_texture_samples_expected_face_texels", pinned_cube_texture_samples_expected_face_texels);
  run_isolated("pinned_deswizzle_slot4_payload_renders_tile_restore", pinned_deswizzle_slot4_payload_renders_tile_restore);
  run_isolated("pinned_deswizzle_identity_renders_restore_pass", pinned_deswizzle_identity_renders_restore_pass);
  run_isolated("untile_tiled_dxt45_matches_documented_layout", untile_tiled_dxt45_matches_documented_layout);
  run_isolated("expand_quad_list_auto_produces_two_triangles", expand_quad_list_auto_produces_two_triangles);
  run_isolated("expand_quad_list_words_remaps_dma_and_refuses_restart", expand_quad_list_words_remaps_dma_and_refuses_restart);
  run_isolated("tiled_dxt45_layout_bounds_footprint", tiled_dxt45_layout_bounds_footprint);
  run_isolated("tiled_dxt1_layout_bounds_footprint", tiled_dxt1_layout_bounds_footprint);
  run_isolated("untile_tiled_dxt1_matches_documented_layout", untile_tiled_dxt1_matches_documented_layout);
  run_isolated("pinned_hoisted_gradients_payload_renders_via_interpolator", pinned_hoisted_gradients_payload_renders_via_interpolator);
  run_isolated("pinned_shader_samples_a_real_texture", [] { pinned_shader_samples_a_real_texture(false); });
  return 0;
}
