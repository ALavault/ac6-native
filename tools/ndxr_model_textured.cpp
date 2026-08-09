// One model, textured, from the package alone.
//
// The chain, all of it contracted or measured:
//   MDLP[id] -> ContainerIndex -> NDXR -> Record(lod1) -> Descriptor
//               positions, normals, texcoords
//   the record's Material -> TextureRef -> texture_id
//   texture_id == the GIDX identifier of an NTXR anywhere in the package
//   decode_ntxr_base_level on that wrapper's COMPUTED extent (cycle 1435:
//   array 1 is padded for an NTXR, exact only for an NDXR)
//
// WHICH TEXTURE. A material carries several; this takes the FIRST and says so.
// Nothing has been read that orders them or names one the base colour.
#include "ac6/demo_flight_view.h"
#include "ac6/ntxr_texture.h"
#include "ac6/retail_container_index.h"
#include "ac6/retail_model_directory.h"
#include "ac6/retail_ndxr_geometry.h"
#include "ac6/retail_transform.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> Read(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)),
                                   std::istreambuf_iterator<char>());
}
std::uint32_t Be32(const std::uint8_t* at) {
  return (std::uint32_t(at[0]) << 24) | (std::uint32_t(at[1]) << 16) |
         (std::uint32_t(at[2]) << 8) | at[3];
}
struct Wrapper { const std::uint8_t* bytes; std::size_t length; };

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) { std::printf("usage: textured MDLP OUTDIR ID [frames] [tilt] [name] [w] [h]\n"); return 2; }
  const std::vector<std::uint8_t> blob = Read(argv[1]);
  const auto directory = ac6::retail::ModelDirectory::open(blob.data(), blob.size());
  if (!directory) { std::printf("not a directory\n"); return 1; }
  const std::uint32_t want = std::strtoul(argv[3], nullptr, 0);
  const int frames = argc >= 5 ? std::atoi(argv[4]) : 120;
  const float tilt = argc >= 6 ? static_cast<float>(std::atof(argv[5])) : 0.42F;
  const char* only = argc >= 7 ? argv[6] : nullptr;
  // THE LIMITING INSTRUMENT, until cycle 1438. Every picture before it was
  // 480x270, and the open question about entry 2 was what a 2.4-kilometre mesh
  // depicts -- which a thumbnail cannot answer whatever the geometry does.
  const int out_w = argc >= 8 ? std::atoi(argv[7]) : 480;
  const int out_h = argc >= 9 ? std::atoi(argv[8]) : 270;

  // Every NTXR in the package, by its GIDX identifier.
  std::map<std::uint32_t, Wrapper> textures;
  ac6::retail::NdxrMesh mesh;
  int pieces = 0;
  std::uint32_t chosen_texture = 0;
  bool first_piece = true;

  for (std::uint32_t id = 0; id < directory->count(); ++id) {
    const auto entry = directory->entry(id);
    if (!entry) continue;
    const std::uint8_t* fhm = blob.data() + entry->offset;
    ac6::retail::ContainerIndex index{};
    if (!ac6::retail::parse_container_index(index, fhm, entry->size,
                                            static_cast<std::uint32_t>(entry->offset)))
      continue;
    for (std::uint32_t j = 0; j < index.count; ++j) {
      const std::uint32_t at = ac6::retail::container_entry(index, fhm, entry->size, j);
      if (at == 0) continue;
      const std::size_t off = at - static_cast<std::uint32_t>(entry->offset);
      if (off + 8 > entry->size) continue;
      const std::uint8_t* sub = fhm + off;
      const std::uint32_t length =
          ac6::retail::container_entry_length(index, fhm, entry->size, j);
      if (off + length > entry->size) continue;

      if (std::memcmp(sub, "NTXR", 4) == 0) {
        for (std::size_t q = 0; q + 12 <= length; ++q) {
          if (std::memcmp(sub + q, "GIDX", 4) != 0) continue;
          textures[Be32(sub + q + 8)] = {sub, length};
        }
        continue;
      }
      if (id != want || std::memcmp(sub, "NDXR", 4) != 0) continue;
      const auto container = ac6::retail::NdxrContainer::Open(sub, length);
      if (!container) continue;
      for (std::uint16_t r = 0; r < container->record_count(); ++r) {
        const auto record = container->Record(r);
        if (!record) continue;
        // THE SAME RULE AS THE ROSTER TOOL, and the first version of this file
        // did not use it: requiring "_lod1" excludes every record that carries
        // no LOD suffix at all, which is what the terrain's `hire01`.. records
        // are. Entry 2 rendered as an aircraft with 2 pieces instead of terrain
        // with 373 because of exactly that.
        {
          const std::string name(record->name);
          if (name.find("crash") != std::string::npos) continue;
          if (name.find("_lod") != std::string::npos &&
              name.find("_lod1") == std::string::npos) continue;
          // OPTIONAL NAME FILTER. Cycle 1437 found that entry 2 is one terrain
          // record plus 368 `nmbs###` props that ALL sit at the same local
          // origin -- so drawing every record of a model stacks them. Until the
          // per-record transform is read, a caller can name the record it wants.
          if (only != nullptr && name.find(only) == std::string::npos) continue;
        }
        for (std::uint16_t k = 0; k < record->descriptor_count; ++k) {
          if (chosen_texture == 0) {
            for (unsigned slot = 0; slot < 4 && chosen_texture == 0; ++slot) {
              const auto material = container->Material(*record, k, slot);
              if (!material || material->texture_count == 0) continue;
              const auto ref = container->TextureRef(*material, 0);   // THE FIRST
              if (ref) chosen_texture = ref->texture_id;
            }
          }
          const auto descriptor = container->Descriptor(*record, k);
          if (!descriptor) continue;
          const auto piece =
              ac6::retail::decode_ndxr_descriptor(*container, sub, length, *descriptor);
          if (!piece) continue;
          if (mesh.positions.size() + piece->positions.size() > 60000) continue;
          const auto base = static_cast<std::uint16_t>(mesh.positions.size());
          for (const auto& p : piece->positions) mesh.positions.push_back(p);
          for (const auto& n : piece->normals) mesh.normals.push_back(n);
          for (const auto& t : piece->texcoords) mesh.texcoords.push_back(t);
          mesh.indices.push_back(ac6::retail::kStripRestart);
          for (const std::uint16_t v : piece->indices)
            mesh.indices.push_back(v == ac6::retail::kStripRestart
                                       ? v : static_cast<std::uint16_t>(base + v));
          if (first_piece) { mesh.bounds = piece->bounds; first_piece = false; }
          else {
            mesh.bounds.min_x = std::fmin(mesh.bounds.min_x, piece->bounds.min_x);
            mesh.bounds.min_y = std::fmin(mesh.bounds.min_y, piece->bounds.min_y);
            mesh.bounds.min_z = std::fmin(mesh.bounds.min_z, piece->bounds.min_z);
            mesh.bounds.max_x = std::fmax(mesh.bounds.max_x, piece->bounds.max_x);
            mesh.bounds.max_y = std::fmax(mesh.bounds.max_y, piece->bounds.max_y);
            mesh.bounds.max_z = std::fmax(mesh.bounds.max_z, piece->bounds.max_z);
          }
          ++pieces;
        }
      }
    }
  }
  if (mesh.positions.empty()) { std::printf("model %u: nothing decoded\n", want); return 1; }

  ac6::retail::DecodedTexture texture;
  bool textured = false;
  const auto found = textures.find(chosen_texture);
  if (found != textures.end()) {
    std::size_t span = found->second.length;
    if (const auto desc = ac6::retail::parse_ntxr_descriptor(found->second.bytes, span)) {
      const std::size_t level = ac6::retail::single_level_surface_bytes(*desc);
      if (level != 0 && 0x10u + desc->data_offset + level <= span)
        span = 0x10u + desc->data_offset + level;
    }
    ac6::retail::NtxrRefusal why{};
    if (const auto decoded =
            ac6::retail::decode_ntxr_base_level(found->second.bytes, span, true, &why)) {
      texture = *decoded;
      textured = true;
    } else {
      std::printf("texture %u refused: %d\n", chosen_texture, static_cast<int>(why));
    }
  }
  std::printf("  merged bounds x[%.1f %.1f] y[%.1f %.1f] z[%.1f %.1f], indices %zu\n",
              mesh.bounds.min_x, mesh.bounds.max_x, mesh.bounds.min_y,
              mesh.bounds.max_y, mesh.bounds.min_z, mesh.bounds.max_z,
              mesh.indices.size());
  std::printf("model %u: %d pieces, %zu verts, texture %u -> %s",
              want, pieces, mesh.positions.size(), chosen_texture,
              textured ? "" : "NOT AVAILABLE");
  if (textured) std::printf("%ux%u", texture.width, texture.height);
  std::printf("\n");

  const float rx = mesh.bounds.max_x - mesh.bounds.min_x;
  const float ry = mesh.bounds.max_y - mesh.bounds.min_y;
  const float rz = mesh.bounds.max_z - mesh.bounds.min_z;
  const float radius = std::fmax(rx, std::fmax(ry, rz));
  for (int frame = 0; frame < frames; ++frame) {
    ac6::demo::Image image;
    image.width = out_w; image.height = out_h;
    image.rgb.assign(std::size_t(out_w) * out_h * 3, 0);
    ac6::retail::RetailBasis basis = ac6::retail::identity_basis();
    ac6::retail::rotate_820A9B30(basis, 6.2831853F * float(frame) / float(frames));
    // The tilt is a camera choice and terrain needs a steeper one: a 2.4 km
    // plane 170 m thick, seen at 0.42 rad, is a sliver -- the first terrain
    // render lit 1374 pixels of 129600 and the mesh was correct all along.
    ac6::retail::rotate_820A99F8(basis, tilt);
    if (textured) {
      ac6::demo::draw_mesh_textured(image, mesh, basis, ac6::demo::DemoCamera{},
                                    radius * 1.6F, texture.pixels.data(),
                                    int(texture.width), int(texture.height));
    } else {
      ac6::demo::draw_mesh_solid(image, mesh, basis, ac6::demo::DemoCamera{},
                                 radius * 1.6F);
    }
    char path[512];
    std::snprintf(path, sizeof(path), "%s/tex-%05d.ppm", argv[2], frame);
    image.write_ppm(path);
  }
  return 0;
}
