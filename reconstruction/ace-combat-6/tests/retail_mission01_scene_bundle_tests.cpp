#include "ac6/retail_mission01_scene_bundle.h"
#include "ac6/retail_mission01_map_render_assets.h"
#include "ac6/ntxr_texture.h"
#include "ac6/retail_ndxr_container.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::printf("FAIL  %s\n", message);
    ++failures;
  }
}

void put16(std::vector<std::uint8_t>& bytes, std::size_t at,
           std::uint16_t value) {
  bytes[at] = static_cast<std::uint8_t>(value >> 8);
  bytes[at + 1] = static_cast<std::uint8_t>(value);
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t at,
           std::uint32_t value) {
  bytes[at] = static_cast<std::uint8_t>(value >> 24);
  bytes[at + 1] = static_cast<std::uint8_t>(value >> 16);
  bytes[at + 2] = static_cast<std::uint8_t>(value >> 8);
  bytes[at + 3] = static_cast<std::uint8_t>(value);
}

std::vector<std::uint8_t> blob(std::size_t size, const char magic[4]) {
  std::vector<std::uint8_t> bytes(size, 0);
  std::memcpy(bytes.data(), magic, 4);
  return bytes;
}

std::vector<std::uint8_t> make_fhm(
    const std::vector<std::vector<std::uint8_t>>& children) {
  const std::size_t table_end = 0x14 + children.size() * 16;
  std::size_t total = table_end;
  for (const std::vector<std::uint8_t>& child : children) total += child.size();
  std::vector<std::uint8_t> bytes;
  bytes.reserve(total);
  bytes.resize(table_end, 0);
  std::memcpy(bytes.data(), "FHM ", 4);
  bytes[4] = 1;
  bytes[5] = 1;
  bytes[7] = 0x10;
  put32(bytes, 0x10, static_cast<std::uint32_t>(children.size()));
  for (std::size_t index = 0; index < children.size(); ++index) {
    if (children[index].empty()) continue;
    put32(bytes, 0x14 + index * 4, static_cast<std::uint32_t>(bytes.size()));
    put32(bytes, 0x14 + children.size() * 4 + index * 4,
          static_cast<std::uint32_t>(children[index].size()));
    bytes.insert(bytes.end(), children[index].begin(), children[index].end());
  }
  return bytes;
}

std::vector<std::vector<std::uint8_t>> prefixed_children(
    std::size_t count, std::size_t live, const char magic[4]) {
  std::vector<std::vector<std::uint8_t>> children(count);
  for (std::size_t index = 0; index < live; ++index) {
    children[index] = blob(4, magic);
  }
  return children;
}

std::vector<std::uint8_t> valid_world() {
  std::vector<std::vector<std::uint8_t>> map(17);
  map[1] = blob(272, "MCA\0");
  map[2] = blob(211472, "MCD\0");
  map[3] = blob(9744, "MCI\0");
  put16(map[2], 8, 413);
  put16(map[3], 8, 4864);
  map[4].resize(256, 0);
  map[5].resize(1250604, 0);
  map[9].resize(256, 0);
  map[10].resize(12288, 0);
  map[11].resize(73184, 0);
  put32(map[11], 0, 4318);
  put32(map[11], 4, 4096);
  map[12].resize(145944, 0);
  map[13].resize(197632, 0);
  map[14] = make_fhm(prefixed_children(256, 170, "NDXR"));
  map[15] = make_fhm(prefixed_children(256, 170, "NTXR"));
  map[16] = make_fhm(prefixed_children(8, 7, "NTXR"));

  std::vector<std::vector<std::uint8_t>> mapset(12);
  mapset[5] = make_fhm(prefixed_children(8, 8, "NDXR"));
  mapset[6] = make_fhm(prefixed_children(8, 8, "NTXR"));
  for (std::size_t index = 7; index <= 11; ++index) {
    mapset[index] = blob(4, "NTXR");
  }

  std::vector<std::vector<std::uint8_t>> root(23);
  root[21] = make_fhm(map);
  root[22] = make_fhm(mapset);
  return make_fhm(root);
}

