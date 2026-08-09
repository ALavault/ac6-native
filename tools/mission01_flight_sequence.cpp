// A trajectory flown end to end from contracted parts, over the real map.
//
// THE CHAIN, and every link is contracted:
//
//   step_flight_session          the stick bends the basis        (A3)
//   basis.rows[2]                a unit forward, exactly          (cycle 1470)
//   integrate_session_position   direction + speed -> position    (0x82303110)
//   TerrainField                 the ground under it              (0x82102568)
//   MapWaterGrid                 land or water                    (0x82101EE8)
//
// WHAT IS MINE, and it is the same short list every capture in this campaign
// carries: the speed, the stick programme, the camera's field of view, the
// colours, the light. And which of at64/at72 is north, which is unestablished.
//
// usage: flight_sequence MAPDIR OUTDIR frames
#include "ac6/demo_flight_view.h"
#include "ac6/retail_flight_session.h"
#include "ac6/retail_flight_step.h"
#include "ac6/retail_map_placement.h"
#include "ac6/retail_map_water.h"
#include "ac6/retail_ndxr_container.h"
#include "ac6/retail_ndxr_geometry.h"
#include "ac6/retail_terrain_field.h"
#include "ac6/retail_transform.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace {
std::vector<std::uint8_t> R(const std::string& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
}  // namespace

int main(int argc, char** argv) {
  using namespace ac6::retail;
  if (argc < 4) { std::fprintf(stderr, "usage: MAPDIR OUTDIR FRAMES\n"); return 2; }
  const std::string dir = argv[1], out = argv[2];
  const int frames = std::atoi(argv[3]);

  const auto g = R(dir + "/004_00_01_02_03.bin"), p = R(dir + "/005_Bl_02_b8.bin");
  const auto a = R(dir + "/001_MCA_00.bin"), i2 = R(dir + "/003_MCI_00.bin");
  const auto m = R(dir + "/002_MCD_00.bin");
  const auto field = TerrainField::open(g.data(), g.size(), p.data(), p.size());
  const auto water = MapWaterGrid::open(a.data(), a.size(), i2.data(), i2.size(),
                                        m.data(), m.size());
  if (!field || !water) { std::fprintf(stderr, "the map refused\n"); return 1; }

  // The placement list, and a cache of each part's triangles in LOCAL space.
  // The cache is what makes this affordable: 170 models loaded once, reused
  // across every frame and every instance.
  const auto pdl = R(dir + "/011_00_00_00_00.bin");
  const auto placement = MapPlacement::open(pdl.data(), pdl.size());
  if (!placement) { std::fprintf(stderr, "the placement list refused\n"); return 1; }
  std::map<std::uint16_t, std::vector<float>> parts;
  auto local_triangles = [&](std::uint16_t id) -> const std::vector<float>& {
    auto it = parts.find(id);
    if (it != parts.end()) return it->second;
    std::vector<float> tris;
    char name[64];
    std::snprintf(name, sizeof(name), "/014_FHM/%03u_NDXR.ndxr", id);
    const auto bytes = R(dir + name);
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
              const std::uint16_t a0 = piece->indices[i - 2];
              const std::uint16_t b0 = piece->indices[i - 1];
              const std::uint16_t c0 = piece->indices[i];
              if (a0 == kStripRestart || b0 == kStripRestart || c0 == kStripRestart)
                continue;
              if (a0 >= piece->positions.size() || b0 >= piece->positions.size() ||
                  c0 >= piece->positions.size())
                continue;
              for (std::uint16_t idx : {a0, b0, c0}) {
                const auto& q = piece->positions[idx];
                tris.push_back(q.x);
                tris.push_back(q.y);
                tris.push_back(q.z);
              }
            }
          }
        }
      }
    }
    return parts.emplace(id, std::move(tris)).first->second;
  };

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

  // Inland, north of the city, heading +z -- which is the identity basis's
  // forward, so no invented starting attitude is needed. The run crosses the
  // hills, the city, the bay and out to sea.
  FlightSessionState state{};
  state.position.at64 = 1000.0F;
  state.position.at68 = 420.0F;
  state.position.at72 = -24000.0F;

  ac6::demo::Image image;
  image.width = 960;
  image.height = 540;
  image.rgb.assign(static_cast<std::size_t>(image.width) * image.height * 3, 0);
  ac6::demo::DemoCamera camera{};

  const float kStep = 1.0F / 60.0F;
  const float kSpeed = 1500.0F;                    // retail's units; mine
  int written = 0;
  for (int f = 0; f < frames; ++f) {
    // The stick programme: hold a row-1 rotation, which cycle 1471 paired with
    // stick 13 by a control matrix. A held command bends the path steadily.
    FlightStick stick{};
    stick.target13 = 0.5F;
    stick.increment13 = 1.0F;
    step_flight_session(state, config, stick, kStep);

    const auto& forward = state.basis.rows[2];
    FlightRates heading{};
    heading.to64 = forward[0];
    heading.to68 = forward[1];
    heading.to72 = forward[2];
    integrate_session_position(state, heading, kSpeed, 0.0F, kStep);

    if (f % 2) continue;                           // 60 Hz sim, 30 fps film
    ac6::demo::draw_terrain_view(image, state.basis, camera, state.position,
                                 *field, &water.value());

    // The buildings, on top of the ground and depth-tested against it.
    // Culled by distance -- 6000 units, mine -- because 4,318 instances at
    // full model detail is not a frame budget.
    std::vector<float> world;
    for (const MapInstance& q : placement->instances()) {
      if (!q.accepted) continue;
      const float dx = q.world_x - state.position.at64;
      const float dz = q.world_z - state.position.at72;
      if (dx * dx + dz * dz > 6000.0F * 6000.0F) continue;
      const std::vector<float>& tris = local_triangles(q.selector);
      for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
        world.push_back(q.world_x + tris[t]);
        world.push_back(q.world_y + tris[t + 1]);
        world.push_back(q.world_z + tris[t + 2]);
      }
    }
    ac6::demo::draw_world_triangles(image, state.basis, camera, state.position,
                                    world);
    char name[512];
    std::snprintf(name, sizeof(name), "%s/frame-%04d.ppm", out.c_str(), written++);
    if (!image.write_ppm(name)) { std::fprintf(stderr, "write failed\n"); return 1; }
  }
  std::printf("%d ticks, %d frames; final position (%.0f, %.0f, %.0f)\n", frames, written,
              state.position.at64, state.position.at68, state.position.at72);
  return 0;
}
