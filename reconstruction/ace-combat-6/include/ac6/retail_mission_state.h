#pragma once

// The retail mission behaviours, reimplemented natively.
//
// Cycle 1096 established what 0x820A7070 does with a parsed unit record and
// cycle 1097 what 0x8226E158 does with a sub-mission script. This header
// declares those two behaviours as native code: the classification and the
// insertion into a unit table on one side, the sub-mission sequencer with its
// step index, its start timestamps and its counter condition on the other.
//
// Two deliberate divergences from retail, both stated where they occur:
//   - the unit table refuses an insertion past its 256 slots, where retail
//     would keep writing into the fields that follow it;
//   - the sequencer returns a status instead of calling the next step, so the
//     caller owns the control flow.
// Everything else, including one asymmetry in the side-code switch, is
// reproduced as retail has it rather than as it would read better.

#include "ac6/product_runtime.h"
#include "ac6/retail_scenario.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ac6::retail {

// The three side flag words the nine-way switch at 0x820A7420 produces.
inline constexpr std::uint32_t kSideFlagsFirst = 0x20000000u;
inline constexpr std::uint32_t kSideFlagsSecond = 0x40000000u;
inline constexpr std::uint32_t kSideFlagsThird = 0x80000000u;

struct UnitClassification {
  std::uint32_t category{};  // the class 0x820A7F48 allocates
  std::uint32_t flags{};     // the side flags written to object +0xD4
  bool operator==(const UnitClassification&) const = default;
};

// The classification of one record.
//
// `side_code` is the faction entry's byte +0x2C, and it only applies when the
// game reports mode 2 or 3 with a faction table present; pass nullopt for the
// other modes, where the class byte alone decides and no side flag is set.
//
// `is_local_player` resolves the one branch that reads runtime state: for side
// codes 0 and 3 the retail consumer walks a 16-entry table with the record's
// ordinal within its branch - not its record index - and yields category 2 when
// the matched row is the local player's, 1 otherwise.
std::optional<UnitClassification> classify_unit_record(
    std::uint8_t class_byte, std::optional<std::uint8_t> side_code,
    bool is_local_player) noexcept;

// The CX360UnitManager's insertion target: 256 slots from +0x04 to +0x400 with
// the count at +0x40C, as its base constructor lays them out.
class RetailUnitTable final {
 public:
  static constexpr std::size_t kSlots = 256;

  // 0x8226FEC0 stores the entry at the incremented index and bumps the count.
  // Retail performs no bounds check; this refuses instead of writing past the
  // table, which in retail would land on the two predicate pointers at +0x404
  // and +0x408 and then on the counter itself.
  bool insert(std::uint32_t entity) noexcept;

  std::size_t count() const noexcept { return count_; }
  std::optional<std::uint32_t> at(std::size_t index) const noexcept;
  void reset() noexcept;

 private:
  std::array<std::uint32_t, kSlots> slots_{};
  std::size_t count_{};
};

// One constructed object, carrying the fields 0x820A7070 writes into it.
struct RetailUnitObject {
  std::uint32_t entity{};
  std::uint32_t record_index{};   // object +0xD0
  std::uint32_t flags{};          // object +0xD4
  std::uint32_t object_count{};   // object +0xDC
  std::uint32_t category{};
  std::uint8_t faction_byte{};
  bool operator==(const RetailUnitObject&) const = default;
};

struct RetailUnitBuild {
  std::vector<RetailUnitObject> objects;
  RetailUnitTable table;
  std::vector<std::uint32_t> faction_census;  // context+0x58, field +0x24
};

// Which record the local-player branch matches. Retail keeps two independent
// running ordinals - one for side code 0, one for side code 3 - and compares
// the ordinal, not the record index, against a 16-entry table. `branch` is 0
// for the side-code-0 counter and 1 for the side-code-3 counter.
struct LocalPlayerSlot {
  std::uint8_t branch{};
  std::uint32_t ordinal{};
  bool operator==(const LocalPlayerSlot&) const = default;
};

