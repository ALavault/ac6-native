#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <vector>

#include "ac6/campaign_progression.h"

namespace ac6 {

using AssetId = std::uint32_t;
using EntityId = std::uint32_t;

struct InputFrame {
  std::int16_t pitch{};
  std::int16_t roll{};
  std::int16_t yaw{};
  std::uint8_t throttle{};
  std::uint16_t buttons{};
  bool operator==(const InputFrame&) const = default;
};

struct WorldFrame {
  std::uint64_t tick{};
  std::uint32_t mission_id{};
  bool mission_ready{};
  float position_x{};
  float position_y{};
  float position_z{};
  float pitch{};
  float roll{};
  float yaw{};
  std::uint32_t active_units{};
  EntityId player_entity{};
  float camera_x{};
  float camera_y{};
  float camera_z{};
  float camera_target_x{};
  float camera_target_y{};
  float camera_target_z{};
  InputFrame input{};
};

struct RuntimeSnapshot {
  std::uint64_t tick{};
  float position_x{};
  float position_y{};
  float position_z{};
  float pitch{};
  float roll{};
  float yaw{};
  float fixed_accumulator{};
  bool operator==(const RuntimeSnapshot&) const = default;
};

enum class EventType : std::uint8_t { StartMission, Pause, Resume, Complete, Abort };

struct Event {
  EventType type{};
  EntityId subject{};
};

struct InputBinding {
  std::uint16_t button_mask{};
  EventType event{EventType::Pause};
  bool valid() const noexcept;
};

class InputMappingDatabase final {
 public:
  bool add(InputBinding binding);
  bool load_manifest(const std::filesystem::path& manifest);
  const InputBinding* resolve(std::uint16_t buttons) const noexcept;

 private:
  std::vector<InputBinding> bindings_;
};

struct UnitRecord {
  EntityId id{};
  EntityId owner{};
  AssetId asset{};
  bool active{};
};

class UnitRegistry final {
 public:
  bool register_unit(UnitRecord unit);
  bool activate(EntityId id) noexcept;
  bool deactivate(EntityId id) noexcept;
  const UnitRecord* find(EntityId id) const noexcept;
  std::size_t size() const noexcept { return units_.size(); }
  std::size_t active_count() const noexcept;

 private:
  std::unordered_map<EntityId, UnitRecord> units_;
};

struct CombatVector {
  float x{};
  float y{};
  float z{};
  bool operator==(const CombatVector&) const = default;
};

struct CombatUnitState {
  EntityId entity{};
  EntityId faction{};
  CombatVector position{};
  float health{};
  float max_health{};
  float collision_radius{1.0f};
  bool active{};
  bool valid() const noexcept;
  bool operator==(const CombatUnitState&) const = default;
};

struct WeaponDefinition {
  std::uint32_t id{};
  float damage{};
  float projectile_speed{};
  float cooldown{};
  float max_range{};
  bool valid() const noexcept;
};

struct ProjectileState {
  std::uint32_t id{};
  EntityId owner{};
  EntityId target{};
  CombatVector position{};
  CombatVector velocity{};
  float damage{};
  float remaining_range{};
  bool active{};
};

class CombatWorld final {
 public:
  bool add_unit(CombatUnitState unit);
  bool add_weapon(WeaponDefinition weapon);
  bool lock_target(EntityId owner, EntityId target) noexcept;
  EntityId locked_target(EntityId owner) const noexcept;
  bool fire(EntityId owner, std::uint32_t weapon_id) noexcept;
  bool apply_damage(EntityId target, float damage) noexcept;
  bool deactivate_unit(EntityId entity) noexcept;
  void tick(float fixed_dt) noexcept;
  const CombatUnitState* unit(EntityId entity) const noexcept;
  std::vector<CombatUnitState> snapshot_units() const;
  bool restore_units(const std::vector<CombatUnitState>& units) noexcept;
  std::size_t active_units() const noexcept;
  std::size_t active_projectiles() const noexcept;
  std::uint64_t damage_events() const noexcept { return damage_events_; }
  void clear() noexcept;

 private:
  struct WeaponRuntime {
    WeaponDefinition definition;
    float cooldown_remaining{};
  };
  std::vector<CombatUnitState> units_;
  std::vector<WeaponRuntime> weapons_;
  std::vector<ProjectileState> projectiles_;
  std::unordered_map<EntityId, EntityId> locks_;
  std::uint32_t next_projectile_id_{1};
  std::uint64_t damage_events_{};
};

struct MissionWaveSpawn {
  std::uint32_t mission_id{};
  std::uint64_t spawn_tick{};
  UnitRecord unit;
  CombatUnitState combat;
  bool valid() const noexcept;
};

class MissionWaveDirector final {
 public:
  bool add(MissionWaveSpawn spawn);
  bool load_manifest(const std::filesystem::path& manifest);
  bool spawn_due(std::uint32_t mission_id, std::uint64_t tick,
                 UnitRegistry& units, CombatWorld& combat) noexcept;
  bool despawn(EntityId entity, UnitRegistry& units, CombatWorld& combat) noexcept;
  std::size_t pending(std::uint32_t mission_id) const noexcept;
  std::size_t spawned(std::uint32_t mission_id) const noexcept;
  void reset() noexcept;

 private:
  struct Entry {
    MissionWaveSpawn spawn;
    bool published{};
  };
  std::vector<Entry> entries_;
};

struct MissionAiRule {
  std::uint32_t mission_id{};
  std::uint64_t first_tick{};
  std::uint64_t period_ticks{1};
  EntityId entity{};
  EntityId target{};
  std::uint32_t weapon_id{};
  bool valid() const noexcept {
    return mission_id != 0 && first_tick != 0 && period_ticks != 0 && entity != 0 &&
           target != 0 && weapon_id != 0;
  }
  bool operator==(const MissionAiRule&) const = default;
};

class MissionAiDirector final {
 public:
  bool add(MissionAiRule rule);
  bool load_manifest(const std::filesystem::path& manifest);
  bool dispatch_due(std::uint32_t mission_id, std::uint64_t tick,
                    CombatWorld& combat) noexcept;
  std::size_t active(std::uint32_t mission_id, std::uint64_t tick) const noexcept;

