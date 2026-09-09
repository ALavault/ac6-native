#include "test_fixtures.h"

int main() {
  auto catalog = ac6_test::catalog_fixture();
  REQUIRE(catalog.find(1)->family == ac6::MissionFamily::AirIntercept);

  ac6::MissionAssetDatabase assets;
  REQUIRE(assets.add({9, "DATA00.PAC@qualified", std::string(64, 'a')}));
  REQUIRE(assets.add({119, "DATA00.PAC@qualified", std::string(64, 'b')}));

  ac6::MissionScenario scenario(1);
  REQUIRE(scenario.dispatch({ac6::EventType::StartMission, 0}));
  REQUIRE(scenario.state() == ac6::ScenarioState::Gameplay);
  REQUIRE(scenario.dispatch({ac6::EventType::Pause, 0}));
  REQUIRE(scenario.state() == ac6::ScenarioState::Paused);
  REQUIRE(scenario.dispatch({ac6::EventType::Resume, 0}));

  ac6::MissionExecution execution(*catalog.find(1), &assets);
  const auto launch = ac6_test::launch_fixture();
  REQUIRE(execution.launch(launch));
  REQUIRE(execution.scenario().state() == ac6::ScenarioState::Gameplay);
  ac6::WorldFrame external{};
  external.position_x = 1234.0F;
  external.position_y = 456.0F;
  external.position_z = -789.0F;
  external.pitch = 0.25F;
  external.roll = -0.5F;
  external.yaw = 0.75F;
  external.speed = 250.0F;
  external.camera_x = 1228.0F;
  external.camera_y = 458.0F;
  external.camera_z = -794.0F;
  external.camera_target_x = 1234.0F;
  external.camera_target_y = 456.0F;
  external.camera_target_z = -700.0F;
  ac6::InputFrame hostile_generic_input{};
  hostile_generic_input.pitch = 32767;
  hostile_generic_input.roll = -32768;
  hostile_generic_input.yaw = 20000;
  hostile_generic_input.throttle = 255U;
  const ac6::WorldFrame external_result = execution.tick_external(
      1.0F / 60.0F, hostile_generic_input, external);
  REQUIRE(external_result.tick == 1U);
  REQUIRE(external_result.position_x == external.position_x);
  REQUIRE(external_result.position_y == external.position_y);
  REQUIRE(external_result.position_z == external.position_z);
  REQUIRE(external_result.pitch == external.pitch);
  REQUIRE(external_result.roll == external.roll);
  REQUIRE(external_result.yaw == external.yaw);
  REQUIRE(execution.snapshot().position_x == external.position_x);
  for (int step = 0; step < 4; ++step) execution.tick(0.25f, {});
  REQUIRE(execution.combat().unit(4098) != nullptr);
  REQUIRE(ac6_test::fnv64("ac6") != 0);
  return 0;
}
