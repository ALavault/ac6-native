// Does a material's texture_id name an NTXR in the same package?
//
// NdxrTextureRef::texture_id is "the key into registry 0x828C8100" -- a runtime
// table this product does not have. The FILE side of that key is the NTXR's own
// GIDX identifier, which ntxr_texture.h locates: 'GIDX' sits exactly 0x10 after
// 'eXt' and the identifier is at GIDX+0x08, verified 346 of 346 there.
//
// So this collects every texture_id the package's materials reference and every
// GIDX identifier its NTXR wrappers carry, and intersects them. A join that
// works is one where the ids are the identifiers.
#include "ac6/retail_container_index.h"
#include "ac6/retail_model_directory.h"
#include "ac6/retail_ndxr_container.h"
#include "ac6/ntxr_texture.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
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
// The identifier, by the relative structure ntxr_texture.h records.
bool GidxIdentifier(const std::uint8_t* bytes, std::size_t size, std::uint32_t& out) {
  for (std::size_t at = 0; at + 4 <= size; ++at) {
    if (bytes[at] != 'G' || bytes[at+1] != 'I' || bytes[at+2] != 'D' || bytes[at+3] != 'X')
      continue;
    if (at + 12 > size) return false;
    out = Be32(bytes + at + 8);
    return true;
  }
  return false;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("usage: join MDLP\n"); return 2; }
  const std::vector<std::uint8_t> blob = Read(argv[1]);
  const auto directory = ac6::retail::ModelDirectory::open(blob.data(), blob.size());
  if (!directory) { std::printf("not a directory\n"); return 1; }

  std::map<std::uint32_t, int> wanted;    // texture_id -> how many refs
  std::set<std::uint32_t> present;        // GIDX identifiers in the package
  std::size_t materials = 0, refs = 0, ntxr = 0, ntxr_without_gidx = 0;
  std::size_t ntxr_decoded = 0, ntxr_texels = 0;
  std::map<std::string,int> refusals;

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
        ++ntxr;
        std::uint32_t gid = 0;
        if (GidxIdentifier(sub, length, gid)) present.insert(gid);
        else ++ntxr_without_gidx;
        // AND DOES IT YIELD PIXELS? An id that names a wrapper the decoder
        // refuses is a join that resolves to nothing usable.
        // ARRAY 1'S LENGTH INCLUDES PADDING for an NTXR. It is the exact
        // content length for an NDXR -- 292 of 292 against the container's own
        // +0x04 at cycle 1419 -- but an NTXR carries no such field, and its
        // sub-entry runs to the next one. The decoder checks the payload size,
        // so it must be handed the computed extent and not the padded span.
        std::size_t span = length;
        if (const auto desc = ac6::retail::parse_ntxr_descriptor(sub, length)) {
          const std::size_t level = ac6::retail::single_level_surface_bytes(*desc);
          if (level != 0) {
            const std::size_t want = 0x10u + desc->data_offset + level;
            if (want <= length) span = want;
          }
        }
        ac6::retail::NtxrRefusal why{};
        const auto decoded =
            ac6::retail::decode_ntxr_base_level(sub, span, false, &why);
        if (decoded) { ++ntxr_decoded; ntxr_texels += decoded->pixels.size(); }
        else {
          static const char* kName[] = {"none","bad-header","not-block-format",
                                        "cube-map","payload-size-mismatch"};
          refusals[kName[static_cast<int>(why)]]++;
        }
        continue;
      }
      if (std::memcmp(sub, "NDXR", 4) != 0) continue;
      const auto container = ac6::retail::NdxrContainer::Open(sub, length);
      if (!container) continue;
      for (std::uint16_t r = 0; r < container->record_count(); ++r) {
        const auto record = container->Record(r);
        if (!record) continue;
        for (std::uint16_t k = 0; k < record->descriptor_count; ++k) {
          for (unsigned slot = 0; slot < 4; ++slot) {
            const auto material = container->Material(*record, k, slot);
            if (!material) continue;
            ++materials;
            for (std::uint16_t t = 0; t < material->texture_count; ++t) {
              const auto ref = container->TextureRef(*material, t);
              if (!ref) continue;
              ++refs;
              wanted[ref->texture_id]++;
            }
          }
        }
      }
    }
  }

  std::size_t resolved = 0, missing = 0;
  for (const auto& [key, count] : wanted) {
    if (present.count(key)) resolved += 1; else { missing += 1; }
  }
  std::printf("materials %zu, texture refs %zu, distinct ids %zu\n",
              materials, refs, wanted.size());
  std::printf("NTXR wrappers %zu (%zu without a GIDX), distinct identifiers %zu\n",
              ntxr, ntxr_without_gidx, present.size());
  std::printf("ids that name an NTXR IN THIS PACKAGE: %zu of %zu; missing %zu\n",
              resolved, wanted.size(), missing);
  std::printf("NTXR wrappers that decode to pixels: %zu of %zu (%zu texels)\n",
              ntxr_decoded, ntxr, ntxr_texels);
  for (const auto& [why, n] : refusals) std::printf("    refused %-22s %d\n", why.c_str(), n);
  if (argc >= 3) {   // dump the wanted ids so a wider scan can intersect them
    std::FILE* out = std::fopen(argv[2], "w");
    if (out) {
      for (const auto& [key, count] : wanted) std::fprintf(out, "%u\t%d\n", key, count);
      std::fclose(out);
      std::printf("wrote %zu wanted ids to %s\n", wanted.size(), argv[2]);
    }
  }
  return 0;
}