 private:
  std::vector<MissionAiRule> rules_;
};

class UnitRegistry;
struct MissionDefinition;

enum class ObjectiveState : std::uint8_t { Pending, Active, Complete, Failed };

struct ObjectiveRecord {
  std::uint32_t id{};
  std::string stable_id;
  bool required{true};
  ObjectiveState state{ObjectiveState::Pending};
  bool valid() const noexcept { return id != 0 && !stable_id.empty(); }
  bool operator==(const ObjectiveRecord&) const = default;
};

class ObjectiveRegistry final {
 public:
  bool add(ObjectiveRecord objective);
  bool activate(std::uint32_t id) noexcept;
  bool complete(std::uint32_t id) noexcept;
  bool fail(std::uint32_t id) noexcept;
  const ObjectiveRecord* find(std::uint32_t id) const noexcept;
  bool all_required_complete() const noexcept;
  std::size_t completed_count() const noexcept;
  std::size_t failed_count() const noexcept;
  std::size_t size() const noexcept { return objectives_.size(); }
  std::vector<ObjectiveRecord> snapshot() const;
  bool restore(const std::vector<ObjectiveRecord>& snapshot) noexcept;

 private:
  std::unordered_map<std::uint32_t, ObjectiveRecord> objectives_;
};

struct MissionObjectiveDefinition {
  std::uint32_t mission_id{};
  ObjectiveRecord objective;
};

class MissionObjectiveDatabase final {
 public:
  bool add(MissionObjectiveDefinition definition);
  bool load_manifest(const std::filesystem::path& manifest);
  std::vector<const ObjectiveRecord*> find_by_mission(std::uint32_t mission_id) const;

 private:
  std::vector<MissionObjectiveDefinition> objectives_;
};

struct RadioMessageDefinition {
  std::uint32_t mission_id{};
  std::uint32_t id{};
  std::string stable_id;
  std::string speaker;
  AssetId audio_asset{};
  AssetId subtitle_asset{};
  bool valid() const noexcept {
    return mission_id != 0 && id != 0 && !stable_id.empty() && !speaker.empty() &&
           audio_asset != 0;
  }
};

class RadioMessageDatabase final {
 public:
  bool add(RadioMessageDefinition message);
  bool load_manifest(const std::filesystem::path& manifest);
  const RadioMessageDefinition* find(std::uint32_t mission_id, std::uint32_t id) const noexcept;

 private:
  std::vector<RadioMessageDefinition> messages_;
};

enum class RadioPlaybackState : std::uint8_t { Idle, Playing, Complete, Interrupted };

struct RadioPlaybackSnapshot {
  std::uint32_t mission_id{};
  std::uint32_t message_id{};
  AssetId audio_asset{};
  AssetId subtitle_asset{};
  float elapsed_seconds{};
  float duration_seconds{};
  RadioPlaybackState state{RadioPlaybackState::Idle};
  bool operator==(const RadioPlaybackSnapshot&) const = default;
};

class RadioPlaybackService final {
 public:
  bool start(const RadioMessageDatabase& messages, std::uint32_t mission_id,
             std::uint32_t message_id, float duration_seconds) noexcept;
  bool tick(float fixed_dt) noexcept;
  bool finish() noexcept;
  bool interrupt() noexcept;
  bool restore(RadioPlaybackSnapshot snapshot) noexcept;
  void reset() noexcept;
  bool playing() const noexcept { return snapshot_.state == RadioPlaybackState::Playing; }
  const RadioPlaybackSnapshot& snapshot() const noexcept { return snapshot_; }

 private:
  RadioPlaybackSnapshot snapshot_{};
};

class MissionExecution;
enum class MissionSequenceEventType : std::uint8_t {
  ActivateObjective,
  CompleteObjective,
  FailObjective,
  PlayRadio,
};

struct MissionSequenceEvent {
  std::uint32_t mission_id{};
  std::uint64_t tick{};
  std::uint32_t order{};
  MissionSequenceEventType type{MissionSequenceEventType::ActivateObjective};
  std::uint32_t id{};
  float duration_seconds{};
  bool valid() const noexcept;
  bool operator==(const MissionSequenceEvent&) const = default;
};

struct MissionSequenceEntrySnapshot {
  MissionSequenceEvent event;
  bool published{};
  bool operator==(const MissionSequenceEntrySnapshot&) const = default;
};

struct MissionSequenceSnapshot {
  std::vector<MissionSequenceEntrySnapshot> entries;
  bool operator==(const MissionSequenceSnapshot&) const = default;
};

class MissionSequenceDirector final {
 public:
  bool add(MissionSequenceEvent event);
  bool load_manifest(const std::filesystem::path& manifest);
  bool dispatch_due(std::uint32_t mission_id, std::uint64_t tick,
                    MissionExecution& execution) noexcept;
  std::size_t pending(std::uint32_t mission_id) const noexcept;
  std::size_t dispatched(std::uint32_t mission_id) const noexcept;
  MissionSequenceSnapshot snapshot() const;
  bool restore(const MissionSequenceSnapshot& snapshot) noexcept;
  void reset() noexcept;

