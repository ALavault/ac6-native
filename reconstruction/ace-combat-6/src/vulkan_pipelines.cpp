#include "vulkan_backend_internal.h"

#include <limits>

namespace ac6 {
namespace {

[[nodiscard]] VkShaderModule create_shader_module(
    VulkanBackendState& state,
    const std::span<const std::uint32_t> code) noexcept {
  if (code.empty() || code.front() != 0x07230203U ||
      code.size_bytes() > std::numeric_limits<std::uint32_t>::max()) {
    return VK_NULL_HANDLE;
  }
  const VkShaderModuleCreateInfo module_info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .codeSize = code.size_bytes(),
      .pCode = code.data(),
  };
  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(state.device, &module_info, nullptr, &module) !=
      VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }
  return module;
}

}  // namespace

VulkanPipelineHandle VulkanBackend::create_pipeline(
    const VulkanRenderTargetHandle target,
    const std::span<const std::uint32_t> vertex_spirv,
    const std::span<const std::uint32_t> fragment_spirv,
    const VulkanPipelineState state) noexcept {
  return create_pipeline_impl(target, vertex_spirv, fragment_spirv, state,
                              false, false);
}

VulkanPipelineHandle VulkanBackend::create_textured_pipeline(
    const VulkanRenderTargetHandle target,
    const std::span<const std::uint32_t> vertex_spirv,
    const std::span<const std::uint32_t> fragment_spirv,
    const VulkanPipelineState state) noexcept {
  if (!state_->caps.sampled_rgba8_unorm ||
      !ensure_vulkan_texture_descriptors(*state_)) {
    return {};
  }
  return create_pipeline_impl(target, vertex_spirv, fragment_spirv, state, true,
                              false);
}

VulkanPipelineHandle VulkanBackend::create_clip_textured_pipeline(
    const VulkanRenderTargetHandle target,
    const std::span<const std::uint32_t> vertex_spirv,
    const std::span<const std::uint32_t> fragment_spirv,
    const VulkanPipelineState state) noexcept {
  if (!state_->caps.sampled_rgba8_unorm ||
      !ensure_vulkan_texture_descriptors(*state_)) {
    return {};
  }
  return create_pipeline_impl(target, vertex_spirv, fragment_spirv, state, true,
                              true);
}