std::size_t descendant_offset(
    std::vector<std::uint8_t>& world,
    std::span<const std::uint32_t> path) {
  const std::optional<ac6::retail::RetailFhmView> root =
      ac6::retail::RetailFhmView::open(world);
  if (!root.has_value()) return world.size();
  const std::optional<std::span<const std::uint8_t>> child =
      root->descendant(path);
  return child.has_value()
             ? static_cast<std::size_t>(child->data() - world.data())
             : world.size();
}

void diagnose_model_bindings(const ac6::retail::RetailMission01SceneBundle& scene) {
  using namespace ac6::retail;
  const std::array<std::pair<const char*, std::optional<RetailFhmView>>, 2> sets{{
      {"parts", scene.map_parts()}, {"mapset", scene.mapset_models()}}};
  for (const auto& [label, models] : sets) {
    if (!models.has_value()) {
      std::printf("diagnostic missing model set %s\n", label);
      continue;
    }
    const std::uint32_t live_models =
        std::strcmp(label, "parts") == 0 ? 170 : models->child_count();
    for (std::uint32_t model_index = 0; model_index < live_models;
         ++model_index) {
      const auto bytes = models->child(model_index);
      const auto model = bytes.has_value()
                             ? NdxrContainer::Open(bytes->data(), bytes->size())
                             : std::nullopt;
      if (!model.has_value()) {
        std::printf("diagnostic NDXR refused %s/%u\n", label, model_index);
        return;
      }
      for (std::uint16_t record_index = 0;
           record_index < model->record_count(); ++record_index) {
        const auto record = model->Record(record_index);
        if (!record.has_value()) {
          std::printf("diagnostic record refused %s/%u/%u\n", label,
                      model_index, record_index);
          return;
        }
        for (std::uint16_t descriptor_index = 0;
             descriptor_index < record->descriptor_count;
             ++descriptor_index) {
          if (!model->Descriptor(*record, descriptor_index).has_value()) {
            std::printf("diagnostic descriptor refused %s/%u/%u/%u\n", label,
                        model_index, record_index, descriptor_index);
            return;
          }
          for (unsigned slot = 0; slot < NdxrContainer::kMaterialSlots; ++slot) {
            const auto material =
                model->Material(*record, descriptor_index, slot);
            if (!material.has_value()) continue;
            for (std::uint16_t texture = 0; texture < material->texture_count;
                 ++texture) {
              if (!model->TextureRef(*material, texture).has_value()) {
                std::printf(
                    "diagnostic texture ref refused %s/%u/%u/%u/%u/%u\n",
                    label, model_index, record_index, descriptor_index, slot,
                    texture);
                return;
              }
            }
          }
        }
      }
    }
  }
  std::printf("diagnostic all model records and texture refs parse\n");
}

