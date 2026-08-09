// The flight view over the map's real ground.
//
// DATA DRIVEN, exiting 77 when the map container is absent.
#include "ac6/demo_flight_view.h"
#include "ac6/retail_map_water.h"
#include "ac6/retail_terrain_field.h"
#include "ac6/retail_transform.h"

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
struct Tally { std::size_t sky = 0, sea = 0, land = 0; };
Tally classify(const ac6::demo::Image& image) {
  Tally t;
  for (std::size_t i = 0; i + 2 < image.rgb.size(); i += 3) {
    const int r = image.rgb[i], g = image.rgb[i + 1], b = image.rgb[i + 2];
    if (b > r && b > g && b > 150) ++t.sky;
    else if (b > r && b >= g) ++t.sea;
    else if (g >= r && g > b) ++t.land;
  }
  return t;
}
}  // namespace

int main(int argc, char** argv) {
  using namespace ac6::retail;
  if (argc < 2) { std::fprintf(stderr, "usage: tests MAP_FHM_DIR\n"); return 77; }
  const std::filesystem::path dir = argv[1];
  for (const char* n : {"004_00_01_02_03.bin", "005_Bl_02_b8.bin",
                        "001_MCA_00.bin", "003_MCI_00.bin", "002_MCD_00.bin"}) {
    if (!std::filesystem::exists(dir / n)) {
      std::fprintf(stderr, "no map container at %s — skipping\n", dir.string().c_str());
      return 77;
    }
  }
  const auto grid = slurp(dir / "004_00_01_02_03.bin");
  const auto patches = slurp(dir / "005_Bl_02_b8.bin");
  const auto mca = slurp(dir / "001_MCA_00.bin");
  const auto mci = slurp(dir / "003_MCI_00.bin");
  const auto mcd = slurp(dir / "002_MCD_00.bin");
  const auto field =
      TerrainField::open(grid.data(), grid.size(), patches.data(), patches.size());
  const auto water = MapWaterGrid::open(mca.data(), mca.size(), mci.data(),
                                        mci.size(), mcd.data(), mcd.size());
  check(field.has_value() && water.has_value(), "the map opens");
  if (!field || !water) return 1;

  ac6::demo::Image image;
  image.width = 640;
  image.height = 360;
  image.rgb.assign(static_cast<std::size_t>(image.width) * image.height * 3, 0);
  ac6::demo::DemoCamera camera{};

  // Over the city, looking along the aircraft's own basis. The attitude is
  // retail's identity, whose row 2 is (0,0,1), so the view faces +z -- and the
  // first version of this test sat at z = +2500 facing the open sea and found
  // no land at all. The position is a world coordinate, not an invention: the
  // map spans 131072 units and the city sits at z = -20442..6784.
  FlightPosition position{};
  position.at64 = -1500.0F;
  position.at68 = 800.0F;
  position.at72 = -9000.0F;

  ac6::demo::draw_terrain_view(image, identity_basis(), camera, position, *field,
                               &water.value());
  const Tally with = classify(image);
  check(with.sky > 1000, "there is sky");
  check(with.sea > 1000, "there is water");
  check(with.land > 1000, "and there is land");
  check(image.depth.size() == image.rgb.size() / 3, "the depth buffer is sized");

  // THE CONTROL: the water grid must change the picture. If the elevation proxy
  // agreed with the bit, the parameter would be decoration -- and cycle 1445
  // measured the city's ground at exactly zero, so it does not agree.
  std::vector<std::uint8_t> before = image.rgb;
  ac6::demo::draw_terrain_view(image, identity_basis(), camera, position, *field,
                               nullptr);
  std::size_t differing = 0;
  for (std::size_t i = 0; i < before.size(); ++i) {
    if (before[i] != image.rgb[i]) ++differing;
  }
  check(differing > 1000,
        "shading by the water bit differs from shading by elevation");

  // And a position outside the lattice must draw sky and nothing else.
  FlightPosition outside{};
  outside.at64 = 400000.0F;
  outside.at68 = 900.0F;
  outside.at72 = 400000.0F;
  ac6::demo::draw_terrain_view(image, identity_basis(), camera, outside, *field,
                               &water.value());
  const Tally none = classify(image);
  check(none.sea == 0 && none.land == 0, "off the map, nothing is drawn");

  std::printf("sky %zu  sea %zu  land %zu  water-shading differs in %zu bytes\n",
              with.sky, with.sea, with.land, differing);
  if (failures == 0) std::printf("terrain view OK\n");
  return failures == 0 ? 0 : 1;
}
