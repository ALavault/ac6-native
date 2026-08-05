#pragma once

#include "ac6/product_runtime.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <array>
#include <filesystem>
#include <vector>

namespace ac6 {

struct SdlAxisMapping {
  std::uint8_t pitch_axis{SDL_GAMEPAD_AXIS_LEFTY};
  std::uint8_t roll_axis{SDL_GAMEPAD_AXIS_LEFTX};
  std::uint8_t yaw_axis{SDL_GAMEPAD_AXIS_RIGHTX};
  std::uint8_t throttle_axis{SDL_GAMEPAD_AXIS_RIGHT_TRIGGER};
  bool invert_pitch{true};
  bool invert_roll{false};
  bool invert_yaw{false};
  bool valid() const noexcept;
};

struct SdlKeyboardMapping {
  SDL_Scancode pitch_up{SDL_SCANCODE_W};
  SDL_Scancode pitch_down{SDL_SCANCODE_S};
  SDL_Scancode roll_left{SDL_SCANCODE_A};
  SDL_Scancode roll_right{SDL_SCANCODE_D};
  SDL_Scancode yaw_left{SDL_SCANCODE_Q};
  SDL_Scancode yaw_right{SDL_SCANCODE_E};
  SDL_Scancode throttle_up{SDL_SCANCODE_R};
  SDL_Scancode throttle_down{SDL_SCANCODE_F};
  bool valid() const noexcept;
};

class SdlInputAdapter final {
 public:
  explicit SdlInputAdapter(SdlAxisMapping mapping = {}, SdlKeyboardMapping keyboard = {})
      : mapping_(mapping), keyboard_(keyboard) {}
  bool apply(const SDL_Event& event, InputFrame& frame, std::uint16_t& buttons,
             const InputMappingDatabase& mappings, EntityId subject,
             std::vector<Event>& events) const noexcept;
  void reset(InputFrame& frame, std::uint16_t& buttons) const noexcept;
  const SdlAxisMapping& mapping() const noexcept { return mapping_; }
  const SdlKeyboardMapping& keyboard_mapping() const noexcept { return keyboard_; }

 private:
  SdlAxisMapping mapping_;
  SdlKeyboardMapping keyboard_;
  mutable std::array<bool, 8> held_keys_{};
};

class SdlInputProfile final {
 public:
  static bool load_manifest(const std::filesystem::path& path,
                            SdlAxisMapping& axes,
                            SdlKeyboardMapping& keyboard) noexcept;
};

class SdlEventPump final {
 public:
  SdlEventPump() = default;
  ~SdlEventPump();
  SdlEventPump(const SdlEventPump&) = delete;
  SdlEventPump& operator=(const SdlEventPump&) = delete;
  bool initialize() noexcept;
  void shutdown() noexcept;
  bool pump(const SdlInputAdapter& adapter, InputFrame& frame, std::uint16_t& buttons,
            const InputMappingDatabase& mappings, EntityId subject,
            std::vector<Event>& events, bool& quit) noexcept;
  bool initialized() const noexcept { return initialized_; }

 private:
  void close_gamepads() noexcept;
  bool initialized_{};
  std::vector<SDL_Gamepad*> gamepads_;
};

class SdlAudioDevice final {
 public:
  SdlAudioDevice() = default;
  ~SdlAudioDevice();
  SdlAudioDevice(const SdlAudioDevice&) = delete;
  SdlAudioDevice& operator=(const SdlAudioDevice&) = delete;
  bool initialize(const SDL_AudioSpec& spec = {SDL_AUDIO_F32, 2, 48000}) noexcept;
  bool queue(const void* samples, int bytes) noexcept;
  void shutdown() noexcept;
  bool initialized() const noexcept { return stream_ != nullptr; }

 private:
  SDL_AudioStream* stream_{};
  bool owns_subsystem_{};
};

class SdlWindow final {
 public:
  SdlWindow() = default;
  ~SdlWindow();
  SdlWindow(const SdlWindow&) = delete;
  SdlWindow& operator=(const SdlWindow&) = delete;
  bool create(const char* title, std::uint32_t width, std::uint32_t height,
              bool vulkan, bool hidden) noexcept;
  bool show() noexcept;
  void destroy() noexcept;
  bool valid() const noexcept { return window_ != nullptr; }
  SDL_Window* native_handle() const noexcept { return window_; }

