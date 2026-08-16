#pragma once

#include "ac6demo/guest_memory.hpp"
#include "ac6demo/xenos_commands.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Read-only PAL evidence for the bootstrap point draws.  This deliberately
// emits raw register/fetch values only; no Xenos semantic name is inferred.
inline void trace_xenos_point_draw(const ac6demo::XenosDrawCommand &draw,
                                   const ac6demo::GuestMemory &memory,
                                   std::uint64_t tick,
                                   std::uint32_t thread) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_POINT_DRAWS") != nullptr;
  static std::uint32_t record_count = 0U;
  if (!enabled || draw.primitive != ac6demo::XenosPrimitive::PointList ||
      !draw.registers || record_count >= 64U) {
    return;
  }
  ++record_count;
  const auto &regs = *draw.registers;
  const auto fetch0 = regs.value(ac6demo::kXenosTextureFetch00);
  const auto fetch1 = regs.value(ac6demo::kXenosTextureFetch00 + 1U);
  const auto address = (fetch0 >> 2U) * 4U;
  const auto dwords = (fetch1 >> 2U) & 0x00FFFFFFU;
  std::fprintf(
      stderr,
      "AC6_POINT_DRAW index=%u tick=%llu thread=%u predicated=%u "
      "vs=%s ps=%s surface=0x%08X color_mask=0x%08X "
      "color_control=0x%08X depth=0x%08X depth1=0x%08X mode=0x%08X "
      "fetch0=0x%08X fetch1=0x%08X address=0x%08X dwords=%u endian=%u\n",
      record_count - 1U, static_cast<unsigned long long>(tick), thread,
      draw.predicated ? 1U : 0U, draw.vertex_shader_sha256.c_str(),
      draw.pixel_shader_sha256.c_str(), regs.value(0x2000U),
      regs.value(0x2104U), regs.value(0x2180U), regs.value(0x2200U),
      regs.value(0x2201U), regs.value(0x2208U), fetch0, fetch1, address,
      dwords, fetch1 & 3U);
  if ((fetch0 & 3U) != 3U || dwords == 0U || dwords > 64U ||
      !memory.mapped(address, dwords * 4U)) {
    return;
  }
  const auto bytes = memory.load_bytes(address, dwords * 4U);
  std::fprintf(stderr, "AC6_POINT_FETCH address=0x%08X bytes=", address);
  for (const auto byte : bytes) {
    std::fprintf(stderr, "%02X",
                 static_cast<unsigned>(std::to_integer<std::uint8_t>(byte)));
  }
  std::fputc('\n', stderr);
}
