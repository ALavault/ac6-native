#include <vulkan/vulkan.h>

#include <openssl/sha.h>

#include "resolve_fast_32bpp_1x2xmsaa_cs.h"
#include "ac6demo/xenos_tiling.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kWidth = 1280;
constexpr std::uint32_t kHeight = 720;
constexpr VkDeviceSize kEdramSize = 0xA00000;
constexpr VkDeviceSize kTiledExtent = 0x398000;
constexpr std::uint8_t kCanary = 0xA5;
constexpr char kExpectedLinearBlackSha[] =
    "0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f";
constexpr char kExpectedTiledBlackSha[] =
    "94831d4c398252020f792d92f546c5122ad522c4270b73be9e8619fde1db641f";
constexpr char kExpectedTiledAsymmetricSha[] =
    "0bf69cf42fd6c3ac73b30c438a4db6d1664eaafa9c716b9ba330a9886c976786";
constexpr char kExpectedLinearAsymmetricSha[] =
    "66dde082635ccc6b24abba5b372ceb10173bc2b062faa2d93de7c4548bb60dc8";

void check(VkResult result, const char *operation) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(std::string(operation) + " failed: " +
                             std::to_string(result));
  }
}

struct Buffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  void *mapping = nullptr;
};

std::uint32_t memory_type(const VkPhysicalDeviceMemoryProperties &properties,
                          std::uint32_t candidates,
                          VkMemoryPropertyFlags required) {
  for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
    if ((candidates & (1U << i)) != 0U &&
        (properties.memoryTypes[i].propertyFlags & required) == required) {
      return i;
    }
  }
  throw std::runtime_error("no qualified host-visible coherent memory type");
}

Buffer make_buffer(VkDevice device,
                   const VkPhysicalDeviceMemoryProperties &memory_properties,
                   VkDeviceSize size) {
  Buffer result;
  result.size = size;
  VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer_info.size = size;
  buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  check(vkCreateBuffer(device, &buffer_info, nullptr, &result.buffer),
        "vkCreateBuffer");
  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device, result.buffer, &requirements);
  VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocation.allocationSize = requirements.size;
  allocation.memoryTypeIndex = memory_type(
      memory_properties, requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  check(vkAllocateMemory(device, &allocation, nullptr, &result.memory),
        "vkAllocateMemory");
  check(vkBindBufferMemory(device, result.buffer, result.memory, 0),
        "vkBindBufferMemory");
  check(vkMapMemory(device, result.memory, 0, size, 0, &result.mapping),
        "vkMapMemory");
  return result;
}

void destroy_buffer(VkDevice device, Buffer &buffer) {
  if (buffer.mapping != nullptr) {
    vkUnmapMemory(device, buffer.memory);
  }
  if (buffer.buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, buffer.buffer, nullptr);
  }
  if (buffer.memory != VK_NULL_HANDLE) {
    vkFreeMemory(device, buffer.memory, nullptr);
  }
}

std::string sha256(const void *data, std::size_t size) {
  std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
  SHA256(static_cast<const unsigned char *>(data), size, digest.data());
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (unsigned char byte : digest) {
    output << std::setw(2) << static_cast<unsigned int>(byte);
  }
  return output.str();
}

struct ResolveDigests {
  std::string tiled;
  std::string linear;
};

// Explicit packing of ReXGlue ResolveCopyShaderConstants for the reached copy
// draw. This avoids depending on implementation-defined C++ bitfield layout.
constexpr std::array<std::uint32_t, 5> reached_constants() {
  constexpr std::uint32_t pitch_tiles = 16;
  constexpr std::uint32_t msaa_1x = 0;
  constexpr std::uint32_t color_base_tiles = 0;
  constexpr std::uint32_t color_format_8_8_8_8 = 0;
  constexpr std::uint32_t edram_info = pitch_tiles | (msaa_1x << 10U) |
                                       (color_base_tiles << 13U) |
                                       (color_format_8_8_8_8 << 24U);
  constexpr std::uint32_t coordinate_info =
      ((kWidth / 8U) << 5U) | (1U << 16U) | (1U << 19U);
  constexpr std::uint32_t dest_info = 0x01000300U;
  constexpr std::uint32_t dest_coordinate_info =
      (kWidth / 32U) | (((kHeight + 31U) / 32U) << 10U);
  constexpr std::uint32_t dest_base_relative = 0;
  return {edram_info, coordinate_info, dest_info, dest_coordinate_info,
          dest_base_relative};
}

