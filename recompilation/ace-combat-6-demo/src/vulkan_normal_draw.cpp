#include "ac6demo/vulkan_normal_draw.hpp"

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include "ac6demo/runtime_error.hpp"
#include "ac6demo/xenos_commands.hpp"
#include "ac6demo/xenos_tiling.hpp"

#include <openssl/evp.h>

#include <array>
#include <algorithm>
#include <bit>
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
constexpr std::uint32_t kTitleWidth = 1280U;
constexpr std::uint32_t kTitleHeight = 720U;
constexpr VkDeviceSize kTitleReadbackBytes =
    VkDeviceSize{kTitleWidth} * kTitleHeight * 4U;

using Image = VulkanNormalDrawImage;

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
                   VkImageAspectFlags aspect, std::uint32_t width = kWidth,
                   std::uint32_t height = kHeight,
                   VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D) {
  Image result;
  VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = format;
  info.extent = {width, height, 1U};
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
  view.viewType = view_type;
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

Buffer create_host_buffer(VkPhysicalDevice physical, VkDevice device,
                          VkDeviceSize size, VkBufferUsageFlags usage) {
  Buffer result;
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  info.size = size;
  info.usage = usage;
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

Buffer create_readback(VkPhysicalDevice physical, VkDevice device) {
  return create_host_buffer(physical, device, kReadbackBytes,
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT);
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

VkQueryPool create_title_pipeline_statistics_query_pool(
    VkPhysicalDevice physical, VkDevice device) {
  VkPhysicalDeviceFeatures supported{};
  vkGetPhysicalDeviceFeatures(physical, &supported);
  if (supported.pipelineStatisticsQuery != VK_TRUE) {
    throw RuntimeTrap(
        "Vulkan title pipeline-statistics query feature is unavailable");
  }
  VkQueryPoolCreateInfo query_info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  query_info.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
  query_info.queryCount = 1U;
  query_info.pipelineStatistics =
      VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
      VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
      VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
      VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
      VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
      VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;
  VkQueryPool pool = VK_NULL_HANDLE;
  if (vkCreateQueryPool(device, &query_info, nullptr, &pool) != VK_SUCCESS) {
    throw RuntimeTrap("Vulkan title pipeline-statistics query-pool creation failed");
  }
  return pool;
}

struct TitlePipelineStatistics final {
  std::uint64_t input_vertices{};
  std::uint64_t input_primitives{};
  std::uint64_t vertex_invocations{};
  std::uint64_t geometry_invocations{};
  std::uint64_t geometry_primitives{};
  std::uint64_t clipping_invocations{};
  std::uint64_t clipping_primitives{};
  std::uint64_t fragment_invocations{};
};

[[nodiscard]] TitlePipelineStatistics read_title_pipeline_statistics(
    VkDevice device, VkQueryPool pool) {
  std::array<std::uint64_t, 8> values{};
  if (vkGetQueryPoolResults(
          device, pool, 0U, 1U, sizeof(values), values.data(),
          sizeof(values.front()), VK_QUERY_RESULT_64_BIT |
                                      VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS) {
    throw RuntimeTrap("Vulkan title pipeline-statistics query readback failed");
  }
  return {values[0], values[1], values[2], values[3],
          values[4], values[5], values[6], values[7]};
}

constexpr std::uint32_t title_tiled_combine(std::uint32_t offset,
                                             std::uint32_t bank,
                                             std::uint32_t pipe,
                                             std::uint32_t y_lsb) {
  return (y_lsb << 4U) | (pipe << 6U) | (bank << 11U) | (offset & 0xFU) |
         (((offset >> 4U) & 1U) << 5U) |
         (((offset >> 5U) & 7U) << 8U) | ((offset >> 8U) << 12U);
}

constexpr std::uint32_t title_tiled_2d(std::uint32_t x, std::uint32_t y,
                                       std::uint32_t pitch_blocks) {
  constexpr std::uint32_t bytes_per_block_log2 = 4U;
  const std::uint32_t outer =
      (((y >> 5U) * (pitch_blocks >> 5U)) + (x >> 5U)) << 6U;
  const std::uint32_t inner = (((y >> 1U) & 7U) << 3U) | (x & 7U);
  const std::uint32_t offset =
      (outer | inner) << bytes_per_block_log2;
  const std::uint32_t bank = (y >> 4U) & 1U;
  const std::uint32_t pipe =
      ((x >> 3U) & 3U) ^ (((y >> 3U) & 1U) << 1U);
  return title_tiled_combine(offset, bank, pipe, y & 1U);
}

constexpr std::uint32_t title_tiled_2d(std::uint32_t x, std::uint32_t y) {
  return title_tiled_2d(x, y, 32U);
}

static_assert(title_tiled_2d(0U, 0U) == 0x0000U);
static_assert(title_tiled_2d(8U, 8U) == 0x20C0U);
static_assert(title_tiled_2d(15U, 15U) == 0x37F0U);

[[nodiscard]] std::vector<std::byte>
untile_title_bc3(std::span<const std::byte> tiled, std::uint32_t width,
                 std::uint32_t height) {
  constexpr std::uint32_t kBlockBytes = 16U;
  if (width == 0U || height == 0U || (width % 4U) != 0U ||
      (height % 4U) != 0U) {
    throw RuntimeTrap("Vulkan title texture tiled extent changed");
  }
  const auto block_width = width / 4U;
  const auto block_height = height / 4U;
  const auto pitch_blocks = std::max(32U, (block_width + 31U) & ~31U);
  const auto padded_height = std::max(32U, (block_height + 31U) & ~31U);
  const auto expected_size = static_cast<std::size_t>(pitch_blocks) *
                             padded_height * kBlockBytes;
  if (tiled.size() != expected_size) {
    throw RuntimeTrap("Vulkan title texture tiled payload changed");
  }
  std::vector<std::byte> linear(static_cast<std::size_t>(block_width) *
                                block_height * kBlockBytes);
  for (std::uint32_t y = 0U; y < block_height; ++y) {
    for (std::uint32_t x = 0U; x < block_width; ++x) {
      const auto source = title_tiled_2d(x, y, pitch_blocks);
      if (source + kBlockBytes > tiled.size()) {
        throw RuntimeTrap("Vulkan title texture tiled address escaped");
      }
      const auto destination =
          (static_cast<std::size_t>(y) * block_width + x) * kBlockBytes;
      for (std::uint32_t byte = 0U; byte < kBlockBytes; byte += 2U) {
        linear[destination + byte] = tiled[source + byte + 1U];
        linear[destination + byte + 1U] = tiled[source + byte];
      }
    }
  }
  return linear;
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

void qualify_reached_title_draw(const XenosDrawCommand &draw) {
  static constexpr std::string_view kVertex =
      "84e2d87ca4c7e6b6463cd007778e3e76f2d33d5b3a2bdf71bdabedf5e2949e6b";
  static constexpr std::string_view kPixel =
      "8982431ab8c37106400e0cc23a09a9a08b6de1d952dcb7f7def0016bd5714825";
  if (draw.primitive != XenosPrimitive::QuadList ||
      draw.source != XenosIndexSource::AutoIndex || draw.index_count != 4U ||
      draw.index_format != XenosIndexFormat::Uint16 || !draw.predicated ||
      draw.vertex_shader_sha256 != kVertex ||
      draw.pixel_shader_sha256 != kPixel || !draw.registers) {
    throw RuntimeTrap("unqualified PAL title draw provenance");
  }
  const auto &registers = *draw.registers;
  if (registers.value(0x2000U) != 0x14000500U ||
      registers.value(0x2001U) != 0x00000000U ||
      registers.value(0x2104U) != 0x0000000FU ||
      registers.value(0x2180U) != 0x10110103U ||
      registers.value(0x2200U) != 0x00700764U ||
      registers.value(0x2201U) != 0x00010706U ||
      registers.value(0x2202U) != 0x87000007U ||
      registers.value(0x2205U) != 0x00218000U ||
      registers.value(0x2302U) != 0x00000005U ||
      registers.value(0x2208U) != 0x00000004U) {
    throw RuntimeTrap("unqualified PAL title draw register profile");
  }
  if (!qualified_title_texture_profile(registers).has_value()) {
    throw RuntimeTrap("unqualified PAL title texture fetch profile");
  }
}

void destroy_normal_draw_resources(VkDevice device, VkQueue queue,
                                   bool submitted, VkFence &fence,
                                   VkQueryPool &query_pool, VkCommandPool &pool,
                                   Buffer &readback) {
  if (submitted && vkQueueWaitIdle(queue) != VK_SUCCESS) {
    throw RuntimeTrap("Vulkan normal draw cleanup could not drain queue");
  }
  if (fence != VK_NULL_HANDLE) vkDestroyFence(device, fence, nullptr);
  if (query_pool != VK_NULL_HANDLE) vkDestroyQueryPool(device, query_pool, nullptr);
  if (pool != VK_NULL_HANDLE) vkDestroyCommandPool(device, pool, nullptr);
  if (readback.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, readback.buffer, nullptr);
  if (readback.memory != VK_NULL_HANDLE) vkFreeMemory(device, readback.memory, nullptr);
}

void destroy_title_draw_resources(
    VkDevice device, VkQueue queue, bool submitted, VkFence &fence,
    VkQueryPool &query_pool, VkQueryPool &pipeline_stats_query_pool,
    VkCommandPool &command_pool,
    VkDescriptorPool &descriptor_pool, VkSampler &sampler, Buffer &readback,
    Buffer &upload, Image &texture) {
  if (submitted && vkQueueWaitIdle(queue) != VK_SUCCESS) {
    throw RuntimeTrap("Vulkan title draw cleanup could not drain queue");
  }
  if (fence != VK_NULL_HANDLE) vkDestroyFence(device, fence, nullptr);
  if (query_pool != VK_NULL_HANDLE) {
    vkDestroyQueryPool(device, query_pool, nullptr);
  }
  if (pipeline_stats_query_pool != VK_NULL_HANDLE) {
    vkDestroyQueryPool(device, pipeline_stats_query_pool, nullptr);
  }
  if (command_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, command_pool, nullptr);
  }
  if (descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
  }
  if (sampler != VK_NULL_HANDLE) vkDestroySampler(device, sampler, nullptr);
  if (readback.buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, readback.buffer, nullptr);
  }
  if (readback.memory != VK_NULL_HANDLE) {
    vkFreeMemory(device, readback.memory, nullptr);
  }
  if (upload.buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, upload.buffer, nullptr);
  }
  if (upload.memory != VK_NULL_HANDLE) {
    vkFreeMemory(device, upload.memory, nullptr);
  }
  destroy_image(device, texture);
}

} // namespace

void destroy_vulkan_normal_draw_target(
    VkDevice device, VulkanNormalDrawTarget &target) noexcept {
  if (target.framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, target.framebuffer, nullptr);
  }
  destroy_image(device, target.resolved);
  destroy_image(device, target.depth);
  destroy_image(device, target.color);
  target = {};
}

