#include "ac6/sdl_input.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>

namespace ac6 {

bool SdlInputProfile::load_manifest(const std::filesystem::path& path,
                                    SdlAxisMapping& axes,
                                    SdlKeyboardMapping& keyboard) noexcept {
  std::ifstream input(path);
  if (!input) return false;
  SdlAxisMapping parsed_axes{};
  SdlKeyboardMapping parsed_keyboard{};
  std::uint16_t seen = 0;
  std::string line;
  auto parse_uint = [](std::string_view value, std::uint32_t& out) {
    if (value.empty()) return false;
    const char* first = value.data();
    const char* last = first + value.size();
    auto result = std::from_chars(first, last, out, 10);
    return result.ec == std::errc{} && result.ptr == last;
  };
  while (std::getline(input, line)) {
    std::string_view row(line);
    if (!row.empty() && row.back() == '\r') row.remove_suffix(1);
    const auto first = row.find_first_not_of(" \t");
    if (first == std::string_view::npos || row[first] == '#') continue;
    row.remove_prefix(first);
    const auto tab = row.find('\t');
    if (tab == std::string_view::npos || row.find('\t', tab + 1) != std::string_view::npos) return false;
    const std::string_view key = row.substr(0, tab);
    const std::string_view value = row.substr(tab + 1);
    std::uint32_t number = 0;
    if (!parse_uint(value, number)) return false;
    std::uint16_t bit = 0;
    if (key == "pitch_axis") { bit = 1u << 0; parsed_axes.pitch_axis = static_cast<std::uint8_t>(number); }
    else if (key == "roll_axis") { bit = 1u << 1; parsed_axes.roll_axis = static_cast<std::uint8_t>(number); }
    else if (key == "yaw_axis") { bit = 1u << 2; parsed_axes.yaw_axis = static_cast<std::uint8_t>(number); }
    else if (key == "throttle_axis") { bit = 1u << 3; parsed_axes.throttle_axis = static_cast<std::uint8_t>(number); }
    else if (key == "invert_pitch") { bit = 1u << 4; if (number > 1) return false; parsed_axes.invert_pitch = number != 0; }
    else if (key == "invert_roll") { bit = 1u << 5; if (number > 1) return false; parsed_axes.invert_roll = number != 0; }
    else if (key == "invert_yaw") { bit = 1u << 6; if (number > 1) return false; parsed_axes.invert_yaw = number != 0; }
    else if (key == "pitch_up") { bit = 1u << 7; parsed_keyboard.pitch_up = static_cast<SDL_Scancode>(number); }
    else if (key == "pitch_down") { bit = 1u << 8; parsed_keyboard.pitch_down = static_cast<SDL_Scancode>(number); }
    else if (key == "roll_left") { bit = 1u << 9; parsed_keyboard.roll_left = static_cast<SDL_Scancode>(number); }
    else if (key == "roll_right") { bit = 1u << 10; parsed_keyboard.roll_right = static_cast<SDL_Scancode>(number); }
    else if (key == "yaw_left") { bit = 1u << 11; parsed_keyboard.yaw_left = static_cast<SDL_Scancode>(number); }
    else if (key == "yaw_right") { bit = 1u << 12; parsed_keyboard.yaw_right = static_cast<SDL_Scancode>(number); }
    else if (key == "throttle_up") { bit = 1u << 13; parsed_keyboard.throttle_up = static_cast<SDL_Scancode>(number); }
    else if (key == "throttle_down") { bit = 1u << 14; parsed_keyboard.throttle_down = static_cast<SDL_Scancode>(number); }
    else return false;
    if ((seen & bit) != 0) return false;
    seen = static_cast<std::uint16_t>(seen | bit);
  }
  if (seen != 0x7fffu || !parsed_axes.valid() || !parsed_keyboard.valid()) return false;
  axes = parsed_axes;
  keyboard = parsed_keyboard;
  return true;
}

bool SdlAxisMapping::valid() const noexcept {
  const auto valid_axis = [](std::uint8_t axis) {
    return axis < SDL_GAMEPAD_AXIS_COUNT;
  };
  return valid_axis(pitch_axis) && valid_axis(roll_axis) && valid_axis(yaw_axis) &&
         valid_axis(throttle_axis);
}

bool SdlKeyboardMapping::valid() const noexcept {
  const SDL_Scancode keys[] = {pitch_up, pitch_down, roll_left, roll_right,
                               yaw_left, yaw_right, throttle_up, throttle_down};
  for (const SDL_Scancode key : keys) {
    if (key == SDL_SCANCODE_UNKNOWN || key >= SDL_SCANCODE_COUNT) return false;
  }
  for (std::size_t i = 0; i < std::size(keys); ++i) {
    for (std::size_t j = i + 1; j < std::size(keys); ++j) {
      if (keys[i] == keys[j]) return false;
    }
  }
  return true;
}

bool SdlInputAdapter::apply(const SDL_Event& event, InputFrame& frame,
                            std::uint16_t& buttons,
                            const InputMappingDatabase& mappings, EntityId subject,
                            std::vector<Event>& events) const noexcept {
  if (!mapping_.valid() || !keyboard_.valid()) return false;
  if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
    const auto axis_value = [](Sint16 value, bool invert) -> std::int16_t {
      const std::int32_t clamped = std::clamp<std::int32_t>(value, -32768, 32767);
      const std::int32_t adjusted = invert ? -clamped : clamped;
      return static_cast<std::int16_t>(std::clamp(adjusted, -32768, 32767));
    };
    if (event.gaxis.axis == mapping_.pitch_axis) frame.pitch = axis_value(event.gaxis.value, mapping_.invert_pitch);
    else if (event.gaxis.axis == mapping_.roll_axis) frame.roll = axis_value(event.gaxis.value, mapping_.invert_roll);
    else if (event.gaxis.axis == mapping_.yaw_axis) frame.yaw = axis_value(event.gaxis.value, mapping_.invert_yaw);
    else if (event.gaxis.axis == mapping_.throttle_axis) {
      const std::int32_t value = std::clamp<std::int32_t>(event.gaxis.value, -32768, 32767);
      frame.throttle = static_cast<std::uint8_t>((value + 32768) * 255 / 65535);
    } else return false;
    return true;
  }
  if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
    const bool pressed = event.type == SDL_EVENT_KEY_DOWN;
    const SDL_Scancode key = event.key.scancode;
    std::size_t key_index = 8;
    if (key == keyboard_.pitch_up) key_index = 0;
    else if (key == keyboard_.pitch_down) key_index = 1;
    else if (key == keyboard_.roll_left) key_index = 2;
    else if (key == keyboard_.roll_right) key_index = 3;
    else if (key == keyboard_.yaw_left) key_index = 4;
    else if (key == keyboard_.yaw_right) key_index = 5;
    else if (key == keyboard_.throttle_up) key_index = 6;
    else if (key == keyboard_.throttle_down) key_index = 7;
    else return false;
    held_keys_[key_index] = pressed;
    const auto signed_axis = [](bool positive, bool negative, std::int16_t positive_value,
                                std::int16_t negative_value) {
      if (positive == negative) return static_cast<std::int16_t>(0);
      return positive ? positive_value : negative_value;
    };
    frame.pitch = signed_axis(held_keys_[0], held_keys_[1], 32767, -32768);
    frame.roll = signed_axis(held_keys_[3], held_keys_[2], 32767, -32768);
    frame.yaw = signed_axis(held_keys_[5], held_keys_[4], 32767, -32768);
    if (held_keys_[6]) frame.throttle = 255u;
    else if (held_keys_[7]) frame.throttle = 0u;
    else frame.throttle = 0u;
    return true;
  }
  if (event.type != SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
      event.type != SDL_EVENT_GAMEPAD_BUTTON_UP) return false;
  if (event.gbutton.button >= 16) return false;
  const std::uint16_t mask = static_cast<std::uint16_t>(1u << event.gbutton.button);
  if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
    buttons = static_cast<std::uint16_t>(buttons | mask);
    const InputBinding* binding = mappings.resolve(buttons);
    if (binding != nullptr) events.push_back({binding->event, subject});
  } else {
    buttons = static_cast<std::uint16_t>(buttons & ~mask);
  }
  return true;
}

