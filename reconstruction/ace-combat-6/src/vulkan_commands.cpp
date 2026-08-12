#include "vulkan_backend_internal.h"

#include <cmath>

namespace ac6 {
namespace {

struct BarrierScope {
  VkPipelineStageFlags stage{};
  VkAccessFlags access{};
};

[[nodiscard]] BarrierScope color_scope(const VkImageLayout layout) noexcept {
  switch (layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT};
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
    default:
      return {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0U};
  }
}

[[nodiscard]] BarrierScope depth_scope(const VkImageLayout layout) noexcept {
  switch (layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
    default:
      return {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0U};
  }
}

}  // namespace

bool submit_vulkan_commands(
    VulkanBackendState& state,
    const std::function<void(VkCommandBuffer)>& record) noexcept {
  const VkCommandBufferAllocateInfo allocation{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = state.command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1U,
  };
  VkCommandBuffer commands = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(state.device, &allocation, &commands) !=
      VK_SUCCESS) {
    return false;
  }
  const VkCommandBufferBeginInfo begin{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
  };
  bool success = vkBeginCommandBuffer(commands, &begin) == VK_SUCCESS;
  if (success) {
    try {
      record(commands);
    } catch (...) {
      success = false;
    }
  }
  if (success) success = vkEndCommandBuffer(commands) == VK_SUCCESS;
  if (success) {
    const VkSubmitInfo submission{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0U,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1U,
        .pCommandBuffers = &commands,
        .signalSemaphoreCount = 0U,
        .pSignalSemaphores = nullptr,
    };
    success = vkQueueSubmit(state.queue, 1U, &submission, VK_NULL_HANDLE) ==
              VK_SUCCESS;
  }
  if (success) success = vkQueueWaitIdle(state.queue) == VK_SUCCESS;
  vkFreeCommandBuffers(state.device, state.command_pool, 1U, &commands);
  return success;
}

void record_color_transition(const VkCommandBuffer commands, const VkImage image,
                             const VkImageLayout old_layout,
                             const VkImageLayout new_layout) noexcept {
  if (old_layout == new_layout) return;
  const BarrierScope source = color_scope(old_layout);
  const BarrierScope destination = color_scope(new_layout);
  const VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = source.access,
      .dstAccessMask = destination.access,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, 0U, 1U},
  };
  vkCmdPipelineBarrier(commands, source.stage, destination.stage, 0U, 0U,
                       nullptr, 0U, nullptr, 1U, &barrier);
}

void record_depth_transition(const VkCommandBuffer commands, const VkImage image,
                             const VkImageLayout old_layout,
                             const VkImageLayout new_layout) noexcept {
  if (old_layout == new_layout) return;
  const BarrierScope source = depth_scope(old_layout);
  const BarrierScope destination = depth_scope(new_layout);
  const VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = source.access,
      .dstAccessMask = destination.access,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0U, 1U, 0U, 1U},
  };
  vkCmdPipelineBarrier(commands, source.stage, destination.stage, 0U, 0U,
                       nullptr, 0U, nullptr, 1U, &barrier);
}

void record_texture_transition(const VkCommandBuffer commands, const VkImage image,
                               const VkImageLayout old_layout,
                               const VkImageLayout new_layout) noexcept {
  if (old_layout == new_layout) return;
  VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  VkAccessFlags source_access = 0U;
  VkAccessFlags destination_access = VK_ACCESS_SHADER_READ_BIT;
  if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    source_access = VK_ACCESS_TRANSFER_WRITE_BIT;
  }
  if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destination_access = VK_ACCESS_TRANSFER_WRITE_BIT;
  }
  const VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = source_access,
      .dstAccessMask = destination_access,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, 0U, 1U},
  };
  vkCmdPipelineBarrier(commands, source_stage, destination_stage, 0U, 0U,
                       nullptr, 0U, nullptr, 1U, &barrier);
}

