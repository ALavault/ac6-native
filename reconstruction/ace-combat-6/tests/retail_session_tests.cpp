// Mission 01 played end to end from the retail container, and ended by its own
// sub-mission script.
//
// Two behaviours are under test and they are deliberately kept apart:
//
//   playable_session   - the product's session loop runs over the world the
//                        retail payload built: input, flight, camera, HUD, for
//                        1800 fixed ticks, twice, identically.
//   mission_completion - the mission reaches its debrief because the script ran
//                        out, and for no other reason.
//
// The second is the one that can be faked, so it is checked three ways: the
// executed step trace must be exactly what the payload's step counts imply; the
// trace must not depend on the cadence at which the script is advanced; and a
// session whose script is never advanced must still be in gameplay at tick 1800
// with nothing completed.
//
// usage: retail-session-tests PAYLOAD [REPORT_JSON] [CAPTURE_DIR] [RETAIL_CACHE]
// exit 77 means the retail payload was absent; it is never committed.

#include "ac6/native_hud.h"
#include "ac6/retail_camera_table.h"
#include "ac6/retail_campaign_bundle.h"
#include "ac6/retail_session.h"
#include "test_fixtures.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <set>
#include <tuple>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace {

constexpr std::uint32_t kMissionId = 1;
constexpr std::size_t kTicks = 1800;
constexpr float kFixedDt = 1.0f / 60.0f;

using ac6::retail::RetailSession;
using ac6::retail::ScriptAdvance;
using ac6::retail::ScriptStepRun;

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const std::string text = buffer.str();
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

class TempStoreRoot final {
 public:
  TempStoreRoot() {
    static std::atomic<unsigned> next{};
    path_ = std::filesystem::temp_directory_path() /
            ("ac6-retail-session-store-" + std::to_string(::getpid()) + "-" +
             std::to_string(next++));
    REQUIRE(std::filesystem::create_directories(path_));
  }
  ~TempStoreRoot() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

void write_file(const std::filesystem::path& path,
                std::span<const std::uint8_t> bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  REQUIRE(static_cast<bool>(output));
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  REQUIRE(static_cast<bool>(output));
}

void put_be32(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value >> 24u);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16u);
  bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8u);
  bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

void put_be16(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value >> 8u);
  bytes[offset + 1] = static_cast<std::uint8_t>(value);
}

ac6::RetailIdentityPolicy make_store_source(
    const std::filesystem::path& source,
    const std::vector<std::uint8_t>& payload, bool invalid_child_range = false) {
  REQUIRE(payload.size() <= UINT32_MAX - 48u);
  REQUIRE(std::filesystem::create_directories(source));
  const std::vector<std::uint8_t> xex{'X', 'E', 'X', '2'};
  const std::vector<std::uint8_t> data01{'P'};
  std::vector<std::uint8_t> campaign_payload(48u + payload.size(), 0);
  const std::array<std::uint8_t, 4> fhm{'F', 'H', 'M', ' '};
  std::copy(fhm.begin(), fhm.end(), campaign_payload.begin());
  campaign_payload[4] = 1;
  campaign_payload[5] = 1;
  put_be16(campaign_payload, 6, 0x10);
  put_be32(campaign_payload, 0x10, 1);
  put_be32(campaign_payload, 0x14,
           invalid_child_range ? UINT32_MAX - 15u : 48u);
  put_be32(campaign_payload, 0x18,
           static_cast<std::uint32_t>(payload.size()));
  std::copy(payload.begin(), payload.end(), campaign_payload.begin() + 48);

  std::vector<std::uint8_t> data00(9u + campaign_payload.size());
  for (std::size_t index = 0; index < 9; ++index) {
    data00[index] = static_cast<std::uint8_t>(index);
  }
  std::copy(campaign_payload.begin(), campaign_payload.end(), data00.begin() + 9);
  ac6::retail_mode1_xor(std::span<std::uint8_t>(data00).subspan(9), 9);

  std::vector<std::uint8_t> table(8u + 10u * 16u, 0);
  put_be32(table, 0, 10);
  put_be32(table, 4, 2);
  for (std::uint32_t index = 0; index < 9; ++index) {
    const std::size_t row = 8u + index * 16u;
    put_be32(table, row, 0x00020000u);
    put_be32(table, row + 4, index);
    put_be32(table, row + 8, 1);
    put_be32(table, row + 12, 1);
  }
  const std::size_t mission_row = 8u + 9u * 16u;
  put_be32(table, mission_row, 0x00020000u);
  put_be32(table, mission_row + 4, 9);
  put_be32(table, mission_row + 8,
           static_cast<std::uint32_t>(campaign_payload.size()));
  put_be32(table, mission_row + 12,
           static_cast<std::uint32_t>(campaign_payload.size()));

  write_file(source / "default.xex", xex);
  write_file(source / "DATA.TBL", table);
  write_file(source / "DATA00.PAC", data00);
  write_file(source / "DATA01.PAC", data01);

  ac6::RetailIdentityPolicy policy;
  policy.data_table_entries = 10;
  policy.pack_count = 2;
  policy.identity.xex_size = xex.size();
  policy.identity.data_table_size = table.size();
  policy.identity.data00_size = data00.size();
  policy.identity.data01_size = data01.size();
  REQUIRE(ac6::sha256_file(source / "default.xex", policy.identity.xex_sha256));
  REQUIRE(ac6::sha256_file(source / "DATA.TBL", policy.identity.data_table_sha256));
  REQUIRE(ac6::sha256_file(source / "DATA00.PAC", policy.identity.data00_sha256));
  REQUIRE(ac6::sha256_file(source / "DATA01.PAC", policy.identity.data01_sha256));
  return policy;
}

