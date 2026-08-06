#include "ac6/interactive.h"
#include "ac6/sdl_input.h"
#include <SDL3/SDL.h>
#include <vector>
namespace ac6::interactive {
bool present_target(const ac6::NativeRenderTarget& target) {
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO) || !SDL_Vulkan_LoadLibrary(nullptr)) return false;
  std::vector<const char*> extensions;
  if (!ac6::SdlVulkanSurface::required_instance_extensions(extensions)) {
    SDL_Vulkan_UnloadLibrary(); SDL_QuitSubSystem(SDL_INIT_VIDEO); return false;
  }
  ac6::VulkanInstance instance; ac6::SdlWindow window; ac6::SdlVulkanSurface surface;
  ac6::VulkanDevice device; ac6::VulkanSwapchain swapchain; ac6::VulkanFramePresenter presenter;
  bool ok = instance.create(extensions) && window.create("ac6-native", 640, 360, true, false) &&
    surface.create(window, instance.handle()) && device.create(instance.handle(), surface.handle()) &&
    swapchain.create(device, surface.handle(), 640, 360) && presenter.create(device, swapchain) &&
    presenter.present_frame(target);
  SDL_Event event{}; while (SDL_PollEvent(&event)) if (event.type == SDL_EVENT_QUIT) ok = false;
  presenter.destroy(); swapchain.destroy(); device.destroy(); surface.destroy(); window.destroy(); instance.destroy();
  SDL_Vulkan_UnloadLibrary(); SDL_QuitSubSystem(SDL_INIT_VIDEO); return ok;
}
}
