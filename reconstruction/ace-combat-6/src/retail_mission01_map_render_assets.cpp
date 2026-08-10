#include "ac6/retail_mission01_map_render_assets.h"

#include "ac6/retail_ndxr_container.h"

#include <algorithm>
#include <array>
#include <bit>
#include <set>
#include <utility>

namespace ac6::retail {
namespace {

constexpr std::size_t kMapModelCount = 170;
constexpr std::size_t kMapRecordCount = 4318;
constexpr std::size_t kAcceptedInstanceCount = 4226;
constexpr std::size_t kSkippedInstanceCount = 92;
constexpr std::size_t kTerrainAtlasSide = 256;
constexpr std::size_t kTerrainAtlasRecordCount = 24;
constexpr std::size_t kTerrainAtlasRecordBytes = 512;
constexpr std::size_t kTerrainAtlasPageCount = 7;
constexpr std::uint32_t kTerrainAtlasTilePixels = 272;
constexpr std::uint32_t kTerrainAtlasColumns = 15;
// 0x820FAE50 stores 0x3F707878 at owner+0x6D80. 0x820FD560 sends it
// to vertex constant c65; entry-163 NSXR contexts 0x04100113/0x04100114 use
// (local - 0.5) * scale + 0.5 before the page-specific c64 atlas step.
constexpr float kTerrainAtlasInnerScale =
    std::bit_cast<float>(std::uint32_t{0x3F707878});

std::uint16_t be16(const std::uint8_t* bytes) noexcept {
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(bytes[0]) << 8) | bytes[1]);
}

bool same_class(Mission01MapDrawClass left, std::uint8_t right) noexcept {
  return static_cast<std::uint8_t>(left) == right;
}

}  // namespace

std::optional<Mission01MapDrawClass> mission01_map_draw_class(
    std::string_view record_name) noexcept {
  constexpr std::string_view marker = "_m01_";
  const std::size_t marker_at = record_name.find(marker);
  if (marker_at == std::string_view::npos) return std::nullopt;
  const std::size_t token_at = marker_at + marker.size();
  const std::size_t token_end = record_name.find('_', token_at);
  if (token_end == std::string_view::npos || token_end == token_at) {
    return std::nullopt;
  }
  const std::string_view token =
      record_name.substr(token_at, token_end - token_at);
  if (token == "l" || token == "airport") {
    return Mission01MapDrawClass::Large;
  }
  if (token == "m") return Mission01MapDrawClass::Medium;
  if (token == "s") return Mission01MapDrawClass::Small;
  if (token == "x") return Mission01MapDrawClass::Extra;
  return std::nullopt;
}

const Mission01TerrainAtlasCell*
Mission01TerrainRenderResource::atlas_cell(
    std::size_t cell_x, std::size_t cell_z) const noexcept {
  if (cell_x >= kTerrainAtlasSide || cell_z >= kTerrainAtlasSide ||
      atlas_cells.size() != kTerrainAtlasSide * kTerrainAtlasSide) {
    return nullptr;
  }
  return &atlas_cells[cell_z * kTerrainAtlasSide + cell_x];
}

std::array<float, 2> Mission01TerrainAtlasUvTransform::map_local_fraction(
    float local_x, float local_z) const noexcept {
  const float inner_x = (local_x - 0.5F) * inner_scale + 0.5F;
  const float inner_z = (local_z - 0.5F) * inner_scale + 0.5F;
  return {u_origin + inner_x * u_step, v_origin + inner_z * v_step};
}

std::optional<Mission01TerrainAtlasUvTransform>
Mission01TerrainRenderResource::atlas_uv_transform(
    std::size_t cell_x, std::size_t cell_z) const noexcept {
  const Mission01TerrainAtlasCell* cell = atlas_cell(cell_x, cell_z);
  if (cell == nullptr || cell->page >= atlas_pages.size()) return std::nullopt;
  const Mission01TerrainAtlasPage& page = atlas_pages[cell->page];
  if (page.descriptor.width == 0 || page.descriptor.height == 0) {
    return std::nullopt;
  }
  const std::uint32_t column = cell->tile % kTerrainAtlasColumns;
  const std::uint32_t row = cell->tile / kTerrainAtlasColumns;
  const float width = static_cast<float>(page.descriptor.width);
  const float height = static_cast<float>(page.descriptor.height);
  return Mission01TerrainAtlasUvTransform{
      cell->page,
      static_cast<float>(column * kTerrainAtlasTilePixels) / width,
      static_cast<float>(row * kTerrainAtlasTilePixels) / height,
      static_cast<float>(kTerrainAtlasTilePixels) / width,
      static_cast<float>(kTerrainAtlasTilePixels) / height,
      kTerrainAtlasInnerScale};
}

