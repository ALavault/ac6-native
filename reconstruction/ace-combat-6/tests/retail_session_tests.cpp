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
// usage: retail-session-tests PAYLOAD [REPORT_JSON] [CAPTURE_DIR]
// exit 77 means the retail payload was absent; it is never committed.

#include "ac6/native_hud.h"
#include "ac6/retail_session.h"
#include "test_fixtures.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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
  std::snprintf(row, sizeof(row), "%llu:%u:%d:%.5f:%.5f:%.5f:%.5f:%.5f:%.5f:%.5f:%u:%u:%u:%u:%d:%d",
                static_cast<unsigned long long>(frame.world.tick), frame.world.mission_id,
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
  for (std::size_t tick = 1; tick <= kTicks; ++tick) {
    if (tick % 300 == 0 && !session->script().ended()) (void)session->advance_script();
    frame = session->tick(kFixedDt, session_input(tick));
    if (tick == 900) REQUIRE(live_hud.render(live_target, frame.world, session->execution()));
  }
  REQUIRE(debrief_hud.render(debrief_target, frame.world, session->execution()));

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

  REQUIRE(live_target.write_ppm(directory / "hud-live.ppm"));
  REQUIRE(debrief_target.write_ppm(directory / "hud-debrief.ppm"));
  std::ofstream metrics(directory / "retail-session-hud.json");
  REQUIRE(static_cast<bool>(metrics));
  metrics << "{\n"
          << "  \"schema\": \"ac6.retail-session-hud.v1\",\n"
          << "  \"mission_id\": " << kMissionId << ",\n"
          << "  \"fixture\": false,\n"
          << "  \"source\": \"retail scenario container only, no manifest\",\n"
          << "  \"width\": 640,\n  \"height\": 360,\n";
  write_snapshot_json(metrics, "live", live, live_target.readback());
  metrics << ",\n";
  write_snapshot_json(metrics, "debrief", done, debrief_target.readback());
  metrics << "\n}\n";
  REQUIRE(static_cast<bool>(metrics));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s PAYLOAD [REPORT_JSON] [CAPTURE_DIR]\n", argv[0]);
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
