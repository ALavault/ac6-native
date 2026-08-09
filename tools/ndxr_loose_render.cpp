// A loose .ndxr file, rendered. idx_0119 holds Mission 01's map as 178 of them
// -- `mapparts_m01_*` -- rather than inside an MDLP, so the model-directory and
// container-index hops do not apply and the file IS the container.
//
// Textures are looked up among loose .ntxr siblings by GIDX identifier, the
// same key cycle 1435 established.
#include "ac6/demo_flight_view.h"
#include "ac6/ntxr_texture.h"
#include "ac6/retail_ndxr_geometry.h"
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
std::vector<std::uint8_t> Read(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
}
std::uint32_t Be32(const std::uint8_t* a) {
  return (std::uint32_t(a[0]) << 24) | (std::uint32_t(a[1]) << 16) |
         (std::uint32_t(a[2]) << 8) | a[3];
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) { std::printf("usage: loose NDXR OUTDIR [frames] [w] [h]\n"); return 2; }
  const std::filesystem::path ndxr = argv[1];
  const int frames = argc >= 4 ? std::atoi(argv[3]) : 1;
  const int w = argc >= 5 ? std::atoi(argv[4]) : 1280;
  const int h = argc >= 6 ? std::atoi(argv[5]) : 720;

  const std::vector<std::uint8_t> bytes = Read(ndxr);
  const auto container = ac6::retail::NdxrContainer::Open(bytes.data(), bytes.size());
  if (!container) { std::printf("refused\n"); return 1; }

  // Every .ntxr beside it, by GIDX identifier.
  std::map<std::uint32_t, std::vector<std::uint8_t>> textures;
  for (const auto& sibling : std::filesystem::directory_iterator(ndxr.parent_path())) {
    if (sibling.path().extension() != ".ntxr") continue;
    auto blob = Read(sibling.path());
    for (std::size_t q = 0; q + 12 <= blob.size(); ++q) {
      if (std::memcmp(blob.data() + q, "GIDX", 4) != 0) continue;
      textures[Be32(blob.data() + q + 8)] = blob;
      break;
    }
  }

  ac6::retail::NdxrMesh mesh;
  std::uint32_t want_texture = 0;
  bool first = true;
  std::string name;
  for (std::uint16_t r = 0; r < container->record_count(); ++r) {
    const auto record = container->Record(r);
    if (!record) continue;
    if (name.empty()) name = std::string(record->name);
    for (std::uint16_t k = 0; k < record->descriptor_count; ++k) {
      if (want_texture == 0) {
        for (unsigned slot = 0; slot < 4 && want_texture == 0; ++slot) {
          const auto material = container->Material(*record, k, slot);
          if (!material || material->texture_count == 0) continue;
          const auto ref = container->TextureRef(*material, 0);
          if (ref) want_texture = ref->texture_id;
        }
      }
      const auto descriptor = container->Descriptor(*record, k);
      if (!descriptor) continue;
      const auto piece = ac6::retail::decode_ndxr_descriptor(
          *container, bytes.data(), bytes.size(), *descriptor);
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
      if (first) { mesh.bounds = piece->bounds; first = false; }
      else {
        mesh.bounds.min_x = std::fmin(mesh.bounds.min_x, piece->bounds.min_x);
        mesh.bounds.min_y = std::fmin(mesh.bounds.min_y, piece->bounds.min_y);
        mesh.bounds.min_z = std::fmin(mesh.bounds.min_z, piece->bounds.min_z);
        mesh.bounds.max_x = std::fmax(mesh.bounds.max_x, piece->bounds.max_x);
        mesh.bounds.max_y = std::fmax(mesh.bounds.max_y, piece->bounds.max_y);
        mesh.bounds.max_z = std::fmax(mesh.bounds.max_z, piece->bounds.max_z);
      }
    }
  }
  if (mesh.positions.empty()) { std::printf("nothing decoded\n"); return 1; }

  ac6::retail::DecodedTexture texture;
  bool textured = false;
  const auto found = textures.find(want_texture);
  if (found != textures.end()) {
    std::size_t span = found->second.size();
    if (const auto d = ac6::retail::parse_ntxr_descriptor(found->second.data(), span)) {
      const std::size_t level = ac6::retail::single_level_surface_bytes(*d);
      if (level != 0 && 0x10u + d->data_offset + level <= span)
        span = 0x10u + d->data_offset + level;
    }
    if (const auto dec = ac6::retail::decode_ntxr_base_level(found->second.data(), span,
                                                             true, nullptr)) {
      texture = *dec; textured = true;
    }
  }
  const float rx = mesh.bounds.max_x - mesh.bounds.min_x;
  const float ry = mesh.bounds.max_y - mesh.bounds.min_y;
  const float rz = mesh.bounds.max_z - mesh.bounds.min_z;
  std::printf("%s: %s  verts %zu  %.1f x %.1f x %.1f  texture %u %s\n",
              ndxr.filename().string().c_str(), name.c_str(), mesh.positions.size(),
              rx, ry, rz, want_texture, textured ? "ok" : "MISSING");
  const float radius = std::fmax(rx, std::fmax(ry, rz));

  for (int frame = 0; frame < frames; ++frame) {
    ac6::demo::Image image;
    image.width = w; image.height = h;
    image.rgb.assign(std::size_t(w) * h * 3, 0);
    ac6::retail::RetailBasis basis = ac6::retail::identity_basis();
    ac6::retail::rotate_820A9B30(basis, 6.2831853F * float(frame) / float(frames ? frames : 1));
    ac6::retail::rotate_820A99F8(basis, 0.75F);
    if (textured) {
      ac6::demo::draw_mesh_textured(image, mesh, basis, ac6::demo::DemoCamera{},
                                    radius * 1.5F, texture.pixels.data(),
                                    int(texture.width), int(texture.height));
    } else {
      ac6::demo::draw_mesh_solid(image, mesh, basis, ac6::demo::DemoCamera{}, radius * 1.5F);
    }
    char path[512];
    std::snprintf(path, sizeof(path), "%s/loose-%05d.ppm", argv[2], frame);
    image.write_ppm(path);
  }
  return 0;
}