// Runs the consumer over a parsed scenario: classify, construct, insert, and
// increment the per-faction census the loader sizes from the faction table.
// Fails when a record names a faction the table does not have, or when the
// table fills up.
std::optional<RetailUnitBuild> build_units(const MissionScenario& scenario,
                                           std::optional<LocalPlayerSlot> local_player);

// The value a time field carries until it is stamped. Retail compares against
// this exact constant, so a field already stamped is never stamped again.
inline constexpr float kNever = 3.4028234663852886e+38f;  // FLT_MAX, at 0x82008120

// The mission area: an axis-aligned rectangle in the world's horizontal plane,
// stored as min and max per axis exactly as FUN_82268B28 normalises it.
struct MissionArea {
  float min_x{};
  float min_z{};
  float max_x{};
  float max_z{};
  bool operator==(const MissionArea&) const = default;
};

// FUN_82268B28: four floats become min and max per axis, whatever their order.
MissionArea normalise_area(float x0, float z0, float x1, float z1) noexcept;

// FUN_82268BA0: is this point inside? Components 0 and 2 of a position triple,
// so altitude never enters.
bool area_contains(const MissionArea& area, const ScenarioVector& point) noexcept;

// 0x8226A018: pick the kind the game mode selects - kind 1 in modes 4, 6, 7, 9
// and 0x0E, kind 0 otherwise - and refuse kind 2, which the installer skips.
// Returns nothing when the scenario has no record of the chosen kind; retail
// then falls back to a static zero rectangle, which the caller supplies.
std::optional<MissionArea> select_mission_area(const MissionScenario& scenario,
                                               bool second_kind) noexcept;

// The sub-mission sequencer of 0x8226E908 and 0x8226E158.
enum class SubMissionStatus : std::uint8_t {
  Running,   // a step is current
  Finished,  // the index reached the parsed count: the mission is over
};

// The three comparisons a tag-7 step applies to a mission counter.
enum class CounterComparison : std::uint8_t {
  Equal,        // operator 0
  AtMost,       // operator 1: counter <= threshold
  AtLeast,      // operator 2: counter >= threshold
};

struct CounterCondition {
  std::uint16_t counter_id{};
  std::int16_t threshold{};
  CounterComparison comparison{};
  std::uint8_t target_sub_mission{};
};

// One entry of the mission counter table the loader sizes from root slot 1:
// 0x14 bytes, of which three fields are established.
struct MissionCounter {
  std::int32_t value{};                  // +0x00, what a tag-7 condition compares
  float reached_one_at{kNever};          // +0x04, stamped the first time value == 1
  std::uint32_t bits{};                  // +0x08, a bit set elsewhere per index
  bool operator==(const MissionCounter&) const = default;
};

// The four operations 0x82267468 dispatches on the operand's byte +0x08.
enum class CounterOperation : std::uint8_t {
  SetLiteral = 0,           // value  = literal
  AddLiteral = 1,           // value += literal
  RandomOneToLiteral = 2,   // value  = 1 + random % literal
  SumOfTwo = 3,             // value  = counters[a] + counters[b]
};

// The operand record the writer reads: a literal at +0x00, two counter ids at
// +0x04 and +0x06, and the operation at +0x08.
struct CounterOperand {
  std::int16_t literal{};
  std::uint16_t source_a{0xFFFF};
  std::uint16_t source_b{0xFFFF};
  CounterOperation operation{CounterOperation::SetLiteral};
};

class SubMissionSequencer final {
 public:
  // Snapshots the counts the loader derives from the payload: the number of
  // sub-missions and the number of mission counters.
  static SubMissionSequencer from(const MissionScenario& scenario,
                                  std::size_t counter_count);

  // 0x8226E908: bound the index, make it current, reset the step index.
  SubMissionStatus select(std::uint32_t index, float now) noexcept;

  std::uint32_t current_sub_mission() const noexcept { return sub_mission_; }
  std::uint32_t current_step() const noexcept { return step_; }
  bool advance_step() noexcept;  // false when the script has no further step

