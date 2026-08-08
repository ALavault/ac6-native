// Decode NTXR wrappers to PPM for visual inspection.
//
// This exists so the decoder's output can be LOOKED at, which the corpus test
// cannot do: every assertion there is a count or a hash, and a decoder that
// emitted plausible noise would satisfy all of them.
//
// What comes out is retail art. It stays local: never committed, never
// redistributed, and never offered as visual parity - the decoder's claims are
// the measured ones in analysis/contracts/mission01-visible-gate-v4.json.
//
// usage: ac6-ntxr-extract CORPUS_DIR OUT_DIR [--skip-runtime]
#include "ac6/ntxr_texture.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: ac6-ntxr-extract CORPUS_DIR OUT_DIR [--skip-runtime]\n");
    return 2;
  }
  const std::filesystem::path root(argv[1]);
  const std::filesystem::path out(argv[2]);
  const bool skip_runtime = argc > 3 && std::string(argv[3]) == "--skip-runtime";
  std::error_code error;
  if (!std::filesystem::exists(root, error)) return 77;
  std::filesystem::create_directories(out, error);

  std::vector<std::filesystem::path> files;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::recursive_directory_iterator(root, error)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".ntxr") continue;
    // The extraction carries every wrapper twice; one copy is enough to look at.
    if (skip_runtime && entry.path().string().find("runtime_idx") != std::string::npos) {
      continue;
    }
    files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());

  std::size_t written = 0, refused = 0;
  for (const std::filesystem::path& file : files) {
    std::ifstream input(file, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();
    const std::vector<std::uint8_t> bytes(text.begin(), text.end());

    ac6::retail::NtxrRefusal refusal{};
    const std::optional<ac6::retail::DecodedTexture> texture =
        ac6::retail::decode_ntxr_base_level(bytes.data(), bytes.size(), true, &refusal);
    if (!texture.has_value()) {
      refused += 1;
      continue;
    }
    const std::optional<ac6::retail::NtxrDescriptor> descriptor =
        ac6::retail::parse_ntxr_descriptor(bytes.data(), bytes.size());
    if (!descriptor.has_value()) {
      refused += 1;
      continue;
    }
    char name[512];
    std::snprintf(name, sizeof(name), "%s/%04zu_%ux%u_fmt%02x_mip%u.ppm", out.c_str(),
                  written, texture->width, texture->height,
                  descriptor->xenos_format, descriptor->mip_count);
    std::ofstream ppm(name, std::ios::binary);
    ppm << "P6\n" << texture->width << " " << texture->height << "\n255\n";
    for (const std::uint32_t pixel : texture->pixels) {
      const char rgb[3] = {static_cast<char>(pixel & 0xFF),
                           static_cast<char>((pixel >> 8) & 0xFF),
                           static_cast<char>((pixel >> 16) & 0xFF)};
      ppm.write(rgb, 3);
    }
    written += 1;
  }
  std::printf("ntxr-extract wrote=%zu refused=%zu\n", written, refused);
  return 0;
}
