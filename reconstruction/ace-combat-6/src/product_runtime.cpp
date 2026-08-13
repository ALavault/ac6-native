#include "ac6/product_runtime.h"
#include "text_parse.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>

namespace ac6 {
namespace {

MissionFamily parse_family(std::string_view family) noexcept {
  if (family == "air_intercept") return MissionFamily::AirIntercept;
  if (family == "strike") return MissionFamily::Strike;
  if (family == "escort") return MissionFamily::Escort;
  return MissionFamily::Unknown;
}

bool parse_asset_ids(std::string_view text, std::vector<AssetId>& asset_ids) {
  while (!text.empty()) {
    const auto comma = text.find(',');
    const auto token = text.substr(0, comma);
    AssetId id{};
    if (token.empty() || !detail::parse_u32(token, id) || id == 0 ||
        std::find(asset_ids.begin(), asset_ids.end(), id) != asset_ids.end()) {
      return false;
    }
    asset_ids.push_back(id);
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
  return !asset_ids.empty();
}

}  // namespace

bool MissionCatalog::add(MissionDefinition definition) {
  if (definition.id == 0 || definition.family == MissionFamily::Unknown ||
      definition.asset_ids.empty()) return false;
  for (std::size_t i = 0; i < definition.asset_ids.size(); ++i) {
    if (definition.asset_ids[i] == 0) return false;
    if (std::find(definition.asset_ids.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                  definition.asset_ids.end(), definition.asset_ids[i]) != definition.asset_ids.end()) {
      return false;
    }
  }
  return missions_.emplace(definition.id, std::move(definition)).second;
}

bool MissionCatalog::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionCatalog loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    if (first == std::string::npos || second == std::string::npos ||
        line.find('\t', second + 1) != std::string::npos) {
      return false;
    }
    std::uint32_t mission_id{};
    const auto id_text = std::string_view(line).substr(0, first);
    if (!detail::parse_u32(id_text, mission_id)) return false;
    std::vector<AssetId> asset_ids;
    const auto assets_text = std::string_view(line).substr(second + 1);
    if (!parse_asset_ids(assets_text, asset_ids)) return false;
    if (!loaded.add({mission_id, parse_family(std::string_view(line).substr(first + 1, second - first - 1)),
                     std::move(asset_ids)})) {
      return false;
    }
  }
  missions_ = std::move(loaded.missions_);
  return true;
}

const MissionDefinition* MissionCatalog::find(std::uint32_t id) const noexcept {
  const auto it = missions_.find(id);
  return it == missions_.end() ? nullptr : &it->second;
}

MissionScenario::MissionScenario(const MissionDefinition& definition) : mission_id_(definition.id) {}

bool MissionScenario::bind_player(const UnitRegistry& units, EntityId entity) noexcept {
  const UnitRecord* unit = units.find(entity);
  if (unit == nullptr || !unit->active) return false;
  player_ = entity;
  return true;
}

bool MissionScenario::dispatch(Event event) noexcept {
  switch (event.type) {
    case EventType::StartMission:
      if (state_ != ScenarioState::Loading && state_ != ScenarioState::Briefing) return false;
      if (player_ != 0 && event.subject != player_) return false;
      state_ = ScenarioState::Gameplay;
      return true;
    case EventType::Pause:
      if (state_ != ScenarioState::Gameplay) return false;
      state_ = ScenarioState::Paused;
      return true;
    case EventType::Resume:
      if (state_ != ScenarioState::Paused) return false;
      state_ = ScenarioState::Gameplay;
      return true;
    case EventType::Complete:
      if (state_ != ScenarioState::Gameplay && state_ != ScenarioState::Paused) return false;
      if (!objectives_.all_required_complete()) return false;
      state_ = ScenarioState::Complete;
      return true;
    case EventType::Abort:
      if (state_ == ScenarioState::Complete || state_ == ScenarioState::Aborted) return false;
      state_ = ScenarioState::Aborted;
      return true;
  }
  return false;
}

bool MissionScenario::complete_objective(std::uint32_t id) noexcept {
  return objectives_.complete(id);
}

