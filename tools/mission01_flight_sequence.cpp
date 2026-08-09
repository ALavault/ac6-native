// A trajectory flown end to end from contracted parts, over the textured map.
//
// THE CHAIN, and every link is contracted:
//
//   step_flight_session          the stick bends the basis        (A3)
//   basis.rows[2]                a unit forward, exactly          (cycle 1470)
//   integrate_session_position   direction + speed -> position    (0x82303110)
//   TerrainField                 the ground under it              (0x82102568)
//   MapWaterGrid                 land or water                    (0x82101EE8)
//   MapPlacement + class filter  which building, where            (0x82102148, 1479)
//   the atlas                    seven pages, 15x15 tiles of 272  (0x820FAE08, 1487)
//   NTXR / NDXR decoders         pixels and triangles             (contracted)
//
// WHAT IS MINE: the speed, the stick programme, the camera, the sky gradient,
// the light and fog curves, the UV inset reading, and which of at64/at72 is
// north. The same list every capture carries, and it is not getting longer.
//
// usage: flight_sequence MAPDIR OUTDIR frames
#include "ac6/demo_flight_view.h"
#include "ac6/ntxr_texture.h"
#include "ac6/retail_flight_session.h"
#include "ac6/retail_flight_step.h"
#include "ac6/retail_map_placement.h"
#include "ac6/retail_map_water.h"
#include "ac6/retail_ndxr_container.h"
#include "ac6/retail_ndxr_geometry.h"
#include "ac6/retail_terrain_field.h"
#include "ac6/retail_transform.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> R(const std::string& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

struct Group { std::uint32_t texture = 0; std::vector<float> xyz, uv; };
struct Part { std::map<unsigned, std::vector<Group>> by_class; };

unsigned class_of(const std::string& name) {
  const auto p = name.find("_m01_");
  if (p == std::string::npos) return 4;
  const auto q = name.find('_', p + 5);
  const std::string tok = name.substr(p + 5, q - (p + 5));
  if (tok == "l" || tok == "airport") return 0;
  if (tok == "m") return 1;
  if (tok == "s") return 2;
  if (tok == "x") return 3;
  return 4;
}

struct Scene {
  std::string dir;
  std::map<std::uint32_t, std::vector<std::uint8_t>> wrappers;
  std::map<std::uint32_t, ac6::retail::DecodedTexture> textures;
  std::map<int, ac6::retail::DecodedTexture> atlas;
  std::map<std::uint16_t, Part> parts;
  std::vector<std::uint8_t> mta, mti;

  const ac6::retail::DecodedTexture* texture_for(std::uint32_t id) {
    auto it = textures.find(id);
    if (it != textures.end()) return it->second.pixels.empty() ? nullptr : &it->second;
    ac6::retail::DecodedTexture out{};
    const auto found = wrappers.find(id);
    if (found != wrappers.end()) {
      ac6::retail::NtxrRefusal why{};
      // No span trim: these wrappers carry mip chains (cycle 1475).
      if (const auto d = ac6::retail::decode_ntxr_base_level(
              found->second.data(), found->second.size(), true, &why))
        out = *d;
    }
    it = textures.emplace(id, std::move(out)).first;
    return it->second.pixels.empty() ? nullptr : &it->second;
  }

  const ac6::retail::DecodedTexture* page_for(int page) {
    auto it = atlas.find(page);
    if (it != atlas.end()) return it->second.pixels.empty() ? nullptr : &it->second;
    ac6::retail::DecodedTexture out{};
    char name[64];
    std::snprintf(name, sizeof(name), "/016_FHM/%03d_NTXR.ntxr", page);
    const auto bytes = R(dir + name);
    if (!bytes.empty()) {
      ac6::retail::NtxrRefusal why{};
      if (const auto d = ac6::retail::decode_ntxr_base_level(bytes.data(), bytes.size(),
                                                             true, &why))
        out = *d;
    }
    it = atlas.emplace(page, std::move(out)).first;
    return it->second.pixels.empty() ? nullptr : &it->second;
  }

  const Part& part_for(std::uint16_t id);
};

