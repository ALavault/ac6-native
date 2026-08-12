#include "vulkan_backend_internal.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace ac6 {
namespace {

[[nodiscard]] bool create_image(VulkanBackendState& state,
                                std::uint32_t width, std::uint32_t height,
                                VkFormat format, VkImageUsageFlags usage,
                                VkImage& image,
                                VkDeviceMemory& memory) noexcept {
  const VkImageCreateInfo image_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = {width, height, 1U},
      .mipLevels = 1U,
      .arrayLayers = 1U,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0U,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  if (vkCreateImage(state.device, &image_info, nullptr, &image) != VK_SUCCESS) {
    return false;
  }
  VkMemoryRequirements requirements{};
  vkGetImageMemoryRequirements(state.device, image, &requirements);
  const auto type = find_vulkan_memory_type(
      state, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (!type) return false;
  const VkMemoryAllocateInfo allocation{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = requirements.size,
      .memoryTypeIndex = *type,
  };
  if (vkAllocateMemory(state.device, &allocation, nullptr, &memory) != VK_SUCCESS) {
    return false;
  }
  return vkBindImageMemory(state.device, image, memory, 0U) == VK_SUCCESS;
}

[[nodiscard]] bool create_image_view(VulkanBackendState& state, VkImage image,
                                     VkFormat format,
                                     VkImageAspectFlags aspect,
                                     VkImageView& view) noexcept {
  const VkImageViewCreateInfo view_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                     VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
      .subresourceRange = {aspect, 0U, 1U, 0U, 1U},
  };
  return vkCreateImageView(state.device, &view_info, nullptr, &view) == VK_SUCCESS;
}

void destroy_target(VulkanBackendState& state,
                    VulkanRenderTargetResource& target) noexcept {
  if (target.framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(state.device, target.framebuffer, nullptr);
  }
  if (target.render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(state.device, target.render_pass, nullptr);
  }
  if (target.depth_view != VK_NULL_HANDLE) {
    vkDestroyImageView(state.device, target.depth_view, nullptr);
  }
  if (target.depth_image != VK_NULL_HANDLE) {
    vkDestroyImage(state.device, target.depth_image, nullptr);
  }
  if (target.depth_memory != VK_NULL_HANDLE) {
    vkFreeMemory(state.device, target.depth_memory, nullptr);
  }
  if (target.color_view != VK_NULL_HANDLE) {
    vkDestroyImageView(state.device, target.color_view, nullptr);
  }
  if (target.color_image != VK_NULL_HANDLE) {
    vkDestroyImage(state.device, target.color_image, nullptr);
  }
  if (target.color_memory != VK_NULL_HANDLE) {
    vkFreeMemory(state.device, target.color_memory, nullptr);
  }
  target = {};
}

[[nodiscard]] bool create_render_pass(VulkanBackendState& state,
                                      VulkanRenderTargetResource& target) noexcept {
  const VkAttachmentDescription color{
      .flags = 0,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };
  const VkAttachmentDescription depth{
      .flags = 0,
      .format = VK_FORMAT_D32_SFLOAT,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };
  const VkAttachmentDescription attachments[]{color, depth};
  const VkAttachmentReference color_reference{
      0U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  const VkAttachmentReference depth_reference{
      1U, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  const VkSubpassDescription subpass{
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0U,
      .pInputAttachments = nullptr,
      .colorAttachmentCount = 1U,
      .pColorAttachments = &color_reference,
      .pResolveAttachments = nullptr,
      .pDepthStencilAttachment = target.with_depth ? &depth_reference : nullptr,
      .preserveAttachmentCount = 0U,
      .pPreserveAttachments = nullptr,
  };
  const VkSubpassDependency dependency{
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0U,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dependencyFlags = 0,
  };
  const VkRenderPassCreateInfo pass_info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .attachmentCount = target.with_depth ? 2U : 1U,
      .pAttachments = attachments,
      .subpassCount = 1U,
      .pSubpasses = &subpass,
      .dependencyCount = 1U,
      .pDependencies = &dependency,
  };
  return vkCreateRenderPass(state.device, &pass_info, nullptr,
                            &target.render_pass) == VK_SUCCESS;
}

}  // namespace

std::optional<std::uint32_t> find_vulkan_memory_type(
    const VulkanBackendState& state, const std::uint32_t type_bits,
    const VkMemoryPropertyFlags required) noexcept {
  for (std::uint32_t index = 0U;
       index < state.memory_properties.memoryTypeCount; ++index) {
    if ((type_bits & (1U << index)) != 0U &&
        (state.memory_properties.memoryTypes[index].propertyFlags & required) ==
            required) {
      return index;
    }
  }
  return std::nullopt;
}

bool create_vulkan_buffer(VulkanBackendState& state, const VkDeviceSize size,
                          const VkBufferUsageFlags usage, VkBuffer& buffer,
                          VkDeviceMemory& memory) noexcept {
  if (size == 0U) return false;
  const VkBufferCreateInfo buffer_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0U,
      .pQueueFamilyIndices = nullptr,
  };
  if (vkCreateBuffer(state.device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
    return false;
  }
  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(state.device, buffer, &requirements);
  const auto type = find_vulkan_memory_type(
      state, requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (!type) return false;
  const VkMemoryAllocateInfo allocation{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = requirements.size,
      .memoryTypeIndex = *type,
  };
  if (vkAllocateMemory(state.device, &allocation, nullptr, &memory) != VK_SUCCESS) {
    return false;
  }
  return vkBindBufferMemory(state.device, buffer, memory, 0U) == VK_SUCCESS;
}

bool create_vulkan_texture_image(VulkanBackendState& state,
                                 const std::uint32_t width,
                                 const std::uint32_t height, VkImage& image,
                                 VkDeviceMemory& memory) noexcept {
  if (width == 0U || height == 0U || !state.caps.sampled_rgba8_unorm) {
    return false;
  }
  return create_image(state, width, height, VK_FORMAT_R8G8B8A8_UNORM,
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT,
                      image, memory);
}

bool create_vulkan_image_view(VulkanBackendState& state, const VkImage image,
                              VkImageView& view) noexcept {
  return image != VK_NULL_HANDLE &&
         create_image_view(state, image, VK_FORMAT_R8G8B8A8_UNORM,
                           VK_IMAGE_ASPECT_COLOR_BIT, view);
}

void destroy_vulkan_texture(VulkanBackendState& state,
                            VulkanTextureResource& texture) noexcept {
  if (texture.descriptor_set != VK_NULL_HANDLE &&
      state.texture_descriptor_pool != VK_NULL_HANDLE) {
    static_cast<void>(vkFreeDescriptorSets(
        state.device, state.texture_descriptor_pool, 1U,
        &texture.descriptor_set));
  }
  if (texture.view != VK_NULL_HANDLE) {
    vkDestroyImageView(state.device, texture.view, nullptr);
  }
  if (texture.image != VK_NULL_HANDLE) {
    vkDestroyImage(state.device, texture.image, nullptr);
  }
  if (texture.memory != VK_NULL_HANDLE) {
    vkFreeMemory(state.device, texture.memory, nullptr);
  }
  texture = {};
}

bool ensure_vulkan_texture_descriptors(VulkanBackendState& state) noexcept {
  if (state.texture_sampler != VK_NULL_HANDLE &&
      state.texture_descriptor_set_layout != VK_NULL_HANDLE &&
      state.texture_descriptor_pool != VK_NULL_HANDLE) {
    return true;
  }
  const VkSamplerCreateInfo sampler_info{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0U,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias = 0.0F,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1.0F,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0F,
      .maxLod = 1.0F,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
  };
  if (vkCreateSampler(state.device, &sampler_info, nullptr,
                      &state.texture_sampler) != VK_SUCCESS) {
    return false;
  }
  const VkDescriptorSetLayoutBinding binding{
      .binding = 0U,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1U,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr,
  };
  const VkDescriptorSetLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0U,
      .bindingCount = 1U,
      .pBindings = &binding,
  };
  if (vkCreateDescriptorSetLayout(state.device, &layout_info, nullptr,
                                  &state.texture_descriptor_set_layout) !=
      VK_SUCCESS) {
    vkDestroySampler(state.device, state.texture_sampler, nullptr);
    state.texture_sampler = VK_NULL_HANDLE;
    return false;
  }
  const VkDescriptorPoolSize pool_size{
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 64U,
  };
  const VkDescriptorPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 64U,
      .poolSizeCount = 1U,
      .pPoolSizes = &pool_size,
  };
  if (vkCreateDescriptorPool(state.device, &pool_info, nullptr,
                             &state.texture_descriptor_pool) != VK_SUCCESS) {
    vkDestroyDescriptorSetLayout(state.device,
                                 state.texture_descriptor_set_layout, nullptr);
    vkDestroySampler(state.device, state.texture_sampler, nullptr);
    state.texture_descriptor_set_layout = VK_NULL_HANDLE;
    state.texture_sampler = VK_NULL_HANDLE;
    return false;
  }
  return true;
}

