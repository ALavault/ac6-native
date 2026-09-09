#include "ac6/native_vulkan_device.h"

#include <cstring>
#include <vector>

namespace ac6::native {

namespace {

constexpr std::uint32_t kBytesPerPixel = 4u;

int device_score(VkPhysicalDeviceType type) noexcept {
  switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return 3;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return 2;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return 1;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return 0;
    default:
      return -1;
  }
}

}  // namespace

VulkanDevice::VulkanDevice(const VulkanDeviceDesc& desc) noexcept
    : width_(desc.width), height_(desc.height) {
  if (width_ == 0u || height_ == 0u) {
    error_ = "Vulkan offscreen dimensions must be nonzero";
    return;
  }
  std::uint32_t extension_count = 0u;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                             nullptr) != VK_SUCCESS) {
    error_ = "vkEnumerateInstanceExtensionProperties failed";
    return;
  }
  std::vector<VkExtensionProperties> extensions(extension_count);
  if (vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                             extensions.data()) != VK_SUCCESS) {
    error_ = "vkEnumerateInstanceExtensionProperties failed";
    return;
  }
  bool has_surface = false;
  bool has_headless = false;
  bool has_xcb = false;
  for (const VkExtensionProperties& extension : extensions) {
    if (std::strcmp(extension.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) ==
        0) {
      has_surface = true;
    }
    if (std::strcmp(extension.extensionName,
                    VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME) == 0) {
      has_headless = true;
    }
    if (std::strcmp(extension.extensionName,
                    VK_KHR_XCB_SURFACE_EXTENSION_NAME) == 0) {
      has_xcb = true;
    }
  }
  if (!has_surface || !has_headless) {
    error_ = "VK_KHR_surface or VK_EXT_headless_surface is unavailable";
    return;
  }
  if (desc.enable_xcb_surface && !has_xcb) {
    error_ = "VK_KHR_xcb_surface is unavailable";
    return;
  }
  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.apiVersion = VK_API_VERSION_1_0;
  const char* wanted[3] = {VK_KHR_SURFACE_EXTENSION_NAME,
                           VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME,
                           VK_KHR_XCB_SURFACE_EXTENSION_NAME};
  VkInstanceCreateInfo instance_info{};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app;
  // Opt-in diagnostics: AC6_VK_VALIDATE=1 enables the Khronos validation
  // layer when it is installed (debug affordance; never set by the product).
  // The layer is only requested when enumerated, so setting the variable on
  // a host without it does not break instance creation.
  const char* validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
  const bool want_validation = std::getenv("AC6_VK_VALIDATE") != nullptr;
  bool validation_available = false;
  if (want_validation) {
    std::uint32_t layer_count = 0u;
    if (vkEnumerateInstanceLayerProperties(&layer_count, nullptr) ==
        VK_SUCCESS) {
      std::vector<VkLayerProperties> layers(layer_count);
      if (vkEnumerateInstanceLayerProperties(&layer_count, layers.data()) ==
          VK_SUCCESS) {
        for (const auto& layer : layers) {
          if (std::strcmp(layer.layerName, validation_layers[0]) == 0) {
            validation_available = true;
            break;
          }
        }
      }
    }
  }
  instance_info.enabledLayerCount =
      (want_validation && validation_available) ? 1u : 0u;
  instance_info.ppEnabledLayerNames =
      (want_validation && validation_available) ? validation_layers : nullptr;
  instance_info.enabledExtensionCount = desc.enable_xcb_surface ? 3u : 2u;
  instance_info.ppEnabledExtensionNames = wanted;
  if (vkCreateInstance(&instance_info, nullptr, &instance_) != VK_SUCCESS ||
      instance_ == VK_NULL_HANDLE) {
    error_ = "vkCreateInstance failed";
    instance_ = VK_NULL_HANDLE;
    return;
  }
  std::uint32_t device_count = 0u;
  if (vkEnumeratePhysicalDevices(instance_, &device_count, nullptr) !=
          VK_SUCCESS ||
      device_count == 0u) {
    error_ = "no Vulkan physical devices enumerated";
    return;
  }
  std::vector<VkPhysicalDevice> devices(device_count);
  if (vkEnumeratePhysicalDevices(instance_, &device_count, devices.data()) !=
      VK_SUCCESS) {
    error_ = "vkEnumeratePhysicalDevices failed";
    return;
  }
  int best_score = -2;
  for (const VkPhysicalDevice candidate : devices) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(candidate, &properties);
    const int score = device_score(properties.deviceType);
    if (score < 0) {
      continue;
    }
    if (desc.require_discrete &&
        properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      continue;
    }
    std::uint32_t family_count = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count,
                                             families.data());
    bool has_graphics = false;
    std::uint32_t family_index = 0u;
    for (const VkQueueFamilyProperties& family : families) {
      if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u &&
          family.queueCount > 0u) {
        has_graphics = true;
        break;
      }
      ++family_index;
    }
    if (!has_graphics) {
      continue;
    }
    if (score > best_score) {
      best_score = score;
      physical_device_ = candidate;
      queue_family_ = family_index;
      char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {};
      std::memcpy(name, properties.deviceName, sizeof(name) - 1u);
      device_name_ = name;
    }
  }
  if (physical_device_ == VK_NULL_HANDLE) {
    error_ = "no Vulkan physical device with a graphics queue found";
    return;
  }
  const float priority = 1.0f;
  VkDeviceQueueCreateInfo queue_info{};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = queue_family_;
  queue_info.queueCount = 1u;
  queue_info.pQueuePriorities = &priority;
  std::uint32_t device_extension_count = 0u;
  if (vkEnumerateDeviceExtensionProperties(physical_device_, nullptr,
                                           &device_extension_count,
                                           nullptr) != VK_SUCCESS) {
    error_ = "vkEnumerateDeviceExtensionProperties failed";
    return;
  }
  std::vector<VkExtensionProperties> device_extensions(device_extension_count);
  if (vkEnumerateDeviceExtensionProperties(physical_device_, nullptr,
                                           &device_extension_count,
                                           device_extensions.data()) !=
      VK_SUCCESS) {
    error_ = "vkEnumerateDeviceExtensionProperties failed";
    return;
  }
  bool has_swapchain = false;
  for (const VkExtensionProperties& extension : device_extensions) {
    if (std::strcmp(extension.extensionName,
                    VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
      has_swapchain = true;
      break;
    }
  }
  if (!has_swapchain) {
    error_ = "VK_KHR_swapchain device extension is unavailable";
    return;
  }
  const char* device_wanted[1] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkDeviceCreateInfo device_info{};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = 1u;
  device_info.pQueueCreateInfos = &queue_info;
  device_info.enabledExtensionCount = 1u;
  device_info.ppEnabledExtensionNames = device_wanted;
  if (vkCreateDevice(physical_device_, &device_info, nullptr, &device_) !=
          VK_SUCCESS ||
      device_ == VK_NULL_HANDLE) {
    error_ = "vkCreateDevice failed";
    device_ = VK_NULL_HANDLE;
    return;
  }
  vkGetDeviceQueue(device_, queue_family_, 0u, &queue_);
  if (queue_ == VK_NULL_HANDLE) {
    error_ = "vkGetDeviceQueue returned null";
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }
}

