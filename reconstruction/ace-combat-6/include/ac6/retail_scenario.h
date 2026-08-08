#pragma once

// The retail Mission 01 scenario container, read natively.
//
// Until now the native product consumed pre-baked TSV manifests. This header
// declares the other half: the container primitive the retail parsers use, the
// three views their consumers read, and the manifest projection, so the product
// can read the retail payload itself.
//
// The primitive, derived in cycle 1084 and executed in cycles 1089-1092:
//
//     node  = { u32 data_offset; u32 table_offset }   both relative to the node
//     table = { s32 count; u32 child_offset[count] }  offsets relative to the table
//
// A child is present when the table declares it and its first two words are not
// both zero. Every offset is validated against the payload size before use: the
// payload is untrusted input and a malformed one must fail closed, never read
// out of bounds.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ac6::retail {

struct ScenarioVector {
  float x{};
  float y{};
  float z{};
  bool operator==(const ScenarioVector&) const = default;
};

class ScenarioPayload final {
 public:
  // Takes ownership of the decoded scenario root node. Fails when the buffer is
  // too small to hold a node.
  static std::optional<ScenarioPayload> open(std::vector<std::uint8_t> bytes);

  std::size_t size() const noexcept { return bytes_.size(); }

  std::optional<std::uint8_t> u8(std::size_t offset) const noexcept;
  std::optional<std::uint16_t> u16(std::size_t offset) const noexcept;
  std::optional<std::uint32_t> u32(std::size_t offset) const noexcept;
  std::optional<float> f32(std::size_t offset) const noexcept;

  // node + node[word], or nullopt when the parser treats the slot as absent.
  std::optional<std::size_t> resolve(std::size_t node, unsigned word) const noexcept;

  // The table's children, in declaration order. An absent table, a negative
  // count or any out-of-range offset yields an empty list rather than a guess.
  std::vector<std::size_t> children(std::size_t node) const;

  // The presence predicate the retail readers apply before descending.
  bool present(std::size_t node) const noexcept;

 private:
  explicit ScenarioPayload(std::vector<std::uint8_t> bytes)
      : bytes_(std::move(bytes)) {}
  std::vector<std::uint8_t> bytes_;
};

// One element of the parsed 'Obj & Unit' slot, as 0x820A7070 reads it.
struct ScenarioUnitRecord {
  std::uint32_t index{};
  std::uint8_t class_byte{};       // data +0x08, the factory's switch selector
  std::uint8_t faction_byte{};     // data +0x0D, the faction table index
  std::uint32_t object_category{}; // what 0x820A7F48 allocates for class_byte
  bool has_behaviour_set{};        // child 0, the Set -> Act -> Order program
  std::vector<ScenarioVector> objects;  // the Obj sub-records' three floats
  bool operator==(const ScenarioUnitRecord&) const = default;
};

// One entry of root slot 5, the table the loader sizes context+0x58 from.
struct ScenarioFaction {
  std::uint32_t index{};
  std::uint8_t side_code{};    // data +0x2C, the nine-way flag selector
  std::uint32_t word_0x28{};
  bool operator==(const ScenarioFaction&) const = default;
};

// One OrderFlagBin order - tag 6 of the per-unit order program, and the only
// thing in the scenario that writes a mission counter. Its payload is
// {u16 counter_id, u16 literal, u8 operation}.
struct ScenarioFlagOrder {
  std::uint32_t unit_index{};
  std::uint16_t counter_id{};
  std::uint16_t literal{};
  std::uint8_t operation{};
  bool operator==(const ScenarioFlagOrder&) const = default;
};

// One entry of root slot 6: an area record keyed by the byte at +0xA6, whose
// four floats 0x82268B28 normalises into the mission rectangle.
struct ScenarioArea {
  std::uint8_t kind{};
  float x0{};
  float z0{};
  float x1{};
  float z1{};
  bool operator==(const ScenarioArea&) const = default;
};

// The record a tag-0 step points at, read at the offsets 0x8226E158 reads it.
// Only the fields with a consumer in that function are taken; the rest of the
// block is left alone rather than guessed at.
struct ScenarioSubMissionSetup {
  bool present{};
  // The four floats handed to FUN_82268B28 at 0x8226E2A8, in its argument
  // order: pfVar3[0], pfVar3[2], pfVar3[1], pfVar3[3].
  float x0{};
  float z0{};
  float x1{};
  float z1{};
  float time_limit{};             // pfVar3[9], the value FUN_822562B0 receives
  std::uint32_t flags{};          // (uint)pfVar3[0xC]; bits 0,1,2,4,5,6,9,10 are read
  std::uint8_t completion_code{}; // the byte at +0x40, compared against 1 and 2
  bool operator==(const ScenarioSubMissionSetup&) const = default;
};

// One entry of root slot 2, with the script 0x8226E158 steps through.
struct ScenarioSubMission {
  std::uint32_t index{};
  std::vector<std::uint8_t> step_tags;
  // The byte 0x82267370 loads to bound the step cursor - the head of the step
  // list's own data block, not the table's declared child count. The two are
  // parsed independently on purpose: a payload where they disagree would tell
  // us the model is wrong, and the runner uses this one because retail does.
  std::uint8_t step_count_byte{};
  // The setup of this sub-mission's first tag-0 step, if it has one.
  ScenarioSubMissionSetup setup;
  bool operator==(const ScenarioSubMission&) const = default;
};

class MissionScenario final {
 public:
  // Reads the three views from the scenario root node at offset 0. Fails when
  // the root does not have the ten slots the dispatch table declares.
  static std::optional<MissionScenario> parse(const ScenarioPayload& payload);

  const std::vector<ScenarioUnitRecord>& units() const noexcept { return units_; }
  const std::vector<ScenarioFaction>& factions() const noexcept { return factions_; }
  const std::vector<ScenarioSubMission>& sub_missions() const noexcept {
    return sub_missions_;
  }
  const std::vector<ScenarioFlagOrder>& flag_orders() const noexcept {
    return flag_orders_;
  }
  const std::vector<ScenarioArea>& areas() const noexcept { return areas_; }
  std::size_t object_records() const noexcept;

  // The u16 count of root slot 1, from which the loader sizes the mission
  // counter table at context+0x5C. Every OrderFlagBin id must fall below it.
  std::uint16_t counter_capacity() const noexcept { return counter_capacity_; }

 private:
  std::vector<ScenarioUnitRecord> units_;
  std::vector<ScenarioFaction> factions_;
  std::vector<ScenarioSubMission> sub_missions_;
  std::vector<ScenarioFlagOrder> flag_orders_;
  std::vector<ScenarioArea> areas_;
  std::uint16_t counter_capacity_{};
};

// The class 0x820A7F48 allocates for a record's class byte, or nullopt for a
// byte outside the switch the retail consumer implements.
std::optional<std::uint32_t> object_category(std::uint8_t class_byte) noexcept;

// The manifest rows, tab separated and newline terminated, in the order the
// retail consumer iterates the records. These reproduce the rows of
// tools/emit_mission01_retail_manifests.py exactly; the '#' provenance header
// that generator writes is not duplicated here.
std::string waves_manifest_rows(const MissionScenario& scenario,
                                std::uint32_t mission_id);
std::string objectives_manifest_rows(const MissionScenario& scenario,
                                     std::uint32_t mission_id);

// The native entity base. Retail element index 0 becomes entity 4097 because
// the native registry rejects a unit whose id equals its owner id.
inline constexpr std::uint32_t kEntityBase = 4097;

}  // namespace ac6::retail
