#include "ac6demo/vulkan_shared_memory.hpp"

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include "ac6demo/hash.hpp"
#include "ac6demo/runtime_error.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <ranges>
#include <utility>
#include <vector>

namespace ac6demo {
namespace {

[[nodiscard]] std::string payload_digest(
    const std::array<std::span<const std::byte>, 5> &payloads) {
  std::size_t total = payloads.size() * sizeof(std::uint64_t);
  for (const auto payload : payloads) {
    if (payload.size() > std::numeric_limits<std::size_t>::max() - total) {
      throw RuntimeTrap("Vulkan reached constant payload extent overflow");
    }
    total += payload.size();
  }

  std::vector<std::byte> canonical;
  canonical.reserve(total);
  for (const auto payload : payloads) {
    const auto length = static_cast<std::uint64_t>(payload.size());
    for (unsigned shift = 0U; shift < 64U; shift += 8U) {
      canonical.push_back(static_cast<std::byte>(length >> shift));
    }
    canonical.insert(canonical.end(), payload.begin(), payload.end());
  }
  return Sha256::bytes(canonical);
}

[[nodiscard]] std::uint64_t saturating_add(std::uint64_t left,
                                           std::uint64_t right) noexcept {
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  return right > maximum - left ? maximum : left + right;
}

} // namespace

std::uint32_t VulkanSharedMemory::host_memory_type(VkPhysicalDevice physical,
                                                   std::uint32_t allowed) {
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(physical, &properties);
  constexpr auto required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
    if ((allowed & (1U << index)) != 0U &&
        (properties.memoryTypes[index].propertyFlags & required) == required) {
      return index;
    }
  }
  throw RuntimeTrap("Vulkan reached buffers lack coherent host-visible memory");
}

VulkanSharedMemory::HostBuffer VulkanSharedMemory::create_buffer(
    VkPhysicalDevice physical, VkDevice device, VkDeviceSize size,
    VkBufferUsageFlags usage) {
  if (size == 0U || size > 128U * 1024U * 1024U) {
    throw RuntimeTrap("Vulkan reached buffer size is invalid");
  }
  HostBuffer result{VK_NULL_HANDLE, VK_NULL_HANDLE, size};
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  info.size = size;
  info.usage = usage;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(device, &info, nullptr, &result.buffer) != VK_SUCCESS) {
    throw RuntimeTrap("Vulkan reached buffer creation failed");
  }
  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device, result.buffer, &requirements);
  VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocation.allocationSize = requirements.size;
  allocation.memoryTypeIndex =
      host_memory_type(physical, requirements.memoryTypeBits);
  if (vkAllocateMemory(device, &allocation, nullptr, &result.memory) !=
          VK_SUCCESS ||
      vkBindBufferMemory(device, result.buffer, result.memory, 0U) !=
          VK_SUCCESS) {
    vkDestroyBuffer(device, result.buffer, nullptr);
    if (result.memory != VK_NULL_HANDLE) {
      vkFreeMemory(device, result.memory, nullptr);
    }
    throw RuntimeTrap("Vulkan reached buffer allocation or bind failed");
  }
  void *mapping = nullptr;
  if (vkMapMemory(device, result.memory, 0U, result.size, 0U, &mapping) !=
      VK_SUCCESS) {
    destroy_buffer(device, result);
    throw RuntimeTrap("Vulkan reached buffer initialization mapping failed");
  }
  std::memset(mapping, 0, static_cast<std::size_t>(result.size));
  vkUnmapMemory(device, result.memory);
  return result;
}

void VulkanSharedMemory::destroy_buffer(VkDevice device,
                                        HostBuffer &buffer) noexcept {
  if (buffer.buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, buffer.buffer, nullptr);
  }
  if (buffer.memory != VK_NULL_HANDLE) {
    vkFreeMemory(device, buffer.memory, nullptr);
  }
  buffer = {};
}

void VulkanSharedMemory::destroy_constant_set(
    VkDevice device, ConstantSet &constants) noexcept {
  if (constants.pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, constants.pool, nullptr);
  }
  for (auto &buffer : constants.buffers) {
    destroy_buffer(device, buffer);
  }
  constants = {};
}

void VulkanSharedMemory::write_buffer(VkDevice device,
                                      const HostBuffer &buffer,
                                      VkDeviceSize offset,
                                      std::span<const std::byte> bytes) {
  if (offset > buffer.size || bytes.size() > buffer.size - offset) {
    throw RuntimeTrap("Vulkan reached upload exceeds its buffer");
  }
  void *mapping = nullptr;
  if (vkMapMemory(device, buffer.memory, offset, bytes.size(), 0U, &mapping) !=
      VK_SUCCESS) {
    throw RuntimeTrap("Vulkan reached buffer mapping failed");
  }
  std::memcpy(mapping, bytes.data(), bytes.size());
  vkUnmapMemory(device, buffer.memory);
}

