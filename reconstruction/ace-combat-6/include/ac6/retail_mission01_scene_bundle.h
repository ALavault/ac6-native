#pragma once

#include "ac6/retail_content.h"
#include "ac6/retail_fhm_view.h"
#include "ac6/retail_map_placement.h"
#include "ac6/retail_map_water.h"
#include "ac6/retail_terrain_field.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace ac6::retail {

enum class Mission01MapResource : std::uint8_t {
  WaterMca = 1,
  WaterMcd = 2,
  WaterMci = 3,
  TerrainGrid = 4,
  TerrainPatches = 5,
  TerrainAtlasMap = 9,
  TerrainAtlasIndex = 10,
  Placement = 11,
  PlacedTrees = 12,
  ProceduralTreeMask = 13,
};

struct Mission01TextureBindingReport final {
  std::size_t model_files{};
  std::size_t model_records{};
  std::size_t descriptors{};
  std::size_t material_slots{};
  std::size_t texture_references{};
  std::size_t unique_texture_references{};
  std::size_t texture_wrappers{};
  std::size_t missing_texture_ids{};
  std::size_t unbound_descriptors{};
  bool complete{};
};

// The qualified entry-119 hierarchy and the common readers that can already be
// opened without a filename manifest. Raw spans remain owned by this object.
class RetailMission01SceneBundle final {
 public:
  static std::optional<RetailMission01SceneBundle> open(
      const RetailContentStore& store);
  static std::optional<RetailMission01SceneBundle> open_payload_for_testing(
      std::span<const std::uint8_t> payload);

  bool store_backed() const noexcept { return store_backed_; }
  const Sha256Digest& content_index_sha256() const noexcept {
    return content_index_sha256_;
  }
  const TerrainField& terrain() const noexcept { return terrain_; }
  const MapWaterGrid& water() const noexcept { return water_; }
  const MapPlacement& placement() const noexcept { return placement_; }

  std::optional<std::span<const std::uint8_t>> map_resource(
      Mission01MapResource resource) const noexcept;
  std::optional<RetailFhmView> map_parts() const noexcept;
  std::optional<RetailFhmView> map_part_textures() const noexcept;
  std::optional<RetailFhmView> terrain_atlas() const noexcept;
  std::optional<RetailFhmView> mapset() const noexcept;
  std::optional<RetailFhmView> mapset_models() const noexcept;
  std::optional<RetailFhmView> mapset_textures() const noexcept;
  std::optional<Mission01TextureBindingReport> audit_texture_bindings()
      const;
  std::optional<std::span<const std::uint8_t>> texture_by_gidx(
      std::uint32_t identifier) const;

 private:
  using TextureRegistry =
      std::map<std::uint32_t, std::span<const std::uint8_t>>;

  bool initialise();
  std::optional<TextureRegistry> texture_registry() const;
  std::optional<RetailFhmView> world_root() const noexcept;
  std::optional<RetailFhmView> map_container() const noexcept;

  bool store_backed_{};
  Sha256Digest content_index_sha256_{};
  std::vector<std::uint8_t> bytes_;
  TerrainField terrain_;
  MapWaterGrid water_;
  MapPlacement placement_;
};

}  // namespace ac6::retail