bool MissionScenario::add_objective(ObjectiveRecord objective) {
  return objectives_.add(std::move(objective));
}

bool MissionScenario::activate_objective(std::uint32_t id) noexcept {
  return objectives_.activate(id);
}

bool MissionScenario::fail_objective(std::uint32_t id) noexcept {
  if (!objectives_.fail(id)) return false;
  state_ = ScenarioState::Aborted;
  return true;
}

bool MissionScenario::evaluate_combat(const UnitRegistry& units,
                                      const CombatWorld& combat) noexcept {
  if (state_ != ScenarioState::Gameplay || player_ == 0) return false;
  bool changed = false;
  for (const ObjectiveRecord& objective : objectives_.snapshot()) {
    if (objective.state != ObjectiveState::Active ||
        objective.condition == ObjectiveCondition::Manual) {
      continue;
    }
    // A condition never completes or fails from a stale entity id.  The unit
    // registry is the authoritative scenario ownership table; combat state
    // alone is not enough to qualify a retail target binding.
    if (units.find(objective.target_entity) == nullptr) continue;
    const CombatUnitState* target = combat.unit(objective.target_entity);
    const bool target_active = target != nullptr && target->active && target->health > 0.0f;
    if (objective.condition == ObjectiveCondition::DestroyUnit && !target_active) {
      changed = objectives_.complete(objective.id) || changed;
    } else if (objective.condition == ObjectiveCondition::ProtectUnit && !target_active) {
      changed = objectives_.fail(objective.id) || changed;
    }
  }
  if (objectives_.failed_count() != 0) state_ = ScenarioState::Aborted;
  return changed;
}

bool MissionScenario::dispatch_radio(const RadioMessageDatabase& messages,
                                     std::uint32_t id) noexcept {
  if (state_ != ScenarioState::Gameplay && state_ != ScenarioState::Paused) return false;
  if (messages.find(mission_id_, id) == nullptr) return false;
  radio_history_.push_back(id);
  return true;
}

std::optional<std::uint32_t> MissionScenario::objective_index(std::uint32_t id) const noexcept {
  const std::vector<ObjectiveRecord> records = objectives_.snapshot();
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (records[index].id == id) return static_cast<std::uint32_t>(index);
  }
  return std::nullopt;
}

MissionDebrief MissionScenario::debrief() const {
  MissionDebrief result;
  result.mission_id = mission_id_;
  result.outcome = state_ == ScenarioState::Complete
                       ? MissionOutcome::Success
                       : (state_ == ScenarioState::Aborted ? MissionOutcome::Failure
                                                            : MissionOutcome::InProgress);
  result.objective_count = static_cast<std::uint32_t>(objectives_.size());
  result.completed_objectives = static_cast<std::uint32_t>(objectives_.completed_count());
  result.failed_objectives = static_cast<std::uint32_t>(objectives_.failed_count());
  result.radio_history = radio_history_;
  return result;
}

MissionScenarioSnapshot MissionScenario::snapshot() const {
  return {mission_id_, state_, player_, objectives_.snapshot(), radio_history_};
}

bool MissionScenario::restore(const MissionScenarioSnapshot& snapshot) noexcept {
  if (snapshot.mission_id != mission_id_ ||
      static_cast<std::uint8_t>(snapshot.state) >
          static_cast<std::uint8_t>(ScenarioState::Aborted) ||
      snapshot.radio_history.size() > 65536) return false;
  for (const std::uint32_t message : snapshot.radio_history) {
    if (message == 0) return false;
  }
  ObjectiveRegistry loaded;
  if (!loaded.restore(snapshot.objectives)) return false;
  objectives_ = std::move(loaded);
  state_ = snapshot.state;
  player_ = snapshot.player;
  radio_history_ = snapshot.radio_history;
  return true;
}

bool MissionScenario::dispatch_buttons(const InputMappingDatabase& mappings,
                                       std::uint16_t buttons, EntityId subject) noexcept {
  const InputBinding* binding = mappings.resolve(buttons);
  return binding != nullptr && dispatch({binding->event, subject});
}