bool VulkanSharedMemory::populate(
    VkPhysicalDevice physical, VkDevice device, VkDescriptorSetLayout layout,
    DemoSession &session, std::span<const XenosCommand> commands,
    std::uint32_t shader_loads, std::uint32_t draws, std::uint32_t presents,
    std::uint32_t translated_modules, std::uint32_t graphics_pipelines) {
  const bool rectangle = std::ranges::any_of(commands, [](const auto &command) {
    const auto *draw = std::get_if<XenosDrawCommand>(&command);
    return draw != nullptr && draw->primitive == XenosPrimitive::RectangleList;
  });
  const bool initial_profile =
      rectangle && graphics_pipelines == 2U && shader_loads == 5U &&
      draws == 26U && presents <= 1U && translated_modules == 4U;
  if (!populated() && !initial_profile) {
    return false;
  }
  if (populated() &&
      (graphics_pipelines != 2U || translated_modules != 4U ||
       shader_loads < 5U || draws < 26U)) {
    throw RuntimeTrap("Vulkan reached shared refresh profile changed");
  }

  constexpr std::uint32_t guest_begin = 0x127CA03CU;
  constexpr std::uint32_t guest_end = 0x127CA0A8U;
  constexpr VkDeviceSize segment_mask = 0x07FFFFFFU;
  const auto guest =
      session.load_guest_bytes(guest_begin, guest_end - guest_begin);
  if (guest.size() != static_cast<std::size_t>(guest_end - guest_begin)) {
    throw RuntimeTrap("Vulkan reached shared refresh extent changed");
  }
  const auto digest = Sha256::bytes(guest);

  if (populated()) {
    if (!shared_version_.needs_upload(digest)) {
      return false;
    }
    shared_version_.validate_candidate(digest);
    write_buffer(device, buffers_[2], guest_begin & segment_mask, guest);
    shared_version_.mark_uploaded(digest);
    return true;
  }

  shared_version_.validate_candidate(digest);
  std::array<HostBuffer, 4> staged{};
  VkDescriptorPool pool = VK_NULL_HANDLE;
  try {
    for (std::uint32_t index = 0; index < staged.size(); ++index) {
      staged[index] = create_buffer(
          physical, device, index == 2U ? (guest_end & segment_mask) : 4U,
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }
    write_buffer(device, staged[2], guest_begin & segment_mask, guest);
    VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4U};
    VkDescriptorPoolCreateInfo pool_info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.maxSets = 1U;
    pool_info.poolSizeCount = 1U;
    pool_info.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &pool) !=
        VK_SUCCESS) {
      throw RuntimeTrap("Vulkan reached shared descriptor pool failed");
    }
    VkDescriptorSetAllocateInfo allocate{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool = pool;
    allocate.descriptorSetCount = 1U;
    allocate.pSetLayouts = &layout;
    VkDescriptorSet descriptor = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &allocate, &descriptor) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan reached shared descriptor allocation failed");
    }
    std::array<VkDescriptorBufferInfo, 4> infos{};
    for (std::uint32_t index = 0; index < infos.size(); ++index) {
      infos[index] = {staged[index].buffer, 0U, staged[index].size};
    }
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = descriptor;
    write.dstBinding = 0U;
    write.descriptorCount = static_cast<std::uint32_t>(infos.size());
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = infos.data();
    vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
    shared_version_.mark_uploaded(digest);
    buffers_ = std::move(staged);
    descriptor_pool_ = pool;
    descriptor_set_ = descriptor;
    return true;
  } catch (...) {
    if (pool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device, pool, nullptr);
    }
    for (auto &buffer : staged) {
      destroy_buffer(device, buffer);
    }
    throw;
  }
}

