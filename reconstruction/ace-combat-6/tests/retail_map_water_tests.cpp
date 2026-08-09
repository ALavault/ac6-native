// The per-position bit, over Mission 01's real map container, cross-checked
// against the independently ported heightfield.
#include "ac6/retail_map_water.h"
#include "ac6/retail_terrain_field.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
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
std::uint32_t next(std::uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }
}  // namespace

int main(int argc, char** argv) {
  using namespace ac6::retail;

  const std::uint8_t junk[64] = {};
  check(MapWaterGrid::open(nullptr, 64, junk, 64, junk, 64) == std::nullopt,
        "a null blob is refused");
  check(MapWaterGrid::open(junk, 64, junk, 64, junk, 64) == std::nullopt,
        "a blob without the magic is refused, per 0x82101FC8");

  if (argc < 2) { std::fprintf(stderr, "usage: tests MAP_FHM_DIR\n"); return 77; }
  const std::filesystem::path dir = argv[1];
  const auto names = {"001_MCA_00.bin", "003_MCI_00.bin", "002_MCD_00.bin",
                      "004_00_01_02_03.bin", "005_Bl_02_b8.bin"};
  for (const char* n : names) {
    if (!std::filesystem::exists(dir / n)) {
      std::fprintf(stderr, "no map container at %s — skipping\n", dir.string().c_str());
      return failures == 0 ? 77 : 1;
    }
  }
  const auto mca = slurp(dir / "001_MCA_00.bin");
  const auto mci = slurp(dir / "003_MCI_00.bin");
  const auto mcd = slurp(dir / "002_MCD_00.bin");
  const auto water = MapWaterGrid::open(mca.data(), mca.size(), mci.data(),
                                        mci.size(), mcd.data(), mcd.size());
  check(water.has_value(), "the triple opens");
  if (!water) return 1;
  check(water->group_count() == 4864, "MCI+8 is its entry count");
  check(water->block_count() == 413, "MCD+8 is its block count");

  bool bit = false;
  check(!water->query(-70000.0F, 0.0F, &bit), "outside the coarse grid is refused");
  check(!water->query(0.0F, 70000.0F, &bit), "on the other axis too");
  check(water->query(0.0F, 0.0F, &bit), "the world origin resolves");

  const auto grid = slurp(dir / "004_00_01_02_03.bin");
  const auto patches = slurp(dir / "005_Bl_02_b8.bin");
  const auto field =
      TerrainField::open(grid.data(), grid.size(), patches.data(), patches.size());
  check(field.has_value(), "the heightfield opens");
  if (!field) return 1;

  // THE MEASUREMENT the name `is_water` rests on, re-run rather than cited.
  std::size_t seen = 0, agree = 0, residual_flat = 0, residual = 0, bit_on_high = 0;
  for (std::size_t sz = 0; sz < 1024; sz += 2) {
    for (std::size_t sx = 0; sx < 1024; sx += 2) {
      const float wx = static_cast<float>(sx) * 128.0F - 65536.0F;
      const float wz = static_cast<float>(sz) * 128.0F - 65536.0F;
      float h = 0.0F;
      if (!water->query(wx, wz, &bit)) continue;
      if (!field->height_at(wx, wz, &h)) continue;
      ++seen;
      const bool land = h > 0.5F;
      if (bit == !land) { ++agree; continue; }
      ++residual;
      if (!land) ++residual_flat;      // flat ground, bit clear
      else ++bit_on_high;              // the rivers
    }
  }
  const double rate = 100.0 * agree / seen;
  check(seen > 250000, "the sweep covered the map");
  check(rate > 97.0 && rate < 98.0, "the bit is water 97.4% of the time");
  check(residual_flat * 10 > residual * 9,
        "and 90%+ of the residual is flat ground with the bit clear");
  check(bit_on_high * 100 < seen, "the bit over high ground is under 1%");

  // A CONTROL: the sea half of the map must be almost entirely set, and the
  // inland half almost entirely clear. A decoder that shifted its index would
  // blur both.
  std::size_t sea_set = 0, sea_n = 0, inland_clear = 0, inland_n = 0;
  for (std::size_t sz = 0; sz < 1024; sz += 4) {
    for (std::size_t sx = 0; sx < 1024; sx += 4) {
      const float wx = static_cast<float>(sx) * 128.0F - 65536.0F;
      const float wz = static_cast<float>(sz) * 128.0F - 65536.0F;
      if (!water->query(wx, wz, &bit)) continue;
      if (sz > 700) { ++sea_n; if (bit) ++sea_set; }
      if (sz < 300) { ++inland_n; if (!bit) ++inland_clear; }
    }
  }
  check(sea_n > 1000 && inland_n > 1000, "both halves are real populations");
  check(sea_set * 100 > sea_n * 99, "the open sea is set almost everywhere");
  check(inland_clear * 100 > inland_n * 95, "the inland is clear almost everywhere");

  std::printf("groups %zu blocks %zu  water %.2f%% of %zu  residual %zu "
              "(flat %zu, high %zu)  sea %zu/%zu inland %zu/%zu\n",
              water->group_count(), water->block_count(), rate, seen, residual,
              residual_flat, bit_on_high, sea_set, sea_n, inland_clear, inland_n);
  if (failures == 0) std::printf("map water OK\n");
  return failures == 0 ? 0 : 1;
}
