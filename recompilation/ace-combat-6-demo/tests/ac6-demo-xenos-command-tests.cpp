#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6demo/xenos_command_processor.hpp"

#include "ac6demo/runtime_error.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <variant>
#include <vector>

namespace {

constexpr std::uint32_t type0(std::uint16_t index, std::uint16_t count) {
  return (static_cast<std::uint32_t>(count - 1U) << 16U) | index;
}

constexpr std::uint32_t type3(std::uint8_t opcode, std::uint16_t count,
                              bool predicated = false) {
  return 0xC0000000U | (static_cast<std::uint32_t>(count - 1U) << 16U) |
         (static_cast<std::uint32_t>(opcode) << 8U) | (predicated ? 1U : 0U);
}

template <typename Callable> void expect_trap(Callable &&callable) {
  bool trapped = false;
  try {
    callable();
  } catch (const ac6demo::RuntimeTrap &) {
    trapped = true;
  }
  assert(trapped);
}

void test_qualified_commands_and_immutable_draws() {
  constexpr std::uint32_t draw_initiator = 0x00030088U;
  const std::vector<std::uint32_t> stream{
      type0(ac6demo::kXenosTextureFetch00, 6U),
      0x8A000002U,
      0x1374A006U,
      0x0059E4FFU,
      0x00001414U,
      0x00000000U,
      0x00000200U,
      type0(0x0100U, 1U),
      0x11111111U,
      type3(0x2BU, 4U),
      0U,
      2U,
      0x01020304U,
      0xAABBCCDDU,
      type3(0x2BU, 3U),
      1U,
      1U,
      0x12345678U,
      type3(0x60U, 1U),
      1U,
      type3(0x62U, 1U),
      1U,
      type3(0x36U, 1U, true),
      draw_initiator,
      type0(0x0100U, 1U),
      0x22222222U,
      type3(0x2BU, 5U),
      0U,
      3U,
      0x89ABCDEFU,
      0x10203040U,
      0x50607080U,
      type3(0x36U, 1U),
      draw_initiator,
      type3(0x64U, 4U),
      0x53574150U,
      0x1374A000U,
      1280U,
      720U,
      0x80000000U};

  ac6demo::XenosCommandProcessor processor;
  const auto commands = processor.process_batch(stream);
  assert(commands.size() == 6U);
  assert(commands.renderer_write_counts ==
         std::vector<std::size_t>(commands.size(), 0U));

  const auto &shader1 = std::get<ac6demo::XenosShaderLoadCommand>(commands[0]);
  const auto &shader2 = std::get<ac6demo::XenosShaderLoadCommand>(commands[1]);
  const auto &draw1 = std::get<ac6demo::XenosDrawCommand>(commands[2]);
  const auto &shader3 = std::get<ac6demo::XenosShaderLoadCommand>(commands[3]);
  const auto &draw2 = std::get<ac6demo::XenosDrawCommand>(commands[4]);
  const auto &present = std::get<ac6demo::XenosPresentCommand>(commands[5]);

  assert(shader1.stage == ac6demo::XenosShaderStage::Vertex);
  assert(shader1.start_dword == 0U && shader1.size_dwords == 2U);
  assert(shader1.guest_source_address == 0U);
  assert(shader1.guest_big_endian_sha256 ==
         "f51c9b0b3e2f4e386b1c77e81d341867052f700fbf3fd12897f298d6e38cbcf1");
  assert((shader1.guest_big_endian_dwords ==
          std::vector<std::uint32_t>{0x01020304U, 0xAABBCCDDU}));
  assert(shader2.stage == ac6demo::XenosShaderStage::Pixel &&
         shader2.size_dwords == 1U);
  assert(shader2.guest_source_address == 0U);
  assert(shader2.guest_big_endian_sha256 ==
         "b2ed992186a5cb19f6668aade821f502c1d00970dfd0e35128d51bac4649916c");
  assert((shader2.guest_big_endian_dwords ==
          std::vector<std::uint32_t>{0x12345678U}));
  assert(shader3.stage == ac6demo::XenosShaderStage::Vertex &&
         shader3.size_dwords == 3U);
  assert(shader3.guest_source_address == 0U);
  assert(shader3.guest_big_endian_sha256.size() == 64U);
  assert(shader3.guest_big_endian_dwords.size() == 3U);

  assert(draw1.predicated && !draw2.predicated);
  assert(draw1.primitive == ac6demo::XenosPrimitive::RectangleList);
  assert(draw1.source == ac6demo::XenosIndexSource::AutoIndex);
  assert(draw1.index_format == ac6demo::XenosIndexFormat::Uint16);
  assert(draw1.index_count == 3U && draw2.index_count == 3U);
  assert(draw1.vertex_shader_sha256 == shader1.guest_big_endian_sha256);
  assert(draw1.pixel_shader_sha256 == shader2.guest_big_endian_sha256);
  assert(draw2.vertex_shader_sha256 == shader3.guest_big_endian_sha256);
  assert(draw2.pixel_shader_sha256 == shader2.guest_big_endian_sha256);
  assert(draw1.registers->value(0x0100U) == 0x11111111U);
  assert(draw2.registers->value(0x0100U) == 0x22222222U);
  assert(draw1.registers->value(ac6demo::kXenosVgtDrawInitiator) ==
         draw_initiator);
  assert(processor.register_value(0x0100U) == 0x22222222U);
  assert(draw1.registers->value(0x0100U) == 0x11111111U);
  assert(present.resource_id.size() == 64U);
  assert(present.format == 6U && present.tiled);
  assert(present.width == 1280U && present.height == 720U);

  auto changed_fetch = stream;
  changed_fetch[1] ^= 1U; // The reached type/format/endian tuple is exact.
  expect_trap([&] {
    ac6demo::XenosCommandProcessor rejected;
    static_cast<void>(rejected.process_batch(changed_fetch));
  });
}

void test_bin_predication_skip() {
  ac6demo::XenosCommandProcessor processor;
  const std::vector<std::uint32_t> stream{
      type3(0x60U, 1U), 0U, type3(0x62U, 1U), 1U, type3(0x7EU, 1U, true), 0U};
  const auto commands = processor.process_batch(stream);
  assert(commands.empty());
  assert(processor.bin_mask() == 0U && processor.bin_select() == 1U);
}

void test_reached_point_draw_shape() {
  ac6demo::XenosCommandProcessor processor;
  const std::vector<std::uint32_t> stream{
      type3(0x2BU, 3U), 0U,         1U, 0x01020304U,
      type3(0x2BU, 3U), 1U,         1U, 0x05060708U,
      type3(0x36U, 1U), 0x00010081U};
  const auto commands = processor.process_batch(stream);
  assert(commands.size() == 3U);
  const auto &draw = std::get<ac6demo::XenosDrawCommand>(commands[2]);
  assert(draw.primitive == ac6demo::XenosPrimitive::PointList);
  assert(draw.source == ac6demo::XenosIndexSource::AutoIndex);
  assert(draw.index_count == 1U);
}

void test_reached_quad_draw_shape() {
  const std::vector<std::uint32_t> prefix{
      type3(0x2BU, 3U), 0U, 1U, 0x01020304U,
      type3(0x2BU, 3U), 1U, 1U, 0x05060708U};
  auto reached = prefix;
  reached.insert(reached.end(),
                 {type3(0x22U, 2U, true), 0U, 0x0004008DU});

  ac6demo::XenosCommandProcessor processor;
  const auto commands = processor.process_batch(reached);
  assert(commands.size() == 3U);
  const auto &draw = std::get<ac6demo::XenosDrawCommand>(commands[2]);
  assert(draw.primitive == ac6demo::XenosPrimitive::QuadList);
  assert(draw.source == ac6demo::XenosIndexSource::AutoIndex);
  assert(draw.index_format == ac6demo::XenosIndexFormat::Uint16);
  assert(draw.index_count == 4U && draw.predicated);

  const auto reject_suffix = [&](std::initializer_list<std::uint32_t> suffix) {
    auto stream = prefix;
    stream.insert(stream.end(), suffix);
    ac6demo::XenosCommandProcessor rejected;
    expect_trap([&] { static_cast<void>(rejected.process_batch(stream)); });
  };
  reject_suffix({type3(0x22U, 2U, true), 1U, 0x0004008DU});
  reject_suffix({type3(0x22U, 2U, true), 0U, 0x00030088U});
  reject_suffix({type3(0x22U, 1U, true), 0x0004008DU});
}

void test_qualified_title_texture_dynamic_base() {
  const auto make_registers = [](const std::array<std::uint32_t, 6> &words) {
    std::array<std::uint32_t, ac6demo::kXenosRegisterCount> values{};
    for (std::uint16_t index = 0U; index < words.size(); ++index) {
      values[ac6demo::kXenosTextureFetch00 + index] = words[index];
    }
    return ac6demo::XenosRegisterSnapshot(std::move(values));
  };

  constexpr std::array<std::uint32_t, 6> kWide{
      0x8A004802U, 0x0E059054U, 0x0059E4FFU,
      0x01280D10U, 0x00000003U, 0x00000200U};
  const auto reached = ac6demo::qualified_title_texture_profile(
      make_registers(kWide));
  assert(reached.has_value());
  assert(reached->address == 0x0E059000U);
  assert(reached->payload_size == 0xF0000U);
  assert(reached->width == 1280U && reached->height == 720U);
  assert(reached->encoding ==
         ac6demo::QualifiedTitleTextureEncoding::Bc3);

  constexpr std::array<std::uint32_t, 6> kTall{
      0x84004802U, 0x0E32C054U, 0x0059E1FFU,
      0x01280D10U, 0x00000003U, 0x00000200U};
  const auto tall = ac6demo::qualified_title_texture_profile(
      make_registers(kTall));
  assert(tall.has_value());
  assert(tall->address == 0x0E32C000U);
  assert(tall->payload_size == 0x60000U);
  assert(tall->width == 512U && tall->height == 720U);
  assert(tall->encoding == ac6demo::QualifiedTitleTextureEncoding::Bc3);

  auto moved_tall = kTall;
  moved_tall[1] = 0x0E400054U;
  const auto moved = ac6demo::qualified_title_texture_profile(
      make_registers(moved_tall));
  assert(moved.has_value() && moved->address == 0x0E400000U);

  for (const auto index : std::array<std::size_t, 5>{0U, 2U, 3U, 4U, 5U}) {
    auto changed = kTall;
    changed[index] ^= 1U;
    assert(!ac6demo::qualified_title_texture_profile(make_registers(changed))
                .has_value());
  }
  auto changed_low_bits = kTall;
  changed_low_bits[1] ^= 1U;
  assert(!ac6demo::qualified_title_texture_profile(
              make_registers(changed_low_bits))
              .has_value());
  auto zero_base = kTall;
  zero_base[1] = 0x00000054U;
  assert(!ac6demo::qualified_title_texture_profile(make_registers(zero_base))
              .has_value());

  constexpr std::array<std::uint32_t, 6> kSwap{
      0x8A000002U, 0x1374A006U, 0x0059E4FFU,
      0x00001414U, 0x00000000U, 0x00000200U};
  const auto swap = ac6demo::qualified_title_texture_profile(
      make_registers(kSwap));
  assert(swap.has_value());
  assert(swap->address == 0x1374A000U);
  assert(swap->payload_size == 0x398000U);
  assert(swap->width == 1280U && swap->height == 720U);
  assert(swap->encoding == ac6demo::QualifiedTitleTextureEncoding::Rgba8);
  for (const auto index : std::array<std::size_t, 5>{0U, 2U, 3U, 4U, 5U}) {
    auto changed = kSwap;
    changed[index] ^= 1U;
    assert(!ac6demo::qualified_title_texture_profile(make_registers(changed))
                .has_value());
  }
  auto moved_swap = kSwap;
  moved_swap[1] = 0x13749006U;
  assert(!ac6demo::qualified_title_texture_profile(make_registers(moved_swap))
              .has_value());
  auto changed_swap_low_bits = kSwap;
  changed_swap_low_bits[1] ^= 1U;
  assert(!ac6demo::qualified_title_texture_profile(
              make_registers(changed_swap_low_bits))
              .has_value());
}

void test_reached_pointer_shader_loads() {
  const auto memory = [](
                          std::uint32_t address)
      -> std::optional<ac6demo::XenosCommandProcessor::GuestBytes> {
    std::uint32_t word{};
    if (address >= 0x155FAB80U && address < 0x155FABBCU) {
      word = 0xA0000000U | ((address - 0x155FAB80U) / 4U);
    } else if (address >= 0x155FAA40U && address < 0x155FAB48U) {
      word = 0xB0000000U | ((address - 0x155FAA40U) / 4U);
    } else {
      return std::nullopt;
    }
    return ac6demo::XenosCommandProcessor::GuestBytes{
        static_cast<std::byte>(word >> 24U),
        static_cast<std::byte>(word >> 16U),
        static_cast<std::byte>(word >> 8U), static_cast<std::byte>(word)};
  };
  const std::vector<std::uint32_t> stream{
      type3(0x27U, 2U), 0x155FAB81U, 0x0000000FU,
      type3(0x27U, 2U), 0x155FAA40U, 0x00000042U,
      type3(0x36U, 1U), 0x00030088U};

  ac6demo::XenosCommandProcessor processor;
  const auto commands = processor.process_batch(stream, memory);
  assert(commands.size() == 3U);
  const auto &pixel = std::get<ac6demo::XenosShaderLoadCommand>(commands[0]);
  const auto &vertex = std::get<ac6demo::XenosShaderLoadCommand>(commands[1]);
  const auto &draw = std::get<ac6demo::XenosDrawCommand>(commands[2]);
  assert(pixel.stage == ac6demo::XenosShaderStage::Pixel);
  assert(pixel.start_dword == 0U && pixel.size_dwords == 15U);
  assert(pixel.guest_source_address == 0x155FAB80U);
  assert(pixel.guest_big_endian_dwords.front() == 0xA0000000U);
  assert(pixel.guest_big_endian_dwords.back() == 0xA000000EU);
  assert(vertex.stage == ac6demo::XenosShaderStage::Vertex);
  assert(vertex.start_dword == 0U && vertex.size_dwords == 66U);
  assert(vertex.guest_source_address == 0x155FAA40U);
  assert(vertex.guest_big_endian_dwords.front() == 0xB0000000U);
  assert(vertex.guest_big_endian_dwords.back() == 0xB0000041U);
  assert(draw.pixel_shader_sha256 == pixel.guest_big_endian_sha256);
  assert(draw.vertex_shader_sha256 == vertex.guest_big_endian_sha256);

  const std::array invalid_stage{type3(0x27U, 2U), 0x155FAB82U,
                                 0x0000000FU};
  expect_trap([&] {
    static_cast<void>(processor.process_batch(invalid_stage, memory));
  });
  const std::array invalid_start{type3(0x27U, 2U), 0x155FAB81U,
                                 0x0001000FU};
  expect_trap([&] {
    static_cast<void>(processor.process_batch(invalid_start, memory));
  });
  const std::array unmapped{type3(0x27U, 2U), 0x155FAB81U, 0x0000000FU};
  expect_trap(
      [&] { static_cast<void>(processor.process_batch(unmapped)); });
}

void test_reached_alu_constant_load() {
  const auto memory = [](
                          std::uint32_t address)
      -> std::optional<ac6demo::XenosCommandProcessor::GuestBytes> {
    if (address < 0x155FAA00U || address >= 0x155FAA40U) {
      return std::nullopt;
    }
    const std::uint32_t word =
        address == 0x155FAA00U
            ? 0x11223344U
            : 0xC0000000U | ((address - 0x155FAA00U) / 4U);
    return ac6demo::XenosCommandProcessor::GuestBytes{
        static_cast<std::byte>(word >> 24U),
        static_cast<std::byte>(word >> 16U),
        static_cast<std::byte>(word >> 8U), static_cast<std::byte>(word)};
  };
  const std::array reached{type3(0x2FU, 3U), 0x155FAA00U, 0x000003F0U,
                           0x00000010U};
  ac6demo::XenosCommandProcessor processor;
  assert(processor.process_batch(reached, memory).empty());
  assert(processor.register_value(0x43F0U) == 0x11223344U);
  assert(processor.register_value(0x43F1U) == 0xC0000001U);
  assert(processor.register_value(0x43FFU) == 0xC000000FU);
  assert(processor.register_value(0x4400U) == 0U);

  const auto reject_without_commit = [&](const auto &stream,
                                         const auto &callback) {
    expect_trap(
        [&] { static_cast<void>(processor.process_batch(stream, callback)); });
    assert(processor.register_value(0x43F0U) == 0x11223344U);
  };
  const std::array unsupported_type{type3(0x2FU, 3U), 0x155FAA00U,
                                    0x000103F0U, 0x00000010U};
  reject_without_commit(unsupported_type, memory);
  const std::array crosses_vertex_constants{type3(0x2FU, 3U), 0x155FAA00U,
                                             0x000003F1U, 0x00000010U};
  reject_without_commit(crosses_vertex_constants, memory);
  const std::array reserved_bits{type3(0x2FU, 3U), 0x555FAA00U,
                                 0x000003F0U, 0x00000010U};
  reject_without_commit(reserved_bits, memory);
  const auto missing_last = [memory](std::uint32_t address) {
    return address == 0x155FAA3CU
               ? std::optional<ac6demo::XenosCommandProcessor::GuestBytes>{}
               : memory(address);
  };
  reject_without_commit(reached, missing_last);
}

void test_failures_have_zero_commit() {
  ac6demo::XenosCommandProcessor processor;
  const std::vector<std::uint32_t> initial{type0(0x20U, 1U), 0xCAFEBABEU};
  assert(processor.process_batch(initial).empty());

  const auto reject_without_commit =
      [&](const std::vector<std::uint32_t> &batch) {
        expect_trap([&] { static_cast<void>(processor.process_batch(batch)); });
        assert(processor.register_value(0x20U) == 0xCAFEBABEU);
        assert(processor.register_value(0x21U) == 0U);
        assert(processor.bin_mask() == 0xFFFFFFFFULL);
      };

  reject_without_commit({type0(0x21U, 1U), 0xDEADBEEFU, type3(0x7EU, 1U), 0U});
  reject_without_commit(
      {type0(0x21U, 1U), 0xDEADBEEFU, type3(0x2BU, 3U), 0U, 2U, 0x12345678U});
  reject_without_commit(
      {type0(0x21U, 1U), 0xDEADBEEFU, type3(0x36U, 1U), 0x00030008U});
  reject_without_commit(
      {type0(0x21U, 1U), 0xDEADBEEFU, type3(0x60U, 2U), 0U, 0U});
  reject_without_commit(
      {type0(0x21U, 1U), 0xDEADBEEFU, type3(0x64U, 4U), 0x53574150U});
  const std::vector<std::uint32_t> reached_opaque{
      type0(0x0A02U, 4U), 0xC0100000U, 0x07F00000U,
      0xC0000000U, 0x00100000U};
  assert(processor.process_batch(reached_opaque).empty());
  assert(processor.register_value(0x0A02U) == 0xC0100000U);
  assert(processor.register_value(0x0A05U) == 0x00100000U);
  reject_without_commit({type0(0x21U, 1U), 0xDEADBEEFU,
                         type0(0x0A02U, 1U), 0xC0100001U});
  const std::vector<std::uint32_t> reached_2290{
      type0(0x2290U, 2U), 0x00000000U, 0x00000000U};
  assert(processor.process_batch(reached_2290).empty());
  reject_without_commit({type0(0x21U, 1U), 0xDEADBEEFU,
                         type0(0x2290U, 1U), 0x00000001U});
  const std::vector<std::uint32_t> reached_230b_2313{
      type0(0x230BU, 7U), 0U, 0U, 0U, 0U, 0U, 0U, 0U,
      type0(0x2313U, 2U), 0U, 0U};
  assert(processor.process_batch(reached_230b_2313).empty());
  reject_without_commit({type0(0x21U, 1U), 0xDEADBEEFU,
                         type0(0x2314U, 1U), 1U});

  ac6demo::XenosCommandProcessor shader_processor;
  expect_trap([&] {
    const std::vector<std::uint32_t> failed_shader{
        type3(0x2BU, 3U), 0U, 1U, 0x01020304U, type3(0x7EU, 1U), 0U};
    static_cast<void>(shader_processor.process_batch(failed_shader));
  });
  expect_trap([&] {
    const std::vector<std::uint32_t> pixel_and_draw{
        type3(0x2BU, 3U), 1U, 1U, 0x05060708U, type3(0x36U, 1U), 0x00030088U};
    static_cast<void>(shader_processor.process_batch(pixel_and_draw));
  });
}

ac6demo::XenosCommandProcessor::MemoryReadCallback
mapped_memory(std::array<std::byte, 4> wait_bytes) {
  return [wait_bytes](
             std::uint32_t address) -> std::optional<std::array<std::byte, 4>> {
    if (address == 0x16AE2004U) {
      return wait_bytes;
    }
    if (address == 0x16AE1000U || address == 0x16AE1004U ||
        address == 0x16A5A000U || address == 0x16A5A004U) {
      return std::array<std::byte, 4>{};
    }
    return std::nullopt;
  };
}

void test_transactional_effects_and_big_endian_bytes() {
  std::vector<std::uint32_t> stream{
      type0(0x1841U, 1U), 0xFFFFFFFFU, type3(0x21U, 3U), 0x1841U,
      0xFFFFF8FFU,        0U,          type3(0x3CU, 5U), 0x13U,
      0x16AE2006U,        1U,          0xFFFFFFFFU,      0x100U,
      type3(0x46U, 1U),   6U,          type3(0x54U, 1U), 4U,
      type3(0x58U, 3U),   3U,          0x16AE1006U,      0x1274CF28U,
      type3(0x58U, 3U),   3U,          0x16A5A006U,      0x1685A144U,
      type3(0x58U, 3U),   3U,          0x16A5A002U,      5U,
      type3(0x3BU, 1U),   0x7FFFU,     type3(0x48U, 18U)};
  stream.insert(stream.end(), 18U, 0U);

  ac6demo::XenosCommandProcessor processor;
  const auto result = processor.process_batch(
      stream,
      mapped_memory({std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}}));
  assert(!result.pending_wait && result.renderer_commands.empty());
  assert(processor.register_value(0x1841U) == 0xFFFFF8FFU);
  assert(processor.register_value(ac6demo::kXenosVgtEventInitiator) == 3U);
  assert(result.effects.register_rmw == 1U);
  assert(result.effects.wait_reg_mem == 1U);
  assert(result.effects.event_write == 1U);
  assert(result.effects.interrupt == 1U);
  assert(result.effects.event_write_shader_done == 3U);
  assert(result.effects.invalidate_state == 1U);
  assert(result.effects.micro_engine_init == 1U);
  assert(result.cpu_interrupts == std::vector<std::uint8_t>{2U});
  assert(result.memory_writes.size() == 3U);
  assert(result.memory_writes[0].address == 0x16AE1004U);
  assert((result.memory_writes[0].guest_bytes ==
          std::array<std::byte, 4>{std::byte{0x12}, std::byte{0x74},
                                   std::byte{0xCF}, std::byte{0x28}}));
  assert(result.memory_writes[1].address == 0x16A5A004U);
  assert((result.memory_writes[1].guest_bytes ==
          std::array<std::byte, 4>{std::byte{0x16}, std::byte{0x85},
                                   std::byte{0xA1}, std::byte{0x44}}));
  assert(result.memory_writes[2].address == 0x16A5A000U);
  assert((result.memory_writes[2].guest_bytes ==
          std::array<std::byte, 4>{std::byte{0}, std::byte{0}, std::byte{0},
                                   std::byte{5}}));
}

