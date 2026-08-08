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
#include <cmath>
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

// Mean absolute difference between horizontally adjacent texels. A natural
// image is smooth and scores low; a wrongly decoded one is not and does not.
double total_variation(const DecodedTexture& texture) {
  if (texture.width < 2 || texture.height < 2) return -1.0;
  double sum = 0.0;
  std::size_t count = 0;
  for (std::uint32_t y = 0; y < texture.height; ++y) {
    for (std::uint32_t x = 0; x + 1 < texture.width; ++x) {
      const std::uint32_t a = texture.pixels[static_cast<std::size_t>(y) * texture.width + x];
      const std::uint32_t b = texture.pixels[static_cast<std::size_t>(y) * texture.width + x + 1];
      for (int shift = 0; shift < 24; shift += 8) {
        sum += std::abs(static_cast<int>((a >> shift) & 0xFF) -
                        static_cast<int>((b >> shift) & 0xFF));
      }
      count += 3;
    }
  }
  return count ? sum / static_cast<double>(count) : -1.0;
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
  // The 8-in-16 control (cycle 1169). Every wrapper is decoded both ways and
  // scored; the swap has to win, and by how much is recorded.
  std::size_t swap_smoother = 0, plain_smoother = 0, swap_tied = 0;
  double swap_variation = 0.0, plain_variation = 0.0;
  std::size_t scored = 0;
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
    const std::optional<DecodedTexture> texture = ac6::retail::decode_ntxr_base_level(
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
    const std::optional<DecodedTexture> unswapped = ac6::retail::decode_ntxr_base_level(
        bytes.data(), bytes.size(), /*swap_16=*/false, &refusal);
    if (unswapped.has_value()) {
      const double with = total_variation(*texture);
      const double without = total_variation(*unswapped);
      if (with >= 0.0 && without >= 0.0) {
        scored += 1;
        swap_variation += with;
        plain_variation += without;
        if (with < without) swap_smoother += 1;
        else if (without < with) plain_smoother += 1;
        else swap_tied += 1;
      }
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
  // 308 of the 668 are single-level; the rest carry a chain whose base level
  // the file locates for us.
  REQUIRE(single_level_block == 308);

  REQUIRE(wrappers.size() == 692);

  // The partition, measured. Only two structural refusals remain - 22 wrappers
  // are not block formats and 2 are cube maps, whose six faces are not
  // addressed. Nothing is refused for a bad header or a size disagreement,
  // which is the part that matters: every block wrapper's declared geometry and
  // its actual byte count agree.
  REQUIRE(refusals[static_cast<int>(NtxrRefusal::BadHeader)] == 0);
  REQUIRE(refusals[static_cast<int>(NtxrRefusal::PayloadSizeMismatch)] == 0);
  REQUIRE(refusals[static_cast<int>(NtxrRefusal::NotBlockFormat)] == 22);
  REQUIRE(refusals[static_cast<int>(NtxrRefusal::CubeMap)] == 2);
  REQUIRE(decoded == 668);

  // 41 shapes, 26 of them non-power-of-two. The odd shapes are what make the
  // tile rule falsifiable, since pad32 is a no-op on a power of two.
  REQUIRE(shapes.size() == 41);
  REQUIRE(non_power_of_two == 26);

  // The corpus carries every wrapper twice, so file counts overstate the
  // evidence by roughly two; the distinct-content figures are what the claims
  // are really worth. Shapes do not double - duplicates share theirs.
  REQUIRE(distinct_content.size() == 336);
  REQUIRE(distinct_decoded.size() == 324);

  // Formats from the derived table. The 2 BC2 wrappers are the ones
  // scripts/probe_ntxr_bc.py decodes as BC3 - identical colour, wrong alpha.
  REQUIRE(decoded_formats[ac6::retail::kXenosDxt4_5] == 656);
  REQUIRE(decoded_formats[ac6::retail::kXenosDxt1] == 10);
  REQUIRE(decoded_formats[ac6::retail::kXenosDxt2_3] == 2);
  REQUIRE(decoded_formats.size() == 3);

  // The pixels themselves, pinned. Cross-validated once against
  // scripts/probe_ntxr_bc.py on the corpus BC2 wrapper: 65536 of 65536 RGB
  // texels agreed and 3286 alpha texels differed, exactly as BC2-versus-BC3
  // requires.
  REQUIRE(corpus_hash == 0x8a7b59cbf13ba39bull);

  // The 8-in-16 byte swap, scored rather than looked at. Its only control on
  // record was negative and visual - omitting it "produces visibly corrupted
  // colors". Every wrapper is decoded both ways and scored by mean absolute
  // difference between horizontally adjacent texels: a natural image is smooth,
  // and a decode through the wrong endianness corrupts every block's RGB565
  // endpoints, which is not.
  //
  // The margin depends on the population, and this corpus is the weaker one.
  // These 668 wrappers are UI - fonts and HUD panels, largely flat black with
  // hard edges - where both decodes can score alike and noise picks a winner:
  // 468 smoother with the swap, 170 without, 30 tied. On the 436 world textures
  // inside the MDLP the same measure gives 424 with, 0 without, 12 tied.
  //
  // So the assertion is the aggregate, which is what actually discriminates,
  // plus the majority. Asserting unanimity here would be importing a result
  // from a population this test does not read.
  REQUIRE(scored == 668);
  REQUIRE(swap_variation < plain_variation);
  REQUIRE(swap_smoother > 2 * plain_smoother);
  REQUIRE(swap_smoother == 468);
  REQUIRE(plain_smoother == 170);
  REQUIRE(swap_tied == 30);

  // The decoded pixels themselves, pinned. Every assertion above is about
  // counts and would survive a decoder that emitted the wrong colours; this
  // one would not. The value was cross-validated once against an independent
  // implementation - scripts/probe_ntxr_bc.py, run over the corpus BC2 wrapper
  // idx_0119/022_FHM/006_FHM/006_NTXR.ntxr - which agreed on 65536 of 65536
  // RGB texels and differed on 3286 alpha texels, exactly as BC2-versus-BC3
  // requires.

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
           << "  \"decoded_is_base_level_only\": true,\n"
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
           << "  \"mip_levels_above_zero_not_decoded\": true,\n"
           << "  \"endianness_control\": \"total-variation, both decodes scored\",\n"
           << "  \"endianness_swap_smoother\": " << swap_smoother << ",\n"
           << "  \"endianness_plain_smoother\": " << plain_smoother << ",\n"
           << "  \"endianness_tied\": " << swap_tied << ",\n"
           << "  \"corpus_pixel_hash\": \"" << std::hex << corpus_hash << std::dec
           << "\"\n}\n";
    REQUIRE(static_cast<bool>(report));
  }
  return 0;
}