MissionRuntime::MissionRuntime(std::uint32_t mission_id, const MissionAssetDatabase* assets)
    : mission_id_(mission_id), assets_(assets) {}

MissionRuntime::MissionRuntime(const MissionDefinition& definition,
                               const MissionAssetDatabase* assets)
    : mission_id_(definition.id), assets_(assets), definition_(&definition) {}

RuntimeSnapshot MissionRuntime::snapshot() const noexcept {
  return {tick_, position_x_, position_y_, position_z_, pitch_, roll_, yaw_, fixed_accumulator_};
}

bool MissionRuntime::restore(RuntimeSnapshot snapshot) noexcept {
  if (!std::isfinite(snapshot.position_x) ||
      !std::isfinite(snapshot.position_y) || !std::isfinite(snapshot.position_z) ||
      !std::isfinite(snapshot.pitch) || !std::isfinite(snapshot.roll) ||
      !std::isfinite(snapshot.yaw) || !std::isfinite(snapshot.fixed_accumulator) ||
      snapshot.fixed_accumulator < 0.0f || snapshot.fixed_accumulator >= 1.0f / 60.0f) return false;
  tick_ = snapshot.tick;
  position_x_ = snapshot.position_x;
  position_y_ = snapshot.position_y;
  position_z_ = snapshot.position_z;
  pitch_ = snapshot.pitch;
  roll_ = snapshot.roll;
  yaw_ = snapshot.yaw;
  fixed_accumulator_ = snapshot.fixed_accumulator;
  return true;
}

bool MissionRuntime::set_definition(const MissionDefinition* definition) noexcept {
  if (definition == nullptr || definition->id != mission_id_ ||
      definition->family == MissionFamily::Unknown || definition->asset_ids.empty()) {
    definition_ = nullptr;
    return false;
  }
  definition_ = definition;
  return true;
}

WorldFrame MissionRuntime::tick(float fixed_dt, InputFrame input) {
  const bool scheduler_stopped = scenario_ != nullptr &&
      (scenario_->state() == ScenarioState::Paused ||
       scenario_->state() == ScenarioState::Complete ||
       scenario_->state() == ScenarioState::Aborted);
  if (!scheduler_stopped) {
    if (!(fixed_dt > 0.0f) || fixed_dt > 0.25f) fixed_dt = 1.0f / 60.0f;
    constexpr float simulation_dt = 1.0f / 60.0f;
    constexpr std::uint32_t max_steps_per_call = 16;
    fixed_accumulator_ = std::min(fixed_accumulator_ + fixed_dt, 0.25f);
    const auto axis = [](std::int16_t value) {
      return std::clamp(static_cast<float>(value) / 32767.0f, -1.0f, 1.0f);
    };
    std::uint32_t steps = 0;
    while (fixed_accumulator_ + 1.0e-7f >= simulation_dt && steps < max_steps_per_call) {
      fixed_accumulator_ = std::max(0.0f, fixed_accumulator_ - simulation_dt);
      ++tick_;
      pitch_ += axis(input.pitch) * simulation_dt;
      roll_ += axis(input.roll) * simulation_dt;
      yaw_ += axis(input.yaw) * simulation_dt;
      position_x_ += yaw_ * simulation_dt;
      position_y_ += pitch_ * simulation_dt;
      position_z_ += (static_cast<float>(input.throttle) / 255.0f) * simulation_dt;
      ++steps;
    }
  }
  bool ready = assets_ != nullptr && definition_ != nullptr && scenario_ != nullptr &&
               scenario_->state() == ScenarioState::Gameplay;
  if (ready) {
    for (const AssetId id : definition_->asset_ids) {
      ready = assets_->resolve(id) != nullptr;
      if (!ready) break;
    }
  }
  const auto active_units = units_ ? static_cast<std::uint32_t>(units_->active_count()) : 0u;
  const auto player = scenario_ ? scenario_->player() : EntityId{};
  constexpr float follow_distance = 12.0f;
  constexpr float follow_height = 3.0f;
  const float forward_speed = static_cast<float>(input.throttle) / 255.0f;
  const float speed = std::sqrt(pitch_ * pitch_ + roll_ * roll_ + yaw_ * yaw_ +
                                forward_speed * forward_speed);
  return WorldFrame{tick_, mission_id_, ready, position_x_, position_y_, position_z_, pitch_, roll_, yaw_,
                    speed, active_units, player, position_x_ - follow_distance, position_y_ + follow_height,
                    position_z_ + follow_distance, position_x_, position_y_, position_z_, input};
}

