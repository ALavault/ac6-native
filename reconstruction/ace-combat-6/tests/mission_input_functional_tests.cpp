#include "test_fixtures.h"

#include <fstream>

int main() {
  ac6::InputMappingDatabase mappings;
  REQUIRE(mappings.add({0x10, ac6::EventType::Pause}));
  REQUIRE(mappings.add({0x20, ac6::EventType::Resume}));
  REQUIRE(!mappings.add({0x10, ac6::EventType::Abort}));
  REQUIRE(mappings.resolve(0x11)->event == ac6::EventType::Pause);

  ac6::MissionScenario scenario(1);
  REQUIRE(scenario.dispatch({ac6::EventType::StartMission, 0}));
  REQUIRE(scenario.dispatch_buttons(mappings, 0x10));
  REQUIRE(scenario.state() == ac6::ScenarioState::Paused);
  REQUIRE(scenario.dispatch_buttons(mappings, 0x20));
  REQUIRE(scenario.state() == ac6::ScenarioState::Gameplay);

  ac6::MissionCatalog catalog;
  REQUIRE(catalog.add({1, ac6::MissionFamily::Strike, {9}}));
  const char* path = "ac6-test-functional-catalog.tsv";
  { std::ofstream out(path); out << "2\tescort\t119\n"; }
  REQUIRE(catalog.load_manifest(path));
  REQUIRE(catalog.find(2)->family == ac6::MissionFamily::Escort);
  std::remove(path);
  return 0;
}
