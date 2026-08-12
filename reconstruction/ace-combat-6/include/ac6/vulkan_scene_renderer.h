#pragma once

#include "ac6/render_scene.h"
#include "ac6/vulkan_backend.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace ac6 {

// Explicit RenderScene sentinel: texture slot 0 is selected by each
// DrawPacket::texture_ids[0], rather than fixed by MaterialPipeline. This is
// the only dynamic texture binding accepted by the bounded world lane.
inline constexpr std::string_view kVulkanDrawPacketTexture0Binding{
    "@vulkan.draw-packet.texture0"};

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

struct VulkanSceneTexturedMeshBinding final {
  std::string_view mesh_id;
  VulkanTexturedMeshHandle mesh{};
  std::uint32_t index_count{};
};

struct VulkanSceneWorldTexturedMeshBinding final {
  std::string_view mesh_id;
  VulkanWorldTexturedMeshHandle mesh{};
  std::uint32_t index_count{};
};

struct VulkanSceneClipTexturedMeshBinding final {
  std::string_view mesh_id;
  VulkanClipTexturedMeshHandle mesh{};
  std::uint32_t index_count{};
};

struct VulkanSceneTextureBinding final {
  std::string_view texture_id;
  VulkanTextureHandle texture{};
};

struct VulkanSceneTexturedMaterialBinding final {
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

  // Bounded textured variant: exactly one RGBA8 texture per draw packet, no
  // transforms/HUD/depth yet. It still submits directly to Vulkan and never
  // falls back to NativeRenderTarget.
  [[nodiscard]] bool render_textured(
      const RenderScene& scene, VulkanRenderTargetHandle target,
      std::span<const VulkanSceneTexturedMeshBinding> meshes,
      std::span<const VulkanSceneTexturedMaterialBinding> materials,
      std::span<const VulkanSceneTextureBinding> textures) noexcept;

  // Bounded projected variant. Vertices are already in homogeneous clip
  // space; the caller owns the object-to-clip proof and no camera is inferred
  // here. This remains a direct Vulkan path with persistent resources only.
  [[nodiscard]] bool render_clip_textured(
      const RenderScene& scene, VulkanRenderTargetHandle target,
      std::span<const VulkanSceneClipTexturedMeshBinding> meshes,
      std::span<const VulkanSceneTexturedMaterialBinding> materials,
      std::span<const VulkanSceneTextureBinding> textures) noexcept;

  // World XYZ is never preprojected on the CPU. A generic Vulkan camera matrix
  // is derived once per submission from RenderScene::camera and composed with
  // each DrawPacket::transform before the 64-byte vertex push. This is not yet
  // a qualification of Mission 01's retail TCAM projection.
  [[nodiscard]] bool render_world_textured(
      const RenderScene& scene, VulkanRenderTargetHandle target,
      std::span<const VulkanSceneWorldTexturedMeshBinding> meshes,
      std::span<const VulkanSceneTexturedMaterialBinding> materials,
      std::span<const VulkanSceneTextureBinding> textures) noexcept;

 private:
  VulkanBackend& backend_;
};

}  // namespace ac6