void SdlInputAdapter::reset(InputFrame& frame, std::uint16_t& buttons) const noexcept {
  held_keys_.fill(false);
  frame = {};
  buttons = 0;
}

void SdlEventPump::close_gamepads() noexcept {
  for (SDL_Gamepad* gamepad : gamepads_) {
    if (gamepad != nullptr) SDL_CloseGamepad(gamepad);
  }
  gamepads_.clear();
}

SdlEventPump::~SdlEventPump() { shutdown(); }

bool SdlEventPump::initialize() noexcept {
  if (initialized_) return true;
  if (!SDL_InitSubSystem(SDL_INIT_EVENTS)) return false;
  // A headless run may have no gamepad backend; event polling remains valid.
  (void)SDL_InitSubSystem(SDL_INIT_GAMEPAD);
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  if (ids != nullptr) {
    for (int i = 0; i < count; ++i) {
      if (SDL_Gamepad* gamepad = SDL_OpenGamepad(ids[i]); gamepad != nullptr) {
        gamepads_.push_back(gamepad);
      }
    }
    SDL_free(ids);
  }
  initialized_ = true;
  return true;
}

void SdlEventPump::shutdown() noexcept {
  if (!initialized_) return;
  close_gamepads();
  SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
  initialized_ = false;
}

SdlAudioDevice::~SdlAudioDevice() { shutdown(); }

