#pragma once

#include "ac6/ntxr_texture.h"
#include "ac6/retail_ndxr_geometry.h"
#include "ac6/vulkan_scene_renderer.h"

#include <cstdint>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ac6 {

// The upload spans are consumed only by build().  VulkanSceneResourceCache
// copies them into persistent device resources; no caller-owned bytes are
// retained after the build returns.
struct VulkanSceneMeshUpload final {
  std::string_view mesh_id;
  std::span<const VulkanVertex> vertices;
  std::span<const std::uint16_t> indices;
};

struct VulkanSceneTexturedMeshUpload final {
  std::string_view mesh_id;
  std::span<const VulkanTexturedVertex> vertices;
  std::span<const std::uint16_t> indices;
};

struct VulkanSceneTextureUpload final {
  std::string_view texture_id;
  std::uint32_t width{};
  std::uint32_t height{};
  std::span<const std::uint8_t> rgba8;
};

// Qualified Mission 01 adapter output.  The adapter owns the converted
// vectors; cache::build_textured copies them into device resources and never
// retains a retail span.
struct VulkanMission01TexturedUpload final {
  std::string mesh_id;
  std::vector<VulkanTexturedVertex> vertices;
  std::vector<std::uint16_t> indices;
  std::string texture_id;
  std::uint32_t texture_width{};
  std::uint32_t texture_height{};
  std::vector<std::uint8_t> rgba8;
};

[[nodiscard]] std::optional<VulkanMission01TexturedUpload>
make_vulkan_mission01_textured_upload(
    std::string_view mesh_id, std::string_view texture_id,
    std::span<const retail::NdxrPosition> positions,
    std::span<const retail::NdxrTexcoord> texcoords,
    std::span<const std::uint16_t> indices,
    const retail::DecodedTexture& texture) noexcept;

struct VulkanSceneMaterialUpload final {
  std::string_view material_id;
  std::span<const std::uint32_t> vertex_spirv;
  std::span<const std::uint32_t> fragment_spirv;
  VulkanPipelineState state{};
  bool textures_resolved{};
};

struct VulkanSceneTexturedMaterialUpload final {
  std::string_view material_id;
  std::span<const std::uint32_t> vertex_spirv;
  std::span<const std::uint32_t> fragment_spirv;
  VulkanPipelineState state{};
  bool textures_resolved{};
};

class VulkanSceneResourceCache final {
 public:
  explicit VulkanSceneResourceCache(VulkanBackend& backend) noexcept;
  VulkanSceneResourceCache(const VulkanSceneResourceCache&) = delete;
  VulkanSceneResourceCache& operator=(const VulkanSceneResourceCache&) = delete;
  ~VulkanSceneResourceCache();

  // Build is a transaction.  On failure no partially-created mesh or
  // pipeline remains live.  Upload bytes are copied into host-visible Vulkan
  // buffers once; render() never allocates or uploads resources.
  [[nodiscard]] bool build(
      const RenderScene& scene, VulkanRenderTargetHandle target,
      std::span<const VulkanSceneMeshUpload> meshes,
      std::span<const VulkanSceneMaterialUpload> materials) noexcept;

  [[nodiscard]] bool build_textured(
      const RenderScene& scene, VulkanRenderTargetHandle target,
      std::span<const VulkanSceneTexturedMeshUpload> meshes,
      std::span<const VulkanSceneTexturedMaterialUpload> materials,
      std::span<const VulkanSceneTextureUpload> textures) noexcept;

  [[nodiscard]] bool render(const RenderScene& scene) noexcept;
  void reset() noexcept;

  [[nodiscard]] bool ready() const noexcept { return ready_; }
  [[nodiscard]] VulkanRenderTargetHandle target() const noexcept {
    return target_;
  }
  [[nodiscard]] std::size_t live_mesh_count() const noexcept;
  [[nodiscard]] std::size_t live_pipeline_count() const noexcept;
  [[nodiscard]] std::size_t live_texture_count() const noexcept;

 private:
  VulkanBackend& backend_;
  VulkanSceneRenderer renderer_;
  VulkanRenderTargetHandle target_{};
  std::vector<std::string> mesh_ids_;
  std::vector<std::string> material_ids_;
  std::vector<VulkanSceneMeshBinding> mesh_bindings_;
  std::vector<VulkanSceneMaterialBinding> material_bindings_;
  std::vector<std::string> texture_ids_;
  std::vector<VulkanSceneTexturedMeshBinding> textured_mesh_bindings_;
  std::vector<VulkanSceneTexturedMaterialBinding> textured_material_bindings_;
  std::vector<VulkanSceneTextureBinding> texture_bindings_;
  std::array<std::uint8_t, 32> scene_digest_{};
  bool ready_{};
  bool textured_mode_{};
};

}  // namespace ac6