void check_synthetic_bundle() {
  using namespace ac6::retail;
  check(mission01_map_draw_class("mapparts_m01_l_0001") ==
            Mission01MapDrawClass::Large &&
            mission01_map_draw_class("mapparts_m01_airport_0001") ==
                Mission01MapDrawClass::Large &&
            mission01_map_draw_class("mapparts_m01_m_0001") ==
                Mission01MapDrawClass::Medium &&
            mission01_map_draw_class("mapparts_m01_s_0001") ==
                Mission01MapDrawClass::Small &&
            mission01_map_draw_class("mapparts_m01_x_0001") ==
                Mission01MapDrawClass::Extra,
        "the five qualified record-name tokens map to four draw classes");
  check(!mission01_map_draw_class("mapparts_m01_unknown_0001").has_value() &&
            !mission01_map_draw_class("mapparts_m01_l").has_value(),
        "an unknown or unterminated record-name token fails closed");
  const std::vector<std::uint8_t> world = valid_world();
  const std::optional<RetailMission01SceneBundle> scene =
      RetailMission01SceneBundle::open_payload_for_testing(world);
  check(scene.has_value(), "the complete synthetic entry-119 hierarchy opens");
  if (!scene.has_value()) return;
  check(!scene->store_backed(), "the test overload cannot claim retail provenance");
  check(scene->terrain().patch_count() == 74, "the terrain has 74 bounded patches");
  check(scene->water().group_count() == 4864 &&
            scene->water().block_count() == 413,
        "the MCA/MCI/MCD triple opens through the product reader");
  check(scene->placement().instances().size() == 4318,
        "the placement partition closes at 4318 records");
  check(scene->map_parts().has_value() && scene->map_parts()->child_count() == 256,
        "the parallel map-parts container retains all 256 slots");
  check(scene->map_part_textures().has_value() &&
            scene->map_part_textures()->child_count() == 256,
        "the parallel map-texture container retains all 256 slots");
  check(scene->terrain_atlas().has_value() &&
            scene->terrain_atlas()->child_count() == 8,
        "the seven-page terrain atlas retains its empty eighth slot");
  check(scene->map_resource(Mission01MapResource::PlacedTrees).has_value() &&
            scene->map_resource(Mission01MapResource::PlacedTrees)->size() == 145944,
        "the placed-tree resource is addressed by retail index, not filename");
  const std::optional<RetailMission01SceneBundle> placeholder_scene =
      RetailMission01SceneBundle::open_payload_for_testing(world);
  check(placeholder_scene.has_value() &&
            !RetailMission01MapRenderAssets::build_for_testing(
                 std::move(*placeholder_scene))
                 .has_value(),
        "placeholder NDXR payloads cannot become render assets");

  std::vector<std::uint8_t> invalid = world;
  const std::array<std::uint32_t, 3> texture_path{21, 15, 0};
  const std::size_t texture = descendant_offset(invalid, texture_path);
  check(texture < invalid.size(), "the negative control found the texture child");
  invalid[texture] = 'X';
  check(!RetailMission01SceneBundle::open_payload_for_testing(invalid).has_value(),
        "a map texture with the wrong identity fails closed");

  invalid = world;
  const std::array<std::uint32_t, 2> grid_path{21, 4};
  const std::size_t grid = descendant_offset(invalid, grid_path);
  check(grid < invalid.size(), "the negative control found the terrain grid");
  invalid[grid] = 74;
  check(!RetailMission01SceneBundle::open_payload_for_testing(invalid).has_value(),
        "a terrain grid naming a missing patch fails closed");

  invalid = world;
  invalid.pop_back();
  check(!RetailMission01SceneBundle::open_payload_for_testing(invalid).has_value(),
        "a truncated nested world fails closed");
}