void check_store_backed_session(const std::vector<std::uint8_t>& payload) {
  TempStoreRoot root;
  const std::filesystem::path source = root.path() / "source";
  const std::filesystem::path cache = root.path() / "cache";
  const ac6::RetailIdentityPolicy policy = make_store_source(source, payload);
  const std::array<std::uint32_t, 1> selected{9};
  const ac6::RetailImportReport imported =
      ac6::RetailContentImporter(policy).run(source, cache, selected);
  REQUIRE(imported.passed());
  REQUIRE(imported.imported_records == 1);

  ac6::RetailContentStore store(policy);
  REQUIRE(store.open(cache));
  const std::optional<ac6::retail::RetailCampaignBundle> bundle =
      ac6::retail::RetailCampaignBundle::open(store, kMissionId);
  REQUIRE(bundle.has_value());
  REQUIRE(bundle->child_count() == 1);
  REQUIRE(bundle->child(0).has_value());
  REQUIRE(bundle->child(0)->size() == payload.size());
  const ac6::CampaignLoadout loadout{1, 1, true};
  const ac6::retail::RetailMissionBundleConfig mission_config{
      kMissionId, ac6::retail::RetailDifficulty::Ace, loadout};
  const std::optional<ac6::retail::RetailMissionBundle> mission =
      ac6::retail::RetailMissionBundle::open(store, mission_config);
  REQUIRE(mission.has_value());
  REQUIRE(mission->difficulty() == ac6::retail::RetailDifficulty::Ace);
  REQUIRE(mission->loadout() == loadout);
  REQUIRE(!ac6::retail::RetailMissionBundle::open(
               store, {kMissionId, static_cast<ac6::retail::RetailDifficulty>(5),
                       loadout})
               .has_value());
  std::unique_ptr<RetailSession> session =
      RetailSession::open(store, loadout,
                          {kMissionId, {0, 0},
                           ac6::retail::kRetailOpeningCameraModeWord,
                           ac6::retail::RetailDifficulty::Ace});
  REQUIRE(session != nullptr);
  REQUIRE(session->bundle().has_value());
  REQUIRE(session->bundle()->data_table_entry == 9);
  REQUIRE(session->bundle()->loadout == loadout);
  REQUIRE(session->bundle()->difficulty == ac6::retail::RetailDifficulty::Ace);
  REQUIRE(session->bundle()->content_index_sha256 == store.index_sha256());
  REQUIRE(RetailSession::open(store, {1, 1, false}, {kMissionId, {0, 0}}) ==
          nullptr);
  REQUIRE(RetailSession::open(store, loadout, {0, {0, 0}}) == nullptr);
  REQUIRE(RetailSession::open(store, loadout, {16, {0, 0}}) == nullptr);
  REQUIRE(RetailSession::open(store, loadout, {kMissionId, {0, 0}, 4}) == nullptr);

  TempStoreRoot invalid_root;
  const std::filesystem::path invalid_source = invalid_root.path() / "source";
  const std::filesystem::path invalid_cache = invalid_root.path() / "cache";
  const ac6::RetailIdentityPolicy invalid_policy =
      make_store_source(invalid_source, payload, true);
  REQUIRE(ac6::RetailContentImporter(invalid_policy)
              .run(invalid_source, invalid_cache, selected)
              .passed());
  ac6::RetailContentStore invalid_store(invalid_policy);
  REQUIRE(invalid_store.open(invalid_cache));
  REQUIRE(!ac6::retail::RetailCampaignBundle::open(invalid_store, kMissionId)
               .has_value());
  REQUIRE(RetailSession::open(invalid_store, loadout,
                              {kMissionId, {0, 0}}) == nullptr);
}