void test_scratch_writeback_produces_wait_values() {
  const std::vector<std::uint32_t> stream{
      type0(0x01DDU, 1U), 0x16AE2000U, type0(0x01DCU, 1U), 0x00020033U,
      type0(0x0579U, 1U), 1U,          type3(0x3CU, 5U),   0x13U,
      0x16AE2006U,        1U,          0xFFFFFFFFU,        0x100U,
      type0(0x0578U, 1U), 4U,          type3(0x3CU, 5U),   0x13U,
      0x16AE2002U,        4U,          0xFFFFFFFFU,        0x100U};
  const auto memory =
      [](std::uint32_t address) -> std::optional<std::array<std::byte, 4>> {
    if (address == 0x16AE2000U || address == 0x16AE2004U) {
      return std::array<std::byte, 4>{};
    }
    return std::nullopt;
  };

  ac6demo::XenosCommandProcessor processor;
  const auto result = processor.process_batch(stream, memory);
  assert(!result.pending_wait);
  assert(result.effects.scratch_writeback == 2U);
  assert(result.effects.wait_reg_mem == 2U);
  assert(result.memory_writes.size() == 2U);
  assert(result.renderer_commands.empty() &&
         result.renderer_write_counts.empty());
  assert(result.memory_writes[0].address == 0x16AE2004U);
  assert((result.memory_writes[0].guest_bytes ==
          std::array<std::byte, 4>{std::byte{0}, std::byte{0}, std::byte{0},
                                   std::byte{1}}));
  assert(result.memory_writes[1].address == 0x16AE2000U);
  assert((result.memory_writes[1].guest_bytes ==
          std::array<std::byte, 4>{std::byte{0}, std::byte{0}, std::byte{0},
                                   std::byte{4}}));
}