VulkanDevice::~VulkanDevice() noexcept {
  if (device_ != VK_NULL_HANDLE) {
    // Draining in-flight queue work before destruction: resources may still
    // be in use by submits on the (shared) physical queue, and a device
    // destroyed while busy corrupts the next device's draws.
    vkDeviceWaitIdle(device_);
    vkDestroyDevice(device_, nullptr);
  }
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
  }
}

VulkanOffscreenTarget::VulkanOffscreenTarget(
    const VulkanDevice& device) noexcept
    : device_(&device), width_(device.width()), height_(device.height()) {
  if (!device.valid()) {
    error_ = "cannot build an offscreen target on an invalid device";
    return;
  }
  byte_size_ = static_cast<std::size_t>(width_) *
               static_cast<std::size_t>(height_) * kBytesPerPixel;
  if (byte_size_ == 0u ||
      byte_size_ / kBytesPerPixel !=
          static_cast<std::size_t>(width_) * height_) {
    error_ = "offscreen target size overflows";
    byte_size_ = 0u;
    return;
  }
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_info.extent.width = width_;
  image_info.extent.height = height_;
  image_info.extent.depth = 1u;
  image_info.mipLevels = 1u;
  image_info.arrayLayers = 1u;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (vkCreateImage(device_->device(), &image_info, nullptr, &image_) !=
          VK_SUCCESS ||
      image_ == VK_NULL_HANDLE) {
    error_ = "vkCreateImage failed";
    image_ = VK_NULL_HANDLE;
    return;
  }
  VkMemoryRequirements image_requirements{};
  vkGetImageMemoryRequirements(device_->device(), image_,
                               &image_requirements);
  const std::uint32_t image_type =
      find_memory(image_requirements.memoryTypeBits,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (image_type == UINT32_MAX) {
    error_ = "no device-local memory type for the offscreen image";
    return;
  }
  VkMemoryAllocateInfo image_alloc{};
  image_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  image_alloc.allocationSize = image_requirements.size;
  image_alloc.memoryTypeIndex = image_type;
  if (vkAllocateMemory(device_->device(), &image_alloc, nullptr,
                       &image_memory_) != VK_SUCCESS) {
    error_ = "vkAllocateMemory for the offscreen image failed";
    return;
  }
  if (vkBindImageMemory(device_->device(), image_, image_memory_, 0u) !=
      VK_SUCCESS) {
    error_ = "vkBindImageMemory failed";
    return;
  }
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = byte_size_;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(device_->device(), &buffer_info, nullptr, &staging_) !=
          VK_SUCCESS ||
      staging_ == VK_NULL_HANDLE) {
    error_ = "vkCreateBuffer for the staging buffer failed";
    staging_ = VK_NULL_HANDLE;
    return;
  }
  VkMemoryRequirements buffer_requirements{};
  vkGetBufferMemoryRequirements(device_->device(), staging_,
                                &buffer_requirements);
  const std::uint32_t buffer_type = find_memory(
      buffer_requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (buffer_type == UINT32_MAX) {
    error_ = "no host-visible coherent memory type for staging";
    return;
  }
  VkMemoryAllocateInfo buffer_alloc{};
  buffer_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  buffer_alloc.allocationSize = buffer_requirements.size;
  buffer_alloc.memoryTypeIndex = buffer_type;
  if (vkAllocateMemory(device_->device(), &buffer_alloc, nullptr,
                       &staging_memory_) != VK_SUCCESS) {
    error_ = "vkAllocateMemory for staging failed";
    return;
  }
  if (vkBindBufferMemory(device_->device(), staging_, staging_memory_, 0u) !=
      VK_SUCCESS) {
    error_ = "vkBindBufferMemory failed";
    return;
  }
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  pool_info.queueFamilyIndex = device_->queue_family();
  if (vkCreateCommandPool(device_->device(), &pool_info, nullptr, &pool_) !=
          VK_SUCCESS ||
      pool_ == VK_NULL_HANDLE) {
    error_ = "vkCreateCommandPool failed";
    pool_ = VK_NULL_HANDLE;
  }
}

VulkanOffscreenTarget::~VulkanOffscreenTarget() noexcept {
  if (device_ == nullptr || !device_->valid()) {
    return;
  }
  if (pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_->device(), pool_, nullptr);
  }
  if (staging_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(device_->device(), staging_, nullptr);
  }
  if (staging_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_->device(), staging_memory_, nullptr);
  }
  if (image_ != VK_NULL_HANDLE) {
    vkDestroyImage(device_->device(), image_, nullptr);
  }
  if (image_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_->device(), image_memory_, nullptr);
  }
}

