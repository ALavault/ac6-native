#pragma once

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace ac6demo {

struct XenosDrawCommand;

struct VulkanNormalDrawResult final {
  std::string resolved_rgba8_sha256;
  std::uint32_t width{};
  std::uint32_t height{};
  // Host readback retained only for the reached neutral-frame join.  It is
  // never treated as a general Xenos EDRAM dump.
  std::vector<std::byte> resolved_rgba8;
};

[[nodiscard]] VulkanNormalDrawResult execute_vulkan_normal_draw(
    VkPhysicalDevice physical, VkDevice device, VkQueue queue,
    std::uint32_t queue_family, const XenosDrawCommand &draw,
    VkRenderPass render_pass, VkPipeline pipeline,
    VkPipelineLayout pipeline_layout, VkDescriptorSet shared,
    VkDescriptorSet constants, bool *cleanup_safe);

} // namespace ac6demo

#endif
