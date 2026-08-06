#include "ac6/native_hud.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {

using namespace ac6;

constexpr char kAssetHash[] =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

struct HudProbe final {
  NativeHudSnapshot snapshot{};
  RenderReadback readback{};
};

struct WaveProbe final {
  bool spawned{};
  EntityId entity{};
  std::uint32_t active_units_before{};
  std::uint32_t active_units_after{};
  std::size_t pending{};
  std::size_t published{};
};

bool add_common_assets(MissionAssetDatabase& assets) {
  return assets.add({9, "fixture/f16.ndxr", kAssetHash, 1, {}}) &&
      assets.add({119, "fixture/terrain.ndxr", kAssetHash, 1, {}}) &&
      assets.add({165, "fixture/sky.ndxr", kAssetHash, 1, {}});
}

MissionDefinition definition() {
  return {1, MissionFamily::AirIntercept, {9, 119, 165}};
}

MissionLaunchDefinition launch() {
  return {1, 4097,
          {{4097, 1, 9, true}, {4098, 2, 119, true}, {4099, 2, 165, true}},
          {{7, 40.0f, 100.0f, 0.0f, 1000.0f}}};
}

bool render_hud(NativeRenderTarget& target, NativeHudRenderer& renderer,
                MissionExecution& execution, WorldFrame frame,
                HudProbe& result, const std::filesystem::path& ppm) {
  if (!target.clear(0x00000000u, 1.0f) ||
      !renderer.render(target, frame, execution)) return false;
  result.snapshot = renderer.snapshot();
  result.readback = target.readback();
  return ppm.empty() || target.write_ppm(ppm);
}

bool write_report(const std::filesystem::path& path, const HudProbe& live,
                  const HudProbe& paused, const HudProbe& success,
                  const HudProbe& failure, const WaveProbe& wave) {
  if (path.empty()) return true;
  std::error_code error;
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
  }
  std::ofstream output(path);
  if (!output) return false;
  const auto outcome_name = [](MissionOutcome outcome) constexpr {
    switch (outcome) {
      case MissionOutcome::InProgress: return "in_progress";
      case MissionOutcome::Success: return "success";
      case MissionOutcome::Failure: return "failure";
    }
    return "unknown";
  };
  const auto write_snapshot = [&](std::string_view name, const HudProbe& probe,
                                  bool comma) {
    const NativeHudSnapshot& hud = probe.snapshot;
    output << "  \"" << name << "\": {\n"
           << "    \"reticle_visible\": " << (hud.reticle_visible ? "true" : "false") << ",\n"
           << "    \"telemetry_visible\": " << (hud.telemetry_visible ? "true" : "false") << ",\n"
           << "    \"weapon_visible\": " << (hud.weapon_visible ? "true" : "false") << ",\n"
           << "    \"target_visible\": " << (hud.target_visible ? "true" : "false") << ",\n"
           << "    \"target_locked\": " << (hud.target_locked ? "true" : "false") << ",\n"
           << "    \"radar_visible\": " << (hud.radar_visible ? "true" : "false") << ",\n"
           << "    \"objective_visible\": " << (hud.objective_visible ? "true" : "false") << ",\n"
           << "    \"radio_visible\": " << (hud.radio_visible ? "true" : "false") << ",\n"
           << "    \"pause_visible\": " << (hud.pause_visible ? "true" : "false") << ",\n"
           << "    \"outcome_visible\": " << (hud.outcome_visible ? "true" : "false") << ",\n"
           << "    \"outcome\": \"" << outcome_name(hud.outcome) << "\",\n"
           << "    \"objective_count\": " << hud.objective_count << ",\n"
           << "    \"active_objective_id\": " << hud.active_objective_id << ",\n"
           << "    \"completed_objectives\": " << hud.completed_objectives << ",\n"
           << "    \"failed_objectives\": " << hud.failed_objectives << ",\n"
           << "    \"target_entity\": " << hud.target_entity << ",\n"
           << "    \"primary_weapon_id\": " << hud.primary_weapon_id << ",\n"
           << "    \"weapon_count\": " << hud.weapon_count << ",\n"
           << "    \"radio_message_id\": " << hud.radio_message_id << ",\n"
           << "    \"pixel_writes\": " << hud.pixel_writes << ",\n"
           << "    \"unique_pixels\": " << hud.unique_pixels << ",\n"
           << "    \"color_coverage\": " << probe.readback.color_coverage << ",\n"
           << "    \"color_hash\": " << probe.readback.color_hash << "\n"
           << "  }" << (comma ? ",\n" : "\n");
  };
  output << "{\n"
         << "  \"schema\": \"ac6.native-hud-debrief-acceptance.v1\",\n"
         << "  \"mission_id\": 1,\n"
         << "  \"fixture\": true,\n"
         << "  \"retail_semantics_qualified\": false,\n"
         << "  \"native_data_path\": true,\n"
         << "  \"wave\": {\n"
         << "    \"spawned\": " << (wave.spawned ? "true" : "false") << ",\n"
         << "    \"entity\": " << wave.entity << ",\n"
         << "    \"active_units_before\": " << wave.active_units_before << ",\n"
         << "    \"active_units_after\": " << wave.active_units_after << ",\n"
         << "    \"pending\": " << wave.pending << ",\n"
         << "    \"published\": " << wave.published << "\n"
         << "  },\n";
  write_snapshot("live", live, true);
  write_snapshot("paused", paused, true);
  write_snapshot("success", success, true);
  write_snapshot("failure", failure, false);
  output << "}\n";
  return static_cast<bool>(output);
}

