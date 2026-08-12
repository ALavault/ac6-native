#include "ac6/sdl_input.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace ac6 {

VulkanSwapchain::~VulkanSwapchain() { destroy(); }

bool VulkanSwapchain::create(const VulkanDevice& device, VkSurfaceKHR surface,
                             std::uint32_t width, std::uint32_t height) noexcept {
  if (!device.valid() || surface == VK_NULL_HANDLE || width == 0 || height == 0 || valid()) return false;
  VkSurfaceCapabilitiesKHR capabilities{};
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical_device(), surface, &capabilities) != VK_SUCCESS) return false;
  std::uint32_t format_count = 0;
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device(), surface, &format_count, nullptr) != VK_SUCCESS || format_count == 0) return false;
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device(), surface, &format_count, formats.data()) != VK_SUCCESS) return false;
  const auto format_it = std::find_if(formats.begin(), formats.end(), [](const VkSurfaceFormatKHR& value) {
    return value.format == VK_FORMAT_B8G8R8A8_UNORM && value.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  });
  const VkSurfaceFormatKHR chosen_format = format_it == formats.end() ? formats.front() : *format_it;
  if (chosen_format.format == VK_FORMAT_UNDEFINED) return false;
  std::uint32_t present_count = 0;
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(device.physical_device(), surface, &present_count, nullptr) != VK_SUCCESS || present_count == 0) return false;
  std::vector<VkPresentModeKHR> present_modes(present_count);
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(device.physical_device(), surface, &present_count, present_modes.data()) != VK_SUCCESS ||
      std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_FIFO_KHR) == present_modes.end()) return false;
  extent_ = capabilities.currentExtent;
  if (extent_.width == std::numeric_limits<std::uint32_t>::max()) {
    extent_.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent_.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
  }
  if (extent_.width == 0 || extent_.height == 0) return false;
  constexpr VkImageUsageFlags kRequiredUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if ((capabilities.supportedUsageFlags & kRequiredUsage) != kRequiredUsage) return false;
  std::uint32_t image_count = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount != 0) image_count = std::min(image_count, capabilities.maxImageCount);
  const VkSwapchainCreateInfoKHR create_info{
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, nullptr, 0, surface, image_count,
      chosen_format.format, chosen_format.colorSpace, extent_, 1, kRequiredUsage,
      VK_SHARING_MODE_EXCLUSIVE, 0, nullptr, capabilities.currentTransform,
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_PRESENT_MODE_FIFO_KHR, VK_TRUE, VK_NULL_HANDLE};
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  if (vkCreateSwapchainKHR(device.device(), &create_info, nullptr, &swapchain) != VK_SUCCESS) return false;
  device_ = device.device();
  swapchain_ = swapchain;
  format_ = chosen_format.format;
  std::uint32_t image_actual_count = 0;
  if (vkGetSwapchainImagesKHR(device_, swapchain_, &image_actual_count, nullptr) != VK_SUCCESS || image_actual_count == 0) {
    destroy();
    return false;
  }
  images_.resize(image_actual_count);
  if (vkGetSwapchainImagesKHR(device_, swapchain_, &image_actual_count, images_.data()) != VK_SUCCESS) {
    destroy();
    return false;
  }
  views_.reserve(images_.size());
  for (const VkImage image : images_) {
    const VkImageViewCreateInfo view_info{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, image, VK_IMAGE_VIEW_TYPE_2D,
        format_, {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                  VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(device_, &view_info, nullptr, &view) != VK_SUCCESS) {
      destroy();
      return false;
    }
    views_.push_back(view);
  }
  return true;
}

void VulkanSwapchain::destroy() noexcept {
  if (device_ == VK_NULL_HANDLE) return;
  for (const VkImageView view : views_) vkDestroyImageView(device_, view, nullptr);
  views_.clear();
  images_.clear();
  if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  swapchain_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
  format_ = VK_FORMAT_UNDEFINED;
  extent_ = {};
}

VulkanFramePresenter::~VulkanFramePresenter() { destroy(); }