void check_qualified_cache(const char* cache_path) {
  using namespace ac6::retail;
  ac6::RetailContentStore store;
  check(store.open(cache_path), "the qualified PAL cache opens");
  if (!store.valid()) return;
  const std::optional<RetailMission01SceneBundle> scene =
      RetailMission01SceneBundle::open(store);
  check(scene.has_value(), "the qualified entry-119 hierarchy opens");
  if (!scene.has_value()) {
    std::vector<std::uint8_t> payload;
    if (store.read_payload(119, payload)) {
      const std::optional<RetailMission01SceneBundle> diagnostic =
          RetailMission01SceneBundle::open_payload_for_testing(payload);
      const std::optional<Mission01TextureBindingReport> bindings =
          diagnostic.has_value() ? diagnostic->audit_texture_bindings()
                                 : std::nullopt;
      if (bindings.has_value()) {
        std::printf(
            "refused bindings models=%zu records=%zu descriptors=%zu "
            "materials=%zu refs=%zu unique=%zu textures=%zu missing=%zu "
            "unbound=%zu complete=%d\n",
            bindings->model_files, bindings->model_records,
            bindings->descriptors, bindings->material_slots,
            bindings->texture_references, bindings->unique_texture_references,
            bindings->texture_wrappers, bindings->missing_texture_ids,
            bindings->unbound_descriptors, bindings->complete ? 1 : 0);
      } else {
        std::printf("refused bindings report unavailable\n");
        if (diagnostic.has_value()) diagnose_model_bindings(*diagnostic);
      }
    }
    return;
  }
  check(scene->store_backed(), "the product path retains content provenance");
  check(ac6::sha256_hex(scene->content_index_sha256()) ==
            "349f5f49fe1acf19984c6470a5d3f16adf3029e36c93e24da8cb3ec58b4cdfd0",
        "the scene carries the sealed cache generation identity");
  check(scene->terrain().patch_count() == 74,
        "the qualified terrain reader sees 74 patches");
  check(scene->placement().instances().size() == 4318,
        "the qualified placement reader sees 4318 instances");
  check(scene->map_parts().has_value() && scene->map_parts()->child(169).has_value(),
        "all 170 qualified map-part models are live");
  check(scene->map_part_textures().has_value() &&
            scene->map_part_textures()->child(169).has_value(),
        "all 170 qualified map-part texture wrappers are live");
  const std::optional<Mission01TextureBindingReport> bindings =
      scene->audit_texture_bindings();
  check(bindings.has_value() && bindings->complete,
        "every NDXR material texture id resolves in the retail registry");
  if (bindings.has_value()) {
    check(bindings->model_files == 178 && bindings->texture_wrappers == 192,
          "the binding audit covers all 178 NDXR and 192 NTXR resources");
    check(bindings->model_records == 4326 && bindings->descriptors == 4326 &&
              bindings->material_slots == 4326 &&
              bindings->texture_references == 4326 &&
              bindings->unique_texture_references == 178,
          "the complete descriptor-to-texture population is pinned");
    check(bindings->missing_texture_ids == 0 &&
              bindings->unbound_descriptors == 0,
          "no descriptor or referenced texture remains unbound");
    const auto first_map_texture = scene->texture_by_gidx(4169);
    check(first_map_texture.has_value() && first_map_texture->size() >= 4 &&
              (*first_map_texture)[0] == 'N' && (*first_map_texture)[1] == 'T',
          "a material GIDX resolves to its bounded NTXR wrapper");
    std::printf(
        "retail bindings models=%zu records=%zu descriptors=%zu materials=%zu "
        "refs=%zu unique=%zu textures=%zu\n",
        bindings->model_files, bindings->model_records, bindings->descriptors,
        bindings->material_slots, bindings->texture_references,
        bindings->unique_texture_references, bindings->texture_wrappers);
  }

  const std::optional<RetailMission01MapRenderAssets> assets =
      RetailMission01MapRenderAssets::open(store);
  check(assets.has_value(),
        "the qualified map bundle builds immutable render assets");
  if (!assets.has_value()) return;
  const Mission01MapRenderAssetReport& render = assets->report();
  check(assets->store_backed() && render.complete,
        "the render assets retain the sealed content generation");
  check(render.model_files == 170 && render.source_records == 4318 &&
            render.decoded_primitives == 4318,
        "all 170 models and 4318 record primitives decode once");
  check(render.texture_references == 4318 && render.texture_assets == 170,
        "the primitives close on the 170 map-part texture assets");
  check(render.source_instances == 4318 && render.record_bindings == 4318 &&
            render.draw_instances == 4226 && render.skipped_instances == 92,
        "the exact placement-to-record bijection excludes only retail's skips");
  check(render.draw_classes == std::array<std::size_t, 4>{345, 584, 3277, 20},
        "the persistent draw list retains the four accepted classes");
  check(render.vertices == 112719 && render.indices == 138610,
        "the immutable geometry population is pinned exactly");
  check(render.terrain_patches == 74 &&
            render.terrain_patch_samples == 74u * 65u * 65u &&
            render.terrain_atlas_cells == 256u * 256u &&
            render.terrain_atlas_bindings == 1390 &&
            render.terrain_atlas_pages == 7,
        "the compact terrain and complete page/tile binding are pinned");
  check(render.water_lookup_entries == 4864 && render.water_blocks == 413,
        "the persistent water upload retains every lookup and bit block");
  std::size_t resolved_draws = 0;
  for (const Mission01MapDrawInstance& draw : assets->draw_instances()) {
    if (assets->primitive_for(draw) != nullptr) ++resolved_draws;
  }
  check(resolved_draws == 4226,
        "every persistent draw command resolves one immutable primitive");
  Mission01MapDrawInstance invalid = assets->draw_instances().front();
  invalid.record_index = 0xFFFF;
  check(assets->primitive_for(invalid) == nullptr,
        "an out-of-range draw command cannot alias another primitive");
  NtxrRefusal refusal = NtxrRefusal::BadHeader;
  const std::optional<DecodedTexture> texture =
      assets->decode_texture(4169, true, &refusal);
  check(texture.has_value() && refusal == NtxrRefusal::None &&
            texture->width == 512 && texture->height == 512 &&
            texture->pixels.size() == 512u * 512u,
        "a persistent map texture decodes explicitly for one-time upload");
  check(!assets->decode_texture(0xFFFFFFFFu, true, &refusal).has_value() &&
            refusal == NtxrRefusal::BadHeader,
        "an unregistered texture id fails closed");

  const Mission01TerrainRenderResource& terrain = assets->terrain_resource();
  check(terrain.patch_grid.size() == 256 &&
            terrain.patch_samples.size() == 312650 &&
            terrain.atlas_pages.size() == 7 &&
            terrain.atlas_cells.size() == 65536,
        "terrain upload sources remain compact and persistent");
  std::array<std::size_t, 7> page_cells{};
  for (const Mission01TerrainAtlasCell& cell : terrain.atlas_cells) {
    if (cell.page < page_cells.size()) ++page_cells[cell.page];
  }
  check(page_cells ==
            std::array<std::size_t, 7>{22183, 11095, 460, 31018, 245, 495, 40},
        "all 65536 retail terrain-cell page bindings are retained");
  check(terrain.atlas_pages[0].descriptor.width == 4096 &&
            terrain.atlas_pages[0].descriptor.height == 4096 &&
            terrain.atlas_pages[6].descriptor.width == 4096 &&
            terrain.atlas_pages[6].descriptor.height == 1024,
        "the quarter-height seventh atlas page keeps its own dimensions");
  check(terrain.atlas_cell(255, 255) != nullptr &&
            terrain.atlas_cell(256, 0) == nullptr,
        "terrain atlas addressing is bounded at 256 cells per side");

  const Mission01WaterRenderResource& water = assets->water_resource();
  check(water.cell_blocks.size() == 4864 &&
            water.block_bits.size() == 413u * 512u,
        "the water upload uses the compact retail lookup representation");
  std::size_t water_compared = 0;
  for (std::size_t z = 0; z < 1024; z += 4) {
    for (std::size_t x = 0; x < 1024; x += 4) {
      const float world_x = static_cast<float>(x) * 128.0F - 65536.0F + 0.25F;
      const float world_z = static_cast<float>(z) * 128.0F - 65536.0F + 0.25F;
      bool source_bit = false;
      bool upload_bit = false;
      const bool source_ok = assets->scene().water().query(
          world_x, world_z, &source_bit);
      const bool upload_ok = water.query(world_x, world_z, &upload_bit);
      if (source_ok == upload_ok && (!source_ok || source_bit == upload_bit)) {
        ++water_compared;
      }
    }
  }
  check(water_compared == 256u * 256u,
        "the upload water query matches all off-lattice source probes");
  bool outside_bit = false;
  check(!water.query(-70000.0F, 0.0F, &outside_bit),
        "the persistent water resource preserves the retail bounds refusal");
  std::printf(
      "retail map assets models=%zu records=%zu vertices=%zu indices=%zu "
      "textures=%zu draws=%zu skipped=%zu atlas=%zu water=%zu/%zu\n",
      render.model_files, render.source_records, render.vertices,
      render.indices, render.texture_assets, render.draw_instances,
      render.skipped_instances, render.terrain_atlas_cells,
      render.water_lookup_entries, render.water_blocks);
}

}  // namespace

int main(int argc, char** argv) {
  check_synthetic_bundle();
  if (argc >= 2) check_qualified_cache(argv[1]);
  if (failures == 0) std::printf("retail Mission 01 scene bundle OK\n");
  return failures == 0 ? 0 : 1;
}
