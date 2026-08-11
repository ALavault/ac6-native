#include "vulkan_backend_internal.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace ac6 {
namespace {

struct DeviceCandidate {
  VkPhysicalDevice device{VK_NULL_HANDLE};
  std::uint32_t queue_family{};
  std::uint32_t score{};
};

[[nodiscard]] std::optional<std::uint32_t> graphics_queue_family(
    VkPhysicalDevice device) {
  std::uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  if (count == 0U) return std::nullopt;
  std::vector<VkQueueFamilyProperties> properties(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());
  for (std::uint32_t index = 0; index < count; ++index) {
    if (properties[index].queueCount != 0U &&
        (properties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
      return index;
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<DeviceCandidate> select_device(VkInstance instance) {
  std::uint32_t count = 0;
  if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS ||
      count == 0U) {
    return std::nullopt;
  }
  std::vector<VkPhysicalDevice> devices(count);
  if (vkEnumeratePhysicalDevices(instance, &count, devices.data()) != VK_SUCCESS) {
    return std::nullopt;
  }
  std::optional<DeviceCandidate> best;
  for (const VkPhysicalDevice device : devices) {
    const auto queue_family = graphics_queue_family(device);
    if (!queue_family) continue;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device, &properties);
    if (properties.apiVersion < VK_API_VERSION_1_3) continue;
    std::uint32_t score = properties.limits.maxImageDimension2D;
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      score += 1'000'000U;
    }
    if (!best || score > best->score) {
      best = DeviceCandidate{device, *queue_family, score};
    }
  }
  return best;
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
}

}  // namespace

VulkanBackend::VulkanBackend(std::unique_ptr<VulkanBackendState> state) noexcept
    : state_(std::move(state)) {}

VulkanBackend::~VulkanBackend() {
  if (!state_) return;
  if (state_->device != VK_NULL_HANDLE) {
    static_cast<void>(vkDeviceWaitIdle(state_->device));
    for (auto& [unused, pipeline] : state_->pipelines) {
      static_cast<void>(unused);
      if (pipeline.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(state_->device, pipeline.pipeline, nullptr);
      }
      if (pipeline.layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(state_->device, pipeline.layout, nullptr);
      }
    }
    for (auto& [unused, target] : state_->targets) {
      static_cast<void>(unused);
      destroy_target(*state_, target);
    }
    for (auto& [unused, mesh] : state_->meshes) {
      static_cast<void>(unused);
      destroy_vulkan_buffer(*state_, mesh.vertex_buffer, mesh.vertex_memory);
      destroy_vulkan_buffer(*state_, mesh.index_buffer, mesh.index_memory);
    }
    if (state_->command_pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(state_->device, state_->command_pool, nullptr);
    }
    vkDestroyDevice(state_->device, nullptr);
  }
  if (state_->instance != VK_NULL_HANDLE) {
    vkDestroyInstance(state_->instance, nullptr);
  }
}

VulkanBackendCreateResult VulkanBackend::create() {
  VulkanBackendCreateResult result;
  auto state = std::make_unique<VulkanBackendState>();
  const auto fail = [&](const VulkanBackendError error) {
    auto cleanup = std::unique_ptr<VulkanBackend>(
        new VulkanBackend(std::move(state)));
    result.error = error;
    return std::move(result);
  };
  const VkApplicationInfo application{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = "ac6-native",
      .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
      .pEngineName = "ac6-vulkan",
      .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
      .apiVersion = VK_API_VERSION_1_3,
  };
  const VkInstanceCreateInfo instance_info{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .pApplicationInfo = &application,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = 0,
      .ppEnabledExtensionNames = nullptr,
  };
  if (vkCreateInstance(&instance_info, nullptr, &state->instance) != VK_SUCCESS) {
    return fail(VulkanBackendError::instance_creation_failed);
  }
  const auto selected = select_device(state->instance);
  if (!selected) {
    return fail(VulkanBackendError::physical_device_unavailable);
  }
  state->physical_device = selected->device;
  state->queue_family = selected->queue_family;
  const float priority = 1.0F;
  const VkDeviceQueueCreateInfo queue_info{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueFamilyIndex = state->queue_family,
      .queueCount = 1,
      .pQueuePriorities = &priority,
  };
  const VkDeviceCreateInfo device_info{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queue_info,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = 0,
      .ppEnabledExtensionNames = nullptr,
      .pEnabledFeatures = nullptr,
  };
  if (vkCreateDevice(state->physical_device, &device_info, nullptr,
                     &state->device) != VK_SUCCESS) {
    return fail(VulkanBackendError::device_creation_failed);
  }
  vkGetDeviceQueue(state->device, state->queue_family, 0, &state->queue);
  vkGetPhysicalDeviceMemoryProperties(state->physical_device,
                                      &state->memory_properties);
  const VkCommandPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
               VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = state->queue_family,
  };
  if (vkCreateCommandPool(state->device, &pool_info, nullptr,
                          &state->command_pool) != VK_SUCCESS) {
    return fail(VulkanBackendError::command_pool_creation_failed);
  }

  VkPhysicalDeviceProperties properties{};
  VkPhysicalDeviceFeatures features{};
  VkFormatProperties depth_properties{};
  vkGetPhysicalDeviceProperties(state->physical_device, &properties);
  vkGetPhysicalDeviceFeatures(state->physical_device, &features);
  vkGetPhysicalDeviceFormatProperties(state->physical_device,
                                      VK_FORMAT_D32_SFLOAT,
                                      &depth_properties);
  state->caps.api_version = properties.apiVersion;
  state->caps.vendor_id = properties.vendorID;
  state->caps.device_id = properties.deviceID;
  state->caps.max_image_dimension_2d = properties.limits.maxImageDimension2D;
  state->caps.max_sampler_anisotropy = properties.limits.maxSamplerAnisotropy;
  state->caps.discrete_gpu =
      properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
  state->caps.depth_d32 =
      (depth_properties.optimalTilingFeatures &
       VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0U;
  state->caps.sampler_anisotropy = features.samplerAnisotropy != VK_FALSE;
  state->caps.device_name = properties.deviceName;

  result.backend = std::unique_ptr<VulkanBackend>(new VulkanBackend(std::move(state)));
  return result;
}

const RenderDeviceCaps& VulkanBackend::caps() const noexcept {
  return state_->caps;
}

std::size_t VulkanBackend::live_mesh_count() const noexcept {
  return state_->meshes.size();
}

std::size_t VulkanBackend::live_render_target_count() const noexcept {
  return state_->targets.size();
}

std::size_t VulkanBackend::live_pipeline_count() const noexcept {
  return state_->pipelines.size();
}

const char* vulkan_backend_error_name(const VulkanBackendError error) noexcept {
  switch (error) {
    case VulkanBackendError::none:
      return "none";
    case VulkanBackendError::instance_creation_failed:
      return "instance_creation_failed";
    case VulkanBackendError::physical_device_unavailable:
      return "physical_device_unavailable";
    case VulkanBackendError::device_creation_failed:
      return "device_creation_failed";
    case VulkanBackendError::command_pool_creation_failed:
      return "command_pool_creation_failed";
  }
  return "unknown";
}

}  // namespace ac6