bool Mission01WaterRenderResource::query(float world_x, float world_z,
                                         bool* bit) const noexcept {
  const long cell_x =
      static_cast<long>((world_x + kWaterWorldBias) / kWaterCellUnits);
  const long cell_z =
      static_cast<long>((world_z + kWaterWorldBias) / kWaterCellUnits);
  const long coarse_x = cell_x >> 4;
  const long coarse_z = cell_z >> 4;
  if (coarse_x < 0 || coarse_z < 0 ||
      coarse_x >= static_cast<long>(kWaterCoarseSide) ||
      coarse_z >= static_cast<long>(kWaterCoarseSide)) {
    return false;
  }
  const std::size_t group = coarse_groups[
      static_cast<std::size_t>(coarse_z) * kWaterCoarseSide +
      static_cast<std::size_t>(coarse_x)];
  const std::size_t lookup =
      (group * 16 + static_cast<std::size_t>(cell_z & 15)) * 16 +
      static_cast<std::size_t>(cell_x & 15);
  if (lookup >= cell_blocks.size()) return false;
  const std::size_t block = cell_blocks[lookup];
  const long row = static_cast<long>(world_z / kWaterBitUnits) & 63;
  const long column = static_cast<long>(world_x / kWaterBitUnits) & 63;
  const long linear = (row << 6) | column;
  const std::size_t byte =
      block * kWaterBlockBytes + static_cast<std::size_t>(linear >> 3);
  if (byte >= block_bits.size()) return false;
  if (bit != nullptr) {
    *bit = ((block_bits[byte] >> (7 - (linear & 7))) & 1u) != 0;
  }
  return true;
}

RetailMission01MapRenderAssets::RetailMission01MapRenderAssets(
    RetailMission01SceneBundle scene)
    : scene_(std::move(scene)) {}

std::optional<RetailMission01MapRenderAssets>
RetailMission01MapRenderAssets::open(const RetailContentStore& store) {
  std::optional<RetailMission01SceneBundle> scene =
      RetailMission01SceneBundle::open(store);
  if (!scene.has_value() || !scene->store_backed()) return std::nullopt;
  return build(std::move(*scene));
}

std::optional<RetailMission01MapRenderAssets>
RetailMission01MapRenderAssets::build_for_testing(
    RetailMission01SceneBundle scene) {
  return build(std::move(scene));
}

std::optional<RetailMission01MapRenderAssets>
RetailMission01MapRenderAssets::build(RetailMission01SceneBundle scene) {
  RetailMission01MapRenderAssets assets(std::move(scene));
  if (!assets.load_models() || !assets.load_textures() ||
      !assets.load_terrain() || !assets.load_water() ||
      !assets.bind_instances()) {
    return std::nullopt;
  }
  Mission01MapRenderAssetReport& report = assets.report_;
  report.complete = report.model_files == kMapModelCount &&
                    report.source_records == kMapRecordCount &&
                    report.decoded_primitives == kMapRecordCount &&
                    report.texture_references == kMapRecordCount &&
                    report.texture_assets == kMapModelCount &&
                    report.source_instances == kMapRecordCount &&
                    report.record_bindings == kMapRecordCount &&
                    report.draw_instances == kAcceptedInstanceCount &&
                    report.skipped_instances == kSkippedInstanceCount &&
                    report.terrain_patches == 74 &&
                    report.terrain_patch_samples == 74 * 65 * 65 &&
                    report.terrain_atlas_cells == 256 * 256 &&
                    report.terrain_atlas_bindings == 1390 &&
                    report.terrain_atlas_pages == 7 &&
                    report.terrain_atlas_uv_transforms == 256 * 256 &&
                    report.water_lookup_entries == 4864 &&
                    report.water_blocks == 413;
  return report.complete
             ? std::optional<RetailMission01MapRenderAssets>(std::move(assets))
             : std::nullopt;
}