const Part& Scene::part_for(std::uint16_t id) {
  auto it = parts.find(id);
  if (it != parts.end()) return it->second;
  Part part;
  char name[64];
  std::snprintf(name, sizeof(name), "/014_FHM/%03u_NDXR.ndxr", id);
  const auto bytes = R(dir + name);
  if (!bytes.empty()) {
    if (const auto c = ac6::retail::NdxrContainer::Open(bytes.data(), bytes.size())) {
      for (std::uint16_t r = 0; r < c->record_count(); ++r) {
        const auto rec = c->Record(r);
        if (!rec) continue;
        std::vector<Group>& groups = part.by_class[class_of(std::string(rec->name))];
        for (std::uint16_t k = 0; k < rec->descriptor_count; ++k) {
          std::uint32_t tex = 0;
          for (unsigned slot = 0; slot < 4 && tex == 0; ++slot) {
            const auto mat = c->Material(*rec, k, slot);
            if (!mat || mat->texture_count == 0) continue;
            if (const auto ref = c->TextureRef(*mat, 0)) tex = ref->texture_id;
          }
          const auto d = c->Descriptor(*rec, k);
          if (!d) continue;
          const auto piece =
              ac6::retail::decode_ndxr_descriptor(*c, bytes.data(), bytes.size(), *d);
          if (!piece || piece->texcoords.size() < piece->positions.size()) continue;
          Group* group = nullptr;
          for (Group& g : groups) if (g.texture == tex) { group = &g; break; }
          if (group == nullptr) { groups.push_back(Group{tex, {}, {}}); group = &groups.back(); }
          for (std::size_t i = 2; i < piece->indices.size(); ++i) {
            const std::uint16_t a = piece->indices[i - 2], b = piece->indices[i - 1],
                                cc = piece->indices[i];
            if (a == ac6::retail::kStripRestart || b == ac6::retail::kStripRestart ||
                cc == ac6::retail::kStripRestart)
              continue;
            if (a >= piece->positions.size() || b >= piece->positions.size() ||
                cc >= piece->positions.size())
              continue;
            for (std::uint16_t idx : {a, b, cc}) {
              const auto& q = piece->positions[idx];
              group->xyz.push_back(q.x);
              group->xyz.push_back(q.y);
              group->xyz.push_back(q.z);
              const auto& uv = piece->texcoords[idx];
              group->uv.push_back(uv.u);
              group->uv.push_back(uv.v);
            }
          }
        }
      }
    }
  }
  return parts.emplace(id, std::move(part)).first->second;
}

struct Camera {
  const ac6::retail::RetailBasis* basis;
  ac6::retail::FlightPosition eye;
  int width, height;
  bool project(float wx, float wy, float wz, int& px, int& py, float& pd) const {
    const float dx = wx - eye.at64, dy = wy - eye.at68, dz = wz - eye.at72;
    const auto& r0 = basis->rows[0]; const auto& r1 = basis->rows[1];
    const auto& r2 = basis->rows[2];
    const float fx = dx * r0[0] + dy * r0[1] + dz * r0[2];
    const float fy = dx * r1[0] + dy * r1[1] + dz * r1[2];
    const float fw = dx * r2[0] + dy * r2[1] + dz * r2[2];
    if (fw <= 1.0F) return false;
    const float focal = 0.5F * height / std::tan(0.5F);
    px = int(width * 0.5F + focal * fx / fw);
    py = int(height * 0.5F - focal * fy / fw);
    pd = fw;
    return true;
  }
};

void draw_sky(ac6::demo::Image& image) {
  // The .sph's record-0 palette-A row means (cycle 1488): retail's colours,
  // my row orientation (row 0 horizon, row 2 zenith) and column averaging.
  static const float kZenith[3] = {86, 106, 126};
  static const float kMid[3] = {105, 128, 152};
  static const float kHorizon[3] = {126, 146, 169};
  for (int y = 0; y < image.height; ++y) {
    const float t = float(y) / float(image.height);
    float c[3];
    for (int i = 0; i < 3; ++i)
      c[i] = t < 0.5F ? kZenith[i] + (kMid[i] - kZenith[i]) * (t * 2.0F)
                      : kMid[i] + (kHorizon[i] - kMid[i]) * ((t - 0.5F) * 2.0F);
    const std::uint8_t r = std::uint8_t(c[0]);
    const std::uint8_t g = std::uint8_t(c[1]);
    const std::uint8_t b = std::uint8_t(c[2]);
    for (int x = 0; x < image.width; ++x) {
      const std::size_t o = (std::size_t(y) * image.width + x) * 3;
      image.rgb[o] = r; image.rgb[o + 1] = g; image.rgb[o + 2] = b;
    }
  }
}

