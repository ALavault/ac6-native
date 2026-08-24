#pragma once

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace ac6demo {

struct XenosDrawCommand;

struct VulkanNormalDrawImage final {
  VkImage image{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  VkImageView view{VK_NULL_HANDLE};
};

struct VulkanNormalDrawTarget final {
  VulkanNormalDrawImage color;
  VulkanNormalDrawImage depth;
  VulkanNormalDrawImage resolved;
  VkFramebuffer framebuffer{VK_NULL_HANDLE};

  [[nodiscard]] bool populated() const noexcept {
    return framebuffer != VK_NULL_HANDLE;
  }
};

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
    VkDescriptorSet constants, VulkanNormalDrawTarget &target,
    bool load_existing, bool *cleanup_safe);

void destroy_vulkan_normal_draw_target(
    VkDevice device, VulkanNormalDrawTarget &target) noexcept;

[[nodiscard]] VulkanNormalDrawResult execute_vulkan_title_draw(
    VkPhysicalDevice physical, VkDevice device, VkQueue queue,
    std::uint32_t queue_family, const XenosDrawCommand &draw,
    VkRenderPass render_pass, VkPipeline pipeline,
    VkPipelineLayout pipeline_layout, VkDescriptorSet shared,
    VkDescriptorSet constants, VkDescriptorSetLayout empty_layout,
    VkDescriptorSetLayout texture_layout,
    std::span<const std::byte> tiled_texture, std::uint32_t texture_width,
    std::uint32_t texture_height, VulkanNormalDrawTarget &target,
    bool load_existing, bool *cleanup_safe);

} // namespace ac6demo

#endif