void destroy_vulkan_buffer(VulkanBackendState& state, VkBuffer& buffer,
                           VkDeviceMemory& memory) noexcept {
  if (buffer != VK_NULL_HANDLE) vkDestroyBuffer(state.device, buffer, nullptr);
  if (memory != VK_NULL_HANDLE) vkFreeMemory(state.device, memory, nullptr);
  buffer = VK_NULL_HANDLE;
  memory = VK_NULL_HANDLE;
}

VulkanMeshHandle VulkanBackend::create_mesh(
    const std::span<const VulkanVertex> vertices,
    const std::span<const std::uint16_t> indices) noexcept {
  if (vertices.empty() || indices.empty() || indices.size() % 3U != 0U ||
      indices.size() > std::numeric_limits<std::uint32_t>::max()) {
    return {};
  }
  for (const auto& vertex : vertices) {
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)) return {};
  }
  for (const std::uint16_t index : indices) {
    if (index >= vertices.size()) return {};
  }
  VulkanMeshResource resource;
  if (!create_vulkan_buffer(*state_, vertices.size_bytes(),
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            resource.vertex_buffer, resource.vertex_memory) ||
      !create_vulkan_buffer(*state_, indices.size_bytes(),
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            resource.index_buffer, resource.index_memory)) {
    destroy_vulkan_buffer(*state_, resource.vertex_buffer,
                          resource.vertex_memory);
    destroy_vulkan_buffer(*state_, resource.index_buffer,
                          resource.index_memory);
    return {};
  }
  void* mapped = nullptr;
  if (vkMapMemory(state_->device, resource.vertex_memory, 0U,
                  vertices.size_bytes(), 0U, &mapped) != VK_SUCCESS) {
    destroy_vulkan_buffer(*state_, resource.vertex_buffer,
                          resource.vertex_memory);
    destroy_vulkan_buffer(*state_, resource.index_buffer,
                          resource.index_memory);
    return {};
  }
  std::memcpy(mapped, vertices.data(), vertices.size_bytes());
  vkUnmapMemory(state_->device, resource.vertex_memory);
  mapped = nullptr;
  if (vkMapMemory(state_->device, resource.index_memory, 0U,
                  indices.size_bytes(), 0U, &mapped) != VK_SUCCESS) {
    destroy_vulkan_buffer(*state_, resource.vertex_buffer,
                          resource.vertex_memory);
    destroy_vulkan_buffer(*state_, resource.index_buffer,
                          resource.index_memory);
    return {};
  }
  std::memcpy(mapped, indices.data(), indices.size_bytes());
  vkUnmapMemory(state_->device, resource.index_memory);
  resource.index_count = static_cast<std::uint32_t>(indices.size());
  const std::uint64_t handle = state_->next_handle++;
  state_->meshes.emplace(handle, resource);
  return {handle};
}