class VulkanResolveHarness {
public:
  VulkanResolveHarness() {
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "ac6-demo-vulkan-resolve-tests";
    application.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo = &application;
    check(vkCreateInstance(&instance_info, nullptr, &instance_),
          "vkCreateInstance");

    std::uint32_t physical_count = 0;
    check(vkEnumeratePhysicalDevices(instance_, &physical_count, nullptr),
          "vkEnumeratePhysicalDevices(count)");
    if (physical_count == 0) {
      throw std::runtime_error("no Vulkan physical device");
    }
    std::vector<VkPhysicalDevice> physical_devices(physical_count);
    check(vkEnumeratePhysicalDevices(instance_, &physical_count,
                                     physical_devices.data()),
          "vkEnumeratePhysicalDevices");
    physical_ = physical_devices.front();
    vkGetPhysicalDeviceMemoryProperties(physical_, &memory_properties_);
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_, &properties);
    if (properties.limits.maxStorageBufferRange < kEdramSize ||
        properties.limits.maxPushConstantsSize < 20) {
      throw std::runtime_error("Vulkan device lacks reached resolve limits");
    }
    destination_guard_ = std::max<VkDeviceSize>(
        0x1000, properties.limits.minStorageBufferOffsetAlignment);
    const VkDeviceSize alignment =
        properties.limits.minStorageBufferOffsetAlignment;
    if (alignment != 0) {
      destination_guard_ =
          (destination_guard_ + alignment - 1U) & ~(alignment - 1U);
    }