bool SdlAudioDevice::initialize(const SDL_AudioSpec& spec) noexcept {
  if (stream_ != nullptr || spec.format == SDL_AUDIO_UNKNOWN || spec.channels <= 0 ||
      spec.freq <= 0) return false;
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
  owns_subsystem_ = true;
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (stream_ == nullptr || !SDL_ResumeAudioStreamDevice(stream_)) {
    shutdown();
    return false;
  }
  return true;
}

bool SdlAudioDevice::queue(const void* samples, int bytes) noexcept {
  return stream_ != nullptr && samples != nullptr && bytes > 0 &&
         SDL_PutAudioStreamData(stream_, samples, bytes);
}

void SdlAudioDevice::shutdown() noexcept {
  if (stream_ != nullptr) {
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
  }
  if (owns_subsystem_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    owns_subsystem_ = false;
  }
}

bool SdlEventPump::pump(const SdlInputAdapter& adapter, InputFrame& frame,
                        std::uint16_t& buttons,
                        const InputMappingDatabase& mappings, EntityId subject,
                        std::vector<Event>& events, bool& quit) noexcept {
  if (!initialized_) return false;
  bool handled = false;
  SDL_Event event{};
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      quit = true;
      handled = true;
      continue;
    }
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
      if (SDL_Gamepad* gamepad = SDL_OpenGamepad(event.gdevice.which); gamepad != nullptr) {
        gamepads_.push_back(gamepad);
      }
      handled = true;
      continue;
    }
    if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
      for (auto it = gamepads_.begin(); it != gamepads_.end(); ++it) {
        if (*it != nullptr && SDL_GetGamepadID(*it) == event.gdevice.which) {
          SDL_CloseGamepad(*it);
          gamepads_.erase(it);
          break;
        }
      }
      adapter.reset(frame, buttons);
      handled = true;
      continue;
    }
    handled = adapter.apply(event, frame, buttons, mappings, subject, events) || handled;
  }
  return handled;
}

SdlWindow::~SdlWindow() { destroy(); }

bool SdlWindow::create(const char* title, std::uint32_t width, std::uint32_t height,
                       bool vulkan, bool hidden) noexcept {
  if (window_ != nullptr || title == nullptr || width == 0 || height == 0) return false;
  SDL_WindowFlags flags = vulkan ? SDL_WINDOW_VULKAN : 0u;
  if (hidden) flags |= SDL_WINDOW_HIDDEN;
  window_ = SDL_CreateWindow(title, static_cast<int>(width), static_cast<int>(height), flags);
  return window_ != nullptr;
}

bool SdlWindow::show() noexcept {
  return window_ != nullptr && SDL_ShowWindow(window_);
}

void SdlWindow::destroy() noexcept {
  if (window_ == nullptr) return;
  SDL_DestroyWindow(window_);
  window_ = nullptr;
}

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

VulkanSwapchain::~VulkanSwapchain() { destroy(); }

