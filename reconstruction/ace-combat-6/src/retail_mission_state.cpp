#include "ac6/retail_mission_state.h"

namespace ac6::retail {
namespace {

// The side flag word for a side code, from the nine-way switch at 0x820A7420:
// codes 0 to 2
// give the first word, 3 to 5 the second, 6 to 8 the third.
std::optional<std::uint32_t> side_flags(std::uint8_t side_code) noexcept {
  switch (side_code / 3u) {
    case 0: return kSideFlagsFirst;
    case 1: return kSideFlagsSecond;
    case 2: return kSideFlagsThird;
    default: return std::nullopt;  // a code above 8 leaves the switch open
  }
}

// The category the same switch produces and hands to the factory 0x820A7F48,
// which is NOT symmetric across the
// three sides. Codes 0 and 3 walk the 16-entry table and yield 2 for the local
// player and 1 otherwise; code 6 does not walk it and yields 5. Codes 1, 4 and
// 7 yield 6; codes 2, 5 and 8 yield 4. Reproduced as retail has it.
std::optional<std::uint32_t> side_category(std::uint8_t side_code,
                                           bool is_local_player) noexcept {
  switch (side_code) {
    case 0:
    case 3: return is_local_player ? 2u : 1u;
    case 6: return 5u;
    case 1:
    case 4:
    case 7: return 6u;
    case 2:
    case 5:
    case 8: return 4u;
    default: return std::nullopt;
  }
}

}  // namespace

std::optional<UnitClassification> classify_unit_record(
    std::uint8_t class_byte, std::optional<std::uint8_t> side_code,
    bool is_local_player) noexcept {
  const std::optional<std::uint32_t> base = object_category(class_byte);
  if (!base.has_value()) return std::nullopt;
  if (!side_code.has_value()) {
    // Modes other than 2 and 3 never reach the faction switch, so the class
    // byte alone decides and no side flag is written.
    return UnitClassification{*base, 0};
  }
  const std::optional<std::uint32_t> flags = side_flags(*side_code);
  const std::optional<std::uint32_t> category =
      side_category(*side_code, is_local_player);
  if (!flags.has_value() || !category.has_value()) return std::nullopt;
  return UnitClassification{*category, *flags};
}

bool record_takes_ordinal(std::uint8_t class_byte,
                          std::optional<std::uint8_t> side_code) noexcept {
  // Only the two arms that walk the participant table keep an ordinal, and
  // each re-tests the class byte before doing so - 820a73f8/820a7404 for side
  // code 0, 820a74bc/820a74c4 for side code 3.
  if (!side_code.has_value()) return false;
  if (*side_code != 0 && *side_code != 3) return false;
  return class_byte == 0;
}

// 0x8226FEC0: store at the incremented index, then bump the count.
MissionArea normalise_area(float x0, float z0, float x1, float z1) noexcept {
  // FUN_82268B28 writes min(a,c) and max(a,c) for one axis and min(b,d),
  // max(b,d) for the other, so a record may state its corners in any order.
  MissionArea area;
  area.min_x = x0 <= x1 ? x0 : x1;
  area.max_x = x0 <= x1 ? x1 : x0;
  area.min_z = z0 <= z1 ? z0 : z1;
  area.max_z = z0 <= z1 ? z1 : z0;
  return area;
}

bool area_contains(const MissionArea& area, const ScenarioVector& point) noexcept {
  // FUN_82268BA0 compares point[0] and point[2] only.
  return !(point.x < area.min_x || area.max_x < point.x || point.z < area.min_z ||
           area.max_z < point.z);
}

std::optional<MissionArea> select_mission_area(const MissionScenario& scenario,
                                               bool second_kind) noexcept {
  // 0x8226A018 caches the last record of each kind and installs kind 1 or kind
  // 0 by mode, never kind 2.
  const std::uint8_t wanted = second_kind ? 1 : 0;
  std::optional<ScenarioArea> chosen;
  for (const ScenarioArea& area : scenario.areas()) {
    if (area.kind == wanted) chosen = area;
  }
  if (!chosen.has_value()) return std::nullopt;
  return normalise_area(chosen->x0, chosen->z0, chosen->x1, chosen->z1);
}

bool RetailUnitTable::insert(std::uint32_t entity) noexcept {
  if (count_ >= kSlots) return false;
  slots_[count_] = entity;
  count_ += 1;
  return true;
}

std::optional<std::uint32_t> RetailUnitTable::at(std::size_t index) const noexcept {
  if (index >= count_) return std::nullopt;
  return slots_[index];
}

void RetailUnitTable::reset() noexcept {
  slots_.fill(0);
  count_ = 0;
}

// The per-record loop of 0x820A7070: classify, construct, insert, count.
std::optional<RetailUnitBuild> build_units(
    const MissionScenario& scenario, std::uint32_t manager_category,
    std::optional<LocalPlayerSlot> local_player) {
  const std::vector<ScenarioFaction>& factions = scenario.factions();
  if (factions.empty()) return std::nullopt;

  RetailUnitBuild build;
  build.faction_census.assign(factions.size(), 0);
  // The two running ordinals retail keeps, one per local-player branch.
  std::array<std::uint32_t, 2> ordinals{};

  // Categories other than 2 and 3 never reach the faction switch, so the side
  // code must not be handed to the classifier for them - 820a7380 branches past
  // the whole block.
  const bool faction_table = consults_faction_table(manager_category);

  for (const ScenarioUnitRecord& record : scenario.units()) {
    if (record.faction_byte >= factions.size()) return std::nullopt;
    const ScenarioFaction& faction = factions[record.faction_byte];
    const std::optional<std::uint8_t> side_code =
        faction_table ? std::optional<std::uint8_t>(faction.side_code)
                      : std::nullopt;
    bool is_local_player = false;
    if (record_takes_ordinal(record.class_byte, side_code)) {
      const std::uint8_t branch = side_code == 0 ? 0 : 1;
      const std::uint32_t ordinal = ordinals[branch]++;
      is_local_player = local_player.has_value() &&
                        local_player->branch == branch &&
                        local_player->ordinal == ordinal;
    }
    const std::optional<UnitClassification> classification =
        classify_unit_record(record.class_byte, side_code, is_local_player);
    if (!classification.has_value()) return std::nullopt;

    RetailUnitObject object;
    object.entity = kEntityBase + record.index;
    object.record_index = record.index;
    object.flags = classification->flags;
    object.object_count = static_cast<std::uint32_t>(record.obj_scalars.size());
    object.category = classification->category;
    object.faction_byte = record.faction_byte;

    // On the campaign arm the class-0 Set is the one retail builds the player
    // from. First match wins: a second would build a second player.
    if (!faction_table && record.class_byte == 0 &&
        !build.campaign_player_entity.has_value()) {
      build.campaign_player_entity = object.entity;
    }

    if (!build.table.insert(object.entity)) return std::nullopt;
    build.objects.push_back(object);
    build.faction_census[record.faction_byte] += 1;
  }
  return build;
}

SubMissionSequencer SubMissionSequencer::from(const MissionScenario& scenario,
                                              std::size_t counter_count) {
  SubMissionSequencer sequencer;
  for (const ScenarioSubMission& sub_mission : scenario.sub_missions()) {
    sequencer.step_counts_.push_back(
        static_cast<std::uint32_t>(sub_mission.step_tags.size()));
  }
  sequencer.started_at_.assign(sequencer.step_counts_.size(), 0.0f);
  sequencer.counters_.assign(counter_count, MissionCounter{});
  return sequencer;
}

// 0x8226E908: bound the index by the parsed count, reset the step, timestamp.
SubMissionStatus SubMissionSequencer::select(std::uint32_t index, float now) noexcept {
  sub_mission_ = index;
  step_ = 0;
  if (index >= step_counts_.size()) return SubMissionStatus::Finished;
  started_at_[index] = now;
  return SubMissionStatus::Running;
}

// The step cursor 0x8226E158 walks at stride 0x28.
bool SubMissionSequencer::advance_step() noexcept {
  if (sub_mission_ >= step_counts_.size()) return false;
  if (step_ + 1 >= step_counts_[sub_mission_]) return false;
  step_ += 1;
  return true;
}

// 0x82267008: has `threshold` elapsed since this sub-mission started?
std::optional<bool> SubMissionSequencer::elapsed_at_least(float threshold,
                                                          float now) const noexcept {
  if (sub_mission_ >= started_at_.size()) return std::nullopt;
  return threshold <= now - started_at_[sub_mission_];
}

std::optional<std::uint32_t> SubMissionSequencer::evaluate(
    const CounterCondition& condition) const noexcept {
  // Retail treats 0 and 0xFFFF as "no condition" before indexing anything.
  if (condition.counter_id == 0 || condition.counter_id == 0xFFFFu) return std::nullopt;
  if (condition.counter_id >= counters_.size()) return std::nullopt;
  const std::int32_t value = counters_[condition.counter_id].value;
  const std::int32_t threshold = condition.threshold;
  bool satisfied = false;
  switch (condition.comparison) {
    case CounterComparison::Equal: satisfied = value == threshold; break;
    case CounterComparison::AtMost: satisfied = value <= threshold; break;
    case CounterComparison::AtLeast: satisfied = value >= threshold; break;
  }
  if (!satisfied) return std::nullopt;
  return condition.target_sub_mission;
}

bool SubMissionSequencer::set_counter(std::uint16_t id, std::int32_t value) noexcept {
  if (id >= counters_.size()) return false;
  counters_[id].value = value;
  return true;
}

std::optional<std::int32_t> SubMissionSequencer::counter(std::uint16_t id) const noexcept {
  if (id >= counters_.size()) return std::nullopt;
  return counters_[id].value;
}

std::optional<MissionCounter> SubMissionSequencer::counter_entry(
    std::uint16_t id) const noexcept {
  if (id >= counters_.size()) return std::nullopt;
  return counters_[id];
}

// 0x82267468, with the random draw retail takes from 0x82380798 injected by the
// caller and the never-sentinel of 0x82008120 guarding the stamp.
bool SubMissionSequencer::apply(std::uint16_t id, const CounterOperand& operand,
                                float now, std::uint32_t random) noexcept {
  if (id >= counters_.size()) return false;
  MissionCounter& target = counters_[id];

  // Retail computes the new value, stores it, and then runs the stamp check -
  // which it also reaches when the operation byte is out of range and nothing
  // was stored. Reproduced in that order.
  switch (operand.operation) {
    case CounterOperation::SetLiteral:
      target.value = operand.literal;
      break;
    case CounterOperation::AddLiteral:
      target.value += operand.literal;
      break;
    case CounterOperation::RandomOneToLiteral: {
      // 1 + random % literal, with retail's signed remainder. A zero literal
      // would divide by zero in retail; refuse rather than reproduce that.
      if (operand.literal == 0) return false;
      const std::int32_t draw = static_cast<std::int32_t>(random);
      target.value = draw - draw / operand.literal * operand.literal + 1;
      break;
    }
    case CounterOperation::SumOfTwo: {
      // Retail treats 0xFFFF as "no counter" and skips the store when either
      // side is absent; it does not range-check the other ids, this does.
      if (operand.source_a == 0xFFFF || operand.source_b == 0xFFFF) break;
      if (operand.source_a >= counters_.size() ||
          operand.source_b >= counters_.size()) {
        return false;
      }
      target.value = counters_[operand.source_a].value +
                     counters_[operand.source_b].value;
      break;
    }
  }

  // The stamp: only at exactly 1, and only while the field still holds kNever.
  if (target.value == 1 && target.reached_one_at == kNever) {
    target.reached_one_at = now;
  }
  return true;
}


// The loader's own sequence: 0x8219BDD8 builds the slots, 0x820A7070 turns the
// unit slot into objects, and root slot 1 sizes the counter table.
std::optional<RetailWorld> build_retail_world(const MissionScenario& scenario,
                                              std::uint32_t mission_id,
                                              std::uint32_t manager_category,
                                              std::optional<LocalPlayerSlot> local_player) {
  std::optional<RetailUnitBuild> build =
      build_units(scenario, manager_category, local_player);
  if (!build.has_value()) return std::nullopt;

  RetailWorld world;
  world.build = std::move(*build);

  for (const RetailUnitObject& object : world.build.objects) {
    const ScenarioUnitRecord& record = scenario.units()[object.record_index];
    const std::optional<ScenarioVector> spawn = initial_world_position(scenario, record);
    const std::uint32_t faction = object.faction_byte + 1u;

    UnitRecord unit;
    unit.id = object.entity;
    unit.owner = faction;
    unit.asset = object.category;
    if (!world.units.register_unit(unit)) return std::nullopt;
    if (!world.units.activate(unit.id)) return std::nullopt;

    CombatUnitState combat;
    combat.entity = object.entity;
    combat.faction = faction;
    combat.position = spawn.has_value() ? CombatVector{spawn->x, spawn->y, spawn->z}
                                        : CombatVector{};
    combat.health = 1.0f;
    combat.max_health = 1.0f;
    combat.collision_radius = 1.0f;
    combat.active = true;
    if (!world.combat.add_unit(combat)) return std::nullopt;
    (spawn.has_value() ? world.placed : world.unplaced).push_back(object.entity);
    world.published += 1;
  }

  for (const ScenarioSubMission& sub_mission : scenario.sub_missions()) {
    MissionObjectiveDefinition definition;
    definition.mission_id = mission_id;
    definition.objective.id = sub_mission.index + 1;
    definition.objective.stable_id =
        "mission01-submission-" + std::to_string(sub_mission.index);
    definition.objective.required = true;
    if (!world.objectives.add(std::move(definition))) return std::nullopt;
  }

  world.sequencer = SubMissionSequencer::from(scenario, scenario.counter_capacity());
  return world;
}

}  // namespace ac6::retail