void check_qualified_store_backed_session(const std::filesystem::path& cache) {
  ac6::RetailContentStore store;
  REQUIRE(store.open(cache));
  REQUIRE(ac6::sha256_hex(store.index_sha256()) ==
          "cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85");
  for (std::uint32_t mission_id = 1; mission_id <= 15; ++mission_id) {
    const std::optional<ac6::retail::RetailCampaignBundle> bundle =
        ac6::retail::RetailCampaignBundle::open(store, mission_id);
    REQUIRE(bundle.has_value());
    REQUIRE(bundle->child_count() == 26);
    REQUIRE(bundle->child(0).has_value());
    const std::optional<std::span<const std::uint8_t>> mdlp = bundle->child(1);
    REQUIRE(mdlp.has_value());
    REQUIRE(mdlp->size() >= 4);
    REQUIRE((*mdlp)[0] == 'M' && (*mdlp)[1] == 'D' &&
            (*mdlp)[2] == 'L' && (*mdlp)[3] == 'P');

    // The common scenario reader is exercised over every qualified campaign
    // payload before any mission-specific product code consumes it.
    const std::optional<ac6::retail::RetailMissionBundle> mission =
        ac6::retail::RetailMissionBundle::open(
            store, {mission_id, ac6::retail::RetailDifficulty::Normal,
                    {1, 1, true}});
    REQUIRE(mission.has_value());
    const std::optional<std::span<const std::uint8_t>> scenario_bytes = mission->child(0);
    REQUIRE(scenario_bytes.has_value());
    std::vector<std::uint8_t> scenario_copy(scenario_bytes->begin(), scenario_bytes->end());
    const std::optional<ac6::retail::ScenarioPayload> scenario_payload =
        ac6::retail::ScenarioPayload::open(std::move(scenario_copy));
    REQUIRE(scenario_payload.has_value());
    REQUIRE(ac6::retail::MissionScenario::parse(*scenario_payload).has_value());
  }
  const std::optional<ac6::retail::RetailCampaignBundle> common =
      ac6::retail::RetailCampaignBundle::open_entry(store, 1);
  REQUIRE(common.has_value());
  REQUIRE(common->child_count() == 55);
  REQUIRE(common->child(35).has_value());
  REQUIRE(common->child(35)->size() == 5184);
  REQUIRE(common->child(36).has_value());
  REQUIRE(common->child(36)->size() == 6480);
  const std::optional<ac6::retail::RetailCameraTable> cameras =
      ac6::retail::RetailCameraTable::open(*common);
  REQUIRE(cameras.has_value());
  const ac6::retail::RetailCameraRecord* first =
      cameras->record_for_loadout({1, 1, true}, 1);
  REQUIRE(first != nullptr);
  const std::optional<std::array<float, 4>> first_offset = first->offset(0);
  REQUIRE(first_offset.has_value());
  REQUIRE((*first_offset)[0] == 0.0F && (*first_offset)[1] == 3.0F &&
          (*first_offset)[2] == 15.0F);
  REQUIRE(first->ease_rate() == 7.0F);
  REQUIRE(first->fov_radians() == 0.6632251143455505F);
  REQUIRE(first->alternate_fov_radians() == 0.8028514385223389F);
  REQUIRE(cameras->record_for_loadout({15, 1, true}, 3) ==
          cameras->record(14, 3));
  const std::optional<ac6::retail::RetailCampaignBundle> world =
      ac6::retail::RetailCampaignBundle::open_entry(store, 119);
  REQUIRE(world.has_value());
  REQUIRE(world->mission_id() == 0);
  REQUIRE(world->data_table_entry() == 119);
  REQUIRE(world->child_count() == 23);
  for (const std::uint32_t index : {21u, 22u}) {
    const std::optional<std::span<const std::uint8_t>> child =
        world->child(index);
    REQUIRE(child.has_value());
    REQUIRE(child->size() >= 4);
    REQUIRE((*child)[0] == 'F' && (*child)[1] == 'H' &&
            (*child)[2] == 'M' && (*child)[3] == ' ');
  }
  const ac6::CampaignLoadout loadout{1, 1, true};
  std::unique_ptr<RetailSession> session =
      RetailSession::open(store, loadout, {kMissionId, {0, 0}});
  REQUIRE(session != nullptr);
  REQUIRE(session->bundle().has_value());
  REQUIRE(session->bundle()->data_table_entry == 9);
  REQUIRE(session->camera_mode().raw_mode == 0 &&
          session->camera_mode().view_mode == 1);
  REQUIRE(session->world().published == 230);
  REQUIRE(session->scenario().sub_missions().size() == 4);
}

// The same input every run: the session must be a function of the payload and
// this stream, and of nothing else.
ac6::InputFrame session_input(std::size_t tick) noexcept {
  ac6::InputFrame input{};
  input.pitch = static_cast<std::int16_t>((tick % 120u < 60u) ? 4096 : -2048);
  input.roll = static_cast<std::int16_t>((tick % 180u < 90u) ? 1024 : -1024);
  input.yaw = static_cast<std::int16_t>((tick % 240u < 120u) ? 768 : -768);
  input.throttle = static_cast<std::uint8_t>(160u + (tick % 64u));
  return input;
}

// A second stream, so that "the session takes input" is a measured claim and
// not an assertion about a field nobody read.
ac6::InputFrame mirrored_input(std::size_t tick) noexcept {
  ac6::InputFrame input = session_input(tick);
  input.pitch = static_cast<std::int16_t>(-input.pitch);
  input.roll = static_cast<std::int16_t>(-input.roll);
  return input;
}