bool VulkanOffscreenTarget::transition(VkCommandBuffer commands,
                                       VkImageLayout next) noexcept {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = layout_;
  barrier.newLayout = next;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image_;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0u;
  barrier.subresourceRange.levelCount = 1u;
  barrier.subresourceRange.baseArrayLayer = 0u;
  barrier.subresourceRange.layerCount = 1u;
  if (layout_ == VK_IMAGE_LAYOUT_UNDEFINED &&
      next == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0u;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  } else if (layout_ == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             next == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  } else if (layout_ == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
             next == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  } else {
    error_ = "unsupported offscreen layout transition";
    return false;
  }
  vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u,
                       nullptr, 1u, &barrier);
  layout_ = next;
  return true;
}

std::uint32_t VulkanOffscreenTarget::find_memory(
    std::uint32_t bits, VkMemoryPropertyFlags flags) const noexcept {
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(device_->physical_device(), &properties);
  for (std::uint32_t index = 0u; index < properties.memoryTypeCount;
       ++index) {
    if ((bits & (1u << index)) != 0u &&
        (properties.memoryTypes[index].propertyFlags & flags) == flags) {
      return index;
    }
  }
  return UINT32_MAX;
}

bool VulkanOffscreenTarget::ensure_transfer_src() noexcept {
  error_.clear();
  if (!valid() || pool_ == VK_NULL_HANDLE) {
    error_ = "ensure_transfer_src on an invalid offscreen target";
    return false;
  }
  if (layout_ == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    return true;
  }
  if (layout_ != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    error_ = "offscreen image is not in a blittable layout";
    return false;
  }
  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = pool_;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = 1u;
  VkCommandBuffer commands = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device_->device(), &alloc, &commands) !=
          VK_SUCCESS ||
      commands == VK_NULL_HANDLE) {
    error_ = "vkAllocateCommandBuffers failed";
    return false;
  }
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  bool ok = vkBeginCommandBuffer(commands, &begin) == VK_SUCCESS;
  if (ok) {
    ok = transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  }
  if (ok) {
    ok = vkEndCommandBuffer(commands) == VK_SUCCESS;
  }
  if (ok) {
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &commands;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    ok = vkCreateFence(device_->device(), &fence_info, nullptr, &fence) ==
             VK_SUCCESS &&
         fence != VK_NULL_HANDLE;
    if (ok) {
      ok = vkQueueSubmit(device_->queue(), 1u, &submit, fence) == VK_SUCCESS;
      if (ok) {
        ok = vkWaitForFences(device_->device(), 1u, &fence, VK_TRUE,
                             10u * 1000u * 1000u * 1000u) == VK_SUCCESS;
      }
      vkDestroyFence(device_->device(), fence, nullptr);
    }
  }
  vkFreeCommandBuffers(device_->device(), pool_, 1u, &commands);
  if (!ok) {
    error_ = "offscreen transfer-src transition failed";
  }
  return ok;
}

bool VulkanOffscreenTarget::begin_transfer_dst(
    VkCommandBuffer commands) noexcept {
  error_.clear();
  if (!valid() || pool_ == VK_NULL_HANDLE) {
    error_ = "begin_transfer_dst on an invalid offscreen target";
    return false;
  }
  if (layout_ == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    return true;
  }
  if (layout_ != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
      layout_ != VK_IMAGE_LAYOUT_UNDEFINED) {
    error_ = "unsupported offscreen layout for transfer destination";
    return false;
  }
  if (!transition(commands, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) {
    return false;
  }
  return true;
}

bool VulkanOffscreenTarget::end_transfer_dst(
    VkCommandBuffer commands) noexcept {
  error_.clear();
  if (!valid() || pool_ == VK_NULL_HANDLE) {
    error_ = "end_transfer_dst on an invalid offscreen target";
    return false;
  }
  if (layout_ != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    error_ = "end_transfer_dst outside a transfer destination";
    return false;
  }
  if (!transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)) {
    return false;
  }
  return true;
}