bool RetailMission01MapRenderAssets::load_models() {
  const std::optional<RetailFhmView> parts = scene_.map_parts();
  if (!parts.has_value() || parts->child_count() < kMapModelCount) return false;
  models_.reserve(kMapModelCount);
  for (std::uint16_t selector = 0; selector < kMapModelCount; ++selector) {
    const std::optional<std::span<const std::uint8_t>> bytes =
        parts->child(selector);
    if (!bytes.has_value() || !load_model(selector, *bytes)) return false;
  }
  return true;
}

bool RetailMission01MapRenderAssets::load_model(
    std::uint16_t selector, std::span<const std::uint8_t> bytes) {
  const std::optional<NdxrContainer> container =
      NdxrContainer::Open(bytes.data(), bytes.size());
  if (!container.has_value()) return false;

  Mission01MapModel model;
  model.selector = selector;
  model.records.reserve(container->record_count());
  for (std::uint16_t index = 0; index < container->record_count(); ++index) {
    const std::optional<NdxrRecord> record = container->Record(index);
    if (!record.has_value() || record->index != index ||
        record->descriptor_count != 1) {
      return false;
    }
    const std::optional<Mission01MapDrawClass> draw_class =
        mission01_map_draw_class(record->name);
    const std::optional<NdxrDescriptor> descriptor =
        container->Descriptor(*record, 0);
    const std::optional<NdxrMaterial> material =
        container->Material(*record, 0, 0);
    if (!draw_class.has_value() || !descriptor.has_value() ||
        !material.has_value() || material->texture_count != 1) {
      return false;
    }
    for (unsigned slot = 1; slot < NdxrContainer::kMaterialSlots; ++slot) {
      if (container->Material(*record, 0, slot).has_value()) return false;
    }
    const std::optional<NdxrTextureRef> texture =
        container->TextureRef(*material, 0);
    std::optional<NdxrMesh> geometry = decode_ndxr_descriptor(
        *container, bytes.data(), bytes.size(), *descriptor);
    if (!texture.has_value() || texture->texture_id == 0 ||
        !geometry.has_value() ||
        geometry->normals.size() != geometry->positions.size() ||
        geometry->texcoords.size() != geometry->positions.size()) {
      return false;
    }

    report_.vertices += geometry->positions.size();
    report_.indices += geometry->indices.size();
    ++report_.source_records;
    ++report_.decoded_primitives;
    ++report_.texture_references;
    model.records.push_back({std::string(record->name), *draw_class,
                             texture->texture_id, std::move(*geometry)});
  }
  ++report_.model_files;
  models_.push_back(std::move(model));
  return true;
}

bool RetailMission01MapRenderAssets::load_textures() {
  std::set<std::uint32_t> identifiers;
  for (const Mission01MapModel& model : models_) {
    for (const Mission01MapPrimitive& record : model.records) {
      identifiers.insert(record.texture_identifier);
    }
  }
  if (identifiers.size() != kMapModelCount) return false;

  const std::optional<std::vector<Mission01TextureResourceView>> resources =
      scene_.texture_resources();
  if (!resources.has_value()) return false;
  textures_.reserve(identifiers.size());
  for (const Mission01TextureResourceView& resource : *resources) {
    if (!identifiers.contains(resource.identifier)) continue;
    const std::optional<NtxrDescriptor> descriptor = parse_ntxr_descriptor(
        resource.bytes.data(), resource.bytes.size());
    if (!descriptor.has_value()) return false;
    textures_.push_back({resource.identifier, *descriptor, resource.bytes});
  }
  if (textures_.size() != identifiers.size()) return false;
  report_.texture_assets = textures_.size();
  return true;
}

