#pragma once

// A playable session whose only input is the retail scenario container.
//
// The product's session command reads TSV manifests: a catalogue, a launch
// table, an objective list. This one reads none of them. It opens the payload,
// builds the world with the consumer behaviours already ported (0x8219BDD8's
// loader order and 0x820A7070's per-record construction, whose insertion into
// the unit table lives in retail_mission_state.cpp where it was derived),
// hands that world to the product's own runtime, and runs the session loop -
// input, flight integration, camera, HUD - over it.
//
// What the session does NOT do is decide when the mission ends. That belongs to
// the sub-mission script, and the script advances only when the caller says so,
// because the gate retail uses is not modelled: 0x822ED708 tests context+0x820
// and the low six bits of FUN_82268C58()'s +0x124 before every advance, and
// neither field has a native counterpart yet. The cadence is therefore the
// caller's, and it is stated as such rather than dressed up as derived. Nothing
// about the *outcome* depends on it: the trace of executed steps and the tick at
// which the script runs out are the same for any cadence.

#include "ac6/product_runtime.h"
#include "ac6/retail_mission_script.h"
#include "ac6/retail_mission_state.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace ac6::retail {

struct RetailSessionConfig {
  std::uint32_t mission_id{1};
  // Which record the local-player branch matches, as 0x820A7420 resolves it.
  LocalPlayerSlot local_player{0, 0};
};

// One frame of the session, as the product's runtime produced it, plus where
// the sub-mission script stands.
struct RetailSessionFrame {
  WorldFrame world;
  std::uint32_t sub_mission{};
  std::uint32_t step{};
  bool script_ended{};
  bool player_inside_area{};  // FUN_82268BA0 against this sub-mission's rectangle
};

class RetailSession final {
 public:
  // Opens the payload and builds everything the session needs from it. Fails
  // when the container does not parse, when the world cannot be built, or when
  // no record matches the local-player slot - a session with no player is not
  // a session.
  static std::unique_ptr<RetailSession> open(std::vector<std::uint8_t> payload,
                                             RetailSessionConfig config);

  const MissionScenario& scenario() const noexcept { return *scenario_; }
  const RetailWorld& world() const noexcept { return *world_; }
  const MissionScriptRunner& script() const noexcept { return script_; }
  MissionExecution& execution() noexcept { return *execution_; }
  const MissionExecution& execution() const noexcept { return *execution_; }
  EntityId player_entity() const noexcept { return player_entity_; }

  // The rectangle this sub-mission installs, normalised by the port of
  // FUN_82268B28. None when the sub-mission has no tag-0 step.
  std::optional<MissionArea> current_area() const noexcept;

  // One session frame: input, flight, camera, HUD state. Does not advance the
  // script.
  RetailSessionFrame tick(float fixed_dt, InputFrame input) noexcept;

  // One call of 0x82267370 through 0x822ED708's update branch. The sub-mission
  // the cursor leaves has its objective completed and the one it arrives on has
  // its objective activated - that is the only thing in this product that
  // moves an objective, and the retail cursor is what drives it.
  ScriptAdvance advance_script() noexcept;

  // Draws the world the container built: one marker per active unit, coloured
  // by the faction byte the retail faction table gave it, the local player
  // distinguished. Returns how many landed on screen.
  //
  // Markers are a diagnostic lane, not geometry - see
  // NativeRenderTarget::draw_world_marker. The retail session knows where 230
  // units are long before it knows what they look like, and this is what makes
  // the placement work checkable instead of merely asserted.
  std::size_t render_world_markers(NativeRenderTarget& target, const WorldFrame& frame,
                                   float far_plane = 0.0f) const noexcept;

  MissionDebrief debrief() const { return execution_->debrief(); }
  ScenarioState state() const noexcept { return execution_->scenario().state(); }

  RetailSession(const RetailSession&) = delete;
  RetailSession& operator=(const RetailSession&) = delete;

 private:
  RetailSession() = default;
  void track_objective(std::uint32_t sub_mission) noexcept;

  std::unique_ptr<ScenarioPayload> payload_;
  std::unique_ptr<MissionScenario> scenario_;
  std::unique_ptr<RetailWorld> world_;
  std::unique_ptr<MissionDefinition> definition_;
  std::unique_ptr<MissionAssetDatabase> assets_;
  std::unique_ptr<MissionExecution> execution_;
  MissionScriptRunner script_;
  EntityId player_entity_{};
  std::uint32_t mission_id_{};
  std::uint64_t tick_{};
};

// The entity 0x820A7420 classified as the local player: the one record whose
// category came out as 2. None when the slot matched nothing.
std::optional<EntityId> local_player_entity(const RetailUnitBuild& build) noexcept;

}  // namespace ac6::retail
