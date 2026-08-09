// End to end, through the PORTS and nothing else.
//
// Cycle 1418's version scanned the file for the NDXR magic. Cycle 1419's walked
// the FHM tables by hand. This one calls the product: ModelDirectory for the
// MDLP level, ContainerIndex for the FHM level, and NdxrContainer::Open on the
// span they produce -- with the length taken from array 1 rather than computed
// by subtracting offsets.
//
//   g++ -std=c++20 -O2 -I reconstruction/ace-combat-6/include \
//       tools/ndxr_open_probe.cpp \
//       reconstruction/ace-combat-6/src/retail_ndxr_container.cpp \
//       reconstruction/ace-combat-6/src/retail_model_directory.cpp \
//       reconstruction/ace-combat-6/src/retail_container_index.cpp -o probe
#include "ac6/retail_container_index.h"
#include "ac6/retail_model_directory.h"
#include "ac6/retail_ndxr_container.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("usage: probe MDLP\n"); return 2; }
  std::ifstream file(argv[1], std::ios::binary);
  std::vector<std::uint8_t> blob((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
  if (blob.empty()) { std::printf("cannot read %s\n", argv[1]); return 1; }

  auto directory = ac6::retail::ModelDirectory::open(blob.data(), blob.size());
  if (!directory) { std::printf("not a model directory\n"); return 1; }
  std::printf("directory: %u entries, all FHM = %s\n", directory->count(),
              directory->every_entry_starts_with("FHM ") ? "yes" : "no");

  int ndxr = 0, opened = 0, length_agrees = 0;
  std::map<std::string, int> refusals;
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
      if (std::string(reinterpret_cast<const char*>(sub), 4) != "NDXR") continue;
      ++ndxr;

      const std::uint32_t length =
          ac6::retail::container_entry_length(index, fhm, entry->size, j);
      const std::uint32_t declared = (std::uint32_t(sub[4]) << 24) |
          (std::uint32_t(sub[5]) << 16) | (std::uint32_t(sub[6]) << 8) | sub[7];
      if (length == declared) ++length_agrees;

      ac6::retail::NdxrRefusal why{};
      if (ac6::retail::NdxrContainer::Open(sub, length, &why)) ++opened;
      else refusals[ac6::retail::RefusalToString(why)]++;
    }
  }
  std::printf("NDXR reached by index: %d\n", ndxr);
  std::printf("  array1 length == the container's own +0x04: %d\n", length_agrees);
  std::printf("  opened with that length: %d\n", opened);
  for (const auto& kv : refusals)
    std::printf("  refused %-20s %d\n", kv.first.c_str(), kv.second);
  return opened == ndxr && length_agrees == ndxr ? 0 : 1;
}
