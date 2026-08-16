#pragma once

#include "ac6demo/runtime_error.hpp"
#include "ac6demo/xenos_commands.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace ac6demo {

[[nodiscard]] inline std::optional<XenosPresentCommand>
single_xenos_present(std::span<const XenosCommand> commands) {
  std::optional<XenosPresentCommand> result;
  for (const auto &command : commands) {
    const auto *present = std::get_if<XenosPresentCommand>(&command);
    if (present == nullptr) {
      continue;
    }
    if (result.has_value()) {
      throw RuntimeTrap("multiple Xenos presentations in batch");
    }
    result = *present;
  }
  return result;
}

[[nodiscard]] inline bool
has_reached_copy_draw(std::span<const XenosCommand> commands) {
  constexpr std::string_view kCopyVertex =
      "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0";
  constexpr std::string_view kPixel =
      "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25";
  bool copy = false;
  for (const auto &command : commands) {
    const auto *draw = std::get_if<XenosDrawCommand>(&command);
    if (draw == nullptr || draw->primitive != XenosPrimitive::RectangleList ||
        draw->index_count != 3U || draw->source != XenosIndexSource::AutoIndex ||
        draw->pixel_shader_sha256 != kPixel) {
      continue;
    }
    copy = copy || draw->vertex_shader_sha256 == kCopyVertex;
  }
  return copy;
}

} // namespace ac6demo
