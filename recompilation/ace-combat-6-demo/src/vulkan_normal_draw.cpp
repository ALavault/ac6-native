#include "ac6demo/vulkan_normal_draw.hpp"

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include "ac6demo/runtime_error.hpp"
#include "ac6demo/xenos_commands.hpp"

#include <openssl/evp.h>

#include <array>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <span>
#include <vector>

namespace ac6demo {
namespace {

constexpr std::uint32_t kWidth = 640U;
constexpr std::uint32_t kHeight = 360U;
constexpr VkDeviceSize kReadbackBytes = VkDeviceSize{kWidth} * kHeight * 4U;

struct Image final {
  VkImage image{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  VkImageView view{VK_NULL_HANDLE};
};

struct Buffer final {
  VkBuffer buffer{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
};

struct MappedMemory final {
  VkDevice device{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  void *data{nullptr};
  MappedMemory(VkDevice mapped_device, VkDeviceMemory mapped_memory,
               void *mapped_data)
      : device(mapped_device), memory(mapped_memory), data(mapped_data) {}
  ~MappedMemory() {
    if (data != nullptr) {
      vkUnmapMemory(device, memory);
    }
  }
  MappedMemory(const MappedMemory &) = delete;
  MappedMemory &operator=(const MappedMemory &) = delete;
};

std::uint32_t memory_type(VkPhysicalDevice physical, std::uint32_t allowed,
                          VkMemoryPropertyFlags required) {
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(physical, &properties);
  for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
    if ((allowed & (1U << index)) != 0U &&
        (properties.memoryTypes[index].propertyFlags & required) == required) {
      return index;
    }
  }
  throw RuntimeTrap("Vulkan normal draw lacks a required memory type");
}

Image create_image(VkPhysicalDevice physical, VkDevice device, VkFormat format,
                   VkSampleCountFlagBits samples, VkImageUsageFlags usage,
                   VkImageAspectFlags aspect) {
  Image result;
  VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = format;
  info.extent = {kWidth, kHeight, 1U};
  info.mipLevels = 1U;
  info.arrayLayers = 1U;
  info.samples = samples;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usage;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (vkCreateImage(device, &info, nullptr, &result.image) != VK_SUCCESS) {
    throw RuntimeTrap("Vulkan normal draw image creation failed");
  }
  VkMemoryRequirements requirements{};
  vkGetImageMemoryRequirements(device, result.image, &requirements);
  VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocation.allocationSize = requirements.size;
  allocation.memoryTypeIndex = memory_type(
      physical, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (vkAllocateMemory(device, &allocation, nullptr, &result.memory) !=
          VK_SUCCESS ||
      vkBindImageMemory(device, result.image, result.memory, 0U) != VK_SUCCESS) {
    if (result.memory != VK_NULL_HANDLE) vkFreeMemory(device, result.memory, nullptr);
    vkDestroyImage(device, result.image, nullptr);
    throw RuntimeTrap("Vulkan normal draw image allocation failed");
  }
  VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  view.image = result.image;
  view.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view.format = format;
  view.subresourceRange.aspectMask = aspect;
  view.subresourceRange.levelCount = 1U;
  view.subresourceRange.layerCount = 1U;
  if (vkCreateImageView(device, &view, nullptr, &result.view) != VK_SUCCESS) {
    vkFreeMemory(device, result.memory, nullptr);
    vkDestroyImage(device, result.image, nullptr);
    throw RuntimeTrap("Vulkan normal draw image-view creation failed");
  }
  return result;
}

void destroy_image(VkDevice device, Image &image) noexcept {
  if (image.view != VK_NULL_HANDLE) vkDestroyImageView(device, image.view, nullptr);
  if (image.image != VK_NULL_HANDLE) vkDestroyImage(device, image.image, nullptr);
  if (image.memory != VK_NULL_HANDLE) vkFreeMemory(device, image.memory, nullptr);
  image = {};
}

Buffer create_readback(VkPhysicalDevice physical, VkDevice device) {
  Buffer result;
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  info.size = kReadbackBytes;
  info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(device, &info, nullptr, &result.buffer) != VK_SUCCESS) {
    throw RuntimeTrap("Vulkan normal draw readback buffer creation failed");
  }
  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device, result.buffer, &requirements);
  VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocation.allocationSize = requirements.size;
  allocation.memoryTypeIndex = memory_type(
      physical, requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (vkAllocateMemory(device, &allocation, nullptr, &result.memory) !=
          VK_SUCCESS ||
      vkBindBufferMemory(device, result.buffer, result.memory, 0U) != VK_SUCCESS) {
    if (result.memory != VK_NULL_HANDLE) vkFreeMemory(device, result.memory, nullptr);
    vkDestroyBuffer(device, result.buffer, nullptr);
    throw RuntimeTrap("Vulkan normal draw readback allocation failed");
  }
  return result;
}

std::string sha256(std::span<const std::byte> bytes) {
  std::array<unsigned char, 32> digest{};
  unsigned int size = 0U;
  EVP_MD_CTX *context = EVP_MD_CTX_new();
  if (context == nullptr || EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1 ||
      EVP_DigestUpdate(context, bytes.data(), bytes.size()) != 1 ||
      EVP_DigestFinal_ex(context, digest.data(), &size) != 1 || size != digest.size()) {
    EVP_MD_CTX_free(context);
    throw RuntimeTrap("Vulkan normal draw readback hash failed");
  }
  EVP_MD_CTX_free(context);
  constexpr char hex[] = "0123456789abcdef";
  std::string output(digest.size() * 2U, '0');
  for (std::size_t index = 0; index < digest.size(); ++index) {
    output[index * 2U] = hex[digest[index] >> 4U];
    output[index * 2U + 1U] = hex[digest[index] & 15U];
  }
  return output;
}

struct ReadbackSummary final {
  std::string digest;
  std::uint32_t black{};
  std::uint32_t sentinel{};
};

[[nodiscard]] ReadbackSummary
summarize_readback(std::span<const std::byte> bytes) {
  ReadbackSummary result{sha256(bytes)};
  for (std::size_t offset = 0; offset < bytes.size(); offset += 4U) {
    const auto *pixel = bytes.data() + offset;
    result.black += pixel[0] == std::byte{} && pixel[1] == std::byte{} &&
                            pixel[2] == std::byte{} && pixel[3] == std::byte{}
                        ? 1U
                        : 0U;
    result.sentinel += pixel[0] == std::byte{0xFF} &&
                               pixel[1] == std::byte{} &&
                               pixel[2] == std::byte{0xFF} &&
                               pixel[3] == std::byte{0xFF}
                           ? 1U
                           : 0U;
  }
  return result;
}

[[nodiscard]] std::uint64_t query_passed_samples(
    VkDevice device, VkQueryPool pool, bool enabled) {
  if (!enabled) {
    return 0U;
  }
  std::uint64_t result = 0U;
  if (vkGetQueryPoolResults(device, pool, 0U, 1U, sizeof(result), &result,
                            sizeof(result), VK_QUERY_RESULT_64_BIT |
                                                VK_QUERY_RESULT_WAIT_BIT) !=
      VK_SUCCESS) {
    throw RuntimeTrap("Vulkan normal draw query readback failed");
  }
  return result;
}

VkQueryPool create_precise_occlusion_query_pool(VkPhysicalDevice physical,
                                                VkDevice device) {
  VkPhysicalDeviceFeatures supported{};
  vkGetPhysicalDeviceFeatures(physical, &supported);
  if (supported.occlusionQueryPrecise != VK_TRUE) {
    throw RuntimeTrap("Vulkan precise occlusion query feature is unavailable");
  }
  VkQueryPoolCreateInfo query_info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  query_info.queryType = VK_QUERY_TYPE_OCCLUSION;
  query_info.queryCount = 1U;
  VkQueryPool pool = VK_NULL_HANDLE;
  if (vkCreateQueryPool(device, &query_info, nullptr, &pool) != VK_SUCCESS) {
    throw RuntimeTrap("Vulkan normal draw query-pool creation failed");
  }
  return pool;
}

} // namespace

namespace {

void qualify_reached_normal_draw(const XenosDrawCommand &draw) {
  static constexpr std::string_view kVertex =
      "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b";
  static constexpr std::string_view kPixel =
      "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25";
  if (draw.primitive != XenosPrimitive::RectangleList ||
      draw.source != XenosIndexSource::AutoIndex || draw.index_count != 3U ||
      draw.index_format != XenosIndexFormat::Uint16 || !draw.predicated ||
      draw.vertex_shader_sha256 != kVertex || draw.pixel_shader_sha256 != kPixel ||
      !draw.registers) {
    throw RuntimeTrap("unqualified PAL normal draw provenance");
  }
  const auto &registers = *draw.registers;
  if (registers.value(0x2000U) != 0x0A020280U ||
      registers.value(0x2104U) != 0x0000FFFFU ||
      registers.value(0x2180U) != 0x10010001U ||
      registers.value(0x2200U) != 0x00008777U ||
      registers.value(0x2201U) != 0x00010001U ||
      registers.value(0x2208U) != 0x00000004U) {
    throw RuntimeTrap("unqualified PAL normal draw register profile");
  }
}

void destroy_normal_draw_resources(VkDevice device, VkQueue queue,
                                   bool submitted, VkFence &fence,
                                   VkQueryPool &query_pool, VkCommandPool &pool,
                                   VkFramebuffer &framebuffer, Buffer &readback,
                                   Image &resolved, Image &depth, Image &color) {
  if (submitted && vkQueueWaitIdle(queue) != VK_SUCCESS) {
    throw RuntimeTrap("Vulkan normal draw cleanup could not drain queue");
  }
  if (fence != VK_NULL_HANDLE) vkDestroyFence(device, fence, nullptr);
  if (query_pool != VK_NULL_HANDLE) vkDestroyQueryPool(device, query_pool, nullptr);
  if (pool != VK_NULL_HANDLE) vkDestroyCommandPool(device, pool, nullptr);
  if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device, framebuffer, nullptr);
  if (readback.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, readback.buffer, nullptr);
  if (readback.memory != VK_NULL_HANDLE) vkFreeMemory(device, readback.memory, nullptr);
  destroy_image(device, resolved);
  destroy_image(device, depth);
  destroy_image(device, color);
}

} // namespace

VulkanNormalDrawResult execute_vulkan_normal_draw(
    VkPhysicalDevice physical, VkDevice device, VkQueue queue,
    std::uint32_t queue_family, const XenosDrawCommand &draw,
    VkRenderPass render_pass, VkPipeline pipeline,
    VkPipelineLayout pipeline_layout, VkDescriptorSet shared,
    VkDescriptorSet constants, bool *cleanup_safe) {
  if (cleanup_safe != nullptr) *cleanup_safe = true;
  qualify_reached_normal_draw(draw);
  if (physical == VK_NULL_HANDLE || device == VK_NULL_HANDLE ||
      queue == VK_NULL_HANDLE || render_pass == VK_NULL_HANDLE ||
      pipeline == VK_NULL_HANDLE || pipeline_layout == VK_NULL_HANDLE ||
      shared == VK_NULL_HANDLE || constants == VK_NULL_HANDLE) {
    throw RuntimeTrap("Vulkan normal draw prerequisites are incomplete");
  }
  Image color, depth, resolved;
  Buffer readback;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  VkQueryPool query_pool = VK_NULL_HANDLE;
  bool submitted = false;
  try {
    color = create_image(physical, device, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_SAMPLE_COUNT_4_BIT,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                         VK_IMAGE_ASPECT_COLOR_BIT);
    depth = create_image(physical, device, VK_FORMAT_D24_UNORM_S8_UINT,
                         VK_SAMPLE_COUNT_4_BIT,
                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                         VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    resolved = create_image(physical, device, VK_FORMAT_R8G8B8A8_UNORM,
                            VK_SAMPLE_COUNT_1_BIT,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                            VK_IMAGE_ASPECT_COLOR_BIT);
    readback = create_readback(physical, device);
    const std::array views{color.view, depth.view, resolved.view};
    VkFramebufferCreateInfo framebuffer_info{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer_info.renderPass = render_pass;
    framebuffer_info.attachmentCount = static_cast<std::uint32_t>(views.size());
    framebuffer_info.pAttachments = views.data();
    framebuffer_info.width = kWidth;
    framebuffer_info.height = kHeight;
    framebuffer_info.layers = 1U;
    if (vkCreateFramebuffer(device, &framebuffer_info, nullptr, &framebuffer) !=
        VK_SUCCESS) {
      throw RuntimeTrap("Vulkan normal draw framebuffer creation failed");
    }
    VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.queueFamilyIndex = queue_family;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    if (vkCreateCommandPool(device, &pool_info, nullptr, &pool) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan normal draw command-pool creation failed");
    }
    VkCommandBufferAllocateInfo allocate{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1U;
    VkCommandBuffer command = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocate, &command) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan normal draw command allocation failed");
    }
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(command, &begin) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan normal draw command begin failed");
    }
    const bool watch_draw_result =
        std::getenv("AC6_DEMO_WATCH_NORMAL_DRAW_RESULT") != nullptr;
    if (watch_draw_result) {
      query_pool = create_precise_occlusion_query_pool(physical, device);
      vkCmdResetQueryPool(command, query_pool, 0U, 1U);
    }
    std::array<VkClearValue, 3> clears{};
    clears[0].color.float32[0] = 1.0F;
    clears[0].color.float32[1] = 0.0F;
    clears[0].color.float32[2] = 1.0F;
    clears[0].color.float32[3] = 1.0F;
    clears[1].depthStencil = {1.0F, 0U};
    VkRenderPassBeginInfo render{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render.renderPass = render_pass;
    render.framebuffer = framebuffer;
    render.renderArea.extent = {kWidth, kHeight};
    render.clearValueCount = static_cast<std::uint32_t>(clears.size());
    render.pClearValues = clears.data();
    vkCmdBeginRenderPass(command, &render, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    const std::array sets{shared, constants};
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout, 0U,
                            static_cast<std::uint32_t>(sets.size()), sets.data(),
                            0U, nullptr);
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical, &properties);
    constexpr std::uint32_t kXenosMaximum2D = 8192U;
    const std::uint32_t viewport_width = std::min(
        kXenosMaximum2D, properties.limits.maxViewportDimensions[0]);
    const std::uint32_t viewport_height = std::min(
        kXenosMaximum2D, properties.limits.maxViewportDimensions[1]);
    // The production frontier keeps the historical bounded host viewport.
    // This opt-in probe applies only the PAL-captured scale/offset hypothesis
    // and remains diagnostic until a non-black result is independently joined.
    const bool pal_viewport_probe =
        std::getenv("AC6_DEMO_EXPERIMENTAL_PAL_VIEWPORT") != nullptr;
    VkViewport viewport{
        pal_viewport_probe ? 0.0F : 0.0F,
        pal_viewport_probe ? 360.0F : 0.0F,
        pal_viewport_probe ? 640.0F : static_cast<float>(viewport_width),
        pal_viewport_probe ? -360.0F : static_cast<float>(viewport_height),
        0.0F, 1.0F};
    if (pal_viewport_probe) {
      std::fputs("AC6_PAL_VIEWPORT_PROBE x=0 y=360 width=640 height=-360\n",
                 stderr);
    }
    VkRect2D scissor{{0, 0}, {kWidth, kHeight}};
    vkCmdSetViewport(command, 0U, 1U, &viewport);
    vkCmdSetScissor(command, 0U, 1U, &scissor);
    vkCmdSetStencilCompareMask(command, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFU);
    vkCmdSetStencilWriteMask(command, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFU);
    vkCmdSetStencilReference(command, VK_STENCIL_FACE_FRONT_AND_BACK, 0U);
    if (watch_draw_result) {
      vkCmdBeginQuery(command, query_pool, 0U, VK_QUERY_CONTROL_PRECISE_BIT);
    }
    vkCmdDraw(command, 3U, 1U, 0U, 0U);
    if (watch_draw_result) {
      vkCmdEndQuery(command, query_pool, 0U);
    }
    vkCmdEndRenderPass(command);
    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1U;
    copy.imageExtent = {kWidth, kHeight, 1U};
    vkCmdCopyImageToBuffer(command, resolved.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readback.buffer, 1U, &copy);
    if (vkEndCommandBuffer(command) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan normal draw command end failed");
    }
    VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateFence(device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan normal draw fence creation failed");
    }
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1U;
    submit.pCommandBuffers = &command;
    if (vkQueueSubmit(queue, 1U, &submit, fence) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan normal draw submission failed");
    }
    submitted = true;
    const VkResult wait_result =
        vkWaitForFences(device, 1U, &fence, VK_TRUE, 10'000'000'000ULL);
    if (wait_result != VK_SUCCESS) {
      // A timeout leaves submitted resources in flight. Drain the queue before
      // cleanup; if draining fails, deliberately leak rather than destroy
      // resources still referenced by the GPU.
      if (vkQueueWaitIdle(queue) != VK_SUCCESS) {
        throw RuntimeTrap("Vulkan normal draw queue did not become idle");
      }
      throw RuntimeTrap("Vulkan normal draw submission timed out");
    }
    const auto passed_samples =
        query_passed_samples(device, query_pool, watch_draw_result);
    void *mapping = nullptr;
    if (vkMapMemory(device, readback.memory, 0U, kReadbackBytes, 0U, &mapping) !=
        VK_SUCCESS) {
      throw RuntimeTrap("Vulkan normal draw readback mapping failed");
    }
    ReadbackSummary summary;
    std::vector<std::byte> observed;
    {
      const MappedMemory mapped{device, readback.memory, mapping};
      const auto readback_bytes = std::span{
          static_cast<const std::byte *>(mapping),
          static_cast<std::size_t>(kReadbackBytes)};
      summary = summarize_readback(readback_bytes);
      observed.assign(readback_bytes.begin(), readback_bytes.end());
      if (watch_draw_result) {
        const auto pixels = kWidth * kHeight;
        std::fprintf(stderr,
                     "AC6_NORMAL_DRAW_RESULT width=%u height=%u samples=4 "
                     "passed_samples=%llu black_pixels=%u sentinel_pixels=%u "
                     "other_pixels=%u sha256=%s\n",
                     kWidth, kHeight,
                     static_cast<unsigned long long>(passed_samples),
                     summary.black, summary.sentinel,
                     pixels - summary.black - summary.sentinel,
                     summary.digest.c_str());
      }
    }
    if (summary.digest !=
        "0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366") {
      throw RuntimeTrap("Vulkan normal draw differs from the CPU oracle: " +
                        summary.digest +
                        " black=" + std::to_string(summary.black) +
                        " sentinel=" + std::to_string(summary.sentinel));
    }
    destroy_normal_draw_resources(device, queue, false, fence, query_pool, pool,
                                  framebuffer, readback, resolved, depth, color);
    return {summary.digest, kWidth, kHeight, std::move(observed)};
  } catch (...) {
    try {
      destroy_normal_draw_resources(device, queue, submitted, fence, query_pool,
                                    pool, framebuffer, readback, resolved,
                                    depth, color);
    } catch (...) {
      if (cleanup_safe != nullptr) *cleanup_safe = false;
      throw;
    }
    throw;
  }
}

} // namespace ac6demo

#endif