bool VulkanBackend::clear_render_target(
    const VulkanRenderTargetHandle target_handle,
    const std::array<float, 4> color, const float depth) noexcept {
  const auto found = state_->targets.find(target_handle.value);
  if (found == state_->targets.end() || !std::isfinite(depth) || depth < 0.0F ||
      depth > 1.0F) {
    return false;
  }
  for (const float channel : color) {
    if (!std::isfinite(channel)) return false;
  }
  VulkanRenderTargetResource& target = found->second;
  const VkImageLayout old_color_layout = target.color_layout;
  const VkImageLayout old_depth_layout = target.depth_layout;
  const bool submitted = submit_vulkan_commands(
      *state_, [&](const VkCommandBuffer commands) {
        record_color_transition(commands, target.color_image, old_color_layout,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        const VkClearColorValue clear_color{{color[0], color[1], color[2], color[3]}};
        const VkImageSubresourceRange color_range{
            VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, 0U, 1U};
        vkCmdClearColorImage(commands, target.color_image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color,
                             1U, &color_range);
        record_color_transition(commands, target.color_image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        if (target.with_depth) {
          record_depth_transition(commands, target.depth_image, old_depth_layout,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
          const VkClearDepthStencilValue clear_depth{depth, 0U};
          const VkImageSubresourceRange depth_range{
              VK_IMAGE_ASPECT_DEPTH_BIT, 0U, 1U, 0U, 1U};
          vkCmdClearDepthStencilImage(commands, target.depth_image,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      &clear_depth, 1U, &depth_range);
          record_depth_transition(
              commands, target.depth_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }
      });
  if (submitted) {
    target.color_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (target.with_depth) {
      target.depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
  }
  return submitted;
}

bool VulkanBackend::draw_indexed(
    const VulkanRenderTargetHandle target_handle,
    const VulkanPipelineHandle pipeline_handle,
    const VulkanMeshHandle mesh_handle) noexcept {
  const auto target = state_->targets.find(target_handle.value);
  const auto pipeline = state_->pipelines.find(pipeline_handle.value);
  const auto mesh = state_->meshes.find(mesh_handle.value);
  if (target == state_->targets.end() || pipeline == state_->pipelines.end() ||
      mesh == state_->meshes.end() ||
      pipeline->second.textured ||
      pipeline->second.render_pass != target->second.render_pass ||
      target->second.color_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
      (pipeline->second.state.depth_test &&
       target->second.depth_layout !=
           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)) {
    return false;
  }
  return submit_vulkan_commands(*state_, [&](const VkCommandBuffer commands) {
    const VkRenderPassBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = target->second.render_pass,
        .framebuffer = target->second.framebuffer,
        .renderArea = {{0, 0}, {target->second.width, target->second.height}},
        .clearValueCount = 0U,
        .pClearValues = nullptr,
    };
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->second.pipeline);
    const VkDeviceSize offset = 0U;
    vkCmdBindVertexBuffers(commands, 0U, 1U, &mesh->second.vertex_buffer,
                           &offset);
    vkCmdBindIndexBuffer(commands, mesh->second.index_buffer, 0U,
                         VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(commands, mesh->second.index_count, 1U, 0U, 0, 0U);
    vkCmdEndRenderPass(commands);
  });
}

bool VulkanBackend::draw_textured_indexed(
    const VulkanRenderTargetHandle target_handle,
    const VulkanPipelineHandle pipeline_handle,
    const VulkanTexturedMeshHandle mesh_handle,
    const VulkanTextureHandle texture_handle) noexcept {
  const auto target = state_->targets.find(target_handle.value);
  const auto pipeline = state_->pipelines.find(pipeline_handle.value);
  const auto mesh = state_->textured_meshes.find(mesh_handle.value);
  const auto texture = state_->textures.find(texture_handle.value);
  if (target == state_->targets.end() || pipeline == state_->pipelines.end() ||
      mesh == state_->textured_meshes.end() ||
      texture == state_->textures.end() || !pipeline->second.textured ||
      pipeline->second.clip_space || pipeline->second.world_space ||
      pipeline->second.render_pass != target->second.render_pass ||
      target->second.color_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
      texture->second.layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
      texture->second.descriptor_set == VK_NULL_HANDLE ||
      (pipeline->second.state.depth_test &&
       target->second.depth_layout !=
           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)) {
    return false;
  }
  return submit_vulkan_commands(*state_, [&](const VkCommandBuffer commands) {
    const VkRenderPassBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = target->second.render_pass,
        .framebuffer = target->second.framebuffer,
        .renderArea = {{0, 0}, {target->second.width, target->second.height}},
        .clearValueCount = 0U,
        .pClearValues = nullptr,
    };
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->second.pipeline);
    const VkDeviceSize offset = 0U;
    vkCmdBindVertexBuffers(commands, 0U, 1U, &mesh->second.vertex_buffer,
                           &offset);
    vkCmdBindIndexBuffer(commands, mesh->second.index_buffer, 0U,
                         VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->second.layout, 0U, 1U,
                            &texture->second.descriptor_set, 0U, nullptr);
    vkCmdDrawIndexed(commands, mesh->second.index_count, 1U, 0U, 0, 0U);
    vkCmdEndRenderPass(commands);
  });
}

bool VulkanBackend::draw_clip_textured_indexed(
    const VulkanRenderTargetHandle target_handle,
    const VulkanPipelineHandle pipeline_handle,
    const VulkanClipTexturedMeshHandle mesh_handle,
    const VulkanTextureHandle texture_handle) noexcept {
  const auto target = state_->targets.find(target_handle.value);
  const auto pipeline = state_->pipelines.find(pipeline_handle.value);
  const auto mesh = state_->clip_textured_meshes.find(mesh_handle.value);
  const auto texture = state_->textures.find(texture_handle.value);
  if (target == state_->targets.end() || pipeline == state_->pipelines.end() ||
      mesh == state_->clip_textured_meshes.end() ||
      texture == state_->textures.end() || !pipeline->second.textured ||
      !pipeline->second.clip_space || pipeline->second.world_space ||
      pipeline->second.render_pass != target->second.render_pass ||
      target->second.color_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
      texture->second.layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
      texture->second.descriptor_set == VK_NULL_HANDLE ||
      (pipeline->second.state.depth_test &&
       target->second.depth_layout !=
           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)) {
    return false;
  }
  return submit_vulkan_commands(*state_, [&](const VkCommandBuffer commands) {
    const VkRenderPassBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = target->second.render_pass,
        .framebuffer = target->second.framebuffer,
        .renderArea = {{0, 0}, {target->second.width, target->second.height}},
        .clearValueCount = 0U,
        .pClearValues = nullptr,
    };
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->second.pipeline);
    const VkDeviceSize offset = 0U;
    vkCmdBindVertexBuffers(commands, 0U, 1U, &mesh->second.vertex_buffer,
                           &offset);
    vkCmdBindIndexBuffer(commands, mesh->second.index_buffer, 0U,
                         VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->second.layout, 0U, 1U,
                            &texture->second.descriptor_set, 0U, nullptr);
    vkCmdDrawIndexed(commands, mesh->second.index_count, 1U, 0U, 0, 0U);
    vkCmdEndRenderPass(commands);
  });
}

