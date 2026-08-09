#include "ac6/retail_mission01_scene_bundle.h"

#include "ac6/ntxr_texture.h"
#include "ac6/retail_ndxr_container.h"

#include <algorithm>
#include <array>
#include <set>
#include <string_view>
#include <utility>

namespace ac6::retail {
namespace {

constexpr std::uint32_t kWorldEntry = 119;
constexpr std::uint32_t kWorldMapChild = 21;
constexpr std::uint32_t kWorldMapsetChild = 22;
constexpr std::uint32_t kMapPartsChild = 14;
constexpr std::uint32_t kMapPartTexturesChild = 15;
constexpr std::uint32_t kTerrainAtlasChild = 16;
constexpr std::uint32_t kMapsetModelsChild = 5;
constexpr std::uint32_t kMapsetTexturesChild = 6;
constexpr std::string_view kWorldPayloadSha256 =
    "e57cbeeb8f97a7a607ee1315b11a822b6af2d32581dcb7cbd557f1a6280e6dbd";

struct RequiredResource final {
  Mission01MapResource resource;
  std::size_t size;
};

constexpr std::array<RequiredResource, 10> kRequiredResources{{
    {Mission01MapResource::WaterMca, 272},
    {Mission01MapResource::WaterMcd, 211472},
    {Mission01MapResource::WaterMci, 9744},
    {Mission01MapResource::TerrainGrid, 256},
    {Mission01MapResource::TerrainPatches, 1250604},
    {Mission01MapResource::TerrainAtlasMap, 256},
    {Mission01MapResource::TerrainAtlasIndex, 12288},
    {Mission01MapResource::Placement, 73184},
    {Mission01MapResource::PlacedTrees, 145944},
    {Mission01MapResource::ProceduralTreeMask, 197632},
}};

bool has_magic(std::span<const std::uint8_t> bytes,
               std::string_view magic) noexcept {
  return magic.size() == 4 && bytes.size() >= magic.size() &&
         std::equal(magic.begin(), magic.end(), bytes.begin());
}

bool validate_live_prefix(const RetailFhmView& view, std::uint32_t live,
                          std::string_view magic) noexcept {
  if (view.child_count() < live) return false;
  for (std::uint32_t index = 0; index < live; ++index) {
    const std::optional<std::span<const std::uint8_t>> child = view.child(index);
    if (!child.has_value() || !has_magic(*child, magic)) return false;
  }
  for (std::uint32_t index = live; index < view.child_count(); ++index) {
    const std::optional<std::uint32_t> length = view.child_length(index);
    if (!length.has_value() || *length != 0) return false;
  }
  return true;
}

}  // namespace

std::optional<RetailMission01SceneBundle> RetailMission01SceneBundle::open(
    const RetailContentStore& store) {
  const RetailContentRecord* record = store.find(kWorldEntry);
  if (!store.valid() || record == nullptr ||
      sha256_hex(record->payload_sha256) != kWorldPayloadSha256) {
    return std::nullopt;
  }
  RetailMission01SceneBundle bundle;
  bundle.store_backed_ = true;
  bundle.content_index_sha256_ = store.index_sha256();
  if (!store.read_payload(kWorldEntry, bundle.bytes_) || !bundle.initialise()) {
    return std::nullopt;
  }
  const std::optional<Mission01TextureBindingReport> bindings =
      bundle.audit_texture_bindings();
  if (!bindings.has_value() || !bindings->complete) return std::nullopt;
  return bundle;
}

std::optional<RetailMission01SceneBundle>
RetailMission01SceneBundle::open_payload_for_testing(
    std::span<const std::uint8_t> payload) {
  RetailMission01SceneBundle bundle;
  bundle.bytes_.assign(payload.begin(), payload.end());
  if (!bundle.initialise()) return std::nullopt;
  return bundle;
}

std::optional<RetailFhmView> RetailMission01SceneBundle::world_root()
    const noexcept {
  return RetailFhmView::open(bytes_);
}

std::optional<RetailFhmView> RetailMission01SceneBundle::map_container()
    const noexcept {
  const std::optional<RetailFhmView> root = world_root();
  return root.has_value() ? root->nested(kWorldMapChild) : std::nullopt;
}

std::optional<std::span<const std::uint8_t>>
RetailMission01SceneBundle::map_resource(
    Mission01MapResource resource) const noexcept {
  const std::optional<RetailFhmView> map = map_container();
  return map.has_value()
             ? map->child(static_cast<std::uint32_t>(resource))
             : std::nullopt;
}

std::optional<RetailFhmView> RetailMission01SceneBundle::map_parts()
    const noexcept {
  const std::optional<RetailFhmView> map = map_container();
  return map.has_value() ? map->nested(kMapPartsChild) : std::nullopt;
}

std::optional<RetailFhmView> RetailMission01SceneBundle::map_part_textures()
    const noexcept {
  const std::optional<RetailFhmView> map = map_container();
  return map.has_value() ? map->nested(kMapPartTexturesChild) : std::nullopt;
}

std::optional<RetailFhmView> RetailMission01SceneBundle::terrain_atlas()
    const noexcept {
  const std::optional<RetailFhmView> map = map_container();
  return map.has_value() ? map->nested(kTerrainAtlasChild) : std::nullopt;
}

std::optional<RetailFhmView> RetailMission01SceneBundle::mapset()
    const noexcept {
  const std::optional<RetailFhmView> root = world_root();
  return root.has_value() ? root->nested(kWorldMapsetChild) : std::nullopt;
}

std::optional<RetailFhmView> RetailMission01SceneBundle::mapset_models()
    const noexcept {
  const std::optional<RetailFhmView> resources = mapset();
  return resources.has_value() ? resources->nested(kMapsetModelsChild)
                               : std::nullopt;
}

std::optional<RetailFhmView> RetailMission01SceneBundle::mapset_textures()
    const noexcept {
  const std::optional<RetailFhmView> resources = mapset();
  return resources.has_value() ? resources->nested(kMapsetTexturesChild)
                               : std::nullopt;
}

std::optional<RetailMission01SceneBundle::TextureRegistry>
RetailMission01SceneBundle::texture_registry() const {
  TextureRegistry registry;
  const auto add = [&](std::span<const std::uint8_t> wrapper) {
    const std::optional<std::uint32_t> identifier =
        ntxr_gidx_identifier(wrapper.data(), wrapper.size());
    return identifier.has_value() &&
           registry.emplace(*identifier, wrapper).second;
  };

  const std::optional<RetailFhmView> root = world_root();
  const std::optional<RetailFhmView> part_textures = map_part_textures();
  const std::optional<RetailFhmView> atlas = terrain_atlas();
  const std::optional<RetailFhmView> set = mapset();
  const std::optional<RetailFhmView> set_textures = mapset_textures();
  if (!root.has_value() || !part_textures.has_value() || !atlas.has_value() ||
      !set.has_value() || !set_textures.has_value()) {
    return std::nullopt;
  }

  for (const std::uint32_t index : {0u, 1u}) {
    const std::optional<std::span<const std::uint8_t>> wrapper =
        root->child(index);
    if (!wrapper.has_value() || !add(*wrapper)) return std::nullopt;
  }
  for (std::uint32_t index = 0; index < 170; ++index) {
    const std::optional<std::span<const std::uint8_t>> wrapper =
        part_textures->child(index);
    if (!wrapper.has_value() || !add(*wrapper)) return std::nullopt;
  }
  for (std::uint32_t index = 0; index < 7; ++index) {
    const std::optional<std::span<const std::uint8_t>> wrapper =
        atlas->child(index);
    if (!wrapper.has_value() || !add(*wrapper)) return std::nullopt;
  }
  for (std::uint32_t index = 0; index < 8; ++index) {
    const std::optional<std::span<const std::uint8_t>> wrapper =
        set_textures->child(index);
    if (!wrapper.has_value() || !add(*wrapper)) return std::nullopt;
  }
  for (std::uint32_t index = 7; index <= 11; ++index) {
    const std::optional<std::span<const std::uint8_t>> wrapper =
        set->child(index);
    if (!wrapper.has_value() || !add(*wrapper)) return std::nullopt;
  }
  if (registry.size() != 192) return std::nullopt;
  return registry;
}

std::optional<std::span<const std::uint8_t>>
RetailMission01SceneBundle::texture_by_gidx(std::uint32_t identifier) const {
  const std::optional<TextureRegistry> registry = texture_registry();
  if (!registry.has_value()) return std::nullopt;
  const auto found = registry->find(identifier);
  return found == registry->end()
             ? std::optional<std::span<const std::uint8_t>>{}
             : std::optional<std::span<const std::uint8_t>>(found->second);
}

std::optional<std::vector<Mission01TextureResourceView>>
RetailMission01SceneBundle::texture_resources() const {
  const std::optional<TextureRegistry> registry = texture_registry();
  if (!registry.has_value()) return std::nullopt;
  std::vector<Mission01TextureResourceView> resources;
  resources.reserve(registry->size());
  for (const auto& [identifier, bytes] : *registry) {
    resources.push_back({identifier, bytes});
  }
  return resources;
}

std::optional<Mission01TextureBindingReport>
RetailMission01SceneBundle::audit_texture_bindings() const {
  const std::optional<TextureRegistry> registry = texture_registry();
  const std::optional<RetailFhmView> parts = map_parts();
  const std::optional<RetailFhmView> set_models = mapset_models();
  if (!registry.has_value() || !parts.has_value() ||
      !set_models.has_value()) {
    return std::nullopt;
  }

  Mission01TextureBindingReport report;
  report.texture_wrappers = registry->size();
  std::set<std::uint32_t> referenced;
  std::set<std::uint32_t> missing;
  const auto scan_models = [&](const RetailFhmView& models,
                               std::uint32_t live_models) {
    if (live_models > models.child_count()) return false;
    for (std::uint32_t model_index = 0; model_index < live_models;
         ++model_index) {
      const std::optional<std::span<const std::uint8_t>> bytes =
          models.child(model_index);
      if (!bytes.has_value()) return false;
      const std::optional<NdxrContainer> model =
          NdxrContainer::Open(bytes->data(), bytes->size());
      if (!model.has_value()) return false;
      ++report.model_files;
      for (std::uint16_t record_index = 0;
           record_index < model->record_count(); ++record_index) {
        const std::optional<NdxrRecord> record = model->Record(record_index);
        if (!record.has_value()) return false;
        ++report.model_records;
        for (std::uint16_t descriptor_index = 0;
             descriptor_index < record->descriptor_count;
             ++descriptor_index) {
          if (!model->Descriptor(*record, descriptor_index).has_value()) {
            return false;
          }
          ++report.descriptors;
          std::size_t descriptor_references = 0;
          for (unsigned slot = 0; slot < NdxrContainer::kMaterialSlots;
               ++slot) {
            const std::optional<NdxrMaterial> material =
                model->Material(*record, descriptor_index, slot);
            if (!material.has_value()) continue;
            ++report.material_slots;
            for (std::uint16_t texture_index = 0;
                 texture_index < material->texture_count; ++texture_index) {
              const std::optional<NdxrTextureRef> reference =
                  model->TextureRef(*material, texture_index);
              if (!reference.has_value()) return false;
              if (reference->texture_id == 0) continue;
              ++descriptor_references;
              ++report.texture_references;
              referenced.insert(reference->texture_id);
              if (!registry->contains(reference->texture_id)) {
                missing.insert(reference->texture_id);
              }
            }
          }
          if (descriptor_references == 0) ++report.unbound_descriptors;
        }
      }
    }
    return true;
  };

  if (!scan_models(*parts, 170) || !scan_models(*set_models, 8)) {
    return std::nullopt;
  }
  report.unique_texture_references = referenced.size();
  report.missing_texture_ids = missing.size();
  report.complete = report.model_files == 178 &&
                    report.texture_wrappers == 192 &&
                    report.missing_texture_ids == 0 &&
                    report.unbound_descriptors == 0;
  return report;
}

bool RetailMission01SceneBundle::initialise() {
  const std::optional<RetailFhmView> root = world_root();
  const std::optional<RetailFhmView> map = map_container();
  const std::optional<RetailFhmView> mapset_resources = mapset();
  if (!root.has_value() || root->child_count() != 23 || !map.has_value() ||
      map->child_count() != 17 || !mapset_resources.has_value() ||
      mapset_resources->child_count() != 12) {
    return false;
  }

  for (const RequiredResource& required : kRequiredResources) {
    const std::optional<std::span<const std::uint8_t>> bytes =
        map_resource(required.resource);
    if (!bytes.has_value() || bytes->size() != required.size) return false;
  }
  const std::optional<std::span<const std::uint8_t>> mca =
      map_resource(Mission01MapResource::WaterMca);
  const std::optional<std::span<const std::uint8_t>> mcd =
      map_resource(Mission01MapResource::WaterMcd);
  const std::optional<std::span<const std::uint8_t>> mci =
      map_resource(Mission01MapResource::WaterMci);
  const std::optional<std::span<const std::uint8_t>> grid =
      map_resource(Mission01MapResource::TerrainGrid);
  const std::optional<std::span<const std::uint8_t>> patches =
      map_resource(Mission01MapResource::TerrainPatches);
  const std::optional<std::span<const std::uint8_t>> pdl =
      map_resource(Mission01MapResource::Placement);
  if (!mca.has_value() || !mcd.has_value() || !mci.has_value() ||
      !grid.has_value() || !patches.has_value() || !pdl.has_value()) {
    return false;
  }

  std::optional<TerrainField> terrain = TerrainField::open(
      grid->data(), grid->size(), patches->data(), patches->size());
  std::optional<MapWaterGrid> water = MapWaterGrid::open(
      mca->data(), mca->size(), mci->data(), mci->size(), mcd->data(),
      mcd->size());
  std::optional<MapPlacement> placement =
      MapPlacement::open(pdl->data(), pdl->size());
  if (!terrain.has_value() || terrain->patch_count() != 74 ||
      // MCI+8 is the big-endian halfword 0x1300 (4864), not the visual byte
      // value 0x13. The existing MapWaterGrid corpus test pins this reading.
      !water.has_value() || water->group_count() != 4864 ||
      water->block_count() != 413 || !placement.has_value() ||
      placement->header_total() != 4318 ||
      placement->instances().size() != 4318) {
    return false;
  }

  const std::optional<RetailFhmView> parts = map_parts();
  const std::optional<RetailFhmView> textures = map_part_textures();
  const std::optional<RetailFhmView> atlas = terrain_atlas();
  const std::optional<RetailFhmView> set_models = mapset_models();
  const std::optional<RetailFhmView> set_textures = mapset_textures();
  if (!parts.has_value() || parts->child_count() != 256 ||
      !validate_live_prefix(*parts, 170, "NDXR") || !textures.has_value() ||
      textures->child_count() != 256 ||
      !validate_live_prefix(*textures, 170, "NTXR") || !atlas.has_value() ||
      atlas->child_count() != 8 || !validate_live_prefix(*atlas, 7, "NTXR") ||
      !set_models.has_value() || set_models->child_count() != 8 ||
      !validate_live_prefix(*set_models, 8, "NDXR") ||
      !set_textures.has_value() || set_textures->child_count() != 8 ||
      !validate_live_prefix(*set_textures, 8, "NTXR")) {
    return false;
  }
  for (std::uint32_t index = 7; index <= 11; ++index) {
    const std::optional<std::span<const std::uint8_t>> texture =
        mapset_resources->child(index);
    if (!texture.has_value() || !has_magic(*texture, "NTXR")) return false;
  }

  terrain_ = std::move(*terrain);
  water_ = std::move(*water);
  placement_ = std::move(*placement);
  return true;
}

}  // namespace ac6::retail