MissionExecution::MissionExecution(const MissionDefinition& definition,
                                   const MissionAssetDatabase* assets,
                                   const MissionObjectiveDatabase* objectives,
                                   const RadioMessageDatabase* radios,
                                   CampaignProgression* campaign,
                                   MissionWaveDirector* waves,
                                   MissionSequenceDirector* sequence,
    const InputMappingDatabase* input,
    MissionAiDirector* ai)
    : definition_(&definition), assets_(assets), objectives_(objectives), radios_(radios), campaign_(campaign),
      waves_(waves), sequence_(sequence), input_(input), ai_(ai),
      runtime_(definition, assets),
      scenario_(definition) {}

bool MissionExecution::launch(const MissionLaunchDefinition& launch) noexcept {
  if (definition_ == nullptr || launch.mission_id != definition_->id) return false;
  if (campaign_ == nullptr) {
    // The standalone runtime path remains available for developer fixtures.
  } else {
    const CampaignMissionStatus* status = campaign_->status(definition_->id);
    if (status == nullptr || status->state != CampaignMissionState::Active) return false;
  }
  units_ = UnitRegistry{};
  combat_.clear();
  radio_.reset();
  primary_weapon_id_ = 0;
  weapon_count_ = 0;
  if (waves_ != nullptr) waves_->reset();
  if (sequence_ != nullptr) sequence_->reset();
  scenario_ = MissionScenario(*definition_);
  if (objectives_ != nullptr) {
    for (const ObjectiveRecord* objective : objectives_->find_by_mission(definition_->id)) {
      if (objective == nullptr || !scenario_.add_objective(*objective)) {
        units_ = UnitRegistry{};
        scenario_ = MissionScenario(*definition_);
        launched_ = false;
        return false;
      }
    }
  }
  if (!configure_mission_launch(launch, units_, scenario_) ||
      !scenario_.dispatch({EventType::StartMission, launch.player_entity})) {
    units_ = UnitRegistry{};
    combat_.clear();
    scenario_ = MissionScenario(*definition_);
    launched_ = false;
    return false;
  }
  const bool supplied_states = !launch.combat_states.empty();
  if (supplied_states && launch.combat_states.size() != launch.units.size()) {
    units_ = UnitRegistry{};
    combat_.clear();
    scenario_ = MissionScenario(*definition_);
    launched_ = false;
    return false;
  }
  std::size_t spawn_index = 0;
  for (std::size_t index = 0; index < launch.units.size(); ++index) {
    const UnitRecord& unit = launch.units[index];
    const float spawn_x = unit.id == launch.player_entity
        ? 0.0f
        : 20.0f + static_cast<float>(spawn_index++) * 5.0f;
    const CombatUnitState state = supplied_states
        ? launch.combat_states[index]
        : CombatUnitState{unit.id, unit.owner, {spawn_x, 0.0f, 0.0f},
                          100.0f, 100.0f, 1.0f, true};
    if (state.entity != unit.id || !combat_.add_unit(state)) {
      units_ = UnitRegistry{};
      combat_.clear();
      scenario_ = MissionScenario(*definition_);
      launched_ = false;
      return false;
    }
  }
  for (const WeaponDefinition weapon : launch.weapons) {
    if (!combat_.add_weapon(weapon)) {
      units_ = UnitRegistry{};
      combat_.clear();
      scenario_ = MissionScenario(*definition_);
      launched_ = false;
      return false;
    }
  }
  if (!launch.weapons.empty()) primary_weapon_id_ = launch.weapons.front().id;
  weapon_count_ = static_cast<std::uint32_t>(launch.weapons.size());
  runtime_.set_definition(definition_);
  runtime_.set_units(&units_);
  runtime_.set_scenario(&scenario_);
  launched_ = true;
  return true;
}

