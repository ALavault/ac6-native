#include "ac6demo/content.hpp"
#include "ac6demo/cli.hpp"
#include "ac6demo/emu_agent_ipc.hpp"
#include "ac6demo/frontier_report.hpp"
#include "ac6demo/guest_bridge.hpp"
#include "ac6demo/hash.hpp"
#include "ac6demo/runtime_error.hpp"
#include "ac6demo/session.hpp"
#include "ac6demo/trace.hpp"
#include "ac6demo/vulkan_shared_memory.hpp"
#include "ac6demo/vulkan_normal_draw.hpp"
#include "ac6demo/vulkan_neutral_resolve.hpp"
#include "xenos_present_join.hpp"
#include "xenos_guest_present_join.hpp"
#include "renderer_frontier_print.hpp"
#ifdef AC6_DEMO_HAVE_REXGLUE_TRANSLATOR
#include "ac6demo/rexglue_runtime_shader.hpp"
#endif
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER
#include "rexglue_rectangle_list_spirv.hpp"
#include <vulkan/vulkan.h>
#endif
namespace {
using ac6demo::GraphicsBackend;
using ac6demo::option_value;
using ac6demo::publish_new_file;
using ac6demo::read_binary_file;
using ac6demo::read_xam_movie_file;
struct RuntimeRendererStats final {
  std::uint32_t shader_loads{};
  std::uint32_t draws{};
  std::uint32_t presents{};
  std::uint32_t translated_modules{};
  std::uint32_t vulkan_modules{};
  std::uint32_t vulkan_descriptor_set_layouts{};
  std::uint32_t vulkan_pipeline_layouts{};
  std::uint32_t vulkan_graphics_pipelines{};
  std::uint32_t vulkan_shared_memory_descriptors{};
  std::uint32_t vulkan_constant_buffer_descriptors{};
  std::uint32_t vulkan_normal_draws{};
  std::uint32_t vulkan_neutral_resolves{};
  std::string normal_readback_sha256, neutral_resolve_sha256,
      guest_linear_sha256;
  bool guest_writeback{};
};
class RuntimeRendererFrontier final {
public:
  explicit RuntimeRendererFrontier(GraphicsBackend backend) {
#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER
    if (backend == GraphicsBackend::Vulkan) {
      try {
        initialize_vulkan();
      } catch (...) {
        cleanup_vulkan();
        throw;
      }
    }
#else
    if (backend == GraphicsBackend::Vulkan) {
      throw ac6demo::RuntimeTrap(
          "Vulkan renderer reached without the pinned Vulkan frontier");
    }
#endif
  }
  ~RuntimeRendererFrontier() {
#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER
    cleanup_vulkan();
#endif
  }
  RuntimeRendererFrontier(const RuntimeRendererFrontier &) = delete;
  RuntimeRendererFrontier &operator=(const RuntimeRendererFrontier &) = delete;
  void consume(ac6demo::DemoSession &session) {
    auto commands = session.consume_renderer_commands();
    if (commands.empty()) {
      return;
    }
#ifdef AC6_DEMO_HAVE_REXGLUE_TRANSLATOR
    cache_.consume(commands);
#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER
    if (device_ != VK_NULL_HANDLE) {
      create_reached_modules(session, commands);
    }
#endif
#else
    throw ac6demo::RuntimeTrap(
        "renderer commands reached without the pinned ReXGlue translator");
#endif
  }
  [[nodiscard]] RuntimeRendererStats stats() const noexcept {
#ifdef AC6_DEMO_HAVE_REXGLUE_TRANSLATOR
    const auto &stats = cache_.stats();
    RuntimeRendererStats result{};
    result.shader_loads = stats.shader_loads;
    result.draws = stats.draws;
    result.presents = stats.presents;
    result.translated_modules = stats.translated_modules;
#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER
    result.vulkan_modules = static_cast<std::uint32_t>(vulkan_modules_.size());
    result.vulkan_descriptor_set_layouts =
        shared_layout_ != VK_NULL_HANDLE && constants_layout_ != VK_NULL_HANDLE
            ? 2U
            : 0U;
    result.vulkan_pipeline_layouts = pipeline_layout_ != VK_NULL_HANDLE ? 1U : 0U;
    result.vulkan_graphics_pipelines =
        static_cast<std::uint32_t>(graphics_pipelines_.size());
    result.vulkan_shared_memory_descriptors = shared_memory_.populated() ? 4U : 0U;
    result.vulkan_constant_buffer_descriptors =
        shared_memory_.constant_descriptor_count();
    result.vulkan_normal_draws = normal_draw_.has_value() ? 1U : 0U;
    result.vulkan_neutral_resolves = neutral_resolve_.has_value() ? 1U : 0U;
    if (normal_draw_.has_value()) {
      result.normal_readback_sha256 = normal_draw_->resolved_rgba8_sha256;
    }
    if (neutral_resolve_.has_value()) {
      result.neutral_resolve_sha256 = neutral_resolve_->linear_rgba8_sha256;
      result.guest_linear_sha256 =
          neutral_resolve_->guest_linear_rgba8_sha256;
      result.guest_writeback = neutral_resolve_->guest_writeback;
    }
#else
    result.vulkan_modules = 0U;
    result.vulkan_descriptor_set_layouts = 0U;
    result.vulkan_pipeline_layouts = 0U;
    result.vulkan_graphics_pipelines = 0U;
    result.vulkan_shared_memory_descriptors = 0U;
    result.vulkan_constant_buffer_descriptors = 0U;
    result.vulkan_normal_draws = 0U;
    result.vulkan_neutral_resolves = 0U;
#endif
    return result;
#else
    return {};
#endif
  }
private:
#ifdef AC6_DEMO_HAVE_REXGLUE_TRANSLATOR
  ac6demo::ReachedShaderRuntimeCache cache_;
#endif
#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER
  void initialize_vulkan() {
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "ac6-demo-recomp-renderer-frontier";
    application.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo = &application;
    if (vkCreateInstance(&instance_info, nullptr, &instance_) != VK_SUCCESS) {
      throw ac6demo::RuntimeTrap("Vulkan renderer instance creation failed");
    }
    std::uint32_t physical_count = 0U;
    if (vkEnumeratePhysicalDevices(instance_, &physical_count, nullptr) !=
            VK_SUCCESS ||
        physical_count == 0U || physical_count > 16U) {
      throw ac6demo::RuntimeTrap("Vulkan renderer has no bounded device set");
    }
    std::vector<VkPhysicalDevice> physical_devices(physical_count);
    if (vkEnumeratePhysicalDevices(instance_, &physical_count,
                                   physical_devices.data()) != VK_SUCCESS) {
      throw ac6demo::RuntimeTrap("Vulkan renderer device enumeration failed");
    }
    physical_ = physical_devices.front();
    std::uint32_t queue_count = 0U;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_, &queue_count, nullptr);
    if (queue_count == 0U || queue_count > 64U) {
      throw ac6demo::RuntimeTrap("Vulkan renderer queue set is invalid");
    }
    std::vector<VkQueueFamilyProperties> queues(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_, &queue_count,
                                             queues.data());
    const auto queue = std::ranges::find_if(queues, [](const auto &candidate) {
      return (candidate.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U &&
             candidate.queueCount != 0U;
    });
    if (queue == queues.end()) {
      throw ac6demo::RuntimeTrap("Vulkan renderer has no graphics queue");
    }
    queue_family_ = static_cast<std::uint32_t>(queue - queues.begin());
    constexpr float priority = 1.0F;
    VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = queue_family_;
    queue_info.queueCount = 1U;
    queue_info.pQueuePriorities = &priority;
    VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.queueCreateInfoCount = 1U;
    device_info.pQueueCreateInfos = &queue_info;
    VkPhysicalDeviceScalarBlockLayoutFeatures scalar_layout{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.pNext = &scalar_layout;
    vkGetPhysicalDeviceFeatures2(physical_, &features);
    if (scalar_layout.scalarBlockLayout != VK_TRUE) {
      throw ac6demo::RuntimeTrap(
          "Vulkan renderer lacks scalar block layout for reached shaders");
    }
    if (features.features.geometryShader != VK_TRUE) {
      throw ac6demo::RuntimeTrap(
          "Vulkan renderer lacks generic rectangle geometry expansion");
    }
    features.features.geometryShader = VK_TRUE;
    // Enable precise occlusion queries only on devices that advertise them.
    // The opt-in draw probe independently fails closed when unavailable.
    if (features.features.occlusionQueryPrecise == VK_TRUE) {
      features.features.occlusionQueryPrecise = VK_TRUE;
    }
    scalar_layout.scalarBlockLayout = VK_TRUE;
    device_info.pEnabledFeatures = &features.features;
    device_info.pNext = &scalar_layout;
    if (vkCreateDevice(physical_, &device_info, nullptr, &device_) !=
        VK_SUCCESS) {
      throw ac6demo::RuntimeTrap("Vulkan renderer device creation failed");
    }
    vkGetDeviceQueue(device_, queue_family_, 0U, &queue_);
    if (queue_ == VK_NULL_HANDLE) {
      throw ac6demo::RuntimeTrap("Vulkan renderer queue is unavailable");
    }
    create_reached_layouts();
    VkShaderModuleCreateInfo geometry_info{
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    geometry_info.codeSize =
        ac6demo::generated::rexglue_rectangle_list_spirv.size() *
        sizeof(std::uint32_t);
    geometry_info.pCode =
        ac6demo::generated::rexglue_rectangle_list_spirv.data();
    if (vkCreateShaderModule(device_, &geometry_info, nullptr,
                             &rectangle_geometry_module_) != VK_SUCCESS) {
      throw ac6demo::RuntimeTrap(
          "Vulkan rejected validated rectangle geometry module");
    }
  }
  void create_reached_layouts() {
    VkDescriptorSetLayoutBinding shared{};
    shared.binding = 0U;
    shared.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    shared.descriptorCount = 4U;
    shared.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo set_info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    set_info.bindingCount = 1U;
    set_info.pBindings = &shared;
    if (vkCreateDescriptorSetLayout(device_, &set_info, nullptr,
                                    &shared_layout_) != VK_SUCCESS) {
      throw ac6demo::RuntimeTrap(
          "Vulkan reached shared-memory layout creation failed");
    }
    std::array<VkDescriptorSetLayoutBinding, 5> constants{};
    for (std::uint32_t index = 0U; index < constants.size(); ++index) {
      constants[index].binding = index;
      constants[index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      constants[index].descriptorCount = 1U;
    }
    constants[0].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    constants[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    constants[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    constants[3].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    constants[4].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    set_info.bindingCount = static_cast<std::uint32_t>(constants.size());
    set_info.pBindings = constants.data();
    if (vkCreateDescriptorSetLayout(device_, &set_info, nullptr,
                                    &constants_layout_) != VK_SUCCESS) {
      throw ac6demo::RuntimeTrap(
          "Vulkan reached constant layout creation failed");
    }
    const std::array layouts{shared_layout_, constants_layout_};
    VkPipelineLayoutCreateInfo pipeline_info{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeline_info.setLayoutCount = static_cast<std::uint32_t>(layouts.size());
    pipeline_info.pSetLayouts = layouts.data();
    if (vkCreatePipelineLayout(device_, &pipeline_info, nullptr,
                               &pipeline_layout_) != VK_SUCCESS) {
      throw ac6demo::RuntimeTrap(
          "Vulkan reached pipeline layout creation failed");
    }
  }
  [[nodiscard]] VkRenderPass create_render_pass(VkSampleCountFlagBits samples,
                                                 bool depth) {
    std::array<VkAttachmentDescription, 3> attachments{};
    attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachments[0].samples = samples;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference color{0U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_reference{
        1U, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkAttachmentReference resolve_reference{
        2U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    if (depth) {
      attachments[1].format = VK_FORMAT_D24_UNORM_S8_UINT;
      attachments[1].samples = samples;
      attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachments[1].finalLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      attachments[2].format = VK_FORMAT_R8G8B8A8_UNORM;
      attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
      attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachments[2].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1U;
    subpass.pColorAttachments = &color;
    subpass.pDepthStencilAttachment = depth ? &depth_reference : nullptr;
    subpass.pResolveAttachments = depth ? &resolve_reference : nullptr;
    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = depth ? 3U : 1U;
    info.pAttachments = attachments.data();
    info.subpassCount = 1U;
    info.pSubpasses = &subpass;
    VkRenderPass result = VK_NULL_HANDLE;
    if (vkCreateRenderPass(device_, &info, nullptr, &result) != VK_SUCCESS) {
      throw ac6demo::RuntimeTrap("Vulkan reached render-pass creation failed");
    }
    return result;
  }
  [[nodiscard]] VkPipeline create_graphics_pipeline(
      VkShaderModule vertex, VkShaderModule pixel, VkRenderPass render_pass,
      VkSampleCountFlagBits samples, bool depth) {
    std::array<VkPipelineShaderStageCreateInfo, 3> stages{};
    for (auto &stage : stages) {
      stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stage.pName = "main";
    }
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = pixel;
    stages[2].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    stages[2].module = rectangle_geometry_module_;
    VkPipelineVertexInputStateCreateInfo vertex_input{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1U;
    viewport.scissorCount = 1U;
    VkPipelineRasterizationStateCreateInfo raster{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.lineWidth = 1.0F;
    VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = samples;
    VkPipelineDepthStencilStateCreateInfo depth_stencil{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth_stencil.depthTestEnable = depth ? VK_TRUE : VK_FALSE;
    depth_stencil.depthWriteEnable = depth ? VK_TRUE : VK_FALSE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depth_stencil.stencilTestEnable = depth ? VK_TRUE : VK_FALSE;
    depth_stencil.front.failOp = VK_STENCIL_OP_KEEP;
    depth_stencil.front.passOp = VK_STENCIL_OP_REPLACE;
    depth_stencil.front.depthFailOp = VK_STENCIL_OP_KEEP;
    depth_stencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
    depth_stencil.back = depth_stencil.front;
    VkPipelineColorBlendAttachmentState color{};
    color.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1U;
    blend.pAttachments = &color;
    constexpr std::array dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE};
    VkPipelineDynamicStateCreateInfo dynamic{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
    dynamic.pDynamicStates = dynamic_states.data();
    VkGraphicsPipelineCreateInfo info{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.stageCount = static_cast<std::uint32_t>(stages.size());
    info.pStages = stages.data();
    info.pVertexInputState = &vertex_input;
    info.pInputAssemblyState = &input_assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depth_stencil;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = pipeline_layout_;
    info.renderPass = render_pass;
    VkPipeline result = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1U, &info, nullptr,
                                  &result) != VK_SUCCESS) {
      throw ac6demo::RuntimeTrap(
          "Vulkan reached graphics-pipeline creation failed");
    }
    return result;
  }
  void create_reached_pipelines(
      std::span<const ac6demo::XenosCommand> commands) {
    static constexpr std::string_view kNormalVertex =
        "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b";
    static constexpr std::string_view kCopyVertex =
        "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0";
    static constexpr std::string_view kPixel =
        "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25";
    for (const auto &command : commands) {
      const auto *draw = std::get_if<ac6demo::XenosDrawCommand>(&command);
      if (draw == nullptr ||
          draw->primitive == ac6demo::XenosPrimitive::PointList) {
        continue;
      }
      const bool normal = draw->vertex_shader_sha256 == kNormalVertex &&
                          draw->pixel_shader_sha256 == kPixel;
      const bool copy = draw->vertex_shader_sha256 == kCopyVertex &&
                        draw->pixel_shader_sha256 == kPixel;
      if ((!normal && !copy) || !draw->registers || draw->index_count != 3U ||
          draw->source != ac6demo::XenosIndexSource::AutoIndex) {
        throw ac6demo::RuntimeTrap("unqualified Vulkan rectangle pipeline");
      }
      const auto &registers = *draw->registers;
      const bool exact =
          normal
              ? registers.value(0x2000U) == 0x0A020280U &&
                    registers.value(0x2104U) == 0x0000FFFFU &&
                    registers.value(0x2180U) == 0x10010001U &&
                    registers.value(0x2200U) == 0x00008777U &&
                    registers.value(0x2201U) == 0x00010001U &&
                    registers.value(0x2208U) == 0x00000004U
              : registers.value(0x2000U) == 0x14000500U &&
                    registers.value(0x2104U) == 0x0000000FU &&
                    registers.value(0x2180U) == 0x00010002U &&
                    registers.value(0x2200U) == 0x00000000U &&
                    registers.value(0x2201U) == 0x00010001U &&
                    registers.value(0x2208U) == 0x00000006U;
      if (!exact) {
        throw ac6demo::RuntimeTrap("Vulkan rectangle register profile changed");
      }
      const std::string key(draw->vertex_shader_sha256);
      if (graphics_pipelines_.contains(key)) {
        continue;
      }
      const auto vertex = vulkan_modules_.find(draw->vertex_shader_sha256);
      const auto pixel = vulkan_modules_.find(draw->pixel_shader_sha256);
      if (vertex == vulkan_modules_.end() || pixel == vulkan_modules_.end()) {
        throw ac6demo::RuntimeTrap("Vulkan pipeline modules are unavailable");
      }
      const auto samples =
          normal ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
      VkRenderPass render_pass = create_render_pass(samples, normal);
      try {
        const VkPipeline pipeline = create_graphics_pipeline(
            vertex->second, pixel->second, render_pass, samples, normal);
        render_passes_.emplace(key, render_pass);
        graphics_pipelines_.emplace(key, pipeline);
      } catch (...) {
        vkDestroyRenderPass(device_, render_pass, nullptr);
        throw;
      }
    }
  }
  void cleanup_vulkan() noexcept {
    if (!vulkan_cleanup_safe_) {
      // A submitted queue could not be drained. No Vulkan object or device is
      // safe to destroy; process teardown owns recovery in this terminal case.
      return;
    }
    if (device_ != VK_NULL_HANDLE) {
      shared_memory_.cleanup(device_);
      for (const auto &[identity, pipeline] : graphics_pipelines_) {
        static_cast<void>(identity);
        vkDestroyPipeline(device_, pipeline, nullptr);
      }
      graphics_pipelines_.clear();
      for (const auto &[identity, render_pass] : render_passes_) {
        static_cast<void>(identity);
        vkDestroyRenderPass(device_, render_pass, nullptr);
      }
      render_passes_.clear();
      for (const auto &[identity, module] : vulkan_modules_) {
        static_cast<void>(identity);
        vkDestroyShaderModule(device_, module, nullptr);
      }
      vulkan_modules_.clear();
      if (rectangle_geometry_module_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, rectangle_geometry_module_, nullptr);
        rectangle_geometry_module_ = VK_NULL_HANDLE;
      }
      if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
      }
      if (constants_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, constants_layout_, nullptr);
        constants_layout_ = VK_NULL_HANDLE;
      }
      if (shared_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, shared_layout_, nullptr);
        shared_layout_ = VK_NULL_HANDLE;
      }
      vkDestroyDevice(device_, nullptr);
      device_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
      vkDestroyInstance(instance_, nullptr);
      instance_ = VK_NULL_HANDLE;
    }
  }
  void create_reached_modules(
      ac6demo::DemoSession &session,
      std::span<const ac6demo::XenosCommand> commands) {
    std::vector<std::string> identities;
    const auto present_command = ac6demo::single_xenos_present(commands);
    for (const auto &command : commands) {
      const auto *draw = std::get_if<ac6demo::XenosDrawCommand>(&command);
      if (draw == nullptr) {
        continue;
      }
      identities.push_back(draw->vertex_shader_sha256);
      identities.push_back(draw->pixel_shader_sha256);
    }
    std::ranges::sort(identities);
    const auto unique_end = std::ranges::unique(identities).begin();
    identities.erase(unique_end, identities.end());
    std::vector<std::pair<std::string, VkShaderModule>> staged;
    try {
      for (const auto &identity : identities) {
        if (vulkan_modules_.contains(identity)) {
          continue;
        }
        const auto *module = cache_.module(identity);
        if (module == nullptr || module->words.empty()) {
          throw ac6demo::RuntimeTrap(
              "Vulkan draw references an unavailable validated module");
        }
        VkShaderModuleCreateInfo create_info{
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        create_info.codeSize = module->words.size() * sizeof(std::uint32_t);
        create_info.pCode = module->words.data();
        VkShaderModule shader = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device_, &create_info, nullptr, &shader) !=
            VK_SUCCESS) {
          throw ac6demo::RuntimeTrap(
              "Vulkan rejected a validated reached shader module");
        }
        staged.emplace_back(identity, shader);
      }
    } catch (...) {
      for (const auto &[identity, module] : staged) {
        static_cast<void>(identity);
        vkDestroyShaderModule(device_, module, nullptr);
      }
      throw;
    }
    for (auto &[identity, module] : staged) {
      vulkan_modules_.emplace(std::move(identity), module);
    }
    create_reached_pipelines(commands);
    const auto reached = cache_.stats();
    shared_memory_.populate(
        physical_, device_, shared_layout_, session, commands,
        reached.shader_loads, reached.draws, reached.presents,
        reached.translated_modules,
        static_cast<std::uint32_t>(graphics_pipelines_.size()));
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_, &properties);
    for (const auto &command : commands) {
      const auto *draw = std::get_if<ac6demo::XenosDrawCommand>(&command);
      if (draw == nullptr ||
          draw->primitive != ac6demo::XenosPrimitive::RectangleList) {
        continue;
      }
      static constexpr std::string_view copy_vertex =
          "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0";
      static constexpr std::string_view normal_vertex =
          "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b";
      if (draw->vertex_shader_sha256 == copy_vertex) copy_draw_command_ = *draw;
      if (draw->vertex_shader_sha256 == normal_vertex) normal_draw_command_ = *draw;
      const auto *vertex = cache_.module(draw->vertex_shader_sha256);
      const auto *pixel = cache_.module(draw->pixel_shader_sha256);
      if (vertex == nullptr || pixel == nullptr) {
        throw ac6demo::RuntimeTrap(
            "Vulkan constants reference unavailable reached modules");
      }
      shared_memory_.populate_constants(
          physical_, device_, constants_layout_, *draw, *vertex, *pixel,
          properties.limits.maxViewportDimensions[0],
          properties.limits.maxViewportDimensions[1]);
    }
    static constexpr std::string_view normal_vertex =
        "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b";
    if (!normal_draw_.has_value() && shared_memory_.populated() &&
        shared_memory_.constant_descriptor_count() == 10U) {
      if (!normal_draw_command_.has_value()) {
        throw ac6demo::RuntimeTrap("Vulkan normal draw command is unavailable");
      }
      normal_draw_ = ac6demo::execute_vulkan_normal_draw(
          physical_, device_, queue_, queue_family_,
          *normal_draw_command_,
          render_passes_.at(std::string(normal_vertex)),
          graphics_pipelines_.at(std::string(normal_vertex)), pipeline_layout_,
          shared_memory_.shared_descriptor_set(),
          shared_memory_.constant_descriptor_set(normal_vertex),
          &vulkan_cleanup_safe_);
    }
    if (!neutral_resolve_.has_value() && normal_draw_.has_value() &&
        copy_draw_command_.has_value() && present_command.has_value() && ac6demo::has_reached_copy_draw(commands)) {
      neutral_resolve_ = ac6demo::execute_vulkan_neutral_resolve(
          physical_, device_, queue_, queue_family_, *normal_draw_,
          *copy_draw_command_, *present_command);
      ac6demo::commit_reached_guest_present(session, *neutral_resolve_);
    }
  }
  VkInstance instance_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_{VK_NULL_HANDLE};
  VkDevice device_{VK_NULL_HANDLE};
  VkQueue queue_{VK_NULL_HANDLE};
  std::uint32_t queue_family_{};
  std::unordered_map<std::string, VkShaderModule> vulkan_modules_;
  std::unordered_map<std::string, VkRenderPass> render_passes_;
  std::unordered_map<std::string, VkPipeline> graphics_pipelines_;
  VkDescriptorSetLayout shared_layout_{VK_NULL_HANDLE};
  VkDescriptorSetLayout constants_layout_{VK_NULL_HANDLE};
  VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
  VkShaderModule rectangle_geometry_module_{VK_NULL_HANDLE};
  ac6demo::VulkanSharedMemory shared_memory_;
  std::optional<ac6demo::VulkanNormalDrawResult> normal_draw_;
  std::optional<ac6demo::VulkanNeutralResolveResult> neutral_resolve_;
  bool vulkan_cleanup_safe_{true};
  std::optional<ac6demo::XenosDrawCommand> normal_draw_command_;
  std::optional<ac6demo::XenosDrawCommand> copy_draw_command_;
#endif
};
[[nodiscard]] std::string json_string(std::string_view value) {
  std::string output{"\""};
  for (const unsigned char character : value) {
    if (character == '\\' || character == '"') {
      output.push_back('\\');
    }
    output.push_back(static_cast<char>(character));
  }
  output.push_back('"');
  return output;
}

void publish_reachability_atlas(
    const std::filesystem::path &path, const std::filesystem::path &trace,
    std::string_view movie, std::uint64_t completed_ticks,
    const std::vector<ac6demo::GuestFunctionReachability> &functions,
    const std::vector<ac6demo::GuestControlFlowEdge> &flow) {
  const auto address = [](std::uint32_t value) {
    std::ostringstream output;
    output << "0x" << std::uppercase << std::hex << std::setfill('0')
           << std::setw(8) << value;
    return output.str();
  };
  std::ostringstream body;
  body << "  \"tick_range\": {\"first\": 0, \"last\": "
       << (completed_ticks == 0U ? 0U : completed_ticks - 1U) << "},\n";
  body << "  \"functions\": [";
  for (std::size_t index = 0U; index < functions.size(); ++index) {
    const auto &function = functions[index];
    if (index != 0U) body << ',';
    body << "{\"address\":" << json_string(address(function.address))
         << ",\"first_tick\":" << function.first_tick
         << ",\"last_tick\":" << function.last_tick
         << ",\"count\":" << function.count << '}';
  }
  body << "],\n  \"indirect_edges\": [";
  bool first_indirect = true;
  for (const auto &edge : flow) {
    if (edge.kind != ac6demo::GuestControlFlowKind::Indirect) continue;
    if (!first_indirect) body << ',';
    first_indirect = false;
    body << "{\"thread\":" << edge.thread
         << ",\"lr\":" << json_string(address(edge.lr))
         << ",\"target\":" << json_string(address(edge.target))
         << ",\"count\":" << edge.count << '}';
  }
  body << "],\n  \"imports\": [";
  bool first_import = true;
  for (const auto &edge : flow) {
    if (edge.kind != ac6demo::GuestControlFlowKind::Import) continue;
    if (!first_import) body << ',';
    first_import = false;
    body << "{\"module\":" << json_string(edge.module)
         << ",\"ordinal\":" << edge.ordinal
         << ",\"caller_lr\":" << json_string(address(edge.lr))
         << ",\"count\":" << edge.count << '}';
  }
  body << "]";
  const auto body_text = body.str();
  const auto body_bytes = std::as_bytes(
      std::span<const char>(body_text.data(), body_text.size()));
  const auto movie_bytes =
      std::as_bytes(std::span<const char>(movie.data(), movie.size()));
  std::ostringstream output;
  output << "{\n  \"schema\": \"ac6-demo-reachability-atlas/v1\",\n"
         << "  \"target\": {\"id\": \"ac6-demo-xbox360-pal\","
            "\"module\": \"Default.xex\","
            "\"xex_sha256\": \"de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8\","
            "\"ghidra_project\": \"ghidra-projects/ace-combat-6-demo\","
            "\"ghidra_language\": \"PowerPC:BE:64:Xenon\"},\n"
         << "  \"replay\": {\"rtply_v4_sha256\": "
         << json_string(ac6demo::Sha256::file(trace))
         << ", \"xam_movie_v1_sha256\": "
         << json_string(ac6demo::Sha256::bytes(movie_bytes)) << "},\n"
         << body_text << ",\n  \"atlas_sha256\": "
         << json_string(ac6demo::Sha256::bytes(body_bytes)) << "\n}\n";
  publish_new_file(path, output.str());
}

int command_import(int argc, char **argv) {
  if (argc < 3) {
    ac6demo::print_cli_usage(std::cerr);
    return 2;
  }
  const std::filesystem::path source = argv[2];
  std::filesystem::path store = ac6demo::DemoStore::default_path();
  for (int index = 3; index < argc; ++index) {
    const std::string_view option = argv[index];
    if (option == "--store") {
      store = option_value(index, argc, argv, option);
    } else {
      throw std::invalid_argument("unknown import option: " +
                                  std::string(option));
    }
  }
  std::string failure;
  if (!ac6demo::DemoStore::import_directory(source, store, &failure)) {
    std::cerr << "import refused: " << failure << '\n';
    return 1;
  }
  std::cout << "imported qualified AC6 demo into " << store << '\n';
  return 0;
}

int command_play(int argc, char **argv) {
  std::filesystem::path store = ac6demo::DemoStore::default_path();
  std::filesystem::path trace = "ac6-demo.trace.jsonl";
  GraphicsBackend backend = GraphicsBackend::Headless;
  std::optional<std::filesystem::path> xam_movie_record;
  std::uint64_t ticks = 1;
  for (int index = 2; index < argc; ++index) {
    const std::string_view option = argv[index];
    if (option == "--store") {
      store = option_value(index, argc, argv, option);
    } else if (option == "--trace") {
      trace = option_value(index, argc, argv, option);
    } else if (option == "--backend") {
      backend = ac6demo::parse_cli_backend(option_value(index, argc, argv, option));
    } else if (option == "--ticks") {
      ticks = ac6demo::parse_cli_ticks(option_value(index, argc, argv, option));
    } else if (option == "--xam-movie-record") {
      if (xam_movie_record.has_value()) {
        throw std::invalid_argument("duplicate xam-movie-record option");
      }
      xam_movie_record = option_value(index, argc, argv, option);
    } else {
      throw std::invalid_argument("unknown play option: " +
                                  std::string(option));
    }
  }
  if (!ac6demo::generated_guest_available()) {
    throw ac6demo::RuntimeTrap(
        "play unavailable: generated guest is not linked in this build");
  }
  if (xam_movie_record.has_value() && *xam_movie_record == trace) {
    throw std::invalid_argument("XAM movie and trace paths must differ");
  }
  ac6demo::DemoSession session(store, backend);
  RuntimeRendererFrontier renderer(backend);
  try {
    if (xam_movie_record.has_value()) {
      session.begin_xam_movie_record();
    }
    session.start(trace);
    for (std::uint64_t tick = 0; tick < ticks; ++tick) {
      session.step();
      renderer.consume(session);
    }
    std::string movie;
    if (xam_movie_record.has_value()) {
      movie = session.finalize_xam_movie();
    }
    session.stop();
    print_renderer_frontier(renderer);
    if (xam_movie_record.has_value()) {
      publish_new_file(*xam_movie_record, movie);
    }
  } catch (...) {
    session.stop();
    throw;
  }
  std::cout << "guest run complete; no frontend claim; trace=" << trace
            << " ticks=" << ticks << '\n';
  return 0;
}

int command_replay(int argc, char **argv) {
  if (argc < 3) {
    ac6demo::print_cli_usage(std::cerr);
    return 2;
  }
  const std::filesystem::path trace = argv[2];
  std::filesystem::path store = ac6demo::DemoStore::default_path();
  GraphicsBackend requested_backend = GraphicsBackend::Headless;
  bool backend_specified = false;
  std::optional<std::filesystem::path> xam_movie_replay;
  for (int index = 3; index < argc; ++index) {
    const std::string_view option = argv[index];
    if (option == "--store") {
      store = option_value(index, argc, argv, option);
    } else if (option == "--backend") {
      requested_backend =
          ac6demo::parse_cli_backend(option_value(index, argc, argv, option));
      backend_specified = true;
    } else if (option == "--xam-movie-replay") {
      if (xam_movie_replay.has_value()) {
        throw std::invalid_argument("duplicate xam-movie-replay option");
      }
      xam_movie_replay = option_value(index, argc, argv, option);
    } else {
      throw std::invalid_argument("unknown replay option: " +
                                  std::string(option));
    }
  }
  if (!ac6demo::generated_guest_available()) {
    throw ac6demo::RuntimeTrap(
        "replay unavailable: generated guest is not linked in this build");
  }
  std::string failure;
  if (!ac6demo::DemoStore::verify(store, &failure)) {
    throw ac6demo::RuntimeTrap("replay store refused: " + failure);
  }
  const auto replay = ac6demo::TraceReader::read(trace);
  replay.validate();
  if (backend_specified && requested_backend != replay.header().backend) {
    throw ac6demo::RuntimeTrap("replay backend differs from captured trace");
  }
  const auto inputs = replay.input_events();
  if (inputs.empty()) {
    throw ac6demo::RuntimeTrap("replay contains no complete input frames");
  }
  for (std::size_t index = 0U; index < inputs.size(); ++index) {
    if (inputs[index].tick != index) {
      throw ac6demo::RuntimeTrap("replay input ticks are not contiguous");
    }
  }
  const auto stamp = static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const auto replayed_trace =
      std::filesystem::temp_directory_path() /
      ("ac6-demo-replay-" + std::to_string(stamp) + ".jsonl");
  ac6demo::DemoSession session(store, replay.header().backend);
  RuntimeRendererFrontier renderer(replay.header().backend);
  try {
    if (xam_movie_replay.has_value()) {
      session.begin_xam_movie_replay(
          read_xam_movie_file(*xam_movie_replay));
    }
    session.start(replayed_trace);
    for (const auto &input : inputs) {
      session.step(input.frame);
      renderer.consume(session);
    }
    if (xam_movie_replay.has_value()) {
      static_cast<void>(session.finalize_xam_movie());
    }
    session.stop();
    const auto expected = read_binary_file(trace);
    const auto actual = read_binary_file(replayed_trace);
    std::error_code error;
    std::filesystem::remove(replayed_trace, error);
    if (expected != actual) {
      throw ac6demo::RuntimeTrap("replay semantics differ from captured trace");
    }
  } catch (...) {
    session.stop();
    std::error_code error;
    std::filesystem::remove(replayed_trace, error);
    throw;
  }
  std::cout << "replay executed; deterministic=true; events="
            << replay.events().size() << " backend="
            << (replay.header().backend == GraphicsBackend::Vulkan ? "vulkan"
                                                                   : "headless")
            << '\n';
  print_renderer_frontier(renderer);
  return 0;
}

[[nodiscard]] std::string_view
session_state_name(ac6demo::DemoSessionState state) noexcept {
  switch (state) {
  case ac6demo::DemoSessionState::Frontend:
    return "frontend";
  case ac6demo::DemoSessionState::Guest:
    return "guest";
  case ac6demo::DemoSessionState::Stopped:
    return "stopped";
  }
  return "stopped";
}

[[nodiscard]] ac6demo::ProbeMilestones
probe_milestones(const ac6demo::DemoSession &session,
                 const ac6demo::GuestSchedulerSnapshot &scheduler) noexcept {
  ac6demo::ProbeMilestones result;
  result.presents = session.graphics_present_count();
  // VdSwap activity only proves that the guest submitted presentation
  // notifications. Until a title/menu state and its Xenos output are jointly
  // qualified, it must not satisfy the frontend milestone.
  result.frontend = false;
  for (std::uint32_t index = 0U; index < scheduler.wait_count; ++index) {
    if (scheduler.waits[index].id == 1U && scheduler.waits[index].finished) {
      result.terminal = true;
      break;
    }
  }
  return result;
}

[[nodiscard]] bool requested_milestone_reached(
    ac6demo::ProbeUntil until,
    const ac6demo::ProbeMilestones &milestones) noexcept {
  switch (until) {
  case ac6demo::ProbeUntil::Frontend:
    return milestones.frontend;
  case ac6demo::ProbeUntil::Mission:
    return milestones.mission;
  case ac6demo::ProbeUntil::Terminal:
    return milestones.terminal;
  }
  return false;
}

[[nodiscard]] std::uint32_t
trap_thread(const ac6demo::RuntimeTrap &trap,
            const std::vector<ac6demo::GuestControlFlowEdge> &edges) noexcept {
  const ac6demo::GuestControlFlowEdge *best = nullptr;
  for (const auto &edge : edges) {
    if (edge.lr != trap.lr() || edge.last_tick > trap.tick()) {
      continue;
    }
    if (best == nullptr || edge.last_tick > best->last_tick ||
        (edge.last_tick == best->last_tick && edge.count > best->count)) {
      best = &edge;
    }
  }
  return best == nullptr ? 0U : best->thread;
}

int command_probe(int argc, char **argv) {
  std::filesystem::path store = ac6demo::DemoStore::default_path();
  std::filesystem::path trace;
  std::filesystem::path report;
  std::optional<std::filesystem::path> atlas;
  GraphicsBackend backend = GraphicsBackend::Headless;
  ac6demo::ProbeUntil until = ac6demo::ProbeUntil::Frontend;
  std::vector<ac6demo::ScheduledInputFrame> scheduled_inputs;
  std::optional<std::filesystem::path> xam_movie_record;
  std::optional<std::filesystem::path> xam_movie_replay;
  std::uint64_t max_ticks{};
  bool has_until = false;
  for (int index = 2; index < argc; ++index) {
    const std::string_view option = argv[index];
    if (option == "--store") {
      store = option_value(index, argc, argv, option);
    } else if (option == "--trace") {
      trace = option_value(index, argc, argv, option);
    } else if (option == "--report") {
      report = option_value(index, argc, argv, option);
    } else if (option == "--backend") {
      backend = ac6demo::parse_cli_backend(option_value(index, argc, argv, option));
    } else if (option == "--max-ticks") {
      max_ticks = ac6demo::parse_cli_ticks(option_value(index, argc, argv, option));
    } else if (option == "--until") {
      const auto value = option_value(index, argc, argv, option);
      if (!ac6demo::parse_probe_until(value, &until)) {
        throw std::invalid_argument(
            "until must be frontend, mission or terminal");
      }
      has_until = true;
    } else if (option == "--input-at") {
      ac6demo::ScheduledInputFrame input;
      const auto value = option_value(index, argc, argv, option);
      if (!ac6demo::parse_scheduled_input(value, &input)) {
        throw std::invalid_argument(
            "input-at must be tick,buttons,lt,rt,lx,ly,rx,ry,connected");
      }
      scheduled_inputs.push_back(input);
    } else if (option == "--xam-movie-record") {
      if (xam_movie_record.has_value()) {
        throw std::invalid_argument("duplicate xam-movie-record option");
      }
      xam_movie_record = option_value(index, argc, argv, option);
    } else if (option == "--xam-movie-replay") {
      if (xam_movie_replay.has_value()) {
        throw std::invalid_argument("duplicate xam-movie-replay option");
      }
      xam_movie_replay = option_value(index, argc, argv, option);
    } else if (option == "--atlas") {
      if (atlas.has_value()) {
        throw std::invalid_argument("duplicate atlas option");
      }
      atlas = option_value(index, argc, argv, option);
    } else {
      throw std::invalid_argument("unknown probe option: " +
                                  std::string(option));
    }
  }
  if (!ac6demo::generated_guest_available()) {
    throw ac6demo::RuntimeTrap(
        "probe unavailable: generated guest is not linked in this build");
  }
  if (!has_until || max_ticks == 0U || trace.empty() || report.empty()) {
    throw std::invalid_argument(
        "probe requires --until, positive --max-ticks, --trace and --report");
  }
  if (xam_movie_record.has_value() && xam_movie_replay.has_value()) {
    throw std::invalid_argument(
        "xam-movie-record and xam-movie-replay are mutually exclusive");
  }
  if (trace == report) {
    throw std::invalid_argument("probe trace and report paths must differ");
  }
  if ((xam_movie_record.has_value() &&
       (*xam_movie_record == trace || *xam_movie_record == report)) ||
      (xam_movie_replay.has_value() &&
       (*xam_movie_replay == trace || *xam_movie_replay == report))) {
    throw std::invalid_argument(
        "XAM movie, trace and report paths must be distinct");
  }
  if (atlas.has_value() &&
      (*atlas == trace || *atlas == report ||
       (xam_movie_record.has_value() && *atlas == *xam_movie_record) ||
       (xam_movie_replay.has_value() && *atlas == *xam_movie_replay))) {
    throw std::invalid_argument(
        "atlas, XAM movie, trace and report paths must be distinct");
  }
  if (atlas.has_value() && !xam_movie_record.has_value() &&
      !xam_movie_replay.has_value()) {
    throw std::invalid_argument("atlas output requires an XAM movie record or replay");
  }
  std::ranges::sort(scheduled_inputs, {}, &ac6demo::ScheduledInputFrame::tick);
  for (std::size_t index = 0U; index < scheduled_inputs.size(); ++index) {
    if (scheduled_inputs[index].tick >= max_ticks ||
        (index != 0U &&
         scheduled_inputs[index - 1U].tick == scheduled_inputs[index].tick)) {
      throw std::invalid_argument(
          "input-at ticks must be unique and below max-ticks");
    }
  }

  ac6demo::DemoSession session(store, backend);
  RuntimeRendererFrontier renderer(backend);
  session.enable_function_reachability(atlas.has_value());
  std::string xam_movie_replay_payload;
  if (xam_movie_record.has_value()) {
    session.begin_xam_movie_record();
  } else if (xam_movie_replay.has_value()) {
    xam_movie_replay_payload = read_xam_movie_file(*xam_movie_replay);
    session.begin_xam_movie_replay(xam_movie_replay_payload);
  }
  session.start(trace);
  try {
    std::string outcome = "max_ticks";
    std::size_t input_index = 0U;
    for (std::uint64_t tick = 0U; tick < max_ticks; ++tick) {
      ac6demo::InputFrame input;
      if (input_index < scheduled_inputs.size() &&
          scheduled_inputs[input_index].tick == tick) {
        input = scheduled_inputs[input_index].frame;
        ++input_index;
      }
      session.step(input);
      renderer.consume(session);
      const auto scheduler = session.guest_scheduler_snapshot();
      if (requested_milestone_reached(until,
                                      probe_milestones(session, scheduler))) {
        outcome = "milestone";
        break;
      }
    }
    const auto state = std::string(session_state_name(session.state()));
    const auto completed_ticks = session.tick();
    const auto scheduler = session.guest_scheduler_snapshot();
    const auto milestones = probe_milestones(session, scheduler);
    const auto graphics_ring = session.xenos_ring_snapshot();
    const auto graphics_swap = session.vd_swap_snapshot();
    auto flow = session.control_flow_snapshot();
    const auto functions = session.function_reachability_snapshot();
    auto frontier = outcome == "milestone"
                        ? std::optional<ac6demo::FrontierSnapshot>{}
                        : ac6demo::derive_progress_frontier(
                              until, completed_ticks, milestones, scheduler);
    std::string movie;
    if (xam_movie_record.has_value() || xam_movie_replay.has_value()) {
      movie = session.finalize_xam_movie();
      if (xam_movie_replay.has_value()) {
        movie = xam_movie_replay_payload;
      }
    }
    session.stop();
    if (xam_movie_record.has_value()) {
      publish_new_file(*xam_movie_record, movie);
    }
    if (atlas.has_value()) {
      publish_reachability_atlas(*atlas, trace, movie, completed_ticks,
                                 functions, flow);
    }
    ac6demo::write_frontier_report(
        report, ac6demo::FrontierReportInput{
                    until, max_ticks, backend, trace, outcome, state,
                    completed_ticks, milestones, graphics_ring, graphics_swap,
                    scheduler, std::move(flow), std::move(frontier)});
    print_renderer_frontier(renderer);
    std::cout << "probe complete; outcome=" << outcome
              << " ticks=" << completed_ticks << " report=" << report << '\n';
    return outcome == "milestone" ? 0 : 4;
  } catch (const ac6demo::RuntimeTrap &trap) {
    const auto state = std::string(session_state_name(session.state()));
    const auto completed_ticks = session.tick();
    const auto scheduler = session.guest_scheduler_snapshot();
    const auto milestones = probe_milestones(session, scheduler);
    const auto graphics_ring = session.xenos_ring_snapshot();
    const auto graphics_swap = session.vd_swap_snapshot();
    auto flow = session.control_flow_snapshot();
    const auto thread = trap_thread(trap, flow);
    session.stop();
    ac6demo::FrontierSnapshot frontier{trap.what(), trap.tick(), thread,
                                       trap.lr(), trap.address()};
    ac6demo::write_frontier_report(
        report, ac6demo::FrontierReportInput{
                    until, max_ticks, backend, trace, "trap", state,
                    completed_ticks, milestones, graphics_ring, graphics_swap,
                    scheduler, std::move(flow), frontier});
    throw;
  }
}

} // namespace
int main(int argc, char **argv) {
  try {
    if (const auto ipc = ac6demo::maybe_run_emu_agent_ipc(argc, argv); ipc.has_value()) return *ipc;
    if (argc < 2 || std::string_view(argv[1]) == "--help" ||
        std::string_view(argv[1]) == "-h") {
      ac6demo::print_cli_usage(std::cout);
      return argc < 2 ? 2 : 0;
    }
    const std::string_view command = argv[1];
    if (command == "import") {
      return command_import(argc, argv);
    }
    if (command == "play") {
      return command_play(argc, argv);
    }
    if (command == "replay") {
      return command_replay(argc, argv);
    }
    if (command == "probe") {
      return command_probe(argc, argv);
    }
    ac6demo::print_cli_usage(std::cerr);
    return 2;
  } catch (const ac6demo::RuntimeTrap &trap) {
    std::cerr << "AC6 runtime trap: " << trap.what() << " tick=" << trap.tick()
              << " lr=0x" << std::hex << trap.lr() << " address=0x"
              << trap.address() << '\n';
    return 3;
  } catch (const std::exception &exception) {
    std::cerr << "AC6 error: " << exception.what() << '\n';
    return 2;
  }
}
