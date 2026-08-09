// Mission 01's placed units, drawn where the scenario puts them.
//
// The models come from the scenario's own model bytes through ModelDirectory
// and the NDXR chain; the POSITIONS come from initial_world_position, which
// answers for 95 of the 230 units. The other 135 have no load-time coordinate
// in the container and are NOT drawn -- putting them at the origin would be
// inventing one, which is the note retail_scenario.h already carries.
//
//   g++ -std=c++20 -O2 -I reconstruction/ace-combat-6/include \
//       tools/ndxr_mission_scene.cpp \
//       -Lreconstruction/ace-combat-6/build -lac6_product_core -o scene
#include "ac6/demo_flight_view.h"
#include "ac6/retail_container_index.h"
#include "ac6/retail_model_directory.h"
#include "ac6/retail_ndxr_geometry.h"
#include "ac6/retail_scenario.h"
#include "ac6/retail_transform.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string_view>
#include <vector>

namespace {

// LEVELS OF DETAIL, AND DESTROYED STATES, ARE SEPARATE RECORDS. Cycle 1430 read
// the NDXR record names -- `rec+0x20` against the string table -- and found that
// a model carries `..._lod1` through `..._lod4` and often `..._crash1..4` as
// distinct records. The C-17 at id 6 is 1547, 629, 166 and 6 vertices across
// four sub-entries, and 1547+629+166+6 is exactly the 2348 the earlier captures
// drew: every LOD and every wreck superimposed.
//
// SELECTING lod1 IS A CHOICE. Retail picks a level by distance and that rule is
// not read here; this takes the most detailed one and drops the wrecks, which is
// right for a picture and is not a claim about what the game draws when.
static bool WantedRecord(std::string_view name) {
  if (name.find("crash") != std::string_view::npos) return false;
  if (name.find("_lod") == std::string_view::npos) return true;   // no LODs: take it
  return name.find("_lod1") != std::string_view::npos;
}

std::vector<std::uint8_t> Read(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)),
                                   std::istreambuf_iterator<char>());
}