bool VulkanOffscreenTarget::clear(float red, float green, float blue,
                                  float alpha) noexcept {
  error_.clear();
  if (!valid() || pool_ == VK_NULL_HANDLE) {
    error_ = "clear on an invalid offscreen target";
    return false;
  }
  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = pool_;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = 1u;
  VkCommandBuffer commands = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device_->device(), &alloc, &commands) !=
          VK_SUCCESS ||
      commands == VK_NULL_HANDLE) {
    error_ = "vkAllocateCommandBuffers failed";
    return false;
  }
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  bool ok = vkBeginCommandBuffer(commands, &begin) == VK_SUCCESS;
  if (ok && layout_ != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    ok = transition(commands, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  }
  if (ok) {
    VkClearColorValue color{};
    color.float32[0] = red;
    color.float32[1] = green;
    color.float32[2] = blue;
    color.float32[3] = alpha;
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0u;
    range.levelCount = 1u;
    range.baseArrayLayer = 0u;
    range.layerCount = 1u;
    vkCmdClearColorImage(commands, image_,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1u,
                         &range);
    ok = vkEndCommandBuffer(commands) == VK_SUCCESS;
  }
  if (ok) {
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &commands;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    ok = vkCreateFence(device_->device(), &fence_info, nullptr, &fence) ==
             VK_SUCCESS &&
         fence != VK_NULL_HANDLE;
    if (ok) {
      ok = vkQueueSubmit(device_->queue(), 1u, &submit, fence) == VK_SUCCESS;
      if (ok) {
        ok = vkWaitForFences(device_->device(), 1u, &fence, VK_TRUE,
                             10u * 1000u * 1000u * 1000u) == VK_SUCCESS;
      }
      vkDestroyFence(device_->device(), fence, nullptr);
    } else {
      error_ = "vkCreateFence failed";
    }
  }
  vkFreeCommandBuffers(device_->device(), pool_, 1u, &commands);
  if (!ok && error_.empty()) {
    error_ = "offscreen clear submission failed";
  }
  return ok;
}

std::vector<std::uint8_t> VulkanOffscreenTarget::readback() noexcept {
  error_.clear();
  std::vector<std::uint8_t> pixels;
  if (!valid() || pool_ == VK_NULL_HANDLE) {
    error_ = "readback on an invalid offscreen target";
    return pixels;
  }
  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = pool_;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = 1u;
  VkCommandBuffer commands = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device_->device(), &alloc, &commands) !=
          VK_SUCCESS ||
      commands == VK_NULL_HANDLE) {
    error_ = "vkAllocateCommandBuffers failed";
    return pixels;
  }
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  bool ok = vkBeginCommandBuffer(commands, &begin) == VK_SUCCESS;
  if (ok) {
    if (!ensure_transfer_src()) {
      ok = false;
    }
  }
  if (ok) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0u;
    region.bufferRowLength = 0u;
    region.bufferImageHeight = 0u;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0u;
    region.imageSubresource.baseArrayLayer = 0u;
    region.imageSubresource.layerCount = 1u;
    region.imageOffset.x = 0;
    region.imageOffset.y = 0;
    region.imageOffset.z = 0;
    region.imageExtent.width = width_;
    region.imageExtent.height = height_;
    region.imageExtent.depth = 1u;
    vkCmdCopyImageToBuffer(commands, image_,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_, 1u,
                           &region);
    ok = vkEndCommandBuffer(commands) == VK_SUCCESS;
  }
  if (ok) {
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &commands;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    ok = vkCreateFence(device_->device(), &fence_info, nullptr, &fence) ==
             VK_SUCCESS &&
         fence != VK_NULL_HANDLE;
    if (ok) {
      ok = vkQueueSubmit(device_->queue(), 1u, &submit, fence) == VK_SUCCESS;
      if (ok) {
        ok = vkWaitForFences(device_->device(), 1u, &fence, VK_TRUE,
                             10u * 1000u * 1000u * 1000u) == VK_SUCCESS;
      }
      vkDestroyFence(device_->device(), fence, nullptr);
    }
  }
  vkFreeCommandBuffers(device_->device(), pool_, 1u, &commands);
  if (!ok) {
    if (error_.empty()) {
      error_ = "offscreen readback submission failed";
    }
    return pixels;
  }
  void* mapped = nullptr;
  if (vkMapMemory(device_->device(), staging_memory_, 0u, byte_size_, 0u,
                  &mapped) != VK_SUCCESS ||
      mapped == nullptr) {
    error_ = "vkMapMemory for readback failed";
    return pixels;
  }
  pixels.assign(static_cast<std::uint8_t*>(mapped),
                static_cast<std::uint8_t*>(mapped) + byte_size_);
  vkUnmapMemory(device_->device(), staging_memory_);
  return pixels;
}

VulkanSwapchain::VulkanSwapchain(const VulkanDevice& device,
                                 std::uint32_t image_count) noexcept
    : device_(&device) {
  if (!device.valid()) {
    error_ = "cannot build a swapchain on an invalid device";
    return;
  }
  if (image_count < 2u) {
    error_ = "swapchain needs at least two images";
    return;
  }
  auto create_headless = reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(
      vkGetInstanceProcAddr(device_->instance(), "vkCreateHeadlessSurfaceEXT"));
  if (create_headless == nullptr) {
    error_ = "vkCreateHeadlessSurfaceEXT is unavailable";
    return;
  }
  VkHeadlessSurfaceCreateInfoEXT surface_info{};
  surface_info.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
  if (create_headless(device_->instance(), &surface_info, nullptr,
                      &surface_) != VK_SUCCESS ||
      surface_ == VK_NULL_HANDLE) {
    error_ = "vkCreateHeadlessSurfaceEXT failed";
    surface_ = VK_NULL_HANDLE;
    return;
  }
  if (!init(surface_, true, image_count)) {
    if (surface_ != VK_NULL_HANDLE) {
      vkDestroySurfaceKHR(device_->instance(), surface_, nullptr);
      surface_ = VK_NULL_HANDLE;
    }
  }
}

