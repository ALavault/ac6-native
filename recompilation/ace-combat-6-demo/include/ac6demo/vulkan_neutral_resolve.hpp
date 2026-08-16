#pragma once

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include "ac6demo/vulkan_normal_draw.hpp"
#include "ac6demo/xenos_commands.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace ac6demo {

struct VulkanNeutralResolveResult final {
  std::string linear_rgba8_sha256;
  std::string tiled_rgba8_sha256;
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t destination_address{};
  std::uint32_t destination_bytes{};
  bool present_joined{};
  // Exact tiled bytes produced by the reached resolve.  This is an ephemeral
  // handoff to DemoSession's qualified guest allocation; it is never emitted
  // into a trace/report or retained as an asset.
  std::vector<std::byte> tiled_bytes;
  std::string guest_tiled_rgba8_sha256;
  std::string guest_linear_rgba8_sha256;
  bool guest_writeback{};
};

[[nodiscard]] VulkanNeutralResolveResult execute_vulkan_neutral_resolve(
    VkPhysicalDevice physical, VkDevice device, VkQueue queue,
    std::uint32_t queue_family, const VulkanNormalDrawResult &normal,
    const XenosDrawCommand &copy, const XenosPresentCommand &present);

} // namespace ac6demo

#endif
