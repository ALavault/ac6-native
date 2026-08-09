// The map with its own textures, its own sun and its own fog.
//
// Cycle 1473 drew flat grey buildings under a flat fill and a reviewer called it
// what it was. Everything missing was already in the archive:
//
//   textures   Material -> TextureRef -> texture id -> the NTXR with that GIDX
//   sun        .sky1.sun.lrx = 40, .sky1.sun.lry = 145
//   fog        .sky1.fog.far = 24000, .sky1.fog.density = 0.014
//   distances  .mapparts.distanceL/M/S = 16000 / 12000 / 10000
//
// The last three come from `022_FHM`'s mapset XML, which cycle 1474 opened. The
// sky gradient is mine; the sun angle it is drawn around is not.
#include "ac6/demo_flight_view.h"
#include "ac6/ntxr_texture.h"
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
#include <string>
#include <vector>

namespace {
std::vector<std::uint8_t> R(const std::string& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
// ONE GROUP PER DESCRIPTOR, not one texture per part. Cycle 1476 measured the
// package: 4,318 descriptors, every one carrying exactly one texture in slot 0
// and every one with texcoords. Taking the first descriptor's texture for the
// whole model -- which cycle 1475 did -- draws most of a building with some
// other building's skin, and the grey faces in that render were one particular
// texture applied where it did not belong.
struct Group { std::uint32_t texture = 0; std::vector<float> xyz, uv; };
// ONE PART PER (model, CLASS). A reviewer saw the bridge stacked the way the
// aircraft were at cycle 1430, and the counts agree exactly: the package holds
// 4,318 records and the placement list holds 4,318 instances, and the record
// name's class token after `_m01_` histograms as l=289 m=584 s=3277 x=112
// airport=56 -- against the tag's bits 30..31 at 345/584/3277/112, with
// l+airport = 345 and x-skipped = 20 matching to the unit.
//
// So bits 30..31 select a DRAW-DISTANCE CLASS, the same l/m/s the mapset's
// .mapparts.distanceL/M/S = 16000/12000/10000 name -- not the rotation quadrant
// cycle 1452 read them as. Drawing every record of a model at every instance
// draws each building as a pile of every variant of itself.
struct Part { std::map<unsigned, std::vector<Group>> by_class; };
static unsigned class_of(const std::string& name) {
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
}  // namespace

int main(int argc, char** argv) {
  using namespace ac6::retail;
  if (argc < 7) { std::fprintf(stderr, "usage: MAPDIR OUT.ppm x y z yawdeg\n"); return 2; }
  const std::string dir = argv[1];
  FlightPosition eye{};
  eye.at64 = std::strtof(argv[3], nullptr);
  eye.at68 = std::strtof(argv[4], nullptr);
  eye.at72 = std::strtof(argv[5], nullptr);
  const float yaw = std::strtof(argv[6], nullptr) * 3.14159265F / 180.0F;

  const auto g = R(dir + "/004_00_01_02_03.bin"), p = R(dir + "/005_Bl_02_b8.bin");
  const auto a = R(dir + "/001_MCA_00.bin"), i2 = R(dir + "/003_MCI_00.bin");
  const auto m = R(dir + "/002_MCD_00.bin"), pdl = R(dir + "/011_00_00_00_00.bin");
  const auto field = TerrainField::open(g.data(), g.size(), p.data(), p.size());
  const auto water = MapWaterGrid::open(a.data(), a.size(), i2.data(), i2.size(),
                                        m.data(), m.size());
  const auto placement = MapPlacement::open(pdl.data(), pdl.size());
  if (!field || !water || !placement) { std::fprintf(stderr, "refused\n"); return 1; }

  // Every .ntxr beside the parts, by GIDX identifier.
  std::map<std::uint32_t, std::vector<std::uint8_t>> wrappers;
  // 015_FHM MIRRORS 014_FHM: 170 .ntxr against 170 .ndxr, 86 .bin against 86.
  // Cycle 1474 looked for textures beside the models, found none, and listed
  // "0 with a texture" -- the parallel container had been an open defect since
  // 1445 and nothing had opened it.
  for (const std::string sub : {"/015_FHM", "/016_FHM"})
  for (const auto& e : std::filesystem::directory_iterator(dir + sub)) {
    if (e.path().extension() != ".ntxr") continue;
    auto blob = R(e.path().string());
    for (std::size_t q = 0; q + 12 <= blob.size(); ++q) {
      if (std::memcmp(blob.data() + q, "GIDX", 4) != 0) continue;
      const std::uint32_t id = (std::uint32_t(blob[q + 8]) << 24) |
                               (std::uint32_t(blob[q + 9]) << 16) |
                               (std::uint32_t(blob[q + 10]) << 8) | blob[q + 11];
      wrappers[id] = blob;
      break;
    }
  }
  std::map<std::uint32_t, DecodedTexture> textures;
  auto texture_for = [&](std::uint32_t id) -> const DecodedTexture* {
    auto it = textures.find(id);
    if (it != textures.end()) return it->second.pixels.empty() ? nullptr : &it->second;
    DecodedTexture out{};
    const auto found = wrappers.find(id);
    if (found != wrappers.end()) {
      // DO NOT TRIM A WRAPPER THAT HAS A MIP CHAIN. The rule
      // `0x10 + data_offset + single_level_surface_bytes` comes from cycle
      // 1435 and is right for a single-level wrapper whose array 1 is padded.
      // Every one of the map's 177 wrappers declares seven levels, and trimming
      // to the base surface cuts the chain off -- so the decoder's own check
      // `base + chain == payload` fails and refuses all 177.
      //
      // Measured: untrimmed 177 of 177 decode, trimmed 0 of 177. The decoder
      // was right the whole time and the caller was wrong.
      std::size_t span = found->second.size();
      if (const auto d = parse_ntxr_descriptor(found->second.data(), span)) {
        const std::size_t level = single_level_surface_bytes(*d);
        if (d->mip_count <= 1 && level != 0 && 0x10u + d->data_offset + level <= span)
          span = 0x10u + d->data_offset + level;
      }
      NtxrRefusal why{};
      if (const auto dec = decode_ntxr_base_level(found->second.data(), span, true, &why))
        out = *dec;
    }
    it = textures.emplace(id, std::move(out)).first;
    return it->second.pixels.empty() ? nullptr : &it->second;
  };

  std::map<std::uint16_t, Part> parts;
  auto part_for = [&](std::uint16_t id) -> const Part& {
    auto it = parts.find(id);
    if (it != parts.end()) return it->second;
    Part part;
    char name[64];
    std::snprintf(name, sizeof(name), "/014_FHM/%03u_NDXR.ndxr", id);
    const auto bytes = R(dir + name);
    if (!bytes.empty()) {
      if (const auto c = NdxrContainer::Open(bytes.data(), bytes.size())) {
        for (std::uint16_t r = 0; r < c->record_count(); ++r) {
          const auto rec = c->Record(r);
          if (!rec) continue;
          const unsigned klass = class_of(std::string(rec->name));
          std::vector<Group>& groups = part.by_class[klass];
          for (std::uint16_t k = 0; k < rec->descriptor_count; ++k) {
            std::uint32_t tex = 0;
            for (unsigned slot = 0; slot < 4 && tex == 0; ++slot) {
              const auto mat = c->Material(*rec, k, slot);
              if (!mat || mat->texture_count == 0) continue;
              if (const auto ref = c->TextureRef(*mat, 0)) tex = ref->texture_id;
            }
            const auto d = c->Descriptor(*rec, k);
            if (!d) continue;
            const auto piece = decode_ndxr_descriptor(*c, bytes.data(), bytes.size(), *d);
            if (!piece || piece->texcoords.size() < piece->positions.size()) continue;
            Group* group = nullptr;
            for (Group& g : groups) if (g.texture == tex) { group = &g; break; }
            if (group == nullptr) { groups.push_back(Group{tex, {}, {}}); group = &groups.back(); }
            for (std::size_t i = 2; i < piece->indices.size(); ++i) {
              const std::uint16_t t0 = piece->indices[i - 2], t1 = piece->indices[i - 1],
                                  t2 = piece->indices[i];
              if (t0 == kStripRestart || t1 == kStripRestart || t2 == kStripRestart) continue;
              if (t0 >= piece->positions.size() || t1 >= piece->positions.size() ||
                  t2 >= piece->positions.size()) continue;
              for (std::uint16_t idx : {t0, t1, t2}) {
                const auto& q = piece->positions[idx];
                group->xyz.push_back(q.x); group->xyz.push_back(q.y); group->xyz.push_back(q.z);
                const auto& uvv = piece->texcoords[idx];
                group->uv.push_back(uvv.u); group->uv.push_back(uvv.v);
              }
            }
          }
        }
      }
    }
    return parts.emplace(id, std::move(part)).first->second;
  };

  ac6::demo::Image image;
  image.width = 1280; image.height = 720;
  image.rgb.assign(std::size_t(image.width) * image.height * 3, 0);
  ac6::demo::DemoCamera camera{};
  RetailBasis basis = identity_basis();
  rotate_820A9B30(basis, yaw);

  // The sky: a vertical gradient, mine, drawn around .sky1's sun angle.
  const float lrx = 40.0F * 3.14159265F / 180.0F;
  const float lry = 145.0F * 3.14159265F / 180.0F;
  const float sun[3] = {std::cos(lrx) * std::sin(lry), std::sin(lrx),
                        std::cos(lrx) * std::cos(lry)};
  for (int y = 0; y < image.height; ++y) {
    const float t = float(y) / float(image.height);
    const std::uint8_t r = std::uint8_t(120 + 100 * t);
    const std::uint8_t gg = std::uint8_t(150 + 80 * t);
    const std::uint8_t b = std::uint8_t(205 + 40 * t);
    for (int x = 0; x < image.width; ++x) {
      const std::size_t o = (std::size_t(y) * image.width + x) * 3;
      image.rgb[o] = r; image.rgb[o + 1] = gg; image.rgb[o + 2] = b;
    }
  }
  image.clear_depth();
  ac6::demo::draw_terrain_view_over(image, basis, camera, eye, *field, &water.value());

  const float kFogFar = 24000.0F, kFogDensity = 0.014F, kDistanceL = 16000.0F;
  std::size_t drawn = 0, textured = 0;
  for (const MapInstance& q : placement->instances()) {
    if (!q.accepted) continue;
    const float dx = q.world_x - eye.at64, dz = q.world_z - eye.at72;
    if (dx * dx + dz * dz > kDistanceL * kDistanceL) continue;
    const Part& part = part_for(q.selector);
    const unsigned klass = (static_cast<std::uint32_t>(q.tag_high) >> 14) & 3u;
    const auto found_class = part.by_class.find(klass);
    if (found_class == part.by_class.end()) continue;
    for (const Group& gr : found_class->second) {
      std::vector<float> world(gr.xyz.size());
      for (std::size_t i = 0; i + 2 < gr.xyz.size(); i += 3) {
        world[i] = q.world_x + gr.xyz[i];
        world[i + 1] = q.world_y + gr.xyz[i + 1];
        world[i + 2] = q.world_z + gr.xyz[i + 2];
      }
      const DecodedTexture* tex = texture_for(gr.texture);
      if (tex) ++textured;
      ac6::demo::draw_world_triangles_textured(
          image, basis, camera, eye, world, gr.uv,
          tex ? tex->pixels.data() : nullptr, tex ? int(tex->width) : 0,
          tex ? int(tex->height) : 0, sun, kFogFar, kFogDensity, 170, 190, 220);
    }
    ++drawn;
  }
  std::printf("%zu instances drawn, %zu with a texture; %zu textures decoded\n",
              drawn, textured, textures.size());
  return image.write_ppm(argv[2]) ? 0 : 1;
}
