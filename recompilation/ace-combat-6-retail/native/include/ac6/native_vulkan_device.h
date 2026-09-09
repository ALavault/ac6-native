#pragma once

// RAII Vulkan device plus 1280x720 RGBA8 offscreen target for the native
// product renderer. Headless by construction: instance without layers or
// surface/swapchain extensions, one graphics queue, offscreen image +
// host-visible staging buffer. Every step is fail-closed with error() and
// never falls back to a silent default mapping.
#ifndef VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#endif
#include <vulkan/vulkan.h>

#include <xcb/xcb.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ac6::native {

struct VulkanDeviceDesc final {
  std::uint32_t width{1280u};
  std::uint32_t height{720u};
  // When false, any physical device with a graphics queue is accepted
  // (discrete preferred); when true, only discrete GPUs qualify.
  bool require_discrete{false};
  // When true, VK_KHR_xcb_surface is also enabled at instance creation for
  // visible presentation under X (Xvfb). Default keeps the headless-only
  // contract (surface + headless extensions).
  bool enable_xcb_surface{false};
};

class VulkanDevice final {
 public:
  explicit VulkanDevice(const VulkanDeviceDesc& desc = {}) noexcept;
  VulkanDevice(const VulkanDevice&) = delete;
  VulkanDevice& operator=(const VulkanDevice&) = delete;
  ~VulkanDevice() noexcept;