bool RetailMission01MapRenderAssets::load_terrain() {
  const TerrainField& terrain = scene_.terrain();
  terrain_resource_.patch_grid = terrain.patch_grid();
  terrain_resource_.patch_samples = terrain.patch_samples();
  const std::optional<std::span<const std::uint8_t>> atlas_map =
      scene_.map_resource(Mission01MapResource::TerrainAtlasMap);
  const std::optional<std::span<const std::uint8_t>> atlas_index =
      scene_.map_resource(Mission01MapResource::TerrainAtlasIndex);
  const std::optional<RetailFhmView> atlas = scene_.terrain_atlas();
  if (terrain_resource_.patch_grid.size() != 256 ||
      terrain_resource_.patch_samples.size() != 74 * 65 * 65 ||
      !atlas_map.has_value() || atlas_map->size() != 256 ||
      !atlas_index.has_value() ||
      atlas_index->size() !=
          kTerrainAtlasRecordCount * kTerrainAtlasRecordBytes ||
      !atlas.has_value() || atlas->child_count() != 8) {
    return false;
  }

  terrain_resource_.atlas_pages.reserve(kTerrainAtlasPageCount);
  for (std::uint8_t page = 0; page < kTerrainAtlasPageCount; ++page) {
    const std::optional<std::span<const std::uint8_t>> source =
        atlas->child(page);
    const std::optional<NtxrDescriptor> descriptor =
        source.has_value()
            ? parse_ntxr_descriptor(source->data(), source->size())
            : std::nullopt;
    const std::optional<std::uint32_t> identifier =
        source.has_value()
            ? ntxr_gidx_identifier(source->data(), source->size())
            : std::nullopt;
    const std::uint16_t expected_height = page == 6 ? 1024 : 4096;
    if (!source.has_value() || !descriptor.has_value() ||
        !identifier.has_value() || descriptor->width != 4096 ||
        descriptor->height != expected_height) {
      return false;
    }
    terrain_resource_.atlas_pages.push_back(
        {page, *identifier, *descriptor, *source});
  }

  std::set<std::uint16_t> distinct;
  terrain_resource_.atlas_cells.reserve(
      kTerrainAtlasSide * kTerrainAtlasSide);
  for (std::size_t z = 0; z < kTerrainAtlasSide; ++z) {
    for (std::size_t x = 0; x < kTerrainAtlasSide; ++x) {
      const std::uint8_t record =
          (*atlas_map)[(z >> 4) * 16 + (x >> 4)];
      if (record >= kTerrainAtlasRecordCount) return false;
      const std::size_t offset =
          static_cast<std::size_t>(record) * kTerrainAtlasRecordBytes +
          (((z & 15) * 16 + (x & 15)) * 2);
      const std::uint8_t page = (*atlas_index)[offset];
      const std::uint8_t tile = (*atlas_index)[offset + 1];
      if (page >= terrain_resource_.atlas_pages.size()) return false;
      const NtxrDescriptor& descriptor =
          terrain_resource_.atlas_pages[page].descriptor;
      const std::uint32_t column = tile % kTerrainAtlasColumns;
      const std::uint32_t row = tile / kTerrainAtlasColumns;
      if ((column + 1) * kTerrainAtlasTilePixels > descriptor.width ||
          (row + 1) * kTerrainAtlasTilePixels > descriptor.height) {
        return false;
      }
      terrain_resource_.atlas_cells.push_back({page, tile});
      distinct.insert(static_cast<std::uint16_t>((page << 8) | tile));
    }
  }
  report_.terrain_patches = terrain.patch_count();
  report_.terrain_patch_samples = terrain_resource_.patch_samples.size();
  report_.terrain_atlas_cells = terrain_resource_.atlas_cells.size();
  report_.terrain_atlas_bindings = distinct.size();
  report_.terrain_atlas_pages = terrain_resource_.atlas_pages.size();
  for (std::size_t z = 0; z < kTerrainAtlasSide; ++z) {
    for (std::size_t x = 0; x < kTerrainAtlasSide; ++x) {
      if (!terrain_resource_.atlas_uv_transform(x, z).has_value()) return false;
      ++report_.terrain_atlas_uv_transforms;
    }
  }
  return true;
}