 private:
  struct Entry {
    MissionSequenceEvent event;
    bool published{};
  };
  std::vector<Entry> entries_;
};

struct MissionRuntimeServices {
  InputMappingDatabase input;
  MissionObjectiveDatabase objectives;
  RadioMessageDatabase radios;
  MissionWaveDirector waves;
  MissionAiDirector ai;
  MissionSequenceDirector sequence;
  CampaignProgression campaign;
  bool has_input{};
  bool has_objectives{};
  bool has_radios{};
  bool has_waves{};
  bool has_ai{};
  bool has_sequence{};
  bool has_campaign{};
};

enum class ScenarioState : std::uint8_t { Loading, Briefing, Gameplay, Paused, Complete, Aborted };

enum class MissionOutcome : std::uint8_t { InProgress, Success, Failure };

struct MissionDebrief {
  std::uint32_t mission_id{};
  MissionOutcome outcome{MissionOutcome::InProgress};
  std::uint32_t objective_count{};
  std::uint32_t completed_objectives{};
  std::uint32_t failed_objectives{};
  std::vector<std::uint32_t> radio_history;
};

struct MissionScenarioSnapshot {
  std::uint32_t mission_id{};
  ScenarioState state{ScenarioState::Loading};
  EntityId player{};
  std::vector<ObjectiveRecord> objectives;
  std::vector<std::uint32_t> radio_history;
  bool operator==(const MissionScenarioSnapshot&) const = default;
};

class MissionScenario final {
 public:
  explicit MissionScenario(std::uint32_t mission_id) : mission_id_(mission_id) {}
  explicit MissionScenario(const MissionDefinition& definition);
  bool dispatch(Event event) noexcept;
  bool add_objective(ObjectiveRecord objective);
  bool activate_objective(std::uint32_t id) noexcept;
  bool complete_objective(std::uint32_t id) noexcept;
  bool fail_objective(std::uint32_t id) noexcept;
  bool dispatch_radio(const RadioMessageDatabase& messages, std::uint32_t id) noexcept;
  bool dispatch_buttons(const InputMappingDatabase& mappings, std::uint16_t buttons,
                        EntityId subject = 0) noexcept;
  void own_player(EntityId entity) noexcept { player_ = entity; }
  bool bind_player(const UnitRegistry& units, EntityId entity) noexcept;
  std::uint32_t mission_id() const noexcept { return mission_id_; }
  ScenarioState state() const noexcept { return state_; }
  EntityId player() const noexcept { return player_; }
  const ObjectiveRegistry& objectives() const noexcept { return objectives_; }
  std::optional<std::uint32_t> objective_index(std::uint32_t id) const noexcept;
  const std::vector<std::uint32_t>& radio_history() const noexcept { return radio_history_; }
  MissionDebrief debrief() const;
  MissionScenarioSnapshot snapshot() const;
  bool restore(const MissionScenarioSnapshot& snapshot) noexcept;

 private:
  std::uint32_t mission_id_;
  ScenarioState state_{ScenarioState::Loading};
  EntityId player_{};
  ObjectiveRegistry objectives_;
  std::vector<std::uint32_t> radio_history_;
};

struct AssetRecord {
  AssetId id{};
  std::string relative_path;
  std::string sha256;
  std::uint64_t byte_size{};
  std::vector<AssetId> dependencies;
  bool valid() const noexcept { return id != 0 && !relative_path.empty() && !sha256.empty(); }
  bool operator==(const AssetRecord&) const = default;
};

enum class MissionFamily : std::uint8_t { AirIntercept, Strike, Escort, Unknown };

struct MissionDefinition {
  std::uint32_t id{};
  MissionFamily family{MissionFamily::Unknown};
  std::vector<AssetId> asset_ids;
};

class MissionCatalog final {
 public:
  bool add(MissionDefinition definition);
  bool load_manifest(const std::filesystem::path& manifest);
  const MissionDefinition* find(std::uint32_t id) const noexcept;

 private:
  std::unordered_map<std::uint32_t, MissionDefinition> missions_;
};

class MissionAssetDatabase final {
 public:
  bool add(AssetRecord record);
  bool load_manifest(const std::filesystem::path& manifest);
  bool load_qualified_manifest(const std::filesystem::path& manifest);
  const AssetRecord* resolve(AssetId id) const noexcept;
  bool empty() const noexcept { return records_.empty(); }

 private:
  std::unordered_map<AssetId, AssetRecord> records_;
};

struct MissionLaunchDefinition {
  std::uint32_t mission_id{};
  EntityId player_entity{};
  std::vector<UnitRecord> units;
  std::vector<WeaponDefinition> weapons;
};

class MissionLaunchDatabase final {
 public:
  bool add(MissionLaunchDefinition definition);
  bool load_manifest(const std::filesystem::path& manifest);
  const MissionLaunchDefinition* find(std::uint32_t mission_id) const noexcept;