    std::uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_, &queue_count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_, &queue_count,
                                             queues.data());
    queue_family_ = queue_count;
    for (std::uint32_t i = 0; i < queue_count; ++i) {
      if ((queues[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0U) {
        queue_family_ = i;
        break;
      }
    }
    if (queue_family_ == queue_count) {
      throw std::runtime_error("no Vulkan compute queue");
    }
    float priority = 1.0F;
    VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = queue_family_;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    check(vkCreateDevice(physical_, &device_info, nullptr, &device_),
          "vkCreateDevice");
    vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
    initialize_pipeline();
    edram_ = make_buffer(device_, memory_properties_, kEdramSize);
    destination_ = make_buffer(device_, memory_properties_,
                               kTiledExtent + 2U * destination_guard_);
    update_descriptors();
  }

  ~VulkanResolveHarness() {
    if (device_ != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device_);
      destroy_buffer(device_, destination_);
      destroy_buffer(device_, edram_);
      vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
      vkDestroyPipeline(device_, pipeline_, nullptr);
      vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
      vkDestroyDescriptorSetLayout(device_, destination_layout_, nullptr);
      vkDestroyDescriptorSetLayout(device_, edram_layout_, nullptr);
      vkDestroyShaderModule(device_, shader_, nullptr);
      vkDestroyCommandPool(device_, command_pool_, nullptr);
      vkDestroyDevice(device_, nullptr);
    }
    if (instance_ != VK_NULL_HANDLE) {
      vkDestroyInstance(instance_, nullptr);
    }
  }

  ResolveDigests dispatch_reached(std::array<std::uint8_t, 4> source_pixel,
                                  std::array<std::uint8_t, 4> expected_pixel) {
    auto *edram_bytes = static_cast<std::uint8_t *>(edram_.mapping);
    for (VkDeviceSize offset = 0; offset < edram_.size; offset += 4U) {
      std::memcpy(edram_bytes + offset, source_pixel.data(), source_pixel.size());
    }
    std::memset(destination_.mapping, kCanary,
                static_cast<std::size_t>(destination_.size));
    VkCommandBufferAllocateInfo allocate{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = command_pool_;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    check(vkAllocateCommandBuffers(device_, &allocate, &command),
          "vkAllocateCommandBuffers");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(command, &begin), "vkBeginCommandBuffer");
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    std::array<VkDescriptorSet, 2> sets{edram_set_, destination_set_};
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline_layout_, 0, sets.size(), sets.data(), 0,
                            nullptr);
    constexpr auto constants = reached_constants();
    vkCmdPushConstants(command, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(constants), constants.data());
    vkCmdDispatch(command, 20, 90, 1);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0,
                         nullptr, 0, nullptr);
    check(vkEndCommandBuffer(command), "vkEndCommandBuffer");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    check(vkQueueSubmit(queue_, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit");
    check(vkQueueWaitIdle(queue_), "vkQueueWaitIdle");
    const auto *allocation =
        static_cast<const std::uint8_t *>(destination_.mapping);
    for (VkDeviceSize i = 0; i < destination_guard_; ++i) {
      if (allocation[i] != kCanary ||
          allocation[destination_guard_ + kTiledExtent + i] != kCanary) {
        throw std::runtime_error("resolve changed a destination guard");
      }
    }
    const auto *tiled = allocation + destination_guard_;
    std::vector<std::byte> expected(static_cast<std::size_t>(kTiledExtent),
                                    std::byte{kCanary});
    for (std::uint32_t y = 0; y < kHeight; ++y) {
      for (std::uint32_t x = 0; x < kWidth; ++x) {
        const std::size_t offset = ac6demo::reached_rgba8_tiled_offset(x, y);
        std::memcpy(expected.data() + offset, expected_pixel.data(), 4U);
      }
    }
    if (std::memcmp(tiled, expected.data(), expected.size()) != 0) {
      throw std::runtime_error(
          "Vulkan resolve tiled bytes differ from the CPU oracle");
    }
    const std::string tiled_digest = sha256(tiled, kTiledExtent);
    std::vector<std::byte> linear(ac6demo::kReachedResolveLinearBytes);
    ac6demo::untile_reached_rgba8(
        std::span<const std::byte>(reinterpret_cast<const std::byte *>(tiled),
                                   static_cast<std::size_t>(kTiledExtent)),
        linear);
    const std::string linear_digest = sha256(linear.data(), linear.size());
    vkFreeCommandBuffers(device_, command_pool_, 1, &command);
    return {tiled_digest, linear_digest};
  }

private:
  void initialize_pipeline() {
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.queueFamilyIndex = queue_family_;
    check(vkCreateCommandPool(device_, &pool, nullptr, &command_pool_),
          "vkCreateCommandPool");
    VkShaderModuleCreateInfo shader_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shader_info.codeSize = sizeof(resolve_fast_32bpp_1x2xmsaa_cs);
    shader_info.pCode = resolve_fast_32bpp_1x2xmsaa_cs;
    check(vkCreateShaderModule(device_, &shader_info, nullptr, &shader_),
          "vkCreateShaderModule");
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo layout{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount = 1;
    layout.pBindings = &binding;
    check(vkCreateDescriptorSetLayout(device_, &layout, nullptr,
                                      &edram_layout_),
          "vkCreateDescriptorSetLayout(edram)");
    check(vkCreateDescriptorSetLayout(device_, &layout, nullptr,
                                      &destination_layout_),
          "vkCreateDescriptorSetLayout(destination)");
    std::array<VkDescriptorSetLayout, 2> layouts{edram_layout_,
                                                  destination_layout_};
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push.size = 20;
    VkPipelineLayoutCreateInfo pipeline_layout_info{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeline_layout_info.setLayoutCount = layouts.size();
    pipeline_layout_info.pSetLayouts = layouts.data();
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push;
    check(vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr,
                                 &pipeline_layout_),
          "vkCreatePipelineLayout");
    VkPipelineShaderStageCreateInfo stage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shader_;
    stage.pName = "main";
    VkComputePipelineCreateInfo pipeline_info{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipeline_info.stage = stage;
    pipeline_info.layout = pipeline_layout_;
    check(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info,
                                   nullptr, &pipeline_),
          "vkCreateComputePipelines");
    VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
    VkDescriptorPoolCreateInfo descriptor_pool_info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    descriptor_pool_info.maxSets = 2;
    descriptor_pool_info.poolSizeCount = 1;
    descriptor_pool_info.pPoolSizes = &pool_size;
    check(vkCreateDescriptorPool(device_, &descriptor_pool_info, nullptr,
                                 &descriptor_pool_),
          "vkCreateDescriptorPool");
    VkDescriptorSetAllocateInfo set_allocate{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    set_allocate.descriptorPool = descriptor_pool_;
    set_allocate.descriptorSetCount = layouts.size();
    set_allocate.pSetLayouts = layouts.data();
    std::array<VkDescriptorSet, 2> sets{};
    check(vkAllocateDescriptorSets(device_, &set_allocate, sets.data()),
          "vkAllocateDescriptorSets");
    edram_set_ = sets[0];
    destination_set_ = sets[1];
  }

  void update_descriptors() {
    VkDescriptorBufferInfo edram_info{edram_.buffer, 0, kEdramSize};
    VkDescriptorBufferInfo destination_info{destination_.buffer,
                                             destination_guard_,
                                             kTiledExtent};
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (auto &write : writes) {
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    writes[0].dstSet = edram_set_;
    writes[0].dstBinding = 0;
    writes[0].pBufferInfo = &edram_info;
    writes[1].dstSet = destination_set_;
    writes[1].dstBinding = 0;
    writes[1].pBufferInfo = &destination_info;
    vkUpdateDescriptorSets(device_, writes.size(), writes.data(), 0, nullptr);
  }

  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_ = VK_NULL_HANDLE;
  VkPhysicalDeviceMemoryProperties memory_properties_{};
  std::uint32_t queue_family_ = 0;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkCommandPool command_pool_ = VK_NULL_HANDLE;
  VkShaderModule shader_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout edram_layout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout destination_layout_ = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
  VkDescriptorSet edram_set_ = VK_NULL_HANDLE;
  VkDescriptorSet destination_set_ = VK_NULL_HANDLE;
  Buffer edram_{};
  Buffer destination_{};
  VkDeviceSize destination_guard_ = 0;
};

} // namespace

int main() {
  try {
    constexpr auto constants = reached_constants();
    static_assert(constants == std::array<std::uint32_t, 5>{
                                   0x00000010U, 0x00091400U, 0x01000300U,
                                   0x00005C28U, 0x00000000U});
    ResolveDigests first_black;
    ResolveDigests first_asymmetric;
    {
      VulkanResolveHarness harness;
      first_black = harness.dispatch_reached(
          {0U, 0U, 0U, 0U}, {0U, 0U, 0U, 0U});
      first_asymmetric = harness.dispatch_reached(
          {0x11U, 0x22U, 0x33U, 0x44U}, {0x33U, 0x22U, 0x11U, 0x44U});
    }
    if (first_black.tiled != kExpectedTiledBlackSha ||
        first_black.linear != kExpectedLinearBlackSha) {
      throw std::runtime_error("Vulkan black resolve digest mismatch: tiled=" +
                               first_black.tiled + " linear=" +
                               first_black.linear);
    }
    if (first_asymmetric.tiled != kExpectedTiledAsymmetricSha ||
        first_asymmetric.linear != kExpectedLinearAsymmetricSha) {
      throw std::runtime_error(
          "Vulkan asymmetric resolve digest mismatch: tiled=" +
          first_asymmetric.tiled + " linear=" + first_asymmetric.linear);
    }
    ResolveDigests second_black;
    ResolveDigests second_asymmetric;
    {
      VulkanResolveHarness harness;
      second_black = harness.dispatch_reached(
          {0U, 0U, 0U, 0U}, {0U, 0U, 0U, 0U});
      second_asymmetric = harness.dispatch_reached(
          {0x11U, 0x22U, 0x33U, 0x44U}, {0x33U, 0x22U, 0x11U, 0x44U});
    }
    if (first_black.tiled != second_black.tiled ||
        first_black.linear != second_black.linear ||
        first_asymmetric.tiled != second_asymmetric.tiled ||
        first_asymmetric.linear != second_asymmetric.linear) {
      throw std::runtime_error("fresh Vulkan resolve digests differ");
    }
    std::cout << "ac6-demo-vulkan-resolve-tests: black_tiled="
              << first_black.tiled << " black_linear=" << first_black.linear
              << " asymmetric_tiled=" << first_asymmetric.tiled
              << " asymmetric_linear=" << first_asymmetric.linear
              << " rexglue=" << AC6_DEMO_REXGLUE_COMMIT << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "ac6-demo-vulkan-resolve-tests: " << error.what() << '\n';
    return 1;
  }
}
