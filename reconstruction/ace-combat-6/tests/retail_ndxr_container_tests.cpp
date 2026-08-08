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
#include <cstdio>
#include <filesystem>
#include <fstream>
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

struct Totals {
  std::uint64_t opened = 0;
  std::uint64_t refused = 0;
  std::uint64_t records = 0;
  std::uint64_t unnamed = 0;
  std::uint64_t relocated = 0;
  std::uint64_t derived_named = 0;
  std::uint64_t rival_named = 0;
};

bool Check(bool condition, const char* what) {
  if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
  return condition;
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
    }

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

  ok = Check(totals.refused == 0, "some files were refused") && ok;
  ok = Check(totals.unnamed == 0, "some records have no name") && ok;
  ok = Check(totals.relocated == 0,
             "the 0x8000 guard is set on disk in some record") && ok;
  ok = Check(totals.derived_named == totals.opened,
             "the derived base did not name every file") && ok;
  // If the rival ever passes, the +0x30 loses its only discriminator and the
  // derivation is weaker than the header claims.
  ok = Check(totals.rival_named == 0, "the rival base produced a name") && ok;

  if (!ok) return 1;
  std::printf("ndxr-container OK\n");
  return 0;
}