bool VulkanSwapchain::create(const VulkanDevice& device, VkSurfaceKHR surface,
                             std::uint32_t width, std::uint32_t height) noexcept {
  if (!device.valid() || surface == VK_NULL_HANDLE || width == 0 || height == 0 || valid()) return false;
  VkSurfaceCapabilitiesKHR capabilities{};
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical_device(), surface, &capabilities) != VK_SUCCESS) return false;
  std::uint32_t format_count = 0;
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device(), surface, &format_count, nullptr) != VK_SUCCESS || format_count == 0) return false;
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device(), surface, &format_count, formats.data()) != VK_SUCCESS) return false;
  const auto format_it = std::find_if(formats.begin(), formats.end(), [](const VkSurfaceFormatKHR& value) {
    return value.format == VK_FORMAT_B8G8R8A8_UNORM && value.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  });
  const VkSurfaceFormatKHR chosen_format = format_it == formats.end() ? formats.front() : *format_it;
  if (chosen_format.format == VK_FORMAT_UNDEFINED) return false;
  std::uint32_t present_count = 0;
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(device.physical_device(), surface, &present_count, nullptr) != VK_SUCCESS || present_count == 0) return false;
  std::vector<VkPresentModeKHR> present_modes(present_count);
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(device.physical_device(), surface, &present_count, present_modes.data()) != VK_SUCCESS ||
      std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_FIFO_KHR) == present_modes.end()) return false;
  extent_ = capabilities.currentExtent;
  if (extent_.width == std::numeric_limits<std::uint32_t>::max()) {
    extent_.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent_.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
  }
  if (extent_.width == 0 || extent_.height == 0) return false;
  constexpr VkImageUsageFlags kRequiredUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if ((capabilities.supportedUsageFlags & kRequiredUsage) != kRequiredUsage) return false;
  std::uint32_t image_count = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount != 0) image_count = std::min(image_count, capabilities.maxImageCount);
  const VkSwapchainCreateInfoKHR create_info{
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, nullptr, 0, surface, image_count,
      chosen_format.format, chosen_format.colorSpace, extent_, 1, kRequiredUsage,
      VK_SHARING_MODE_EXCLUSIVE, 0, nullptr, capabilities.currentTransform,
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_PRESENT_MODE_FIFO_KHR, VK_TRUE, VK_NULL_HANDLE};
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  if (vkCreateSwapchainKHR(device.device(), &create_info, nullptr, &swapchain) != VK_SUCCESS) return false;
  device_ = device.device();
  swapchain_ = swapchain;
  format_ = chosen_format.format;
  std::uint32_t image_actual_count = 0;
  if (vkGetSwapchainImagesKHR(device_, swapchain_, &image_actual_count, nullptr) != VK_SUCCESS || image_actual_count == 0) {
    destroy();
    return false;
  }
  images_.resize(image_actual_count);
  if (vkGetSwapchainImagesKHR(device_, swapchain_, &image_actual_count, images_.data()) != VK_SUCCESS) {
    destroy();
    return false;
  }
  views_.reserve(images_.size());
  for (const VkImage image : images_) {
    const VkImageViewCreateInfo view_info{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, image, VK_IMAGE_VIEW_TYPE_2D,
        format_, {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                  VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(device_, &view_info, nullptr, &view) != VK_SUCCESS) {
      destroy();
      return false;
    }
    views_.push_back(view);
  }
  return true;
}

void VulkanSwapchain::destroy() noexcept {
  if (device_ == VK_NULL_HANDLE) return;
  for (const VkImageView view : views_) vkDestroyImageView(device_, view, nullptr);
  views_.clear();
  images_.clear();
  if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  swapchain_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
  format_ = VK_FORMAT_UNDEFINED;
  extent_ = {};
}

VulkanFramePresenter::~VulkanFramePresenter() { destroy(); }