bool VulkanSharedMemory::populate_constants(
    VkPhysicalDevice physical, VkDevice device, VkDescriptorSetLayout layout,
    const XenosDrawCommand &draw, const ReachedShaderSpirv &vertex,
    const ReachedShaderSpirv &pixel, std::uint32_t viewport_x_max,
    std::uint32_t viewport_y_max) {
  const auto payloads = build_reached_constant_payloads(
      draw, vertex, pixel, viewport_x_max, viewport_y_max);
  const std::array<std::span<const std::byte>, 5> payload_spans{
      payloads.system, payloads.float_vertex, payloads.float_pixel,
      payloads.bool_loop, payloads.fetch};
  const auto digest = payload_digest(payload_spans);

  auto slot = std::ranges::find_if(constants_, [&](const ConstantSet &set) {
    return set.vertex_shader == draw.vertex_shader_sha256;
  });
  if (slot != constants_.end() && !slot->version.needs_upload(digest)) {
    return false;
  }
  if (slot == constants_.end()) {
    slot = std::ranges::find_if(constants_, [](const ConstantSet &set) {
      return set.set == VK_NULL_HANDLE;
    });
  }
  if (slot == constants_.end()) {
    throw RuntimeTrap("Vulkan reached constant descriptor limit exceeded");
  }

  ConstantSet staged;
  staged.vertex_shader = draw.vertex_shader_sha256;
  staged.version = slot->version;
  try {
    for (std::uint32_t index = 0; index < staged.buffers.size(); ++index) {
      staged.buffers[index] = create_buffer(
          physical, device, payload_spans[index].size(),
          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
      write_buffer(device, staged.buffers[index], 0U, payload_spans[index]);
    }
    VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5U};
    VkDescriptorPoolCreateInfo pool_info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.maxSets = 1U;
    pool_info.poolSizeCount = 1U;
    pool_info.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &staged.pool) !=
        VK_SUCCESS) {
      throw RuntimeTrap("Vulkan reached constant descriptor pool failed");
    }
    VkDescriptorSetAllocateInfo allocate{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool = staged.pool;
    allocate.descriptorSetCount = 1U;
    allocate.pSetLayouts = &layout;
    if (vkAllocateDescriptorSets(device, &allocate, &staged.set) != VK_SUCCESS) {
      throw RuntimeTrap("Vulkan reached constant descriptor allocation failed");
    }
    std::array<VkDescriptorBufferInfo, 5> infos{};
    std::array<VkWriteDescriptorSet, 5> writes{};
    for (std::uint32_t index = 0; index < infos.size(); ++index) {
      infos[index] = {staged.buffers[index].buffer, 0U,
                      staged.buffers[index].size};
      writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[index].dstSet = staged.set;
      writes[index].dstBinding = index;
      writes[index].descriptorCount = 1U;
      writes[index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      writes[index].pBufferInfo = &infos[index];
    }
    vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(writes.size()),
                           writes.data(), 0U, nullptr);
    staged.version.mark_uploaded(digest);
  } catch (...) {
    destroy_constant_set(device, staged);
    throw;
  }

  ConstantSet previous = std::move(*slot);
  *slot = std::move(staged);
  destroy_constant_set(device, previous);
  return true;
}

std::uint32_t VulkanSharedMemory::constant_descriptor_count() const noexcept {
  return 5U * static_cast<std::uint32_t>(std::ranges::count_if(
                  constants_, [](const ConstantSet &set) {
                    return set.set != VK_NULL_HANDLE;
                  }));
}

VkDescriptorSet VulkanSharedMemory::constant_descriptor_set(
    std::string_view vertex_shader) const noexcept {
  const auto found = std::ranges::find_if(constants_, [&](const auto &set) {
    return set.vertex_shader == vertex_shader;
  });
  return found == constants_.end() ? VK_NULL_HANDLE : found->set;
}

std::uint64_t VulkanSharedMemory::constant_upload_generation(
    std::string_view vertex_shader) const noexcept {
  const auto found = std::ranges::find_if(constants_, [&](const auto &set) {
    return set.vertex_shader == vertex_shader;
  });
  return found == constants_.end() ? 0U : found->version.generation();
}

std::uint64_t VulkanSharedMemory::refresh_epoch() const noexcept {
  std::uint64_t epoch = shared_version_.generation();
  for (const auto &constants : constants_) {
    epoch = saturating_add(epoch, constants.version.generation());
  }
  return epoch;
}

void VulkanSharedMemory::cleanup(VkDevice device) noexcept {
  if (device == VK_NULL_HANDLE) {
    return;
  }
  if (descriptor_pool_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, descriptor_pool_, nullptr);
  }
  for (auto &constants : constants_) {
    destroy_constant_set(device, constants);
  }
  for (auto &buffer : buffers_) {
    destroy_buffer(device, buffer);
  }
  descriptor_pool_ = VK_NULL_HANDLE;
  descriptor_set_ = VK_NULL_HANDLE;
  shared_version_.reset();
}

} // namespace ac6demo

#endif
