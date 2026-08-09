// The placement list, over Mission 01's real map container, cross-checked
// against the ported heightfield.
//
// DATA DRIVEN, exiting 77 when the container is absent: the extracted corpus is
// retail content and is never committed.
#include "ac6/retail_map_placement.h"
#include "ac6/retail_ndxr_container.h"
#include "ac6/retail_terrain_field.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
int failures = 0;
void check(bool c, const char* w) {
  if (!c) { std::printf("FAIL  %s\n", w); ++failures; }
}
std::vector<std::uint8_t> slurp(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
}
// A deterministic generator, so the null model is the same on every machine.
std::uint32_t next(std::uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }

// THE DRAW CLASS, JOINED AGAINST THE PACKAGE'S OWN RECORD NAMES.
//
// Each record name carries a class token after `_m01_`. If bits 30..31 are that
// class, the two histograms must agree -- and they do, to the unit, across four
// buckets and 4,318 records. Nothing is fitted: the tag counts come from the
// placement list and the name counts from the models, which are different files
// parsed by different code.
void check_class_join(const std::filesystem::path& dir, const std::size_t quad[4],
                      std::size_t skipped) {
    std::map<std::string, std::size_t> tokens;
    std::size_t records = 0;
    for (int id = 0; id < 170; ++id) {
      char name[64];
      std::snprintf(name, sizeof(name), "014_FHM/%03d_NDXR.ndxr", id);
      const auto path = dir / name;
      if (!std::filesystem::exists(path)) continue;
      const std::vector<std::uint8_t> blob = slurp(path);
      const auto container = ac6::retail::NdxrContainer::Open(blob.data(), blob.size());
      if (!container) continue;
      for (std::uint16_t r = 0; r < container->record_count(); ++r) {
        const auto record = container->Record(r);
        if (!record) continue;
        ++records;
        const std::string text(record->name);
        const auto at = text.find("_m01_");
        if (at == std::string::npos) continue;
        const auto end = text.find('_', at + 5);
        ++tokens[text.substr(at + 5, end - (at + 5))];
      }
    }
    check(records == 4318, "the package holds one record per instance");
    check(tokens["l"] + tokens["airport"] == quad[0], "class 0 is l and airport");
    check(tokens["m"] == quad[1], "class 1 is m");
    check(tokens["s"] == quad[2], "class 2 is s");
    // class 3 counts only the ACCEPTED records; x includes the 92 retail skips.
    check(tokens["x"] == quad[3] + skipped,
          "class 3 is x, less the records retail skips");
    std::printf("class join: l+airport %zu/%zu  m %zu/%zu  s %zu/%zu  x %zu/%zu\n",
                tokens["l"] + tokens["airport"], quad[0], tokens["m"], quad[1],
                tokens["s"], quad[2], tokens["x"], quad[3]);
}

}  // namespace

