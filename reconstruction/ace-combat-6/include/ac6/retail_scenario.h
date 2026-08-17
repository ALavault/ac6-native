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

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ac6::retail {

// 0x820A7944 tests the model byte against 0xFF before doing anything with it.
inline constexpr std::uint8_t kNoModel = 0xFF;

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

// The first three words of an Obj sub-record's data block: three independent
// scalars, not a vector. See ScenarioUnitRecord::obj_scalars for the consumers.
struct ScenarioObjScalars {
  float first{};
  float second{};
  float third{};
  // The heading, at +0x10 of the same block. Cycle 1431 read the consumer:
  //
  //   0x8229B0B0  lwz   r11,388(r3)     the Obj data block, entity+0x184
  //   0x8229B0B4  lfs   f1,16(r11)      THIS
  //   0x8229B0B8  fcmpu cr6,f1,f0       against 0.0
  //   0x8229B0BC  beq   -> skip         zero means NO ROTATION, explicitly
  //   0x8229B0C0  addi  r3,r3,96        the entity's transform
  //   0x8229B0C4  bl    0x820A9B30      rotate_820A9B30 -- already contracted
  //
  // So it is a yaw applied through A3.1's own rotation kernel, and which axis
  // it turns about is settled by which of the three rotations retail passes it
  // to rather than by a guess.
  //
  // It is the MIDDLE of a triple: 0x8229ADF8 reads +0x0C, +0x10 and +0x14
  // together at 0x8229AF50..0x8229AF60. In Mission 01 the outer two are zero in
  // every one of the 434 Obj records and only 52 carry a non-zero middle, whose
  // fourteen distinct values are all clean radians -- +-pi, +-pi/2, +-pi/4,
  // +-pi/6, and degree-round angles like -80 and -50 degrees.
  float heading{};
  bool operator==(const ScenarioObjScalars&) const = default;
};

// The pair of model-directory indices an Obj record carries, and the retail
// chain that gives them meaning (cycles 1157-1171).
//
// 0x8232F380 builds, per unit, a three-word list header: +0x00 the list node's
// data block, +0x04 an 8-byte element array, +0x08 a 0x20-byte record array
// sized from byte 0 of that data block. 0x8232F198 fills one element and one
// record per Obj entry, and 0x82330158 - ObjBin::read - sets word 0 of the
// record from the entry's **child[0]**:
//
//     82330184  lwz r11,0x0(r30)   ; the child's relative data offset
//     82330198  add r11,r11,r30
//     823301a0  stw r11,0x0(r27)   ; record+0x00 = that data block
//
// It is that block, not the entry's own, that carries the model bytes.
// 0x820A7070 reads them and turns each into a resource:
//
//     820a7944  lbz r11,0x61(r28)   ; 0xFF -> no model at all, skip the block
//     820a795c  lbz r4,0x61(r28)
//     820a7964  bl  0x8228e9b8      ; -> directory entry
//     820a7968  lbz r4,0x62(r28)    ; 0xFF -> only the first
//     820a797c  bl  0x8228e9b8
//     820a79c8  stw r30,0x15c(r31)  ; the object's model resource
//
// 0x8228E9B8 indexes the container the unit manager supplies through its vtable
// slot +0x0C (0x820A85E0), whose writer 0x8228E988 sets word0 = blob,
// word1 = blob + [blob+0x0C], word2 = blob + [blob+0x10] and whose count is
// [blob+0x04] - the header layout of Mission 01's own 94-entry MDLP.
//
// Measured over Mission 01's 434 Obj records: `primary` takes 38 non-sentinel
// values (0, 2, 4 ... 74, only 19 and 43 odd) with 0xFF in 123; `secondary` is
// `primary + 1` in 281 records and is never set when `primary` is the sentinel;
// every value is below the directory's 94 entries.
//
// What the pair means was measured in cycle 1173 rather than guessed. Mission
// 01's directory holds 94 entries of which 47 carry geometry, and:
//
//     the 38 primaries    land in a geometry-bearing entry   38 of 38
//     the 38 secondaries  land in a geometry-bearing entry    0 of 38
//
// A perfect partition, in both directions. So `primary` addresses the mesh
// bundle and `secondary` the texture bundle beside it - which is why the two
// indices are consecutive and why `primary` is almost always even.
//
// That is a measurement over one mission, not a reading of the code: 0x820A7070
// performs two identical lookups and nothing in it distinguishes their roles.
// The struct therefore still carries the two bytes without acting on the
// distinction.
struct ScenarioModelBinding {
  std::uint8_t primary{kNoModel};    // data +0x61
  std::uint8_t secondary{kNoModel};  // data +0x62
  bool has_model() const noexcept { return primary != kNoModel; }
  bool has_secondary() const noexcept { return secondary != kNoModel; }
  bool operator==(const ScenarioModelBinding&) const = default;
};

// The one WeaponBin field whose consumer is qualified in both binaries.
// Retail 0x822C6700 and demo 0x82272750 select ObjBin +0x10/+0x14/+0x18 and
// dispatch on byte +0x5C of the pointed-to data. The selector is not a weapon
// id: the two builds deliberately map some values to different result codes.
struct ScenarioWeaponBinReference {
  std::size_t data{};
  std::uint8_t selector{};  // WeaponBin data +0x5C
  bool operator==(const ScenarioWeaponBinReference&) const = default;
};

// Bounded offsets retained from one 0x20-byte ObjBin record. The internal
// DurableBin layout and all WeaponBin fields except the consumer-qualified
// selector above remain unqualified. The three weapon entries map exactly to
// ObjBin children 3, 4 and 5.
struct ScenarioObjBinReferences {
  std::optional<std::size_t> data;
  std::optional<std::size_t> parameter;   // child 0
  std::optional<std::size_t> maneuvers;   // child 1
  std::optional<std::size_t> durable;     // child 2
  std::array<std::optional<ScenarioWeaponBinReference>, 3> weapons;  // children 3..5
  std::optional<std::size_t> tail;        // child 6, role unknown
  bool operator==(const ScenarioObjBinReferences&) const = default;
};

// One element of the parsed 'Obj & Unit' slot, as 0x820A7070 reads it.
struct ScenarioUnitRecord {
  std::uint32_t index{};
  std::uint8_t class_byte{};       // data +0x08, the factory's switch selector
  std::uint8_t faction_byte{};     // data +0x0D, the faction table index
  std::uint32_t object_category{}; // what 0x820A7F48 allocates for class_byte
  bool has_behaviour_set{};        // child 0, the Set -> Act -> Order program
  // The first three words of each Obj sub-record's data block: the entity's
  // initial position. Cycle 1142 read the chain end to end:
  //
  //   0x8232F198  element+0x00 = this node's data pointer, in the 8-byte array
  //               the Obj list builds beside the 0x20-byte ObjBin records
  //   0x820A7A1C  entity+0x184 = that element word, at construction
  //   0x8229AFD0  lfs +0x00/+0x04/+0x08 of [entity+0x184] onto the stack
  //   0x8229B090  stvx128 -> entity+0xA0, the staging translation row
  //   0x8229BE98  commits +0x70..+0xA0 into +0x20..+0x50, the live transform
  //
  // Cycle 1125 called these three unrelated scalars, because it followed
  // entity+0x180 and found three consumers reading them one at a time behind
  // different guard bytes - +0x00 under +0x51, +0x04 beside +0x52, +0x08
  // reloaded into a countdown. Those consumers are real. The one that reads all
  // three together goes through +0x184, the neighbouring field, which that
  // cycle never looked at. "No route reads them together" was only true of the
  // routes it had walked.
  //
  // The values are small - (-50, -6.25, 50), (0, -200, -1000) - so they are
  // relative. 0x8229AF80 tests [entity+0x188] and a bit of its +0x118 before
  // writing, so there is a parent; what it is, and how its transform enters,
  // is not established.
  std::vector<ScenarioObjScalars> obj_scalars;
  // One per Obj record, in the same order as obj_scalars.
  std::vector<ScenarioModelBinding> model_bindings;
  // One structural record per Obj, preserving only resolved payload offsets.
  std::vector<ScenarioObjBinReferences> obj_bin_references;
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
// {u16 counter_id, u16 literal, u8 operation}. The Set/Act/Order indices retain
// its authored location without implying when the order runs.
struct ScenarioFlagOrder {
  std::uint32_t unit_index{};
  std::uint32_t act_index{};
  std::uint32_t order_index{};
  std::uint16_t counter_id{};
  std::uint16_t literal{};
  std::uint8_t operation{};
  bool operator==(const ScenarioFlagOrder&) const = default;
};

// Lossless Set -> Act -> Order census. The tag is only an identifier here;
// command 30's activation consumer remains a runtime FSM boundary.
struct ScenarioOrderRecord {
  std::uint32_t unit_index{};
  std::uint32_t act_index{};
  std::uint32_t order_index{};
  std::uint8_t tag{};
  bool operator==(const ScenarioOrderRecord&) const = default;
};

// A tag-2 order's position record, read at the offsets 0x822953F0 reads it.
// This is the record the game resolves into a world position, and Mission 01
// carries 890 of them - the only world-scale coordinates in the payload.
struct ScenarioPositionRecord {
  std::uint32_t unit_index{};
  float x{};                  // +0x08
  float y{};                  // +0x0C
  float z{};                  // +0x10
  std::uint16_t flags{};      // +0x40; bit 0 sends the y through a height query
  std::uint8_t mode{};        // +0x42; 1 means the triple is anchor-relative
  std::uint8_t anchor_a{};    // +0x43, the first argument of the unit lookup
  std::uint8_t anchor_b{};    // +0x44, the second
  std::uint8_t kind{};        // +0x45, the ten-way switch of 0x82295A88
  std::uint8_t slot{};        // +0x46, 0xFF when the order names no unit slot
  bool operator==(const ScenarioPositionRecord&) const = default;
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

// The condition record consumed by a tag-7 script step.  The first child of
// the step is read as six bytes by 0x8226E158:
//   +0x00 u16 counter id, +0x02 s16 threshold,
//   +0x04 u8 comparison (0 ==, 1 <=, 2 >=), +0x05 u8 target sub-mission.
// IDs 0 and 0xFFFF are retail's explicit "no condition" sentinels and are
// represented by an empty optional in ScenarioSubMission::step_conditions.
struct ScenarioStepCondition {
  std::uint16_t counter_id{};
  std::int16_t threshold{};
  std::uint8_t comparison{};
  std::uint8_t target_sub_mission{};
  bool operator==(const ScenarioStepCondition&) const = default;
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
  // One optional condition for every step tag.  Only tag 7 has a payload;
  // non-tag-7 entries are empty.  Keeping this vector parallel to step_tags
  // prevents a condition from being detached from its retail cursor index.
  std::vector<std::optional<ScenarioStepCondition>> step_conditions;
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
  const std::vector<ScenarioOrderRecord>& orders() const noexcept {
    return orders_;
  }
  const std::vector<ScenarioArea>& areas() const noexcept { return areas_; }
  const std::vector<ScenarioPositionRecord>& positions() const noexcept {
    return positions_;
  }
  std::size_t object_records() const noexcept;

  // The u16 count of root slot 1, from which the loader sizes the mission
  // counter table at context+0x5C. Every OrderFlagBin id must fall below it.
  std::uint16_t counter_capacity() const noexcept { return counter_capacity_; }

 private:
  std::vector<ScenarioUnitRecord> units_;
  std::vector<ScenarioFaction> factions_;
  std::vector<ScenarioSubMission> sub_missions_;
  std::vector<ScenarioFlagOrder> flag_orders_;
  std::vector<ScenarioOrderRecord> orders_;
  std::vector<ScenarioArea> areas_;
  std::vector<ScenarioPositionRecord> positions_;
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

// The coordinates the native runtime is handed for a unit. Named a placeholder
// because that is what it is: retail places nothing on the load path.
ScenarioVector position_placeholder(const ScenarioUnitRecord& record);

// The unit's load-time world position, or nothing when the container does not
// give it one. Cycle 1145 replaced position_placeholder on the session path
// after measuring what that function actually returns: 169 of Mission 01's 230
// units have (0,0,0) as their first Obj triple, so for three quarters of the
// world it was not an offset with the wrong frame, it was not a position at
// all.
//
// The two candidate sources, and why only one of them is a world position:
//
//   The Obj triple reaches the transform through entity+0x184, and the only
//   consumer that reads all three together is 0x8229AF80. That function opens
//   by loading [entity+0x188] and returns 0 when it is null (0x8229AF9C ->
//   0x8229B100), so it places nothing unless the entity has a parent. When it
//   does, 0x8229AFC0 forms parent+0x60 and 0x8229B004-0x8229B04C dot the triple
//   against the parent's staging basis rows at +0x70/+0x80/+0x90, then
//   0x8229B060-0x8229B080 add the parent's staging translation at
//   +0xA0/+0xA4/+0xA8 - child_world = parent.translation + parent.basis * offset.
//
//   The constructor at 0x8229A5AC-0x8229A5B0 zeroes both +0x184 and +0x188, so
//   an entity has no parent until something assigns one. Cycle 1145 wrote that
//   nothing on the load path does. **That was wrong, and cycle 1147 corrected
//   it**: 0x820A7B2C, inside 0x820A7070 - the unit constructor this product
//   ports - assigns the parent from byte +0x18 of the Obj record, using 0xFF as
//   the sentinel for "none":
//
//       820a7b0c  lbz  r11,0x18(r27)     ; the parent's Obj index
//       820a7b10  cmplwi cr6,r11,0xff    ; 0xFF -> leave +0x188 null
//       820a7b18  subf r11,r24,r11       ; less this record's own index
//       820a7b1c  add  r11,r11,r3        ; plus 0x8226F050's base
//       820a7b24  rlwinm r11,r11,0x2,..  ; times four
//       820a7b28  lwzx r11,r11,r30       ; the entity pointer array
//       820a7b2c  stw  r11,0x188(r31)
//
//   So the mechanism is real. What makes the triple unusable as a world
//   position is not its absence but its rarity, and that is measured: **27 of
//   Mission 01's 434 Obj records name a parent and 407 carry the 0xFF
//   sentinel**, including every record belonging to a unit whose first triple
//   this function would otherwise return. For those, 0x8229AF80 places nothing,
//   and reading the triple as a world coordinate reads a frame never applied.
//
//   The tag-2 order in the unit's Set -> Act -> Order program is a world
//   position. 0x82295A88 switches on the record's +0x45 at 0x82295B6C-0x82295B74
//   over ten arms; the arms for 5, 8 and 9 do their own arithmetic and every
//   other value falls to the default arm, which calls 0x822953F0 at 0x82295BF0
//   with the record as r5. That is the resolver this product already ports.
//
// Which leaves this function honest about its coverage rather than complete:
// it answers for the units whose first tag-2 order 0x822953F0 can resolve
// without an anchor, and refuses for the rest. On Mission 01 that is 95 of 230
// - and 94 of those 95 land inside the union of the rectangles the four
// sub-missions install, which is a cross-check and not a restatement, because
// those rectangles are parsed from the sub-mission setup steps and these
// coordinates from the behaviour programs. The remaining 135 have no load-time
// position in the container and the session records them as unplaced; putting
// them at the origin would be inventing one.
std::optional<ScenarioVector> initial_world_position(const MissionScenario& scenario,
                                                     const ScenarioUnitRecord& record);

// The native entity base. Retail element index 0 becomes entity 4097 because
// the native registry rejects a unit whose id equals its owner id.
inline constexpr std::uint32_t kEntityBase = 4097;

}  // namespace ac6::retail
