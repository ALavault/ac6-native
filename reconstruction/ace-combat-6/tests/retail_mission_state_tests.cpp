// The retail mission behaviours, natively.
//
// These assert what cycles 1096 and 1097 established, on values chosen here:
// the classification switch with its asymmetry, the 256-slot insertion and its
// one deliberate divergence, the per-faction census, and the sub-mission
// sequencer with its bound, its timestamps and its three comparisons.
//
// usage: retail-mission-state-tests [PAYLOAD]
// exit 77 means the retail payload was absent and only the chosen-value half
// ran; the payload is never committed.

#include "ac6/retail_mission_state.h"
#include "test_fixtures.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace {

using ac6::retail::CounterComparison;
using ac6::retail::CounterCondition;
using ac6::retail::MissionScenario;
using ac6::retail::RetailUnitTable;
using ac6::retail::ScenarioPayload;
using ac6::retail::SubMissionSequencer;
using ac6::retail::SubMissionStatus;
using ac6::retail::UnitClassification;

void check_classification() {
  // Without a side code - the modes that never reach the faction switch - the
  // class byte alone decides and no side flag is written.
  REQUIRE(ac6::retail::classify_unit_record(0, std::nullopt, false) ==
          (UnitClassification{1, 0}));
  REQUIRE(ac6::retail::classify_unit_record(2, std::nullopt, false) ==
          (UnitClassification{4, 0}));
  REQUIRE(ac6::retail::classify_unit_record(4, std::nullopt, false) ==
          (UnitClassification{3, 0}));
  REQUIRE(!ac6::retail::classify_unit_record(5, std::nullopt, false).has_value());

  // The three side words, one per group of three codes.
  const std::uint32_t first = ac6::retail::kSideFlagsFirst;
  const std::uint32_t second = ac6::retail::kSideFlagsSecond;
  const std::uint32_t third = ac6::retail::kSideFlagsThird;
  REQUIRE(ac6::retail::classify_unit_record(2, std::uint8_t{1}, false)->flags == first);
  REQUIRE(ac6::retail::classify_unit_record(2, std::uint8_t{4}, false)->flags == second);
  REQUIRE(ac6::retail::classify_unit_record(2, std::uint8_t{7}, false)->flags == third);

  // Codes 0 and 3 read the local player slot; code 6 does not, and yields 5.
  // That asymmetry is retail's, not a simplification.
  REQUIRE(ac6::retail::classify_unit_record(2, std::uint8_t{0}, false)->category == 1);
  REQUIRE(ac6::retail::classify_unit_record(2, std::uint8_t{0}, true)->category == 2);
  REQUIRE(ac6::retail::classify_unit_record(2, std::uint8_t{3}, true)->category == 2);
  REQUIRE(ac6::retail::classify_unit_record(2, std::uint8_t{6}, true)->category == 5);
  REQUIRE(ac6::retail::classify_unit_record(2, std::uint8_t{6}, false)->category == 5);
  REQUIRE(ac6::retail::classify_unit_record(2, std::uint8_t{4}, false)->category == 6);
  REQUIRE(ac6::retail::classify_unit_record(2, std::uint8_t{5}, false)->category == 4);

  // A code the nine-way switch does not cover is not a category.
  REQUIRE(!ac6::retail::classify_unit_record(2, std::uint8_t{9}, false).has_value());
}

void check_unit_table() {
  RetailUnitTable table;
  REQUIRE(table.count() == 0);
  REQUIRE(!table.at(0).has_value());
  for (std::uint32_t index = 0; index < RetailUnitTable::kSlots; ++index) {
    REQUIRE(table.insert(4097 + index));
  }
  REQUIRE(table.count() == RetailUnitTable::kSlots);
  REQUIRE(table.at(0) == 4097u);
  REQUIRE(table.at(RetailUnitTable::kSlots - 1) == 4097u + RetailUnitTable::kSlots - 1);

  // The divergence, asserted so it cannot be lost: retail would write past the
  // table here, onto the two predicate pointers and then the counter. The
  // native table refuses and leaves the count alone.
  REQUIRE(!table.insert(9999));
  REQUIRE(table.count() == RetailUnitTable::kSlots);

  table.reset();
  REQUIRE(table.count() == 0);
}

