#include "ac6demo/graphics.hpp"

#include <algorithm>
#include <array>

namespace ac6demo {

XenosSwapPacket
make_xenos_swap_packet(const std::array<std::uint32_t, 6> &fetch_words,
                       std::uint32_t frontbuffer_physical_address,
                       std::uint32_t width, std::uint32_t height) {
  if (frontbuffer_physical_address == 0U || width == 0U || height == 0U) {
    throw RuntimeTrap("invalid Xenos swap packet dimensions or address");
  }
  // BSD-licensed ReXGlue dcd41b VdSwap / Xenos wire contract. Keeping the
  // numeric packet values here does not link the ReXGlue runtime.
  constexpr std::uint32_t kFetchPacket = 0x00054800U;
  constexpr std::uint32_t kSwapPacket = 0xC0036400U;
  constexpr std::uint32_t kSwapSignature = 0x53574150U;
  constexpr std::uint32_t kType2Nop = 0x80000000U;
  XenosSwapPacket packet;
  packet.fill(kType2Nop);
  packet[0] = kFetchPacket;
  std::copy(fetch_words.begin(), fetch_words.end(), packet.begin() + 1);
  packet[7] = kSwapPacket;
  packet[8] = kSwapSignature;
  packet[9] = frontbuffer_physical_address;
  packet[10] = width;
  packet[11] = height;
  return packet;
}

void D3D9LtcgHle::qualify_function(std::uint32_t address, std::string name) {
  if (address == 0U || name.empty()) {
    throw RuntimeTrap("invalid D3D9LTCG function qualification", current_tick_,
                      0, address);
  }
  qualified_functions_[address] = std::move(name);
}

void D3D9LtcgHle::begin_frame(std::uint64_t tick) {
  if (frame_open_) {
    throw RuntimeTrap("D3D frame began before previous PRESENT", tick);
  }
  current_tick_ = tick;
  frame_open_ = true;
  ++stats_.frame;
}

void D3D9LtcgHle::set_render_state(std::uint32_t state, std::uint32_t value) {
  if (!frame_open_ || state > 0x400U) {
    throw RuntimeTrap("unqualified D3D render state", current_tick_, 0, state);
  }
  render_states_[state] = value;
}

void D3D9LtcgHle::create_resource(std::uint32_t id, XenosFormat format,
                                  std::uint32_t width, std::uint32_t height) {
  if (!frame_open_ || id == 0U || width == 0U || height == 0U ||
      width > GraphicsProfile::width || height > GraphicsProfile::height) {
    throw RuntimeTrap("unqualified Xenos resource", current_tick_, 0, id);
  }
  resources_[id] = Resource{format, width, height};
}

void D3D9LtcgHle::audit_shader(std::uint32_t id,
                               std::span<const std::uint32_t> opcodes) {
  if (!frame_open_ || id == 0U || opcodes.empty()) {
    throw RuntimeTrap("empty or unqualified Xenos shader", current_tick_, 0,
                      id);
  }
  for (const std::uint32_t opcode : opcodes) {
    // The local XenosRecomp audit supplies this compact opcode class. 0xFF
    // is reserved for an unknown instruction and is deliberately fatal.
    if (((opcode >> 24U) & 0xffU) == 0xffU) {
      throw RuntimeTrap("unknown Xenos shader opcode", current_tick_, 0,
                        opcode);
    }
  }
  audited_shaders_.insert(id);
}

void D3D9LtcgHle::clear(std::uint32_t /*rgba*/) {
  if (!frame_open_) {
    throw RuntimeTrap("D3D clear outside frame", current_tick_);
  }
  ++stats_.clears;
}

void D3D9LtcgHle::draw(std::uint32_t vertex_count, std::uint32_t index_count) {
  if (!frame_open_ || (vertex_count == 0U && index_count == 0U)) {
    throw RuntimeTrap("unqualified or empty D3D draw", current_tick_);
  }
  ++stats_.draws;
}

void D3D9LtcgHle::resolve(std::uint32_t resource_id) {
  if (!frame_open_ || resources_.find(resource_id) == resources_.end()) {
    throw RuntimeTrap("resolve references unknown Xenos resource",
                      current_tick_, 0, resource_id);
  }
  ++stats_.resolves;
}

void D3D9LtcgHle::present(std::uint64_t tick) {
  if (!frame_open_ || tick < current_tick_ ||
      ((tick - current_tick_) % GraphicsProfile::present_interval) != 0U) {
    throw RuntimeTrap("presentation violates qualified vblank interval", tick);
  }
  current_tick_ = tick;
  ++stats_.presents;
  frame_open_ = false;
}

void VulkanBackend::submit(const D3D9LtcgHle &hle) {
  if (profile_.backend != GraphicsBackend::Vulkan ||
      hle.stats().presents == 0U) {
    throw RuntimeTrap("Vulkan submission has no qualified presentation");
  }
  // This class is the bounded HLE boundary. A platform build may attach a
  // Vulkan command translator here; no CPU rasterizer or asynchronous
  // substitute is permitted.
  presents_ += hle.stats().presents;
}

} // namespace ac6demo