  [[nodiscard]] bool valid() const noexcept { return device_ != VK_NULL_HANDLE; }
  [[nodiscard]] const std::string& error() const noexcept { return error_; }
  [[nodiscard]] const std::string& device_name() const noexcept {
    return device_name_;
  }
  [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
  [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
  [[nodiscard]] VkDevice device() const noexcept { return device_; }
  [[nodiscard]] VkInstance instance() const noexcept { return instance_; }
  [[nodiscard]] VkPhysicalDevice physical_device() const noexcept {
    return physical_device_;
  }
  [[nodiscard]] std::uint32_t queue_family() const noexcept {
    return queue_family_;
  }
  [[nodiscard]] VkQueue queue() const noexcept { return queue_; }

 private:
  std::string error_;
  std::string device_name_;
  std::uint32_t width_{};
  std::uint32_t height_{};
  VkInstance instance_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  std::uint32_t queue_family_{0u};
  VkDevice device_{VK_NULL_HANDLE};
  VkQueue queue_{VK_NULL_HANDLE};
};

class VulkanOffscreenTarget final {
 public:
  explicit VulkanOffscreenTarget(const VulkanDevice& device) noexcept;
  VulkanOffscreenTarget(const VulkanOffscreenTarget&) = delete;
  VulkanOffscreenTarget& operator=(const VulkanOffscreenTarget&) = delete;
  ~VulkanOffscreenTarget() noexcept;

  [[nodiscard]] bool valid() const noexcept { return image_ != VK_NULL_HANDLE; }
  [[nodiscard]] const std::string& error() const noexcept { return error_; }
  [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
  [[nodiscard]] std::uint32_t height() const noexcept { return height_; }

  // Clears every pixel to (red, green, blue, alpha) and makes the image
  // available for readback. Fails closed on any Vulkan error.
  bool clear(float red, float green, float blue, float alpha) noexcept;
  // Copies the image to host memory as tightly packed RGBA8 rows.
  // Empty on failure (check error()).
  [[nodiscard]] std::vector<std::uint8_t> readback() noexcept;
  [[nodiscard]] VkImage image() const noexcept { return image_; }
  [[nodiscard]] VkImageLayout layout() const noexcept { return layout_; }
  // Moves the image to TRANSFER_SRC_OPTIMAL from DST or SRC (no-op in the
  // latter case). False from any other layout or on barrier failure.
  bool ensure_transfer_src() noexcept;
  // Pinned-draw resolve support (r257): TRANSFER_SRC -> TRANSFER_DST before
  // the EDRAM resolve copy and back afterwards, keeping the tracked layout
  // in sync. Fail-closed on other layouts.
  bool begin_transfer_dst(VkCommandBuffer commands) noexcept;
  bool end_transfer_dst(VkCommandBuffer commands) noexcept;

private:
  bool transition(VkCommandBuffer commands, VkImageLayout next) noexcept;
  [[nodiscard]] std::uint32_t find_memory(std::uint32_t bits,
                                          VkMemoryPropertyFlags flags) const noexcept;

  const VulkanDevice* device_{nullptr};
  std::string error_;
  std::uint32_t width_{};
  std::uint32_t height_{};
  VkImage image_{VK_NULL_HANDLE};
  VkDeviceMemory image_memory_{VK_NULL_HANDLE};
  VkBuffer staging_{VK_NULL_HANDLE};
  VkDeviceMemory staging_memory_{VK_NULL_HANDLE};
  VkCommandPool pool_{VK_NULL_HANDLE};
  VkImageLayout layout_{VK_IMAGE_LAYOUT_UNDEFINED};
  std::size_t byte_size_{0u};
};

// Headless swapchain over VK_EXT_headless_surface: acquire/present plumbing
// against the real driver with no display, XCB, or Wayland dependency.
// Present images are driver-owned; pixel proof stays with the offscreen
// target (blit into a swapchain image is a later stage, not claimed here).
class VulkanSwapchain final {
 public:
  explicit VulkanSwapchain(const VulkanDevice& device,
                           std::uint32_t image_count = 2u) noexcept;
  // Borrowed-surface variant for visible presentation (e.g. XCB under
  // Xvfb): the surface stays owned by the caller and is never destroyed
  // here. Dimensions come from the device descriptor.
  VulkanSwapchain(const VulkanDevice& device, VkSurfaceKHR surface,
                  std::uint32_t image_count = 2u) noexcept;
  VulkanSwapchain(const VulkanSwapchain&) = delete;
  VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
  ~VulkanSwapchain() noexcept;

  [[nodiscard]] bool valid() const noexcept {
    return swapchain_ != VK_NULL_HANDLE;
  }
  [[nodiscard]] const std::string& error() const noexcept { return error_; }
  [[nodiscard]] std::uint32_t image_count() const noexcept {
    return static_cast<std::uint32_t>(images_.size());
  }
  [[nodiscard]] std::uint64_t presents() const noexcept { return presents_; }

  // Acquires the next image; false when none is available within the bound.
  [[nodiscard]] bool acquire(std::uint32_t& index) noexcept;
  // Presents a previously acquired image. False on driver error or when the
  // swapchain needs recreation (out of date is reported, not hidden).
  bool present(std::uint32_t index) noexcept;
  // Blits the offscreen target's pixels into the acquired swapchain image
  // (format-converting when the driver requires it) and presents it.
  // Reading the presented bytes back is the caller's proof (see test).
  bool present_offscreen(VulkanOffscreenTarget& source,
                         std::uint32_t index) noexcept;
  // Copies a swapchain image to host memory as tightly packed rows in the
  // swapchain's own format. Empty on failure. Test/diagnostic proof only.
  [[nodiscard]] std::vector<std::uint8_t> readback_image(
      std::uint32_t index) noexcept;

 private:
  bool init(VkSurfaceKHR surface, bool owns_surface,
            std::uint32_t image_count) noexcept;

  const VulkanDevice* device_{nullptr};
  std::string error_;
  VkSurfaceKHR surface_{VK_NULL_HANDLE};
  bool owns_surface_{true};
  VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
  VkFormat format_{VK_FORMAT_UNDEFINED};
  VkCommandPool pool_{VK_NULL_HANDLE};
  std::vector<VkImage> images_;
  std::vector<VkImageLayout> layouts_;
  std::uint64_t presents_{0u};
};

// Mapped XCB window (typically under Xvfb) for visible presentation.
// Fails closed when DISPLAY is unset or the server is unreachable; the
// caller keeps the window mapped while presenting.
class VulkanXcbWindow final {
 public:
  explicit VulkanXcbWindow(std::uint32_t width = 1280u,
                           std::uint32_t height = 720u) noexcept;
  VulkanXcbWindow(const VulkanXcbWindow&) = delete;
  VulkanXcbWindow& operator=(const VulkanXcbWindow&) = delete;
  ~VulkanXcbWindow() noexcept;

  [[nodiscard]] bool valid() const noexcept { return window_ != 0u; }
  [[nodiscard]] const std::string& error() const noexcept { return error_; }
  [[nodiscard]] xcb_connection_t* connection() const noexcept {
    return connection_;
  }
  [[nodiscard]] xcb_window_t window() const noexcept { return window_; }

 private:
  std::string error_;
  xcb_connection_t* connection_{nullptr};
  xcb_window_t window_{0u};
};

}  // namespace ac6::native