std::uint64_t frame_hash(const ac6::retail::RetailSessionFrame& frame) {
  char row[256];
  std::snprintf(row, sizeof(row), "%llu:%u:%u:%u:%d:%.5f:%.5f:%.5f:%.5f:%.5f:%.5f:%.5f:%u:%u:%u:%u:%d:%d",
                static_cast<unsigned long long>(frame.world.tick), frame.world.mission_id,
                frame.camera_mode.raw_mode, frame.camera_mode.view_mode,
                frame.world.mission_ready ? 1 : 0, frame.world.position_x, frame.world.position_y,
                frame.world.position_z, frame.world.camera_x, frame.world.camera_y,
                frame.world.camera_z, frame.world.speed, frame.world.active_units,
                frame.world.player_entity, frame.sub_mission, frame.step,
                frame.script_ended ? 1 : 0, frame.player_inside_area ? 1 : 0);
  return ::ac6_test::fnv64(row);
}

struct SessionRun {
  std::uint64_t hash{};
  std::vector<ScriptStepRun> trace;
  std::size_t advances{};
  std::size_t ended_at_tick{};
  ac6::MissionDebrief debrief;
  ac6::ScenarioState state{};
  ac6::retail::RetailSessionFrame last;
};

// One full session. `cadence` is how many ticks pass between two calls of the
// script advance; zero means the script is never advanced at all.
SessionRun run_session(const std::vector<std::uint8_t>& payload, std::size_t cadence,
                       ac6::InputFrame (*input)(std::size_t) = session_input) {
  std::unique_ptr<RetailSession> session = RetailSession::open(payload, {kMissionId, {0, 0}});
  REQUIRE(session != nullptr);
  SessionRun run;
  run.hash = 1469598103934665603ull;
  for (std::size_t tick = 1; tick <= kTicks; ++tick) {
    // The advance happens before the frame it belongs to, the way the mission
    // state's update branch reaches 0x82267370 before the rest of its work.
    if (cadence != 0 && tick % cadence == 0 && !session->script().ended()) {
      run.advances += 1;
      if (session->advance_script() == ScriptAdvance::Exhausted) run.ended_at_tick = tick;
    }
    run.last = session->tick(kFixedDt, input(tick));
    REQUIRE(run.last.camera_mode.raw_mode == 0 &&
            run.last.camera_mode.view_mode == 1);
    run.hash ^= frame_hash(run.last) * 1099511628211ull;
  }
  run.trace = session->script().executed();
  run.debrief = session->debrief();
  run.state = session->state();
  return run;
}

void write_snapshot_json(std::ostream& out, const char* name,
                         const ac6::NativeHudSnapshot& snapshot,
                         const ac6::RenderReadback& readback) {
  out << "  \"" << name << "\": {\n"
      << "    \"tick\": " << snapshot.tick << ",\n"
      << "    \"active_units\": " << snapshot.active_units << ",\n"
      << "    \"player_entity\": " << snapshot.player_entity << ",\n"
      << "    \"objective_count\": " << snapshot.objective_count << ",\n"
      << "    \"active_objective_id\": " << snapshot.active_objective_id << ",\n"
      << "    \"completed_objectives\": " << snapshot.completed_objectives << ",\n"
      << "    \"failed_objectives\": " << snapshot.failed_objectives << ",\n"
      << "    \"outcome\": " << static_cast<unsigned>(snapshot.outcome) << ",\n"
      << "    \"reticle_visible\": " << (snapshot.reticle_visible ? "true" : "false") << ",\n"
      << "    \"telemetry_visible\": " << (snapshot.telemetry_visible ? "true" : "false") << ",\n"
      << "    \"objective_visible\": " << (snapshot.objective_visible ? "true" : "false") << ",\n"
      << "    \"outcome_visible\": " << (snapshot.outcome_visible ? "true" : "false") << ",\n"
      << "    \"pixel_writes\": " << snapshot.pixel_writes << ",\n"
      << "    \"unique_pixels\": " << snapshot.unique_pixels << ",\n"
      << "    \"color_coverage\": " << readback.color_coverage << ",\n"
      << "    \"color_hash\": " << readback.color_hash << "\n"
      << "  }";
}

