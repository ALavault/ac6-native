#pragma once

// Diagnostic-only. The panel/reticle geometry below is hand-picked pixel
// coordinates, not derived from a retail draw call; see
// reports/ac6-native-visual-shader-hud-20260829.md for what it validates
// and, explicitly, what it does not. It must not be used as a source of
// HUD pixel data in a gated delivery path (CLAUDE.md, HUD rule).

#include "ac6/render_scene.h"
#include "ac6/vulkan_backend.h"

#include <cstdint>
#include <vector>

namespace ac6 {

// Screen-space HUD pass for direct Vulkan presentation. The world renderer
// rejects unqualified scene HUD packets; this pass keeps the native HUD visible
// without introducing guest coordinates or a CPU interactive raster.
class NativeHudGpuOverlay final {
 public:
  NativeHudGpuOverlay() = default;
  NativeHudGpuOverlay(const NativeHudGpuOverlay&) = delete;
  NativeHudGpuOverlay& operator=(const NativeHudGpuOverlay&) = delete;
  ~NativeHudGpuOverlay();

  bool initialize(VulkanBackend& backend, VulkanRenderTargetHandle target,
                  std::uint32_t width, std::uint32_t height) noexcept;
  bool render(const SimulationSnapshot& snapshot,
              bool target_marker_visible) noexcept;
  void reset() noexcept;

 private:
  VulkanBackend* backend_{};
  VulkanRenderTargetHandle target_{};
  VulkanPipelineHandle pipeline_{};
  std::vector<VulkanClipTexturedMeshHandle> meshes_;
  std::vector<VulkanTextureHandle> textures_;
};

}  // namespace ac6