 private:
  std::unordered_map<std::uint32_t, MissionLaunchDefinition> launches_;
};

struct MissionManifestPaths {
  std::filesystem::path campaign;
  std::filesystem::path catalog;
  std::filesystem::path assets;
  std::filesystem::path launches;
  std::filesystem::path input;
  std::filesystem::path controls;
  std::filesystem::path objectives;
  std::filesystem::path radios;
  std::filesystem::path waves;
  std::filesystem::path ai;
  std::filesystem::path sequence;
  std::filesystem::path render;
  std::filesystem::path drawables;
  std::filesystem::path transforms;
  std::filesystem::path materials;
  std::filesystem::path textures;
  std::filesystem::path shaders;
  std::filesystem::path targets;
  std::filesystem::path passes;
  std::filesystem::path resolves;
  std::filesystem::path buffers;
  std::filesystem::path camera;
  bool valid() const noexcept {
    return !catalog.empty() && !assets.empty() && !launches.empty();
  }
  bool render_valid() const noexcept {
    return valid() && !render.empty() && !drawables.empty() && !transforms.empty() &&
           !materials.empty() && !textures.empty() && !shaders.empty() &&
           !targets.empty() && !passes.empty() && !resolves.empty() && !buffers.empty();
  }
};

class MissionRenderDatabase;
class MissionDrawableDatabase;
class MissionTransformDatabase;
class MissionMaterialDatabase;
class MissionTextureDatabase;
class ShaderPermutationDatabase;
class MissionRenderTargetDatabase;
class MissionRenderPassDatabase;
class MissionRenderResolveDatabase;
class QualifiedBufferDatabase;
class NativeGeometryDatabase;

struct MissionCameraDefinition {
  std::uint32_t mission_id{};
  std::array<float, 16> clip_rows{};
  // Retail vertex code forms the output vector as x*c218+y*c219+z*c220+c221.
  // Keep the historical row-major default, and opt into the qualified vector
  // (column-major) interpretation explicitly in the manifest.
  bool column_major{false};
  bool qualified{false};
  bool valid() const noexcept;
};

class MissionCameraDatabase final {
 public:
  bool add(MissionCameraDefinition definition);
  bool load_manifest(const std::filesystem::path& manifest);
  const MissionCameraDefinition* find(std::uint32_t mission_id) const noexcept;
 private:
  std::vector<MissionCameraDefinition> cameras_;
};

class MissionManifestLoader final {
 public:
  bool load_paths(const std::filesystem::path& manifest,
                  MissionManifestPaths& paths) const;
  bool load_campaign(const std::filesystem::path& manifest,
                     CampaignProgression& campaign) const;
  bool load_runtime(const std::filesystem::path& manifest,
                    MissionCatalog& catalog, MissionAssetDatabase& assets,
                    MissionLaunchDatabase& launches) const;
  bool load_runtime(const std::filesystem::path& manifest,
                    MissionCatalog& catalog, MissionAssetDatabase& assets,
                    MissionLaunchDatabase& launches,
                    MissionRuntimeServices& services) const;
  bool load_input(const std::filesystem::path& manifest,
                  InputMappingDatabase& input) const;
  bool load_camera(const std::filesystem::path& manifest,
                   MissionCameraDatabase& cameras) const;
  bool load_render(const std::filesystem::path& manifest,
                   MissionRenderDatabase& render, MissionDrawableDatabase& drawables,
                   MissionTransformDatabase& transforms, MissionMaterialDatabase& materials,
                   MissionTextureDatabase& textures, ShaderPermutationDatabase& shaders,
                   MissionRenderTargetDatabase& targets, MissionRenderPassDatabase& passes,
                   MissionRenderResolveDatabase& resolves, QualifiedBufferDatabase& buffers) const;
  bool load_render(const std::filesystem::path& manifest,
                   MissionRenderDatabase& render, MissionDrawableDatabase& drawables,
                   MissionTransformDatabase& transforms, MissionMaterialDatabase& materials,
                   MissionTextureDatabase& textures, ShaderPermutationDatabase& shaders,
                   MissionRenderTargetDatabase& targets, MissionRenderPassDatabase& passes,
                   MissionRenderResolveDatabase& resolves, QualifiedBufferDatabase& buffers,
                   NativeGeometryDatabase& geometries) const;
};

bool configure_mission_launch(const MissionLaunchDefinition& launch, UnitRegistry& units,
                              MissionScenario& scenario) noexcept;

struct MissionRenderDefinition {
  std::uint32_t mission_id{};
  std::vector<AssetId> asset_ids;
};

class MissionRenderDatabase final {
 public:
  bool add(MissionRenderDefinition definition);
  bool load_manifest(const std::filesystem::path& manifest);
  const MissionRenderDefinition* find(std::uint32_t mission_id) const noexcept;
  const std::unordered_map<std::uint32_t, MissionRenderDefinition>& definitions() const noexcept {
    return renders_;
  }

 private:
  std::unordered_map<std::uint32_t, MissionRenderDefinition> renders_;
};

struct MissionDrawable {
  std::uint32_t mission_id{};
  std::string stable_id;
  std::string kind;
  AssetId asset{};
  std::uint32_t primitive_count{};
  std::string buffer_id;
  std::uint32_t vertex_count{};
  std::uint32_t index_count{};
  std::string content_hash;
  bool has_buffer_contract() const noexcept {
    return !buffer_id.empty() && vertex_count != 0 && index_count != 0 && !content_hash.empty();
  }
};

class MissionDrawableDatabase final {
 public:
  bool add(MissionDrawable drawable);
  bool load_manifest(const std::filesystem::path& manifest);
  const MissionDrawable* find(std::uint32_t mission_id, const std::string& stable_id) const noexcept;
  std::vector<const MissionDrawable*> find_by_asset(std::uint32_t mission_id,
                                                    AssetId asset) const;

 private:
  std::vector<MissionDrawable> drawables_;
};

struct MissionDrawableTransform {
  std::uint32_t mission_id{};
  std::string stable_id;
  float translate_x{};
  float translate_y{};
  float translate_z{};
  float scale_x{1.0f};
  float scale_y{1.0f};
  float scale_z{1.0f};
  bool valid() const noexcept;
};

class MissionTransformDatabase final {
 public:
  bool add(MissionDrawableTransform transform);
  bool load_manifest(const std::filesystem::path& manifest);
  const MissionDrawableTransform* find(std::uint32_t mission_id,
                                       const std::string& stable_id) const noexcept;

 private:
  std::vector<MissionDrawableTransform> transforms_;
};

struct MissionMaterial {
  std::uint32_t mission_id{};
  std::string stable_id;
  std::string shader_permutation;
  bool depth_test{true};
  bool depth_write{true};
  std::string blend_mode;
  std::uint32_t base_color{0xFFFFFFFFu};
  std::uint64_t mate_id{};
  bool valid() const noexcept;
};

class MissionMaterialDatabase final {
 public:
  bool add(MissionMaterial material);
  bool load_manifest(const std::filesystem::path& manifest);
  const MissionMaterial* find(std::uint32_t mission_id,
                              const std::string& stable_id) const noexcept;

