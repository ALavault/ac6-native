#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ac6demo {

inline constexpr std::size_t kXenosRegisterCount = 0x8000U;
inline constexpr std::uint16_t kXenosVgtDrawInitiator = 0x21FCU;
inline constexpr std::uint16_t kXenosVgtEventInitiator = 0x21F9U;
inline constexpr std::uint16_t kXenosCoherStatusHost = 0x0A31U;
inline constexpr std::uint16_t kXenosTextureFetch00 = 0x4800U;

class XenosRegisterSnapshot final {
public:
  XenosRegisterSnapshot() = default;
  explicit XenosRegisterSnapshot(
      std::array<std::uint32_t, kXenosRegisterCount> values)
      : values_(std::move(values)) {}

  [[nodiscard]] std::uint32_t value(std::uint16_t index) const noexcept {
    return values_[index];
  }

private:
  friend class XenosCommandProcessor;
  std::array<std::uint32_t, kXenosRegisterCount> values_{};
};

enum class XenosShaderStage : std::uint8_t { Vertex, Pixel };

struct XenosShaderLoadCommand final {
  XenosShaderStage stage{};
  std::uint16_t start_dword{};
  std::uint16_t size_dwords{};
  std::string guest_big_endian_sha256;
  // Runtime-only source for the pinned shader translator. These words are
  // never serialized into reports, traces or the installed package.
  std::vector<std::uint32_t> guest_big_endian_dwords;
};

enum class XenosPrimitive : std::uint8_t {
  PointList = 0x01U,
  RectangleList = 0x08U,
};
enum class XenosIndexSource : std::uint8_t { AutoIndex = 0x02U };
enum class XenosIndexFormat : std::uint8_t { Uint16, Uint32 };

struct XenosDrawCommand final {
  XenosPrimitive primitive{XenosPrimitive::RectangleList};
  XenosIndexSource source{XenosIndexSource::AutoIndex};
  XenosIndexFormat index_format{XenosIndexFormat::Uint16};
  std::uint16_t index_count{};
  bool predicated{};
  std::string vertex_shader_sha256;
  std::string pixel_shader_sha256;
  std::shared_ptr<const XenosRegisterSnapshot> registers;
};

struct XenosPresentCommand final {
  std::string resource_id;
  std::uint8_t format{};
  bool tiled{};
  std::uint32_t width{};
  std::uint32_t height{};
  // Exact physical destination carried by the reached XE_SWAP packet. This
  // remains a demo-observed address; it is not a host pointer or a generic
  // framebuffer allocation.
  std::uint32_t physical_address{};
};

using XenosCommand =
    std::variant<XenosShaderLoadCommand, XenosDrawCommand, XenosPresentCommand>;

struct XenosGuestMemoryWrite final {
  std::uint32_t address{};
  std::array<std::byte, 4> guest_bytes{};
};

struct XenosEffectCounters final {
  std::uint32_t scratch_writeback{};
  std::uint32_t register_rmw{};
  std::uint32_t wait_reg_mem{};
  std::uint32_t conditional_write{};
  std::uint32_t event_write{};
  std::uint32_t interrupt{};
  std::uint32_t event_write_shader_done{};
  std::uint32_t invalidate_state{};
  std::uint32_t micro_engine_init{};
};

struct XenosBatchResult final {
  std::vector<XenosCommand> renderer_commands;
  std::vector<XenosGuestMemoryWrite> memory_writes;
  std::vector<std::uint8_t> cpu_interrupts;
  XenosEffectCounters effects;
  std::size_t consumed_dwords{};
  bool pending_wait{};
  bool pending_wait_memory{};
  std::uint32_t pending_wait_address{};
  std::uint32_t pending_wait_observed{};
  std::uint32_t pending_wait_reference{};
  std::uint32_t pending_wait_mask{};
  std::uint32_t pending_wait_interval{};

  // Preserve iteration over the renderer subset for existing consumers.
  [[nodiscard]] auto begin() const noexcept {
    return renderer_commands.begin();
  }
  [[nodiscard]] auto end() const noexcept { return renderer_commands.end(); }
  [[nodiscard]] bool empty() const noexcept {
    return renderer_commands.empty();
  }
  [[nodiscard]] std::size_t size() const noexcept {
    return renderer_commands.size();
  }
  [[nodiscard]] const XenosCommand &operator[](std::size_t index) const {
    return renderer_commands[index];
  }
};

} // namespace ac6demo
