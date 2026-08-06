#include "ac6/interactive.h"

#include "ac6/sdl_input.h"

#include <SDL3/SDL.h>

#include <vector>

namespace ac6::interactive {
namespace {

class SdlVideoRuntime final {
 public:
  SdlVideoRuntime() = default;
  SdlVideoRuntime(const SdlVideoRuntime&) = delete;
  SdlVideoRuntime& operator=(const SdlVideoRuntime&) = delete;

  ~SdlVideoRuntime() {
    if (vulkan_loaded_) SDL_Vulkan_UnloadLibrary();
    if (video_initialized_) SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }

  bool initialize() noexcept {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return false;
    video_initialized_ = true;
    if (!SDL_Vulkan_LoadLibrary(nullptr)) return false;
    vulkan_loaded_ = true;
    return true;
  }

 private:
  bool video_initialized_{};
  bool vulkan_loaded_{};
};

}  // namespace

bool present_target(const ac6::NativeRenderTarget& target) {
  SdlVideoRuntime runtime;
  if (!runtime.initialize()) return false;

  std::vector<const char*> extensions;
  if (!ac6::SdlVulkanSurface::required_instance_extensions(extensions)) return false;

  ac6::VulkanInstance instance;
  ac6::SdlWindow window;
  ac6::SdlVulkanSurface surface;
  ac6::VulkanDevice device;
  ac6::VulkanSwapchain swapchain;
  ac6::VulkanFramePresenter presenter;

  bool ok = instance.create(extensions) &&
            window.create("ac6-native", 640, 360, true, false) &&
            surface.create(window, instance.handle()) &&
            device.create(instance.handle(), surface.handle()) &&
            swapchain.create(device, surface.handle(), 640, 360) &&
            presenter.create(device, swapchain) &&
            presenter.present_frame(target);

  SDL_Event event{};
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) ok = false;
  }
  return ok;
}

}  // namespace ac6::interactive