 private:
  std::vector<MissionMaterial> materials_;
};

struct MissionTextureBinding {
  std::uint32_t mission_id{};
  std::string stable_id;
  std::string texture_id;
  std::string sampler_filter;
  std::string sampler_address;
  std::uint64_t content_hash{};
  std::filesystem::path source_path;
  std::uint64_t source_size{};
  std::uint32_t source_width{};
  std::uint32_t source_height{};
  std::uint32_t source_format{};
  std::uint64_t gidx{};
  bool valid() const noexcept;
};

class MissionTextureDatabase final {
 public:
  bool add(MissionTextureBinding texture);
  bool load_manifest(const std::filesystem::path& manifest);
  const MissionTextureBinding* find(std::uint32_t mission_id,
                                    const std::string& stable_id) const noexcept;
  bool sample(std::uint32_t mission_id, const std::string& stable_id,
              float u, float v, std::uint32_t& rgba) const noexcept;

 private:
  struct Image { std::uint32_t width{}, height{}; std::vector<std::uint32_t> pixels; };
  std::vector<MissionTextureBinding> textures_;
  std::unordered_map<std::string, Image> images_;
};

struct ShaderPermutation {
  std::string id;
  std::string vertex_layout;
  std::uint32_t texture_fetches{};
  std::uint32_t constant_count{};
  std::string render_target_format;
  bool valid() const noexcept;
};

class ShaderPermutationDatabase final {
 public:
  bool add(ShaderPermutation permutation);
  bool load_manifest(const std::filesystem::path& manifest);
  const ShaderPermutation* find(const std::string& id) const noexcept;

 private:
  std::vector<ShaderPermutation> permutations_;
};

struct MissionRenderTargetDefinition {
  std::uint32_t mission_id{};
  std::string target_id;
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t sample_count{1};
  std::string color_format;
  std::string depth_format;
  bool depth_enabled{true};
  bool valid() const noexcept;
};

class MissionRenderTargetDatabase final {
 public:
  bool add(MissionRenderTargetDefinition definition);
  bool load_manifest(const std::filesystem::path& manifest);
  const MissionRenderTargetDefinition* find(std::uint32_t mission_id) const noexcept;
  const MissionRenderTargetDefinition* find(std::uint32_t mission_id,
                                            const std::string& target_id) const noexcept;

 private:
  std::vector<MissionRenderTargetDefinition> targets_;
};

struct MissionRenderPass {
  std::uint32_t mission_id{};
  std::string pass_id;
  std::uint32_t order{};
  std::string color_target;
  std::string depth_target;
  std::uint32_t clear_color{};
  float clear_depth{1.0f};
  bool valid() const noexcept;
};

class MissionRenderPassDatabase final {
 public:
  bool add(MissionRenderPass pass);
  bool load_manifest(const std::filesystem::path& manifest);
  const MissionRenderPass* find(std::uint32_t mission_id,
                                const std::string& pass_id) const noexcept;

 private:
  std::vector<MissionRenderPass> passes_;
};

struct MissionRenderResolve {
  std::uint32_t mission_id{};
  std::string source_pass;
  std::string source_target;
  std::string destination_target;
  std::string mode;
  bool valid() const noexcept;
};

class MissionRenderResolveDatabase final {
 public:
  bool add(MissionRenderResolve resolve);
  bool load_manifest(const std::filesystem::path& manifest);
  const MissionRenderResolve* find(std::uint32_t mission_id,
                                   const std::string& source_pass) const noexcept;

 private:
  std::vector<MissionRenderResolve> resolves_;
};

struct QualifiedBufferRecord {
  std::string buffer_id;
  std::filesystem::path path;
  std::uint64_t byte_size{};
  std::uint64_t fnv64{};
  bool verified{};
};

class QualifiedBufferDatabase final {
 public:
  bool add(QualifiedBufferRecord record);
  bool load_manifest(const std::filesystem::path& manifest);
  bool verify(const std::string& buffer_id);
  bool has_verified(const std::string& buffer_id) const noexcept;
  const QualifiedBufferRecord* find(const std::string& buffer_id) const noexcept;

 private:
  std::vector<QualifiedBufferRecord> buffers_;
};

enum class NativeIndexTopology : std::uint8_t {
  TriangleList,
  TriangleStripRestart,
};

struct NativeGeometryMetadata {
  std::string buffer_id;
  std::string source_format;
  NativeIndexTopology topology{NativeIndexTopology::TriangleList};
  std::uint32_t vertex_count{};
  std::uint32_t index_count{};
  std::uint32_t primitive_count{};
  std::uint32_t vertex_section_count{};
  std::uint32_t index_section_count{};
  std::uint32_t polygon_descriptor_count{};
  std::uint32_t vertex_stride{};
  std::uint32_t index_size{};
  std::uint64_t vertex_byte_size{};
  std::uint64_t index_byte_size{};
};

struct DecodedVertex {
  float x{};
  float y{};
  float z{};
  float u{};
  float v{};
};

struct DecodedGeometryBounds {
  float min_x{};
  float min_y{};
  float min_z{};
  float max_x{};
  float max_y{};
  float max_z{};
  bool valid{};
};

struct DecodedGeometry {
  std::string buffer_id;
  std::vector<DecodedVertex> vertices;
  std::vector<std::uint32_t> indices;
  DecodedGeometryBounds bounds;
};

class NativeGeometryDatabase final {
 public:
  bool load_verified(const MissionDrawable& drawable, const QualifiedBufferDatabase& buffers);
  const NativeGeometryMetadata* find(const std::string& buffer_id) const noexcept;
  const DecodedGeometry* decoded(const std::string& buffer_id) const noexcept;

