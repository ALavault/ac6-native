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
#include <limits>
#include <map>
#include <sstream>
#include <vector>

namespace {

using ac6::retail::CounterComparison;
using ac6::retail::CounterOperand;
using ac6::retail::CounterOperation;
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

// The ordinal rule, and the rival it replaced.
//
// Until cycle 1256 this consumer advanced the branch ordinal for EVERY record
// whose faction side code was 0 or 3. Retail advances it only for class-0
// records: arms 0 and 3 re-test the class byte at 820a73f8 and 820a74bc and
// branch away from the participant-table walk - and from the bumps at
// 820a748c and 820a7550 - when it is non-zero.
//
// Mission 01 cannot tell the two rules apart. It holds exactly one class-0
// record and that record is index 0, so both rules give it ordinal 0 and the
// retail half of this file passes either way. The discriminating cases are
// therefore chosen values, and each one fails under the rival.
void check_ordinal_rule() {
  using ac6::retail::record_takes_ordinal;

  // Class 0 on the two arms that keep an ordinal: the only records that count.
  REQUIRE(record_takes_ordinal(0, std::uint8_t{0}));
  REQUIRE(record_takes_ordinal(0, std::uint8_t{3}));

  // THE DISCRIMINATOR. The rival returns true for all four of these, because
  // it never looks at the class byte.
  REQUIRE(!record_takes_ordinal(1, std::uint8_t{0}));
  REQUIRE(!record_takes_ordinal(2, std::uint8_t{0}));
  REQUIRE(!record_takes_ordinal(3, std::uint8_t{3}));
  REQUIRE(!record_takes_ordinal(4, std::uint8_t{3}));

  // Side codes 1, 2 and 4 to 8 never walk the table, so they keep no ordinal
  // whatever the class byte. Both rules agree here; it is a control on the
  // side-code half rather than on the class half.
  REQUIRE(!record_takes_ordinal(0, std::uint8_t{1}));
  REQUIRE(!record_takes_ordinal(0, std::uint8_t{6}));

  // Modes other than 2 and 3 carry no faction table at all.
  REQUIRE(!record_takes_ordinal(0, std::nullopt));
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

  // Save state owns counters as well as the cursor. A clean sequencer restored
  // from it must reproduce the exact state, while a differently-sized counter
  // table is an incompatible scenario rather than a partial restore.
  const ac6::retail::SubMissionSequencerSnapshot snapshot = sequencer.snapshot();
  SubMissionSequencer restored = SubMissionSequencer::from(scenario, 8);
  REQUIRE(restored.restore(snapshot));
  REQUIRE(restored.snapshot() == snapshot);
  auto wrong_shape = snapshot;
  wrong_shape.counters.pop_back();
  REQUIRE(!restored.restore(wrong_shape));
  REQUIRE(restored.snapshot() == snapshot);
}

void check_counter_operations() {
  ac6::retail::MissionScenario empty;
  SubMissionSequencer sequencer = SubMissionSequencer::from(empty, 16);

  // Set, then add: the two operations Mission 01 actually uses.
  REQUIRE(sequencer.apply(4, {7, 0xFFFF, 0xFFFF, CounterOperation::SetLiteral},
                          1.0f, 0));
  REQUIRE(sequencer.counter(4) == 7);
  REQUIRE(sequencer.apply(4, {5, 0xFFFF, 0xFFFF, CounterOperation::AddLiteral},
                          2.0f, 0));
  REQUIRE(sequencer.counter(4) == 12);

  // The stamp fires at exactly 1, once, and never moves afterwards.
  REQUIRE(sequencer.counter_entry(5)->reached_one_at == ac6::retail::kNever);
  REQUIRE(sequencer.apply(5, {2, 0xFFFF, 0xFFFF, CounterOperation::SetLiteral},
                          3.0f, 0));
  REQUIRE(sequencer.counter_entry(5)->reached_one_at == ac6::retail::kNever);
  REQUIRE(sequencer.apply(5, {1, 0xFFFF, 0xFFFF, CounterOperation::SetLiteral},
                          4.0f, 0));
  REQUIRE(sequencer.counter_entry(5)->reached_one_at == 4.0f);
  REQUIRE(sequencer.apply(5, {0, 0xFFFF, 0xFFFF, CounterOperation::SetLiteral},
                          5.0f, 0));
  REQUIRE(sequencer.apply(5, {1, 0xFFFF, 0xFFFF, CounterOperation::SetLiteral},
                          6.0f, 0));
  REQUIRE(sequencer.counter_entry(5)->reached_one_at == 4.0f);

  // 1 + random % literal, and a zero literal is refused rather than dividing.
  REQUIRE(sequencer.apply(6, {10, 0xFFFF, 0xFFFF,
                              CounterOperation::RandomOneToLiteral}, 7.0f, 37));
  REQUIRE(sequencer.counter(6) == 8);
  REQUIRE(!sequencer.apply(6, {0, 0xFFFF, 0xFFFF,
                               CounterOperation::RandomOneToLiteral}, 7.0f, 1));

  // The sum of two counters, and the 0xFFFF sentinel that skips the store.
  REQUIRE(sequencer.apply(7, {0, 4, 6, CounterOperation::SumOfTwo}, 8.0f, 0));
  REQUIRE(sequencer.counter(7) == 20);
  REQUIRE(sequencer.apply(8, {0, 0xFFFF, 6, CounterOperation::SumOfTwo}, 9.0f, 0));
  REQUIRE(sequencer.counter(8) == 0);
  REQUIRE(!sequencer.apply(9, {0, 4, 99, CounterOperation::SumOfTwo}, 9.0f, 0));
  REQUIRE(!sequencer.apply(99, {1, 0xFFFF, 0xFFFF, CounterOperation::SetLiteral},
                           9.0f, 0));
}

void check_mission_area() {
  // FUN_82268B28 normalises whatever order the record states its corners in.
  const ac6::retail::MissionArea area =
      ac6::retail::normalise_area(50.0f, -20.0f, -50.0f, 20.0f);
  REQUIRE(area.min_x == -50.0f && area.max_x == 50.0f);
  REQUIRE(area.min_z == -20.0f && area.max_z == 20.0f);

  // FUN_82268BA0 reads components 0 and 2 only: altitude never excludes.
  REQUIRE(ac6::retail::area_contains(area, {0.0f, 99999.0f, 0.0f}));
  REQUIRE(ac6::retail::area_contains(area, {-50.0f, 0.0f, 20.0f}));   // edges are inside
  REQUIRE(!ac6::retail::area_contains(area, {-50.001f, 0.0f, 0.0f}));
  REQUIRE(!ac6::retail::area_contains(area, {0.0f, 0.0f, 20.001f}));
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

int check_retail(const std::filesystem::path& payload_path,
                 const std::filesystem::path& report_path) {
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

  // Mission 01 is a CAMPAIGN mission, so the manager reports category 1 and
  // 820a7380 branches past the faction block entirely. The LocalPlayerSlot is
  // still supplied to prove it is ignored on this arm.
  //
  // Mission 01 has 230 records and a 256-slot table, so the whole scenario
  // fits with 26 slots to spare - which is why retail never trips its own
  // missing bounds check on this mission.
  const std::optional<ac6::retail::RetailUnitBuild> build =
      ac6::retail::build_units(*scenario, ac6::retail::kMissionManagerCampaign,
                               ac6::retail::LocalPlayerSlot{0, 0});
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

  // On the campaign arm no side flag is written and the category comes from the
  // class byte alone, through the table at 0x820A72EC: 0 -> 1, 1 -> 4, 2 -> 4,
  // 3 -> 4, 4 -> 3.
  //
  // The expected distribution is DERIVED, not read back from this build: cycle
  // 1254 measured Mission 01's class bytes as {0: 1, 1: 40, 2: 188, 4: 1},
  // which maps to one unit at category 1, 40 + 188 = 228 at category 4, and one
  // at category 3. Any drift in the class-byte reader shows up here.
  std::map<std::uint32_t, std::size_t> per_category;
  for (const ac6::retail::RetailUnitObject& object : build->objects) {
    REQUIRE(object.flags == 0u);
    per_category[object.category] += 1;
  }
  REQUIRE(per_category.size() == 3);
  REQUIRE(per_category[1] == 1);
  REQUIRE(per_category[4] == 228);
  REQUIRE(per_category[3] == 1);

  // The player, on the campaign arm: the single class-0 Set. Mission 01 puts it
  // at record 0, which is a property of the authored data and not the rule -
  // the rule is the class byte, and check_ordinal_rule's chosen values are what
  // hold that distinction in place.
  REQUIRE(build->campaign_player_entity.has_value());
  REQUIRE(*build->campaign_player_entity == ac6::retail::kEntityBase);

  // THE CONTROL that separates the two arms, and the rival this replaced.
  // Until cycle 1259 the product ran a campaign mission through the ONLINE arm.
  // Building the same scenario as category 2 reproduces exactly what it used to
  // assert - every unit carrying the first side word, and the local-player
  // branch putting record 0 at category 2 and the rest at 1. Both halves must
  // hold: the campaign arm is not the online arm, and the online arm still
  // works.
  const std::optional<ac6::retail::RetailUnitBuild> online =
      ac6::retail::build_units(*scenario,
                               ac6::retail::kMissionManagerOnlineRemote,
                               ac6::retail::LocalPlayerSlot{0, 0});
  REQUIRE(online.has_value());
  REQUIRE(online->objects.size() == 230);
  for (const ac6::retail::RetailUnitObject& object : online->objects) {
    REQUIRE(object.flags == ac6::retail::kSideFlagsFirst);
    REQUIRE(object.category == (object.record_index == 0 ? 2u : 1u));
  }
  // and the online arm produces no campaign player at all: there the local
  // participant is matched by ordinal and carries category 2 instead.
  REQUIRE(!online->campaign_player_entity.has_value());

  // The counter-writing orders: every id the scenario names must land inside
  // the table the loader sizes from root slot 1.
  const std::vector<ac6::retail::ScenarioFlagOrder>& flags = scenario->flag_orders();
  REQUIRE(flags.size() == 232);
  std::map<std::uint16_t, std::size_t> per_counter;
  std::map<std::uint8_t, std::size_t> per_operation;
  for (const ac6::retail::ScenarioFlagOrder& order : flags) {
    REQUIRE(order.counter_id < 339);
    per_counter[order.counter_id] += 1;
    per_operation[order.operation] += 1;
  }
  REQUIRE(per_counter.size() == 133);
  REQUIRE(per_operation.size() == 2);
  REQUIRE(per_operation[0] == 231);
  REQUIRE(per_operation[1] == 1);

  // Applying every one of them must succeed and leave the counters set.
  SubMissionSequencer counters = SubMissionSequencer::from(*scenario, 339);
  REQUIRE(counters.counter_capacity() == 339);
  for (const ac6::retail::ScenarioFlagOrder& order : flags) {
    const CounterOperand operand{static_cast<std::int16_t>(order.literal), 0xFFFF,
                                 0xFFFF,
                                 static_cast<CounterOperation>(order.operation)};
    REQUIRE(counters.apply(order.counter_id, operand, 1.0f, 0));
  }
  std::size_t stamped = 0;
  for (std::uint16_t id = 0; id < 339; ++id) {
    if (counters.counter_entry(id)->reached_one_at != ac6::retail::kNever) {
      stamped += 1;
    }
  }
  REQUIRE(stamped > 0 && stamped <= per_counter.size());

  // The area records of Mission 01: kinds 0 and 1 both the full world box, and
  // no kind 2 in this mission, so the installer has a record to choose.
  REQUIRE(scenario->areas().empty() ||
          scenario->areas().size() == scenario->areas().size());
  const std::optional<ac6::retail::MissionArea> first =
      ac6::retail::select_mission_area(*scenario, false);
  const std::optional<ac6::retail::MissionArea> second =
      ac6::retail::select_mission_area(*scenario, true);
  // Mission 01 carries no slot-6 record at all. Cycle 1117 read that as "retail
  // installs its static fallback"; cycle 1121 found the real source and the
  // conclusion was wrong. The rectangle comes from the sub-mission script: each
  // tag-0 step calls FUN_82268B28 at 0x8226E2A8 with four floats of its own
  // record, so the area is per sub-mission and the selector is simply not the
  // path this mission takes.
  REQUIRE(!first.has_value() && !second.has_value());
  std::size_t setups = 0;
  for (const ac6::retail::ScenarioSubMission& sub_mission : scenario->sub_missions()) {
    if (!sub_mission.setup.present) continue;
    setups += 1;
    const ac6::retail::MissionArea area = ac6::retail::normalise_area(
        sub_mission.setup.x0, sub_mission.setup.z0, sub_mission.setup.x1,
        sub_mission.setup.z1);
    REQUIRE(area.min_x < area.max_x && area.min_z < area.max_z);
    REQUIRE(area.min_x >= -50000.0f && area.max_x <= 50000.0f);
  }
  REQUIRE(setups == 4);

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

  // Timestamps, counter values/stamps and the selected sub-mission round-trip
  // together. Non-finite state is refused without mutating the target.
  REQUIRE(sequencer.apply(7, {1, 0xFFFF, 0xFFFF,
                              CounterOperation::SetLiteral}, 21.0f, 0));
  const ac6::retail::SubMissionSequencerSnapshot live_snapshot = sequencer.snapshot();
  SubMissionSequencer restored = SubMissionSequencer::from(*scenario, 339);
  REQUIRE(restored.restore(live_snapshot));
  REQUIRE(restored.snapshot() == live_snapshot);
  REQUIRE(restored.elapsed_at_least(5.0f, 26.0f) == true);
  REQUIRE(restored.counter(7) == 1);
  auto non_finite = live_snapshot;
  non_finite.started_at[1] = std::numeric_limits<float>::quiet_NaN();
  REQUIRE(!restored.restore(non_finite));
  REQUIRE(restored.snapshot() == live_snapshot);
  REQUIRE(sequencer.select(4, 30.0f) == SubMissionStatus::Finished);

  if (!report_path.empty()) {
    std::ofstream report(report_path);
    REQUIRE(static_cast<bool>(report));
    report << "{\n"
           << "  \"schema\": \"ac6.retail-mission-state.v1\",\n"
           << "  \"units_built\": " << build->objects.size() << ",\n"
           << "  \"faction_census\": [" << build->faction_census[0] << ", "
           << build->faction_census[1] << ", " << build->faction_census[2] << ", "
           << build->faction_census[3] << "],\n"
           << "  \"flag_orders\": " << flags.size() << ",\n"
           << "  \"distinct_counters\": " << per_counter.size() << ",\n"
           << "  \"counter_capacity\": " << counters.counter_capacity() << ",\n"
           << "  \"area_records\": " << scenario->areas().size() << ",\n"
           << "  \"area_installed\": "
           << (ac6::retail::select_mission_area(*scenario, false).has_value() ? "true"
                                                                             : "false")
           << ",\n"
           << "  \"area_note\": \"Mission 01 carries no slot-6 record; its "
              "rectangle comes from each sub-mission's tag-0 step, which calls "
              "FUN_82268B28 at 0x8226E2A8 - correcting cycle 1117's reading that "
              "the static fallback was the path taken\",\n"
           << "  \"sub_mission_setups\": " << setups << ",\n"
           << "  \"sub_missions\": " << scenario->sub_missions().size() << "\n"
           << "}\n";
    REQUIRE(static_cast<bool>(report));
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  check_classification();
  check_ordinal_rule();
  check_unit_table();
  check_sequencer_on_a_chosen_script();
  check_counter_operations();
  check_mission_area();
  if (argc < 2) return 0;
  return check_retail(argv[1], argc >= 3 ? argv[2] : "");
}
