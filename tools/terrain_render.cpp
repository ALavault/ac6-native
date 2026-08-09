// Draws the ported terrain heightfield in perspective, from a named viewpoint.
//
// Everything spatial here comes from `ac6::retail::TerrainField`, which carries
// the derivation of `0x82102568`. The camera, the colours and the light are
// mine and are not claimed to be retail's -- the same separation the flight
// captures keep, where `DemoCamera`'s fields are all spelled `invented_`.
//
// usage: terrain_render DIR OUT.ppm eye_x eye_y eye_z yaw_deg pitch_deg [range]
#include "ac6/demo_flight_view.h"
#include "ac6/retail_terrain_field.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

namespace {

std::vector<std::uint8_t> slurp(const char* p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
}

struct Rgb { std::uint8_t r, g, b; };

// A hypsometric ramp. Invented: the retail material for terrain is not read.
Rgb tint(float h) {
  static const struct { float at; Rgb c; } ramp[] = {
      {0.0F, {26, 54, 92}},    {1.0F, {38, 88, 126}},  {2.0F, {198, 188, 142}},
      {60.0F, {74, 118, 62}},  {160.0F, {120, 128, 66}}, {300.0F, {146, 122, 88}},
      {420.0F, {188, 184, 180}}, {520.0F, {250, 250, 250}}};
  for (int i = 0; i + 1 < 8; ++i) {
    if (h <= ramp[i + 1].at) {
      const float span = ramp[i + 1].at - ramp[i].at;
      const float t = span <= 0.0F ? 0.0F : (h - ramp[i].at) / span;
      return {static_cast<std::uint8_t>(ramp[i].c.r + (ramp[i + 1].c.r - ramp[i].c.r) * t),
              static_cast<std::uint8_t>(ramp[i].c.g + (ramp[i + 1].c.g - ramp[i].c.g) * t),
              static_cast<std::uint8_t>(ramp[i].c.b + (ramp[i + 1].c.b - ramp[i].c.b) * t)};
    }
  }
  return ramp[7].c;
}

}  // namespace