// Two captures and the numbers that make them mean something: the HUD in the
// middle of the mission, and the HUD once the script has run out. A capture
// that only "looks right" is worth nothing, so every field written below is
// asserted first.
void write_capture_bundle(const std::filesystem::path& directory,
                          const std::vector<std::uint8_t>& payload) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  REQUIRE(!error);
  std::unique_ptr<RetailSession> session = RetailSession::open(payload, {kMissionId, {0, 0}});
  REQUIRE(session != nullptr);

  ac6::NativeRenderTarget live_target, debrief_target;
  REQUIRE(live_target.resize(640, 360) && live_target.clear(0xFF000000u, 1.0f));
  REQUIRE(debrief_target.resize(640, 360) && debrief_target.clear(0xFF000000u, 1.0f));
  ac6::NativeHudRenderer live_hud, debrief_hud;
  ac6::retail::RetailSessionFrame frame;
  std::size_t live_markers = 0, debrief_markers = 0;
  for (std::size_t tick = 1; tick <= kTicks; ++tick) {
    if (tick % 300 == 0 && !session->script().ended()) (void)session->advance_script();
    frame = session->tick(kFixedDt, session_input(tick));
    if (tick == 900) {
      // World first, HUD over it - the order the geometry path uses.
      live_markers = session->render_world_markers(live_target, frame.world);
      REQUIRE(live_hud.render(live_target, frame.world, session->execution()));
    }
  }
  debrief_markers = session->render_world_markers(debrief_target, frame.world);
  REQUIRE(debrief_hud.render(debrief_target, frame.world, session->execution()));

  // The world the container built is now visible, and measured. Every marker
  // is a unit the retail faction table placed; none is invented, and the count
  // is bounded by the 230 the payload declares.
  REQUIRE(live_markers > 0 && live_markers <= 230);
  REQUIRE(debrief_markers > 0 && debrief_markers <= 230);
  REQUIRE(live_target.world_marker_writes() > 0);

  // Cycle 1146's lesson, held by a control instead of a comment.
  //
  // project_point normalises depth as view_z / far, so a caller that keeps the
  // 4096 default loses every unit beyond 4,096 units: they saturate to 1.0,
  // which is the clear value, and the depth test drops them. Mission 01 spans
  // about 66,000 units, so the default silently discards most of the world and
  // leaves a picture that looks deliberate.
  //
  // The two renders below differ ONLY in the far plane. If someone restores the
  // default in a caller, or if project_point stops normalising by it, this
  // stops discriminating and says so.
  float ex_min = 0.0f, ex_max = 0.0f, ez_min = 0.0f, ez_max = 0.0f;
  std::size_t placed_count = 0;
  for (const ac6::CombatUnitState& unit : session->world().combat.snapshot_units()) {
    const std::vector<ac6::EntityId>& placed = session->world().placed;
    if (std::find(placed.begin(), placed.end(), unit.entity) == placed.end()) continue;
    if (placed_count == 0) {
      ex_min = ex_max = unit.position.x;
      ez_min = ez_max = unit.position.z;
    } else {
      ex_min = std::min(ex_min, unit.position.x);
      ex_max = std::max(ex_max, unit.position.x);
      ez_min = std::min(ez_min, unit.position.z);
      ez_max = std::max(ez_max, unit.position.z);
    }
    placed_count += 1;
  }
  REQUIRE(placed_count > 0);
  const float placed_extent = std::max({ex_max - ex_min, ez_max - ez_min, 1.0f});
  // Mission 01's own span, asserted so a parser regression that collapses the
  // world cannot leave this control passing on a one-unit extent.
  REQUIRE(placed_extent > 10000.0f);

  // The comparison is against an explicitly UNDERSIZED plane, not against the
  // default. It used to be against the default, and cycle 1273 broke it by
  // replacing that default with retail's own 24000 -- at which point the
  // default is large enough and the strict inequality fails. That failure was
  // correct and it said what the control had really been testing: "4096 is too
  // small", not "the far plane matters". The fact worth holding is the second.
  constexpr float kUndersizedFarPlane = 4096.0f;  // the product's former default
  ac6::NativeRenderTarget probe_small, probe_derived;
  REQUIRE(probe_small.resize(640, 360) && probe_small.clear(0xFF000000u, 1.0f));
  REQUIRE(probe_derived.resize(640, 360) && probe_derived.clear(0xFF000000u, 1.0f));
  const std::size_t with_small =
      session->render_world_markers(probe_small, frame.world, kUndersizedFarPlane);
  const std::size_t with_derived =
      session->render_world_markers(probe_derived, frame.world, 4.0f * placed_extent);
  REQUIRE(with_derived > with_small);

  const ac6::NativeHudSnapshot& live = live_hud.snapshot();
  REQUIRE(live.tick == 900);
  REQUIRE(live.pixel_writes > 0 && live.unique_pixels > 0);
  // Three advances have run by tick 900, and two of them crossed a sub-mission
  // boundary, so exactly two of the four rows are behind the cursor.
  REQUIRE(live.objective_count == 4 && live.completed_objectives == 2);
  REQUIRE(live.active_objective_id == 3);
  REQUIRE(live.active_units == 230);
  REQUIRE(live.outcome == ac6::MissionOutcome::InProgress && !live.outcome_visible);
  REQUIRE(live.reticle_visible && live.telemetry_visible && live.objective_visible);

  const ac6::NativeHudSnapshot& done = debrief_hud.snapshot();
  REQUIRE(done.pixel_writes > 0);
  REQUIRE(done.objective_count == 4 && done.completed_objectives == 4);
  REQUIRE(done.failed_objectives == 0);
  REQUIRE(done.outcome == ac6::MissionOutcome::Success && done.outcome_visible);
  // The two captures are different images, so neither is a copy of the other.
  REQUIRE(live_target.readback().color_hash != debrief_target.readback().color_hash);

  // The overview. The session camera follows the player, and the player has no
  // load-time position, so it sits at the origin while the 95 placed units are
  // tens of thousands of units away - the live capture shows four of them.
  //
  // This third image exists to make the placement checkable rather than merely
  // counted, and its camera is **chosen, not derived**: it is placed from the
  // bounding box of the derived positions themselves, looking down at their
  // centroid. That is legitimate here and nowhere else, because the marker lane
  // is already declared diagnostic - no material, no texture, no topology, and
  // no frame-parity claim. A capture that framed itself this way while claiming
  // to be the retail camera would be worthless. This one claims to be a plot.
  ac6::NativeRenderTarget overview;
  REQUIRE(overview.resize(640, 360) && overview.clear(0xFF000000u, 1.0f));
  float min_x = 0.0f, max_x = 0.0f, min_y = 0.0f, max_y = 0.0f, min_z = 0.0f, max_z = 0.0f;
  bool first = true;
  for (const ac6::CombatUnitState& unit : session->world().combat.snapshot_units()) {
    if (std::find(session->world().placed.begin(), session->world().placed.end(),
                  unit.entity) == session->world().placed.end()) {
      continue;
    }
    if (first) {
      min_x = max_x = unit.position.x;
      min_y = max_y = unit.position.y;
      min_z = max_z = unit.position.z;
      first = false;
      continue;
    }
    min_x = std::min(min_x, unit.position.x); max_x = std::max(max_x, unit.position.x);
    min_y = std::min(min_y, unit.position.y); max_y = std::max(max_y, unit.position.y);
    min_z = std::min(min_z, unit.position.z); max_z = std::max(max_z, unit.position.z);
  }
  REQUIRE(!first);
  std::set<std::tuple<float, float, float>> distinct;
  for (const ac6::CombatUnitState& unit : session->world().combat.snapshot_units()) {
    if (std::find(session->world().placed.begin(), session->world().placed.end(),
                  unit.entity) != session->world().placed.end()) {
      distinct.insert({unit.position.x, unit.position.y, unit.position.z});
    }
  }
  const std::size_t distinct_positions = distinct.size();
  ac6::WorldFrame plot = frame.world;
  plot.camera_target_x = 0.5f * (min_x + max_x);
  plot.camera_target_y = 0.5f * (min_y + max_y);
  plot.camera_target_z = 0.5f * (min_z + max_z);
  // Far enough back that the whole extent fits the 60-degree fallback frustum,
  // and above it, so the plot reads as a map.
  const float extent = std::max({max_x - min_x, max_z - min_z, 1.0f});
  plot.camera_x = plot.camera_target_x;
  plot.camera_y = plot.camera_target_y + 0.62f * extent;
  plot.camera_z = plot.camera_target_z - 0.006f * extent;
  // The plot's own extent is its far plane: without it every marker beyond
  // 4096 units normalises to depth 1.0 and the depth test drops all of them.
  const std::size_t plotted =
      session->render_world_markers(overview, plot, 4.0f * extent);

  // Nearly all of the placed units must land, or the plot is not framing what
  // it claims to frame. This is the assertion that makes the image evidence.
  // 95 placed units occupy 59 distinct coordinates, and 54 of those reach a
  // pixel of their own. Both gaps are real and neither is a defect here:
  //
  //   95 -> 59  retail spawns a formation's members at one point. Their
  //             per-member separation is the Obj triple, which needs the parent
  //             frame cycle 1145 showed is never assigned - so the same debt
  //             surfaces again, this time as markers sitting on top of markers.
  //   59 -> 57  at this zoom two distinct positions round onto a pixel another
  //             marker already wrote.
  //
  // Asserted exactly, because a plot whose count drifts is a plot that stopped
  // being evidence.
  REQUIRE(distinct_positions == 59);
  REQUIRE(plotted == 57);
  REQUIRE(plotted <= distinct_positions);
  REQUIRE(plotted <= session->world().placed.size());
  REQUIRE(overview.readback().color_hash != live_target.readback().color_hash);
  // The overview must actually contain something: an all-black frame would
  // satisfy the inequality above and still be a picture of nothing.
  REQUIRE(overview.readback().color_coverage > 0);

  REQUIRE(live_target.write_ppm(directory / "hud-live.ppm"));
  REQUIRE(debrief_target.write_ppm(directory / "hud-debrief.ppm"));
  REQUIRE(overview.write_ppm(directory / "world-overview.ppm"));
  std::ofstream metrics(directory / "retail-session-hud.json");
  REQUIRE(static_cast<bool>(metrics));
  metrics << "{\n"
          << "  \"schema\": \"ac6.retail-session-hud.v1\",\n"
          << "  \"mission_id\": " << kMissionId << ",\n"
          << "  \"fixture\": false,\n"
          << "  \"source\": \"retail scenario container only, no manifest\",\n"
          << "  \"width\": 640,\n  \"height\": 360,\n"
          << "  \"world_markers_live\": " << live_markers << ",\n"
          << "  \"world_markers_debrief\": " << debrief_markers << ",\n"
          << "  \"world_marker_writes\": " << live_target.world_marker_writes() << ",\n"
          << "  \"world_markers_are_diagnostic\": true,\n"
          << "  \"units_placed\": " << session->world().placed.size() << ",\n"
          << "  \"units_without_load_time_position\": " << session->world().unplaced.size()
          << ",\n"
          << "  \"overview_markers\": " << plotted << ",\n"
          << "  \"distinct_spawn_positions\": " << distinct_positions << ",\n"
          << "  \"overview_camera_is_chosen_not_derived\": true,\n"
          << "  \"overview_bounds_x\": [" << min_x << ", " << max_x << "],\n"
          << "  \"overview_bounds_y\": [" << min_y << ", " << max_y << "],\n"
          << "  \"overview_bounds_z\": [" << min_z << ", " << max_z << "],\n"
          // The overview's own colour hash, recorded and not merely compared.
          // Line 320 already asserts it differs from the live frame's; until
          // cycle 1274 it was never written down, so world-overview.png was the
          // one committed image nothing could be checked against. It is nested
          // rather than flat so that the key path ends in "color_hash", which is
          // what tools/audit_capture_images_match_metrics.py matches on.
          << "  \"overview\": {\n"
          << "    \"markers\": " << plotted << ",\n"
          << "    \"color_coverage\": " << overview.readback().color_coverage << ",\n"
          << "    \"color_hash\": " << overview.readback().color_hash << "\n"
          << "  },\n";
  write_snapshot_json(metrics, "live", live, live_target.readback());
  metrics << ",\n";
  write_snapshot_json(metrics, "debrief", done, debrief_target.readback());
  metrics << "\n}\n";
  REQUIRE(static_cast<bool>(metrics));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr,
                 "usage: %s PAYLOAD [REPORT_JSON] [CAPTURE_DIR] [RETAIL_CACHE]\n",
                 argv[0]);
    return 2;
  }
  if (!std::filesystem::exists(argv[1])) {
    std::fprintf(stderr, "retail payload absent\n");
    return 77;
  }
  const std::vector<std::uint8_t> payload = read_file(argv[1]);

  // The parse first: retail's own step bound and the table the native reader
  // walks must agree, because the runner uses the first and the dispatch uses
  // the second.
  std::unique_ptr<RetailSession> probe = RetailSession::open(payload, {kMissionId, {0, 0}});
  REQUIRE(probe != nullptr);
  REQUIRE(!probe->bundle().has_value());
  check_store_backed_session(payload);
  if (argc >= 5) check_qualified_store_backed_session(argv[4]);
  const std::vector<ac6::retail::ScenarioSubMission>& sub_missions =
      probe->scenario().sub_missions();
  REQUIRE(sub_missions.size() == 4);
  std::size_t total_steps = 0;
  for (const ac6::retail::ScenarioSubMission& sub_mission : sub_missions) {
    REQUIRE(sub_mission.step_count_byte == sub_mission.step_tags.size());
    total_steps += sub_mission.step_tags.size();
  }
  REQUIRE(total_steps == 6);
  // The world came from the container and the player is the record the
  // faction switch classified, not a chosen index.
  REQUIRE(probe->world().published == 230);
  REQUIRE(probe->player_entity() != 0);
  // Sub-mission 0's tag-0 step installs a rectangle; the port of FUN_82268B28
  // normalises it and FUN_82268BA0 answers on x and z only.
  const std::optional<ac6::retail::MissionArea> area = probe->current_area();
  REQUIRE(area.has_value());
  REQUIRE(area->min_x == -50000.0f && area->max_x == 50000.0f);
  REQUIRE(area->min_z == -50000.0f && area->max_z == 50000.0f);
  REQUIRE(ac6::retail::area_contains(*area, {0.0f, 1.0e9f, 0.0f}));
  REQUIRE(!ac6::retail::area_contains(*area, {60000.0f, 0.0f, 0.0f}));

  // The trace the payload's step counts imply, written out rather than derived
  // in the test, so a runner that walked the script differently would fail.
  const std::vector<ScriptStepRun> expected = {
      {0, 0, 0}, {0, 1, 1}, {1, 0, 0}, {2, 0, 1}, {2, 1, 0}, {3, 0, 0},
  };

  // Three cadences, one trace. The script's shape is a property of the payload;
  // when it advances is the caller's business.
  const SessionRun paced = run_session(payload, 300);
  const SessionRun fast = run_session(payload, 1);
  const SessionRun slow = run_session(payload, 257);
  REQUIRE(paced.trace == expected);
  REQUIRE(fast.trace == expected);
  REQUIRE(slow.trace == expected);
  REQUIRE(paced.advances == 6 && paced.ended_at_tick == 1800);
  REQUIRE(fast.advances == 6 && fast.ended_at_tick == 6);
  REQUIRE(slow.advances == 6 && slow.ended_at_tick == 1542);

  // The debrief is reached, and it is reached with every sub-mission behind the
  // cursor - not with a flag.
  REQUIRE(paced.state == ac6::ScenarioState::Complete);
  REQUIRE(paced.debrief.outcome == ac6::MissionOutcome::Success);
  REQUIRE(paced.debrief.objective_count == 4);
  REQUIRE(paced.debrief.completed_objectives == 4);
  REQUIRE(paced.debrief.failed_objectives == 0);
  REQUIRE(paced.last.script_ended);

  // The anti-goal, asserted: without the script, nothing ends the mission.
  const SessionRun idle = run_session(payload, 0);
  REQUIRE(idle.advances == 0);
  REQUIRE(idle.state == ac6::ScenarioState::Gameplay);
  REQUIRE(idle.debrief.outcome == ac6::MissionOutcome::InProgress);
  REQUIRE(idle.debrief.completed_objectives == 0);
  REQUIRE(!idle.last.script_ended);
  REQUIRE(idle.last.world.tick == kTicks);
  REQUIRE(idle.last.world.player_entity == probe->player_entity());
  REQUIRE(idle.last.world.active_units == 230);
  // mission_ready is the product's asset-readiness flag, and it is false here
  // on purpose: the retail session declares no external asset, because the
  // archives stay outside the runtime and visual parity is out of JF's scope.
  // Asserting it true would mean fabricating asset records. Assert the truth.
  REQUIRE(!idle.last.world.mission_ready);

  // The session loop is a session loop: the flight integrator moved the player
  // off the origin, the camera follows it at the runtime's fixed offset, and
  // the camera aims at the player rather than at a constant.
  const ac6::WorldFrame& last = idle.last.world;
  REQUIRE(last.position_x != 0.0f || last.position_y != 0.0f || last.position_z != 0.0f);
  REQUIRE(last.camera_x == last.position_x - 12.0f);
  REQUIRE(last.camera_y == last.position_y + 3.0f);
  REQUIRE(last.camera_z == last.position_z + 12.0f);
  REQUIRE(last.camera_target_x == last.position_x &&
          last.camera_target_y == last.position_y &&
          last.camera_target_z == last.position_z);

  // Input reaches the flight model: a mirrored stick gives a different session.
  const SessionRun mirrored = run_session(payload, 0, mirrored_input);
  REQUIRE(mirrored.last.world.tick == kTicks);
  REQUIRE(mirrored.last.world.position_y != last.position_y);
  REQUIRE(mirrored.hash != idle.hash);

  // The session is deterministic: the same payload and the same input stream
  // give the same 1800 frames, camera included.
  const SessionRun repeat = run_session(payload, 300);
  REQUIRE(repeat.hash == paced.hash);
  const SessionRun idle_repeat = run_session(payload, 0);
  REQUIRE(idle_repeat.hash == idle.hash);
  // A different cadence is a different session, so the hashes must differ; an
  // equality here would mean the frames carry nothing about the script.
  REQUIRE(fast.hash != paced.hash);

  if (argc >= 4) write_capture_bundle(argv[3], payload);

  std::printf("retail_session ticks=%zu steps=%zu advances=%zu ended_at=%zu completed=%u\n",
              kTicks, paced.trace.size(), paced.advances, paced.ended_at_tick,
              paced.debrief.completed_objectives);

  if (argc >= 3) {
    std::ofstream report(argv[2]);
    REQUIRE(static_cast<bool>(report));
    report << "{\n"
           << "  \"schema\": \"ac6.retail-session.v1\",\n"
           << "  \"mission_id\": " << kMissionId << ",\n"
           << "  \"source\": \"retail scenario container only, no manifest\",\n"
           << "  \"units_published\": " << probe->world().published << ",\n"
           << "  \"player_entity\": " << probe->player_entity() << ",\n"
           << "  \"ticks\": " << kTicks << ",\n"
           << "  \"sub_missions\": " << sub_missions.size() << ",\n"
           << "  \"script_steps\": " << paced.trace.size() << ",\n"
           << "  \"script_advances\": " << paced.advances << ",\n"
           << "  \"script_exhausted_at_tick\": " << paced.ended_at_tick << ",\n"
           << "  \"trace_independent_of_cadence\": true,\n"
           << "  \"objectives\": " << paced.debrief.objective_count << ",\n"
           << "  \"objectives_completed\": " << paced.debrief.completed_objectives << ",\n"
           << "  \"outcome\": \"Success\",\n"
           << "  \"no_script_no_completion\": true,\n"
           << "  \"asset_readiness\": false,\n"
           << "  \"asset_readiness_note\": \"the session declares no external "
              "asset, so the product's readiness flag stays false; retail "
              "archives remain outside the runtime\",\n"
           << "  \"script_cadence_is_not_derived\": true,\n"
           << "  \"mission_area_x\": [" << area->min_x << ", " << area->max_x << "],\n"
           << "  \"mission_area_z\": [" << area->min_z << ", " << area->max_z << "],\n"
           << "  \"deterministic_replay\": true,\n"
           << "  \"session_hash\": \"" << std::hex << paced.hash << std::dec << "\"\n"
           << "}\n";
    REQUIRE(static_cast<bool>(report));
  }
  return 0;
}
