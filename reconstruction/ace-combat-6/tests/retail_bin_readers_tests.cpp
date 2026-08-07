// The native *Bin readers against the retail parsers' own writes.
//
// analysis/microexec/reader-digests.tsv reduces every committed p-code snapshot
// to one digest over its written memory. Those snapshots were produced by
// executing the retail instructions in Ghidra's emulator, so agreeing with them
// is agreeing with the machine code, not with another rewrite of the same idea.
//
// usage: retail-bin-readers-tests DIGESTS [PAYLOAD]
// exit 77 means the retail payload was absent; the digests alone prove nothing
// without it, so the whole comparison is skipped.

#include "ac6/retail_bin_readers.h"
#include "test_fixtures.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct DigestCase {
  std::string klass;
  std::size_t node{};
  std::size_t run_count{};
  std::size_t written_bytes{};
  std::uint64_t digest{};
};

std::vector<DigestCase> read_digests(const std::filesystem::path& path) {
  std::vector<DigestCase> cases;
  std::ifstream input(path);
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::istringstream fields(line);
    DigestCase entry;
    std::string node;
    std::string digest;
    if (!std::getline(fields, entry.klass, '\t') ||
        !std::getline(fields, node, '\t')) {
      return {};
    }
    std::string run_count;
    std::string written;
    if (!std::getline(fields, run_count, '\t') ||
        !std::getline(fields, written, '\t') || !std::getline(fields, digest)) {
      return {};
    }
    entry.node = std::stoul(node, nullptr, 16);
    entry.run_count = std::stoul(run_count);
    entry.written_bytes = std::stoul(written);
    entry.digest = std::stoull(digest, nullptr, 16);
    cases.push_back(std::move(entry));
  }
  return cases;
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s DIGESTS [PAYLOAD]\n", argv[0]);
    return 2;
  }
  const std::vector<DigestCase> cases = read_digests(argv[1]);
  REQUIRE(cases.size() == 138);

  if (argc < 3 || !std::filesystem::exists(argv[2])) {
    std::fprintf(stderr, "retail payload absent, digests not replayed\n");
    return 77;
  }
  const std::string raw = read_file(argv[2]);
  const std::optional<ac6::retail::ScenarioPayload> payload =
      ac6::retail::ScenarioPayload::open(
          std::vector<std::uint8_t>(raw.begin(), raw.end()));
  REQUIRE(payload.has_value());

  std::map<std::string, std::size_t> per_class;
  std::size_t total_written = 0;
  for (const DigestCase& entry : cases) {
    ac6::retail::BinImage image;
    ac6::retail::BinReaders readers(*payload, image);
    if (!readers.run(entry.klass, entry.node)) {
      std::fprintf(stderr, "%s@node+0x%zx failed: %s\n", entry.klass.c_str(),
                   entry.node, std::string(readers.failure()).c_str());
      return 1;
    }
    if (image.run_count() != entry.run_count ||
        image.written_bytes() != entry.written_bytes ||
        image.digest() != entry.digest) {
      std::fprintf(stderr,
                   "%s@node+0x%zx diverges: runs %zu vs %zu, bytes %zu vs %zu, "
                   "digest %016llx vs %016llx\n",
                   entry.klass.c_str(), entry.node, image.run_count(),
                   entry.run_count, image.written_bytes(), entry.written_bytes,
                   static_cast<unsigned long long>(image.digest()),
                   static_cast<unsigned long long>(entry.digest));
      return 1;
    }
    // Negative control, once: a single extra word must move the digest, so a
    // match cannot be an artefact of a digest that ignores content.
    if (per_class.empty()) {
      const std::uint64_t before = image.digest();
      image.write32(ac6::retail::kRecordBase + 0xF0, 0x5A5A5A5A);
      REQUIRE(image.digest() != before);
    }

    per_class[entry.klass] += 1;
    total_written += entry.written_bytes;
  }

  // The census of the family, so a silently shrinking table is visible.
  REQUIRE(per_class.size() == 10);
  REQUIRE(per_class["ObjBin"] == 25);
  REQUIRE(per_class["OrderBin"] == 32);
  REQUIRE(per_class["ManeuverBin"] == 25);
  REQUIRE(per_class["ActBin"] == 24);
  REQUIRE(per_class["SetBin"] == 24);
  REQUIRE(per_class["SubMisBin"] == 4);
  REQUIRE(per_class["ComBin"] == 1);
  REQUIRE(per_class["ComTblBin"] == 1);
  REQUIRE(per_class["SubMisTblBin"] == 1);
  REQUIRE(per_class["RadioTblBin"] == 1);
  REQUIRE(total_written > 0);

  std::printf("retail_bin_readers cases=%zu written_bytes=%zu\n", cases.size(),
              total_written);
  return 0;
}