bool MissionExecution::dispatch(Event event) noexcept {
  if (!launched_) return false;
  if (event.type == EventType::Complete && campaign_ != nullptr &&
      !campaign_->can_complete(definition_->id)) return false;
  if (event.type == EventType::Abort && campaign_ != nullptr) {
    const CampaignMissionStatus* status = campaign_->status(definition_->id);
    if (status == nullptr || status->state != CampaignMissionState::Active) return false;
  }
  if (!scenario_.dispatch(event)) return false;
  if (event.type == EventType::Complete && campaign_ != nullptr) {
    return campaign_->complete(definition_->id);
  }
  if (event.type == EventType::Abort && campaign_ != nullptr) {
    return campaign_->fail(definition_->id);
  }
  return true;
}

bool MissionExecution::activate_objective(std::uint32_t id) noexcept {
  return launched_ && scenario_.activate_objective(id);
}

bool MissionExecution::complete_objective(std::uint32_t id) noexcept {
  if (!launched_) return false;
  const auto index = scenario_.objective_index(id);
  if (!index || (campaign_ != nullptr &&
                 !campaign_->can_complete_objective(definition_->id, *index))) return false;
  if (!scenario_.complete_objective(id)) return false;
  return campaign_ == nullptr || campaign_->complete_objective(definition_->id, *index);
}

bool MissionExecution::fail_objective(std::uint32_t id) noexcept {
  if (!launched_ || !scenario_.fail_objective(id)) return false;
  return campaign_ == nullptr || campaign_->fail(definition_->id);
}

bool MissionExecution::dispatch_radio(std::uint32_t id) noexcept {
  return launched_ && radios_ != nullptr && scenario_.dispatch_radio(*radios_, id);
}

bool MissionExecution::play_radio(std::uint32_t id, float duration_seconds) noexcept {
  if (!launched_ || radios_ == nullptr || !radio_.start(*radios_, definition_->id, id,
                                                          duration_seconds)) return false;
  if (scenario_.dispatch_radio(*radios_, id)) return true;
  radio_.reset();
  return false;
}

bool MissionSequenceDirector::dispatch_due(std::uint32_t mission_id, std::uint64_t tick,
                                           MissionExecution& execution) noexcept {
  for (Entry& entry : entries_) {
    if (entry.published || entry.event.mission_id != mission_id || entry.event.tick > tick) {
      continue;
    }
    bool dispatched = false;
    switch (entry.event.type) {
      case MissionSequenceEventType::ActivateObjective:
        dispatched = execution.activate_objective(entry.event.id);
        break;
      case MissionSequenceEventType::CompleteObjective:
        dispatched = execution.complete_objective(entry.event.id);
        break;
      case MissionSequenceEventType::FailObjective:
        dispatched = execution.fail_objective(entry.event.id);
        break;
      case MissionSequenceEventType::PlayRadio:
        dispatched = execution.play_radio(entry.event.id, entry.event.duration_seconds);
        break;
    }
    if (!dispatched) return false;
    entry.published = true;
  }
  return true;
}

std::size_t MissionSequenceDirector::pending(std::uint32_t mission_id) const noexcept {
  return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
      [mission_id](const Entry& entry) {
        return entry.event.mission_id == mission_id && !entry.published;
      }));
}

std::size_t MissionSequenceDirector::dispatched(std::uint32_t mission_id) const noexcept {
  return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
      [mission_id](const Entry& entry) {
        return entry.event.mission_id == mission_id && entry.published;
      }));
}

MissionSequenceSnapshot MissionSequenceDirector::snapshot() const {
  MissionSequenceSnapshot result;
  result.entries.reserve(entries_.size());
  for (const Entry& entry : entries_) result.entries.push_back({entry.event, entry.published});
  return result;
}