bool VulkanBackend::draw_world_textured_indexed(
    const VulkanRenderTargetHandle target_handle,
    const VulkanPipelineHandle pipeline_handle,
    const VulkanWorldTexturedMeshHandle mesh_handle,
    const VulkanTextureHandle texture_handle,
    const std::array<float, 16>& object_to_clip) noexcept {
  for (const float value : object_to_clip) {
    if (!std::isfinite(value)) return false;
  }
  const auto target = state_->targets.find(target_handle.value);
  const auto pipeline = state_->pipelines.find(pipeline_handle.value);
  const auto mesh = state_->world_textured_meshes.find(mesh_handle.value);
  const auto texture = state_->textures.find(texture_handle.value);
  if (target == state_->targets.end() || pipeline == state_->pipelines.end() ||
      mesh == state_->world_textured_meshes.end() ||
      texture == state_->textures.end() || !target->second.with_depth ||
      !pipeline->second.textured || pipeline->second.clip_space ||
      !pipeline->second.world_space || !pipeline->second.state.depth_test ||
      !pipeline->second.state.depth_write ||
      pipeline->second.render_pass != target->second.render_pass ||
      target->second.color_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
      target->second.depth_layout !=
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
      texture->second.layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
      texture->second.descriptor_set == VK_NULL_HANDLE) {
    return false;
  }
  return submit_vulkan_commands(*state_, [&](const VkCommandBuffer commands) {
    const VkRenderPassBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = target->second.render_pass,
        .framebuffer = target->second.framebuffer,
        .renderArea = {{0, 0}, {target->second.width, target->second.height}},
        .clearValueCount = 0U,
        .pClearValues = nullptr,
    };
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->second.pipeline);
    const VkDeviceSize offset = 0U;
    vkCmdBindVertexBuffers(commands, 0U, 1U, &mesh->second.vertex_buffer,
                           &offset);
    vkCmdBindIndexBuffer(commands, mesh->second.index_buffer, 0U,
                         VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->second.layout, 0U, 1U,
                            &texture->second.descriptor_set, 0U, nullptr);
    vkCmdPushConstants(commands, pipeline->second.layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0U,
                       sizeof(std::array<float, 16>), object_to_clip.data());
    vkCmdDrawIndexed(commands, mesh->second.index_count, 1U, 0U, 0, 0U);
    vkCmdEndRenderPass(commands);
  });
}

}  // namespace ac6
