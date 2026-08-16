#include "ac6demo/rexglue_runtime_shader.hpp"

#include "ac6demo/runtime_error.hpp"

#include <cassert>
#include <array>
#include <cstring>
#include <iostream>

int main() {
  ac6demo::XenosShaderLoadCommand unknown;
  unknown.stage = ac6demo::XenosShaderStage::Vertex;
  unknown.size_dwords = 3U;
  unknown.guest_big_endian_sha256 = std::string(64U, '0');
  unknown.guest_big_endian_dwords = {1U, 2U, 3U};
  bool rejected = false;
  try {
    (void)ac6demo::translate_reached_shader_spirv(unknown, 1U);
  } catch (const ac6demo::RuntimeTrap &) {
    rejected = true;
  }
  assert(rejected);

  const auto require_source = [](std::uint16_t dwords, const char *hash,
                                 std::uint32_t start,
                                 std::uint32_t end_exclusive) {
    ac6demo::XenosShaderLoadCommand shader;
    shader.stage = ac6demo::XenosShaderStage::Vertex;
    shader.size_dwords = dwords;
    shader.guest_big_endian_sha256 = hash;
    const auto source = ac6demo::qualified_reached_shader_image_source(shader);
    assert(source.has_value());
    assert(source->start == start);
    assert(source->end_exclusive == end_exclusive);
    assert(source->end_exclusive - source->start == dwords * 4U);
  };
  require_source(24U,
                 "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3",
                 0x82013E20U, 0x82013E80U);
  require_source(27U,
                 "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b",
                 0x820140A0U, 0x8201410CU);
  require_source(15U,
                 "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0",
                 0x82014140U, 0x8201417CU);
  ac6demo::XenosShaderLoadCommand wrong_source;
  wrong_source.stage = ac6demo::XenosShaderStage::Vertex;
  wrong_source.size_dwords = 27U;
  wrong_source.guest_big_endian_sha256 = std::string(64U, 'f');
  assert(!ac6demo::qualified_reached_shader_image_source(wrong_source));

  std::array<std::uint32_t, ac6demo::kXenosRegisterCount> registers{};
  registers[0x2180U] = 0U;
  auto snapshot =
      std::make_shared<ac6demo::XenosRegisterSnapshot>(std::move(registers));
  ac6demo::XenosDrawCommand draw;
  draw.index_count = 3U;
  draw.vertex_shader_sha256 = unknown.guest_big_endian_sha256;
  draw.pixel_shader_sha256 = unknown.guest_big_endian_sha256;
  draw.registers = std::move(snapshot);
  std::vector<ac6demo::XenosCommand> commands;
  commands.emplace_back(unknown);
  commands.emplace_back(std::move(draw));
  ac6demo::ReachedShaderRuntimeCache cache;
  rejected = false;
  try {
    cache.consume(commands);
  } catch (const ac6demo::RuntimeTrap &) {
    rejected = true;
  }
  assert(rejected);
  assert(cache.stats().shader_loads == 0U);
  assert(cache.stats().draws == 0U);
  assert(cache.stats().translated_modules == 0U);

  std::array<std::uint32_t, ac6demo::kXenosRegisterCount> reached_regs{};
  reached_regs[0x2000U] = 0x0A020280U;
  reached_regs[0x2002U] = 0x000102D0U;
  reached_regs[0x200FU] = 0x20002000U;
  reached_regs[0x2100U] = 0x0000FFFFU;
  reached_regs[0x210FU] = 0x44200000U;
  reached_regs[0x2110U] = 0x44200000U;
  reached_regs[0x2111U] = 0xC3B40000U;
  reached_regs[0x2112U] = 0x43B40000U;
  reached_regs[0x2113U] = 0x3F800000U;
  reached_regs[0x2200U] = 0x00008777U;
  reached_regs[0x2204U] = 0x00010000U;
  reached_regs[0x2205U] = 0x00010000U;
  reached_regs[0x2206U] = 0x00000300U;
  reached_regs[0x4800U] = 0x127CA03FU;
  reached_regs[0x4801U] = 0x10000056U;
  ac6demo::XenosDrawCommand reached_draw;
  reached_draw.index_count = 3U;
  reached_draw.vertex_shader_sha256 = "vertex";
  reached_draw.pixel_shader_sha256 = "pixel";
  reached_draw.registers = std::make_shared<ac6demo::XenosRegisterSnapshot>(
      std::move(reached_regs));
  ac6demo::ReachedShaderSpirv reached_vertex;
  reached_vertex.stage = ac6demo::XenosShaderStage::Vertex;
  reached_vertex.microcode_sha256 = "vertex";
  ac6demo::ReachedShaderSpirv reached_pixel;
  reached_pixel.stage = ac6demo::XenosShaderStage::Pixel;
  reached_pixel.microcode_sha256 = "pixel";
  const auto payloads = ac6demo::build_reached_constant_payloads(
      reached_draw, reached_vertex, reached_pixel, 16384U, 16384U);
  assert(payloads.system.size() == 504U);
  assert(payloads.float_vertex.size() == 16U);
  assert(payloads.float_pixel.size() == 16U);
  assert(payloads.bool_loop.size() == 160U);
  assert(payloads.fetch.size() == 768U);
  std::uint32_t flags = 0U;
  std::memcpy(&flags, payloads.system.data(), sizeof(flags));
  assert(flags == 0x00074B00U);
  std::uint32_t fetch0 = 0U;
  std::uint32_t fetch1 = 0U;
  std::memcpy(&fetch0, payloads.fetch.data(), sizeof(fetch0));
  std::memcpy(&fetch1, payloads.fetch.data() + sizeof(fetch0), sizeof(fetch1));
  assert(fetch0 == 0x127CA03FU);
  assert(fetch1 == 0x10000056U);
  std::array<std::uint32_t, ac6demo::kXenosRegisterCount> copy_regs{};
  copy_regs[0x2000U] = 0x14000500U;
  copy_regs[0x2002U] = 0x000102D0U;
  copy_regs[0x200FU] = 0x20002000U;
  copy_regs[0x2100U] = 0x0000FFFFU;
  copy_regs[0x210FU] = 0x44200000U;
  copy_regs[0x2110U] = 0x44200000U;
  copy_regs[0x2111U] = 0xC3B40000U;
  copy_regs[0x2112U] = 0x43B40000U;
  copy_regs[0x2113U] = 0x3F800000U;
  copy_regs[0x2202U] = 0x87000007U;
  copy_regs[0x2204U] = 0x00010000U;
  copy_regs[0x2205U] = 0x00010000U;
  copy_regs[0x2206U] = 0x00000300U;
  copy_regs[0x4800U] = 0x127CA093U;
  copy_regs[0x4801U] = 0x1000001AU;
  reached_draw.vertex_shader_sha256 = "copy-vertex";
  reached_draw.registers = std::make_shared<ac6demo::XenosRegisterSnapshot>(
      std::move(copy_regs));
  reached_vertex.microcode_sha256 = "copy-vertex";
  const auto copy_payloads = ac6demo::build_reached_constant_payloads(
      reached_draw, reached_vertex, reached_pixel, 16384U, 16384U);
  std::memcpy(&flags, copy_payloads.system.data(), sizeof(flags));
  assert(flags == 0x00070B00U);
  std::cout << "ac6-demo-rexglue-runtime-tests: fail-closed ok\n";
  return 0;
}
