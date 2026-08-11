#include "ac6/sdl_input.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace ac6 {

SdlVulkanSurface::~SdlVulkanSurface() { destroy(); }

bool SdlVulkanSurface::required_instance_extensions(
    std::vector<const char*>& extensions) noexcept {
  Uint32 count = 0;
  const char* const* names = SDL_Vulkan_GetInstanceExtensions(&count);
  if (names == nullptr || count == 0) return false;
  extensions.assign(names, names + count);
  return true;
}

bool SdlVulkanSurface::create(const SdlWindow& window, VkInstance instance) noexcept {
  if (!window.valid() || window.native_handle() == nullptr || instance == VK_NULL_HANDLE ||
      surface_ != VK_NULL_HANDLE) return false;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (!SDL_Vulkan_CreateSurface(window.native_handle(), instance, nullptr, &surface)) return false;
  instance_ = instance;
  surface_ = surface;
  return true;
}

void SdlVulkanSurface::destroy() noexcept {
  if (surface_ == VK_NULL_HANDLE) return;
  SDL_Vulkan_DestroySurface(instance_, surface_, nullptr);
  surface_ = VK_NULL_HANDLE;
  instance_ = VK_NULL_HANDLE;
}

VulkanInstance::~VulkanInstance() { destroy(); }

bool VulkanInstance::create(const std::vector<const char*>& extensions) noexcept {
  if (instance_ != VK_NULL_HANDLE) return false;
  for (const char* extension : extensions) {
    if (extension == nullptr || extension[0] == '\0') return false;
  }
  for (std::size_t i = 0; i < extensions.size(); ++i) {
    for (std::size_t j = i + 1; j < extensions.size(); ++j) {
      if (std::strcmp(extensions[i], extensions[j]) == 0) return false;
    }
  }
  const VkApplicationInfo application{
      VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "ac6-native", 1,
      "ac6-native", 1, VK_API_VERSION_1_0};
  const VkInstanceCreateInfo create_info{
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &application,
      0, nullptr, static_cast<std::uint32_t>(extensions.size()), extensions.data()};
  return vkCreateInstance(&create_info, nullptr, &instance_) == VK_SUCCESS;
}

void VulkanInstance::destroy() noexcept {
  if (instance_ == VK_NULL_HANDLE) return;
  vkDestroyInstance(instance_, nullptr);
  instance_ = VK_NULL_HANDLE;
}

VulkanDevice::~VulkanDevice() { destroy(); }

bool VulkanDevice::create(VkInstance instance, VkSurfaceKHR surface) noexcept {
  if (instance == VK_NULL_HANDLE || surface == VK_NULL_HANDLE || valid()) return false;
  std::uint32_t device_count = 0;
  if (vkEnumeratePhysicalDevices(instance, &device_count, nullptr) != VK_SUCCESS ||
      device_count == 0) return false;
  std::vector<VkPhysicalDevice> physical_devices(device_count);
  if (vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data()) != VK_SUCCESS) {
    return false;
  }
  for (const VkPhysicalDevice candidate : physical_devices) {
    std::uint32_t extension_count = 0;
    if (vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, nullptr) != VK_SUCCESS) {
      continue;
    }
    std::vector<VkExtensionProperties> extensions(extension_count);
    if (vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count,
                                             extensions.data()) != VK_SUCCESS) continue;
    const bool has_swapchain = std::any_of(
        extensions.begin(), extensions.end(), [](const VkExtensionProperties& extension) {
          return std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
        });
    if (!has_swapchain) continue;
    std::uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, queues.data());
    for (std::uint32_t family = 0; family < queue_count; ++family) {
      VkBool32 present = VK_FALSE;
      if ((queues[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 ||
          vkGetPhysicalDeviceSurfaceSupportKHR(candidate, family, surface, &present) != VK_SUCCESS ||
          present == VK_FALSE || queues[family].queueCount == 0) continue;
      const float priority = 1.0f;
      const VkDeviceQueueCreateInfo queue_info{
          VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, family, 1, &priority};
      const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
      const VkDeviceCreateInfo device_info{
          VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, 0, 1, &queue_info, 0, nullptr,
          1, device_extensions, nullptr};
      VkDevice logical_device = VK_NULL_HANDLE;
      if (vkCreateDevice(candidate, &device_info, nullptr, &logical_device) != VK_SUCCESS) continue;
      physical_device_ = candidate;
      device_ = logical_device;
      queue_family_ = family;
      vkGetDeviceQueue(device_, queue_family_, 0, &graphics_queue_);
      return graphics_queue_ != VK_NULL_HANDLE;
    }
  }
  return false;
}

void VulkanDevice::destroy() noexcept {
  if (device_ == VK_NULL_HANDLE) return;
  vkDeviceWaitIdle(device_);
  vkDestroyDevice(device_, nullptr);
  device_ = VK_NULL_HANDLE;
  physical_device_ = VK_NULL_HANDLE;
  graphics_queue_ = VK_NULL_HANDLE;
  queue_family_ = 0;
}


}  // namespace ac6
