// The NTXR decoder over the whole extracted wrapper corpus.
//
// The claim under test is not "an image appears". It is that the decoder's
// population boundary is exactly where cycle 1151 measured it: every
// single-level block wrapper decodes, every other wrapper is refused with a
// named cause, and the two sets partition the corpus with nothing left over.
//
// usage: ntxr-texture-tests CORPUS_DIR [REPORT_JSON]
// exit 77 means the corpus was absent; retail bytes are never committed.

#include "ac6/ntxr_texture.h"

#include "test_fixtures.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using ac6::retail::DecodedTexture;
using ac6::retail::NtxrDescriptor;
using ac6::retail::NtxrRefusal;

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const std::string text = buffer.str();
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

std::uint64_t hash_pixels(const DecodedTexture& texture) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const std::uint32_t pixel : texture.pixels) {
    hash ^= pixel;
    hash *= 1099511628211ull;
  }
  return hash;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: ntxr-texture-tests CORPUS_DIR [REPORT_JSON]\n");
    return 2;
  }
  const std::filesystem::path root(argv[1]);
  std::error_code error;
  if (!std::filesystem::exists(root, error)) return 77;

  std::vector<std::filesystem::path> wrappers;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::recursive_directory_iterator(root, error)) {
    if (entry.is_regular_file() && entry.path().extension() == ".ntxr") {
      wrappers.push_back(entry.path());
    }
  }
  if (wrappers.empty()) return 77;
  std::sort(wrappers.begin(), wrappers.end());

  std::size_t decoded = 0;
  std::map<int, std::size_t> refusals;
  std::map<std::uint8_t, std::size_t> decoded_formats;
  std::set<std::pair<std::uint32_t, std::uint32_t>> shapes;
  std::size_t non_power_of_two = 0;
  std::uint64_t corpus_hash = 1469598103934665603ull;
  std::size_t single_level_block = 0;
  // The corpus carries each wrapper twice, under idx_NNNN and runtime_idx_NNNN.
  // Counting files overstates how much independent evidence there is, so the
  // distinct-content population is counted too and asserted separately.
  std::set<std::uint64_t> distinct_content;
  std::set<std::uint64_t> distinct_decoded;

  for (const std::filesystem::path& path : wrappers) {
    const std::vector<std::uint8_t> bytes = read_file(path);
    const std::optional<NtxrDescriptor> descriptor =
        ac6::retail::parse_ntxr_descriptor(bytes.data(), bytes.size());

    // Independently of the decoder, count what the corpus declares: this is
    // the population the decode result must reproduce exactly.
    const bool is_block = descriptor.has_value() &&
                          ac6::retail::single_level_surface_bytes(*descriptor) != 0;
    if (is_block && descriptor->mip_count == 1 && !descriptor->cube_map) {
      single_level_block += 1;
    }

    std::uint64_t content = 1469598103934665603ull;
    for (const std::uint8_t byte : bytes) {
      content ^= byte;
      content *= 1099511628211ull;
    }
    distinct_content.insert(content);

    NtxrRefusal refusal = NtxrRefusal::None;
    const std::optional<DecodedTexture> texture = ac6::retail::decode_ntxr_single_level(
        bytes.data(), bytes.size(), /*swap_16=*/true, &refusal);
    if (!texture.has_value()) {
      REQUIRE(refusal != NtxrRefusal::None);
      refusals[static_cast<int>(refusal)] += 1;
      continue;
    }
    REQUIRE(refusal == NtxrRefusal::None);
    REQUIRE(descriptor.has_value());
    // Every decoded surface is fully populated and correctly sized.
    REQUIRE(texture->width == descriptor->width);
    REQUIRE(texture->height == descriptor->height);
    REQUIRE(texture->pixels.size() ==
            static_cast<std::size_t>(texture->width) * texture->height);
    decoded += 1;
    distinct_decoded.insert(content);
    decoded_formats[descriptor->xenos_format] += 1;
    const auto shape = std::make_pair<std::uint32_t, std::uint32_t>(
        descriptor->width, descriptor->height);
    if (shapes.insert(shape).second) {
      const bool w_pow2 = (shape.first & (shape.first - 1)) == 0;
      const bool h_pow2 = (shape.second & (shape.second - 1)) == 0;
      if (!w_pow2 || !h_pow2) non_power_of_two += 1;
    }
    corpus_hash ^= hash_pixels(*texture);
    corpus_hash *= 1099511628211ull;
  }

  // The partition. Cycle 1151 measured 308 single-level block wrappers over 692
  // extracted, and the decoder must accept exactly those and refuse the rest -
  // no wrapper counted twice, none unaccounted for.
  std::size_t refused_total = 0;
  for (const std::pair<const int, std::size_t>& row : refusals) refused_total += row.second;
  REQUIRE(decoded + refused_total == wrappers.size());
  REQUIRE(decoded == single_level_block);
  REQUIRE(decoded == 308);
  REQUIRE(wrappers.size() == 692);

  // Nothing is refused for a reason that would indicate a parse defect: every
  // refusal is a population boundary, not a broken header.
  REQUIRE(refusals[static_cast<int>(NtxrRefusal::BadHeader)] == 0);
  REQUIRE(refusals[static_cast<int>(NtxrRefusal::PayloadSizeMismatch)] == 0);
  // 360 carry a mip chain, 22 are not block formats, and 2 are cube maps -
  // the cube maps declare one level, so they would have slipped past a mip
  // check alone. Together with the 308 decoded these account for all 692.
  REQUIRE(refusals[static_cast<int>(NtxrRefusal::HasMipChain)] == 360);
  REQUIRE(refusals[static_cast<int>(NtxrRefusal::NotBlockFormat)] == 22);
  REQUIRE(refusals[static_cast<int>(NtxrRefusal::CubeMap)] == 2);

  // 26 of the decoded shapes are non-power-of-two, and those are the ones that
  // make the tile-padding rule falsifiable at all.
  REQUIRE(shapes.size() == 38);
  REQUIRE(non_power_of_two == 26);

  // What the counts above are really worth. 692 files hold 336 distinct
  // wrappers and the 308 decoded hold 144 distinct textures, so the file counts
  // roughly double the apparent evidence. The shape figures do not double -
  // duplicates share their shape - which is why the 26 non-power-of-two shapes,
  // and not the 308, are what make the tile rule falsifiable.
  REQUIRE(distinct_content.size() == 336);
  REQUIRE(distinct_decoded.size() == 144);

  // The formats come from the derived table, not from a guess. The two BC2
  // wrappers are the point: BC2 and BC3 share their colour half byte for byte
  // and differ only in alpha, so scripts/probe_ntxr_bc.py - which decodes
  // every wrapper as BC3 - produces correct colour and wrong alpha for exactly
  // these two, one of which is the profile the workspace validated by eye.
  REQUIRE(decoded_formats[ac6::retail::kXenosDxt4_5] == 300);
  REQUIRE(decoded_formats[ac6::retail::kXenosDxt1] == 6);
  REQUIRE(decoded_formats[ac6::retail::kXenosDxt2_3] == 2);
  REQUIRE(decoded_formats.size() == 3);

  // The decoded pixels themselves, pinned. Every assertion above is about
  // counts and would survive a decoder that emitted the wrong colours; this
  // one would not. The value was cross-validated once against an independent
  // implementation - scripts/probe_ntxr_bc.py, run over the corpus BC2 wrapper
  // idx_0119/022_FHM/006_FHM/006_NTXR.ntxr - which agreed on 65536 of 65536
  // RGB texels and differed on 3286 alpha texels, exactly as BC2-versus-BC3
  // requires.
  REQUIRE(corpus_hash == 0x949b3bb0fb7dcdfbull);

  std::printf("ntxr decoded=%zu refused=%zu shapes=%zu npo2=%zu\n", decoded,
              refused_total, shapes.size(), non_power_of_two);

  if (argc >= 3) {
    std::ofstream report(argv[2]);
    REQUIRE(static_cast<bool>(report));
    report << "{\n"
           << "  \"schema\": \"ac6.ntxr-decode.v1\",\n"
           << "  \"source\": \"extracted NTXR wrappers, retail bytes never committed\",\n"
           << "  \"wrappers\": " << wrappers.size() << ",\n"
           << "  \"decoded\": " << decoded << ",\n"
           << "  \"refused_mip_chain\": "
           << refusals[static_cast<int>(NtxrRefusal::HasMipChain)] << ",\n"
           << "  \"refused_not_block_format\": "
           << refusals[static_cast<int>(NtxrRefusal::NotBlockFormat)] << ",\n"
           << "  \"refused_cube_map\": "
           << refusals[static_cast<int>(NtxrRefusal::CubeMap)] << ",\n"
           << "  \"refused_bad_header\": "
           << refusals[static_cast<int>(NtxrRefusal::BadHeader)] << ",\n"
           << "  \"refused_payload_mismatch\": "
           << refusals[static_cast<int>(NtxrRefusal::PayloadSizeMismatch)] << ",\n"
           << "  \"decoded_bc3\": " << decoded_formats[ac6::retail::kXenosDxt4_5] << ",\n"
           << "  \"decoded_bc1\": " << decoded_formats[ac6::retail::kXenosDxt1] << ",\n"
           << "  \"decoded_bc2\": " << decoded_formats[ac6::retail::kXenosDxt2_3] << ",\n"
           << "  \"distinct_wrappers_by_content\": " << distinct_content.size() << ",\n"
           << "  \"distinct_decoded_by_content\": " << distinct_decoded.size() << ",\n"
           << "  \"distinct_shapes\": " << shapes.size() << ",\n"
           << "  \"non_power_of_two_shapes\": " << non_power_of_two << ",\n"
           << "  \"mip_chains_are_not_addressed\": true,\n"
           << "  \"endianness_control_is_visual\": true,\n"
           << "  \"corpus_pixel_hash\": \"" << std::hex << corpus_hash << std::dec
           << "\"\n}\n";
    REQUIRE(static_cast<bool>(report));
  }
  return 0;
}
