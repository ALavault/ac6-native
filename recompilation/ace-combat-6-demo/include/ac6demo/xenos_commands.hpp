#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
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

enum class QualifiedTitleTextureEncoding : std::uint8_t { Bc3, Rgba8 };

struct QualifiedTitleTextureProfile final {
  std::uint32_t address{};
  std::uint32_t payload_size{};
  std::uint32_t width{};
  std::uint32_t height{};
  QualifiedTitleTextureEncoding encoding{QualifiedTitleTextureEncoding::Bc3};
};

[[nodiscard]] inline std::optional<QualifiedTitleTextureProfile>
qualified_title_texture_profile(
    const XenosRegisterSnapshot &registers) noexcept {
  std::array<std::uint32_t, 6> words{};
  for (std::uint16_t index = 0U; index < words.size(); ++index) {
    words[index] = registers.value(kXenosTextureFetch00 + index);
  }
  const auto address = words[1] & 0xFFFFF000U;
  words[1] &= 0x00000FFFU;
  if (address == 0U) {
    return std::nullopt;
  }

  struct Shape final {
    std::array<std::uint32_t, 6> words;
    std::uint32_t required_address;
    std::uint32_t payload_size;
    std::uint32_t width;
    std::uint32_t height;
    QualifiedTitleTextureEncoding encoding;
  };
  static constexpr std::array shapes{
      Shape{{0x81000002U, 0x00000054U, 0x0007E03FU, 0x01280D10U,
             0x00000003U, 0x00000200U},
            0U, 0x4000U, 64U, 64U, QualifiedTitleTextureEncoding::Bc3},
      Shape{{0x84004802U, 0x00000054U, 0x003FE1FFU, 0x01280D10U,
             0x00000003U, 0x00000200U},
            0U, 0x40000U, 512U, 512U,
            QualifiedTitleTextureEncoding::Bc3},
      Shape{{0x84004802U, 0x00000054U, 0x0059E1FFU, 0x01280D10U,
             0x00000003U, 0x00000200U},
            0U, 0x60000U, 512U, 720U,
            QualifiedTitleTextureEncoding::Bc3},
      Shape{{0x8A004802U, 0x00000054U, 0x0059E4FFU, 0x01280D10U,
             0x00000003U, 0x00000200U},
            0U, 0xF0000U, 1280U, 720U,
            QualifiedTitleTextureEncoding::Bc3},
      // The eighth brandLogo sibling. The movie reaches it at root frame
      // 1562 of 2220 and the run trapped here at tick 4911 with
      // f0=0x83004802 f2=0x0013E13F; the Xenos size word decodes as
      // width-1 = 0x13F (320) and height-1 = 0x9F (160). The payload size is
      // read, not derived from the format: 008_NTXR is 102400 bytes on disk,
      // and file size minus the 4096-byte header reproduces the payload of
      // all seven already-qualified siblings exactly.
      Shape{{0x83004802U, 0x00000054U, 0x0013E13FU, 0x01280D10U,
             0x00000003U, 0x00000200U},
            0U, 0x18000U, 320U, 160U,
            QualifiedTitleTextureEncoding::Bc3},
      Shape{{0x8A000002U, 0x00000006U, 0x0059E4FFU, 0x00001414U,
             0x00000000U, 0x00000200U},
            0x1374A000U, 0x398000U, 1280U, 720U,
            QualifiedTitleTextureEncoding::Rgba8},
  };
  for (const auto &shape : shapes) {
    if (words == shape.words &&
        (shape.required_address == 0U || address == shape.required_address)) {
      return QualifiedTitleTextureProfile{
          address, shape.payload_size, shape.width, shape.height,
          shape.encoding};
    }
  }
  return std::nullopt;
}

enum class XenosShaderStage : std::uint8_t { Vertex, Pixel };

struct XenosShaderLoadCommand final {
  XenosShaderStage stage{};
  std::uint16_t start_dword{};
  std::uint16_t size_dwords{};
  std::string guest_big_endian_sha256;
  // Runtime-only source for the pinned shader translator. These words are
  // never serialized into reports, traces or the installed package.
  std::vector<std::uint32_t> guest_big_endian_dwords;
  // Zero for an immediate load, otherwise the exact guest source carried by
  // the qualified pointer-load packet. This is runtime-only provenance.
  std::uint32_t guest_source_address{};
};

enum class XenosPrimitive : std::uint8_t {
  PointList = 0x01U,
  RectangleList = 0x08U,
  QuadList = 0x0DU,
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

struct XenosRendererPayload final {
  std::uint32_t address{};
  std::vector<std::byte> bytes;
};

// One command-processor producer boundary plus the exact guest bytes observed
// after that boundary. This is runtime-only and is never serialized.
struct XenosRendererBatch final {
  std::uint64_t sequence{};
  std::vector<XenosCommand> commands;
  std::vector<XenosRendererPayload> payloads;
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
  // Number of staged guest writes visible when each renderer command was
  // emitted. This keeps payload capture at the command's point of use.
  std::vector<std::size_t> renderer_write_counts;
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
