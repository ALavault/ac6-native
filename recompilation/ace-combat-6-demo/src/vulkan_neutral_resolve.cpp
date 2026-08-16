#include "ac6demo/vulkan_neutral_resolve.hpp"

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include "ac6demo/runtime_error.hpp"
#include "ac6demo/xenos_tiling.hpp"

#include "resolve_fast_32bpp_1x2xmsaa_cs.h"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <span>
#include <vector>

namespace ac6demo {
namespace {

constexpr std::uint32_t kWidth = 1280U;
constexpr std::uint32_t kHeight = 720U;
constexpr std::uint32_t kNormalWidth = 640U;
constexpr std::uint32_t kNormalHeight = 360U;
constexpr VkDeviceSize kEdramBytes = 0xA00000U;
constexpr VkDeviceSize kTiledBytes = 0x398000U;
constexpr VkDeviceSize kGuardBytes = 0x1000U;
constexpr VkDeviceSize kEdramTileBytes = 80U * 16U * 4U;
constexpr std::uint32_t kEdramPitchTiles = 16U;
constexpr VkDeviceSize kEdramSurfaceBytes =
    kEdramPitchTiles * ((kHeight + 15U) / 16U) * kEdramTileBytes;
constexpr std::byte kCanary{0xA5};
constexpr std::byte kEdramCanary{0x5A};
constexpr char kNormalBlack[] =
    "0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366";
constexpr char kLinearBlack[] =
    "0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f";

struct Buffer final {
  VkBuffer buffer{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  VkDeviceSize size{};
  void *mapping{};
};

void check(VkResult result, std::string_view operation) {
  if (result != VK_SUCCESS) {
    throw RuntimeTrap(std::string(operation) + " failed: " +
                      std::to_string(result));
  }
}

std::uint32_t memory_type(VkPhysicalDevice physical, std::uint32_t allowed) {
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(physical, &properties);
  constexpr auto required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  for (std::uint32_t index = 0U; index < properties.memoryTypeCount; ++index) {
    if ((allowed & (1U << index)) != 0U &&
        (properties.memoryTypes[index].propertyFlags & required) == required) {
      return index;
    }
  }
  throw RuntimeTrap("Vulkan neutral resolve lacks coherent host memory");
}

Buffer create_buffer(VkPhysicalDevice physical, VkDevice device,
                     VkDeviceSize size) {
  Buffer result;
  result.size = size;
  try {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(device, &info, nullptr, &result.buffer),
          "neutral resolve buffer creation");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, result.buffer, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex =
        memory_type(physical, requirements.memoryTypeBits);
    check(vkAllocateMemory(device, &allocation, nullptr, &result.memory),
          "neutral resolve memory allocation");
    check(vkBindBufferMemory(device, result.buffer, result.memory, 0U),
          "neutral resolve memory binding");
    check(vkMapMemory(device, result.memory, 0U, size, 0U, &result.mapping),
          "neutral resolve memory mapping");
    return result;
  } catch (...) {
    if (result.mapping != nullptr) vkUnmapMemory(device, result.memory);
    if (result.memory != VK_NULL_HANDLE) vkFreeMemory(device, result.memory, nullptr);
    if (result.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, result.buffer, nullptr);
    throw;
  }
}

void destroy_buffer(VkDevice device, Buffer &buffer) noexcept {
  if (buffer.mapping != nullptr) vkUnmapMemory(device, buffer.memory);
  if (buffer.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer.buffer, nullptr);
  if (buffer.memory != VK_NULL_HANDLE) vkFreeMemory(device, buffer.memory, nullptr);
  buffer = {};
}

std::string sha256(std::span<const std::byte> bytes) {
  std::array<unsigned char, 32> digest{};
  unsigned int digest_size = 0U;
  EVP_MD_CTX *context = EVP_MD_CTX_new();
  if (context == nullptr ||
      EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1 ||
      EVP_DigestUpdate(context, bytes.data(), bytes.size()) != 1 ||
      EVP_DigestFinal_ex(context, digest.data(), &digest_size) != 1 ||
      digest_size != digest.size()) {
    EVP_MD_CTX_free(context);
    throw RuntimeTrap("neutral resolve SHA-256 failed");
  }
  EVP_MD_CTX_free(context);
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const auto byte : digest) {
    output << std::setw(2) << static_cast<unsigned>(byte);
  }
  return output.str();
}

void qualify(const VulkanNormalDrawResult &normal,
             const XenosDrawCommand &copy,
             const XenosPresentCommand &present) {
  static constexpr std::string_view kCopyVertex =
      "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0";
  static constexpr std::string_view kPixel =
      "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25";
  constexpr std::size_t kNormalReadbackBytes =
      static_cast<std::size_t>(640U) * 360U * 4U;
  const bool normal_readback_is_black =
      normal.resolved_rgba8.size() == kNormalReadbackBytes &&
      std::all_of(normal.resolved_rgba8.begin(), normal.resolved_rgba8.end(),
                  [](std::byte value) { return value == std::byte{}; });
  if (normal.width != 640U || normal.height != 360U ||
      normal.resolved_rgba8_sha256 != kNormalBlack ||
      !normal_readback_is_black ||
      copy.primitive != XenosPrimitive::RectangleList || copy.index_count != 3U ||
      copy.source != XenosIndexSource::AutoIndex || copy.predicated ||
      copy.vertex_shader_sha256 != kCopyVertex ||
      copy.pixel_shader_sha256 != kPixel || !copy.registers) {
    throw RuntimeTrap("unqualified neutral copy/resolve join");
  }
  const auto &registers = *copy.registers;
  if (registers.value(0x2000U) != 0x14000500U ||
      registers.value(0x2104U) != 0x0000000FU ||
      registers.value(0x2180U) != 0x00010002U ||
      registers.value(0x2200U) != 0x00000000U ||
      registers.value(0x2201U) != 0x00010001U ||
      registers.value(0x2208U) != 0x00000006U ||
      registers.value(0x2318U) != 0x00100000U ||
      registers.value(0x2319U) != 0x1374A000U ||
      registers.value(0x231AU) != 0x02D00500U ||
      registers.value(0x231BU) != 0x01000300U) {
    std::ostringstream values;
    values << "neutral copy/resolve register profile changed:" << std::hex;
    for (const auto index : {0x2000U, 0x2104U, 0x2180U, 0x2200U, 0x2201U,
                             0x2208U, 0x2318U, 0x2319U, 0x231AU, 0x231BU}) {
      values << " " << index << "=" << registers.value(index);
    }
    throw RuntimeTrap(values.str());
  }
  if (present.format != 6U || !present.tiled || present.width != kWidth ||
      present.height != kHeight || present.physical_address != 0x1374A000U) {
    throw RuntimeTrap("neutral resolve XE_SWAP destination join changed");
  }
}

// The reached normal draw is a 640x360 4x-MSAA color surface.  Its exact
// readback is all zero, so each pixel is known to occupy the same zero value
// at all four Xenos samples.  Materialize only that qualified black surface
// into the Xenos 80x16-sample, 16-tile-pitch layout.  A non-zero readback is
// deliberately rejected until endian/channel packing has its own PAL proof.
void materialize_reached_black_edram(const VulkanNormalDrawResult &normal,
                                     Buffer &edram) {
  constexpr std::size_t kNormalBytes =
      static_cast<std::size_t>(kNormalWidth) * kNormalHeight * 4U;
  if (normal.width != kNormalWidth || normal.height != kNormalHeight ||
      normal.resolved_rgba8.size() != kNormalBytes ||
      normal.resolved_rgba8_sha256 != kNormalBlack ||
      !std::all_of(normal.resolved_rgba8.begin(), normal.resolved_rgba8.end(),
                   [](std::byte value) { return value == std::byte{}; })) {
    throw RuntimeTrap(
        "non-black or incomplete normal readback cannot materialize reached EDRAM");
  }
  if (edram.mapping == nullptr || edram.size < kEdramBytes) {
    throw RuntimeTrap("reached EDRAM buffer is incomplete");
  }
  auto *bytes = static_cast<std::byte *>(edram.mapping);
  std::fill_n(bytes, static_cast<std::size_t>(kEdramBytes), kEdramCanary);
  for (std::uint32_t y = 0U; y < kNormalHeight; ++y) {
    for (std::uint32_t x = 0U; x < kNormalWidth; ++x) {
      const auto *pixel = normal.resolved_rgba8.data() +
                          (static_cast<std::size_t>(y) * kNormalWidth + x) * 4U;
      for (std::uint32_t sample_y = 0U; sample_y < 2U; ++sample_y) {
        for (std::uint32_t sample_x = 0U; sample_x < 2U; ++sample_x) {
          const std::uint32_t sample_x_abs = (x << 1U) | sample_x;
          const std::uint32_t sample_y_abs = (y << 1U) | sample_y;
          const std::uint32_t tile_x = sample_x_abs / 80U;
          const std::uint32_t tile_y = sample_y_abs / 16U;
          const std::uint32_t in_tile_x = sample_x_abs % 80U;
          const std::uint32_t in_tile_y = sample_y_abs % 16U;
          const VkDeviceSize offset =
              (static_cast<VkDeviceSize>(tile_y) * kEdramPitchTiles + tile_x) *
                  kEdramTileBytes +
              (static_cast<VkDeviceSize>(in_tile_y) * 80U + in_tile_x) * 4U;
          if (offset + 4U > kEdramSurfaceBytes || offset + 4U > kEdramBytes) {
            throw RuntimeTrap("reached EDRAM sample exceeds qualified surface");
          }
          std::copy_n(pixel, 4U, bytes + offset);
        }
      }
    }
  }
  if (!std::all_of(bytes, bytes + kEdramSurfaceBytes,
                   [](std::byte value) { return value == std::byte{}; })) {
    throw RuntimeTrap("reached EDRAM black surface materialization mismatch");
  }
}

} // namespace

