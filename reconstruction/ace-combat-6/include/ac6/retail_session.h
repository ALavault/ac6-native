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
// The native session does not synthesize the product scheduler's signal -2.
// Retail's three guards and their objective/combat producers have no complete
// native counterpart yet. Payload-only fixtures can call advance_script
// explicitly, or opt into a named diagnostic cadence. Store-backed sessions
// reject that diagnostic mode. Tag-7 conditions are evaluated when their step
// becomes current; their counter producers remain a separate boundary rather
// than being synthesized.

#include "ac6/campaign_progression.h"
#include "ac6/frontend_runtime.h"
#include "ac6/product_runtime.h"
#include "ac6/render_scene.h"
#include "ac6/retail_camera_table.h"
#include "ac6/retail_content.h"
#include "ac6/retail_mission_script.h"
#include "ac6/retail_mission_bundle.h"
#include "ac6/retail_mission_state.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace ac6::retail {

enum class RetailScriptDrive : std::uint8_t {
  // Fail-closed: a qualified external runtime/producer must call
  // advance_script only after the retail guards and producer join permit it.
  ExternalProbe = 0,
  // Reserved for the native implementation of those guards and producers.
  // Neither session entry point accepts this mode today.
  QualifiedRuntime = 1,
  // Payload-only diagnostic: inject signal -2 once per native fixed tick.
  // This is deliberately rejected by the sealed-store product entry point.
  DiagnosticFixedTick = 2,
};

struct RetailSessionConfig {
  std::uint32_t mission_id{1};
  // Which record the local-player branch matches, as 0x820A7420 resolves it.
  LocalPlayerSlot local_player{0, 0};
  // Raw player camera selector consumed by 0x82226D80. The campaign opening
  // path is the qualified zero word, which 0x82223AC0 maps to view 1.
  std::uint32_t camera_mode_word{kRetailOpeningCameraModeWord};
  RetailDifficulty difficulty{RetailDifficulty::Normal};
  RetailScriptDrive script_drive{RetailScriptDrive::ExternalProbe};
};

// One frame of the session, as the product's runtime produced it, plus where
// the sub-mission script stands.
struct RetailSessionFrame {
  WorldFrame world;
  RetailCameraModeSelection camera_mode{retail_opening_camera_mode()};
  std::uint32_t sub_mission{};
  std::uint32_t step{};
  bool script_ended{};
  bool player_inside_area{};  // FUN_82268BA0 against this sub-mission's rectangle
};

// Provenance retained when a session is opened through the sealed retail
// content store. The payload-only overload intentionally has no provenance:
// it exists for bounded parser/runtime tests, not for a product launch.
struct RetailSessionBundle final {
  std::uint32_t data_table_entry{};
  CampaignLoadout loadout{};
  Sha256Digest content_index_sha256{};
  RetailDifficulty difficulty{RetailDifficulty::Normal};
  bool operator==(const RetailSessionBundle&) const = default;
};

class RetailSession final {
 public:
  // Opens the payload and builds everything the session needs from it. Fails
  // when the container does not parse, when the world cannot be built, or when
  // no record matches the local-player slot - a session with no player is not
  // a session.
  static std::unique_ptr<RetailSession> open(std::vector<std::uint8_t> payload,
                                             RetailSessionConfig config);

  // Product entry point. Mission ids 1..15 map to the qualified PAL campaign
  // entries 9..23. A store that is incomplete/incompatible or a loadout with
  // unresolved capability data is rejected before the payload is parsed.
  static std::unique_ptr<RetailSession> open(const RetailContentStore& store,
                                             CampaignLoadout loadout,
                                             RetailSessionConfig config);

  const MissionScenario& scenario() const noexcept { return *scenario_; }
  const RetailWorld& world() const noexcept { return *world_; }
  const MissionScriptRunner& script() const noexcept { return script_; }
  MissionExecution& execution() noexcept { return *execution_; }
  const MissionExecution& execution() const noexcept { return *execution_; }
  EntityId player_entity() const noexcept { return player_entity_; }
  const std::optional<RetailSessionBundle>& bundle() const noexcept {
    return bundle_;
  }
  const CampaignProgression* campaign() const noexcept {
    return campaign_enabled_ ? &campaign_ : nullptr;
  }
  // A complete PAL cache owns the frontend handoff. Scenario-only fixtures
  // intentionally keep this disabled and remain on the diagnostic path.
  bool frontend_enabled() const noexcept { return frontend_enabled_; }
  FrontendState frontend_state() const noexcept { return frontend_.state(); }
  CampaignSaveSnapshot campaign_snapshot() const {
    return campaign_enabled_ ? campaign_.snapshot() : CampaignSaveSnapshot{};
  }
  const RetailCameraModeSelection& camera_mode() const noexcept {
    return camera_mode_;
  }

