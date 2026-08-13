#include "ac6/retail_session.h"

#include "ac6/retail_campaign_bundle.h"
#include "ac6/retail_camera_table.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <span>

namespace ac6::retail {
namespace {

// 0x820A7420 hands the factory category 2 for the record its 16-entry table
// matched as the local player, and 1 for every other friendly. At most one
// record can match, because at most one ordinal can.
constexpr std::uint32_t kLocalPlayerCategory = 2;

constexpr std::array<std::uint8_t, 4> kRetailSequencerMagic{{'R', 'S', 'Q', '1'}};
constexpr std::uint32_t kRetailSequencerVersion = 1;
constexpr std::size_t kRetailSequencerMaximumEntries = 4096;
constexpr std::uint16_t kTargetButton = 0x4000u;  // X
constexpr std::uint16_t kFireButton = 0x1000u;    // A
constexpr std::uint32_t kPrimaryWeaponId = 1u;

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  for (unsigned int shift = 0; shift < 32; shift += 8) {
    bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
  }
}

void append_f32(std::vector<std::uint8_t>& bytes, float value) {
  std::uint32_t raw = 0;
  raw = std::bit_cast<std::uint32_t>(value);
  append_u32(bytes, raw);
}

bool take_u32(std::span<const std::uint8_t> bytes, std::size_t& offset,
              std::uint32_t& value) noexcept {
  if (bytes.size() < sizeof(value) || offset > bytes.size() - sizeof(value)) return false;
  value = static_cast<std::uint32_t>(bytes[offset]) |
          (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
          (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
          (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
  offset += sizeof(value);
  return true;
}

bool take_f32(std::span<const std::uint8_t> bytes, std::size_t& offset,
              float& value) noexcept {
  std::uint32_t raw = 0;
  if (!take_u32(bytes, offset, raw)) return false;
  value = std::bit_cast<float>(raw);
  return std::isfinite(value);
}

bool encode_sequencer(const SubMissionSequencerSnapshot& snapshot,
                      std::vector<std::uint8_t>& bytes) {
  if (snapshot.step_counts.size() > kRetailSequencerMaximumEntries ||
      snapshot.started_at.size() != snapshot.step_counts.size() ||
      snapshot.counters.size() > kRetailSequencerMaximumEntries) {
    return false;
  }
  for (const float started_at : snapshot.started_at) {
    if (!std::isfinite(started_at)) return false;
  }
  for (const MissionCounter& counter : snapshot.counters) {
    if (!std::isfinite(counter.reached_one_at)) return false;
  }
  bytes.clear();
  bytes.reserve(32 + snapshot.step_counts.size() * 8 + snapshot.counters.size() * 12);
  bytes.insert(bytes.end(), kRetailSequencerMagic.begin(), kRetailSequencerMagic.end());
  append_u32(bytes, kRetailSequencerVersion);
  append_u32(bytes, static_cast<std::uint32_t>(snapshot.step_counts.size()));
  append_u32(bytes, static_cast<std::uint32_t>(snapshot.counters.size()));
  append_u32(bytes, snapshot.sub_mission);
  append_u32(bytes, snapshot.step);
  for (const std::uint32_t count : snapshot.step_counts) append_u32(bytes, count);
  for (const float started_at : snapshot.started_at) append_f32(bytes, started_at);
  for (const MissionCounter& counter : snapshot.counters) {
    append_u32(bytes, static_cast<std::uint32_t>(counter.value));
    append_f32(bytes, counter.reached_one_at);
    append_u32(bytes, counter.bits);
  }
  return bytes.size() <= 1u << 20;
}

bool decode_sequencer(std::span<const std::uint8_t> bytes,
                      SubMissionSequencerSnapshot& snapshot) noexcept {
  std::size_t offset = 0;
  if (bytes.size() < kRetailSequencerMagic.size() ||
      !std::equal(kRetailSequencerMagic.begin(), kRetailSequencerMagic.end(), bytes.begin())) {
    return false;
  }
  offset = kRetailSequencerMagic.size();
  std::uint32_t version = 0;
  std::uint32_t sub_count = 0;
  std::uint32_t counter_count = 0;
  if (!take_u32(bytes, offset, version) || version != kRetailSequencerVersion ||
      !take_u32(bytes, offset, sub_count) || !take_u32(bytes, offset, counter_count) ||
      !take_u32(bytes, offset, snapshot.sub_mission) ||
      !take_u32(bytes, offset, snapshot.step) ||
      sub_count > kRetailSequencerMaximumEntries ||
      counter_count > kRetailSequencerMaximumEntries) {
    return false;
  }
  snapshot.step_counts.resize(sub_count);
  snapshot.started_at.resize(sub_count);
  snapshot.counters.resize(counter_count);
  for (std::uint32_t& count : snapshot.step_counts) {
    if (!take_u32(bytes, offset, count)) return false;
  }
  for (float& started_at : snapshot.started_at) {
    if (!take_f32(bytes, offset, started_at)) return false;
  }
  for (MissionCounter& counter : snapshot.counters) {
    std::uint32_t value = 0;
    if (!take_u32(bytes, offset, value) || !take_f32(bytes, offset, counter.reached_one_at) ||
        !take_u32(bytes, offset, counter.bits)) {
      return false;
    }
    counter.value = std::bit_cast<std::int32_t>(value);
  }
  return offset == bytes.size();
}

}  // namespace

std::optional<EntityId> local_player_entity(const RetailUnitBuild& build) noexcept {
  // The campaign arm carries no participant table, so category 2 is never
  // produced there and the class-0 Set is the answer instead (cycle 1259).
  if (build.campaign_player_entity.has_value()) {
    return *build.campaign_player_entity;
  }
  for (const RetailUnitObject& object : build.objects) {
    if (object.category == kLocalPlayerCategory) return object.entity;
  }
  return std::nullopt;
}

std::unique_ptr<RetailSession> RetailSession::open(const RetailContentStore& store,
                                                   CampaignLoadout loadout,
                                                   RetailSessionConfig config) {
  if (!store.valid() || !loadout.valid() || config.mission_id == 0 ||
      config.mission_id > kPalCampaignDataTableEntries.size() ||
      (config.script_drive != RetailScriptDrive::ExternalProbe &&
       config.script_drive != RetailScriptDrive::QualifiedRuntime)) {
    return nullptr;
  }
  // The common camera table is the first retail capability table already
  // imported into the sealed cache.  Qualify the aircraft ordinal against it
  // before opening the mission payload; an arbitrary non-zero loadout must
  // not masquerade as a hangar-resolved aircraft.
  // Bounded parser/session fixtures may intentionally import only the
  // selected campaign entry.  A product launch (the public commands) imports
  // the common table and is checked here when it is present; the payload-only
  // fixture remains a valid test overload without claiming a hangar model.
  if (store.find(kRetailCameraTableEntry) != nullptr) {
    const std::optional<RetailCampaignBundle> common =
        RetailCampaignBundle::open_entry(store, kRetailCameraTableEntry);
    if (!common.has_value()) return nullptr;
    const std::optional<RetailCameraTable> cameras = RetailCameraTable::open(*common);
    if (!cameras.has_value() || cameras->record_for_loadout(loadout, 1u) == nullptr) {
      return nullptr;
    }
  }
  std::optional<RetailMissionBundle> mission = RetailMissionBundle::open(
      store, {config.mission_id, config.difficulty, loadout});
  if (!mission.has_value()) return nullptr;
  if (!mission->scenario_payload().has_value() || !mission->scenario().has_value()) {
    return nullptr;
  }
  std::unique_ptr<RetailSession> session = open_parsed(
      std::move(*mission->scenario_payload()), std::move(*mission->scenario()), config);
  if (session == nullptr) return nullptr;
  session->bundle_ = RetailSessionBundle{
      mission->data_table_entry(), loadout, mission->content_index_sha256(),
      mission->difficulty()};
  return session;
}

std::unique_ptr<RetailSession> RetailSession::open(std::vector<std::uint8_t> bytes,
                                                   RetailSessionConfig config) {
  if (config.script_drive != RetailScriptDrive::ExternalProbe &&
      config.script_drive != RetailScriptDrive::QualifiedRuntime &&
      config.script_drive != RetailScriptDrive::DiagnosticFixedTick) {
    return nullptr;
  }
  const std::optional<RetailCameraModeSelection> camera_mode =
      resolve_retail_camera_mode(config.camera_mode_word);
  if (!camera_mode.has_value()) return nullptr;
  std::optional<ScenarioPayload> payload = ScenarioPayload::open(std::move(bytes));
  if (!payload.has_value()) return nullptr;
  std::optional<MissionScenario> scenario = MissionScenario::parse(*payload);
  if (!scenario.has_value()) return nullptr;
  return open_parsed(std::move(*payload), std::move(*scenario), config);
}

std::unique_ptr<RetailSession> RetailSession::open_parsed(ScenarioPayload payload,
                                                           MissionScenario scenario,
                                                           RetailSessionConfig config) {
  const std::optional<RetailCameraModeSelection> camera_mode =
      resolve_retail_camera_mode(config.camera_mode_word);
  if (!camera_mode.has_value()) return nullptr;
  // 0x8219BDD8's order: read the container, publish it at context+0x264, then
  // size the counter table, the sub-mission timestamps and the faction census
  // from the slots it just published. build_retail_world does that and then
  // runs 0x820A7070 over the unit slot.
  std::optional<RetailWorld> world =
      build_retail_world(scenario, config.mission_id,
                         ac6::retail::kMissionManagerCampaign,
                         config.local_player);
  if (!world.has_value()) return nullptr;
  const std::optional<EntityId> player = local_player_entity(world->build);
  if (!player.has_value()) return nullptr;

  std::unique_ptr<RetailSession> session(new RetailSession);
  session->mission_id_ = config.mission_id;
  session->camera_mode_ = *camera_mode;
  session->player_entity_ = *player;
  session->script_drive_ = config.script_drive;
  session->payload_ = std::make_unique<ScenarioPayload>(std::move(payload));
  session->scenario_ = std::make_unique<MissionScenario>(std::move(scenario));
  session->world_ = std::make_unique<RetailWorld>(std::move(*world));
  session->script_ = MissionScriptRunner::from(*session->scenario_);

  // The mission declares no external asset, and that is a statement, not an
  // omission: visual parity is out of JF's scope, so the session does not
  // pretend to resolve retail archives. mission_ready therefore means what is
  // left of it - the scenario is in gameplay.
  session->definition_ = std::make_unique<MissionDefinition>(
      MissionDefinition{config.mission_id, MissionFamily::Unknown, {}});
  session->assets_ = std::make_unique<MissionAssetDatabase>();
  session->execution_ = std::make_unique<MissionExecution>(
      *session->definition_, session->assets_.get(), &session->world_->objectives);

  MissionLaunchDefinition launch;
  launch.mission_id = config.mission_id;
  launch.player_entity = *player;
  // The first native combat slice uses one deterministic primary weapon. Its
  // projectile and collision code are the shared CombatWorld implementation;
  // no wave/target is synthesized by the session.
  launch.weapons.push_back({kPrimaryWeaponId, 100.0F, 2000.0F, 0.25F,
                            1.0e9F});
  launch.combat_states = session->world_->combat.snapshot_units();
  launch.units.reserve(launch.combat_states.size());
  for (const CombatUnitState& state : launch.combat_states) {
    const UnitRecord* unit = session->world_->units.find(state.entity);
    if (unit == nullptr) return nullptr;
    launch.units.push_back(*unit);
  }
  if (!session->execution_->launch(launch)) return nullptr;

  // 0x82258D88 at 0x8225912C: the mission state's entry branch makes the first
  // advance itself, and the guard in 0x82267370 is what makes it run step 0
  // rather than skip it.
  session->script_.start();
  (void)session->world_->sequencer.select(session->script_.sub_mission(), 0.0f);
  (void)session->resolve_tag7_conditions();
  session->track_objective(session->script_.sub_mission());
  return session;
}

std::optional<MissionArea> RetailSession::current_area() const noexcept {
  const std::vector<ScenarioSubMission>& sub_missions = scenario_->sub_missions();
  const std::uint32_t index = script_.sub_mission();
  if (index >= sub_missions.size()) return std::nullopt;
  const ScenarioSubMissionSetup& setup = sub_missions[index].setup;
  if (!setup.present) return std::nullopt;
  // 0x8226E2A8: FUN_82268B28(pfVar3[0], pfVar3[2], pfVar3[1], pfVar3[3], ctx).
  return normalise_area(setup.x0, setup.z0, setup.x1, setup.z1);
}

bool RetailSession::lock_nearest_target() noexcept {
  if (!execution_) return false;
  const EntityId target = execution_->nearest_enemy(player_entity_);
  return target != 0 && execution_->lock_target(target);
}

bool RetailSession::fire_primary() noexcept {
  return execution_ != nullptr && execution_->fire_weapon(kPrimaryWeaponId);
}

EntityId RetailSession::target_entity() const noexcept {
  return execution_ == nullptr ? 0 : execution_->locked_target();
}

RetailSessionFrame RetailSession::tick(float fixed_dt, InputFrame input) noexcept {
  RetailSessionFrame frame;
  const std::uint16_t pressed =
      static_cast<std::uint16_t>(input.buttons & ~previous_buttons_);
  if ((pressed & kTargetButton) != 0U) (void)lock_nearest_target();
  if ((pressed & kFireButton) != 0U) (void)fire_primary();
  previous_buttons_ = input.buttons;
  if (script_drive_ == RetailScriptDrive::QualifiedRuntime) {
    (void)advance_qualified_scheduler();
  } else if (script_drive_ == RetailScriptDrive::DiagnosticFixedTick &&
             !script_.ended()) {
    (void)advance_script();
  }
  frame.world = execution_->tick(fixed_dt, input);
  frame.camera_mode = camera_mode_;
  tick_ = frame.world.tick;
  frame.sub_mission = script_.sub_mission();
  frame.step = script_.step();
  frame.script_ended = script_.ended();
  const std::optional<MissionArea> area = current_area();
  if (area.has_value()) {
    // FUN_82268BA0 reads components 0 and 2 of the position and ignores the
    // second, so the flight integrator's altitude never enters this test.
    frame.player_inside_area = area_contains(
        *area, ScenarioVector{frame.world.position_x, frame.world.position_y,
                              frame.world.position_z});
  }
  return frame;
}

bool RetailSession::advance_qualified_scheduler() noexcept {
  // These are the three retail scheduler guards made explicit at the native
  // boundary: a live execution/context, a live cursor, and a producer-owned
  // world with at least the local player published.  A paused/terminal HSM
  // state also blocks signal -2, matching MissionExecution::tick.
  const bool context_ready = execution_ != nullptr && world_ != nullptr &&
                              scenario_ != nullptr;
  const bool cursor_ready = !script_.ended() && script_.step_current();
  const bool producer_ready = context_ready && player_entity_ != 0 &&
                              world_->units.find(player_entity_) != nullptr;
  if (!context_ready || !cursor_ready || !producer_ready ||
      execution_->scenario().state() != ScenarioState::Gameplay) {
    return false;
  }
  return advance_script() == ScriptAdvance::Ran;
}

// The native objective row for a sub-mission is a positional label - cycle 1097
// established that Mission 01's script carries no objective condition at all -
// so the row follows the cursor and nothing else: it becomes active when the
// cursor arrives on its sub-mission and complete when the cursor leaves. A
// sub-mission the cursor jumps over is left where it was, because retail's
// tag-7 jump does not visit it either.
void RetailSession::track_objective(std::uint32_t sub_mission) noexcept {
  if (sub_mission < scenario_->sub_missions().size()) {
    (void)execution_->activate_objective(sub_mission + 1);
  }
}

std::size_t RetailSession::render_world_markers(NativeRenderTarget& target,
                                                const WorldFrame& frame,
                                                float far_plane) const noexcept {
  // The faction byte is the retail one, +1 as build_retail_world stores it, so
  // these colours partition the world exactly as the scenario's faction table
  // does - 140/42/48 on Mission 01. Nothing here is chosen per mission.
  static constexpr std::uint32_t kFactionColor[4] = {
      0xFF9BD1FFu,  // faction 1
      0xFFFF8A6Bu,  // faction 2
      0xFFB6F5A0u,  // faction 3
      0xFFD9D9D9u,  // any further faction the table declares
  };
  // A unit the container gives no load-time position is not drawn at all. The
  // origin is not a fallback, it is a different claim.
  const std::vector<EntityId>& placed = world_->placed;
  std::size_t drawn = 0;
  for (const CombatUnitState& unit : world_->combat.snapshot_units()) {
    if (!unit.active) continue;
    if (std::find(placed.begin(), placed.end(), unit.entity) == placed.end()) continue;
    const bool is_player = unit.entity == player_entity_;
    const std::uint32_t color = is_player
        ? 0xFFFFFFFFu
        : kFactionColor[unit.faction == 0 ? 3 : (unit.faction - 1) % 4];
    if (target.draw_world_marker(frame, unit.position, color, is_player ? 3u : 1u,
                                 nullptr, far_plane)) {
      drawn += 1;
    }
  }
  return drawn;
}

bool RetailSession::apply_flag_order(std::size_t order_index, float now,
                                     std::uint32_t random) noexcept {
  if (scenario_ == nullptr || world_ == nullptr ||
      order_index >= scenario_->flag_orders().size()) {
    return false;
  }
  const ScenarioFlagOrder& order = scenario_->flag_orders()[order_index];
  if (order.operation > static_cast<std::uint8_t>(CounterOperation::SumOfTwo)) {
    return false;
  }
  const CounterOperand operand{
      static_cast<std::int16_t>(order.literal), 0xFFFFu, 0xFFFFu,
      static_cast<CounterOperation>(order.operation)};
  return world_->sequencer.apply(order.counter_id, operand, now, random);
}

std::optional<std::int32_t> RetailSession::counter(std::uint16_t id) const noexcept {
  return world_ == nullptr ? std::nullopt : world_->sequencer.counter(id);
}

ScriptAdvance RetailSession::advance_script() noexcept {
  const std::uint32_t before = script_.sub_mission();
  ScriptAdvance result = script_.drive_frame();
  if (result == ScriptAdvance::Ran) {
    result = resolve_tag7_conditions();
  }
  const std::uint32_t after = script_.sub_mission();
  if (after != before) {
    (void)world_->sequencer.select(after, static_cast<float>(tick_) / 60.0f);
    (void)execution_->complete_objective(before + 1);
    track_objective(after);
  }
  return result;
}

bool RetailSession::save_checkpoint(MissionExecution::Checkpoint& checkpoint) const noexcept {
  if (execution_ == nullptr || !execution_->save_checkpoint(checkpoint)) return false;
  if (!encode_sequencer(world_->sequencer.snapshot(), checkpoint.retail_sequencer_state)) {
    return false;
  }
  checkpoint.retail_script_state_valid = true;
  checkpoint.retail_script_sub_mission = script_.sub_mission();
  checkpoint.retail_script_step = script_.step();
  checkpoint.retail_script_end_code = script_.end_code();
  return true;
}

bool RetailSession::restore_checkpoint(
    const MissionExecution::Checkpoint& checkpoint) noexcept {
  if (execution_ == nullptr || !checkpoint.retail_script_state_valid ||
      checkpoint.retail_sequencer_state.empty()) {
    return false;
  }
  SubMissionSequencerSnapshot sequencer_snapshot;
  if (!decode_sequencer(checkpoint.retail_sequencer_state, sequencer_snapshot)) return false;
  if (sequencer_snapshot.sub_mission != checkpoint.retail_script_sub_mission) return false;
  MissionScriptRunner previous_script = script_;
  SubMissionSequencer previous_sequencer = world_->sequencer;
  if (!script_.restore_cursor(checkpoint.retail_script_sub_mission,
                              checkpoint.retail_script_step,
                              checkpoint.retail_script_end_code)) {
    return false;
  }
  if (!world_->sequencer.restore(sequencer_snapshot) ||
      !execution_->restore_checkpoint(checkpoint)) {
    script_ = std::move(previous_script);
    world_->sequencer = std::move(previous_sequencer);
    return false;
  }
  tick_ = checkpoint.flight.tick;
  previous_buttons_ = 0;
  return true;
}

bool RetailSession::restore_save(const SessionSaveSnapshot& snapshot) noexcept {
  if (!bundle_.has_value() || snapshot.mission_id != mission_id_ ||
      !snapshot.checkpoint.has_value() ||
      snapshot.checkpoint->retail_sequencer_state.empty() ||
      std::all_of(snapshot.content_index_sha256.begin(),
                  snapshot.content_index_sha256.end(),
                  [](std::uint8_t byte) { return byte == 0; }) ||
      snapshot.content_index_sha256 != bundle_->content_index_sha256) {
    return false;
  }
  return restore_checkpoint(*snapshot.checkpoint);
}

ScriptAdvance RetailSession::resolve_tag7_conditions() noexcept {
  // A target can itself dispatch another tag-7 step. Retail follows that
  // dispatch synchronously; retain the same property while bounding malformed
  // self-jumps from untrusted cache bytes.
  constexpr std::size_t kMaxHops = 4096;
  for (std::size_t hop = 0; hop < kMaxHops; ++hop) {
    const std::optional<ScriptStepRun> current = script_.current_step();
    if (!current.has_value() || current->tag != 7) return ScriptAdvance::Ran;

    const std::optional<ScenarioStepCondition> raw = script_.current_condition();
    ScriptAdvance result = ScriptAdvance::Ran;
    if (!raw.has_value()) {
      result = script_.advance();
    } else {
      const CounterCondition condition{
          raw->counter_id, raw->threshold,
          static_cast<CounterComparison>(raw->comparison),
          raw->target_sub_mission};
      const std::optional<std::uint32_t> target = world_->sequencer.evaluate(condition);
      result = target.has_value() ? script_.select(*target) : script_.advance();
    }
    if (result == ScriptAdvance::Exhausted) {
      script_.end_by_script();
      return result;
    }
  }

  // Retail would keep following a self-jump. The native boundary refuses to
  // spin forever and leaves the mission running at the last dispatched step.
  return ScriptAdvance::Ran;
}

}  // namespace ac6::retail