VulkanTexturedMeshHandle VulkanBackend::create_textured_mesh(
    const std::span<const VulkanTexturedVertex> vertices,
    const std::span<const std::uint16_t> indices) noexcept {
  if (vertices.empty() || indices.empty() || indices.size() % 3U != 0U ||
      indices.size() > std::numeric_limits<std::uint32_t>::max()) {
    return {};
  }
  for (const auto& vertex : vertices) {
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) ||
        !std::isfinite(vertex.u) || !std::isfinite(vertex.v)) {
      return {};
    }
  }
  for (const std::uint16_t index : indices) {
    if (index >= vertices.size()) return {};
  }
  VulkanTexturedMeshResource resource;
  if (!create_vulkan_buffer(*state_, vertices.size_bytes(),
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            resource.vertex_buffer, resource.vertex_memory) ||
      !create_vulkan_buffer(*state_, indices.size_bytes(),
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            resource.index_buffer, resource.index_memory)) {
    destroy_vulkan_buffer(*state_, resource.vertex_buffer,
                          resource.vertex_memory);
    destroy_vulkan_buffer(*state_, resource.index_buffer,
                          resource.index_memory);
    return {};
  }
  void* mapped = nullptr;
  if (vkMapMemory(state_->device, resource.vertex_memory, 0U,
                  vertices.size_bytes(), 0U, &mapped) != VK_SUCCESS) {
    destroy_vulkan_buffer(*state_, resource.vertex_buffer,
                          resource.vertex_memory);
    destroy_vulkan_buffer(*state_, resource.index_buffer,
                          resource.index_memory);
    return {};
  }
  std::memcpy(mapped, vertices.data(), vertices.size_bytes());
  vkUnmapMemory(state_->device, resource.vertex_memory);
  if (vkMapMemory(state_->device, resource.index_memory, 0U,
                  indices.size_bytes(), 0U, &mapped) != VK_SUCCESS) {
    destroy_vulkan_buffer(*state_, resource.vertex_buffer,
                          resource.vertex_memory);
    destroy_vulkan_buffer(*state_, resource.index_buffer,
                          resource.index_memory);
    return {};
  }
  std::memcpy(mapped, indices.data(), indices.size_bytes());
  vkUnmapMemory(state_->device, resource.index_memory);
  resource.index_count = static_cast<std::uint32_t>(indices.size());
  const std::uint64_t handle = state_->next_handle++;
  state_->textured_meshes.emplace(handle, resource);
  return {handle};
}

