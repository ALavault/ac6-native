#pragma once

#include "ac6/native_xenos.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace ac6::native {

// Capability contract for the product Vulkan backend.  Hardware-facing Vulkan
// object creation lives behind this narrow boundary; unsupported Xenos state
// is rejected instead of silently mapped to an arbitrary host default.
struct VulkanCapabilities final {
  bool edram_aliasing{true};
  bool msaa{true};
  bool resolves{true};
  bool texture_2d{true};
  bool texture_3d{true};
  bool texture_cube{true};
  bool depth_stencil{true};
  bool blend_scissor{true};
  bool primitive_restart{true};
};

class VulkanBackend final {
 public:
  explicit VulkanBackend(VulkanCapabilities capabilities = {}) noexcept
      : capabilities_(capabilities) {}

  [[nodiscard]] bool submit(const XenosState& state,
                            std::span<const XenosCommand> commands);
  [[nodiscard]] const std::string& error() const noexcept { return error_; }
  [[nodiscard]] std::uint64_t present_count() const noexcept {
    return present_count_;
  }
  [[nodiscard]] std::uint64_t draw_count() const noexcept { return draw_count_; }
  [[nodiscard]] std::uint64_t resolve_count() const noexcept {
    return resolve_count_;
  }

 private:
  VulkanCapabilities capabilities_;
  std::string error_;
  std::uint64_t present_count_{};
  std::uint64_t draw_count_{};
  std::uint64_t resolve_count_{};
};

}  // namespace ac6::native