int main(int argc, char** argv) {
  using namespace ac6::retail;

  // Pure checks first, so a clean clone still exercises something.
  check(MapPlacement::open(nullptr, 4096) == std::nullopt, "a null list is refused");
  const std::uint8_t four[4] = {0, 0, 0, 0};
  check(MapPlacement::open(four, 4) == std::nullopt,
        "a four-byte list is empty, per 0x820FBF5C");
  check(MapPlacement::world_from_local(0, 0.0F) == -kPlacementOriginBias,
        "cell 0 sits at -61440");
  check(MapPlacement::world_from_local(8, 0.0F) == 8.0F * kPlacementCoarseUnits
                                                   - kPlacementOriginBias,
        "the transform is coarse * 8192 - 61440");
  // A list whose header does not partition its body must be refused.
  std::vector<std::uint8_t> bogus(kPlacementHeaderBytes + 32, 0);
  bogus[3] = 99;                                        // cell 0 claims 99
  bogus[7] = 0; bogus[6] = 0x10;                        // at offset 0x1000
  check(MapPlacement::open(bogus.data(), bogus.size()) == std::nullopt,
        "a header that does not partition the body is refused");

  if (argc < 2) { std::fprintf(stderr, "usage: tests MAP_FHM_DIR\n"); return 77; }
  const std::filesystem::path dir = argv[1];
  const auto pdl_path = dir / "011_00_00_00_00.bin";
  const auto grid_path = dir / "004_00_01_02_03.bin";
  const auto patch_path = dir / "005_Bl_02_b8.bin";
  for (const auto& p : {pdl_path, grid_path, patch_path}) {
    if (!std::filesystem::exists(p)) {
      std::fprintf(stderr, "no map container at %s — skipping\n",
                   dir.string().c_str());
      return failures == 0 ? 77 : 1;
    }
  }
  const std::vector<std::uint8_t> pdl = slurp(pdl_path);
  const auto placement = MapPlacement::open(pdl.data(), pdl.size());
  check(placement.has_value(), "the placement list opens");
  if (!placement) return 1;

  check(placement->header_partitions_body(), "the header partitions the body");
  check(placement->header_total() == 4318, "4318 instances by the header");
  check(placement->instances().size() == 4318, "4318 instances decoded");

  std::set<std::uint16_t> ids;
  float min_x = 1e30F, max_x = -1e30F, min_z = 1e30F, max_z = -1e30F;
  std::size_t zero_y = 0;
  for (const MapInstance& q : placement->instances()) {
    ids.insert(q.part_id);
    min_x = std::fmin(min_x, q.world_x); max_x = std::fmax(max_x, q.world_x);
    min_z = std::fmin(min_z, q.world_z); max_z = std::fmax(max_z, q.world_z);
    if (q.world_y == 0.0F) ++zero_y;
    // Every instance must land inside the cell the header filed it under.
    const float cx0 = MapPlacement::world_from_local(q.coarse_x, 0.0F);
    const float cz0 = MapPlacement::world_from_local(q.coarse_z, 0.0F);
    check(std::fabs(q.world_x - cx0) <= kPlacementCoarseUnits * 0.5F + 1.0F &&
          std::fabs(q.world_z - cz0) <= kPlacementCoarseUnits * 0.5F + 1.0F,
          "an instance lies inside its own coarse cell");
  }
  check(ids.size() == 173 && *ids.begin() == 0 && *ids.rbegin() == 172,
        "173 part ids, 0..172");
  check(zero_y == 4138, "4138 instances sit at y exactly zero");
  check(max_x - min_x < 40000.0F && max_z - min_z < 40000.0F,
        "the instances are a city, not a scatter over a 131072-unit map");

  // THE CONTROL: land them on the ported heightfield, which is a different file
  // decoded from a different retail function, and score a null model of the same
  // size. A wrong header reading scatters, and a scatter scores the null model.
  const std::vector<std::uint8_t> grid = slurp(grid_path);
  const std::vector<std::uint8_t> patches = slurp(patch_path);
  const auto field =
      TerrainField::open(grid.data(), grid.size(), patches.data(), patches.size());
  check(field.has_value(), "the heightfield opens");
  if (!field) return 1;

  std::size_t placed_flat = 0, placed_seen = 0;
  for (const MapInstance& q : placement->instances()) {
    float h = 0.0F;
    if (!field->height_at(q.world_x, q.world_z, &h)) continue;
    ++placed_seen;
    if (h < 1.0F) ++placed_flat;
  }
  std::uint32_t seed = 20240809u;
  std::size_t null_flat = 0, null_seen = 0;
  for (std::size_t i = 0; i < placement->instances().size(); ++i) {
    const float x = static_cast<float>(next(seed) % 130000u) - 65000.0F;
    const float z = static_cast<float>(next(seed) % 130000u) - 65000.0F;
    float h = 0.0F;
    if (!field->height_at(x, z, &h)) continue;
    ++null_seen;
    if (h < 1.0F) ++null_flat;
  }
  const double placed_rate = 100.0 * placed_flat / placed_seen;
  const double null_rate = 100.0 * null_flat / null_seen;
  check(placed_seen > 4000 && null_seen > 4000, "both populations are real");
  check(placed_rate > 95.0, "the instances sit on flat ground");
  check(null_rate < 70.0, "a random scatter does not");
  check(placed_rate > null_rate + 25.0,
        "and the gap is the finding, not the rate alone");

  // THE TAG'S FIELDS, as 0x82102340..0x82102364 mask them.
  std::size_t accepted = 0, kind7 = 0;
  std::size_t quad[4] = {0, 0, 0, 0};
  std::uint16_t sel_min = 0xFFFF, sel_max = 0;
  std::size_t mid_nonzero = 0;
  // The populations are kept apart deliberately: the 92 records retail skips
  // carry different field values, and averaging them in is how the first
  // version of these three assertions came out wrong.
  for (const MapInstance& q : placement->instances()) {
    if (!q.accepted) {
      if (((q.tag_high >> 9) & 3) != 0) ++mid_nonzero;
      continue;
    }
    ++accepted;
    if (q.kind == 7) ++kind7;
    ++quad[q.draw_class];
    sel_min = std::min(sel_min, q.selector);
    sel_max = std::max(sel_max, q.selector);
  }
  check(accepted == 4226 && kind7 == 4226,
        "4226 records carry 7 in the three-bit field and retail accepts those");
  check(placement->instances().size() - accepted == 92,
        "and skips the other 92, per 0x82102350");
  check(quad[0] == 345 && quad[1] == 584 && quad[2] == 3277 && quad[3] == 20,
        "the draw class takes all four values over the accepted records");
  check(sel_min == 8 && sel_max == 169,
        "the nine-bit selector runs 8..169 over the accepted records");
  check(mid_nonzero == 78,
        "bits 25..26 are zero on every accepted record and set on 78 skipped ones");

  // THE NINE-BIT FIELD IS THE MODEL INDEX, and this is the falsifiable form:
  // every selector must name a model file, and tag & 0xFFFF must not.
  std::size_t sel_ok = 0, low_missing = 0;
  for (std::uint16_t v : [&] {
         std::set<std::uint16_t> u;
         for (const MapInstance& q : placement->instances())
           if (q.accepted) u.insert(q.selector);
         return u;
       }()) {
    char n[64];
    std::snprintf(n, sizeof(n), "014_FHM/%03u_NDXR.ndxr", v);
    if (std::filesystem::exists(dir / n)) ++sel_ok;
  }
  for (std::uint16_t v : [&] {
         std::set<std::uint16_t> u;
         for (const MapInstance& q : placement->instances())
           if (q.accepted) u.insert(q.part_id);
         return u;
       }()) {
    char n[64];
    std::snprintf(n, sizeof(n), "014_FHM/%03u_NDXR.ndxr", v);
    if (!std::filesystem::exists(dir / n)) ++low_missing;
  }
  check(sel_ok == 160, "every one of the 160 selectors names a model file");
  check(low_missing == 3,
        "while three of the 173 low-sixteen values name no model at all");

  check_class_join(dir, quad, placement->instances().size() - accepted);

  // NEITHER FIELD IS A MODEL ID ON ITS OWN: the pair is unique per instance.
  // Asserted against the collision count chance would predict, because "all
  // distinct" means nothing until you know how surprising that is.
  std::set<std::uint32_t> pairs;
  std::set<std::uint16_t> lows, sels;
  for (const MapInstance& q : placement->instances()) {
    if (!q.accepted) continue;
    pairs.insert((static_cast<std::uint32_t>(q.part_id) << 16) | q.selector);
    lows.insert(q.part_id);
    sels.insert(q.selector);
  }
  check(pairs.size() == accepted, "every accepted instance has a distinct pair");
  check(lows.size() == 173 && sels.size() == 160, "173 low values, 160 selectors");
  {
    const double cells = static_cast<double>(lows.size()) * sels.size();
    const double expected = accepted * (accepted - 1) / (2.0 * cells);
    check(expected > 50.0,
          "and chance would have produced dozens of collisions, so zero is a fact");
  }

  // The four-fold statistic of cycle 1451 is REAL and its mechanism is the
  // two-bit field above, not an angle. Kept under test because the number is
  // true; see the header for what it actually measures. Asserting the
  // comparison, not just the value, because a single number cannot show that
  // the structure is specifically four-fold.
  const double r4 = placement->four_fold_resultant(4);
  check(r4 > 0.95, "the 4th harmonic resultant is near 1");
  for (int h : {1, 2, 3, 5, 6}) {
    check(placement->four_fold_resultant(h) < r4 - 0.2,
          "and it stands well clear of every neighbouring harmonic");
  }
  std::printf("R(4t) %.4f  vs 1t %.4f 2t %.4f 3t %.4f 5t %.4f 6t %.4f\n", r4,
              placement->four_fold_resultant(1), placement->four_fold_resultant(2),
              placement->four_fold_resultant(3), placement->four_fold_resultant(5),
              placement->four_fold_resultant(6));

  std::printf("instances %zu  ids %zu  x %.0f..%.0f  z %.0f..%.0f  "
              "flat %.1f%% vs null %.1f%%\n",
              placement->instances().size(), ids.size(), min_x, max_x, min_z,
              max_z, placed_rate, null_rate);
  if (failures == 0) std::printf("map placement OK\n");
  return failures == 0 ? 0 : 1;
}
