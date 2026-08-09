// The decoder, over every descriptor in Mission 01's model package.
//
// DATA DRIVEN, and it exits 77 when the package is absent, the same arrangement
// as ac6-retail-ndxr-container: the extracted corpus is retail content and is
// never committed, so a clean clone skips rather than fails.
#include "ac6/retail_container_index.h"
#include "ac6/retail_model_directory.h"
#include "ac6/retail_ndxr_geometry.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("FAIL  %s\n", w); ++failures; } }
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: tests MDLP\n"); return 77; }
  const std::filesystem::path path = argv[1];
  if (!std::filesystem::exists(path)) {
    std::fprintf(stderr, "no package at %s — skipping\n", path.string().c_str());
    return 77;
  }
  std::ifstream input(path, std::ios::binary);
  const std::vector<std::uint8_t> blob((std::istreambuf_iterator<char>(input)),
                                       std::istreambuf_iterator<char>());
  const auto directory = ac6::retail::ModelDirectory::open(blob.data(), blob.size());
  check(directory.has_value(), "the package is a model directory");
  if (!directory) return 1;

  std::size_t descriptors = 0, decoded = 0, restarts = 0;
  std::size_t positions = 0, normals = 0, texcoords = 0, off_unit = 0, off_uv = 0;
  std::size_t zero_normals = 0;
  double worst_unit = 0.0;
  std::set<std::uint32_t> strides;
  for (std::uint32_t id = 0; id < directory->count(); ++id) {
    const auto entry = directory->entry(id);
    if (!entry) continue;
    const std::uint8_t* fhm = blob.data() + entry->offset;
    ac6::retail::ContainerIndex index{};
    if (!ac6::retail::parse_container_index(index, fhm, entry->size,
                                            static_cast<std::uint32_t>(entry->offset))) {
      continue;
    }
    for (std::uint32_t j = 0; j < index.count; ++j) {
      const std::uint32_t at =
          ac6::retail::container_entry(index, fhm, entry->size, j);
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
          ++descriptors;
          strides.insert(descriptor->vertex_stride);
          const auto mesh =
              ac6::retail::decode_ndxr_descriptor(*container, sub, length, *descriptor);
          if (!mesh) continue;
          ++decoded;
          positions += mesh->positions.size();
          check(mesh->positions.size() == descriptor->vertex_count,
                "every vertex is decoded");
          check(mesh->indices.size() == descriptor->index_count,
                "and every index kept");
          check(mesh->bounds.valid, "the bounds are computed");
          check(std::isfinite(mesh->bounds.min_x) && std::isfinite(mesh->bounds.max_x),
                "and finite");
          check(mesh->bounds.min_x <= mesh->bounds.max_x, "and ordered");
          // THE CONTROL ON THE COMPONENT TYPES. Cycle 1433 read the element
          // tables for the offsets but measured the types: normals are four
          // float16 and the first three must be a UNIT vector. Bytes read as
          // the wrong type do not come out unit-length, so this fails loudly
          // for any mis-reading of the layout.
          for (const ac6::retail::NdxrPosition& n : mesh->normals) {
            ++normals;
            const double length = std::sqrt(double(n.x)*n.x + double(n.y)*n.y +
                                            double(n.z)*n.z);
            const double error = std::fabs(length - 1.0);
            if (error > worst_unit) worst_unit = error;
            if (length < 1e-6) { ++zero_normals; continue; }
            if (error > 0.01) { ++off_unit;
              if (off_unit <= 3)
                std::printf("  off-unit normal: %.6f %.6f %.6f  |n| = %.6f\n",
                            n.x, n.y, n.z, length); }
          }
          // And texture coordinates land in [0, 1]; a few models tile, so this
          // counts rather than asserts, and the count is printed.
          for (const ac6::retail::NdxrTexcoord& t : mesh->texcoords) {
            ++texcoords;
            if (t.u < -0.01f || t.u > 1.01f || t.v < -0.01f || t.v > 1.01f) ++off_uv;
          }
          for (const std::uint16_t value : mesh->indices) {
            if (value == ac6::retail::kStripRestart) { ++restarts; continue; }
            check(value < descriptor->vertex_count,
                  "an index addresses this descriptor's own vertices");
          }
        }
      }
    }
  }

  std::printf("descriptors %zu, decoded %zu, vertices %zu, restarts %zu, strides %zu\n",
              descriptors, decoded, positions, restarts, strides.size());
  std::printf("normals %zu (zero: %zu, non-unit non-zero: %zu), "
              "texcoords %zu (outside [0,1]: %zu)\n",
              normals, zero_normals, off_unit, texcoords, off_uv);
  check(normals == positions, "every vertex has a normal at these strides");
  // EVERY NORMAL IS UNIT OR EXACTLY ZERO. The zeros are a property of the data
  // -- degenerate vertices -- and are counted rather than tolerated silently.
  // Anything neither unit nor zero would mean the float16 reading is wrong.
  check(off_unit == 0, "every normal is unit length or exactly zero");
  check(zero_normals * 200 < normals, "and the zeros are a small minority");
  check(texcoords == positions, "and a texture coordinate");
  // EVERY descriptor must decode. A silent refusal would show as decoded <
  // descriptors, and the whole addressing was arbitrated on all 1227 of them.
  check(descriptors > 0, "the package yielded descriptors");
  check(decoded == descriptors, "EVERY descriptor decodes -- no silent refusals");
  // CONTROL: the restart really occurs. If it never did, the 0xFFFF branch
  // would be untested and the three failed arbitrations of cycle 1426 would
  // have had no cause.
  check(restarts > 0, "the 0xFFFF strip restart occurs in the real data");
  check(strides.size() <= 4, "the package uses few vertex formats");

  if (failures == 0) std::printf("retail_ndxr_geometry: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
