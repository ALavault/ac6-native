#pragma once

#include "ac6/render_scene.h"
#include "ac6/vulkan_backend.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace ac6 {

// Bindings are owned by the caller and remain valid for the duration of one
// scene submission. Stable IDs come from RenderScene; handles are persistent
// Vulkan resources, never guest pointers or per-frame staging allocations.
struct VulkanSceneMeshBinding final {
  std::string_view mesh_id;
  VulkanMeshHandle mesh{};
  std::uint32_t index_count{};
};

struct VulkanSceneMaterialBinding final {
  std::string_view material_id;
  VulkanPipelineHandle pipeline{};
  VulkanPipelineState state{};
  bool textures_resolved{};
};

class VulkanSceneRenderer final {
 public:
  explicit VulkanSceneRenderer(VulkanBackend& backend) noexcept : backend_(backend) {}

  VulkanSceneRenderer(const VulkanSceneRenderer&) = delete;
  VulkanSceneRenderer& operator=(const VulkanSceneRenderer&) = delete;

  // Submits a bounded headless scene directly to Vulkan. Unsupported scene
  // features fail explicitly; there is no NativeRenderTarget/CPU fallback.
  [[nodiscard]] bool render(
      const RenderScene& scene, VulkanRenderTargetHandle target,
      std::span<const VulkanSceneMeshBinding> meshes,
      std::span<const VulkanSceneMaterialBinding> materials) noexcept;

 private:
  VulkanBackend& backend_;
};

}  // namespace ac6