 private:
  SDL_Window* window_{};
};

class SdlVulkanSurface final {
 public:
  SdlVulkanSurface() = default;
  ~SdlVulkanSurface();
  SdlVulkanSurface(const SdlVulkanSurface&) = delete;
  SdlVulkanSurface& operator=(const SdlVulkanSurface&) = delete;
  static bool required_instance_extensions(std::vector<const char*>& extensions) noexcept;
  bool create(const SdlWindow& window, VkInstance instance) noexcept;
  void destroy() noexcept;
  bool valid() const noexcept { return surface_ != VK_NULL_HANDLE; }
  VkSurfaceKHR handle() const noexcept { return surface_; }

 private:
  VkInstance instance_{VK_NULL_HANDLE};
  VkSurfaceKHR surface_{VK_NULL_HANDLE};
};

class VulkanInstance final {
 public:
  VulkanInstance() = default;
  ~VulkanInstance();
  VulkanInstance(const VulkanInstance&) = delete;
  VulkanInstance& operator=(const VulkanInstance&) = delete;
  bool create(const std::vector<const char*>& extensions) noexcept;
  void destroy() noexcept;
  bool valid() const noexcept { return instance_ != VK_NULL_HANDLE; }
  VkInstance handle() const noexcept { return instance_; }

 private:
  VkInstance instance_{VK_NULL_HANDLE};
};

class VulkanDevice final {
 public:
  VulkanDevice() = default;
  ~VulkanDevice();
  VulkanDevice(const VulkanDevice&) = delete;
  VulkanDevice& operator=(const VulkanDevice&) = delete;
  bool create(VkInstance instance, VkSurfaceKHR surface) noexcept;
  void destroy() noexcept;
  bool valid() const noexcept { return device_ != VK_NULL_HANDLE; }
  VkPhysicalDevice physical_device() const noexcept { return physical_device_; }
  VkDevice device() const noexcept { return device_; }
  VkQueue graphics_queue() const noexcept { return graphics_queue_; }
  std::uint32_t queue_family() const noexcept { return queue_family_; }

 private:
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkDevice device_{VK_NULL_HANDLE};
  VkQueue graphics_queue_{VK_NULL_HANDLE};
  std::uint32_t queue_family_{};
};

class VulkanSwapchain final {
 public:
  VulkanSwapchain() = default;
  ~VulkanSwapchain();
  VulkanSwapchain(const VulkanSwapchain&) = delete;
  VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
  bool create(const VulkanDevice& device, VkSurfaceKHR surface,
              std::uint32_t width, std::uint32_t height) noexcept;
  void destroy() noexcept;
  bool valid() const noexcept { return swapchain_ != VK_NULL_HANDLE && !views_.empty(); }
  VkSwapchainKHR handle() const noexcept { return swapchain_; }
  VkFormat format() const noexcept { return format_; }
  VkExtent2D extent() const noexcept { return extent_; }
  std::size_t image_count() const noexcept { return images_.size(); }
  const std::vector<VkImage>& images() const noexcept { return images_; }

 private:
  VkDevice device_{VK_NULL_HANDLE};
  VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
  VkFormat format_{VK_FORMAT_UNDEFINED};
  VkExtent2D extent_{};
  std::vector<VkImage> images_;
  std::vector<VkImageView> views_;
};

class VulkanFramePresenter final {
 public:
  VulkanFramePresenter() = default;
  ~VulkanFramePresenter();
  VulkanFramePresenter(const VulkanFramePresenter&) = delete;
  VulkanFramePresenter& operator=(const VulkanFramePresenter&) = delete;
  bool create(const VulkanDevice& device, const VulkanSwapchain& swapchain) noexcept;
  bool present_clear(float red, float green, float blue, float alpha) noexcept;
  bool present_frame(const NativeRenderTarget& target) noexcept;
  void destroy() noexcept;
  bool valid() const noexcept { return command_pool_ != VK_NULL_HANDLE; }

 private:
  VkDevice device_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkQueue queue_{VK_NULL_HANDLE};
  VkCommandPool command_pool_{VK_NULL_HANDLE};
  VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
  VkSemaphore image_available_{VK_NULL_HANDLE};
  VkSemaphore render_finished_{VK_NULL_HANDLE};
  VkFence fence_{VK_NULL_HANDLE};
  VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
  VkExtent2D extent_{};
  std::vector<VkImage> images_;
  std::vector<bool> initialized_images_;
};

}  // namespace ac6
