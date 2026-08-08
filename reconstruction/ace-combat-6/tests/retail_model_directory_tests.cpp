// The model directory, over Mission 01's own file.
//
// The claim is not "it parses". It is that the three header words 0x8228E988
// reads describe this file, that the getter 0x8228E9B8 ports reaches an FHM
// bundle for every index, and that the model bytes the scenario carries all
// address entries this directory can serve.
//
// usage: retail-model-directory-tests MDLP [SCENARIO]
// exit 77 means the retail file was absent; it is never committed.

#include "ac6/retail_model_directory.h"
#include "ac6/retail_scenario.h"

#include "test_fixtures.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

namespace {

void write_be_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
                  std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value >> 24u);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16u);
  bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8u);
  bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

void verify_signature_bounds() {
  std::vector<std::uint8_t> bytes(28, 0);
  write_be_u32(bytes, 4, 1);
  write_be_u32(bytes, 12, 20);
  write_be_u32(bytes, 16, 0);

  write_be_u32(bytes, 20, 26);
  const auto truncated = ac6::retail::ModelDirectory::open(bytes.data(), bytes.size());
  REQUIRE(truncated.has_value());
  REQUIRE(!truncated->every_entry_starts_with("FHM "));

  write_be_u32(bytes, 20, 24);
  bytes[24] = 'F';
  bytes[25] = 'H';
  bytes[26] = 'M';
  bytes[27] = ' ';
  const auto exact = ac6::retail::ModelDirectory::open(bytes.data(), bytes.size());
  REQUIRE(exact.has_value());
  REQUIRE(exact->every_entry_starts_with("FHM "));
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const std::string text = buffer.str();
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  verify_signature_bounds();
  const std::vector<std::uint8_t> blob = read_file(argv[1]);
  if (blob.empty()) return 77;

  const std::optional<ac6::retail::ModelDirectory> directory =
      ac6::retail::ModelDirectory::open(blob.data(), blob.size());
  REQUIRE(directory.has_value());

  // The header words, as 0x8228E988 and 0x8228E9A8 read them.
  REQUIRE(directory->count() == 94);

  // Every entry resolves and every one is an FHM bundle. A wrong table offset
  // or a wrong base breaks this immediately, which is why it is the check.
  REQUIRE(directory->every_entry_starts_with("FHM "));

  // Entries are monotonic and none is empty, so the extents this port computes
  // are usable slices rather than artefacts of a malformed table.
  std::size_t previous = 0;
  for (std::uint32_t index = 0; index < directory->count(); ++index) {
    const std::optional<ac6::retail::ModelDirectoryEntry> row = directory->entry(index);
    REQUIRE(row.has_value());
    REQUIRE(row->offset >= previous);
    REQUIRE(row->size > 0);
    REQUIRE(row->offset + row->size <= blob.size());
    previous = row->offset;
  }

  // The bound check of 0x8228E9C0 - the path a 0xFF model byte takes.
  REQUIRE(!directory->entry(directory->count()).has_value());
  REQUIRE(!directory->entry(0xFFu).has_value());

  std::printf("model_directory entries=%u\n", directory->count());

  if (argc < 3) return 0;

  // The join, end to end: every model index the scenario carries must address
  // an entry this directory serves. An index the directory refuses would mean
  // the container and the directory disagree.
  const std::vector<std::uint8_t> payload = read_file(argv[2]);
  if (payload.empty()) return 0;
  std::optional<ac6::retail::ScenarioPayload> parsed =
      ac6::retail::ScenarioPayload::open(payload);
  REQUIRE(parsed.has_value());
  const std::optional<ac6::retail::MissionScenario> scenario =
      ac6::retail::MissionScenario::parse(*parsed);
  REQUIRE(scenario.has_value());

  std::set<std::uint8_t> primaries, secondaries;
  std::size_t resolved = 0;
  for (const ac6::retail::ScenarioUnitRecord& unit : scenario->units()) {
    for (const ac6::retail::ScenarioModelBinding& binding : unit.model_bindings) {
      if (binding.has_model()) {
        REQUIRE(directory->entry(binding.primary).has_value());
        primaries.insert(binding.primary);
        resolved += 1;
      }
      if (binding.has_secondary()) {
        REQUIRE(directory->entry(binding.secondary).has_value());
        secondaries.insert(binding.secondary);
      }
    }
  }
  REQUIRE(resolved == 311);
  REQUIRE(primaries.size() == 38);
  REQUIRE(secondaries.size() == 38);

  std::printf("join resolved=%zu primaries=%zu secondaries=%zu\n", resolved,
              primaries.size(), secondaries.size());
  return 0;
}
