#pragma once

#include "ac6/ntxr_texture.h"
#include "ac6/retail_mission01_scene_bundle.h"
#include "ac6/retail_ndxr_geometry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ac6::retail {

// Bits 30..31 of a .pdl instance select the token carried by the corresponding
// NDXR record name. The four-way 4,318-record join is documented in
// retail_map_placement.h and enforced again while these assets are built.
enum class Mission01MapDrawClass : std::uint8_t {
  Large = 0,
  Medium = 1,
  Small = 2,
  Extra = 3,
};

std::optional<Mission01MapDrawClass> mission01_map_draw_class(
    std::string_view record_name) noexcept;

struct Mission01MapPrimitive final {
  std::string record_name;
  Mission01MapDrawClass draw_class{};
  std::uint32_t texture_identifier{};
  NdxrMesh geometry;
};

struct Mission01MapModel final {
  std::uint16_t selector{};
  std::vector<Mission01MapPrimitive> records;
};

struct Mission01MapTextureAsset final {
  std::uint32_t identifier{};
  NtxrDescriptor descriptor{};
  std::span<const std::uint8_t> source;
};

struct Mission01TerrainAtlasCell final {
  std::uint8_t page{};
  std::uint8_t tile{};
  bool operator==(const Mission01TerrainAtlasCell&) const = default;
};

struct Mission01TerrainAtlasPage final {
  std::uint8_t page{};
  std::uint32_t identifier{};
  NtxrDescriptor descriptor{};
  std::span<const std::uint8_t> source;
};

// Retail's vertex shaders for contexts 0x04100113 and 0x04100114 map local
// world X to U and local world Z to V. They contract each 272-pixel tile about
// its centre by float bits 0x3F707878 before applying the page-specific steps.
// The resulting inner span is 255.5 pixels with an 8.25-pixel inset per edge.
struct Mission01TerrainAtlasUvTransform final {
  std::uint8_t page{};
  float u_origin{};
  float v_origin{};
  float u_step{};
  float v_step{};
  float inner_scale{};

  std::array<float, 2> map_local_fraction(float local_x,
                                          float local_z) const noexcept;
};

// Compact, persistent terrain upload sources. The 256-byte patch grid and 74
// 65x65 sample blocks are the retail representation; no million-vertex CPU
// expansion is rebuilt per frame. Atlas UVs preserve the two retail vertex
// shader variants' centred contraction and X->U / Z->V orientation.
struct Mission01TerrainRenderResource final {
  std::span<const std::uint8_t> patch_grid;
  std::span<const float> patch_samples;
  std::vector<Mission01TerrainAtlasCell> atlas_cells;
  std::vector<Mission01TerrainAtlasPage> atlas_pages;

  const Mission01TerrainAtlasCell* atlas_cell(
      std::size_t cell_x, std::size_t cell_z) const noexcept;
  std::optional<Mission01TerrainAtlasUvTransform> atlas_uv_transform(
      std::size_t cell_x, std::size_t cell_z) const noexcept;
};

// Host-endian upload form of MCA/MCI/MCD. It retains the full 8-world-unit bit
// resolution rather than classifying a 128-unit terrain quad at its centre.
struct Mission01WaterRenderResource final {
  std::array<std::uint8_t, 256> coarse_groups{};
  std::vector<std::uint16_t> cell_blocks;
  std::vector<std::uint8_t> block_bits;

  bool query(float world_x, float world_z, bool* bit) const noexcept;
};

// A persistent draw command. `selector` chooses parts/%d and `record_index`
// chooses exactly one NDXR record inside it. Translation is the complete
// transform present in .pdl; the format contains no inferred rotation.
struct Mission01MapDrawInstance final {
  float world_x{};
  float world_y{};
  float world_z{};
  std::uint16_t selector{};
  std::uint16_t record_index{};
  Mission01MapDrawClass draw_class{};
};

struct Mission01MapRenderAssetReport final {
  std::size_t model_files{};
  std::size_t source_records{};
  std::size_t decoded_primitives{};
  std::size_t vertices{};
  std::size_t indices{};
  std::size_t texture_references{};
  std::size_t texture_assets{};
  std::size_t source_instances{};
  std::size_t record_bindings{};
  std::size_t draw_instances{};
  std::size_t skipped_instances{};
  std::array<std::size_t, 4> draw_classes{};
  std::size_t terrain_patches{};
  std::size_t terrain_patch_samples{};
  std::size_t terrain_atlas_cells{};
  std::size_t terrain_atlas_bindings{};
  std::size_t terrain_atlas_pages{};
  std::size_t terrain_atlas_uv_transforms{};
  std::size_t water_lookup_entries{};
  std::size_t water_blocks{};
  bool complete{};
};

// Store-backed, immutable CPU assets for Mission 01's placed map parts.
// Geometry and the draw list are decoded once. Texture surfaces remain in the
// owned retail bundle and are decoded explicitly by a renderer during its
// one-time upload, never while iterating a frame.
class RetailMission01MapRenderAssets final {
 public:
  RetailMission01MapRenderAssets(
      const RetailMission01MapRenderAssets&) = delete;
  RetailMission01MapRenderAssets& operator=(
      const RetailMission01MapRenderAssets&) = delete;
  RetailMission01MapRenderAssets(RetailMission01MapRenderAssets&&) noexcept =
      default;
  RetailMission01MapRenderAssets& operator=(
      RetailMission01MapRenderAssets&&) noexcept = default;

  static std::optional<RetailMission01MapRenderAssets> open(
      const RetailContentStore& store);
  static std::optional<RetailMission01MapRenderAssets> build_for_testing(
      RetailMission01SceneBundle scene);

  bool store_backed() const noexcept { return scene_.store_backed(); }
  const Sha256Digest& content_index_sha256() const noexcept {
    return scene_.content_index_sha256();
  }
  const RetailMission01SceneBundle& scene() const noexcept { return scene_; }
  const std::vector<Mission01MapModel>& models() const noexcept {
    return models_;
  }
  const std::vector<Mission01MapTextureAsset>& textures() const noexcept {
    return textures_;
  }
  const std::vector<Mission01MapDrawInstance>& draw_instances() const noexcept {
    return draw_instances_;
  }
  const Mission01TerrainRenderResource& terrain_resource() const noexcept {
    return terrain_resource_;
  }
  const Mission01WaterRenderResource& water_resource() const noexcept {
    return water_resource_;
  }
  const Mission01MapRenderAssetReport& report() const noexcept {
    return report_;
  }

  const Mission01MapPrimitive* primitive_for(
      const Mission01MapDrawInstance& instance) const noexcept;
  std::optional<DecodedTexture> decode_texture(
      std::uint32_t identifier, bool swap_16,
      NtxrRefusal* refusal = nullptr) const noexcept;

 private:
  explicit RetailMission01MapRenderAssets(RetailMission01SceneBundle scene);

  static std::optional<RetailMission01MapRenderAssets> build(
      RetailMission01SceneBundle scene);
  bool load_models();
  bool load_model(std::uint16_t selector,
                  std::span<const std::uint8_t> bytes);
  bool load_textures();
  bool load_terrain();
  bool load_water();
  bool bind_instances();

  RetailMission01SceneBundle scene_;
  std::vector<Mission01MapModel> models_;
  std::vector<Mission01MapTextureAsset> textures_;
  std::vector<Mission01MapDrawInstance> draw_instances_;
  Mission01TerrainRenderResource terrain_resource_;
  Mission01WaterRenderResource water_resource_;
  Mission01MapRenderAssetReport report_;
};

}  // namespace ac6::retail