VulkanSwapchain::VulkanSwapchain(const VulkanDevice& device,
                                 VkSurfaceKHR surface,
                                 std::uint32_t image_count) noexcept
    : device_(&device) {
  if (!device.valid()) {
    error_ = "cannot build a swapchain on an invalid device";
    return;
  }
  if (surface == VK_NULL_HANDLE) {
    error_ = "cannot build a swapchain on a null surface";
    return;
  }
  if (image_count < 2u) {
    error_ = "swapchain needs at least two images";
    return;
  }
  surface_ = surface;
  init(surface_, false, image_count);
}

bool VulkanSwapchain::init(VkSurfaceKHR surface, bool owns_surface,
                           std::uint32_t image_count) noexcept {
  owns_surface_ = owns_surface;
  VkBool32 supported = VK_FALSE;
  if (vkGetPhysicalDeviceSurfaceSupportKHR(device_->physical_device(),
                                           device_->queue_family(), surface_,
                                           &supported) != VK_SUCCESS ||
      supported == VK_FALSE) {
    error_ = "headless surface presentation is unsupported on this queue";
    return false;
  }
  VkSurfaceCapabilitiesKHR capabilities{};
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_->physical_device(),
                                                surface_,
                                                &capabilities) != VK_SUCCESS) {
    error_ = "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed";
    return false;
  }
  std::uint32_t format_count = 0u;
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(device_->physical_device(), surface_,
                                           &format_count, nullptr) !=
          VK_SUCCESS ||
      format_count == 0u) {
    error_ = "no swapchain surface formats available";
    return false;
  }
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(device_->physical_device(), surface_,
                                           &format_count,
                                           formats.data()) != VK_SUCCESS) {
    error_ = "vkGetPhysicalDeviceSurfaceFormatsKHR failed";
    return false;
  }
  std::uint32_t present_count = 0u;
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(device_->physical_device(),
                                                surface_, &present_count,
                                                nullptr) != VK_SUCCESS ||
      present_count == 0u) {
    error_ = "no swapchain present modes available";
    return false;
  }
  std::vector<VkPresentModeKHR> modes(present_count);
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(device_->physical_device(),
                                                surface_, &present_count,
                                                modes.data()) != VK_SUCCESS) {
    error_ = "vkGetPhysicalDeviceSurfacePresentModesKHR failed";
    return false;
  }
  VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
  bool have_mode = false;
  for (const VkPresentModeKHR candidate : modes) {
    if (candidate == VK_PRESENT_MODE_FIFO_KHR) {
      have_mode = true;
      break;
    }
  }
  if (!have_mode) {
    mode = modes[0];
  }
  VkExtent2D extent{device_->width(), device_->height()};
  if (capabilities.currentExtent.width != UINT32_MAX) {
    extent = capabilities.currentExtent;
  } else {
    if (extent.width < capabilities.minImageExtent.width ||
        extent.width > capabilities.maxImageExtent.width ||
        extent.height < capabilities.minImageExtent.height ||
        extent.height > capabilities.maxImageExtent.height) {
      error_ = "swapchain extent is outside the supported range";
      return false;
    }
  }
  std::uint32_t wanted = image_count;
  if (wanted < capabilities.minImageCount) {
    wanted = capabilities.minImageCount;
  }
  if (capabilities.maxImageCount != 0u && wanted > capabilities.maxImageCount) {
    wanted = capabilities.maxImageCount;
  }
  VkSwapchainCreateInfoKHR create{};
  create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create.surface = surface_;
  create.minImageCount = wanted;
  format_ = formats[0].format;
  create.imageFormat = format_;
  create.imageColorSpace = formats[0].colorSpace;  create.imageExtent = extent;
  create.imageArrayLayers = 1u;
  create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create.preTransform = capabilities.currentTransform;
  create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create.presentMode = mode;
  create.clipped = VK_TRUE;
  if (vkCreateSwapchainKHR(device_->device(), &create, nullptr, &swapchain_) !=
          VK_SUCCESS ||
      swapchain_ == VK_NULL_HANDLE) {
    error_ = "vkCreateSwapchainKHR failed";
    swapchain_ = VK_NULL_HANDLE;
    return false;
  }
  std::uint32_t actual = 0u;
  if (vkGetSwapchainImagesKHR(device_->device(), swapchain_, &actual,
                              nullptr) != VK_SUCCESS ||
      actual == 0u) {
    error_ = "vkGetSwapchainImagesKHR failed";
    return false;
  }
  images_.resize(actual);
  if (vkGetSwapchainImagesKHR(device_->device(), swapchain_, &actual,
                              images_.data()) != VK_SUCCESS) {
    error_ = "vkGetSwapchainImagesKHR failed";
    images_.clear();
    return false;
  }
  // Every swapchain image starts UNDEFINED; presenting requires
  // PRESENT_SRC_KHR, so transition each image once up front. The layout
  // survives the acquire/present cycle.
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  pool_info.queueFamilyIndex = device_->queue_family();
  VkCommandPool pool = VK_NULL_HANDLE;
  if (vkCreateCommandPool(device_->device(), &pool_info, nullptr, &pool) !=
          VK_SUCCESS ||
      pool == VK_NULL_HANDLE) {
    error_ = "vkCreateCommandPool for present transitions failed";
    return false;
  }
  bool transitioned = true;
  for (const VkImage image : images_) {
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1u;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    transitioned =
        vkAllocateCommandBuffers(device_->device(), &alloc, &commands) ==
            VK_SUCCESS &&
        commands != VK_NULL_HANDLE;
    if (!transitioned) {
      break;
    }
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    transitioned = vkBeginCommandBuffer(commands, &begin) == VK_SUCCESS;
    if (transitioned) {
      VkImageMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = image;
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.subresourceRange.baseMipLevel = 0u;
      barrier.subresourceRange.levelCount = 1u;
      barrier.subresourceRange.baseArrayLayer = 0u;
      barrier.subresourceRange.layerCount = 1u;
      barrier.srcAccessMask = 0u;
      barrier.dstAccessMask = 0u;
      vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u,
                           nullptr, 0u, nullptr, 1u, &barrier);
      transitioned = vkEndCommandBuffer(commands) == VK_SUCCESS;
    }
    if (transitioned) {
      VkSubmitInfo submit{};
      submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submit.commandBufferCount = 1u;
      submit.pCommandBuffers = &commands;
      VkFenceCreateInfo fence_info{};
      fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      VkFence fence = VK_NULL_HANDLE;
      transitioned =
          vkCreateFence(device_->device(), &fence_info, nullptr, &fence) ==
              VK_SUCCESS &&
          fence != VK_NULL_HANDLE;
      if (transitioned) {
        transitioned =
            vkQueueSubmit(device_->queue(), 1u, &submit, fence) == VK_SUCCESS;
        if (transitioned) {
          transitioned =
              vkWaitForFences(device_->device(), 1u, &fence, VK_TRUE,
                              10u * 1000u * 1000u * 1000u) == VK_SUCCESS;
        }
        vkDestroyFence(device_->device(), fence, nullptr);
      }
    }
    vkFreeCommandBuffers(device_->device(), pool, 1u, &commands);
    if (!transitioned) {
      break;
    }
  }
  vkDestroyCommandPool(device_->device(), pool, nullptr);
  if (!transitioned) {
    error_ = "swapchain present-layout transition failed";
    images_.clear();
    layouts_.clear();
    vkDestroySwapchainKHR(device_->device(), swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
    return false;
  }
  layouts_.assign(images_.size(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  VkCommandPoolCreateInfo swap_pool{};
  swap_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  swap_pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  swap_pool.queueFamilyIndex = device_->queue_family();
  if (vkCreateCommandPool(device_->device(), &swap_pool, nullptr, &pool_) !=
          VK_SUCCESS ||
      pool_ == VK_NULL_HANDLE) {
    error_ = "vkCreateCommandPool for swapchain blits failed";
    pool_ = VK_NULL_HANDLE;
    images_.clear();
    layouts_.clear();
    vkDestroySwapchainKHR(device_->device(), swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
    return false;
  }
  return true;
}

VulkanSwapchain::~VulkanSwapchain() noexcept {
  if (device_ == nullptr || !device_->valid()) {
    return;
  }
  if (pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_->device(), pool_, nullptr);
  }
  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device_->device(), swapchain_, nullptr);
  }
  if (owns_surface_ && surface_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(device_->instance(), surface_, nullptr);
  }
}

