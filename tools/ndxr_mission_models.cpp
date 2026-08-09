// Every model Mission 01 actually spawns, drawn as a contact sheet.
//
// The ids are NOT chosen here. They come from the scenario's own model bytes at
// +0x61/+0x62 of each Obj record, parsed by MissionScenario and joined to the
// package by ModelDirectory -- the join retail_model_directory_tests.cpp
// already checks at 311 resolved bindings and 38 distinct primaries.
//
//   g++ -std=c++20 -O2 -I reconstruction/ace-combat-6/include \
//       tools/ndxr_mission_models.cpp \
//       -Lreconstruction/ace-combat-6/build -lac6_product_core -o sheet
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
#include <set>
#include <vector>

namespace {

std::vector<std::uint8_t> Read(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)),
                                   std::istreambuf_iterator<char>());
}

// Every descriptor of every NDXR in one directory entry, merged.
bool LoadModel(const std::vector<std::uint8_t>& blob,
               const ac6::retail::ModelDirectory& directory, std::uint32_t id,
               ac6::retail::NdxrMesh& out, int& pieces) {
  const auto entry = directory.entry(id);
  if (!entry) return false;
  const std::uint8_t* fhm = blob.data() + entry->offset;
  ac6::retail::ContainerIndex index{};
  if (!ac6::retail::parse_container_index(index, fhm, entry->size,
                                          static_cast<std::uint32_t>(entry->offset))) {
    return false;
  }
  pieces = 0;
  bool first = true;
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
        if (first) { out.bounds = mesh->bounds; first = false; }
        else {
          out.bounds.min_x = std::fmin(out.bounds.min_x, mesh->bounds.min_x);
          out.bounds.min_y = std::fmin(out.bounds.min_y, mesh->bounds.min_y);
          out.bounds.min_z = std::fmin(out.bounds.min_z, mesh->bounds.min_z);
          out.bounds.max_x = std::fmax(out.bounds.max_x, mesh->bounds.max_x);
          out.bounds.max_y = std::fmax(out.bounds.max_y, mesh->bounds.max_y);
          out.bounds.max_z = std::fmax(out.bounds.max_z, mesh->bounds.max_z);
        }
        ++pieces;
      }
    }
  }
  return !first;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) { std::printf("usage: sheet MDLP SCENARIO OUT.ppm [tilt]\n"); return 2; }
  const std::vector<std::uint8_t> blob = Read(argv[1]);
  const std::vector<std::uint8_t> payload = Read(argv[2]);
  if (blob.empty() || payload.empty()) { std::printf("missing input\n"); return 1; }

  const auto directory = ac6::retail::ModelDirectory::open(blob.data(), blob.size());
  const auto parsed = ac6::retail::ScenarioPayload::open(payload);
  if (!directory || !parsed) { std::printf("cannot open inputs\n"); return 1; }
  const auto scenario = ac6::retail::MissionScenario::parse(*parsed);
  if (!scenario) { std::printf("cannot parse the scenario\n"); return 1; }

  // THE IDS COME FROM THE MISSION, not from this file.
  std::map<std::uint8_t, int> uses;
  for (const auto& unit : scenario->units()) {
    for (const auto& binding : unit.model_bindings) {
      if (binding.has_model()) uses[binding.primary]++;
    }
  }
  std::printf("scenario: %zu units, %zu distinct primary model ids\n",
              scenario->units().size(), uses.size());

  const int columns = 8;
  const int tile_w = 200, tile_h = 130;
  const int rows = static_cast<int>((uses.size() + columns - 1) / columns);
  ac6::demo::Image sheet;
  sheet.width = columns * tile_w;
  sheet.height = rows * tile_h;
  sheet.rgb.assign(static_cast<std::size_t>(sheet.width) * sheet.height * 3, 0);
  sheet.clear(10, 12, 18);

  const float tilt = argc >= 5 ? static_cast<float>(std::atof(argv[4])) : 0.45F;
  int cell = 0, drawn = 0;
  long long total_vertices = 0;
  for (const auto& [id, count] : uses) {
    ac6::retail::NdxrMesh mesh;
    int pieces = 0;
    const bool ok = LoadModel(blob, *directory, id, mesh, pieces);
    if (ok) {
      const float rx = mesh.bounds.max_x - mesh.bounds.min_x;
      const float ry = mesh.bounds.max_y - mesh.bounds.min_y;
      const float rz = mesh.bounds.max_z - mesh.bounds.min_z;
      const float radius = std::fmax(rx, std::fmax(ry, rz));
      ac6::demo::Image tile;
      tile.width = tile_w; tile.height = tile_h;
      tile.rgb.assign(static_cast<std::size_t>(tile_w) * tile_h * 3, 0);
      ac6::retail::RetailBasis basis = ac6::retail::identity_basis();
      ac6::retail::rotate_820A9B30(basis, 0.9F);
      ac6::retail::rotate_820A99F8(basis, tilt);
      ac6::demo::draw_mesh_wireframe(tile, mesh, basis, ac6::demo::DemoCamera{},
                                     radius * 1.7F);
      const int ox = (cell % columns) * tile_w;
      const int oy = (cell / columns) * tile_h;
      for (int y = 0; y < tile_h; ++y) {
        for (int x = 0; x < tile_w; ++x) {
          const std::size_t s = (static_cast<std::size_t>(y) * tile_w + x) * 3;
          const std::size_t d = (static_cast<std::size_t>(oy + y) * sheet.width + ox + x) * 3;
          sheet.rgb[d] = tile.rgb[s];
          sheet.rgb[d + 1] = tile.rgb[s + 1];
          sheet.rgb[d + 2] = tile.rgb[s + 2];
        }
      }
      ++drawn;
      total_vertices += static_cast<long long>(mesh.positions.size());
      std::printf("  id %3u  used %3d  pieces %4d  verts %6zu  extent %8.2f %8.2f %8.2f\n",
                  id, count, pieces, mesh.positions.size(), rx, ry, rz);
    } else {
      std::printf("  id %3u  used %3d  NOTHING DECODED\n", id, count);
    }
    ++cell;
  }
  std::printf("drew %d of %zu, %lld vertices total\n", drawn, uses.size(), total_vertices);
  return sheet.write_ppm(argv[3]) ? 0 : 1;
}