bool VulkanFramePresenter::create(const VulkanDevice& device,
                                  const VulkanSwapchain& swapchain) noexcept {
  if (!device.valid() || !swapchain.valid() || valid()) return false;
  device_ = device.device();
  physical_device_ = device.physical_device();
  queue_ = device.graphics_queue();
  swapchain_ = swapchain.handle();
  extent_ = swapchain.extent();
  images_ = swapchain.images();
  initialized_images_.assign(images_.size(), false);
  const VkCommandPoolCreateInfo pool_info{
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, device.queue_family()};
  if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS) {
    destroy();
    return false;
  }
  const VkCommandBufferAllocateInfo allocate_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, command_pool_,
      VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
  if (vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer_) != VK_SUCCESS) {
    destroy();
    return false;
  }
  const VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
  if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_) != VK_SUCCESS ||
      vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_) != VK_SUCCESS) {
    destroy();
    return false;
  }
  const VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
                                     VK_FENCE_CREATE_SIGNALED_BIT};
  if (vkCreateFence(device_, &fence_info, nullptr, &fence_) != VK_SUCCESS) {
    destroy();
    return false;
  }
  return true;
}

bool VulkanFramePresenter::present_frame(const NativeRenderTarget& target) noexcept {
  std::vector<std::uint8_t> source;
  if (!valid() || !target.copy_rgba8(source) || source.empty()) return false;
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(extent_.width) * extent_.height * 4u);
  for (std::uint32_t y = 0; y < extent_.height; ++y) {
    const std::uint32_t source_y = std::min(target.height() - 1u,
        static_cast<std::uint32_t>((static_cast<std::uint64_t>(y) * target.height()) / extent_.height));
    for (std::uint32_t x = 0; x < extent_.width; ++x) {
      const std::uint32_t source_x = std::min(target.width() - 1u,
          static_cast<std::uint32_t>((static_cast<std::uint64_t>(x) * target.width()) / extent_.width));
      const std::size_t source_offset = (static_cast<std::size_t>(source_y) * target.width() + source_x) * 4u;
      const std::size_t destination_offset = (static_cast<std::size_t>(y) * extent_.width + x) * 4u;
      std::memcpy(pixels.data() + destination_offset, source.data() + source_offset, 4u);
    }
  }
  VkBuffer staging = VK_NULL_HANDLE;
  VkDeviceMemory staging_memory = VK_NULL_HANDLE;
  const auto cleanup = [&]() {
    if (staging_memory != VK_NULL_HANDLE) vkFreeMemory(device_, staging_memory, nullptr);
    if (staging != VK_NULL_HANDLE) vkDestroyBuffer(device_, staging, nullptr);
  };
  const VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0,
                                       pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VK_SHARING_MODE_EXCLUSIVE, 0, nullptr};
  if (vkCreateBuffer(device_, &buffer_info, nullptr, &staging) != VK_SUCCESS) return false;
  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_, staging, &requirements);
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
  std::uint32_t memory_type = memory_properties.memoryTypeCount;
  for (std::uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
    const VkMemoryPropertyFlags flags = memory_properties.memoryTypes[i].propertyFlags;
    if ((requirements.memoryTypeBits & (1u << i)) != 0 &&
        (flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
      memory_type = i;
      break;
    }
  }
  if (memory_type == memory_properties.memoryTypeCount) { cleanup(); return false; }
  const VkMemoryAllocateInfo allocate_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr,
                                           requirements.size, memory_type};
  if (vkAllocateMemory(device_, &allocate_info, nullptr, &staging_memory) != VK_SUCCESS ||
      vkBindBufferMemory(device_, staging, staging_memory, 0) != VK_SUCCESS) {
    cleanup(); return false;
  }
  void* mapped = nullptr;
  if (vkMapMemory(device_, staging_memory, 0, pixels.size(), 0, &mapped) != VK_SUCCESS) {
    cleanup(); return false;
  }
  std::memcpy(mapped, pixels.data(), pixels.size());
  vkUnmapMemory(device_, staging_memory);
  if (vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX) != VK_SUCCESS ||
      vkResetFences(device_, 1, &fence_) != VK_SUCCESS) { cleanup(); return false; }
  std::uint32_t image_index = 0;
  const VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                                  image_available_, VK_NULL_HANDLE, &image_index);
  if ((acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) || image_index >= images_.size() ||
      vkResetCommandBuffer(command_buffer_, 0) != VK_SUCCESS) { cleanup(); return false; }
  const VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
  if (vkBeginCommandBuffer(command_buffer_, &begin_info) != VK_SUCCESS) { cleanup(); return false; }
  const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  if (image_index >= initialized_images_.size()) { cleanup(); return false; }
  const VkImageMemoryBarrier to_transfer{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
      initialized_images_[image_index] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
      images_[image_index], range};
  vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_transfer);
  const VkBufferImageCopy copy_region{0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                      {0, 0, 0}, {extent_.width, extent_.height, 1}};
  vkCmdCopyBufferToImage(command_buffer_, staging, images_[image_index],
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
  const VkImageMemoryBarrier to_present{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, 0,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, images_[image_index], range};
  vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_present);
  if (vkEndCommandBuffer(command_buffer_) != VK_SUCCESS) { cleanup(); return false; }
  const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  const VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, &image_available_,
                                 &wait_stage, 1, &command_buffer_, 1, &render_finished_};
  if (vkQueueSubmit(queue_, 1, &submit_info, fence_) != VK_SUCCESS) { cleanup(); return false; }
  initialized_images_[image_index] = true;
  const VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1,
                                      &render_finished_, 1, &swapchain_, &image_index, nullptr};
  const VkResult presented = vkQueuePresentKHR(queue_, &present_info);
  const bool waited = vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
  cleanup();
  return waited && (presented == VK_SUCCESS || presented == VK_SUBOPTIMAL_KHR);
}

