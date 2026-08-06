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
  for (int step = 0; step < 4; ++step) execution.tick(0.25f, {});
  REQUIRE(execution.combat().unit(4098) != nullptr);
  REQUIRE(ac6_test::fnv64("ac6") != 0);
  return 0;
}