bool MissionSequenceDirector::restore(const MissionSequenceSnapshot& snapshot) noexcept {
  if (snapshot.entries.size() > 4096) return false;
  std::vector<Entry> loaded;
  loaded.reserve(snapshot.entries.size());
  std::uint32_t previous_mission = 0;
  std::uint64_t previous_tick = 0;
  std::uint32_t previous_order = 0;
  for (const MissionSequenceEntrySnapshot& candidate : snapshot.entries) {
    const MissionSequenceEvent& event = candidate.event;
    if (!event.valid() ||
        (event.mission_id < previous_mission) ||
        (event.mission_id == previous_mission && event.tick < previous_tick) ||
        (event.mission_id == previous_mission && event.tick == previous_tick &&
         event.order <= previous_order)) return false;
    loaded.push_back({event, candidate.published});
    previous_mission = event.mission_id;
    previous_tick = event.tick;
    previous_order = event.order;
  }
  entries_ = std::move(loaded);
  return true;
}

void MissionSequenceDirector::reset() noexcept {
  for (Entry& entry : entries_) entry.published = false;
}

bool MissionExecution::lock_target(EntityId target) noexcept {
  return launched_ && combat_.lock_target(scenario_.player(), target);
}

EntityId MissionExecution::locked_target() const noexcept {
  return launched_ ? combat_.locked_target(scenario_.player()) : 0;
}

EntityId MissionExecution::nearest_enemy(const EntityId owner) const noexcept {
  if (!launched_) return 0;
  const CombatUnitState* source = combat_.unit(owner);
  if (source == nullptr || !source->active) return 0;
  EntityId result = 0;
  float best_distance = std::numeric_limits<float>::max();
  for (const CombatUnitState& candidate : combat_.snapshot_units()) {
    if (!candidate.active || candidate.entity == owner ||
        candidate.faction == source->faction) {
      continue;
    }
    const float dx = candidate.position.x - source->position.x;
    const float dy = candidate.position.y - source->position.y;
    const float dz = candidate.position.z - source->position.z;
    const float distance = dx * dx + dy * dy + dz * dz;
    if (!std::isfinite(distance) || distance <= 0.000001F ||
        (distance > best_distance && result != 0) ||
        (distance == best_distance && candidate.entity >= result)) {
      continue;
    }
    best_distance = distance;
    result = candidate.entity;
  }
  return result;
}

bool MissionExecution::fire_weapon(std::uint32_t weapon_id) noexcept {
  return launched_ && combat_.fire(scenario_.player(), weapon_id);
}

WorldFrame MissionExecution::tick(float fixed_dt, InputFrame input) noexcept {
  if (!launched_) return {};
  if (input_ != nullptr && input.buttons != 0) {
    const InputBinding* binding = input_->resolve(input.buttons);
    if (binding != nullptr && !dispatch({binding->event, scenario_.player()})) return {};
  }
  if (scenario_.state() == ScenarioState::Gameplay) combat_.tick(fixed_dt);
  WorldFrame frame = runtime_.tick(fixed_dt, input);
  if (scenario_.state() == ScenarioState::Gameplay) (void)radio_.tick(fixed_dt);
  if (scenario_.state() == ScenarioState::Gameplay && waves_ != nullptr &&
      !waves_->spawn_due(definition_->id, frame.tick, units_, combat_)) {
    frame.mission_ready = false;
    return frame;
  }
  if (scenario_.state() == ScenarioState::Gameplay && ai_ != nullptr &&
      !ai_->dispatch_due(definition_->id, frame.tick, combat_)) {
    frame.mission_ready = false;
    return frame;
  }
  if (scenario_.state() == ScenarioState::Gameplay && sequence_ != nullptr &&
      !sequence_->dispatch_due(definition_->id, frame.tick, *this)) {
    frame.mission_ready = false;
    return frame;
  }
  for (const CombatUnitState& unit : combat_.snapshot_units()) {
    (void)units_.set_active(unit.entity, unit.active && unit.health > 0.0f);
  }
  frame.active_units = static_cast<std::uint32_t>(units_.active_count());
  (void)scenario_.evaluate_combat(units_, combat_);
  if (scenario_.state() != ScenarioState::Gameplay) frame.mission_ready = false;
  if (scenario_.state() == ScenarioState::Gameplay) {
    const CombatUnitState* player = combat_.unit(scenario_.player());
    const bool player_destroyed = player == nullptr || !player->active;
    const bool expired = failure_tick_ != 0 && frame.tick >= failure_tick_;
    if (player_destroyed || expired) {
      if (dispatch({EventType::Abort, scenario_.player()})) {
        frame.mission_ready = false;
        frame.active_units = static_cast<std::uint32_t>(combat_.active_units());
      }
    }
  }
  if (scenario_.state() == ScenarioState::Gameplay && scenario_.objectives().size() != 0 &&
      scenario_.objectives().all_required_complete()) {
    // Objective completion is the native HSM terminal condition. Keep the
    // explicit dispatch API for qualified event consumers, but do not require
    // an external caller to synthesize the mission-complete event.
    if (dispatch({EventType::Complete, scenario_.player()})) frame.mission_ready = false;
  }
  return frame;
}

