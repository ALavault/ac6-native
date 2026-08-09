// The city standing on the terrain: the ported placement list, the ported
// heightfield, and the ported NDXR decoder, drawn together.
//
// Every position comes from retail data through a contracted decoder --
// `MapPlacement` for where a part goes, `TerrainField` for the ground,
// `decode_ndxr_descriptor` for the geometry. The camera, the palette and the
// light are mine and are not claimed to be retail's.
//
// ROTATION. `tag >> 16` is 4-fold periodic in u16 space at R(4*theta) = 0.9757,
// against ~0.69 at every other harmonic and 0 of 2000 random fields under two
// null models (cycle 1451) -- a right-angle street grid at 79.33 degrees. Pass
// a trailing `+1` or `-1` to apply it with that sign, or `0` to draw
// axis-aligned as cycle 1449 did. The SIGN is not derived; it is chosen by
// looking at the two renders, and the report says so.
//
// usage: city_render MAPDIR OUT.ppm eye_x eye_y eye_z yaw pitch [range]
#include "ac6/demo_flight_view.h"
#include "ac6/retail_map_placement.h"
#include "ac6/retail_map_water.h"
#include "ac6/retail_ndxr_container.h"
#include "ac6/retail_ndxr_geometry.h"
#include "ac6/retail_terrain_field.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {
std::vector<std::uint8_t> Read(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
}
struct Tri { float x[3], y[3], z[3]; };
}  // namespace