bool VulkanFramePresenter::create(const VulkanDevice& device,
                                  const VulkanSwapchain& swapchain) noexcept {
  if (!device.valid() || !swapchain.valid() || valid()) return false;
  device_ = device.device();
  physical_device_ = device.physical_device();
  queue_ = device.graphics_queue();
  swapchain_ = swapchain.handle();
  extent_ = swapchain.extent();
  images_ = swapchain.images();
  initialized_images_.assign(images_.size(), false);
  const VkCommandPoolCreateInfo pool_info{
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, device.queue_family()};
  if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS) {
    destroy();
    return false;
  }
  const VkCommandBufferAllocateInfo allocate_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, command_pool_,
      VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
  if (vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer_) != VK_SUCCESS) {
    destroy();
    return false;
  }
  const VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
  if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_) != VK_SUCCESS ||
      vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_) != VK_SUCCESS) {
    destroy();
    return false;
  }
  const VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
                                     VK_FENCE_CREATE_SIGNALED_BIT};
  if (vkCreateFence(device_, &fence_info, nullptr, &fence_) != VK_SUCCESS) {
    destroy();
    return false;
  }
  staging_size_ = static_cast<VkDeviceSize>(extent_.width) * extent_.height * 4u;
  if (staging_size_ == 0) {
    destroy();
    return false;
  }
  const VkBufferCreateInfo buffer_info{
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, staging_size_,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0,
      nullptr};
  if (vkCreateBuffer(device_, &buffer_info, nullptr, &staging_buffer_) != VK_SUCCESS) {
    destroy();
    return false;
  }
  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_, staging_buffer_, &requirements);
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
  std::uint32_t memory_type = memory_properties.memoryTypeCount;
  for (std::uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
    const VkMemoryPropertyFlags flags = memory_properties.memoryTypes[index].propertyFlags;
    if ((requirements.memoryTypeBits & (1u << index)) != 0 &&
        (flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
      memory_type = index;
      break;
    }
  }
  if (memory_type == memory_properties.memoryTypeCount) {
    destroy();
    return false;
  }
  const VkMemoryAllocateInfo staging_allocate_info{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, requirements.size,
      memory_type};
  if (vkAllocateMemory(device_, &staging_allocate_info, nullptr, &staging_memory_) != VK_SUCCESS ||
      vkBindBufferMemory(device_, staging_buffer_, staging_memory_, 0) != VK_SUCCESS ||
      vkMapMemory(device_, staging_memory_, 0, staging_size_, 0,
                  &staging_mapped_) != VK_SUCCESS) {
    destroy();
    return false;
  }
  source_pixels_.reserve(static_cast<std::size_t>(staging_size_));
  frame_pixels_.resize(static_cast<std::size_t>(staging_size_));
  return true;
}

bool VulkanFramePresenter::present_frame(const NativeRenderTarget& target) noexcept {
  if (!valid() || !persistent_upload_ready() ||
      !target.copy_rgba8(source_pixels_) || source_pixels_.empty() ||
      frame_pixels_.size() != static_cast<std::size_t>(staging_size_)) {
    return false;
  }
  for (std::uint32_t y = 0; y < extent_.height; ++y) {
    const std::uint32_t source_y = std::min(target.height() - 1u,
        static_cast<std::uint32_t>((static_cast<std::uint64_t>(y) * target.height()) / extent_.height));
    for (std::uint32_t x = 0; x < extent_.width; ++x) {
      const std::uint32_t source_x = std::min(target.width() - 1u,
          static_cast<std::uint32_t>((static_cast<std::uint64_t>(x) * target.width()) / extent_.width));
      const std::size_t source_offset = (static_cast<std::size_t>(source_y) * target.width() + source_x) * 4u;
      const std::size_t destination_offset = (static_cast<std::size_t>(y) * extent_.width + x) * 4u;
      std::memcpy(frame_pixels_.data() + destination_offset,
                  source_pixels_.data() + source_offset, 4u);
    }
  }
  if (vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX) != VK_SUCCESS ||
      vkResetFences(device_, 1, &fence_) != VK_SUCCESS) return false;
  // The upload buffer is shared by consecutive submissions.  Wait for the
  // previous fence before overwriting it; the old per-frame buffer masked
  // this ordering requirement by construction.
  std::memcpy(staging_mapped_, frame_pixels_.data(), frame_pixels_.size());
  std::uint32_t image_index = 0;
  const VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                                  image_available_, VK_NULL_HANDLE, &image_index);
  if ((acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) || image_index >= images_.size() ||
      vkResetCommandBuffer(command_buffer_, 0) != VK_SUCCESS) return false;
  const VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
  if (vkBeginCommandBuffer(command_buffer_, &begin_info) != VK_SUCCESS) return false;
  const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  if (image_index >= initialized_images_.size()) return false;
  const VkImageMemoryBarrier to_transfer{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
      initialized_images_[image_index] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
      images_[image_index], range};
  vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_transfer);
  const VkBufferImageCopy copy_region{0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                      {0, 0, 0}, {extent_.width, extent_.height, 1}};
  vkCmdCopyBufferToImage(command_buffer_, staging_buffer_, images_[image_index],
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
  const VkImageMemoryBarrier to_present{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, 0,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, images_[image_index], range};
  vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_present);
  if (vkEndCommandBuffer(command_buffer_) != VK_SUCCESS) return false;
  const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  const VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, &image_available_,
                                 &wait_stage, 1, &command_buffer_, 1, &render_finished_};
  if (vkQueueSubmit(queue_, 1, &submit_info, fence_) != VK_SUCCESS) return false;
  initialized_images_[image_index] = true;
  const VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1,
                                      &render_finished_, 1, &swapchain_, &image_index, nullptr};
  const VkResult presented = vkQueuePresentKHR(queue_, &present_info);
  const bool waited = vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
  return waited && (presented == VK_SUCCESS || presented == VK_SUBOPTIMAL_KHR);
}