void check_sequencer_on_a_chosen_script() {
  ac6::retail::MissionScenario scenario;  // empty: the sequencer needs counts only
  SubMissionSequencer sequencer = SubMissionSequencer::from(scenario, 8);
  // With no sub-mission at all, any index is already past the end.
  REQUIRE(sequencer.select(0, 0.0f) == SubMissionStatus::Finished);
  REQUIRE(!sequencer.elapsed_at_least(1.0f, 2.0f).has_value());

  // The counters exist independently of the script.
  REQUIRE(sequencer.set_counter(3, 5));
  REQUIRE(sequencer.counter(3) == 5);
  REQUIRE(!sequencer.set_counter(8, 1));
  REQUIRE(!sequencer.counter(8).has_value());

  // Ids 0 and 0xFFFF are "no condition" before any indexing happens.
  REQUIRE(!sequencer.evaluate({0, 0, CounterComparison::Equal, 2}).has_value());
  REQUIRE(!sequencer.evaluate({0xFFFF, 0, CounterComparison::Equal, 2}).has_value());

  REQUIRE(sequencer.evaluate({3, 5, CounterComparison::Equal, 2}) == 2u);
  REQUIRE(!sequencer.evaluate({3, 4, CounterComparison::Equal, 2}).has_value());
  REQUIRE(sequencer.evaluate({3, 5, CounterComparison::AtMost, 1}) == 1u);
  REQUIRE(sequencer.evaluate({3, 9, CounterComparison::AtMost, 1}) == 1u);
  REQUIRE(!sequencer.evaluate({3, 4, CounterComparison::AtMost, 1}).has_value());
  REQUIRE(sequencer.evaluate({3, 5, CounterComparison::AtLeast, 0}) == 0u);
  REQUIRE(!sequencer.evaluate({3, 6, CounterComparison::AtLeast, 0}).has_value());
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

int check_retail(const std::filesystem::path& payload_path) {
  if (!std::filesystem::exists(payload_path)) {
    std::fprintf(stderr, "retail payload absent, chosen-value half only\n");
    return 77;
  }
  const std::string raw = read_file(payload_path);
  const std::optional<ScenarioPayload> payload = ScenarioPayload::open(
      std::vector<std::uint8_t>(raw.begin(), raw.end()));
  REQUIRE(payload.has_value());
  const std::optional<MissionScenario> scenario = MissionScenario::parse(*payload);
  REQUIRE(scenario.has_value());

  // The local player is the first record of the side-code-0 branch. Every
  // Mission 01 faction carries side code 0, so that ordinal is record 0.
  // Mission 01 has 230 records and a 256-slot table, so the whole scenario
  // fits with 26 slots to spare - which is why retail never trips its own
  // missing bounds check on this mission.
  const std::optional<ac6::retail::RetailUnitBuild> build =
      ac6::retail::build_units(*scenario, ac6::retail::LocalPlayerSlot{0, 0});
  REQUIRE(build.has_value());
  REQUIRE(build->objects.size() == 230);
  REQUIRE(build->table.count() == 230);
  REQUIRE(RetailUnitTable::kSlots - build->table.count() == 26);
  REQUIRE(build->table.at(0) == ac6::retail::kEntityBase);
  REQUIRE(build->table.at(229) == ac6::retail::kEntityBase + 229);

  // The census the loader sizes from the faction table, incremented once per
  // created unit: the static distribution, reproduced by running the consumer.
  REQUIRE(build->faction_census.size() == 4);
  REQUIRE(build->faction_census[0] == 140);
  REQUIRE(build->faction_census[1] == 42);
  REQUIRE(build->faction_census[2] == 48);
  REQUIRE(build->faction_census[3] == 0);

  // All four Mission 01 faction entries carry side code 0, so every record
  // takes the first side word and the local-player branch.
  for (const ac6::retail::RetailUnitObject& object : build->objects) {
    REQUIRE(object.flags == ac6::retail::kSideFlagsFirst);
    REQUIRE(object.category == (object.record_index == 0 ? 2u : 1u));
  }

  // The sequencer over the real script: four sub-missions, then finished.
  SubMissionSequencer sequencer = SubMissionSequencer::from(*scenario, 339);
  REQUIRE(sequencer.select(0, 10.0f) == SubMissionStatus::Running);
  REQUIRE(sequencer.current_step() == 0);
  REQUIRE(sequencer.advance_step());        // sub-mission 0 has two steps
  REQUIRE(sequencer.current_step() == 1);
  REQUIRE(!sequencer.advance_step());
  REQUIRE(sequencer.elapsed_at_least(5.0f, 16.0f) == true);
  REQUIRE(sequencer.elapsed_at_least(5.0f, 14.0f) == false);
  REQUIRE(sequencer.select(1, 20.0f) == SubMissionStatus::Running);
  REQUIRE(!sequencer.advance_step());       // sub-mission 1 has one step
  REQUIRE(sequencer.select(4, 30.0f) == SubMissionStatus::Finished);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  check_classification();
  check_unit_table();
  check_sequencer_on_a_chosen_script();
  if (argc < 2) return 0;
  return check_retail(argv[1]);
}