WorldFrame MissionExecution::run_replay(float fixed_dt, const ReplayLog& replay) noexcept {
  WorldFrame frame{};
  for (const InputFrame input : replay.frames()) frame = tick(fixed_dt, input);
  return frame;
}

RuntimeSnapshot MissionExecution::snapshot() const noexcept {
  return runtime_.snapshot();
}

MissionDebrief MissionExecution::debrief() const {
  return scenario_.debrief();
}

bool MissionExecution::restore(RuntimeSnapshot snapshot) noexcept {
  return launched_ && runtime_.restore(snapshot);
}

bool MissionExecution::save_checkpoint(Checkpoint& checkpoint) const noexcept {
  if (!launched_ || combat_.active_projectiles() != 0) {
    return false;
  }
  Checkpoint candidate;
  candidate.mission_id = definition_ == nullptr ? 0 : definition_->id;
  candidate.flight = runtime_.snapshot();
  candidate.scenario = scenario_.snapshot();
  candidate.unit_records = units_.snapshot();
  candidate.combat_units = combat_.snapshot_units();
  if (assets_ != nullptr) {
    candidate.resource_identities.reserve(definition_->asset_ids.size());
    for (const AssetId id : definition_->asset_ids) {
      const AssetRecord* resource = assets_->resolve(id);
      if (resource == nullptr || !resource->valid()) return false;
      candidate.resource_identities.push_back(*resource);
    }
    std::sort(candidate.resource_identities.begin(), candidate.resource_identities.end(),
              [](const AssetRecord& left, const AssetRecord& right) {
                return left.id < right.id;
              });
  }
  candidate.failure_tick = failure_tick_;
  candidate.waves = waves_ == nullptr ? MissionWaveSnapshot{} : waves_->snapshot();
  candidate.sequence = sequence_ == nullptr ? MissionSequenceSnapshot{} : sequence_->snapshot();
  candidate.radio_playback = radio_.snapshot();
  if (candidate.mission_id == 0) return false;
  checkpoint = std::move(candidate);
  return true;
}