void draw_ground(ac6::demo::Image& image, const Camera& cam, Scene& scene,
                 const ac6::retail::TerrainField& field,
                 const ac6::retail::MapWaterGrid& water) {
  using namespace ac6::retail;
  const float kTileUv = 0.06640625F, kInner = 0.9393382F;
  const float kInset = (1.0F - kInner) * 0.5F;
  const float step = kTerrainSampleUnits;
  const long side = long(TerrainField::field_side()) - 1;
  const long cx0 = long((cam.eye.at64 + kTerrainWorldBias) / step);
  const long cz0 = long((cam.eye.at72 + kTerrainWorldBias) / step);
  const long reach = 110;
  for (long z = cz0 - reach; z < cz0 + reach; ++z) {
    if (z < 0 || z >= side) continue;
    for (long x = cx0 - reach; x < cx0 + reach; ++x) {
      if (x < 0 || x >= side) continue;
      const float h[4] = {field.sample(x, z), field.sample(x + 1, z),
                          field.sample(x + 1, z + 1), field.sample(x, z + 1)};
      bool present = true;
      for (float v : h) present = present && sample_is_present(v);
      if (!present) continue;
      const float wx = float(x) * step - kTerrainWorldBias;
      const float wz = float(z) * step - kTerrainWorldBias;
      int sx[4], sy[4]; float dp[4]; bool ok = true;
      const float corner[4][3] = {{wx, h[0], wz}, {wx + step, h[1], wz},
                                  {wx + step, h[2], wz + step}, {wx, h[3], wz + step}};
      for (int i = 0; i < 4 && ok; ++i)
        ok = cam.project(corner[i][0], corner[i][1], corner[i][2], sx[i], sy[i], dp[i]);
      if (!ok) continue;
      const float nx = (h[0] - h[1]) / step, nz = (h[0] - h[3]) / step;
      const float inv = 1.0F / std::sqrt(nx * nx + nz * nz + 1.0F);
      float lit = (0.45F * nx + 0.30F * nz + 0.85F) * inv + 0.20F;
      lit = lit < 0.35F ? 0.35F : (lit > 1.15F ? 1.15F : lit);
      float fog = 1.0F - std::exp(-0.014F * (dp[0] / 24000.0F) * 100.0F);
      if (fog > 1.0F) fog = 1.0F;
      const float shade = lit * (1.0F - fog);
      if (water.is_water(wx + step * 0.5F, wz + step * 0.5F)) {
        const auto c = [&](int v) {
          return std::uint8_t(v * shade > 255.0F ? 255 : int(v * shade));
        };
        image.triangle(sx[0], sy[0], dp[0], sx[1], sy[1], dp[1], sx[2], sy[2], dp[2],
                       c(44), c(74), c(116));
        image.triangle(sx[0], sy[0], dp[0], sx[2], sy[2], dp[2], sx[3], sy[3], dp[3],
                       c(44), c(74), c(116));
        continue;
      }
      const long gx = x >> 2, gz = z >> 2;
      const int record = scene.mta[std::size_t((gz >> 4) * 16 + (gx >> 4))];
      const std::size_t mo = std::size_t(record) * 512 +
                             std::size_t(((gz & 15) * 16 + (gx & 15)) * 2);
      const int page = scene.mti[mo], tile = scene.mti[mo + 1];
      const auto* tex = scene.page_for(page);
      if (tex == nullptr) continue;
      const int tcol = tile % 15, trow = tile / 15;
      const float fx0 = float(x & 3) * 0.25F, fz0 = float(z & 3) * 0.25F;
      // Pixel-space, divided by the page's OWN dimensions: page 6 is
      // 4096 x 1024 (three tile rows, tiles 0..39 in .mti), and dividing its
      // rows by 4096 smeared them into the bottom edge -- the white shoreline
      // patches of cycle 1487, which the brightest real shoreline tile (mean
      // 125, pale farmland) could not have produced.
      auto uv = [&](float fx, float fz, float& u, float& v) {
        u = 272.0F * (float(tcol) + kInset + fx * kInner) / float(tex->width);
        v = 272.0F * (float(trow) + kInset + fz * kInner) / float(tex->height);
      };
      float u0, v0, u1, v1, u2, v2, u3, v3;
      uv(fx0, fz0, u0, v0);
      uv(fx0 + 0.25F, fz0, u1, v1);
      uv(fx0 + 0.25F, fz0 + 0.25F, u2, v2);
      uv(fx0, fz0 + 0.25F, u3, v3);
      image.triangle_textured(sx[0], sy[0], dp[0], u0, v0, sx[1], sy[1], dp[1], u1, v1,
                              sx[2], sy[2], dp[2], u2, v2,
                              tex->pixels.data(), int(tex->width), int(tex->height), shade);
      image.triangle_textured(sx[0], sy[0], dp[0], u0, v0, sx[2], sy[2], dp[2], u2, v2,
                              sx[3], sy[3], dp[3], u3, v3,
                              tex->pixels.data(), int(tex->width), int(tex->height), shade);
    }
  }
}

