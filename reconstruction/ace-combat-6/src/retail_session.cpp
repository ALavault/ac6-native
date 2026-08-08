#include "ac6/retail_session.h"

namespace ac6::retail {
namespace {

// 0x820A7420 hands the factory category 2 for the record its 16-entry table
// matched as the local player, and 1 for every other friendly. At most one
// record can match, because at most one ordinal can.
constexpr std::uint32_t kLocalPlayerCategory = 2;

}  // namespace

std::optional<EntityId> local_player_entity(const RetailUnitBuild& build) noexcept {
  for (const RetailUnitObject& object : build.objects) {
    if (object.category == kLocalPlayerCategory) return object.entity;
  }
  return std::nullopt;
}

std::unique_ptr<RetailSession> RetailSession::open(std::vector<std::uint8_t> bytes,
                                                   RetailSessionConfig config) {
  std::optional<ScenarioPayload> payload = ScenarioPayload::open(std::move(bytes));
  if (!payload.has_value()) return nullptr;
  std::optional<MissionScenario> scenario = MissionScenario::parse(*payload);
  if (!scenario.has_value()) return nullptr;
  // 0x8219BDD8's order: read the container, publish it at context+0x264, then
  // size the counter table, the sub-mission timestamps and the faction census
  // from the slots it just published. build_retail_world does that and then
  // runs 0x820A7070 over the unit slot.
  std::optional<RetailWorld> world =
      build_retail_world(*scenario, config.mission_id, config.local_player);
  if (!world.has_value()) return nullptr;
  const std::optional<EntityId> player = local_player_entity(world->build);
  if (!player.has_value()) return nullptr;

  std::unique_ptr<RetailSession> session(new RetailSession);
  session->mission_id_ = config.mission_id;
  session->player_entity_ = *player;
  session->payload_ = std::make_unique<ScenarioPayload>(std::move(*payload));
  session->scenario_ = std::make_unique<MissionScenario>(std::move(*scenario));
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

RetailSessionFrame RetailSession::tick(float fixed_dt, InputFrame input) noexcept {
  RetailSessionFrame frame;
  frame.world = execution_->tick(fixed_dt, input);
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

ScriptAdvance RetailSession::advance_script() noexcept {
  const std::uint32_t before = script_.sub_mission();
  const ScriptAdvance result = script_.drive_frame();
  const std::uint32_t after = script_.sub_mission();
  if (after != before) {
    (void)execution_->complete_objective(before + 1);
    track_objective(after);
  }
  return result;
}

}  // namespace ac6::retail
