// The NDXR container walk over the whole extracted corpus.
//
// Every assertion here is a prediction the derivation makes and the files could
// falsify. The important one is the last: it re-runs the string-table
// computation with the +0x30 bias removed and requires that RIVAL to fail. A
// control that cannot fail proves nothing, and cycles 1196 and 1203 both found
// in-bounds checks confirming wrong constants as happily as right ones.
//
// The first version of this test asked only for a NUL-terminated printable run,
// which a single stray character satisfies, and the rival passed in 78 of 537
// files. Measured properly: at a minimum length of 4 the rival is already 0 of
// 537 while the derived base is 537 of 537, and it stays that way at 8. The
// threshold below is 8, chosen with both curves in hand rather than tuned until
// the test went green.
//
// exit 77 means the corpus was absent. Retail bytes are never committed, so
// this test is skipped in a clean clone and load-bearing only where the
// extracted corpus exists.

#include "ac6/retail_ndxr_container.h"

#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace {

// The rival base and the derived base differ by exactly this, so a name that
// survives it is what separates them.
constexpr std::size_t kMinimumNameLength = 8;
constexpr std::size_t kBodyBaseBias = 0x30;

std::vector<std::uint8_t> ReadFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input),
                                   std::istreambuf_iterator<char>());
}

std::uint32_t Be32(const std::uint8_t* p) {
  return (static_cast<std::uint32_t>(p[0]) << 24) |
         (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) |
         static_cast<std::uint32_t>(p[3]);
}

// Length of the printable NUL-terminated string at `at`, or 0 when there is
// none. Capped so an unterminated run cannot walk the whole buffer.
std::size_t PrintableStringLength(const std::vector<std::uint8_t>& bytes,
                                  std::size_t at) {
  constexpr std::size_t kCap = 64;
  if (at >= bytes.size()) return 0;
  std::size_t i = at;
  while (i < bytes.size() && i - at < kCap && bytes[i] != 0) {
    if (bytes[i] < 32 || bytes[i] >= 127) return 0;
    ++i;
  }
  if (i >= bytes.size() || i == at) return 0;
  return i - at;
}

// Cycle 1207's discriminating control, re-expressed here so it lives with the
// code rather than only in a report: the texture-record stride separates from
// its rivals on whether the parameter chain that follows the array terminates.
// 0x10 is the stride of the MATE tables that bracket the material bodies, so it
// is the natural wrong guess, and it must fail.
//
// HOW MUCH THIS CONTROL CAN PROVE DEPENDS ON THE CORPUS, and cycle 1211 found
// that out the hard way. The chain begins at material + 0x20 + count*stride, so
// the rivals are separated by count*(stride difference). Every material in the
// standalone corpus has count == 1, which puts stride 0x20 only 8 bytes from
// 0x18 - and it lands on a valid chain too, 13,014 times out of 13,014. In the
// MDLP corpus 934 of 1,227 materials have count == 2 and the rival dies there.
//
// So this test asserts what THIS corpus can discriminate: 0x10 must fail, and
// 0x20 is reported but not asserted. The count census is asserted instead, so
// that if the corpus ever gains a material with count > 1 the reason this
// exemption exists is visible rather than silently obsolete.
std::uint32_t ChainLengthWithStride(const std::vector<std::uint8_t>& bytes,
                                    std::size_t material, std::uint16_t count,
                                    std::size_t stride) {
  std::size_t at = material + 0x20 + static_cast<std::size_t>(count) * stride;
  for (std::uint32_t nodes = 0; nodes < 4096; ++nodes) {
    if (at + 12 > bytes.size()) return 0;
    const std::uint32_t step = Be32(bytes.data() + at);
    if (step == 0) return nodes + 1;
    if (step > bytes.size()) return 0;
    at += step;
  }
  return 0;
}

// Cycle 1233 found the product reading UV four bytes early on every vertex of
// every mesh, and ctest passed 27 of 27 before and after because nothing here
// asserted a UV. This is that missing control.
//
// The derived layout puts TEXCOORD at +24 for stride 32 (POSITION@0, NORMAL@12,
// COLOR@20, TEXCOORD@24). A texture coordinate read from the right place is a
// finite number of modest magnitude; read from the wrong place it is four bytes
// of colour or normal reinterpreted as a float, which almost never is. The
// rivals are the two offsets the product actually used.
bool PlausibleUv(float value) {
  return std::isfinite(value) && std::fabs(value) <= 64.0f;
}