VulkanPipelineHandle VulkanBackend::create_pipeline_impl(
    const VulkanRenderTargetHandle target_handle,
    const std::span<const std::uint32_t> vertex_spirv,
    const std::span<const std::uint32_t> fragment_spirv,
    const VulkanPipelineState pipeline_state, const bool textured,
    const bool clip_space) noexcept {
  const auto target = state_->targets.find(target_handle.value);
  if (target == state_->targets.end() ||
      (pipeline_state.depth_test && !target->second.with_depth) ||
      (pipeline_state.depth_write && !pipeline_state.depth_test)) {
    return {};
  }
  const VkShaderModule vertex_module = create_shader_module(*state_, vertex_spirv);
  const VkShaderModule fragment_module = create_shader_module(*state_, fragment_spirv);
  if (vertex_module == VK_NULL_HANDLE || fragment_module == VK_NULL_HANDLE) {
    if (vertex_module != VK_NULL_HANDLE) {
      vkDestroyShaderModule(state_->device, vertex_module, nullptr);
    }
    if (fragment_module != VK_NULL_HANDLE) {
      vkDestroyShaderModule(state_->device, fragment_module, nullptr);
    }
    return {};
  }
  const VkPipelineShaderStageCreateInfo stages[]{
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .pNext = nullptr,
       .flags = 0,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vertex_module,
       .pName = "main",
       .pSpecializationInfo = nullptr},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .pNext = nullptr,
       .flags = 0,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = fragment_module,
       .pName = "main",
       .pSpecializationInfo = nullptr},
  };
  const VkVertexInputBindingDescription binding{
      .binding = 0U,
      .stride = static_cast<std::uint32_t>(
          clip_space ? sizeof(VulkanClipTexturedVertex)
                     : textured ? sizeof(VulkanTexturedVertex)
                                : sizeof(VulkanVertex)),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
  const VkVertexInputAttributeDescription attributes[]{
      {.location = 0U,
       .binding = 0U,
       .format = clip_space ? VK_FORMAT_R32G32B32A32_SFLOAT
                            : VK_FORMAT_R32G32_SFLOAT,
       .offset = 0U},
      {.location = 1U,
       .binding = 0U,
       .format = VK_FORMAT_R32G32_SFLOAT,
       .offset = static_cast<std::uint32_t>(
           sizeof(float) * (clip_space ? 4U : 2U))},
  };
  const VkPipelineVertexInputStateCreateInfo vertex_input{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .vertexBindingDescriptionCount = 1U,
      .pVertexBindingDescriptions = &binding,
      .vertexAttributeDescriptionCount = textured ? 2U : 1U,
      .pVertexAttributeDescriptions = attributes,
  };
  const VkPipelineInputAssemblyStateCreateInfo input_assembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };
  const VkViewport viewport{0.0F, 0.0F,
                            static_cast<float>(target->second.width),
                            static_cast<float>(target->second.height),
                            0.0F, 1.0F};
  const VkRect2D scissor{{0, 0}, {target->second.width, target->second.height}};
  const VkPipelineViewportStateCreateInfo viewport_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .viewportCount = 1U,
      .pViewports = &viewport,
      .scissorCount = 1U,
      .pScissors = &scissor,
  };
  const VkPipelineRasterizationStateCreateInfo rasterization{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0F,
      .depthBiasClamp = 0.0F,
      .depthBiasSlopeFactor = 0.0F,
      .lineWidth = 1.0F,
  };
  const VkPipelineMultisampleStateCreateInfo multisample{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0F,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };
  const VkPipelineDepthStencilStateCreateInfo depth_stencil{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .depthTestEnable = pipeline_state.depth_test ? VK_TRUE : VK_FALSE,
      .depthWriteEnable = pipeline_state.depth_write ? VK_TRUE : VK_FALSE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front = {},
      .back = {},
      .minDepthBounds = 0.0F,
      .maxDepthBounds = 1.0F,
  };
  const VkPipelineColorBlendAttachmentState blend_attachment{
      .blendEnable = pipeline_state.alpha_blend ? VK_TRUE : VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };
  const VkPipelineColorBlendStateCreateInfo color_blend{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1U,
      .pAttachments = &blend_attachment,
      .blendConstants = {0.0F, 0.0F, 0.0F, 0.0F},
  };
  const VkPipelineLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount = textured ? 1U : 0U,
      .pSetLayouts = textured ? &state_->texture_descriptor_set_layout : nullptr,
      .pushConstantRangeCount = 0U,
      .pPushConstantRanges = nullptr,
  };
  VulkanPipelineResource resource;
  resource.render_pass = target->second.render_pass;
  resource.state = pipeline_state;
  resource.textured = textured;
  resource.clip_space = clip_space;
  if (vkCreatePipelineLayout(state_->device, &layout_info, nullptr,
                             &resource.layout) != VK_SUCCESS) {
    vkDestroyShaderModule(state_->device, vertex_module, nullptr);
    vkDestroyShaderModule(state_->device, fragment_module, nullptr);
    return {};
  }
  const VkGraphicsPipelineCreateInfo pipeline_info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stageCount = 2U,
      .pStages = stages,
      .pVertexInputState = &vertex_input,
      .pInputAssemblyState = &input_assembly,
      .pTessellationState = nullptr,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterization,
      .pMultisampleState = &multisample,
      .pDepthStencilState = target->second.with_depth ? &depth_stencil : nullptr,
      .pColorBlendState = &color_blend,
      .pDynamicState = nullptr,
      .layout = resource.layout,
      .renderPass = target->second.render_pass,
      .subpass = 0U,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };
  const VkResult created = vkCreateGraphicsPipelines(
      state_->device, VK_NULL_HANDLE, 1U, &pipeline_info, nullptr,
      &resource.pipeline);
  vkDestroyShaderModule(state_->device, vertex_module, nullptr);
  vkDestroyShaderModule(state_->device, fragment_module, nullptr);
  if (created != VK_SUCCESS) {
    vkDestroyPipelineLayout(state_->device, resource.layout, nullptr);
    return {};
  }
  const std::uint64_t handle = state_->next_handle++;
  state_->pipelines.emplace(handle, resource);
  return {handle};
}

void VulkanBackend::release_pipeline(
    const VulkanPipelineHandle pipeline) noexcept {
  const auto found = state_->pipelines.find(pipeline.value);
  if (found == state_->pipelines.end()) return;
  static_cast<void>(vkDeviceWaitIdle(state_->device));
  vkDestroyPipeline(state_->device, found->second.pipeline, nullptr);
  vkDestroyPipelineLayout(state_->device, found->second.layout, nullptr);
  state_->pipelines.erase(found);
}

bool VulkanBackend::has_pipeline(
    const VulkanPipelineHandle pipeline) const noexcept {
  return pipeline && state_->pipelines.contains(pipeline.value);
}

}  // namespace ac6
