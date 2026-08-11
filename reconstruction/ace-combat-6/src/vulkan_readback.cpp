#include "vulkan_backend_internal.h"

#include <cstring>
#include <limits>

namespace ac6 {

std::vector<std::uint8_t> VulkanBackend::readback_rgba8(
    const VulkanRenderTargetHandle target_handle) noexcept {
  const auto found = state_->targets.find(target_handle.value);
  if (found == state_->targets.end() ||
      found->second.color_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
    return {};
  }
  VulkanRenderTargetResource& target = found->second;
  const std::uint64_t byte_count_64 =
      static_cast<std::uint64_t>(target.width) * target.height * 4U;
  if (byte_count_64 == 0U ||
      byte_count_64 > std::numeric_limits<std::size_t>::max()) {
    return {};
  }
  const std::size_t byte_count = static_cast<std::size_t>(byte_count_64);
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  if (!create_vulkan_buffer(*state_, byte_count, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            buffer, memory)) {
    destroy_vulkan_buffer(*state_, buffer, memory);
    return {};
  }
  const VkImageLayout old_layout = target.color_layout;
  const bool submitted = submit_vulkan_commands(
      *state_, [&](const VkCommandBuffer commands) {
        record_color_transition(commands, target.color_image, old_layout,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        const VkBufferImageCopy copy{
            .bufferOffset = 0U,
            .bufferRowLength = 0U,
            .bufferImageHeight = 0U,
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 0U, 1U},
            .imageOffset = {0, 0, 0},
            .imageExtent = {target.width, target.height, 1U},
        };
        vkCmdCopyImageToBuffer(commands, target.color_image,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1U,
                               &copy);
        record_color_transition(commands, target.color_image,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, old_layout);
      });
  if (!submitted) {
    destroy_vulkan_buffer(*state_, buffer, memory);
    return {};
  }
  std::vector<std::uint8_t> bytes(byte_count);
  void* mapped = nullptr;
  if (vkMapMemory(state_->device, memory, 0U, byte_count, 0U, &mapped) !=
      VK_SUCCESS) {
    destroy_vulkan_buffer(*state_, buffer, memory);
    return {};
  }
  std::memcpy(bytes.data(), mapped, byte_count);
  vkUnmapMemory(state_->device, memory);
  destroy_vulkan_buffer(*state_, buffer, memory);
  return bytes;
}

}  // namespace ac6