float BeFloat(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  const std::uint32_t bits = Be32(bytes.data() + at);
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

// Cycle 1243's control, and it is the strongest structural one available
// without an oracle: every descriptor's vertex extent, taken together, must TILE
// [0, [buf+0x18]) - no gap, no overlap, ending exactly on the block length.
// Measured before it was written: 534 of 537 files tile exactly, 3 are
// contiguous but eight bytes short (the same file in three copies), and ZERO
// have a gap or an overlap.
//
// It is a joint constraint on every descriptor at once, where the max-extent
// control above constrains only the furthest. It kills T8-alone and the
// constants 16, 20, 28 and 36 at 0 files each.
//
// WHAT IT CANNOT DO, stated so nobody mistakes it for more: a constant stride of
// 32 also tiles 534 files, because 12,978 of the corpus's 13,014 descriptors are
// format 0x0613 and only 36 are 0x0611. The corpus is 99.7% one format, so this
// control cannot separate the derived formula from that one constant. Same
// limitation as the texture_count == 1 exemption above, from the same cause.
struct Interval {
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
};

bool TilesExactly(std::vector<Interval> intervals, std::uint64_t block_length) {
  if (intervals.empty()) return false;
  std::sort(intervals.begin(), intervals.end(),
            [](const Interval& a, const Interval& b) { return a.begin < b.begin; });
  std::uint64_t cursor = 0;
  for (const Interval& span : intervals) {
    if (span.begin != cursor) return false;  // a gap or an overlap
    cursor = span.end;
  }
  return cursor == block_length;
}

struct Totals {
  std::uint64_t opened = 0;
  std::uint64_t refused = 0;
  std::uint64_t records = 0;
  std::uint64_t unnamed = 0;
  std::uint64_t relocated = 0;
  std::uint64_t derived_named = 0;
  std::uint64_t rival_named = 0;
  std::uint64_t materials = 0;
  std::uint64_t material_resolved = 0;
  std::uint64_t textures = 0;
  std::uint64_t texture_resolved = 0;
  std::uint64_t chain_ok = 0;
  std::uint64_t chain_rival_10 = 0;
  std::uint64_t chain_rival_20 = 0;
  std::uint64_t single_texture = 0;
  std::uint64_t descriptors = 0;
  std::uint64_t stride_known = 0;
  std::uint64_t vertex_fits = 0;
  std::uint64_t extent_exact_derived = 0;
  std::uint64_t extent_exact_t8_only = 0;
  std::uint64_t extent_exact_t18_only = 0;
  std::uint64_t uv_sampled = 0;
  std::uint64_t uv_plausible_derived = 0;
  std::uint64_t uv_plausible_rival_20 = 0;
  std::uint64_t uv_plausible_rival_16 = 0;
  std::uint64_t tiles_derived = 0;
  std::uint64_t tiles_rival_t8 = 0;
  std::uint64_t tiles_rival_16 = 0;
  std::uint64_t tiles_rival_28 = 0;
  std::uint64_t tiles_rival_36 = 0;
};

// The two terms of cycle 1217's rule, so the test can score the rivals that are
// the rule with one term dropped. Those are the ones worth beating: a constant
// or a swapped-byte reading is easy to kill, but T8 alone scores 4083 of 4338 on
// divisibility and dies only on the extent.
std::uint32_t T8Only(std::uint8_t hi, std::uint8_t lo) {
  static constexpr std::uint16_t kT8[8] = {16, 32, 48, 64, 16, 24, 20, 36};
  (void)lo;
  return (hi & 0x0Fu) < 8 ? kT8[hi & 0x0Fu] : 0;
}
std::uint32_t T18Only(std::uint8_t hi, std::uint8_t lo) {
  static constexpr std::uint16_t kT18[18] = {4,  8,  8,  12, 12, 16, 8,  16, 12,
                                             20, 16, 24, 4,  8,  8,  12, 12, 16};
  (void)hi;
  const unsigned h = static_cast<unsigned>(lo) >> 4;
  if (h == 0) return 0;
  const unsigned j = (h - 1) * 6 + (lo & 0x0Fu);
  return j < 18 ? kT18[j] : 0;
}

// Lifted out of main to stay inside the repository's per-function line budget,
// which ac6-cpp-complexity enforces at 220. Returns false on a write failure so
// the caller can fail the run rather than report success with no artefact.

bool WriteMetrics(const char* path, std::size_t file_count,
                  const Totals& totals) {
  std::FILE* out = std::fopen(path, "wb");
  if (out == nullptr) {
    std::fprintf(stderr, "cannot write %s\n", path);
    return false;
  }
  std::fprintf(out,
               "{\n"
               "  \"files\": %zu,\n"
               "  \"opened\": %llu,\n"
               "  \"refused\": %llu,\n"
               "  \"records\": %llu,\n"
               "  \"unnamed_records\": %llu,\n"
               "  \"relocation_guard_set_on_disk\": %llu,\n"
               "  \"derived_string_base_named\": %llu,\n"
               "  \"rival_string_base_named\": %llu,\n"
               "  \"minimum_name_length\": %zu,\n"
               "  \"materials\": %llu,\n"
               "  \"texture_refs\": %llu,\n"
               "  \"materials_texture_count_one\": %llu,\n"
               "  \"resolve_guard_set_on_disk_material\": %llu,\n"
               "  \"resolve_guard_set_on_disk_texture\": %llu,\n"
               "  \"parameter_chain_terminates\": %llu,\n"
               "  \"rival_stride_0x10_terminates\": %llu,\n"
               "  \"rival_stride_0x20_terminates\": %llu,\n"
               "  \"rival_stride_0x20_asserted\": false,\n"
               "  \"rival_stride_0x20_note\": \"not discriminated at "
               "texture_count == 1; see ChainLengthWithStride\",\n"
               "  \"descriptors\": %llu,\n"
               "  \"descriptors_stride_resolved\": %llu,\n"
               "  \"descriptors_vertex_extent_in_bounds\": %llu,\n"
               "  \"files_vertex_extent_exact_derived\": %llu,\n"
               "  \"files_vertex_extent_exact_rival_t8_only\": %llu,\n"
               "  \"files_vertex_extent_exact_rival_t18_only\": %llu,\n"
               "  \"uv_sampled\": %llu,\n"
               "  \"uv_plausible_at_24_derived\": %llu,\n"
               "  \"uv_plausible_at_20_rival\": %llu,\n"
               "  \"uv_plausible_at_16_rival\": %llu,\n"
               "  \"files_vertex_extents_tile_derived\": %llu,\n"
               "  \"files_tile_rival_t8\": %llu,\n"
               "  \"files_tile_rival_16\": %llu,\n"
               "  \"files_tile_rival_28\": %llu,\n"
               "  \"files_tile_rival_36\": %llu,\n"
               "  \"tiling_note\": \"no gap and no overlap in any file; a "
               "constant 32 also tiles 534, because 12,978 of 13,014 "
               "descriptors are format 0x0613 - not asserted\"\n"
               "}\n",
               file_count, static_cast<unsigned long long>(totals.opened),
               static_cast<unsigned long long>(totals.refused),
               static_cast<unsigned long long>(totals.records),
               static_cast<unsigned long long>(totals.unnamed),
               static_cast<unsigned long long>(totals.relocated),
               static_cast<unsigned long long>(totals.derived_named),
               static_cast<unsigned long long>(totals.rival_named),
               kMinimumNameLength,
               static_cast<unsigned long long>(totals.materials),
               static_cast<unsigned long long>(totals.textures),
               static_cast<unsigned long long>(totals.single_texture),
               static_cast<unsigned long long>(totals.material_resolved),
               static_cast<unsigned long long>(totals.texture_resolved),
               static_cast<unsigned long long>(totals.chain_ok),
               static_cast<unsigned long long>(totals.chain_rival_10),
               static_cast<unsigned long long>(totals.chain_rival_20),
               static_cast<unsigned long long>(totals.descriptors),
               static_cast<unsigned long long>(totals.stride_known),
               static_cast<unsigned long long>(totals.vertex_fits),
               static_cast<unsigned long long>(totals.extent_exact_derived),
               static_cast<unsigned long long>(totals.extent_exact_t8_only),
               static_cast<unsigned long long>(totals.extent_exact_t18_only),
               static_cast<unsigned long long>(totals.uv_sampled),
               static_cast<unsigned long long>(totals.uv_plausible_derived),
               static_cast<unsigned long long>(totals.uv_plausible_rival_20),
               static_cast<unsigned long long>(totals.uv_plausible_rival_16),
               static_cast<unsigned long long>(totals.tiles_derived),
               static_cast<unsigned long long>(totals.tiles_rival_t8),
               static_cast<unsigned long long>(totals.tiles_rival_16),
               static_cast<unsigned long long>(totals.tiles_rival_28),
               static_cast<unsigned long long>(totals.tiles_rival_36));
  std::fclose(out);
  return true;
}

// Lifted out of main for the 220-line per-function budget ac6-cpp-complexity
// enforces. Accumulates the descriptor totals for one record and folds the three
// competing vertex extents - the derived rule and the two rivals that are the
// rule with one term dropped.
void AccumulateDescriptors(const ac6::retail::NdxrContainer& container,
                           const ac6::retail::NdxrRecord& record,
                           std::uint64_t vertex_length, Totals* totals,
                           std::uint64_t* derived_extent,
                           std::uint64_t* t8_extent, std::uint64_t* t18_extent,
                           const std::vector<std::uint8_t>* bytes,
                           std::size_t vertex_base,
                           std::vector<Interval>* derived_spans,
                           std::vector<Interval>* t8_spans,
                           std::vector<Interval>* c16_spans,
                           std::vector<Interval>* c28_spans,
                           std::vector<Interval>* c36_spans) {
  for (std::uint16_t d = 0; d < record.descriptor_count; ++d) {
    const auto desc = container.Descriptor(record, d);
    if (!desc.has_value()) continue;
    ++totals->descriptors;
    if (desc->vertex_stride != 0) ++totals->stride_known;
    const std::uint64_t count = desc->vertex_count;
    const std::uint64_t end = desc->vertex_offset + count * desc->vertex_stride;
    if (end <= vertex_length) ++totals->vertex_fits;
    *derived_extent = std::max(*derived_extent, end);
    *t8_extent = std::max(
        *t8_extent,
        desc->vertex_offset + count * T8Only(desc->format_hi, desc->format_lo));
    // Sample this descriptor's vertices at the derived UV offset and at the two
    // the product used before cycle 1233.
    if (desc->vertex_stride == 32) {
      constexpr std::uint16_t kSampleCap = 40;
      const std::uint16_t take = std::min<std::uint16_t>(desc->vertex_count, kSampleCap);
      for (std::uint16_t k = 0; k < take; ++k) {
        const std::size_t base = vertex_base + desc->vertex_offset +
                                 static_cast<std::size_t>(k) * desc->vertex_stride;
        if (base + desc->vertex_stride > bytes->size()) break;
        ++totals->uv_sampled;
        struct { std::size_t offset; std::uint64_t* counter; } probes[] = {
            {24, &totals->uv_plausible_derived},
            {20, &totals->uv_plausible_rival_20},
            {16, &totals->uv_plausible_rival_16}};
        for (const auto& probe : probes) {
          if (PlausibleUv(BeFloat(*bytes, base + probe.offset)) &&
              PlausibleUv(BeFloat(*bytes, base + probe.offset + 4))) {
            ++*probe.counter;
          }
        }
      }
    }
    const std::uint64_t span_begin = desc->vertex_offset;
    const std::uint64_t span_count = desc->vertex_count;
    if (desc->vertex_stride != 0) {
      derived_spans->push_back(
          {span_begin, span_begin + span_count * desc->vertex_stride});
    }
    const std::uint32_t t8_stride = T8Only(desc->format_hi, desc->format_lo);
    if (t8_stride != 0) {
      t8_spans->push_back({span_begin, span_begin + span_count * t8_stride});
    }
    c16_spans->push_back({span_begin, span_begin + span_count * 16});
    c28_spans->push_back({span_begin, span_begin + span_count * 28});
    c36_spans->push_back({span_begin, span_begin + span_count * 36});

    *t18_extent = std::max(
        *t18_extent,
        desc->vertex_offset + count * T18Only(desc->format_hi, desc->format_lo));
  }
}

bool Check(bool condition, const char* what) {
  if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
  return condition;
}

// Lifted out of main for the 220-line per-function budget. Cycle 1233's control:
// the derived UV offset must dominate and both rivals must collapse. If a rival
// ever rises, the element-list reading behind it is wrong.
// Lifted out of main for the 220-line budget. Cycle 1243's tiling control: the
// derived stride must tile the vertex block for most of the corpus and each
// rival must tile none. A rival that starts tiling means the stride rule behind
// it has become indistinguishable, which is a finding, not a pass.
bool CheckTilingControl(const Totals& totals) {
  std::printf("  vertex extents TILE the block, derived    : %llu of %llu\n",
              static_cast<unsigned long long>(totals.tiles_derived),
              static_cast<unsigned long long>(totals.opened));
  std::printf("    rivals T8 / 16 / 28 / 36               : %llu / %llu / %llu / %llu\n",
              static_cast<unsigned long long>(totals.tiles_rival_t8),
              static_cast<unsigned long long>(totals.tiles_rival_16),
              static_cast<unsigned long long>(totals.tiles_rival_28),
              static_cast<unsigned long long>(totals.tiles_rival_36));
  bool ok = Check(totals.tiles_derived > totals.opened / 2,
                  "the derived stride does not tile the vertex block");
  ok = Check(totals.tiles_rival_t8 == 0, "rival T8-alone tiled") && ok;
  ok = Check(totals.tiles_rival_16 == 0, "rival stride 16 tiled") && ok;
  ok = Check(totals.tiles_rival_28 == 0, "rival stride 28 tiled") && ok;
  ok = Check(totals.tiles_rival_36 == 0, "rival stride 36 tiled") && ok;
  return ok;
}

bool CheckUvControl(const Totals& totals) {
  if (totals.uv_sampled == 0) return true;
  const double sampled = static_cast<double>(totals.uv_sampled);
  const double derived = 100.0 * static_cast<double>(totals.uv_plausible_derived) / sampled;
  const double rival20 = 100.0 * static_cast<double>(totals.uv_plausible_rival_20) / sampled;
  const double rival16 = 100.0 * static_cast<double>(totals.uv_plausible_rival_16) / sampled;
  std::printf("  UV plausible at +24 derived / +20 / +16    : %.1f%% / %.1f%% / %.1f%%\n",
              derived, rival20, rival16);
  bool ok = Check(derived > 90.0, "the derived UV offset is not plausible");
  ok = Check(rival20 < 10.0, "the +20 rival UV offset became plausible") && ok;
  ok = Check(rival16 < 10.0, "the +16 rival UV offset became plausible") && ok;
  return ok;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <corpus-root>\n", argv[0]);
    return 2;
  }

  const std::filesystem::path root(argv[1]);
  std::error_code error;
  if (!std::filesystem::exists(root, error)) {
    std::fprintf(stderr, "corpus absent at %s — skipping\n",
                 root.string().c_str());
    return 77;
  }

  std::vector<std::filesystem::path> files;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::recursive_directory_iterator(root, error)) {
    if (entry.is_regular_file() && entry.path().extension() == ".ndxr") {
      files.push_back(entry.path());
    }
  }
  if (files.empty()) {
    std::fprintf(stderr, "no .ndxr under %s — skipping\n",
                 root.string().c_str());
    return 77;
  }

  Totals totals;
  bool ok = true;

  for (const std::filesystem::path& path : files) {
    const std::vector<std::uint8_t> bytes = ReadFile(path);
    if (bytes.empty()) continue;

    ac6::retail::NdxrRefusal refusal = ac6::retail::NdxrRefusal::kNone;
    const auto container =
        ac6::retail::NdxrContainer::Open(bytes.data(), bytes.size(), &refusal);
    if (!container) {
      ++totals.refused;
      std::fprintf(stderr, "refused %s: %s\n", path.filename().string().c_str(),
                   ac6::retail::RefusalToString(refusal));
      continue;
    }
    ++totals.opened;
    // 0x82362190 gives the vertex buffer Length = [buf+0x18]. With [buf+0x1C]
    // zero in every shipped file, that is exactly end - second.
    const std::uint64_t vertex_length =
        container->sections().end - container->sections().second;
    std::uint64_t derived_extent = 0, t8_extent = 0, t18_extent = 0;
    std::vector<Interval> derived_spans, t8_spans, c16_spans, c28_spans, c36_spans;

    // Cycle 1195: every shipped file carries 0x200, which is why Open() serves
    // that code alone.
    ok = Check(container->type_code() == 0x200, "type code is not 0x200") && ok;

    for (std::uint16_t i = 0; i < container->record_count(); ++i) {
      const auto record = container->Record(i);
      if (!record.has_value()) {
        ++totals.unnamed;
        continue;
      }
      ++totals.records;
      // Cycle 1197: bit 0x8000 means "already relocated". A file on disk has
      // never been through the loader, so it must be clear; if this fires, the
      // field is not what the derivation says it is.
      if (record->relocated) ++totals.relocated;
      if (record->name.empty()) ++totals.unnamed;

      AccumulateDescriptors(*container, *record, vertex_length, &totals,
                            &derived_extent, &t8_extent, &t18_extent, &bytes,
                            container->sections().second, &derived_spans,
                            &t8_spans, &c16_spans, &c28_spans, &c36_spans);

      // Cycle 1207: the four slots are MATERIALS. Only slot 0 is ever used in
      // this corpus, so the loop covers all four and the count proves it.
      for (std::uint16_t d = 0; d < record->descriptor_count; ++d) {
        for (unsigned slot = 0; slot < 4; ++slot) {
          const auto material = container->Material(*record, d, slot);
          if (!material.has_value()) continue;
          ++totals.materials;
          if (material->texture_count == 1) ++totals.single_texture;
          if (material->resolved) ++totals.material_resolved;
          for (std::uint16_t k = 0; k < material->texture_count; ++k) {
            const auto texture = container->TextureRef(*material, k);
            if (!texture.has_value()) continue;
            ++totals.textures;
            if (texture->resolved) ++totals.texture_resolved;
          }
          if (container->ParameterChainLength(*material).has_value()) {
            ++totals.chain_ok;
          }
          if (ChainLengthWithStride(bytes, material->offset,
                                    material->texture_count, 0x10) != 0) {
            ++totals.chain_rival_10;
          }
          if (ChainLengthWithStride(bytes, material->offset,
                                    material->texture_count, 0x20) != 0) {
            ++totals.chain_rival_20;
          }
        }
      }
    }

    if (TilesExactly(derived_spans, vertex_length)) ++totals.tiles_derived;
    if (TilesExactly(t8_spans, vertex_length)) ++totals.tiles_rival_t8;
    if (TilesExactly(c16_spans, vertex_length)) ++totals.tiles_rival_16;
    if (TilesExactly(c28_spans, vertex_length)) ++totals.tiles_rival_28;
    if (TilesExactly(c36_spans, vertex_length)) ++totals.tiles_rival_36;

    if (derived_extent == vertex_length) ++totals.extent_exact_derived;
    if (t8_extent == vertex_length) ++totals.extent_exact_t8_only;
    if (t18_extent == vertex_length) ++totals.extent_exact_t18_only;

    // THE DISCRIMINATING CONTROL, cycle 1196. The body base is
    // buf + [buf+0x10] + 0x30, and dropping the 0x30 still lands inside the
    // file every time, so in-bounds cannot separate the two. What separates
    // them is what is AT the result: a string table, or binary data.
    const std::size_t name_offset =
        Be32(bytes.data() + ac6::retail::NdxrContainer::kRecordArrayBase + 0x20);
    const std::size_t derived_at = container->sections().end + name_offset;
    const std::size_t rival_at =
        container->sections().end - kBodyBaseBias + name_offset;
    if (PrintableStringLength(bytes, derived_at) >= kMinimumNameLength) {
      ++totals.derived_named;
    }
    if (PrintableStringLength(bytes, rival_at) >= kMinimumNameLength) {
      ++totals.rival_named;
    }
  }

  std::printf("ndxr-container files=%zu opened=%llu refused=%llu records=%llu\n",
              files.size(), static_cast<unsigned long long>(totals.opened),
              static_cast<unsigned long long>(totals.refused),
              static_cast<unsigned long long>(totals.records));
  std::printf("  unnamed records                    : %llu (must be 0)\n",
              static_cast<unsigned long long>(totals.unnamed));
  std::printf("  relocation guard set on disk       : %llu (must be 0)\n",
              static_cast<unsigned long long>(totals.relocated));
  std::printf("  derived base names >= %zu chars      : %llu of %llu\n",
              kMinimumNameLength,
              static_cast<unsigned long long>(totals.derived_named),
              static_cast<unsigned long long>(totals.opened));
  std::printf("  rival base (no +0x30) likewise     : %llu (must be 0)\n",
              static_cast<unsigned long long>(totals.rival_named));

  std::printf("  materials=%llu textures=%llu\n",
              static_cast<unsigned long long>(totals.materials),
              static_cast<unsigned long long>(totals.textures));
  std::printf("  material/texture resolve bits set on disk : %llu / %llu\n",
              static_cast<unsigned long long>(totals.material_resolved),
              static_cast<unsigned long long>(totals.texture_resolved));
  std::printf("  parameter chain terminates, stride 0x18   : %llu of %llu\n",
              static_cast<unsigned long long>(totals.chain_ok),
              static_cast<unsigned long long>(totals.materials));
  std::printf("  descriptors=%llu stride resolved=%llu vertex block fits=%llu\n",
              static_cast<unsigned long long>(totals.descriptors),
              static_cast<unsigned long long>(totals.stride_known),
              static_cast<unsigned long long>(totals.vertex_fits));
  std::printf("  files whose furthest vertex lands exactly on [buf+0x18]:\n");
  std::printf("    derived T8[i]+T18[j] : %llu of %llu\n",
              static_cast<unsigned long long>(totals.extent_exact_derived),
              static_cast<unsigned long long>(totals.opened));
  std::printf("    rival T8 alone       : %llu   rival T18 alone : %llu\n",
              static_cast<unsigned long long>(totals.extent_exact_t8_only),
              static_cast<unsigned long long>(totals.extent_exact_t18_only));
  std::printf("  materials with texture_count == 1         : %llu of %llu\n",
              static_cast<unsigned long long>(totals.single_texture),
              static_cast<unsigned long long>(totals.materials));
  std::printf("  same, rival stride 0x10 / 0x20            : %llu / %llu\n",
              static_cast<unsigned long long>(totals.chain_rival_10),
              static_cast<unsigned long long>(totals.chain_rival_20));

  ok = Check(totals.refused == 0, "some files were refused") && ok;
  ok = Check(totals.material_resolved == 0,
             "a material carries the 0x4000 resolve bit on disk") && ok;
  ok = Check(totals.texture_resolved == 0,
             "a texture record carries the 0x4000 resolve bit on disk") && ok;
  ok = Check(totals.chain_ok == totals.materials,
             "the parameter chain did not terminate for every material") && ok;
  // The rivals must lose. If either ever matches, stride 0x18 is no longer
  // discriminated and cycle 1207's control is void.
  ok = Check(totals.chain_rival_10 == 0, "rival stride 0x10 terminated") && ok;
  // Deliberately NOT asserted: see the note on ChainLengthWithStride. Asserting
  // it would mean weakening the test until it passed, which is the opposite of
  // what a control is for.
  ok = Check(totals.single_texture == totals.materials,
             "a material has texture_count != 1, so the 0x20 exemption above is "
             "stale and that rival should now be asserted") && ok;
  ok = Check(totals.unnamed == 0, "some records have no name") && ok;
  ok = Check(totals.relocated == 0,
             "the 0x8000 guard is set on disk in some record") && ok;
  ok = Check(totals.derived_named == totals.opened,
             "the derived base did not name every file") && ok;
  // If the rival ever passes, the +0x30 loses its only discriminator and the
  // derivation is weaker than the header claims.
  ok = Check(totals.rival_named == 0, "the rival base produced a name") && ok;
  ok = Check(totals.stride_known == totals.descriptors,
             "a descriptor's format code fell outside T8/T18") && ok;
  ok = Check(totals.vertex_fits == totals.descriptors,
             "a descriptor's vertex extent leaves [buf+0x18]") && ok;
  // Cycle 1217's control. The rivals are the derived rule with one term
  // dropped - the ones worth beating - and both must reach zero while the
  // derived rule names most of the corpus.
  ok = Check(totals.extent_exact_t8_only == 0, "rival T8-alone matched a file") && ok;
  ok = Check(totals.extent_exact_t18_only == 0, "rival T18-alone matched a file") && ok;
  ok = CheckUvControl(totals) && ok;
  ok = CheckTilingControl(totals) && ok;
  ok = Check(totals.extent_exact_derived > totals.opened / 2,
             "the derived stride did not land exactly for most files") && ok;

  if (!ok) return 1;

  if (argc >= 3 && !WriteMetrics(argv[2], files.size(), totals)) return 1;

  std::printf("ndxr-container OK\n");
  return 0;
}