VulkanNeutralResolveResult execute_vulkan_neutral_resolve(
    VkPhysicalDevice physical, VkDevice device, VkQueue queue,
    std::uint32_t queue_family, const VulkanNormalDrawResult &normal,
    const XenosDrawCommand &copy, const XenosPresentCommand &present) {
  qualify(normal, copy, present);
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical, &properties);
  if (properties.limits.maxStorageBufferRange < kEdramBytes ||
      properties.limits.maxPushConstantsSize < 20U) {
    throw RuntimeTrap("Vulkan neutral resolve limits are insufficient");
  }
  Buffer edram{};
  Buffer destination{};
  VkShaderModule shader = VK_NULL_HANDLE;
  VkDescriptorSetLayout edram_layout = VK_NULL_HANDLE;
  VkDescriptorSetLayout destination_layout = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  VkCommandPool command_pool = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  try {
    edram = create_buffer(physical, device, kEdramBytes);
    destination = create_buffer(physical, device,
                                kGuardBytes + kTiledBytes + kGuardBytes);
    // Build the source from the exact reached normal draw, never from an
    // unqualified synthetic EDRAM image.  The helper leaves a canary outside
    // the proven 640x360 4x-MSAA surface so an over-read changes the result.
    materialize_reached_black_edram(normal, edram);
    std::memset(destination.mapping, std::to_integer<int>(kCanary),
                static_cast<std::size_t>(kGuardBytes + kTiledBytes + kGuardBytes));
    VkShaderModuleCreateInfo shader_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shader_info.codeSize = sizeof(resolve_fast_32bpp_1x2xmsaa_cs);
    shader_info.pCode = resolve_fast_32bpp_1x2xmsaa_cs;
    check(vkCreateShaderModule(device, &shader_info, nullptr, &shader),
          "neutral resolve shader creation");
    VkDescriptorSetLayoutBinding binding{};
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1U;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo layout_info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.bindingCount = 1U;
    layout_info.pBindings = &binding;
    check(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &edram_layout),
          "neutral resolve EDRAM layout creation");
    check(vkCreateDescriptorSetLayout(device, &layout_info, nullptr,
                                      &destination_layout),
          "neutral resolve destination layout creation");
    const std::array layouts{edram_layout, destination_layout};
    VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0U, 20U};
    VkPipelineLayoutCreateInfo pipeline_layout_info{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeline_layout_info.setLayoutCount = 2U;
    pipeline_layout_info.pSetLayouts = layouts.data();
    pipeline_layout_info.pushConstantRangeCount = 1U;
    pipeline_layout_info.pPushConstantRanges = &push;
    check(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr,
                                 &pipeline_layout),
          "neutral resolve pipeline layout creation");
    VkPipelineShaderStageCreateInfo stage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shader;
    stage.pName = "main";
    VkComputePipelineCreateInfo pipeline_info{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipeline_info.stage = stage;
    pipeline_info.layout = pipeline_layout;
    check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1U, &pipeline_info,
                                   nullptr, &pipeline),
          "neutral resolve pipeline creation");
    VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2U};
    VkDescriptorPoolCreateInfo pool_info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.maxSets = 2U;
    pool_info.poolSizeCount = 1U;
    pool_info.pPoolSizes = &pool_size;
    check(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool),
          "neutral resolve descriptor pool creation");
    VkDescriptorSetAllocateInfo allocate{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool = descriptor_pool;
    allocate.descriptorSetCount = 2U;
    allocate.pSetLayouts = layouts.data();
    std::array<VkDescriptorSet, 2> sets{};
    check(vkAllocateDescriptorSets(device, &allocate, sets.data()),
          "neutral resolve descriptor allocation");
    VkDescriptorBufferInfo edram_info{edram.buffer, 0U, kEdramBytes};
    VkDescriptorBufferInfo destination_info{destination.buffer, kGuardBytes,
                                             kTiledBytes};
    std::array<VkWriteDescriptorSet, 2> writes{};
    std::array infos{edram_info, destination_info};
    for (std::uint32_t index = 0U; index < 2U; ++index) {
      writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[index].dstSet = sets[index];
      writes[index].descriptorCount = 1U;
      writes[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[index].pBufferInfo = &infos[index];
    }
    vkUpdateDescriptorSets(device, 2U, writes.data(), 0U, nullptr);
    VkCommandPoolCreateInfo command_pool_info{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    command_pool_info.queueFamilyIndex = queue_family;
    check(vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool),
          "neutral resolve command pool creation");
    VkCommandBufferAllocateInfo command_allocate{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    command_allocate.commandPool = command_pool;
    command_allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_allocate.commandBufferCount = 1U;
    VkCommandBuffer command = VK_NULL_HANDLE;
    check(vkAllocateCommandBuffers(device, &command_allocate, &command),
          "neutral resolve command allocation");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    check(vkBeginCommandBuffer(command, &begin), "neutral resolve command begin");
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline_layout, 0U, 2U, sets.data(), 0U, nullptr);
    constexpr std::array<std::uint32_t, 5> constants{
        0x00000010U, 0x00091400U, 0x01000300U, 0x00005C28U, 0U};
    vkCmdPushConstants(command, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0U, 20U, constants.data());
    vkCmdDispatch(command, 20U, 90U, 1U);
    check(vkEndCommandBuffer(command), "neutral resolve command end");
    VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    check(vkCreateFence(device, &fence_info, nullptr, &fence),
          "neutral resolve fence creation");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1U;
    submit.pCommandBuffers = &command;
    check(vkQueueSubmit(queue, 1U, &submit, fence), "neutral resolve submit");
    check(vkWaitForFences(device, 1U, &fence, VK_TRUE, 10'000'000'000ULL),
          "neutral resolve wait");
    const auto *bytes = static_cast<const std::byte *>(destination.mapping);
    if (!std::all_of(bytes, bytes + kGuardBytes,
                     [](std::byte value) { return value == kCanary; }) ||
        !std::all_of(bytes + kGuardBytes + kTiledBytes,
                     bytes + kGuardBytes + kTiledBytes + kGuardBytes,
                     [](std::byte value) { return value == kCanary; })) {
      throw RuntimeTrap("neutral resolve crossed a destination guard");
    }
    std::vector<std::byte> expected(static_cast<std::size_t>(kTiledBytes),
                                    kCanary);
    for (std::uint32_t y = 0U; y < kHeight; ++y) {
      for (std::uint32_t x = 0U; x < kWidth; ++x) {
        const auto offset = reached_rgba8_tiled_offset(x, y);
        std::fill_n(expected.begin() + static_cast<std::ptrdiff_t>(offset), 4,
                    std::byte{});
      }
    }
    const auto *tiled = bytes + kGuardBytes;
    if (std::memcmp(tiled, expected.data(), expected.size()) != 0) {
      throw RuntimeTrap("neutral resolve tiled output differs from CPU oracle");
    }
    std::vector<std::byte> linear(kReachedResolveLinearBytes);
    untile_reached_rgba8(
        std::span<const std::byte>(tiled, static_cast<std::size_t>(kTiledBytes)),
        linear);
    const std::string digest = sha256(linear);
    if (digest != kLinearBlack) {
      throw RuntimeTrap("neutral resolve linear digest mismatch: " + digest);
    }
    const std::string tiled_digest = sha256(
        std::span<const std::byte>(tiled, static_cast<std::size_t>(kTiledBytes)));
    std::vector<std::byte> tiled_bytes(static_cast<std::size_t>(kTiledBytes));
    std::copy_n(tiled, static_cast<std::size_t>(kTiledBytes),
                tiled_bytes.data());
    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, command_pool, nullptr);
    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, destination_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, edram_layout, nullptr);
    vkDestroyShaderModule(device, shader, nullptr);
    destroy_buffer(device, destination);
    destroy_buffer(device, edram);
    return {digest, tiled_digest, kWidth, kHeight,
            present.physical_address, static_cast<std::uint32_t>(kTiledBytes),
            true, std::move(tiled_bytes), {}, {}, false};
  } catch (...) {
    if (fence != VK_NULL_HANDLE) vkDestroyFence(device, fence, nullptr);
    if (command_pool != VK_NULL_HANDLE) vkDestroyCommandPool(device, command_pool, nullptr);
    if (descriptor_pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline, nullptr);
    if (pipeline_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    if (destination_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, destination_layout, nullptr);
    if (edram_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, edram_layout, nullptr);
    if (shader != VK_NULL_HANDLE) vkDestroyShaderModule(device, shader, nullptr);
    destroy_buffer(device, destination);
    destroy_buffer(device, edram);
    throw;
  }
}

} // namespace ac6demo

#endif