void VulkanBackend::release_textured_mesh(
    const VulkanTexturedMeshHandle mesh) noexcept {
  const auto found = state_->textured_meshes.find(mesh.value);
  if (found == state_->textured_meshes.end()) return;
  static_cast<void>(vkDeviceWaitIdle(state_->device));
  destroy_vulkan_buffer(*state_, found->second.vertex_buffer,
                        found->second.vertex_memory);
  destroy_vulkan_buffer(*state_, found->second.index_buffer,
                        found->second.index_memory);
  state_->textured_meshes.erase(found);
}

bool VulkanBackend::has_textured_mesh(
    const VulkanTexturedMeshHandle mesh) const noexcept {
  return mesh && state_->textured_meshes.contains(mesh.value);
}

VulkanTextureHandle VulkanBackend::create_texture_rgba8(
    const std::uint32_t width, const std::uint32_t height,
    const std::span<const std::uint8_t> rgba8) noexcept {
  const std::uint64_t expected = static_cast<std::uint64_t>(width) * height * 4U;
  if (width == 0U || height == 0U || expected != rgba8.size() ||
      expected > std::numeric_limits<VkDeviceSize>::max() ||
      width > state_->caps.max_image_dimension_2d ||
      height > state_->caps.max_image_dimension_2d ||
      !state_->caps.sampled_rgba8_unorm || !ensure_vulkan_texture_descriptors(*state_)) {
    return {};
  }
  VulkanTextureResource resource;
  resource.width = width;
  resource.height = height;
  if (!create_vulkan_texture_image(*state_, width, height, resource.image,
                                   resource.memory) ||
      !create_vulkan_image_view(*state_, resource.image, resource.view)) {
    destroy_vulkan_texture(*state_, resource);
    return {};
  }
  VkBuffer staging_buffer = VK_NULL_HANDLE;
  VkDeviceMemory staging_memory = VK_NULL_HANDLE;
  if (!create_vulkan_buffer(*state_, static_cast<VkDeviceSize>(expected),
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_buffer,
                            staging_memory)) {
    destroy_vulkan_texture(*state_, resource);
    return {};
  }
  void* mapped = nullptr;
  if (vkMapMemory(state_->device, staging_memory, 0U,
                  static_cast<VkDeviceSize>(expected), 0U, &mapped) !=
      VK_SUCCESS) {
    destroy_vulkan_buffer(*state_, staging_buffer, staging_memory);
    destroy_vulkan_texture(*state_, resource);
    return {};
  }
  std::memcpy(mapped, rgba8.data(), rgba8.size());
  vkUnmapMemory(state_->device, staging_memory);
  const bool submitted = submit_vulkan_commands(
      *state_, [&](const VkCommandBuffer commands) {
        record_texture_transition(commands, resource.image,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        const VkBufferImageCopy copy{
            .bufferOffset = 0U,
            .bufferRowLength = 0U,
            .bufferImageHeight = 0U,
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 0U, 1U},
            .imageOffset = {0, 0, 0},
            .imageExtent = {width, height, 1U},
        };
        vkCmdCopyBufferToImage(commands, staging_buffer, resource.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U,
                               &copy);
        record_texture_transition(commands, resource.image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      });
  destroy_vulkan_buffer(*state_, staging_buffer, staging_memory);
  if (!submitted) {
    destroy_vulkan_texture(*state_, resource);
    return {};
  }
  const VkDescriptorSetAllocateInfo allocate_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = nullptr,
      .descriptorPool = state_->texture_descriptor_pool,
      .descriptorSetCount = 1U,
      .pSetLayouts = &state_->texture_descriptor_set_layout,
  };
  if (vkAllocateDescriptorSets(state_->device, &allocate_info,
                               &resource.descriptor_set) != VK_SUCCESS) {
    destroy_vulkan_texture(*state_, resource);
    return {};
  }
  const VkDescriptorImageInfo image_info{
      .sampler = state_->texture_sampler,
      .imageView = resource.view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  const VkWriteDescriptorSet write{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = nullptr,
      .dstSet = resource.descriptor_set,
      .dstBinding = 0U,
      .dstArrayElement = 0U,
      .descriptorCount = 1U,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &image_info,
      .pBufferInfo = nullptr,
      .pTexelBufferView = nullptr,
  };
  vkUpdateDescriptorSets(state_->device, 1U, &write, 0U, nullptr);
  resource.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  const std::uint64_t handle = state_->next_handle++;
  state_->textures.emplace(handle, resource);
  return {handle};
}

void VulkanBackend::release_texture(const VulkanTextureHandle texture) noexcept {
  const auto found = state_->textures.find(texture.value);
  if (found == state_->textures.end()) return;
  static_cast<void>(vkDeviceWaitIdle(state_->device));
  destroy_vulkan_texture(*state_, found->second);
  state_->textures.erase(found);
}

bool VulkanBackend::has_texture(const VulkanTextureHandle texture) const noexcept {
  return texture && state_->textures.contains(texture.value);
}

void VulkanBackend::release_mesh(const VulkanMeshHandle mesh) noexcept {
  const auto found = state_->meshes.find(mesh.value);
  if (found == state_->meshes.end()) return;
  static_cast<void>(vkDeviceWaitIdle(state_->device));
  destroy_vulkan_buffer(*state_, found->second.vertex_buffer,
                        found->second.vertex_memory);
  destroy_vulkan_buffer(*state_, found->second.index_buffer,
                        found->second.index_memory);
  state_->meshes.erase(found);
}

bool VulkanBackend::has_mesh(const VulkanMeshHandle mesh) const noexcept {
  return mesh && state_->meshes.contains(mesh.value);
}

VulkanRenderTargetHandle VulkanBackend::create_render_target(
    const std::uint32_t width, const std::uint32_t height,
    const bool with_depth) noexcept {
  if (width == 0U || height == 0U ||
      width > state_->caps.max_image_dimension_2d ||
      height > state_->caps.max_image_dimension_2d ||
      (with_depth && !state_->caps.depth_d32)) {
    return {};
  }
  VulkanRenderTargetResource resource;
  resource.width = width;
  resource.height = height;
  resource.with_depth = with_depth;
  const bool color_ready = create_image(
      *state_, width, height, VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
          VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      resource.color_image, resource.color_memory);
  if (!color_ready ||
      !create_image_view(*state_, resource.color_image,
                         VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
                         resource.color_view)) {
    destroy_target(*state_, resource);
    return {};
  }
  if (with_depth &&
      (!create_image(*state_, width, height, VK_FORMAT_D32_SFLOAT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                     resource.depth_image, resource.depth_memory) ||
       !create_image_view(*state_, resource.depth_image, VK_FORMAT_D32_SFLOAT,
                          VK_IMAGE_ASPECT_DEPTH_BIT, resource.depth_view))) {
    destroy_target(*state_, resource);
    return {};
  }
  if (!create_render_pass(*state_, resource)) {
    destroy_target(*state_, resource);
    return {};
  }
  const VkImageView attachments[]{resource.color_view, resource.depth_view};
  const VkFramebufferCreateInfo framebuffer_info{
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .renderPass = resource.render_pass,
      .attachmentCount = with_depth ? 2U : 1U,
      .pAttachments = attachments,
      .width = width,
      .height = height,
      .layers = 1U,
  };
  if (vkCreateFramebuffer(state_->device, &framebuffer_info, nullptr,
                          &resource.framebuffer) != VK_SUCCESS) {
    destroy_target(*state_, resource);
    return {};
  }
  const std::uint64_t handle = state_->next_handle++;
  state_->targets.emplace(handle, resource);
  return {handle};
}

void VulkanBackend::release_render_target(
    const VulkanRenderTargetHandle target) noexcept {
  const auto found = state_->targets.find(target.value);
  if (found == state_->targets.end()) return;
  static_cast<void>(vkDeviceWaitIdle(state_->device));
  for (auto pipeline = state_->pipelines.begin();
       pipeline != state_->pipelines.end();) {
    if (pipeline->second.render_pass != found->second.render_pass) {
      ++pipeline;
      continue;
    }
    vkDestroyPipeline(state_->device, pipeline->second.pipeline, nullptr);
    vkDestroyPipelineLayout(state_->device, pipeline->second.layout, nullptr);
    pipeline = state_->pipelines.erase(pipeline);
  }
  destroy_target(*state_, found->second);
  state_->targets.erase(found);
}

bool VulkanBackend::has_render_target(
    const VulkanRenderTargetHandle target) const noexcept {
  return target && state_->targets.contains(target.value);
}

}  // namespace ac6