bool RetailMission01MapRenderAssets::load_water() {
  const MapWaterGrid& water = scene_.water();
  const std::span<const std::uint8_t> mca = water.mca_bytes();
  const std::span<const std::uint8_t> mci = water.mci_bytes();
  const std::span<const std::uint8_t> mcd = water.mcd_bytes();
  if (mca.size() != kWaterHeaderBytes + 256 ||
      mci.size() < kWaterHeaderBytes ||
      (mci.size() - kWaterHeaderBytes) % 2 != 0 ||
      mcd.size() != kWaterHeaderBytes +
                        water.block_count() * kWaterBlockBytes) {
    return false;
  }
  std::copy_n(mca.begin() + kWaterHeaderBytes,
              water_resource_.coarse_groups.size(),
              water_resource_.coarse_groups.begin());
  const std::size_t lookups = (mci.size() - kWaterHeaderBytes) / 2;
  water_resource_.cell_blocks.reserve(lookups);
  for (std::size_t index = 0; index < lookups; ++index) {
    const std::uint16_t block =
        be16(mci.data() + kWaterHeaderBytes + index * 2);
    if (block >= water.block_count()) return false;
    water_resource_.cell_blocks.push_back(block);
  }
  water_resource_.block_bits.assign(mcd.begin() + kWaterHeaderBytes,
                                    mcd.end());
  report_.water_lookup_entries = water_resource_.cell_blocks.size();
  report_.water_blocks =
      water_resource_.block_bits.size() / kWaterBlockBytes;
  return true;
}

bool RetailMission01MapRenderAssets::bind_instances() {
  std::vector<std::vector<bool>> referenced;
  referenced.reserve(models_.size());
  for (const Mission01MapModel& model : models_) {
    referenced.emplace_back(model.records.size(), false);
  }

  const std::vector<MapInstance>& source = scene_.placement().instances();
  report_.source_instances = source.size();
  draw_instances_.reserve(source.size());
  for (const MapInstance& instance : source) {
    if (instance.selector >= models_.size()) return false;
    const Mission01MapModel& model = models_[instance.selector];
    if (instance.record_index >= model.records.size() ||
        referenced[instance.selector][instance.record_index]) {
      return false;
    }
    const Mission01MapPrimitive& primitive =
        model.records[instance.record_index];
    if (!same_class(primitive.draw_class, instance.draw_class)) return false;
    referenced[instance.selector][instance.record_index] = true;
    ++report_.record_bindings;
    if (!instance.accepted) {
      ++report_.skipped_instances;
      continue;
    }
    ++report_.draw_classes[instance.draw_class];
    draw_instances_.push_back(
        {instance.world_x, instance.world_y, instance.world_z,
         instance.selector, instance.record_index, primitive.draw_class});
  }
  for (const std::vector<bool>& model : referenced) {
    if (std::find(model.begin(), model.end(), false) != model.end()) return false;
  }
  report_.draw_instances = draw_instances_.size();
  return true;
}

const Mission01MapPrimitive* RetailMission01MapRenderAssets::primitive_for(
    const Mission01MapDrawInstance& instance) const noexcept {
  if (instance.selector >= models_.size()) return nullptr;
  const Mission01MapModel& model = models_[instance.selector];
  if (instance.record_index >= model.records.size()) return nullptr;
  const Mission01MapPrimitive& primitive = model.records[instance.record_index];
  return primitive.draw_class == instance.draw_class ? &primitive : nullptr;
}

std::optional<DecodedTexture> RetailMission01MapRenderAssets::decode_texture(
    std::uint32_t identifier, bool swap_16, NtxrRefusal* refusal) const noexcept {
  const auto found = std::lower_bound(
      textures_.begin(), textures_.end(), identifier,
      [](const Mission01MapTextureAsset& asset, std::uint32_t value) {
        return asset.identifier < value;
      });
  if (found == textures_.end() || found->identifier != identifier) {
    if (refusal != nullptr) *refusal = NtxrRefusal::BadHeader;
    return std::nullopt;
  }
  return decode_ntxr_base_level(found->source.data(), found->source.size(),
                                swap_16, refusal);
}

}  // namespace ac6::retail
