#include "ac6/retail_mission_world_bundle.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL %s\n", message);
    ++failures;
  }
}

bool magic(std::optional<std::span<const std::uint8_t>> bytes,
           std::array<std::uint8_t, 4> expected) {
  return bytes.has_value() && bytes->size() >= expected.size() &&
         std::equal(expected.begin(), expected.end(), bytes->begin());
}

}  // namespace

int main(int argc, char** argv) {
  using ac6::retail::RetailMissionWorldBundle;
  for (std::uint32_t mission = 1; mission <= 15; ++mission) {
    check(ac6::retail::mission_world_data_table_entry(mission) ==
              std::optional<std::uint32_t>(118u + mission),
          "PAL mission world mapping is consecutive");
  }
  check(!ac6::retail::mission_world_data_table_entry(0).has_value() &&
            !ac6::retail::mission_world_data_table_entry(16).has_value(),
        "world mapping rejects ids outside the PAL campaign");
  if (argc < 2) return failures == 0 ? 0 : 1;

  ac6::RetailContentStore store;
  check(store.open(argv[1]), "qualified cache opens");
  if (!store.valid()) return failures == 0 ? 1 : failures;
  for (std::uint32_t mission = 1; mission <= 15; ++mission) {
    const std::optional<RetailMissionWorldBundle> world =
        RetailMissionWorldBundle::open(store, mission);
    check(world.has_value(), "qualified world hierarchy opens");
    if (!world.has_value()) continue;
    check(world->data_table_entry() == 118u + mission &&
              world->root_child_count() == 23,
          "world identity and root count are retained");
    const std::optional<ac6::retail::RetailFhmView> map = world->map();
    const std::optional<ac6::retail::RetailFhmView> mapset = world->mapset();
    check(map.has_value() && map->child_count() == 17 && mapset.has_value() &&
              mapset->child_count() == 12,
          "map and mapset FHM tables retain the common PAL shape");
    if (!map.has_value() || !mapset.has_value()) continue;
    const std::optional<ac6::retail::RetailFhmView> map_parts = map->nested(14);
    const std::optional<ac6::retail::RetailFhmView> map_part_textures =
        map->nested(15);
    const std::optional<ac6::retail::RetailFhmView> terrain_atlas = map->nested(16);
    const std::optional<ac6::retail::RetailFhmView> mapset_models = mapset->nested(5);
    const std::optional<ac6::retail::RetailFhmView> mapset_textures =
        mapset->nested(6);
    check(map_parts.has_value() && map_parts->child_count() == 256 &&
              map_part_textures.has_value() &&
              map_part_textures->child_count() == 256 &&
              terrain_atlas.has_value() && terrain_atlas->child_count() == 8 &&
              mapset_models.has_value() && mapset_models->child_count() == 8 &&
              mapset_textures.has_value() && mapset_textures->child_count() == 8,
          "map/model/texture FHM tables retain the common PAL shape");
    check(magic(world->map_resource(1), {'M', 'C', 'A', 0}) &&
              magic(world->map_resource(2), {'M', 'C', 'D', 0}) &&
              magic(world->map_resource(3), {'M', 'C', 'I', 0}),
          "map MCA/MCD/MCI resources are bound by index");
    check(magic(world->mapset_resource(7), {'N', 'T', 'X', 'R'}) &&
              magic(world->mapset_resource(11), {'N', 'T', 'X', 'R'}),
          "mapset NTXR resources are bound by index");
  }
  std::printf("retail mission world bundle missions=15\n");
  return failures == 0 ? 0 : 1;
}