void test_bootstrap_scratch_writeback() {
  const std::vector<std::uint32_t> stream{
      type0(0x01DDU, 1U), 0x16A5B000U, type0(0x01DCU, 1U), 0x00020033U,
      type0(0x057CU, 1U), 0x0BADF00DU, type0(0x057BU, 1U), 0U};
  const auto memory =
      [](std::uint32_t address) -> std::optional<std::array<std::byte, 4>> {
    return address == 0x16A5B010U ? std::optional{std::array<std::byte, 4>{}}
                                  : std::nullopt;
  };
  ac6demo::XenosCommandProcessor processor;
  const auto result = processor.process_batch(stream, memory);
  assert(result.effects.scratch_writeback == 1U);
  assert(result.memory_writes.size() == 1U);
  assert(result.memory_writes[0].address == 0x16A5B010U);
  assert((result.memory_writes[0].guest_bytes ==
          std::array<std::byte, 4>{std::byte{0x0B}, std::byte{0xAD},
                                   std::byte{0xF0}, std::byte{0x0D}}));
}

void test_renderer_write_boundaries() {
  const std::vector<std::uint32_t> stream{
      type3(0x2BU, 3U), 0U, 1U, 0x01020304U,
      type0(0x01DDU, 1U), 0x16A5B000U,
      type0(0x01DCU, 1U), 0x00020033U,
      type0(0x0578U, 1U), 0xAABBCCDDU,
      type3(0x2BU, 3U), 1U, 1U, 0x05060708U,
      type3(0x36U, 1U), 0x00030088U};
  const auto memory =
      [](std::uint32_t address) -> std::optional<std::array<std::byte, 4>> {
    return address == 0x16A5B000U
               ? std::optional{std::array<std::byte, 4>{}}
               : std::nullopt;
  };

  ac6demo::XenosCommandProcessor processor;
  const auto result = processor.process_batch(stream, memory);
  assert(result.renderer_commands.size() == 3U);
  assert((result.renderer_write_counts ==
          std::vector<std::size_t>{0U, 1U, 1U}));
  assert(result.memory_writes.size() == 1U);
}

