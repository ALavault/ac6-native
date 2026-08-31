#include "ac6/native_vulkan_backend.h"

#include <type_traits>

namespace ac6::native {

namespace {
bool valid_primitive(std::uint32_t primitive) noexcept {
  return (primitive >= 1u && primitive <= 8u) ||
         (primitive >= 0x0Cu && primitive <= 0x12u);
}
}  // namespace

bool VulkanBackend::submit(const XenosState& state,
                           std::span<const XenosCommand> commands) {
  error_.clear();
  std::uint64_t draws = 0u;
  std::uint64_t resolves = 0u;
  std::uint64_t presents = 0u;
  for (const XenosCommand& command : commands) {
    const bool accepted = std::visit(
        [&](const auto& packet) -> bool {
          using Packet = std::decay_t<decltype(packet)>;
          if constexpr (std::is_same_v<Packet, DrawPacket>) {
            if (packet.vertex_count == 0u || packet.index_count == 0u ||
                packet.vertex_stride == 0u || !valid_primitive(packet.primitive_type) ||
                packet.index_format > 1u) {
              error_ = "DRAW packet has no drawable vertices";
              return false;
            }
            if ((packet.texture_dimension == 1u && !capabilities_.texture_2d) ||
                (packet.texture_dimension == 2u && !capabilities_.texture_3d) ||
                (packet.texture_dimension == 3u && !capabilities_.texture_cube) ||
                packet.texture_dimension > 3u) {
              error_ = "Vulkan backend lacks the requested texture dimension";
              return false;
            }
            if (packet.uses_blend_scissor && !capabilities_.blend_scissor) {
              error_ = "Vulkan backend lacks blend/scissor capability";
              return false;
            }
            if (packet.uses_primitive_restart && !capabilities_.primitive_restart) {
              error_ = "Vulkan backend lacks primitive-restart capability";
              return false;
            }
            const std::uint64_t vertex_bytes =
                static_cast<std::uint64_t>(packet.vertex_count) *
                packet.vertex_stride;
            const std::uint64_t index_bytes =
                static_cast<std::uint64_t>(packet.index_count) *
                (packet.index_format == 0u ? 2u : 4u);
            if ((packet.vertex_address != 0u &&
                 (vertex_bytes == 0u ||
                  static_cast<std::uint64_t>(packet.vertex_address) +
                          vertex_bytes > (1ull << 32u))) ||
                (packet.index_address != 0u &&
                 (index_bytes == 0u ||
                  static_cast<std::uint64_t>(packet.index_address) +
                          index_bytes > (1ull << 32u)))) {
              error_ = "DRAW packet guest range overflows 32-bit address space";
              return false;
            }
            ++draws;
            return true;
          } else if constexpr (std::is_same_v<Packet, ResolvePacket>) {
            if (!capabilities_.resolves || !capabilities_.edram_aliasing) {
              error_ = "Vulkan backend lacks EDRAM resolve capability";
              return false;
            }
            const std::uint64_t source_bytes =
                static_cast<std::uint64_t>(packet.width) * packet.height *
                packet.sample_count * 4u;
            if (packet.source_edram >= state.edram_bytes() ||
                source_bytes == 0u ||
                static_cast<std::uint64_t>(packet.source_edram) + source_bytes >
                    state.edram_bytes() ||
                packet.destination_surface >= 16u ||
                packet.width == 0u || packet.width > 4096u ||
                packet.height == 0u || packet.height > 4096u ||
                (packet.sample_count != 1u && packet.sample_count != 2u &&
                 packet.sample_count != 4u && packet.sample_count != 8u) ||
                static_cast<std::uint32_t>(packet.color_endian) > 3u) {
              error_ = "RESOLVE packet references invalid EDRAM or dimensions";
              return false;
            }
            if (packet.sample_count > 1u && !capabilities_.msaa) {
              error_ = "Vulkan backend lacks MSAA capability";
              return false;
            }
            if (packet.depth && !capabilities_.depth_stencil) {
              error_ = "Vulkan backend lacks depth/stencil capability";
              return false;
            }
            ++resolves;
            return true;
          } else if constexpr (std::is_same_v<Packet, PresentPacket>) {
            if (packet.surface >= 16u || packet.width == 0u ||
                packet.height == 0u) {
              error_ = "PRESENT packet references invalid surface";
              return false;
            }
            ++presents;
            return true;
          } else if constexpr (std::is_same_v<Packet, WaitPacket>) {
            if (packet.selector > 7u) {
              error_ = "WAIT selector is not qualified";
              return false;
            }
            return true;
          } else if constexpr (std::is_same_v<Packet, IndirectBufferPacket>) {
            error_ = "INDIRECT_BUFFER requires a bound guest-memory resolver";
            return false;
          } else if constexpr (std::is_same_v<Packet, MicroEngineInitPacket>) {
            // The packet is a qualified Xenon bootstrap envelope. Its payload
            // is retained by the command stream but never run as host code.
            return true;
          } else if constexpr (std::is_same_v<Packet, EventWriteShdPacket>) {
            return (packet.initiator & ~0x8000003Fu) == 0u;
          } else if constexpr (std::is_same_v<Packet, ImmediateShaderPacket>) {
            return packet.shader_type <= 1u && packet.start == 0u &&
                   packet.dword_count != 0u;
          }
          return false;
        },
        command);
    if (!accepted) return false;
  }
  draw_count_ += draws;
  resolve_count_ += resolves;
  present_count_ += presents;
  return true;
}

}  // namespace ac6::native