bool VulkanSwapchain::acquire(std::uint32_t& index) noexcept {
  error_.clear();
  index = UINT32_MAX;
  if (!valid()) {
    error_ = "acquire on an invalid swapchain";
    return false;
  }
  const VkResult result = vkAcquireNextImageKHR(
      device_->device(), swapchain_, 2u * 1000u * 1000u * 1000u,
      VK_NULL_HANDLE, VK_NULL_HANDLE, &index);
  if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
    return true;
  }
  if (result == VK_TIMEOUT || result == VK_NOT_READY) {
    error_ = "no swapchain image became available in time";
    return false;
  }
  error_ = "vkAcquireNextImageKHR failed";
  return false;
}

bool VulkanSwapchain::present(std::uint32_t index) noexcept {
  error_.clear();
  if (!valid()) {
    error_ = "present on an invalid swapchain";
    return false;
  }
  if (index >= images_.size()) {
    error_ = "present image index is out of range";
    return false;
  }
  VkPresentInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.swapchainCount = 1u;
  info.pSwapchains = &swapchain_;
  info.pImageIndices = &index;
  const VkResult result = vkQueuePresentKHR(device_->queue(), &info);
  if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
    ++presents_;
    return true;
  }
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    error_ = "swapchain is out of date and needs recreation";
    return false;
  }
  error_ = "vkQueuePresentKHR failed";
  return false;
}