VulkanNormalDrawResult execute_vulkan_normal_draw(
    VkPhysicalDevice physical, VkDevice device, VkQueue queue,
    std::uint32_t queue_family, const XenosDrawCommand &draw,
    VkRenderPass render_pass, VkPipeline pipeline,
    VkPipelineLayout pipeline_layout, VkDescriptorSet shared,
    VkDescriptorSet constants, VulkanNormalDrawTarget &target,
    bool load_existing, bool *cleanup_safe) {
  if (cleanup_safe != nullptr) *cleanup_safe = true;
  qualify_reached_normal_draw(draw);
  if (physical == VK_NULL_HANDLE || device == VK_NULL_HANDLE ||
      queue == VK_NULL_HANDLE || render_pass == VK_NULL_HANDLE ||
      pipeline == VK_NULL_HANDLE || pipeline_layout == VK_NULL_HANDLE ||
      shared == VK_NULL_HANDLE || constants == VK_NULL_HANDLE) {
    throw RuntimeTrap("Vulkan normal draw prerequisites are incomplete");
  }
  if (target.populated() != load_existing) {
    throw RuntimeTrap("Vulkan normal draw target lifetime changed");
  }
  Buffer readback;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  VkQueryPool query_pool = VK_NULL_HANDLE;
  bool submitted = false;
  try {
    if (!load_existing) {
      target.color = create_image(physical, device,
                                  VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_SAMPLE_COUNT_4_BIT,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                  VK_IMAGE_ASPECT_COLOR_BIT);
      target.depth = create_image(
          physical, device, VK_FORMAT_D24_UNORM_S8_UINT,
          VK_SAMPLE_COUNT_4_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
          VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
      target.resolved = create_image(
          physical, device, VK_FORMAT_R8G8B8A8_UNORM,
          VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          VK_IMAGE_ASPECT_COLOR_BIT);
    }
    readback = create_readback(physical, device);
    if (!load_existing) {
      const std::array views{target.color.view, target.depth.view,
                             target.resolved.view};
      VkFramebufferCreateInfo framebuffer_info{
          VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
      framebuffer_info.renderPass = render_pass;
      framebuffer_info.attachmentCount =
          static_cast<std::uint32_t>(views.size());
      framebuffer_info.pAttachments = views.data();
      framebuffer_info.width = kWidth;
      framebuffer_info.height = kHeight;
      framebuffer_info.layers = 1U;
      if (vkCreateFramebuffer(device, &framebuffer_info, nullptr,
                              &target.framebuffer) != VK_SUCCESS) {
        throw RuntimeTrap("Vulkan normal draw framebuffer creation failed");
      }
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
    query_pool = create_precise_occlusion_query_pool(physical, device);
    vkCmdResetQueryPool(command, query_pool, 0U, 1U);
    std::array<VkClearValue, 3> clears{};
    clears[0].color.float32[0] = 1.0F;
    clears[0].color.float32[1] = 0.0F;
    clears[0].color.float32[2] = 1.0F;
    clears[0].color.float32[3] = 1.0F;
    clears[1].depthStencil = {1.0F, 0U};
    VkRenderPassBeginInfo render{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render.renderPass = render_pass;
    render.framebuffer = target.framebuffer;
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
    vkCmdBeginQuery(command, query_pool, 0U, VK_QUERY_CONTROL_PRECISE_BIT);
    vkCmdDraw(command, 3U, 1U, 0U, 0U);
    vkCmdEndQuery(command, query_pool, 0U);
    vkCmdEndRenderPass(command);
    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1U;
    copy.imageExtent = {kWidth, kHeight, 1U};
    vkCmdCopyImageToBuffer(command, target.resolved.image,
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
    const auto passed_samples = query_passed_samples(device, query_pool, true);
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
    if ((!load_existing && passed_samples == 0U) ||
        summary.sentinel == kWidth * kHeight) {
      throw RuntimeTrap("Vulkan normal draw did not modify RT0");
    }
    destroy_normal_draw_resources(device, queue, false, fence, query_pool, pool,
                                  readback);
    return {summary.digest, kWidth, kHeight, std::move(observed)};
  } catch (...) {
    try {
      destroy_normal_draw_resources(device, queue, submitted, fence, query_pool,
                                    pool, readback);
      destroy_vulkan_normal_draw_target(device, target);
    } catch (...) {
      if (cleanup_safe != nullptr) *cleanup_safe = false;
      throw;
    }
    throw;
  }
}

VulkanNormalDrawResult execute_vulkan_title_draw(
    VkPhysicalDevice physical, VkDevice device, VkQueue queue,
    std::uint32_t queue_family, const XenosDrawCommand &draw,
    VkRenderPass render_pass, VkPipeline pipeline,
    VkPipelineLayout pipeline_layout, VkDescriptorSet shared,
    VkDescriptorSet constants, VkDescriptorSetLayout empty_layout,
    VkDescriptorSetLayout texture_layout,
    std::span<const std::byte> tiled_texture, std::uint32_t texture_width,
    std::uint32_t texture_height, VulkanNormalDrawTarget &target,
    bool load_existing, bool *cleanup_safe) {
  if (cleanup_safe != nullptr) *cleanup_safe = true;
  qualify_reached_title_draw(draw);
  if (physical == VK_NULL_HANDLE || device == VK_NULL_HANDLE ||
      queue == VK_NULL_HANDLE || render_pass == VK_NULL_HANDLE ||
      pipeline == VK_NULL_HANDLE || pipeline_layout == VK_NULL_HANDLE ||
      shared == VK_NULL_HANDLE || constants == VK_NULL_HANDLE ||
      empty_layout == VK_NULL_HANDLE || texture_layout == VK_NULL_HANDLE) {
    throw RuntimeTrap("Vulkan title draw received an unavailable dependency");
  }
  const auto fetch0 = draw.registers->value(kXenosTextureFetch00);
  const auto sampler_address_mode = [](std::uint32_t clamp_mode) {
    switch (clamp_mode) {
    case 0U:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case 1U:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case 2U:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    default:
      throw RuntimeTrap("unqualified PAL title sampler clamp mode");
    }
  };
  const auto address_mode_u = sampler_address_mode((fetch0 >> 10U) & 7U);
  const auto address_mode_v = sampler_address_mode((fetch0 >> 13U) & 7U);
  if (target.populated() != load_existing) {
    throw RuntimeTrap("Vulkan title draw target lifetime changed");
  }
  const auto texture_profile = qualified_title_texture_profile(*draw.registers);
  if (!texture_profile.has_value()) {
    throw RuntimeTrap("unqualified PAL title texture fetch profile");
  }
  const auto texture_format =
      texture_profile->encoding == QualifiedTitleTextureEncoding::Bc3
          ? VK_FORMAT_BC3_UNORM_BLOCK
          : VK_FORMAT_R8G8B8A8_UNORM;
  VkFormatProperties format_properties{};
  vkGetPhysicalDeviceFormatProperties(physical, texture_format,
                                      &format_properties);
  constexpr VkFormatFeatureFlags kRequiredTextureFeatures =
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  if ((format_properties.optimalTilingFeatures & kRequiredTextureFeatures) !=
      kRequiredTextureFeatures) {
    throw RuntimeTrap("Vulkan title draw lacks sampled texture support");
  }
  std::vector<std::byte> linear_texture;
  if (texture_profile->encoding == QualifiedTitleTextureEncoding::Bc3) {
    linear_texture =
        untile_title_bc3(tiled_texture, texture_width, texture_height);
  } else {
    linear_texture.resize(kReachedResolveLinearBytes);
    untile_reached_rgba8(tiled_texture, linear_texture);
  }
  if (texture_profile->encoding == QualifiedTitleTextureEncoding::Bc3 &&
      texture_width == 512U &&
      std::getenv("AC6_DEMO_TRACE_TITLE_BC3") != nullptr) {
    const auto byte_at = [&](std::size_t index) {
      return std::to_integer<std::uint8_t>(linear_texture[index]);
    };
    const auto blocks_x = texture_width / 4U;
    const auto blocks_y = texture_height / 4U;
    std::uint64_t alpha_nonzero = 0U;
    std::uint64_t alpha_opaque = 0U;
    std::uint64_t color_nonzero = 0U;
    for (std::uint32_t y = 0U; y < blocks_y; ++y) {
      for (std::uint32_t x = 0U; x < blocks_x; ++x) {
        const auto base =
            (static_cast<std::size_t>(y) * blocks_x + x) * 16U;
        const auto alpha0 = byte_at(base);
        const auto alpha1 = byte_at(base + 1U);
        std::uint64_t indices = 0U;
        for (std::uint32_t byte = 0U; byte < 6U; ++byte) {
          indices |= static_cast<std::uint64_t>(byte_at(base + 2U + byte))
                     << (8U * byte);
        }
        std::array<std::uint8_t, 8> alpha_table{alpha0, alpha1};
        if (alpha0 > alpha1) {
          for (std::uint32_t index = 1U; index <= 6U; ++index) {
            alpha_table[index + 1U] = static_cast<std::uint8_t>(
                ((7U - index) * alpha0 + index * alpha1) / 7U);
          }
        } else {
          for (std::uint32_t index = 1U; index <= 4U; ++index) {
            alpha_table[index + 1U] = static_cast<std::uint8_t>(
                ((5U - index) * alpha0 + index * alpha1) / 5U);
          }
          alpha_table[6] = 0U;
          alpha_table[7] = 255U;
        }
        for (std::uint32_t texel = 0U; texel < 16U; ++texel) {
          const auto alpha = alpha_table[(indices >> (3U * texel)) & 7U];
          alpha_nonzero += alpha != 0U ? 1U : 0U;
          alpha_opaque += alpha == 255U ? 1U : 0U;
        }
        for (std::uint32_t byte = 8U; byte < 16U; ++byte) {
          color_nonzero += byte_at(base + byte) != 0U ? 1U : 0U;
        }
      }
    }
    std::fprintf(stderr,
                 "AC6_TITLE_BC3 width=512 bytes=%zu alpha_nonzero=%llu "
                 "alpha_opaque=%llu color_nonzero_bytes=%llu first="
                 "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
                 linear_texture.size(),
                 static_cast<unsigned long long>(alpha_nonzero),
                 static_cast<unsigned long long>(alpha_opaque),
                 static_cast<unsigned long long>(color_nonzero),
                 byte_at(0U), byte_at(1U), byte_at(2U), byte_at(3U),
                 byte_at(4U), byte_at(5U), byte_at(6U), byte_at(7U),
                 byte_at(8U), byte_at(9U), byte_at(10U), byte_at(11U),
                 byte_at(12U), byte_at(13U), byte_at(14U), byte_at(15U));
  }

  Image texture;
  Buffer upload;
  Buffer readback;
  VkSampler sampler = VK_NULL_HANDLE;
  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  VkCommandPool command_pool = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  VkQueryPool query_pool = VK_NULL_HANDLE;
  VkQueryPool pipeline_stats_query_pool = VK_NULL_HANDLE;
  bool submitted = false;
  const bool trace_pipeline_stats =
      std::getenv("AC6_DEMO_TRACE_TITLE_PIPELINE_STATS") != nullptr;
  try {
    if (!load_existing) {
      target.color = create_image(
          physical, device, VK_FORMAT_R8G8B8A8_UNORM,
          VK_SAMPLE_COUNT_1_BIT,
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          VK_IMAGE_ASPECT_COLOR_BIT, kTitleWidth, kTitleHeight);
    }
    texture = create_image(
        physical, device, texture_format, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT, texture_width, texture_height,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    upload = create_host_buffer(physical, device, linear_texture.size(),
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    readback = create_host_buffer(physical, device, kTitleReadbackBytes,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    void *upload_mapping = nullptr;
    if (vkMapMemory(device, upload.memory, 0U, linear_texture.size(), 0U,
                    &upload_mapping) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title texture staging mapping failed");
    }
    {
      const MappedMemory mapped{device, upload.memory, upload_mapping};
      std::memcpy(upload_mapping, linear_texture.data(), linear_texture.size());
    }

    VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = address_mode_u;
    sampler_info.addressModeV = address_mode_v;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.minLod = 0.0F;
    sampler_info.maxLod = 0.25F;
    sampler_info.maxAnisotropy = 1.0F;
    if (vkCreateSampler(device, &sampler_info, nullptr, &sampler) !=
        VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title sampler creation failed");
    }

    const std::array pool_sizes{
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2U},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 1U}};
    VkDescriptorPoolCreateInfo pool_info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.maxSets = 2U;
    pool_info.poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    if (vkCreateDescriptorPool(device, &pool_info, nullptr,
                               &descriptor_pool) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title descriptor pool creation failed");
    }
    const std::array descriptor_layouts{empty_layout, texture_layout};
    std::array<VkDescriptorSet, 2> local_sets{};
    VkDescriptorSetAllocateInfo descriptor_allocate{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptor_allocate.descriptorPool = descriptor_pool;
    descriptor_allocate.descriptorSetCount =
        static_cast<std::uint32_t>(descriptor_layouts.size());
    descriptor_allocate.pSetLayouts = descriptor_layouts.data();
    if (vkAllocateDescriptorSets(device, &descriptor_allocate,
                                 local_sets.data()) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title descriptor allocation failed");
    }
    const VkDescriptorImageInfo unsigned_image{
        VK_NULL_HANDLE, texture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkDescriptorImageInfo signed_image = unsigned_image;
    const VkDescriptorImageInfo sampler_descriptor{sampler, VK_NULL_HANDLE,
                                                   VK_IMAGE_LAYOUT_UNDEFINED};
    std::array<VkWriteDescriptorSet, 3> descriptor_writes{};
    const std::array image_infos{unsigned_image, signed_image,
                                 sampler_descriptor};
    for (std::uint32_t binding = 0U; binding < descriptor_writes.size();
         ++binding) {
      descriptor_writes[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_writes[binding].dstSet = local_sets[1];
      descriptor_writes[binding].dstBinding = binding;
      descriptor_writes[binding].descriptorCount = 1U;
      descriptor_writes[binding].descriptorType =
          binding < 2U ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
                       : VK_DESCRIPTOR_TYPE_SAMPLER;
      descriptor_writes[binding].pImageInfo = &image_infos[binding];
    }
    vkUpdateDescriptorSets(device,
                           static_cast<std::uint32_t>(descriptor_writes.size()),
                           descriptor_writes.data(), 0U, nullptr);

    VkFramebufferCreateInfo framebuffer_info{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer_info.renderPass = render_pass;
    framebuffer_info.attachmentCount = 1U;
    framebuffer_info.pAttachments = &target.color.view;
    framebuffer_info.width = kTitleWidth;
    framebuffer_info.height = kTitleHeight;
    framebuffer_info.layers = 1U;
    if (!load_existing &&
        vkCreateFramebuffer(device, &framebuffer_info, nullptr,
                            &target.framebuffer) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title framebuffer creation failed");
    }
    query_pool = create_precise_occlusion_query_pool(physical, device);
    if (trace_pipeline_stats) {
      pipeline_stats_query_pool =
          create_title_pipeline_statistics_query_pool(physical, device);
    }
    VkCommandPoolCreateInfo command_pool_info{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    command_pool_info.queueFamilyIndex = queue_family;
    if (vkCreateCommandPool(device, &command_pool_info, nullptr,
                            &command_pool) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title command pool creation failed");
    }
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo command_allocate{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    command_allocate.commandPool = command_pool;
    command_allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_allocate.commandBufferCount = 1U;
    if (vkAllocateCommandBuffers(device, &command_allocate, &command) !=
        VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title command allocation failed");
    }
    VkCommandBufferBeginInfo command_begin{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(command, &command_begin) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title command begin failed");
    }

    VkImageMemoryBarrier texture_upload{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    texture_upload.srcAccessMask = 0U;
    texture_upload.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    texture_upload.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    texture_upload.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    texture_upload.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    texture_upload.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    texture_upload.image = texture.image;
    texture_upload.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    texture_upload.subresourceRange.levelCount = 1U;
    texture_upload.subresourceRange.layerCount = 1U;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0U, 0U, nullptr, 0U,
                         nullptr, 1U, &texture_upload);
    VkBufferImageCopy texture_copy{};
    texture_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    texture_copy.imageSubresource.layerCount = 1U;
    texture_copy.imageExtent = {texture_width, texture_height, 1U};
    vkCmdCopyBufferToImage(command, upload.buffer, texture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U,
                           &texture_copy);
    VkImageMemoryBarrier texture_sample = texture_upload;
    texture_sample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    texture_sample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    texture_sample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    texture_sample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U,
                         nullptr, 0U, nullptr, 1U, &texture_sample);

    vkCmdResetQueryPool(command, query_pool, 0U, 1U);
    if (pipeline_stats_query_pool != VK_NULL_HANDLE) {
      vkCmdResetQueryPool(command, pipeline_stats_query_pool, 0U, 1U);
    }
    VkClearValue clear{};
    VkRenderPassBeginInfo render{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render.renderPass = render_pass;
    render.framebuffer = target.framebuffer;
    render.renderArea.extent = {kTitleWidth, kTitleHeight};
    render.clearValueCount = 1U;
    render.pClearValues = &clear;
    vkCmdBeginRenderPass(command, &render, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    const std::array descriptor_sets{shared, constants, local_sets[0],
                                     local_sets[1]};
    vkCmdBindDescriptorSets(
        command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0U,
        static_cast<std::uint32_t>(descriptor_sets.size()),
        descriptor_sets.data(), 0U, nullptr);
    const VkViewport viewport{0.0F, 0.0F, static_cast<float>(kTitleWidth),
                              static_cast<float>(kTitleHeight), 0.0F, 1.0F};
    const VkRect2D scissor{{0, 0}, {kTitleWidth, kTitleHeight}};
    vkCmdSetViewport(command, 0U, 1U, &viewport);
    vkCmdSetScissor(command, 0U, 1U, &scissor);
    vkCmdSetStencilCompareMask(command, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFU);
    vkCmdSetStencilWriteMask(command, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFU);
    vkCmdSetStencilReference(command, VK_STENCIL_FACE_FRONT_AND_BACK, 0U);
    vkCmdBeginQuery(command, query_pool, 0U, VK_QUERY_CONTROL_PRECISE_BIT);
    if (pipeline_stats_query_pool != VK_NULL_HANDLE) {
      vkCmdBeginQuery(command, pipeline_stats_query_pool, 0U, 0U);
    }
    vkCmdDraw(command, 4U, 1U, 0U, 0U);
    if (pipeline_stats_query_pool != VK_NULL_HANDLE) {
      vkCmdEndQuery(command, pipeline_stats_query_pool, 0U);
    }
    vkCmdEndQuery(command, query_pool, 0U);
    vkCmdEndRenderPass(command);

    VkImageMemoryBarrier color_readback{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    color_readback.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    color_readback.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    color_readback.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_readback.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    color_readback.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    color_readback.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    color_readback.image = target.color.image;
    color_readback.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    color_readback.subresourceRange.levelCount = 1U;
    color_readback.subresourceRange.layerCount = 1U;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0U, 0U, nullptr, 0U,
                         nullptr, 1U, &color_readback);
    VkBufferImageCopy readback_copy{};
    readback_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    readback_copy.imageSubresource.layerCount = 1U;
    readback_copy.imageExtent = {kTitleWidth, kTitleHeight, 1U};
    vkCmdCopyImageToBuffer(command, target.color.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readback.buffer, 1U, &readback_copy);
    VkImageMemoryBarrier color_restore = color_readback;
    color_restore.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    color_restore.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    color_restore.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    color_restore.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0U,
                         0U, nullptr, 0U, nullptr, 1U, &color_restore);
    if (vkEndCommandBuffer(command) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title command end failed");
    }
    VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateFence(device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title fence creation failed");
    }
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1U;
    submit.pCommandBuffers = &command;
    if (vkQueueSubmit(queue, 1U, &submit, fence) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title submission failed");
    }
    submitted = true;
    if (vkWaitForFences(device, 1U, &fence, VK_TRUE, 10'000'000'000ULL) !=
        VK_SUCCESS) {
      if (vkQueueWaitIdle(queue) != VK_SUCCESS) {
        throw RuntimeTrap("Vulkan title queue did not become idle");
      }
      throw RuntimeTrap("Vulkan title submission timed out");
    }
    const auto passed_samples = query_passed_samples(device, query_pool, true);
    if (pipeline_stats_query_pool != VK_NULL_HANDLE) {
      const auto stats = read_title_pipeline_statistics(
          device, pipeline_stats_query_pool);
      std::fprintf(
          stderr,
          "AC6_TITLE_PIPELINE_STATS width=%u height=%u "
          "input_vertices=%llu input_primitives=%llu "
          "vertex_invocations=%llu geometry_invocations=%llu "
          "geometry_primitives=%llu clipping_invocations=%llu "
          "clipping_primitives=%llu fragment_invocations=%llu\n",
          texture_width, texture_height,
          static_cast<unsigned long long>(stats.input_vertices),
          static_cast<unsigned long long>(stats.input_primitives),
          static_cast<unsigned long long>(stats.vertex_invocations),
          static_cast<unsigned long long>(stats.geometry_invocations),
          static_cast<unsigned long long>(stats.geometry_primitives),
          static_cast<unsigned long long>(stats.clipping_invocations),
          static_cast<unsigned long long>(stats.clipping_primitives),
          static_cast<unsigned long long>(stats.fragment_invocations));
    }
    void *readback_mapping = nullptr;
    if (vkMapMemory(device, readback.memory, 0U, kTitleReadbackBytes, 0U,
                    &readback_mapping) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan title readback mapping failed");
    }
    ReadbackSummary summary;
    std::vector<std::byte> observed;
    std::uint64_t rgb_nonzero = 0U;
    {
      const MappedMemory mapped{device, readback.memory, readback_mapping};
      const auto bytes = std::span{
          static_cast<const std::byte *>(readback_mapping),
          static_cast<std::size_t>(kTitleReadbackBytes)};
      summary = summarize_readback(bytes);
      observed.assign(bytes.begin(), bytes.end());
      for (std::size_t pixel = 0U; pixel < bytes.size(); pixel += 4U) {
        rgb_nonzero += bytes[pixel] != std::byte{} ||
                               bytes[pixel + 1U] != std::byte{} ||
                               bytes[pixel + 2U] != std::byte{}
                           ? 1U
                           : 0U;
      }
    }
    std::fprintf(stderr,
                 "AC6_TITLE_DRAW_RESULT width=%u height=%u rgb_nonzero=%llu "
                 "black_pixels=%u texture_nonzero=%zu passed_samples=%llu "
                 "sha256=%s\n",
                 kTitleWidth, kTitleHeight,
                 static_cast<unsigned long long>(rgb_nonzero), summary.black,
                 static_cast<std::size_t>(std::ranges::count_if(
                     linear_texture,
                     [](std::byte value) { return value != std::byte{}; })),
                 static_cast<unsigned long long>(passed_samples),
                 summary.digest.c_str());
    if (const char *ppm_path = std::getenv("AC6_DEMO_DUMP_READBACK_PPM");
        texture_width == 1280U && ppm_path != nullptr) {
      if (FILE *file = std::fopen(ppm_path, "wb"); file != nullptr) {
        std::fprintf(file, "P6\n%u %u\n255\n", kTitleWidth, kTitleHeight);
        for (std::size_t pixel = 0U; pixel < observed.size(); pixel += 4U) {
          std::fwrite(&observed[pixel], 1U, 3U, file);
        }
        std::fclose(file);
      }
    }
    destroy_title_draw_resources(
        device, queue, false, fence, query_pool, pipeline_stats_query_pool,
        command_pool,
        descriptor_pool, sampler, readback, upload, texture);
    return {summary.digest, kTitleWidth, kTitleHeight, std::move(observed)};
  } catch (...) {
    try {
      destroy_title_draw_resources(
          device, queue, submitted, fence, query_pool,
          pipeline_stats_query_pool, command_pool,
          descriptor_pool, sampler, readback, upload, texture);
      destroy_vulkan_normal_draw_target(device, target);
    } catch (...) {
      if (cleanup_safe != nullptr) *cleanup_safe = false;
      throw;
    }
    throw;
  }
}

} // namespace ac6demo

#endif