 private:
  std::vector<NativeGeometryMetadata> geometries_;
  std::vector<DecodedGeometry> decoded_;
};

class ReplayLog;

struct RenderReadback {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t color_coverage{};
  std::uint32_t depth_coverage{};
  std::uint64_t color_hash{};
  std::uint64_t depth_hash{};
  bool has_world_coverage() const noexcept {
    return width != 0 && height != 0 && color_coverage != 0 && depth_coverage != 0;
  }
};

class NativeRenderTarget final {
 public:
  bool resize(std::uint32_t width, std::uint32_t height);
  bool clear(std::uint32_t color, float depth);
  bool mark_world_asset(const WorldFrame& frame, AssetId asset, std::uint32_t ordinal) noexcept;
  bool draw_world_asset(const WorldFrame& frame, const MissionDrawable& drawable,
                        std::uint32_t ordinal) noexcept;
  bool draw_world_geometry(const WorldFrame& frame, const MissionDrawable& drawable,
                           const NativeGeometryMetadata& geometry,
                           const DecodedGeometry& decoded,
                           const MissionDrawableTransform& transform,
                           const MissionMaterial& material,
                           const MissionTextureBinding& texture,
                           const ShaderPermutation& shader,
                           const MissionRenderTargetDefinition& render_target,
                           const MissionRenderTargetDefinition& destination_target,
                           const MissionRenderPass& pass,
                           const MissionRenderResolve& resolve,
                           const MissionCameraDefinition* camera,
                           const MissionTextureDatabase* texture_database,
                           std::uint32_t ordinal) noexcept;
  RenderReadback readback() const noexcept;
  bool copy_rgba8(std::vector<std::uint8_t>& pixels) const;
  bool copy_depth(std::vector<float>& depth) const;
  // Verification-only export; the product runtime does not present this file.
  bool write_ppm(const std::filesystem::path& path) const noexcept;
  // Verification-only export of the native depth plane as little-endian f32.
  bool write_depth_f32(const std::filesystem::path& path) const noexcept;
  std::uint32_t width() const noexcept { return width_; }
  std::uint32_t height() const noexcept { return height_; }
  std::uint32_t geometry_calls() const noexcept { return geometry_calls_; }
  std::uint32_t raster_triangles() const noexcept { return raster_triangles_; }
  std::uint64_t raster_writes() const noexcept { return raster_writes_; }

 private:
  std::uint32_t width_{};
  std::uint32_t height_{};
  std::vector<std::uint32_t> color_;
  std::vector<float> depth_;
  std::uint32_t geometry_calls_{};
  std::uint32_t raster_triangles_{};
  std::uint64_t raster_writes_{};
};

class MissionRuntime final {
 public:
  MissionRuntime(std::uint32_t mission_id, const MissionAssetDatabase* assets = nullptr);
  MissionRuntime(const MissionDefinition& definition, const MissionAssetDatabase* assets = nullptr);
  WorldFrame tick(float fixed_dt, InputFrame input);
  WorldFrame run_replay(float fixed_dt, const ReplayLog& replay);
  RuntimeSnapshot snapshot() const noexcept;
  bool restore(RuntimeSnapshot snapshot) noexcept;
  bool set_definition(const MissionDefinition* definition) noexcept;
  void set_scenario(const MissionScenario* scenario) noexcept { scenario_ = scenario; }
  void set_units(const UnitRegistry* units) noexcept { units_ = units; }
  std::uint32_t mission_id() const noexcept { return mission_id_; }

 private:
  std::uint32_t mission_id_;
  const MissionAssetDatabase* assets_;
  const MissionDefinition* definition_{};
  const MissionScenario* scenario_{};
  const UnitRegistry* units_{};
  std::uint64_t tick_{};
  float position_x_{};
  float position_y_{};
  float position_z_{};
  float pitch_{};
  float roll_{};
  float yaw_{};
  float fixed_accumulator_{};
};

class MissionExecution final {
 public:
  MissionExecution(const MissionDefinition& definition,
                   const MissionAssetDatabase* assets = nullptr,
                   const MissionObjectiveDatabase* objectives = nullptr,
                   const RadioMessageDatabase* radios = nullptr,
                   CampaignProgression* campaign = nullptr,
                   MissionWaveDirector* waves = nullptr,
                   MissionSequenceDirector* sequence = nullptr,
                   const InputMappingDatabase* input = nullptr,
                   MissionAiDirector* ai = nullptr);
  bool launch(const MissionLaunchDefinition& launch) noexcept;
  bool dispatch(Event event) noexcept;
  bool activate_objective(std::uint32_t id) noexcept;
  bool complete_objective(std::uint32_t id) noexcept;
  bool fail_objective(std::uint32_t id) noexcept;
  bool dispatch_radio(std::uint32_t id) noexcept;
  bool play_radio(std::uint32_t id, float duration_seconds) noexcept;
  bool lock_target(EntityId target) noexcept;
  bool fire_weapon(std::uint32_t weapon_id) noexcept;
  void set_failure_tick(std::uint64_t tick) noexcept { failure_tick_ = tick; }
  std::uint64_t failure_tick() const noexcept { return failure_tick_; }
  WorldFrame tick(float fixed_dt, InputFrame input) noexcept;
  WorldFrame run_replay(float fixed_dt, const ReplayLog& replay) noexcept;
  RuntimeSnapshot snapshot() const noexcept;
  bool restore(RuntimeSnapshot snapshot) noexcept;
  struct Checkpoint {
    std::uint32_t mission_id{};
    RuntimeSnapshot flight;
    MissionScenarioSnapshot scenario;
    std::vector<CombatUnitState> combat_units;
    std::vector<AssetRecord> resource_identities;
    std::uint64_t failure_tick{};
    MissionSequenceSnapshot sequence;
    RadioPlaybackSnapshot radio_playback;
    bool operator==(const Checkpoint&) const = default;
  };
  bool save_checkpoint(Checkpoint& checkpoint) const noexcept;
  bool restore_checkpoint(const Checkpoint& checkpoint) noexcept;
  MissionDebrief debrief() const;
  bool launched() const noexcept { return launched_; }
  const MissionScenario& scenario() const noexcept { return scenario_; }
  UnitRegistry& units() noexcept { return units_; }
  const UnitRegistry& units() const noexcept { return units_; }
  CombatWorld& combat() noexcept { return combat_; }
  const CombatWorld& combat() const noexcept { return combat_; }
  RadioPlaybackService& radio() noexcept { return radio_; }
  const RadioPlaybackService& radio() const noexcept { return radio_; }

