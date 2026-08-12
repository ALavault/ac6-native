#include "ac6/sdl_input.h"

#include <cstdio>
#include <vector>

int main() {
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "sdl_video_init_failed:%s\n", SDL_GetError());
    return 77;
  }
  if (!SDL_Vulkan_LoadLibrary(nullptr)) {
    std::fprintf(stderr, "sdl_vulkan_loader_failed:%s\n", SDL_GetError());
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 77;
  }
  std::vector<const char*> extensions;
  if (!ac6::SdlVulkanSurface::required_instance_extensions(extensions)) {
    std::fprintf(stderr, "sdl_vulkan_extensions_failed:%s\n", SDL_GetError());
    SDL_Vulkan_UnloadLibrary();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 77;
  }
  ac6::VulkanInstance instance;
  if (!instance.create(extensions)) {
    std::fprintf(stderr, "vk_instance_failed\n");
    SDL_Vulkan_UnloadLibrary();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 1;
  }
  ac6::SdlWindow window;
  if (!window.create("ac6-native-vulkan-smoke", 320, 180, true, true)) {
    std::fprintf(stderr, "sdl_vulkan_window_failed:%s\n", SDL_GetError());
    instance.destroy();
    SDL_Vulkan_UnloadLibrary();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 77;
  }
  ac6::SdlVulkanSurface surface;
  if (!surface.create(window, instance.handle())) {
    std::fprintf(stderr, "sdl_vulkan_surface_failed:%s\n", SDL_GetError());
    window.destroy();
    instance.destroy();
    SDL_Vulkan_UnloadLibrary();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 1;
  }
  ac6::VulkanDevice device;
  if (!device.create(instance.handle(), surface.handle())) {
    std::fprintf(stderr, "vk_device_failed\n");
    surface.destroy();
    window.destroy();
    instance.destroy();
    SDL_Vulkan_UnloadLibrary();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 1;
  }
  ac6::VulkanSwapchain swapchain;
  if (!swapchain.create(device, surface.handle(), 320, 180)) {
    std::fprintf(stderr, "vk_swapchain_failed\n");
    device.destroy();
    surface.destroy();
    window.destroy();
    instance.destroy();
    SDL_Vulkan_UnloadLibrary();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 1;
  }
  ac6::VulkanFramePresenter presenter;
  ac6::NativeRenderTarget native_target;
  if (!native_target.resize(64, 32) || !native_target.clear(0xFF163D7Au, 1.0f) ||
      !presenter.create(device, swapchain) || !presenter.persistent_upload_ready() ||
      !presenter.present_clear(0.03f, 0.12f, 0.24f, 1.0f) ||
      !presenter.present_frame(native_target) || !presenter.present_frame(native_target)) {
    std::fprintf(stderr, "vk_present_failed\n");
    presenter.destroy();
    swapchain.destroy();
    device.destroy();
    surface.destroy();
    window.destroy();
    instance.destroy();
    SDL_Vulkan_UnloadLibrary();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 1;
  }
  std::fprintf(stderr, "sdl_vulkan_surface=1 extensions=%zu queue_family=%u swapchain_images=%zu\n",
               extensions.size(), device.queue_family(), swapchain.image_count());
  presenter.destroy();
  swapchain.destroy();
  device.destroy();
  surface.destroy();
  window.destroy();
  instance.destroy();
  SDL_Vulkan_UnloadLibrary();
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  return 0;
}
