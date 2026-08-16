#pragma once

#include "ac6demo/runtime_error.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ac6demo {

using XenosSwapPacket = std::array<std::uint32_t, 64>;

// Build the bounded Xenos stream produced by the reached VdSwap ABI. This
// does not decode, translate or present the packet.
[[nodiscard]] XenosSwapPacket
make_xenos_swap_packet(const std::array<std::uint32_t, 6> &fetch_words,
                       std::uint32_t frontbuffer_physical_address,
                       std::uint32_t width, std::uint32_t height);

enum class GraphicsBackend : std::uint8_t { Headless, Vulkan };

struct GraphicsProfile final {
  static constexpr std::uint32_t width = 1280;
  static constexpr std::uint32_t height = 720;
  static constexpr std::uint32_t refresh_hz = 60;
  static constexpr std::uint32_t present_interval = 2;
  static constexpr bool letterbox = true;

  GraphicsBackend backend{GraphicsBackend::Headless};
};

enum class XenosFormat : std::uint8_t { Rgba8, Bgra8, Rgba16Float, D24S8 };

struct GraphicsStats final {
  std::uint64_t frame{};
  std::uint32_t clears{};
  std::uint32_t draws{};
  std::uint32_t resolves{};
  std::uint32_t presents{};
};

class D3D9LtcgHle final {
public:
  explicit D3D9LtcgHle(GraphicsProfile profile) : profile_(profile) {}

  void qualify_function(std::uint32_t address, std::string name);
  void begin_frame(std::uint64_t tick);
  void set_render_state(std::uint32_t state, std::uint32_t value);
  void create_resource(std::uint32_t id, XenosFormat format,
                       std::uint32_t width, std::uint32_t height);
  void audit_shader(std::uint32_t id, std::span<const std::uint32_t> opcodes);
  void clear(std::uint32_t rgba);
  void draw(std::uint32_t vertex_count, std::uint32_t index_count);
  void resolve(std::uint32_t resource_id);
  void present(std::uint64_t tick);

  [[nodiscard]] const GraphicsStats &stats() const noexcept { return stats_; }
  [[nodiscard]] std::uint64_t current_tick() const noexcept {
    return current_tick_;
  }

private:
  struct Resource final {
    XenosFormat format;
    std::uint32_t width;
    std::uint32_t height;
  };

  GraphicsProfile profile_;
  std::unordered_map<std::uint32_t, std::string> qualified_functions_;
  std::unordered_map<std::uint32_t, Resource> resources_;
  std::unordered_set<std::uint32_t> audited_shaders_;
  std::unordered_map<std::uint32_t, std::uint32_t> render_states_;
  GraphicsStats stats_{};
  std::uint64_t current_tick_{};
  bool frame_open_{};
};

class VulkanBackend final {
public:
  explicit VulkanBackend(GraphicsProfile profile) : profile_(profile) {}

  void submit(const D3D9LtcgHle &hle);
  [[nodiscard]] const GraphicsProfile &profile() const noexcept {
    return profile_;
  }
  [[nodiscard]] std::uint32_t validated_presents() const noexcept {
    return presents_;
  }

private:
  GraphicsProfile profile_;
  std::uint32_t presents_{};
};

} // namespace ac6demo
