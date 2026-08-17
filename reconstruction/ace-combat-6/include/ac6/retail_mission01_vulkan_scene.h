#pragma once

#include "ac6/retail_mission01_map_render_assets.h"
#include "ac6/vulkan_scene_resource_cache.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ac6::retail {

// A bounded, store-backed bridge from one accepted Mission 01 map draw to the
// direct Vulkan scene contract.  It owns all converted bytes and never keeps a
// view into the sealed cache after construction.  The caller supplies the
// object-to-clip proof and the SPIR-V; this class deliberately does not infer a
// camera, a matrix convention, or a retail shader from static asset metadata.
struct RetailMission01VulkanSceneReport final {
  Sha256Digest content_index_sha256{};
  std::uint32_t draw_instance_index{};
  std::uint16_t selector{};
  std::uint16_t record_index{};
  std::uint32_t texture_identifier{};
  std::size_t vertex_count{};
  std::size_t index_count{};
  bool store_backed{};
  bool clip_matrix_supplied{};
  bool placement_translation_applied{};
  bool shader_bytes_supplied{};
  bool jv_eligible{};
  std::size_t runtime_draw_instances{};
  std::size_t runtime_meshes{};
  std::size_t runtime_textures{};
  std::size_t terrain_draw_instances{};
  std::size_t water_lookup_entries{};
  bool complete_render_scene{};
};

class RetailMission01VulkanScene final {
 public:
  RetailMission01VulkanScene(
      const RetailMission01VulkanScene&) = delete;
  RetailMission01VulkanScene& operator=(
      const RetailMission01VulkanScene&) = delete;
  RetailMission01VulkanScene(RetailMission01VulkanScene&&) noexcept = default;
  RetailMission01VulkanScene& operator=(
      RetailMission01VulkanScene&&) noexcept = default;

  // `draw_instance_index` indexes the accepted map draw list, not the raw
  // placement table. `object_to_clip` is row-major and is applied after the
  // qualified .pdl translation for that draw. The SPIR-V spans are copied
  // before this function returns.
  static std::optional<RetailMission01VulkanScene> open(
      const RetailContentStore& store, std::uint32_t draw_instance_index,
      bool swap_16, const std::array<float, 16>& object_to_clip,
      std::span<const std::uint32_t> vertex_spirv,
      std::span<const std::uint32_t> fragment_spirv,
      std::uint64_t vertex_shader_hash,
      std::uint64_t fragment_shader_hash,
      std::uint32_t width = 1280U, std::uint32_t height = 720U);

  // Production handoff for callers that already decoded the immutable map
  // assets. This preserves store provenance while avoiding a second parse of
  // the 4,226-instance placement graph.
  static std::optional<RetailMission01VulkanScene> open_assets(
      RetailMission01MapRenderAssets assets,
      std::uint32_t draw_instance_index, bool swap_16,
      const std::array<float, 16>& object_to_clip,
      std::span<const std::uint32_t> vertex_spirv,
      std::span<const std::uint32_t> fragment_spirv,
      std::uint64_t vertex_shader_hash,
      std::uint64_t fragment_shader_hash,
      std::uint32_t width = 1280U, std::uint32_t height = 720U);

  // Product handoff. The sealed cache and the current native simulation own
  // asset selection, transforms, camera and shader identity. No draw index,
  // projection matrix or shader byte span crosses this boundary.
  static std::optional<RetailMission01VulkanScene> open_runtime(
      const RetailContentStore& store, const SimulationSnapshot& snapshot,
      bool swap_16 = true, std::uint32_t width = 1280U,
      std::uint32_t height = 720U);

  // Test-only construction still follows the same NDXR/NTXR and scene
  // contracts, but does not claim sealed-cache provenance.
  static std::optional<RetailMission01VulkanScene> build_for_testing(
      RetailMission01MapRenderAssets assets,
      std::uint32_t draw_instance_index, bool swap_16,
      const std::array<float, 16>& object_to_clip,
      std::span<const std::uint32_t> vertex_spirv,
      std::span<const std::uint32_t> fragment_spirv,
      std::uint64_t vertex_shader_hash,
      std::uint64_t fragment_shader_hash,
      std::uint32_t width = 1280U, std::uint32_t height = 720U);

  const RenderScene& scene() const noexcept { return scene_; }
  const RetailMission01VulkanSceneReport& report() const noexcept {
    return report_;
  }
  const RetailMission01MapRenderAssets& assets() const noexcept {
    return assets_;
  }

  // Views are rebuilt on demand so they remain valid after the owning object
  // is moved. They are consumed synchronously by VulkanSceneResourceCache::
  // build_clip_textured and never retained by the adapter.
  [[nodiscard]] VulkanSceneClipTexturedMeshUpload mesh_upload() const noexcept;
  [[nodiscard]] VulkanSceneTexturedMaterialUpload material_upload() const noexcept;
  [[nodiscard]] VulkanSceneTextureUpload texture_upload() const noexcept;

  [[nodiscard]] std::vector<VulkanSceneWorldTexturedMeshUpload>
  world_mesh_uploads() const;
  [[nodiscard]] VulkanSceneTexturedMaterialUpload world_material_upload()
      const noexcept;
  [[nodiscard]] std::vector<VulkanSceneTextureUpload> world_texture_uploads()
      const;

  // Updates only frame-owned camera/tick metadata. Persistent GPU resources
  // remain untouched; the resource cache has a separate dynamic render gate.
  [[nodiscard]] bool update_snapshot(const SimulationSnapshot& snapshot) noexcept;

 private:
  RetailMission01VulkanScene(RetailMission01MapRenderAssets assets,
                             VulkanMission01ClipTexturedUpload upload,
                             std::vector<VulkanMission01WorldTexturedUpload>
                                 world_uploads,
                             std::vector<VulkanMission01TextureUpload>
                                 world_texture_uploads,
                             std::vector<std::uint32_t> vertex_spirv,
                             std::vector<std::uint32_t> fragment_spirv,
                             RenderScene scene,
                             RetailMission01VulkanSceneReport report,
                             std::string material_id) noexcept;

  static std::optional<RetailMission01VulkanScene> build(
      RetailMission01MapRenderAssets assets,
      std::uint32_t draw_instance_index, bool swap_16,
      const std::array<float, 16>& object_to_clip,
      std::span<const std::uint32_t> vertex_spirv,
      std::span<const std::uint32_t> fragment_spirv,
      std::uint64_t vertex_shader_hash,
      std::uint64_t fragment_shader_hash,
      std::uint32_t width, std::uint32_t height);

  static std::optional<RetailMission01VulkanScene> build_runtime(
      RetailMission01MapRenderAssets assets, const SimulationSnapshot& snapshot,
      bool swap_16, std::uint32_t width, std::uint32_t height);

  RetailMission01MapRenderAssets assets_;
  VulkanMission01ClipTexturedUpload upload_;
  std::vector<VulkanMission01WorldTexturedUpload> world_uploads_;
  std::vector<VulkanMission01TextureUpload> world_texture_uploads_;
  std::vector<std::uint32_t> vertex_spirv_;
  std::vector<std::uint32_t> fragment_spirv_;
  RenderScene scene_;
  RetailMission01VulkanSceneReport report_;
  std::string material_id_;
};

}  // namespace ac6::retail