 private:
  const MissionDefinition* definition_{};
  const MissionAssetDatabase* assets_{};
  const MissionObjectiveDatabase* objectives_{};
  const RadioMessageDatabase* radios_{};
  CampaignProgression* campaign_{};
  MissionWaveDirector* waves_{};
  MissionSequenceDirector* sequence_{};
  const InputMappingDatabase* input_{};
  MissionAiDirector* ai_{};
  MissionRuntime runtime_;
  MissionScenario scenario_;
  UnitRegistry units_;
  CombatWorld combat_;
  RadioPlaybackService radio_;
  std::uint64_t failure_tick_{};
  bool launched_{};
};

class VulkanRenderer final {
 public:
  struct RenderAssets {
    const MissionAssetDatabase* database{};
    const MissionRenderDefinition* definition{};
    const MissionDrawableDatabase* drawables{};
    const QualifiedBufferDatabase* buffers{};
    const NativeGeometryDatabase* geometries{};
    const MissionTransformDatabase* transforms{};
    const MissionMaterialDatabase* materials{};
    const MissionTextureDatabase* textures{};
    const ShaderPermutationDatabase* shaders{};
    const MissionRenderTargetDatabase* render_targets{};
    const MissionRenderPassDatabase* render_passes{};
    const MissionRenderResolveDatabase* render_resolves{};
    const MissionCameraDefinition* camera{};
    bool has(AssetId id) const noexcept {
      return database != nullptr && database->resolve(id) != nullptr;
    }
    bool ready_for(const WorldFrame& frame) const noexcept {
      if (definition == nullptr || definition->mission_id != frame.mission_id ||
          definition->asset_ids.empty()) {
        return false;
      }
      for (const AssetId id : definition->asset_ids) {
        if (!has(id)) return false;
        if (drawables != nullptr && drawables->find_by_asset(frame.mission_id, id).empty()) {
          return false;
        }
        if (drawables != nullptr && buffers != nullptr) {
          for (const MissionDrawable* drawable : drawables->find_by_asset(frame.mission_id, id)) {
            if (drawable == nullptr || !buffers->has_verified(drawable->buffer_id)) return false;
            if (geometries != nullptr && geometries->find(drawable->buffer_id) == nullptr) {
              return false;
            }
            if (geometries != nullptr && geometries->decoded(drawable->buffer_id) == nullptr) {
              return false;
            }
            if (geometries != nullptr &&
                (transforms == nullptr ||
                 transforms->find(frame.mission_id, drawable->stable_id) == nullptr)) {
              return false;
            }
            if (geometries != nullptr &&
                (materials == nullptr ||
                 materials->find(frame.mission_id, drawable->stable_id) == nullptr)) {
              return false;
            }
            if (geometries != nullptr &&
                (textures == nullptr ||
                 textures->find(frame.mission_id, drawable->stable_id) == nullptr)) {
              return false;
            }
            if (geometries != nullptr) {
              const MissionMaterial* material =
                  materials == nullptr ? nullptr : materials->find(frame.mission_id, drawable->stable_id);
              if (material == nullptr || shaders == nullptr ||
                  shaders->find(material->shader_permutation) == nullptr) {
                return false;
              }
            }
            if (geometries != nullptr &&
                (render_passes == nullptr || render_passes->find(frame.mission_id, "world") == nullptr ||
                 render_targets == nullptr ||
                 render_targets->find(frame.mission_id,
                                      render_passes->find(frame.mission_id, "world")->color_target) == nullptr)) {
              return false;
            }
            if (geometries != nullptr &&
                (render_passes == nullptr || render_passes->find(frame.mission_id, "world") == nullptr)) {
              return false;
            }
            if (geometries != nullptr &&
                (render_resolves == nullptr ||
                 render_resolves->find(frame.mission_id, "world") == nullptr)) {
              return false;
            }
          }
        }
      }
      return true;
    }
  };

