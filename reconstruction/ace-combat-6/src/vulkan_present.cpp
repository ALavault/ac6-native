#include "vulkan_backend_internal.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace ac6 {
namespace {

bool choose_present_format(VkPhysicalDevice device, VkSurfaceKHR surface,
                           VkSurfaceFormatKHR& chosen) noexcept {
  std::uint32_t count = 0;
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr) !=
          VK_SUCCESS ||
      count == 0U) {
    return false;
  }
  std::vector<VkSurfaceFormatKHR> formats(count);
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count,
                                           formats.data()) != VK_SUCCESS) {
    return false;
  }
  const auto preferred = std::find_if(
      formats.begin(), formats.end(), [](const VkSurfaceFormatKHR& format) {
        return format.format == VK_FORMAT_R8G8B8A8_UNORM &&
               format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      });
  if (preferred != formats.end()) {
    chosen = *preferred;
    return true;
  }
  const auto bgra = std::find_if(
      formats.begin(), formats.end(), [](const VkSurfaceFormatKHR& format) {
        return format.format == VK_FORMAT_B8G8R8A8_UNORM &&
               format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      });
  chosen = bgra == formats.end() ? formats.front() : *bgra;
  return chosen.format != VK_FORMAT_UNDEFINED;
}

}  // namespace

bool create_present_swapchain(VulkanBackendState& state,
                              const std::uint32_t width,
                              const std::uint32_t height) noexcept {
  if (state.present_surface == VK_NULL_HANDLE || width == 0U || height == 0U) {
    return false;
  }
  VkSurfaceCapabilitiesKHR capabilities{};
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.physical_device,
                                                 state.present_surface,
                                                 &capabilities) != VK_SUCCESS) {
    return false;
  }
  VkSurfaceFormatKHR format{};
  if (!choose_present_format(state.physical_device, state.present_surface,
                             format)) {
    return false;
  }
  std::uint32_t present_count = 0;
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(state.physical_device,
                                                 state.present_surface,
                                                 &present_count, nullptr) != VK_SUCCESS ||
      present_count == 0U) {
    return false;
  }
  std::vector<VkPresentModeKHR> present_modes(present_count);
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(
          state.physical_device, state.present_surface, &present_count,
          present_modes.data()) != VK_SUCCESS ||
      std::find(present_modes.begin(), present_modes.end(),
                VK_PRESENT_MODE_FIFO_KHR) == present_modes.end()) {
    return false;
  }
  VkExtent2D extent = capabilities.currentExtent;
  if (extent.width == std::numeric_limits<std::uint32_t>::max()) {
    extent.width = std::clamp(width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
  }
  if (extent.width == 0U || extent.height == 0U ||
      (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0U) {
    return false;
  }
  std::uint32_t image_count = capabilities.minImageCount + 1U;
  if (capabilities.maxImageCount != 0U) {
    image_count = std::min(image_count, capabilities.maxImageCount);
  }
  const VkSwapchainCreateInfoKHR create_info{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .surface = state.present_surface,
      .minImageCount = image_count,
      .imageFormat = format.format,
      .imageColorSpace = format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1U,
      .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0U,
      .pQueueFamilyIndices = nullptr,
      .preTransform = capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };
  if (vkCreateSwapchainKHR(state.device, &create_info, nullptr,
                           &state.present_swapchain) != VK_SUCCESS) {
    return false;
  }
  std::uint32_t actual_count = 0;
  if (vkGetSwapchainImagesKHR(state.device, state.present_swapchain,
                              &actual_count, nullptr) != VK_SUCCESS ||
      actual_count == 0U) {
    destroy_present_swapchain(state);
    return false;
  }
  state.present_images.resize(actual_count);
  if (vkGetSwapchainImagesKHR(state.device, state.present_swapchain,
                              &actual_count, state.present_images.data()) !=
      VK_SUCCESS) {
    destroy_present_swapchain(state);
    return false;
  }
  state.present_views.reserve(state.present_images.size());
  for (const VkImage image : state.present_images) {
    const VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format.format,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, 0U, 1U},
    };
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(state.device, &view_info, nullptr, &view) !=
        VK_SUCCESS) {
      destroy_present_swapchain(state);
      return false;
    }
    state.present_views.push_back(view);
  }
  const VkFenceCreateInfo fence_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  if (vkCreateFence(state.device, &fence_info, nullptr, &state.present_fence) !=
      VK_SUCCESS) {
    destroy_present_swapchain(state);
    return false;
  }
  state.present_format = format.format;
  state.present_extent = extent;
  state.present_initialized.assign(state.present_images.size(), false);
  return true;
}