void draw_parts(ac6::demo::Image& image, const Camera& cam, Scene& scene,
                const ac6::retail::MapPlacement& placement) {
  using namespace ac6::retail;
  // .mapparts.distanceL/M/S = 16000/12000/10000, the mapset's own values.
  const float kDistance[4] = {16000.0F, 12000.0F, 10000.0F, 16000.0F};
  for (const MapInstance& q : placement.instances()) {
    if (!q.accepted) continue;
    const float dx = q.world_x - cam.eye.at64, dz = q.world_z - cam.eye.at72;
    const unsigned klass = (unsigned(q.tag_high) >> 14) & 3u;
    const float cut = kDistance[klass];
    if (dx * dx + dz * dz > cut * cut) continue;
    const Part& part = scene.part_for(q.selector);
    const auto found = part.by_class.find(klass);
    if (found == part.by_class.end()) continue;
    for (const Group& gr : found->second) {
      const auto* tex = scene.texture_for(gr.texture);
      for (std::size_t t = 0; t + 8 < gr.xyz.size(); t += 9) {
        const float* v = gr.xyz.data() + t;
        const float* w = gr.uv.data() + (t / 9) * 6;
        int px[3], py[3]; float pd[3]; bool ok = true;
        for (int i = 0; i < 3 && ok; ++i)
          ok = cam.project(q.world_x + v[i * 3], q.world_y + v[i * 3 + 1],
                           q.world_z + v[i * 3 + 2], px[i], py[i], pd[i]);
        if (!ok) continue;
        const float ax = v[3] - v[0], ay = v[4] - v[1], az = v[5] - v[2];
        const float bx = v[6] - v[0], by = v[7] - v[1], bz = v[8] - v[2];
        float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
        const float len = std::sqrt(nx * nx + ny * ny + nz * nz) + 1e-6F;
        float lit = 0.45F + 0.55F * std::fabs(ny / len);
        float fog = 1.0F - std::exp(-0.014F * (pd[0] / 24000.0F) * 100.0F);
        if (fog > 1.0F) fog = 1.0F;
        const float shade = lit * (1.0F - fog);
        if (tex != nullptr) {
          image.triangle_textured(px[0], py[0], pd[0], w[0], w[1],
                                  px[1], py[1], pd[1], w[2], w[3],
                                  px[2], py[2], pd[2], w[4], w[5],
                                  tex->pixels.data(), int(tex->width), int(tex->height),
                                  shade);
        } else {
          const auto c = [&](float base) {
            const float value = base * shade;
            return std::uint8_t(value > 255.0F ? 255.0F : value);
          };
          image.triangle(px[0], py[0], pd[0], px[1], py[1], pd[1], px[2], py[2], pd[2],
                         c(200.0F), c(192.0F), c(180.0F));
        }
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  using namespace ac6::retail;
  if (argc < 4) { std::fprintf(stderr, "usage: MAPDIR OUTDIR FRAMES\n"); return 2; }
  Scene scene;
  scene.dir = argv[1];
  const std::string out = argv[2];
  const int frames = std::atoi(argv[3]);

  const auto g = R(scene.dir + "/004_00_01_02_03.bin"), p = R(scene.dir + "/005_Bl_02_b8.bin");
  const auto a = R(scene.dir + "/001_MCA_00.bin"), i2 = R(scene.dir + "/003_MCI_00.bin");
  const auto m = R(scene.dir + "/002_MCD_00.bin"), pdl = R(scene.dir + "/011_00_00_00_00.bin");
  scene.mta = R(scene.dir + "/009_00_01_02_03.bin");
  scene.mti = R(scene.dir + "/010_00_00_00_01.bin");
  const auto field = TerrainField::open(g.data(), g.size(), p.data(), p.size());
  const auto water = MapWaterGrid::open(a.data(), a.size(), i2.data(), i2.size(),
                                        m.data(), m.size());
  const auto placement = MapPlacement::open(pdl.data(), pdl.size());
  if (!field || !water || !placement) { std::fprintf(stderr, "the map refused\n"); return 1; }
  for (const std::string sub : {"/015_FHM", "/016_FHM"}) {
    for (const auto& e : std::filesystem::directory_iterator(scene.dir + sub)) {
      if (e.path().extension() != ".ntxr") continue;
      auto blob = R(e.path().string());
      for (std::size_t q = 0; q + 12 <= blob.size(); ++q) {
        if (std::memcmp(blob.data() + q, "GIDX", 4) != 0) continue;
        const std::uint32_t id = (std::uint32_t(blob[q + 8]) << 24) |
                                 (std::uint32_t(blob[q + 9]) << 16) |
                                 (std::uint32_t(blob[q + 10]) << 8) | blob[q + 11];
        scene.wrappers[id] = std::move(blob);
        break;
      }
    }
  }

  FlightModelConfig config{};
  config.limits = FlightRotationLimits{5.0F, 1.399999976158142F, 5.400000095367432F};
  config.rates304 = LiveAxisRates{4.0F, 2.0F};
  config.rates308 = LiveAxisRates{3.0F, 1.5F};
  config.rates312 = LiveAxisRates{5.0F, 2.5F};
  config.servo304 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
  config.servo308 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
  config.servo312 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
  config.rampRate952 = 3.0F;
  config.rampRate956 = 3.0F;
  config.rampThreshold404 = 0.5F;

  FlightSessionState state{};
  state.position.at64 = 1000.0F;
  state.position.at68 = 420.0F;
  state.position.at72 = -24000.0F;

  ac6::demo::Image image;
  image.width = 960;
  image.height = 540;
  image.rgb.assign(std::size_t(image.width) * image.height * 3, 0);

  const float kStep = 1.0F / 60.0F;
  int written = 0;
  for (int f = 0; f < frames; ++f) {
    FlightStick stick{};
    stick.target13 = 0.5F;
    stick.increment13 = 1.0F;
    step_flight_session(state, config, stick, kStep);
    const auto& forward = state.basis.rows[2];
    FlightRates heading{};
    heading.to64 = forward[0];
    heading.to68 = forward[1];
    heading.to72 = forward[2];
    integrate_session_position(state, heading, 1500.0F, 0.0F, kStep);

    if (f % 2) continue;                           // 60 Hz sim, 30 fps film
    Camera cam{&state.basis, state.position, image.width, image.height};
    draw_sky(image);
    image.clear_depth();
    draw_ground(image, cam, scene, *field, water.value());
    draw_parts(image, cam, scene, *placement);
    ac6::demo::apply_mapset_post(image, ac6::demo::MapsetPost{});
    char name[512];
    std::snprintf(name, sizeof(name), "%s/frame-%04d.ppm", out.c_str(), written++);
    if (!image.write_ppm(name)) { std::fprintf(stderr, "write failed\n"); return 1; }
  }
  std::printf("%d ticks, %d frames; final position (%.0f, %.0f, %.0f)\n", frames,
              written, state.position.at64, state.position.at68, state.position.at72);
  return 0;
}