bool MissionExecution::restore_checkpoint(const Checkpoint& checkpoint) noexcept {
  if (!launched_ || definition_ == nullptr || checkpoint.mission_id != definition_->id ||
      checkpoint.scenario.mission_id != definition_->id ||
      checkpoint.scenario.player == 0 ||
      checkpoint.combat_units.empty() ||
      checkpoint.resource_identities.size() > 4096 ||
      (sequence_ == nullptr && !checkpoint.sequence.entries.empty()) ||
      (waves_ == nullptr && !checkpoint.waves.entries.empty())) return false;
  if (std::find_if(checkpoint.combat_units.begin(), checkpoint.combat_units.end(),
                   [&](const CombatUnitState& unit) {
                     return unit.entity == checkpoint.scenario.player;
                   }) == checkpoint.combat_units.end()) return false;
  AssetId previous_resource = 0;
  for (const AssetRecord& resource : checkpoint.resource_identities) {
    if (!resource.valid() || resource.id <= previous_resource) return false;
    previous_resource = resource.id;
  }
  if (!checkpoint.resource_identities.empty()) {
    if (assets_ == nullptr || checkpoint.resource_identities.size() != definition_->asset_ids.size()) {
      return false;
    }
    for (const AssetRecord& checkpoint_resource : checkpoint.resource_identities) {
      const AssetRecord* current_resource = assets_->resolve(checkpoint_resource.id);
      if (current_resource == nullptr || *current_resource != checkpoint_resource) return false;
    }
  }
  for (const MissionSequenceEntrySnapshot& entry : checkpoint.sequence.entries) {
    if (!entry.event.valid() || entry.event.mission_id != definition_->id) return false;
  }
  if (!checkpoint.unit_records.empty()) {
    EntityId previous_unit_record = 0;
    for (const UnitRecord& record : checkpoint.unit_records) {
      if (record.id == 0 || record.asset == 0 || record.owner == record.id ||
          record.id <= previous_unit_record) return false;
      previous_unit_record = record.id;
      const auto combat_unit = std::find_if(
          checkpoint.combat_units.begin(), checkpoint.combat_units.end(),
          [record](const CombatUnitState& unit) { return unit.entity == record.id; });
      if (combat_unit == checkpoint.combat_units.end() || combat_unit->faction != record.owner) {
        return false;
      }
    }
    for (const CombatUnitState& unit : checkpoint.combat_units) {
      if (std::find_if(checkpoint.unit_records.begin(), checkpoint.unit_records.end(),
                       [unit](const UnitRecord& record) { return record.id == unit.entity; }) ==
          checkpoint.unit_records.end()) return false;
    }
  }
  if (waves_ != nullptr && !checkpoint.waves.entries.empty()) {
    MissionWaveDirector validated_waves;
    if (!validated_waves.restore(checkpoint.waves)) return false;
  }
  const RuntimeSnapshot old_flight = runtime_.snapshot();
  const MissionScenarioSnapshot old_scenario = scenario_.snapshot();
  const std::vector<UnitRecord> old_unit_records = units_.snapshot();
  const std::vector<CombatUnitState> old_units = combat_.snapshot_units();
  const std::uint64_t old_failure_tick = failure_tick_;
  const MissionWaveSnapshot old_waves = waves_ == nullptr ? MissionWaveSnapshot{} : waves_->snapshot();
  const MissionSequenceSnapshot old_sequence =
      sequence_ == nullptr ? MissionSequenceSnapshot{} : sequence_->snapshot();
  const RadioPlaybackSnapshot old_radio = radio_.snapshot();
  if (!runtime_.restore(checkpoint.flight) || !scenario_.restore(checkpoint.scenario) ||
      (!checkpoint.unit_records.empty() && !units_.restore(checkpoint.unit_records)) ||
      !combat_.restore_units(checkpoint.combat_units) ||
      (waves_ != nullptr && !checkpoint.waves.entries.empty() &&
       !waves_->restore(checkpoint.waves)) ||
      (sequence_ != nullptr && !sequence_->restore(checkpoint.sequence)) ||
      !radio_.restore(checkpoint.radio_playback)) {
    (void)runtime_.restore(old_flight);
    (void)scenario_.restore(old_scenario);
    (void)units_.restore(old_unit_records);
    (void)combat_.restore_units(old_units);
    failure_tick_ = old_failure_tick;
    if (waves_ != nullptr) (void)waves_->restore(old_waves);
    if (sequence_ != nullptr) (void)sequence_->restore(old_sequence);
    (void)radio_.restore(old_radio);
    return false;
  }
  failure_tick_ = checkpoint.failure_tick;
  return true;
}

WorldFrame MissionRuntime::run_replay(float fixed_dt, const ReplayLog& replay) {
  WorldFrame frame{};
  for (const InputFrame input : replay.frames()) frame = tick(fixed_dt, input);
  return frame;
}

}  // namespace ac6