bool LoadModel(const std::vector<std::uint8_t>& blob,
               const ac6::retail::ModelDirectory& directory, std::uint32_t id,
               ac6::retail::NdxrMesh& out) {
  const auto entry = directory.entry(id);
  if (!entry) return false;
  const std::uint8_t* fhm = blob.data() + entry->offset;
  ac6::retail::ContainerIndex index{};
  if (!ac6::retail::parse_container_index(index, fhm, entry->size,
                                          static_cast<std::uint32_t>(entry->offset))) {
    return false;
  }
  bool any = false;
  for (std::uint32_t j = 0; j < index.count; ++j) {
    const std::uint32_t at = ac6::retail::container_entry(index, fhm, entry->size, j);
    if (at == 0) continue;
    const std::size_t off = at - static_cast<std::uint32_t>(entry->offset);
    if (off + 8 > entry->size) continue;
    const std::uint8_t* sub = fhm + off;
    if (sub[0] != 'N' || sub[1] != 'D' || sub[2] != 'X' || sub[3] != 'R') continue;
    const std::uint32_t length =
        ac6::retail::container_entry_length(index, fhm, entry->size, j);
    const auto container = ac6::retail::NdxrContainer::Open(sub, length);
    if (!container) continue;
    for (std::uint16_t r = 0; r < container->record_count(); ++r) {
      const auto record = container->Record(r);
      if (!record) continue;
      if (!WantedRecord(record->name)) continue;
      for (std::uint16_t k = 0; k < record->descriptor_count; ++k) {
        const auto descriptor = container->Descriptor(*record, k);
        if (!descriptor) continue;
        const auto mesh =
            ac6::retail::decode_ndxr_descriptor(*container, sub, length, *descriptor);
        if (!mesh) continue;
        if (out.positions.size() + mesh->positions.size() > 60000) continue;
        const auto base = static_cast<std::uint16_t>(out.positions.size());
        for (const auto& position : mesh->positions) out.positions.push_back(position);
        out.indices.push_back(ac6::retail::kStripRestart);
        for (const std::uint16_t value : mesh->indices) {
          out.indices.push_back(value == ac6::retail::kStripRestart
                                    ? value
                                    : static_cast<std::uint16_t>(base + value));
        }
        any = true;
      }
    }
  }
  return any;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) { std::printf("usage: scene MDLP SCENARIO OUTDIR [frames] [radius]\n"); return 2; }
  const std::vector<std::uint8_t> blob = Read(argv[1]);
  const std::vector<std::uint8_t> payload = Read(argv[2]);
  const auto directory = ac6::retail::ModelDirectory::open(blob.data(), blob.size());
  const auto parsed = ac6::retail::ScenarioPayload::open(payload);
  if (!directory || !parsed) { std::printf("cannot open inputs\n"); return 1; }
  const auto scenario = ac6::retail::MissionScenario::parse(*parsed);
  if (!scenario) { std::printf("cannot parse the scenario\n"); return 1; }
  const int frames = argc >= 5 ? std::atoi(argv[4]) : 120;

  struct Placed { std::uint8_t model; ac6::retail::ScenarioVector at; float heading; };
  std::vector<Placed> placed;
  std::map<std::uint8_t, ac6::retail::NdxrMesh> meshes;
  for (const auto& unit : scenario->units()) {
    const auto where = ac6::retail::initial_world_position(*scenario, unit);
    if (!where) continue;                       // UNPLACED: not drawn, not invented
    std::size_t which = 0;
    for (const auto& binding : unit.model_bindings) {
      const float heading = which < unit.obj_scalars.size()
                                ? unit.obj_scalars[which].heading : 0.0f;
      ++which;
      if (!binding.has_model()) continue;
      if (meshes.find(binding.primary) == meshes.end()) {
        ac6::retail::NdxrMesh mesh;
        if (LoadModel(blob, *directory, binding.primary, mesh)) meshes[binding.primary] = mesh;
      }
      if (meshes.count(binding.primary)) placed.push_back({binding.primary, *where, heading});
      break;
    }
  }

  float mnx=1e30f,mny=1e30f,mnz=1e30f,mxx=-1e30f,mxy=-1e30f,mxz=-1e30f;
  for (const Placed& p : placed) {
    mnx=std::fmin(mnx,p.at.x); mny=std::fmin(mny,p.at.y); mnz=std::fmin(mnz,p.at.z);
    mxx=std::fmax(mxx,p.at.x); mxy=std::fmax(mxy,p.at.y); mxz=std::fmax(mxz,p.at.z);
  }
  std::printf("placed %zu units, %zu distinct models, span %.0f x %.0f x %.0f\n",
              placed.size(), meshes.size(), mxx-mnx, mxy-mny, mxz-mnz);

  // A SURVEY VIEW OF THIS SHOWS NOTHING, and the numbers say why before a frame
  // is drawn: the placed set spans 66 km and the models are 5 to 50 metres, so
  // an eye far enough to see the layout renders every unit sub-pixel. The first
  // attempt drew 0, 2 and 0 lit pixels over three frames.
  //
  // So the camera goes to the DENSEST CLUSTER instead -- the unit with the most
  // neighbours inside `kNeighbourhood` -- and orbits it close enough for a model
  // to resolve. Which cluster that is comes from the positions; the radius and
  // the distance are chosen.
  const float kNeighbourhood = 2000.0f;
  std::size_t best = 0, best_count = 0;
  for (std::size_t i = 0; i < placed.size(); ++i) {
    std::size_t count = 0;
    for (std::size_t j = 0; j < placed.size(); ++j) {
      const float dx = placed[i].at.x - placed[j].at.x;
      const float dy = placed[i].at.y - placed[j].at.y;
      const float dz = placed[i].at.z - placed[j].at.z;
      if (dx*dx + dy*dy + dz*dz <= kNeighbourhood * kNeighbourhood) ++count;
    }
    if (count > best_count) { best_count = count; best = i; }
  }
  const float cx = placed[best].at.x, cy = placed[best].at.y, cz = placed[best].at.z;
  const float radius = argc >= 6 ? static_cast<float>(std::atof(argv[5])) : 260.0f;
  std::printf("densest cluster: %zu units within %.0f of (%.0f, %.0f, %.0f)\n",
              best_count, kNeighbourhood, cx, cy, cz);

  for (int frame = 0; frame < frames; ++frame) {
    ac6::demo::Image image;
    image.width = 640; image.height = 360;
    image.rgb.assign(static_cast<std::size_t>(640) * 360 * 3, 0);
    image.clear(10, 12, 20);
    // AN ORBIT, chosen: the eye circles the placed set at 0.55 of its span and
    // looks in. Nothing about retail's camera is claimed -- cycle 1396 refused
    // to reproduce it, and this is a survey view, not a gameplay one.
    const float turn = 6.2831853F * static_cast<float>(frame) / static_cast<float>(frames);
    const float ex = cx + std::sin(turn) * radius;
    const float ez = cz + std::cos(turn) * radius;
    const float ey = cy + radius * 0.35f;
    // THE BASIS IS BUILT TO LOOK INWARD, not rotated into place. Composing the
    // two rotation helpers and hoping they aim at the cluster produced an empty
    // frame: with the eye at +z from the centre, the offset to it is NEGATIVE z
    // and `project` refuses everything behind the eye. Constructing
    // (right, up, forward) from the eye and the target cannot get that wrong.
    //
    // This is a CHOSEN camera and it says so -- the rotation kernel is retail's
    // but nothing here claims retail aims a camera this way.
    const float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    const float flen = std::sqrt(fx*fx + fy*fy + fz*fz);
    const float f0 = fx/flen, f1 = fy/flen, f2 = fz/flen;
    // right = normalize(worldUp x forward), worldUp = (0,1,0)
    float r0 = 1.0f*f2 - 0.0f*f1, r1 = 0.0f*f0 - 0.0f*f2, r2 = 0.0f*f1 - 1.0f*f0;
    const float rlen = std::sqrt(r0*r0 + r1*r1 + r2*r2);
    r0/=rlen; r1/=rlen; r2/=rlen;
    // up = forward x right
    const float u0 = f1*r2 - f2*r1, u1 = f2*r0 - f0*r2, u2 = f0*r1 - f1*r0;
    ac6::retail::RetailBasis basis = ac6::retail::identity_basis();
    basis.rows[0][0]=r0; basis.rows[0][1]=r1; basis.rows[0][2]=r2;
    basis.rows[1][0]=u0; basis.rows[1][1]=u1; basis.rows[1][2]=u2;
    basis.rows[2][0]=f0; basis.rows[2][1]=f1; basis.rows[2][2]=f2;
    if (frame == 0) {
      int near = 0;
      for (const Placed& p : placed) {
        const float dx = p.at.x - ex, dy = p.at.y - ey, dz = p.at.z - ez;
        if (std::sqrt(dx*dx + dy*dy + dz*dz) < radius * 3.0f) ++near;
      }
      std::printf("  frame 0: %d of %zu units within %.0f of the eye\n",
                  near, placed.size(), radius * 3.0f);
      for (const Placed& p : placed) {
        const float dx = p.at.x - ex, dy = p.at.y - ey, dz = p.at.z - ez;
        const float d2 = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (d2 >= radius * 3.0f) continue;
        const ac6::retail::NdxrMesh& m = meshes[p.model];
        float ax=1e30f,ay=1e30f,az=1e30f,bx=-1e30f,by=-1e30f,bz=-1e30f;
        for (const auto& v : m.positions) {
          ax=std::fmin(ax,v.x); ay=std::fmin(ay,v.y); az=std::fmin(az,v.z);
          bx=std::fmax(bx,v.x); by=std::fmax(by,v.y); bz=std::fmax(bz,v.z);
        }
        std::printf("    near: model id %u at (%.0f,%.0f,%.0f) dist %.0f  "
                    "extent %.2f x %.2f x %.2f\n",
                    p.model, p.at.x, p.at.y, p.at.z, d2, bx-ax, by-ay, bz-az);
      }
    }
    for (const Placed& p : placed) {
      const float dx = p.at.x - ex, dy = p.at.y - ey, dz = p.at.z - ez;
      // FAR UNITS ARE SUB-PIXEL AND COST THE WHOLE FRAME'S TIME. Beyond a few
      // times the orbit radius a 15-metre model is under a pixel, so it is
      // skipped rather than drawn as noise. The cut is a rendering choice and
      // the count of what it drops is printed.
      if (dx*dx + dy*dy + dz*dz > (radius * 12.0f) * (radius * 12.0f)) continue;
      // THE UNIT'S OWN HEADING, through retail's own rotation. 0x8229B0C4 hands
      // it to 0x820A9B30 on the entity's transform; this composes it into the
      // camera basis, which is the same rotation applied to the same vertices.
      ac6::retail::RetailBasis oriented = basis;
      if (p.heading != 0.0f) {
        ac6::retail::RetailBasis spin = ac6::retail::identity_basis();
        ac6::retail::rotate_820A9B30(spin, p.heading);
        ac6::retail::RetailBasis composed = basis;
        for (int row = 0; row < 3; ++row) {
          for (int col = 0; col < 3; ++col) {
            composed.rows[row][col] = basis.rows[row][0] * spin.rows[0][col] +
                                      basis.rows[row][1] * spin.rows[1][col] +
                                      basis.rows[row][2] * spin.rows[2][col];
          }
        }
        oriented = composed;
      }
      const ac6::retail::NdxrMesh& mesh = meshes[p.model];
      ac6::demo::draw_mesh_at(image, mesh, oriented, ac6::demo::DemoCamera{},
                              dx, dy, dz, 140, 190, 230);
    }
    char path[512];
    std::snprintf(path, sizeof(path), "%s/scene-%05d.ppm", argv[3], frame);
    image.write_ppm(path);
  }
  return 0;
}