void test_wait_commits_prefix_and_retries_at_header() {
  const std::vector<std::uint32_t> stream{type0(0x01DDU, 1U),
                                          0x16AE2000U,
                                          type0(0x01DCU, 1U),
                                          0x00020033U,
                                          type0(0x0579U, 1U),
                                          1U,
                                          type3(0x3CU, 5U),
                                          0x13U,
                                          0x16AE2006U,
                                          1U,
                                          0xFFFFFFFFU,
                                          0x100U,
                                          type0(0x0578U, 1U),
                                          4U,
                                          type3(0x3CU, 5U),
                                          0x13U,
                                          0x16AE2002U,
                                          4U,
                                          0xFFFFFFFFU,
                                          0x100U,
                                          type3(0x54U, 1U),
                                          4U,
                                          type3(0x3CU, 5U, true),
                                          0x13U,
                                          0x16AE2006U,
                                          0U,
                                          0xFFFFFFFFU,
                                          0x100U};
  const auto memory =
      [](std::uint32_t address) -> std::optional<std::array<std::byte, 4>> {
    if (address == 0x16AE2000U || address == 0x16AE2004U) {
      return std::array<std::byte, 4>{};
    }
    return std::nullopt;
  };

  ac6demo::XenosCommandProcessor processor;
  const auto prefix = processor.process_batch(stream, memory);
  assert(prefix.pending_wait && prefix.consumed_dwords == 22U);
  assert(prefix.pending_wait_address == 0x16AE2006U);
  assert(prefix.effects.scratch_writeback == 2U);
  assert(prefix.effects.wait_reg_mem == 2U);
  assert(prefix.effects.interrupt == 1U);
  assert(prefix.memory_writes.size() == 2U);
  assert(prefix.cpu_interrupts == std::vector<std::uint8_t>{2U});
  assert(processor.register_value(0x0579U) == 1U);
  assert(processor.register_value(0x0578U) == 4U);

  const auto resumed = processor.process_batch(
      std::span<const std::uint32_t>(stream).subspan(prefix.consumed_dwords),
      memory);
  assert(!resumed.pending_wait && resumed.consumed_dwords == 6U);
  assert(resumed.effects.wait_reg_mem == 1U);
  assert(resumed.memory_writes.empty() && resumed.cpu_interrupts.empty());
}