int main(int argc, char** argv) {
  using namespace ac6::retail;
  if (argc < 8) {
    std::fprintf(stderr,
                 "usage: %s DIR OUT.ppm eye_x eye_y eye_z yaw_deg pitch_deg [range]\n",
                 argv[0]);
    return 2;
  }
  const std::string dir = argv[1];
  const auto grid = slurp((dir + "/004_00_01_02_03.bin").c_str());
  const auto patches = slurp((dir + "/005_Bl_02_b8.bin").c_str());
  const auto field =
      TerrainField::open(grid.data(), grid.size(), patches.data(), patches.size());
  if (!field) { std::fprintf(stderr, "the map container did not open\n"); return 1; }

  const float ex = std::strtof(argv[3], nullptr);
  const float ey = std::strtof(argv[4], nullptr);
  const float ez = std::strtof(argv[5], nullptr);
  const float yaw = std::strtof(argv[6], nullptr) * 3.14159265F / 180.0F;
  const float pitch = std::strtof(argv[7], nullptr) * 3.14159265F / 180.0F;
  const float range = argc > 8 ? std::strtof(argv[8], nullptr) : 40000.0F;

  ac6::demo::Image image;
  image.width = 1600;
  image.height = 900;
  image.rgb.assign(static_cast<std::size_t>(image.width) * image.height * 3, 0);
  image.clear(24, 34, 56);
  image.clear_depth();

  const float cy = std::cos(yaw), sy = std::sin(yaw);
  const float cp = std::cos(pitch), sp = std::sin(pitch);
  const float focal = 0.5F * image.height / std::tan(0.5F * 0.95F);

  // Project a world point; returns false behind the eye.
  auto project = [&](float wx, float wy, float wz, float& sxo, float& syo,
                     float& depth) {
    const float dx = wx - ex, dy = wy - ey, dz = wz - ez;
    const float fx = dx * cy - dz * sy;       // right
    const float fz = dx * sy + dz * cy;       // forward
    const float fy = dy * cp - fz * sp;       // up
    const float fw = fz * cp + dy * sp;       // depth
    if (fw <= 1.0F) return false;
    sxo = image.width * 0.5F + focal * fx / fw;
    syo = image.height * 0.5F - focal * fy / fw;
    depth = fw;
    return true;
  };

  const float step = kTerrainSampleUnits;
  const long half = static_cast<long>(range / step);
  const long cx0 = static_cast<long>((ex + kTerrainWorldBias) / step);
  const long cz0 = static_cast<long>((ez + kTerrainWorldBias) / step);
  const long side = static_cast<long>(TerrainField::field_side()) - 1;

  std::size_t drawn = 0;
  for (long z = cz0 - half; z < cz0 + half; ++z) {
    if (z < 0 || z >= side) continue;
    for (long x = cx0 - half; x < cx0 + half; ++x) {
      if (x < 0 || x >= side) continue;
      const float h[4] = {
          field->sample(static_cast<std::size_t>(x), static_cast<std::size_t>(z)),
          field->sample(static_cast<std::size_t>(x + 1), static_cast<std::size_t>(z)),
          field->sample(static_cast<std::size_t>(x + 1), static_cast<std::size_t>(z + 1)),
          field->sample(static_cast<std::size_t>(x), static_cast<std::size_t>(z + 1))};
      bool present = true;
      for (const float v : h) present = present && sample_is_present(v);
      if (!present) continue;

      const float wx = x * step - kTerrainWorldBias;
      const float wz = z * step - kTerrainWorldBias;
      const float corner[4][3] = {{wx, h[0], wz},
                                  {wx + step, h[1], wz},
                                  {wx + step, h[2], wz + step},
                                  {wx, h[3], wz + step}};
      float sx[4], sv[4], dp[4];
      bool ok = true;
      for (int i = 0; i < 4; ++i) {
        ok = ok && project(corner[i][0], corner[i][1], corner[i][2], sx[i], sv[i], dp[i]);
      }
      if (!ok) continue;

      // A lambert term from the quad's own normal, light from the north-west.
      const float nx = (h[0] - h[1]) / step, nz = (h[0] - h[3]) / step;
      const float inv = 1.0F / std::sqrt(nx * nx + nz * nz + 1.0F);
      float shade = (0.55F * nx + 0.35F * nz + 0.80F) * inv + 0.22F;
      shade = shade < 0.30F ? 0.30F : (shade > 1.25F ? 1.25F : shade);
      const float mean = 0.25F * (h[0] + h[1] + h[2] + h[3]);
      const Rgb c = tint(mean);
      // Distance haze, so the far field does not read as noise.
      const float haze = std::fmin(1.0F, 0.25F * (dp[0] / range));
      auto mix = [&](std::uint8_t v, std::uint8_t sky) {
        const float lit = v * shade;
        return static_cast<std::uint8_t>(lit + (sky - lit) * haze);
      };
      const std::uint8_t r = mix(c.r, 24), g = mix(c.g, 34), b = mix(c.b, 56);

      image.triangle(static_cast<int>(sx[0]), static_cast<int>(sv[0]), dp[0],
                     static_cast<int>(sx[1]), static_cast<int>(sv[1]), dp[1],
                     static_cast<int>(sx[2]), static_cast<int>(sv[2]), dp[2], r, g, b);
      image.triangle(static_cast<int>(sx[0]), static_cast<int>(sv[0]), dp[0],
                     static_cast<int>(sx[2]), static_cast<int>(sv[2]), dp[2],
                     static_cast<int>(sx[3]), static_cast<int>(sv[3]), dp[3], r, g, b);
      ++drawn;
    }
  }
  std::printf("%zu quads drawn from (%.0f, %.0f, %.0f)\n", drawn, ex, ey, ez);
  return image.write_ppm(argv[2]) ? 0 : 1;
}
