#pragma once

#include "ac6/vulkan_backend.h"

#include <vulkan/vulkan.h>

#include <functional>
#include <optional>
#include <unordered_map>

namespace ac6 {

struct VulkanMeshResource {
  VkBuffer vertex_buffer{VK_NULL_HANDLE};
  VkDeviceMemory vertex_memory{VK_NULL_HANDLE};
  VkBuffer index_buffer{VK_NULL_HANDLE};
  VkDeviceMemory index_memory{VK_NULL_HANDLE};
  std::uint32_t index_count{};
};

struct VulkanRenderTargetResource {
  VkImage color_image{VK_NULL_HANDLE};
  VkDeviceMemory color_memory{VK_NULL_HANDLE};
  VkImageView color_view{VK_NULL_HANDLE};
  VkImage depth_image{VK_NULL_HANDLE};
  VkDeviceMemory depth_memory{VK_NULL_HANDLE};
  VkImageView depth_view{VK_NULL_HANDLE};
  VkRenderPass render_pass{VK_NULL_HANDLE};
  VkFramebuffer framebuffer{VK_NULL_HANDLE};
  VkImageLayout color_layout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageLayout depth_layout{VK_IMAGE_LAYOUT_UNDEFINED};
  std::uint32_t width{};
  std::uint32_t height{};
  bool with_depth{};
};

struct VulkanPipelineResource {
  VkPipeline pipeline{VK_NULL_HANDLE};
  VkPipelineLayout layout{VK_NULL_HANDLE};
  VkRenderPass render_pass{VK_NULL_HANDLE};
  VulkanPipelineState state{};
};

struct VulkanBackendState {
  VkInstance instance{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device{VK_NULL_HANDLE};
  VkDevice device{VK_NULL_HANDLE};
  VkQueue queue{VK_NULL_HANDLE};
  VkCommandPool command_pool{VK_NULL_HANDLE};
  VkPhysicalDeviceMemoryProperties memory_properties{};
  std::uint32_t queue_family{};
  std::uint64_t next_handle{1U};
  RenderDeviceCaps caps;
  std::unordered_map<std::uint64_t, VulkanMeshResource> meshes;
  std::unordered_map<std::uint64_t, VulkanRenderTargetResource> targets;
  std::unordered_map<std::uint64_t, VulkanPipelineResource> pipelines;
};

[[nodiscard]] std::optional<std::uint32_t> find_vulkan_memory_type(
    const VulkanBackendState& state, std::uint32_t type_bits,
    VkMemoryPropertyFlags required) noexcept;
[[nodiscard]] bool create_vulkan_buffer(VulkanBackendState& state,
                                        VkDeviceSize size,
                                        VkBufferUsageFlags usage,
                                        VkBuffer& buffer,
                                        VkDeviceMemory& memory) noexcept;
void destroy_vulkan_buffer(VulkanBackendState& state, VkBuffer& buffer,
                           VkDeviceMemory& memory) noexcept;
[[nodiscard]] bool submit_vulkan_commands(
    VulkanBackendState& state,
    const std::function<void(VkCommandBuffer)>& record) noexcept;
void record_color_transition(VkCommandBuffer commands, VkImage image,
                             VkImageLayout old_layout,
                             VkImageLayout new_layout) noexcept;
void record_depth_transition(VkCommandBuffer commands, VkImage image,
                             VkImageLayout old_layout,
                             VkImageLayout new_layout) noexcept;

}  // namespace ac6
