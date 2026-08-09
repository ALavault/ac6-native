// Renders one NDXR mesh, reached entirely through contracted resolution.
#include "ac6/demo_flight_view.h"
#include "ac6/retail_container_index.h"
#include "ac6/retail_model_directory.h"
#include "ac6/retail_ndxr_geometry.h"
#include "ac6/retail_transform.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
int main(int argc, char** argv) {
  if (argc < 4) { std::printf("usage: model MDLP OUTDIR MODEL_ID [frames]\n"); return 2; }
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<std::uint8_t> blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  auto dir = ac6::retail::ModelDirectory::open(blob.data(), blob.size());
  if (!dir) { std::printf("not a directory\n"); return 1; }
  const std::uint32_t id = std::strtoul(argv[3], nullptr, 0);
  const int frames = argc >= 5 ? std::atoi(argv[4]) : 120;
  auto e = dir->entry(id);
  if (!e) { std::printf("no entry %u\n", id); return 1; }
  const std::uint8_t* fhm = blob.data() + e->offset;
  ac6::retail::ContainerIndex ix{};
  if (!ac6::retail::parse_container_index(ix, fhm, e->size, (std::uint32_t)e->offset)) return 1;

  // Gather EVERY descriptor of every NDXR in this entry into one mesh list.
  struct Piece { ac6::retail::NdxrMesh mesh; };
  std::vector<Piece> pieces;
  for (std::uint32_t j = 0; j < ix.count; ++j) {
    std::uint32_t at = ac6::retail::container_entry(ix, fhm, e->size, j);
    if (!at) continue;
    std::size_t off = at - (std::uint32_t)e->offset;
    if (off + 8 > e->size) continue;
    const std::uint8_t* sub = fhm + off;
    if (sub[0]!='N'||sub[1]!='D'||sub[2]!='X'||sub[3]!='R') continue;
    std::uint32_t len = ac6::retail::container_entry_length(ix, fhm, e->size, j);
    auto c = ac6::retail::NdxrContainer::Open(sub, len);
    if (!c) continue;
    for (std::uint16_t r = 0; r < c->record_count(); ++r) {
      auto rec = c->Record(r); if (!rec) continue;
      for (std::uint16_t k = 0; k < rec->descriptor_count; ++k) {
        auto d = c->Descriptor(*rec, k); if (!d) continue;
        auto mesh = ac6::retail::decode_ndxr_descriptor(*c, sub, len, *d);
        if (mesh) pieces.push_back({std::move(*mesh)});
      }
    }
  }
  if (pieces.empty()) { std::printf("model %u: nothing decoded\n", id); return 1; }

  // One combined mesh so the framing sees the whole model.
  ac6::retail::NdxrMesh all;
  for (const Piece& p : pieces) {
    const std::uint16_t base = (std::uint16_t)all.positions.size();
    if (all.positions.size() + p.mesh.positions.size() > 60000) break;
    for (const auto& v : p.mesh.positions) all.positions.push_back(v);
    all.indices.push_back(ac6::retail::kStripRestart);
    for (std::uint16_t i2 : p.mesh.indices)
      all.indices.push_back(i2 == ac6::retail::kStripRestart ? i2 : (std::uint16_t)(base + i2));
  }
  all.bounds = pieces[0].mesh.bounds;
  for (const Piece& p : pieces) {
    all.bounds.min_x = std::fmin(all.bounds.min_x, p.mesh.bounds.min_x);
    all.bounds.min_y = std::fmin(all.bounds.min_y, p.mesh.bounds.min_y);
    all.bounds.min_z = std::fmin(all.bounds.min_z, p.mesh.bounds.min_z);
    all.bounds.max_x = std::fmax(all.bounds.max_x, p.mesh.bounds.max_x);
    all.bounds.max_y = std::fmax(all.bounds.max_y, p.mesh.bounds.max_y);
    all.bounds.max_z = std::fmax(all.bounds.max_z, p.mesh.bounds.max_z);
  }
  const float rx = all.bounds.max_x - all.bounds.min_x;
  const float ry = all.bounds.max_y - all.bounds.min_y;
  const float rz = all.bounds.max_z - all.bounds.min_z;
  const float radius = std::fmax(rx, std::fmax(ry, rz));
  std::printf("model %u: %zu pieces, %zu vertices, %zu indices, extent %.2f x %.2f x %.2f\n",
              id, pieces.size(), all.positions.size(), all.indices.size(), rx, ry, rz);

  char mpath[512];
  std::snprintf(mpath, sizeof(mpath), "%s/metrics.json", argv[2]);
  std::FILE* metrics = std::fopen(mpath, "w");
  if (metrics) {
    std::fprintf(metrics, "{\n  \"schema\": \"ac6.ndxr-model-capture.v1\",\n");
    std::fprintf(metrics, "  \"model_id\": %u,\n  \"pieces\": %zu,\n"
                          "  \"vertices\": %zu,\n  \"indices\": %zu,\n",
                 id, pieces.size(), all.positions.size(), all.indices.size());
    std::fprintf(metrics, "  \"extent\": [%.4f, %.4f, %.4f],\n", rx, ry, rz);
    std::fprintf(metrics, "  \"note\": \"color_hash is the renderer's FNV-1a over "
                          "0xFFRRGGBB, written from the framebuffer so that checking a "
                          "PNG against it checks the pnmtopng conversion too\",\n"
                          "  \"frames\": {\n");
  }
  bool first_metric = true;
  for (int frame = 0; frame < frames; ++frame) {
    ac6::demo::Image image; image.width=480; image.height=270;
    image.rgb.assign(std::size_t(480)*270*3, 0);
    ac6::retail::RetailBasis basis = ac6::retail::identity_basis();
    const float turn = 6.2831853F * float(frame) / float(frames);
    ac6::retail::rotate_820A9B30(basis, turn);
    ac6::retail::rotate_820A99F8(basis, 0.45F);
    ac6::demo::draw_mesh_wireframe(image, all, basis, ac6::demo::DemoCamera{}, radius * 1.6F);
    char path[512];
    std::snprintf(path, sizeof(path), "%s/model-%05d.ppm", argv[2], frame);
    image.write_ppm(path);
    if (metrics) {
      std::uint64_t digest = 1469598103934665603ULL;
      for (std::size_t q = 0; q + 2 < image.rgb.size(); q += 3) {
        const std::uint32_t colour = 0xFF000000u |
            (std::uint32_t(image.rgb[q]) << 16) |
            (std::uint32_t(image.rgb[q+1]) << 8) | std::uint32_t(image.rgb[q+2]);
        digest = (digest ^ colour) * 1099511628211ULL;
      }
      std::fprintf(metrics, "%s    \"model-%05d\": {\"color_hash\": %llu}",
                   first_metric ? "" : ",\n", frame,
                   (unsigned long long)digest);
      first_metric = false;
    }
  }
  if (metrics) { std::fprintf(metrics, "\n  }\n}\n"); std::fclose(metrics); }
  return 0;
}