void test_register_wait_forms() {
  ac6demo::XenosCommandProcessor processor;
  const std::vector<std::uint32_t> stream{
      type0(ac6demo::kXenosCoherStatusHost, 1U),
      1U,
      type3(0x3CU, 5U),
      3U,
      ac6demo::kXenosCoherStatusHost,
      0U,
      0x80000000U,
      8U,
      type0(0x1973U, 1U),
      0U,
      type3(0x3CU, 5U),
      3U,
      0x1973U,
      0U,
      1U,
      0x100U};
  const auto result = processor.process_batch(stream);
  assert(!result.pending_wait && result.effects.wait_reg_mem == 2U);
  assert(processor.register_value(ac6demo::kXenosCoherStatusHost) == 0U);
}

void test_malformed_suffix_is_rejected_before_prefix_commit() {
  const std::vector<std::uint32_t> stream{
      type0(0x20U, 1U), 0x22222222U, type3(0x3CU, 5U), 0x13U, 0x16AE2006U, 1U,
      0xFFFFFFFFU,      0x100U,      type3(0x7EU, 1U), 0U};
  ac6demo::XenosCommandProcessor processor;
  expect_trap([&] {
    static_cast<void>(processor.process_batch(
        stream, mapped_memory(std::array<std::byte, 4>{})));
  });
  assert(processor.register_value(0x20U) == 0U);
}