int main(int argc, char** argv) {
  using namespace ac6::retail;
  if (argc < 8) { std::fprintf(stderr, "usage: see header\n"); return 2; }
  const std::string dir = argv[1];
  const float ex = std::strtof(argv[3], nullptr), ey = std::strtof(argv[4], nullptr);
  const float ez = std::strtof(argv[5], nullptr);
  const float yaw = std::strtof(argv[6], nullptr) * 3.14159265F / 180.0F;
  const float pitch = std::strtof(argv[7], nullptr) * 3.14159265F / 180.0F;
  const float range = argc > 8 ? std::strtof(argv[8], nullptr) : 12000.0F;
  const float spin = argc > 9 ? std::strtof(argv[9], nullptr) : 0.0F;
  // WHICH FIELD NAMES THE MODEL is not established (cycle 1452). 0 selects with
  // `tag & 0xFFFF`, which is what cycles 1449-1451 drew; 1 selects with the
  // nine-bit field retail hands to the resource table at vtable slot +0x5C.
  const int selector_field = argc > 10 ? std::atoi(argv[10]) : 0;

  const auto grid = Read(dir + "/004_00_01_02_03.bin");
  const auto patches = Read(dir + "/005_Bl_02_b8.bin");
  const auto pdl = Read(dir + "/011_00_00_00_00.bin");
  // The water bit, through the ported decoder. The first version of this file
  // shaded the ground by elevation and painted the whole city blue: cycle 1445
  // had already measured that the city's ground IS flat at zero and that only
  // the bit separates it from the bay. Elevation is not the authority on water.
  const auto mca = Read(dir + "/001_MCA_00.bin");
  const auto mci = Read(dir + "/003_MCI_00.bin");
  const auto mcd = Read(dir + "/002_MCD_00.bin");
  const auto water = MapWaterGrid::open(mca.data(), mca.size(), mci.data(),
                                        mci.size(), mcd.data(), mcd.size());
  if (!water) { std::fprintf(stderr, "the water grid refused\n"); return 1; }
  const auto field = TerrainField::open(grid.data(), grid.size(),
                                        patches.data(), patches.size());
  const auto placement = MapPlacement::open(pdl.data(), pdl.size());
  if (!field || !placement) { std::fprintf(stderr, "refused\n"); return 1; }

  ac6::demo::Image image;
  image.width = 1600; image.height = 900;
  image.rgb.assign(static_cast<std::size_t>(image.width) * image.height * 3, 0);
  image.clear(150, 172, 198);
  image.clear_depth();

  const float cy = std::cos(yaw), sy = std::sin(yaw);
  const float cp = std::cos(pitch), sp = std::sin(pitch);
  const float focal = 0.5F * image.height / std::tan(0.5F * 0.95F);
  auto project = [&](float wx, float wy, float wz, float& sx, float& sv, float& dp) {
    const float dx = wx - ex, dy = wy - ey, dz = wz - ez;
    const float fx = dx * cy - dz * sy, fz = dx * sy + dz * cy;
    const float fy = dy * cp - fz * sp, fw = fz * cp + dy * sp;
    if (fw <= 1.0F) return false;
    sx = image.width * 0.5F + focal * fx / fw;
    sv = image.height * 0.5F - focal * fy / fw;
    dp = fw;
    return true;
  };
  auto emit = [&](const Tri& t, int r, int g, int b) {
    float sx[3], sv[3], dp[3];
    for (int i = 0; i < 3; ++i)
      if (!project(t.x[i], t.y[i], t.z[i], sx[i], sv[i], dp[i])) return;
    const float haze = std::fmin(0.85F, dp[0] / (range * 2.2F));
    auto mix = [&](int v, int s) { return static_cast<std::uint8_t>(v + (s - v) * haze); };
    image.triangle(int(sx[0]), int(sv[0]), dp[0], int(sx[1]), int(sv[1]), dp[1],
                   int(sx[2]), int(sv[2]), dp[2], mix(r, 150), mix(g, 172), mix(b, 198));
  };

  // The ground.
  const float step = kTerrainSampleUnits;
  const long half = long(range / step);
  const long cx0 = long((ex + kTerrainWorldBias) / step);
  const long cz0 = long((ez + kTerrainWorldBias) / step);
  const long side = long(TerrainField::field_side()) - 1;
  for (long z = cz0 - half; z < cz0 + half; ++z) {
    if (z < 0 || z >= side) continue;
    for (long x = cx0 - half; x < cx0 + half; ++x) {
      if (x < 0 || x >= side) continue;
      const float h[4] = {field->sample(x, z), field->sample(x + 1, z),
                          field->sample(x + 1, z + 1), field->sample(x, z + 1)};
      bool ok = true;
      for (float v : h) ok = ok && sample_is_present(v);
      if (!ok) continue;
      const float wx = x * step - kTerrainWorldBias, wz = z * step - kTerrainWorldBias;
      const bool sea = water->is_water(wx + step * 0.5F, wz + step * 0.5F);
      const int r = sea ? 44 : 96, g = sea ? 74 : 108, b = sea ? 116 : 74;
      emit({{wx, wx + step, wx + step}, {h[0], h[1], h[2]}, {wz, wz, wz + step}}, r, g, b);
      emit({{wx, wx + step, wx}, {h[0], h[2], h[3]}, {wz, wz + step, wz + step}}, r, g, b);
    }
  }

  // The parts, loaded by integer id -- retail's own `parts/%d`.
  std::map<std::uint16_t, std::vector<Tri>> cache;
  std::size_t drawn = 0, missing = 0;
  std::size_t skipped = 0;
  for (const MapInstance& q : placement->instances()) {
    // 0x82102350 skips every record whose kind is not 0 or 7.
    if (!q.accepted) { ++skipped; continue; }
    const float dx = q.world_x - ex, dz = q.world_z - ez;
    if (dx * dx + dz * dz > range * range) continue;
    const std::uint16_t model = selector_field ? q.selector : q.part_id;
    auto it = cache.find(model);
    if (it == cache.end()) {
      char name[64];
      std::snprintf(name, sizeof(name), "/014_FHM/%03u_NDXR.ndxr", model);
      const auto bytes = Read(dir + name);
      std::vector<Tri> tris;
      if (!bytes.empty()) {
        if (const auto c = NdxrContainer::Open(bytes.data(), bytes.size())) {
          for (std::uint16_t r = 0; r < c->record_count(); ++r) {
            const auto rec = c->Record(r);
            if (!rec) continue;
            for (std::uint16_t k = 0; k < rec->descriptor_count; ++k) {
              const auto d = c->Descriptor(*rec, k);
              if (!d) continue;
              const auto piece =
                  decode_ndxr_descriptor(*c, bytes.data(), bytes.size(), *d);
              if (!piece) continue;
              for (std::size_t i = 2; i < piece->indices.size(); ++i) {
                const std::uint16_t a = piece->indices[i - 2], bb = piece->indices[i - 1],
                                    cc = piece->indices[i];
                if (a == kStripRestart || bb == kStripRestart || cc == kStripRestart)
                  continue;
                if (a >= piece->positions.size() || bb >= piece->positions.size() ||
                    cc >= piece->positions.size())
                  continue;
                const auto& p0 = piece->positions[a];
                const auto& p1 = piece->positions[bb];
                const auto& p2 = piece->positions[cc];
                tris.push_back({{p0.x, p1.x, p2.x}, {p0.y, p1.y, p2.y},
                                {p0.z, p1.z, p2.z}});
              }
            }
          }
        }
      }
      it = cache.emplace(model, std::move(tris)).first;
    }
    if (it->second.empty()) { ++missing; continue; }
    const float theta = spin * 2.0F * 3.14159265F * float(q.tag_high) / 65536.0F;
    const float ct = std::cos(theta), st = std::sin(theta);
    for (const Tri& t : it->second) {
      const float ax = t.x[1] - t.x[0], ay = t.y[1] - t.y[0], az = t.z[1] - t.z[0];
      const float bx = t.x[2] - t.x[0], by = t.y[2] - t.y[0], bz = t.z[2] - t.z[0];
      float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
      const float len = std::sqrt(nx * nx + ny * ny + nz * nz) + 1e-6F;
      const float lit = 0.45F + 0.55F * std::fabs(ny / len);
      const int v = int(200 * lit);
      Tri w;
      for (int i = 0; i < 3; ++i) {
        w.x[i] = q.world_x + t.x[i] * ct - t.z[i] * st;
        w.y[i] = q.world_y + t.y[i];
        w.z[i] = q.world_z + t.x[i] * st + t.z[i] * ct;
      }
      emit(w, v, int(v * 0.95F), int(v * 0.88F));
      ++drawn;
    }
  }
  std::printf("%zu triangles from %zu cached parts (field %s), %zu with no file, "
              "%zu records skipped by retail's kind test\n",
              drawn, cache.size(), selector_field ? "nine-bit" : "tag&0xFFFF",
              missing, skipped);
  return image.write_ppm(argv[2]) ? 0 : 1;
}