bool VulkanFramePresenter::present_clear(float red, float green, float blue,
                                         float alpha) noexcept {
  if (!valid() || images_.empty() || !std::isfinite(red) || !std::isfinite(green) ||
      !std::isfinite(blue) || !std::isfinite(alpha)) return false;
  if (vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX) != VK_SUCCESS ||
      vkResetFences(device_, 1, &fence_) != VK_SUCCESS) return false;
  std::uint32_t image_index = 0;
  const VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                                  image_available_, VK_NULL_HANDLE, &image_index);
  if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) return false;
  if (image_index >= images_.size() || vkResetCommandBuffer(command_buffer_, 0) != VK_SUCCESS) return false;
  const VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
  if (vkBeginCommandBuffer(command_buffer_, &begin_info) != VK_SUCCESS) return false;
  if (image_index >= initialized_images_.size()) return false;
  const VkImageMemoryBarrier to_transfer{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
      initialized_images_[image_index] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
      images_[image_index], {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_transfer);
  const VkClearColorValue color{{red, green, blue, alpha}};
  const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdClearColorImage(command_buffer_, images_[image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       &color, 1, &range);
  const VkImageMemoryBarrier to_present{
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, 0,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, images_[image_index], range};
  vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_present);
  if (vkEndCommandBuffer(command_buffer_) != VK_SUCCESS) return false;
  const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  const VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, &image_available_,
                                 &wait_stage, 1, &command_buffer_, 1, &render_finished_};
  if (vkQueueSubmit(queue_, 1, &submit_info, fence_) != VK_SUCCESS) return false;
  initialized_images_[image_index] = true;
  const VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1,
                                      &render_finished_, 1, &swapchain_, &image_index, nullptr};
  const VkResult presented = vkQueuePresentKHR(queue_, &present_info);
  return presented == VK_SUCCESS || presented == VK_SUBOPTIMAL_KHR;
}

void VulkanFramePresenter::destroy() noexcept {
  if (device_ == VK_NULL_HANDLE) return;
  vkDeviceWaitIdle(device_);
  if (fence_ != VK_NULL_HANDLE) vkDestroyFence(device_, fence_, nullptr);
  if (render_finished_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, render_finished_, nullptr);
  if (image_available_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, image_available_, nullptr);
  if (command_pool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, command_pool_, nullptr);
  fence_ = VK_NULL_HANDLE;
  render_finished_ = VK_NULL_HANDLE;
  image_available_ = VK_NULL_HANDLE;
  command_buffer_ = VK_NULL_HANDLE;
  command_pool_ = VK_NULL_HANDLE;
  swapchain_ = VK_NULL_HANDLE;
  images_.clear();
  initialized_images_.clear();
  queue_ = VK_NULL_HANDLE;
  physical_device_ = VK_NULL_HANDLE;
  extent_ = {};
  device_ = VK_NULL_HANDLE;
}

}  // namespace ac6