void destroy_present_swapchain(VulkanBackendState& state) noexcept {
  if (state.device == VK_NULL_HANDLE) return;
  if (state.present_fence != VK_NULL_HANDLE) {
    vkDestroyFence(state.device, state.present_fence, nullptr);
    state.present_fence = VK_NULL_HANDLE;
  }
  for (const VkImageView view : state.present_views) {
    vkDestroyImageView(state.device, view, nullptr);
  }
  state.present_views.clear();
  state.present_images.clear();
  state.present_initialized.clear();
  if (state.present_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(state.device, state.present_swapchain, nullptr);
    state.present_swapchain = VK_NULL_HANDLE;
  }
  state.present_format = VK_FORMAT_UNDEFINED;
  state.present_extent = {};
}

bool VulkanBackend::present_target(
    const VulkanRenderTargetHandle target_handle) noexcept {
  const auto target_it = state_->targets.find(target_handle.value);
  if (target_it == state_->targets.end() ||
      state_->present_swapchain == VK_NULL_HANDLE ||
      target_it->second.color_layout == VK_IMAGE_LAYOUT_UNDEFINED ||
      target_it->second.width != state_->present_extent.width ||
      target_it->second.height != state_->present_extent.height) {
    return false;
  }
  if (vkQueueWaitIdle(state_->queue) != VK_SUCCESS ||
      vkResetFences(state_->device, 1U, &state_->present_fence) != VK_SUCCESS) {
    return false;
  }
  std::uint32_t image_index = 0;
  const VkResult acquired = vkAcquireNextImageKHR(
      state_->device, state_->present_swapchain, UINT64_MAX, VK_NULL_HANDLE,
      state_->present_fence, &image_index);
  if ((acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) ||
      image_index >= state_->present_images.size() ||
      vkWaitForFences(state_->device, 1U, &state_->present_fence, VK_TRUE,
                      UINT64_MAX) != VK_SUCCESS) {
    return false;
  }
  VulkanRenderTargetResource& target = target_it->second;
  const VkImageLayout old_target_layout = target.color_layout;
  const VkImageLayout old_present_layout =
      state_->present_initialized[image_index]
          ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
          : VK_IMAGE_LAYOUT_UNDEFINED;
  const bool submitted = submit_vulkan_commands(
      *state_, [&](const VkCommandBuffer commands) {
        record_color_transition(commands, target.color_image, old_target_layout,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        const VkImageMemoryBarrier to_destination{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0U,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = old_present_layout,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = state_->present_images[image_index],
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, 0U, 1U},
        };
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0U, 0U, nullptr,
                             0U, nullptr, 1U, &to_destination);
        const VkImageBlit blit{
            .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 0U, 1U},
            .srcOffsets = {{0, 0, 0},
                           {static_cast<std::int32_t>(target.width),
                            static_cast<std::int32_t>(target.height), 1}},
            .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 0U, 1U},
            .dstOffsets = {{0, 0, 0},
                           {static_cast<std::int32_t>(state_->present_extent.width),
                            static_cast<std::int32_t>(state_->present_extent.height),
                            1}},
        };
        vkCmdBlitImage(commands, target.color_image,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       state_->present_images[image_index],
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &blit,
                       VK_FILTER_NEAREST);
        const VkImageMemoryBarrier to_present{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = 0U,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = state_->present_images[image_index],
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, 0U, 1U},
        };
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0U, 0U,
                             nullptr, 0U, nullptr, 1U, &to_present);
        record_color_transition(commands, target.color_image,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                old_target_layout);
      });
  if (!submitted) return false;
  state_->present_initialized[image_index] = true;
  const VkPresentInfoKHR present_info{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount = 0U,
      .pWaitSemaphores = nullptr,
      .swapchainCount = 1U,
      .pSwapchains = &state_->present_swapchain,
      .pImageIndices = &image_index,
      .pResults = nullptr,
  };
  const VkResult presented = vkQueuePresentKHR(state_->queue, &present_info);
  return presented == VK_SUCCESS || presented == VK_SUBOPTIMAL_KHR;
}

}  // namespace ac6