bool VulkanSwapchain::present_offscreen(VulkanOffscreenTarget& source,
                                        std::uint32_t index) noexcept {
  error_.clear();
  if (!valid() || pool_ == VK_NULL_HANDLE) {
    error_ = "present_offscreen on an invalid swapchain";
    return false;
  }
  if (index >= images_.size() || index >= layouts_.size()) {
    error_ = "present_offscreen image index is out of range";
    return false;
  }
  if (layouts_[index] != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    error_ = "swapchain image is not in PRESENT_SRC layout";
    return false;
  }
  if (!source.valid() || source.image() == VK_NULL_HANDLE ||
      source.width() != device_->width() ||
      source.height() != device_->height()) {
    error_ = "present_offscreen source does not match the swapchain device";
    return false;
  }
  if (!source.ensure_transfer_src()) {
    error_ = "present_offscreen source not readable: " + source.error();
    return false;
  }
  VkFormatProperties src_props{};
  vkGetPhysicalDeviceFormatProperties(device_->physical_device(),
                                      VK_FORMAT_R8G8B8A8_UNORM, &src_props);
  VkFormatProperties dst_props{};
  vkGetPhysicalDeviceFormatProperties(device_->physical_device(), format_,
                                      &dst_props);
  if ((src_props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ==
          0u ||
      (dst_props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) ==
          0u) {
    error_ = "blit between offscreen and swapchain formats is unsupported";
    return false;
  }
  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = pool_;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = 1u;
  VkCommandBuffer commands = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device_->device(), &alloc, &commands) !=
          VK_SUCCESS ||
      commands == VK_NULL_HANDLE) {
    error_ = "vkAllocateCommandBuffers failed";
    return false;
  }
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  bool ok = vkBeginCommandBuffer(commands, &begin) == VK_SUCCESS;
  if (ok) {
    VkImageMemoryBarrier to_dst{};
    to_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_dst.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.image = images_[index];
    to_dst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_dst.subresourceRange.baseMipLevel = 0u;
    to_dst.subresourceRange.levelCount = 1u;
    to_dst.subresourceRange.baseArrayLayer = 0u;
    to_dst.subresourceRange.layerCount = 1u;
    to_dst.srcAccessMask = 0u;
    to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u,
                         nullptr, 1u, &to_dst);
    VkImageBlit blit{};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = 0u;
    blit.srcSubresource.baseArrayLayer = 0u;
    blit.srcSubresource.layerCount = 1u;
    blit.srcOffsets[1].x = static_cast<std::int32_t>(source.width());
    blit.srcOffsets[1].y = static_cast<std::int32_t>(source.height());
    blit.srcOffsets[1].z = 1;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = 0u;
    blit.dstSubresource.baseArrayLayer = 0u;
    blit.dstSubresource.layerCount = 1u;
    blit.dstOffsets[1].x = static_cast<std::int32_t>(device_->width());
    blit.dstOffsets[1].y = static_cast<std::int32_t>(device_->height());
    blit.dstOffsets[1].z = 1;
    vkCmdBlitImage(commands, source.image(),
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, images_[index],
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &blit,
                   VK_FILTER_LINEAR);
    VkImageMemoryBarrier to_present{};
    to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.image = images_[index];
    to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_present.subresourceRange.baseMipLevel = 0u;
    to_present.subresourceRange.levelCount = 1u;
    to_present.subresourceRange.baseArrayLayer = 0u;
    to_present.subresourceRange.layerCount = 1u;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_present.dstAccessMask = 0u;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, nullptr,
                         0u, nullptr, 1u, &to_present);
    ok = vkEndCommandBuffer(commands) == VK_SUCCESS;
  }
  if (ok) {
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &commands;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    ok = vkCreateFence(device_->device(), &fence_info, nullptr, &fence) ==
             VK_SUCCESS &&
         fence != VK_NULL_HANDLE;
    if (ok) {
      ok = vkQueueSubmit(device_->queue(), 1u, &submit, fence) == VK_SUCCESS;
      if (ok) {
        ok = vkWaitForFences(device_->device(), 1u, &fence, VK_TRUE,
                             10u * 1000u * 1000u * 1000u) == VK_SUCCESS;
      }
      vkDestroyFence(device_->device(), fence, nullptr);
    }
  }
  vkFreeCommandBuffers(device_->device(), pool_, 1u, &commands);
  if (!ok) {
    if (error_.empty()) {
      error_ = "present_offscreen blit submission failed";
    }
    return false;
  }
  return present(index);
}