  // 0x82267008: has `threshold` elapsed since this sub-mission started?
  std::optional<bool> elapsed_at_least(float threshold, float now) const noexcept;

  // The tag-7 evaluation. Returns the sub-mission to jump to when the
  // condition holds, nullopt when it does not or when the counter is out of
  // range - retail treats ids 0 and 0xFFFF as "no condition".
  std::optional<std::uint32_t> evaluate(const CounterCondition& condition) const noexcept;

  bool set_counter(std::uint16_t id, std::int32_t value) noexcept;
  std::optional<std::int32_t> counter(std::uint16_t id) const noexcept;
  std::optional<MissionCounter> counter_entry(std::uint16_t id) const noexcept;
  std::size_t counter_capacity() const noexcept { return counters_.size(); }

  // 0x82267468. `now` is the mission clock the stamp uses; `random` is the
  // value retail draws from 0x82380798, passed in so a replay is deterministic.
  // False when the id is out of the table, when a SumOfTwo operand names a
  // counter the table does not have, or when RandomOneToLiteral is given a zero
  // literal - retail would divide by zero there.
  bool apply(std::uint16_t id, const CounterOperand& operand, float now,
             std::uint32_t random) noexcept;

 private:
  std::vector<std::uint32_t> step_counts_;
  std::vector<float> started_at_;      // context+0x2C
  std::vector<MissionCounter> counters_;  // context+0x5C, stride 0x14
  std::uint32_t sub_mission_{};
  std::uint32_t step_{};
};


// A world built from the retail payload alone - no manifest is read, and none
// can be: the only input is the parsed scenario.
struct RetailWorld {
  UnitRegistry units;
  CombatWorld combat;
  MissionObjectiveDatabase objectives;
  SubMissionSequencer sequencer;
  RetailUnitBuild build;
  std::size_t published{};
};

// Populates a world from a parsed scenario: one combat unit per record with its
// faction and the class the factory selects, one Manual objective per
// sub-mission, and a counter table sized by root slot 1.
//
// Positions come from the record's first Obj sub-record, which carries a
// relative offset rather than a world coordinate; the field is filled because
// the runtime needs one, and the distinction is stated here rather than hidden.
//
// Cycle 1122 read 0x822953F0 and found where the payload's world coordinates
// live: not in the Obj sub-record, but in the tag-2 orders - 890 of them in
// Mission 01, spanning tens of thousands of world units. See
// retail_world_position.h.
//
// Cycle 1124 then read the Obj sub-record's own consumer, 0x820A7070, and the
// answer is sharper and less comfortable: it does not read these three floats
// at all. Of the ObjBin data block it reads exactly three bytes - +0x56, the
// factory key, and +0x61 and +0x62, two unit slots - and it hands the record
// itself to the Obj entity, whose vtable slot +0x50 (0x8228F678) files it at
// +0x180 and touches no coordinate. All 434 take that path and leave the loader
// with the identity transform and a translation of zero.
//
// Cycle 1126 separated two things cycle 1124 had run together. 0x820A7070
// builds one *unit* per record - 230, through the manager's virtual +0x10 at
// 0x820A7638, inserted by 0x8226FEC0 - and one *Obj entity* per ObjBin record -
// 434, from record word 2. It is the unit the order program steers, because at
// 0x82295C0C the tag-2 order reads +0xE4, a field only the unit carries. And of
// the unit, this function writes exactly +0x60, +0xD0, +0xD4, +0xD8, +0xDC,
// +0xE0 and +0xE4. Never a position.
//
// So this field is not "a relative offset where a world coordinate should be".
// It is a number retail does not consult on this path, filled because the
// runtime needs a position and nothing derived is available. What sets a unit's
// first position is still not found; cycle 1126 lists where it is not.
//
// Durability has no source in the payload at all and is left at 1.
std::optional<RetailWorld> build_retail_world(const MissionScenario& scenario,
                                              std::uint32_t mission_id,
                                              std::optional<LocalPlayerSlot> local_player);

}  // namespace ac6::retail
