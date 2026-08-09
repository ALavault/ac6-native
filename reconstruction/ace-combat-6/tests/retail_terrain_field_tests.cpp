// The heightfield decoder, over Mission 01's real map container.
//
// DATA DRIVEN, exiting 77 when the container is absent: the extracted corpus is
// retail content and is never committed, so a clean clone skips rather than
// fails. Same arrangement as ac6-retail-ndxr-geometry.
#include "ac6/retail_terrain_field.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
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
}  // namespace

int main(int argc, char** argv) {
  using namespace ac6::retail;

  // The sentinel behaviour is pure and is checked without any data at all, so a
  // clean clone still exercises it.
  check(sample_is_present(0.0F), "zero is a present sample");
  check(sample_is_present(487.0F), "a hill is a present sample");
  check(!sample_is_present(kAbsentSample), "9990 is absent, per 0x8210272C");
  check(!sample_is_present(std::nanf("")), "a NaN is absent -- 007 is all 0xFF");
  check(!sample_is_present(1.0e9F), "above the sentinel is absent");

  if (argc < 2) { std::fprintf(stderr, "usage: tests MAP_FHM_DIR\n"); return 77; }
  const std::filesystem::path dir = argv[1];
  const auto grid_path = dir / "004_00_01_02_03.bin";
  const auto patch_path = dir / "005_Bl_02_b8.bin";
  if (!std::filesystem::exists(grid_path) || !std::filesystem::exists(patch_path)) {
    std::fprintf(stderr, "no map container at %s — skipping\n", dir.string().c_str());
    return failures == 0 ? 77 : 1;
  }

  const std::vector<std::uint8_t> grid = slurp(grid_path);
  const std::vector<std::uint8_t> patches = slurp(patch_path);
  check(grid.size() == 256, "the patch grid is 16 x 16 bytes");
  check(patches.size() % kTerrainPatchBytes == 4,
        "the patch array is a whole number of 0x4204 records, plus four bytes");

  const auto field =
      TerrainField::open(grid.data(), grid.size(), patches.data(), patches.size());
  check(field.has_value(), "the container opens");
  if (!field) return 1;

  check(field->patch_count() == 74, "Mission 01 has 74 patches");
  check(TerrainField::field_side() == 1025, "the lattice is 1025 samples across");

  // THE CONTROL. If the 65th row and column are the shared edge, a patch's
  // column 64 equals its right neighbour's column 0. This is the whole basis of
  // the 65 x 65 reading, so the test asserts it rather than citing cycle 1445.
  const SharedEdgeReport edges = field->check_shared_edges();
  check(edges.mismatched == 0, "no shared edge disagrees");
  check(edges.matched == 31200, "31200 shared-edge samples compared");
  check(edges.worst == 0.0F, "the shared edges are bit-identical");

  // A CONTROL ON THE CONTROL: agreement must not be the default. Compare the
  // same edges against patches that are NOT the neighbour and expect failure.
  std::size_t stranger_match = 0, stranger_total = 0;
  for (std::size_t cz = 0; cz < 16; ++cz) {
    for (std::size_t cx = 0; cx + 1 < 16; ++cx) {
      const std::uint8_t a = field->patch_id(cx, cz);
      const std::uint8_t wrong = static_cast<std::uint8_t>(
          (field->patch_id(cx + 1, cz) + 37u) % field->patch_count());
      for (std::size_t r = 0; r < 65; ++r) {
        const float u = field->patch_sample(a, r, 64);
        const float v = field->patch_sample(wrong, r, 0);
        if (!(sample_is_present(u) && sample_is_present(v))) continue;
        ++stranger_total;
        if (std::fabs(u - v) <= 1e-3F) ++stranger_match;
      }
    }
  }
  check(stranger_total > 10000, "the control compared a real population");
  check(stranger_match * 2 < stranger_total,
        "unrelated patches mostly DISAGREE, so agreement is not the default");

  // The span the query's +65536 bias implies, and it was not fitted.
  float lowest = 1.0e30F, highest = -1.0e30F;
  std::size_t present = 0;
  for (std::size_t z = 0; z < TerrainField::field_side(); ++z) {
    for (std::size_t x = 0; x < TerrainField::field_side(); ++x) {
      const float h = field->sample(x, z);
      if (!sample_is_present(h)) continue;
      ++present;
      if (h < lowest) lowest = h;
      if (h > highest) highest = h;
    }
  }
  check(present == 1025u * 1025u, "every lattice sample is present");
  check(lowest == 0.0F, "the sea is exactly zero");
  check(highest > 480.0F && highest < 490.0F, "the highest ground is ~487");
  check(std::fabs((TerrainField::field_side() - 1) * kTerrainSampleUnits -
                  2.0F * kTerrainWorldBias) < 1e-3F,
        "the lattice spans exactly the +/-65536 the query's bias implies");

  // The world transform, at the corner and at the origin.
  float h = 0.0F;
  check(field->height_at(-kTerrainWorldBias, -kTerrainWorldBias, &h),
        "the south-west corner resolves");
  check(h == field->sample(0, 0), "and it is sample (0,0)");
  check(field->height_at(0.0F, 0.0F, &h), "the world origin resolves");
  check(h == field->sample(512, 512), "and it is the lattice centre");
  check(!field->height_at(-70000.0F, 0.0F, &h),
        "outside the bounds check, the query is abandoned");
  check(!field->height_at(0.0F, 70000.0F, &h),
        "outside on the other axis too");

  // The segment early-out. A segment far above the highest ground provably
  // misses; one at zero over land does not.
  const float high_a[3] = {0.0F, 5000.0F, 0.0F};
  const float high_b[3] = {1000.0F, 5000.0F, 0.0F};
  check(!field->segment_may_reach_terrain(high_a, high_b),
        "5000 units up is above everything, so the segment misses");

  std::size_t reached = 0, tested = 0;
  for (long wz = -60000; wz < 60000; wz += 4096) {
    for (long wx = -60000; wx < 60000; wx += 4096) {
      float ground = 0.0F;
      if (!field->height_at(static_cast<float>(wx), static_cast<float>(wz), &ground)) {
        continue;
      }
      ++tested;
      const float low_a[3] = {static_cast<float>(wx), ground - 1.0F,
                              static_cast<float>(wz)};
      const float low_b[3] = {static_cast<float>(wx) + 64.0F, ground - 1.0F,
                              static_cast<float>(wz)};
      if (field->segment_may_reach_terrain(low_a, low_b)) ++reached;
    }
  }
  check(tested > 500, "the sweep covered the map");
  check(reached == tested,
        "a segment below the local ground never misses -- the max is >= it");

  std::printf("patches %zu  edges %zu/%zu  height %.2f..%.2f  sweep %zu/%zu\n",
              field->patch_count(), edges.matched,
              edges.matched + edges.mismatched, lowest, highest, reached, tested);
  if (failures == 0) std::printf("terrain field OK\n");
  return failures == 0 ? 0 : 1;
}