std::vector<std::uint8_t> VulkanSwapchain::readback_image(
    std::uint32_t index) noexcept {
  error_.clear();
  std::vector<std::uint8_t> pixels;
  if (!valid() || pool_ == VK_NULL_HANDLE) {
    error_ = "readback_image on an invalid swapchain";
    return pixels;
  }
  if (index >= images_.size() || index >= layouts_.size()) {
    error_ = "readback_image index is out of range";
    return pixels;
  }
  if (layouts_[index] != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    error_ = "swapchain image is not in PRESENT_SRC layout";
    return pixels;
  }
  const std::size_t byte_size = static_cast<std::size_t>(device_->width()) *
                                static_cast<std::size_t>(device_->height()) *
                                4u;
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = byte_size;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VkBuffer staging = VK_NULL_HANDLE;
  if (vkCreateBuffer(device_->device(), &buffer_info, nullptr, &staging) !=
          VK_SUCCESS ||
      staging == VK_NULL_HANDLE) {
    error_ = "vkCreateBuffer for swapchain readback failed";
    return pixels;
  }
  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_->device(), staging, &requirements);
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(device_->physical_device(), &properties);
  std::uint32_t type = UINT32_MAX;
  for (std::uint32_t i = 0u; i < properties.memoryTypeCount; ++i) {
    if ((requirements.memoryTypeBits & (1u << i)) != 0u &&
        (properties.memoryTypes[i].propertyFlags &
         (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
      type = i;
      break;
    }
  }
  VkDeviceMemory memory = VK_NULL_HANDLE;
  bool ok = type != UINT32_MAX;
  if (ok) {
    VkMemoryAllocateInfo alloc_mem{};
    alloc_mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_mem.allocationSize = requirements.size;
    alloc_mem.memoryTypeIndex = type;
    ok = vkAllocateMemory(device_->device(), &alloc_mem, nullptr, &memory) ==
         VK_SUCCESS;
  } else {
    error_ = "no host-visible memory for swapchain readback";
  }
  if (ok) {
    ok = vkBindBufferMemory(device_->device(), staging, memory, 0u) ==
         VK_SUCCESS;
  }
  VkCommandBuffer commands = VK_NULL_HANDLE;
  if (ok) {
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = pool_;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1u;
    ok = vkAllocateCommandBuffers(device_->device(), &alloc, &commands) ==
             VK_SUCCESS &&
         commands != VK_NULL_HANDLE;
  }
  if (ok) {
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    ok = vkBeginCommandBuffer(commands, &begin) == VK_SUCCESS;
  }
  if (ok) {
    VkImageMemoryBarrier to_src{};
    to_src.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_src.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_src.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_src.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_src.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_src.image = images_[index];
    to_src.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_src.subresourceRange.baseMipLevel = 0u;
    to_src.subresourceRange.levelCount = 1u;
    to_src.subresourceRange.baseArrayLayer = 0u;
    to_src.subresourceRange.layerCount = 1u;
    to_src.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    to_src.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u,
                         nullptr, 1u, &to_src);
    VkBufferImageCopy region{};
    region.bufferOffset = 0u;
    region.bufferRowLength = 0u;
    region.bufferImageHeight = 0u;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0u;
    region.imageSubresource.baseArrayLayer = 0u;
    region.imageSubresource.layerCount = 1u;
    region.imageOffset.x = 0;
    region.imageOffset.y = 0;
    region.imageOffset.z = 0;
    region.imageExtent.width = device_->width();
    region.imageExtent.height = device_->height();
    region.imageExtent.depth = 1u;
    vkCmdCopyImageToBuffer(commands, images_[index],
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging, 1u,
                           &region);
    VkImageMemoryBarrier to_present{};
    to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.image = images_[index];
    to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_present.subresourceRange.baseMipLevel = 0u;
    to_present.subresourceRange.levelCount = 1u;
    to_present.subresourceRange.baseArrayLayer = 0u;
    to_present.subresourceRange.layerCount = 1u;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    to_present.dstAccessMask = 0u;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, nullptr,
                         0u, nullptr, 1u, &to_present);
    ok = vkEndCommandBuffer(commands) == VK_SUCCESS;
  }
  if (ok) {
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &commands;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    ok = vkCreateFence(device_->device(), &fence_info, nullptr, &fence) ==
             VK_SUCCESS &&
         fence != VK_NULL_HANDLE;
    if (ok) {
      ok = vkQueueSubmit(device_->queue(), 1u, &submit, fence) == VK_SUCCESS;
      if (ok) {
        ok = vkWaitForFences(device_->device(), 1u, &fence, VK_TRUE,
                             10u * 1000u * 1000u * 1000u) == VK_SUCCESS;
      }
      vkDestroyFence(device_->device(), fence, nullptr);
    }
  }
  if (commands != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(device_->device(), pool_, 1u, &commands);
  }
  if (ok) {
    void* mapped = nullptr;
    if (vkMapMemory(device_->device(), memory, 0u, byte_size, 0u, &mapped) ==
            VK_SUCCESS &&
        mapped != nullptr) {
      pixels.assign(static_cast<std::uint8_t*>(mapped),
                    static_cast<std::uint8_t*>(mapped) + byte_size);
      vkUnmapMemory(device_->device(), memory);
    } else {
      error_ = "vkMapMemory for swapchain readback failed";
      ok = false;
    }
  } else if (error_.empty()) {
    error_ = "swapchain readback submission failed";
  }
  if (staging != VK_NULL_HANDLE) {
    vkDestroyBuffer(device_->device(), staging, nullptr);
  }
  if (memory != VK_NULL_HANDLE) {
    vkFreeMemory(device_->device(), memory, nullptr);
  }
  return pixels;
}

VulkanXcbWindow::VulkanXcbWindow(std::uint32_t width,
                                 std::uint32_t height) noexcept {
  if (width == 0u || height == 0u) {
    error_ = "XCB window dimensions must be nonzero";
    return;
  }
  int screen_number = 0;
  connection_ = xcb_connect(nullptr, &screen_number);
  if (connection_ == nullptr || xcb_connection_has_error(connection_) != 0) {
    error_ = "xcb_connect failed (no X server at DISPLAY?)";
    if (connection_ != nullptr) {
      xcb_disconnect(connection_);
      connection_ = nullptr;
    }
    return;
  }
  const xcb_setup_t* setup = xcb_get_setup(connection_);
  xcb_screen_iterator_t iterator = xcb_setup_roots_iterator(setup);
  for (int index = 0; index < screen_number; ++index) {
    xcb_screen_next(&iterator);
  }
  if (iterator.rem == 0) {
    error_ = "XCB screen is unavailable";
    return;
  }
  const xcb_screen_t* screen = iterator.data;
  window_ = xcb_generate_id(connection_);
  const std::uint32_t values[2] = {screen->white_pixel,
                                   XCB_EVENT_MASK_EXPOSURE};
  xcb_create_window(connection_, XCB_COPY_FROM_PARENT, window_, screen->root,
                    0, 0, static_cast<std::uint16_t>(width),
                    static_cast<std::uint16_t>(height), 0u,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                    XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
  xcb_map_window(connection_, window_);
  xcb_flush(connection_);
  if (xcb_connection_has_error(connection_) != 0) {
    error_ = "XCB window creation failed";
    window_ = 0u;
    return;
  }
}

VulkanXcbWindow::~VulkanXcbWindow() noexcept {
  if (connection_ == nullptr) {
    return;
  }
  if (window_ != 0u) {
    xcb_destroy_window(connection_, window_);
    xcb_flush(connection_);
  }
  xcb_disconnect(connection_);
}

}  // namespace ac6::native