void test_gamma_lut_conditional_sequence() {
  std::vector<std::uint32_t> stream{
      type0(0x1921U, 1U), 0U, type0(0x1927U, 1U), 7U, type0(0x1922U, 1U), 0U};
  for (std::uint32_t index = 0U; index < 256U; ++index) {
    const std::uint32_t value = ((index & 0x3FFU) << 20U) |
                                ((index & 0x3FFU) << 10U) | (index & 0x3FFU);
    stream.insert(stream.end(),
                  {type0(0x1925U, 1U), value, type3(0x45U, 6U), 7U, 0x1925U,
                   value, 0xFFFFFFFFU, 0x1922U, index + 1U});
  }

  ac6demo::XenosCommandProcessor processor;
  const auto result = processor.process_batch(stream);
  assert(result.effects.conditional_write == 256U);
  assert(processor.register_value(0x1922U) == 256U);
  assert(processor.gamma_lut_component() == 0U);
  assert(processor.gamma_lut_value(0U) == 0U);
  assert(processor.gamma_lut_value(127U) == 0x07F1FC7FU);
  assert(processor.gamma_lut_value(255U) == 0x0FF3FCFFU);
}

void test_effect_variants_fail_closed() {
  const auto reject = [](std::vector<std::uint32_t> stream) {
    ac6demo::XenosCommandProcessor processor;
    expect_trap([&] { static_cast<void>(processor.process_batch(stream)); });
  };
  reject({type3(0x21U, 3U), 0x1841U, 0xFFFFFFFFU, 0U});
  reject({type3(0x3CU, 5U), 0x12U, 0x16AE2006U, 1U, 0xFFFFFFFFU, 0x100U});
  reject({type3(0x3CU, 5U), 4U, 0x1973U, 0U, 1U, 0x100U});
  reject({type3(0x45U, 6U), 0x107U, 0x16AE2006U, 1U, 0xFFFFFFFFU, 0x16AE1006U,
          1U});
  reject({type3(0x46U, 1U), 7U});
  reject({type3(0x54U, 1U), 8U});
  reject({type3(0x58U, 3U), 0x80000003U, 0x16AE1006U, 0U});
}

} // namespace

int main() {
  test_qualified_commands_and_immutable_draws();
  test_bin_predication_skip();
  test_reached_point_draw_shape();
  test_reached_quad_draw_shape();
  test_qualified_title_texture_dynamic_base();
  test_reached_pointer_shader_loads();
  test_reached_alu_constant_load();
  test_failures_have_zero_commit();
  test_transactional_effects_and_big_endian_bytes();
  test_scratch_writeback_produces_wait_values();
  test_bootstrap_scratch_writeback();
  test_renderer_write_boundaries();
  test_wait_commits_prefix_and_retries_at_header();
  test_register_wait_forms();
  test_malformed_suffix_is_rejected_before_prefix_commit();
  test_gamma_lut_conditional_sequence();
  test_effect_variants_fail_closed();
  std::cout << "ac6-demo-xenos-command-tests: ok\n";
  return 0;
}
