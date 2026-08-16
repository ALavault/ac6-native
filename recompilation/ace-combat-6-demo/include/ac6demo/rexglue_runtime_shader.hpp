#pragma once

#include "ac6demo/xenos_commands.hpp"

#include <cstdint>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace ac6demo {

struct ReachedShaderImageSource final {
  std::uint32_t start{};
  std::uint32_t end_exclusive{};
};

[[nodiscard]] std::optional<ReachedShaderImageSource>
qualified_reached_shader_image_source(const XenosShaderLoadCommand &shader) noexcept;

struct ReachedShaderSpirv final {
  XenosShaderStage stage{};
  std::string microcode_sha256;
  std::string spirv_sha256;
  std::vector<std::uint32_t> words;
  std::array<std::uint64_t, 4> float_bitmap{};
  std::array<std::uint32_t, 8> bool_bitmap{};
  std::uint32_t loop_bitmap{};
  std::uint32_t float_count{};
  bool float_dynamic_addressing{};
  std::optional<ReachedShaderImageSource> image_source;
};

struct ReachedConstantPayloads final {
  std::vector<std::byte> system;
  std::vector<std::byte> float_vertex;
  std::vector<std::byte> float_pixel;
  std::array<std::byte, 160> bool_loop{};
  std::array<std::byte, 768> fetch{};
};

[[nodiscard]] ReachedConstantPayloads build_reached_constant_payloads(
    const XenosDrawCommand &draw, const ReachedShaderSpirv &vertex,
    const ReachedShaderSpirv &pixel, std::uint32_t viewport_x_max,
    std::uint32_t viewport_y_max);

// Generic ReXGlue translation constrained to exact shader identities reached
// in the qualified PAL demo. No input, disassembly or output is serialized.
[[nodiscard]] ReachedShaderSpirv
translate_reached_shader_spirv(const XenosShaderLoadCommand &shader,
                               std::uint32_t register_count);

struct ReachedShaderRuntimeStats final {
  std::uint32_t shader_loads{};
  std::uint32_t draws{};
  std::uint32_t presents{};
  std::uint32_t translated_modules{};
};

class ReachedShaderRuntimeCache final {
public:
  void consume(std::span<const XenosCommand> commands);
  [[nodiscard]] const ReachedShaderSpirv *
  module(std::string_view microcode_sha256) const noexcept;
  [[nodiscard]] const ReachedShaderRuntimeStats &stats() const noexcept {
    return stats_;
  }

private:
  std::unordered_map<std::string, XenosShaderLoadCommand> shader_loads_;
  std::unordered_map<std::string, ReachedShaderSpirv> modules_;
  ReachedShaderRuntimeStats stats_{};
};

} // namespace ac6demo