bool VulkanFramePresenter::present_clear(float red, float green, float blue,
                                         float alpha) noexcept {
  if (!valid() || images_.empty() || !std::isfinite(red) || !std::isfinite(green) ||
      !std::isfinite(blue) || !std::isfinite(alpha)) return false;
  if (vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX) != VK_SUCCESS ||
      vkResetFences(device_, 1, &fence_) != VK_SUCCESS) return false;
  std::uint32_t image_index = 0;
  const VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                                  image_available_, VK_NULL_HANDLE, &image_index);
  if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) return false;
  if (image_index >= images_.size() || vkResetCommandBuffer(command_buffer_, 0) != VK_SUCCESS) return false;
  const VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
  if (vkBeginCommandBuffer(command_buffer_, &begin_info) != VK_SUCCESS) return false;
  if (image_index >= initialized_images_.size()) return false;
  const VkImageMemoryBarrier to_transfer{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
      initialized_images_[image_index] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
      images_[image_index], {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_transfer);
  const VkClearColorValue color{{red, green, blue, alpha}};
  const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdClearColorImage(command_buffer_, images_[image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       &color, 1, &range);
  const VkImageMemoryBarrier to_present{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, 0,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, images_[image_index], range};
  vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_present);
  if (vkEndCommandBuffer(command_buffer_) != VK_SUCCESS) return false;
  const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  const VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, &image_available_,
                                 &wait_stage, 1, &command_buffer_, 1, &render_finished_};
  if (vkQueueSubmit(queue_, 1, &submit_info, fence_) != VK_SUCCESS) return false;
  initialized_images_[image_index] = true;
  const VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1,
                                      &render_finished_, 1, &swapchain_, &image_index, nullptr};
  const VkResult presented = vkQueuePresentKHR(queue_, &present_info);
  return presented == VK_SUCCESS || presented == VK_SUBOPTIMAL_KHR;
}

void VulkanFramePresenter::destroy() noexcept {
  if (device_ == VK_NULL_HANDLE) return;
  vkDeviceWaitIdle(device_);
  if (staging_mapped_ != nullptr && staging_memory_ != VK_NULL_HANDLE) {
    vkUnmapMemory(device_, staging_memory_);
  }
  if (staging_memory_ != VK_NULL_HANDLE) vkFreeMemory(device_, staging_memory_, nullptr);
  if (staging_buffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, staging_buffer_, nullptr);
  staging_mapped_ = nullptr;
  staging_memory_ = VK_NULL_HANDLE;
  staging_buffer_ = VK_NULL_HANDLE;
  staging_size_ = 0;
  source_pixels_.clear();
  frame_pixels_.clear();
  if (fence_ != VK_NULL_HANDLE) vkDestroyFence(device_, fence_, nullptr);
  if (render_finished_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, render_finished_, nullptr);
  if (image_available_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, image_available_, nullptr);
  if (command_pool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, command_pool_, nullptr);
  fence_ = VK_NULL_HANDLE;
  render_finished_ = VK_NULL_HANDLE;
  image_available_ = VK_NULL_HANDLE;
  command_buffer_ = VK_NULL_HANDLE;
  command_pool_ = VK_NULL_HANDLE;
  swapchain_ = VK_NULL_HANDLE;
  images_.clear();
  initialized_images_.clear();
  queue_ = VK_NULL_HANDLE;
  physical_device_ = VK_NULL_HANDLE;
  extent_ = {};
  device_ = VK_NULL_HANDLE;
}

}  // namespace ac6