  bool render(const WorldFrame& frame, RenderAssets assets, NativeRenderTarget* target = nullptr) noexcept {
    if (!frame.mission_ready || frame.active_units == 0 || frame.player_entity == 0 ||
        !assets.ready_for(frame)) return false;
    if (target != nullptr) {
      for (std::uint32_t i = 0; i < assets.definition->asset_ids.size(); ++i) {
        const AssetId asset = assets.definition->asset_ids[i];
        if (assets.drawables != nullptr) {
          const auto drawables = assets.drawables->find_by_asset(frame.mission_id, asset);
          if (drawables.empty()) return false;
          for (std::uint32_t j = 0; j < drawables.size(); ++j) {
            if (assets.geometries != nullptr) {
              const NativeGeometryMetadata* geometry = assets.geometries->find(drawables[j]->buffer_id);
              const DecodedGeometry* decoded = assets.geometries->decoded(drawables[j]->buffer_id);
              const MissionDrawableTransform* transform =
                  assets.transforms == nullptr ? nullptr :
                      assets.transforms->find(frame.mission_id, drawables[j]->stable_id);
              const MissionMaterial* material =
                  assets.materials == nullptr ? nullptr :
                      assets.materials->find(frame.mission_id, drawables[j]->stable_id);
              const MissionTextureBinding* texture =
                  assets.textures == nullptr ? nullptr :
                      assets.textures->find(frame.mission_id, drawables[j]->stable_id);
              const ShaderPermutation* shader =
                  material == nullptr || assets.shaders == nullptr ? nullptr :
                      assets.shaders->find(material->shader_permutation);
              const MissionRenderTargetDefinition* render_target =
                  assets.render_targets == nullptr ? nullptr :
                      assets.render_targets->find(frame.mission_id);
              const MissionRenderPass* pass =
                  assets.render_passes == nullptr ? nullptr :
                      assets.render_passes->find(frame.mission_id, "world");
              const MissionRenderResolve* resolve =
                  assets.render_resolves == nullptr ? nullptr :
                      assets.render_resolves->find(frame.mission_id, "world");
              if (geometry == nullptr || decoded == nullptr || transform == nullptr ||
                  material == nullptr || texture == nullptr || shader == nullptr ||
                  pass == nullptr || resolve == nullptr) {
                return false;
              }
              render_target = assets.render_targets == nullptr ? nullptr :
                  assets.render_targets->find(frame.mission_id, pass->color_target);
              const MissionRenderTargetDefinition* destination_target =
                  assets.render_targets == nullptr ? nullptr :
                      assets.render_targets->find(frame.mission_id, resolve->destination_target);
              if (render_target == nullptr || destination_target == nullptr ||
                  !target->draw_world_geometry(frame, *drawables[j], *geometry, *decoded,
                                               *transform, *material, *texture, *shader, *render_target,
                                               *destination_target, *pass, *resolve,
                                               assets.camera,
                                               assets.textures,
                                               i * 4096u + j)) {
                return false;
              }
            } else if (!target->draw_world_asset(frame, *drawables[j], i * 4096u + j)) {
              return false;
            }
          }
        } else if (!target->mark_world_asset(frame, asset, i)) {
          return false;
        }
      }
    }
    ++submitted_frames_;
    last_world_asset_count_ = static_cast<std::uint32_t>(assets.definition->asset_ids.size());
    world_asset_submissions_ += last_world_asset_count_;
    return true;
  }
  std::uint64_t submitted_frames() const noexcept { return submitted_frames_; }
  std::uint32_t last_world_asset_count() const noexcept { return last_world_asset_count_; }
  std::uint64_t world_asset_submissions() const noexcept { return world_asset_submissions_; }

 private:
  std::uint64_t submitted_frames_{};
  std::uint32_t last_world_asset_count_{};
  std::uint64_t world_asset_submissions_{};
};

enum class FrontendState : std::uint8_t { Title, NewGame, Briefing, Hangar, Loading, Mission, Debrief };
enum class FrontendDifficulty : std::uint8_t { Normal, Easy, Hard };
enum class FrontendControls : std::uint8_t { Normal, Expert };
enum class FrontendLanguage : std::uint8_t { English, French, German, Italian, Spanish };

struct FrontendSettings {
  FrontendDifficulty difficulty{FrontendDifficulty::Normal};
  FrontendControls controls{FrontendControls::Normal};
  FrontendLanguage language{FrontendLanguage::English};
  bool valid() const noexcept {
    return difficulty == FrontendDifficulty::Normal && controls == FrontendControls::Normal &&
           language == FrontendLanguage::English;
  }
};

class FrontendController final {
 public:
  FrontendState state() const noexcept { return state_; }
  std::uint32_t selected_mission() const noexcept { return selected_mission_; }
  const FrontendSettings& settings() const noexcept { return settings_; }
  bool configure(FrontendSettings settings) noexcept;
  void set_campaign(CampaignProgression* campaign) noexcept { campaign_ = campaign; }
  bool select_mission(const MissionCatalog& catalog, std::uint32_t mission_id) noexcept;
  bool set_loadout(CampaignLoadout loadout) noexcept;
  const MissionDefinition* mission_definition(const MissionCatalog& catalog) const noexcept;
  bool launch_selected(const MissionCatalog& catalog, const MissionLaunchDatabase& launches,
                       MissionExecution& execution) const noexcept;
  bool enter_debrief(const MissionExecution& execution) noexcept;
  bool return_to_campaign() noexcept;
  const MissionDebrief* debrief() const noexcept {
    return debrief_.has_value() ? &*debrief_ : nullptr;
  }
  bool advance() noexcept;
  bool dispatch(Event event) noexcept;
  bool dispatch_buttons(const InputMappingDatabase& mappings,
                        std::uint16_t buttons) noexcept;

 private:
  FrontendState state_{FrontendState::Title};
  std::uint32_t selected_mission_{};
  FrontendSettings settings_{};
  CampaignProgression* campaign_{};
  std::optional<MissionDebrief> debrief_;
};

class SaveStore final {
 public:
  bool save(std::uint32_t slot, RuntimeSnapshot snapshot);
  const RuntimeSnapshot* load(std::uint32_t slot) const noexcept;
  bool write_file(const std::filesystem::path& path) const;
  bool read_file(const std::filesystem::path& path);

 private:
  std::unordered_map<std::uint32_t, RuntimeSnapshot> slots_;
};

struct SessionSaveSnapshot {
  std::uint32_t mission_id{};
  RuntimeSnapshot flight{};
  CampaignSaveSnapshot campaign;
  std::optional<MissionExecution::Checkpoint> checkpoint;
  bool operator==(const SessionSaveSnapshot&) const = default;
};

class SessionSaveStore final {
 public:
  bool save(std::uint32_t slot, SessionSaveSnapshot snapshot);
  const SessionSaveSnapshot* load(std::uint32_t slot) const noexcept;
  bool write_file(const std::filesystem::path& path) const;
  bool read_file(const std::filesystem::path& path);

 private:
  std::unordered_map<std::uint32_t, SessionSaveSnapshot> slots_;
};

class ReplayLog final {
 public:
  void append(InputFrame input) { frames_.push_back(input); }
  const std::vector<InputFrame>& frames() const noexcept { return frames_; }
  void clear() noexcept { frames_.clear(); }
  bool write_file(const std::filesystem::path& path) const;
  bool read_file(const std::filesystem::path& path);

 private:
  std::vector<InputFrame> frames_;
};

}  // namespace ac6