  // The rectangle this sub-mission installs, normalised by the port of
  // FUN_82268B28. None when the sub-mission has no tag-0 step.
  std::optional<MissionArea> current_area() const noexcept;

  // One session frame: input, flight, camera, HUD state. Script advancement is
  // producer-owned unless a payload-only diagnostic explicitly requests the
  // fixed-tick cadence.
  RetailSessionFrame tick(float fixed_dt, InputFrame input) noexcept;

  // Read-only renderer handoff. It does not tick simulation or advance the
  // script; callers use it to build/update persistent GPU resources.
  [[nodiscard]] SimulationSnapshot render_snapshot() const noexcept;

  // Mission 01's first native combat bridge. X selects the deterministic
  // nearest hostile and A fires the qualified primary weapon on its rising
  // edge; no target or projectile is created without that player action.
  bool lock_nearest_target() noexcept;
  bool fire_primary() noexcept;
  EntityId target_entity() const noexcept;

  // Controller lifecycle actions. Start toggles the retail gameplay HSM
  // between Gameplay and Paused; Back restores the sealed launch checkpoint.
  bool pause() noexcept;
  bool resume() noexcept;
  bool toggle_pause() noexcept;
  bool restart() noexcept;

  // One call of 0x82267370 through 0x822ED708's update branch. Callers invoke
  // this only after an external probe/runtime has qualified the retail guards.
  // The sub-mission the cursor leaves has its objective completed and the one
  // it arrives on has its objective activated.
  ScriptAdvance advance_script() noexcept;

  // Save/restore the execution and retail script cursor as one product
  // boundary. A retail save must carry the sealed cache identity; legacy
  // manifest saves are intentionally rejected here.
  bool save_checkpoint(MissionExecution::Checkpoint& checkpoint) const noexcept;
  bool restore_checkpoint(const MissionExecution::Checkpoint& checkpoint) noexcept;
  bool restore_save(const SessionSaveSnapshot& snapshot) noexcept;

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

  // Executes one authored OrderFlagBin through the qualified counter writer.
  // The behaviour scheduler owns when an order is reached; this explicit
  // boundary keeps that timing separate from the byte-accurate operation.
  bool apply_flag_order(std::size_t order_index, float now,
                        std::uint32_t random) noexcept;
  std::optional<std::int32_t> counter(std::uint16_t id) const noexcept;

  MissionDebrief debrief() const { return execution_->debrief(); }
  ScenarioState state() const noexcept { return execution_->scenario().state(); }

  RetailSession(const RetailSession&) = delete;
  RetailSession& operator=(const RetailSession&) = delete;

 private:
  RetailSession() = default;
  static std::unique_ptr<RetailSession> open_parsed(ScenarioPayload payload,
                                                    MissionScenario scenario,
                                                    RetailSessionConfig config,
                                                    const RetailContentStore* store = nullptr,
                                                    CampaignLoadout loadout = {});
  void track_objective(std::uint32_t sub_mission) noexcept;
  RetailSessionFrame frame_from_snapshot(InputFrame input) const noexcept;
  // Resolve tag-7 steps at the same dispatch boundary as retail. A satisfied
  // condition selects its target; an unsatisfied or sentinel condition calls
  // the normal script advance. The loop is bounded so a malformed self-jump
  // cannot hang the native session.
  ScriptAdvance resolve_tag7_conditions() noexcept;

  std::unique_ptr<ScenarioPayload> payload_;
  std::unique_ptr<MissionScenario> scenario_;
  std::unique_ptr<RetailWorld> world_;
  std::unique_ptr<MissionDefinition> definition_;
  std::unique_ptr<MissionAssetDatabase> assets_;
  CampaignProgression campaign_;
  bool campaign_enabled_{};
  FrontendController frontend_;
  bool frontend_enabled_{};
  std::unique_ptr<MissionExecution> execution_;
  MissionScriptRunner script_;
  std::optional<RetailSessionBundle> bundle_;
  EntityId player_entity_{};
  std::uint32_t mission_id_{};
  RetailCameraModeSelection camera_mode_{retail_opening_camera_mode()};
  std::uint64_t tick_{};
  RetailScriptDrive script_drive_{RetailScriptDrive::ExternalProbe};
  std::uint16_t previous_buttons_{};
  std::optional<MissionExecution::Checkpoint> launch_checkpoint_;
};

// The entity 0x820A7420 classified as the local player: the one record whose
// category came out as 2. None when the slot matched nothing.
std::optional<EntityId> local_player_entity(const RetailUnitBuild& build) noexcept;

}  // namespace ac6::retail