bool check_live_snapshot(const HudProbe& probe) {
  const NativeHudSnapshot& hud = probe.snapshot;
  return hud.reticle_visible && hud.telemetry_visible && hud.weapon_visible &&
      hud.target_visible && hud.target_locked && hud.radar_visible &&
      hud.objective_visible && hud.radio_visible && hud.objective_count == 1 &&
      hud.active_objective_id == 1 && hud.target_entity == 4098 &&
      hud.primary_weapon_id == 7 && hud.weapon_count == 1 &&
      hud.radio_message_id == 15 && hud.outcome == MissionOutcome::InProgress &&
      !hud.outcome_visible && hud.pixel_writes != 0 && hud.unique_pixels != 0 &&
      probe.readback.color_coverage != 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::filesystem::path output_path = argc > 1 ? argv[1] : std::filesystem::path{};
  const std::filesystem::path output_root = output_path.empty()
      ? std::filesystem::path{}
      : output_path.parent_path();
  const std::filesystem::path live_ppm = output_root / "hud-live.ppm";
  const std::filesystem::path success_ppm = output_root / "hud-success.ppm";
  const std::filesystem::path failure_ppm = output_root / "hud-failure.ppm";

  const MissionDefinition mission = definition();
  MissionAssetDatabase assets;
  MissionObjectiveDatabase live_objectives;
  RadioMessageDatabase radios;
  if (!add_common_assets(assets) ||
      !live_objectives.add({1, {1, "native_fixture_objective", true,
                                ObjectiveState::Pending, ObjectiveCondition::Manual, 0}}) ||
      !radios.add({1, 15, "native_fixture_radio", "AWACS", 34, 34})) {
    return 1;
  }

  MissionExecution execution(mission, &assets, &live_objectives, &radios);
  const MissionLaunchDefinition mission_launch = launch();
  if (!execution.launch(mission_launch) || !execution.activate_objective(1) ||
      !execution.lock_target(4098) || !execution.play_radio(15, 1.0f)) return 2;
  const WorldFrame live_frame = execution.tick(1.0f / 60.0f, {4096, 1024, -512, 180, 0});
  NativeRenderTarget target;
  NativeHudRenderer renderer;
  if (!target.resize(640, 360)) return 3;
  HudProbe live;
  if (!render_hud(target, renderer, execution, live_frame, live, live_ppm) ||
      !check_live_snapshot(live)) return 4;

  if (!execution.dispatch({EventType::Pause, execution.scenario().player()})) return 5;
  const WorldFrame paused_frame = execution.tick(1.0f / 60.0f, {});
  renderer.reset();
  HudProbe paused;
  if (!render_hud(target, renderer, execution, paused_frame, paused, {}) ||
      !paused.snapshot.pause_visible || paused.snapshot.outcome_visible) return 6;
  if (!execution.dispatch({EventType::Resume, execution.scenario().player()})) return 7;

  MissionWaveDirector waves;
  if (!waves.add({1, 2, {5000, 2, 119, false},
                  {5000, 2, {20.0f, 0.0f, 0.0f}, 100.0f, 100.0f, 1.0f, true}})) return 8;
  MissionExecution wave_execution(mission, &assets, nullptr, nullptr, nullptr, &waves);
  if (!wave_execution.launch(mission_launch)) return 9;
  const std::uint32_t active_units_before =
      static_cast<std::uint32_t>(wave_execution.combat().active_units());
  (void)wave_execution.tick(1.0f / 60.0f, {});
  const WorldFrame wave_frame = wave_execution.tick(1.0f / 60.0f, {});
  const std::uint32_t active_units_after =
      static_cast<std::uint32_t>(wave_execution.combat().active_units());
  const WaveProbe wave{
      wave_execution.combat().unit(5000) != nullptr &&
          wave_execution.units().find(5000) != nullptr &&
          wave_execution.units().find(5000)->active,
      5000,
      active_units_before,
      active_units_after,
      waves.pending(1),
      waves.spawned(1)};
  (void)wave_frame;
  if (!wave.spawned || wave.active_units_after != wave.active_units_before + 1u ||
      wave.pending != 0 || wave.published != 1) return 10;

  MissionObjectiveDatabase success_objectives;
  if (!success_objectives.add({1, {1, "native_fixture_success", true,
                                  ObjectiveState::Pending, ObjectiveCondition::Manual, 0}})) return 8;
  MissionExecution success_execution(mission, &assets, &success_objectives, &radios);
  if (!success_execution.launch(mission_launch) ||
      !success_execution.activate_objective(1) ||
      !success_execution.complete_objective(1)) return 11;
  const WorldFrame success_frame = success_execution.tick(1.0f / 60.0f, {});
  renderer.reset();
  HudProbe success;
  if (!render_hud(target, renderer, success_execution, success_frame, success, success_ppm) ||
      success.snapshot.outcome != MissionOutcome::Success ||
      !success.snapshot.outcome_visible || success.snapshot.completed_objectives != 1) return 12;

  MissionObjectiveDatabase failure_objectives;
  if (!failure_objectives.add({1, {1, "native_fixture_failure", true,
                                  ObjectiveState::Pending, ObjectiveCondition::ProtectUnit, 4098}})) return 13;
  MissionExecution failure_execution(mission, &assets, &failure_objectives, &radios);
  if (!failure_execution.launch(mission_launch) ||
      !failure_execution.activate_objective(1) ||
      !failure_execution.combat().apply_damage(4098, 100.0f)) return 14;
  const WorldFrame failure_frame = failure_execution.tick(1.0f / 60.0f, {});
  renderer.reset();
  HudProbe failure;
  if (!render_hud(target, renderer, failure_execution, failure_frame, failure, failure_ppm) ||
      failure.snapshot.outcome != MissionOutcome::Failure ||
      !failure.snapshot.outcome_visible || failure.snapshot.failed_objectives != 1) return 15;

  if (!write_report(output_path, live, paused, success, failure, wave)) return 16;
  std::cout << "native_hud_debrief_acceptance=pass\n";
  return 0;
}
