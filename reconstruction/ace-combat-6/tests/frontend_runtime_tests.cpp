#include "ac6/frontend_runtime.h"
#include "test_fixtures.h"

int main() {
  ac6::FrontendController frontend;
  REQUIRE(frontend.configure({ac6::FrontendDifficulty::Normal,
                               ac6::FrontendControls::Normal,
                               ac6::FrontendLanguage::English}));
  const ac6::MissionCatalog catalog = ac6_test::catalog_fixture();
  REQUIRE(frontend.select_mission(catalog, 1));
  REQUIRE(frontend.advance());
  REQUIRE(frontend.state() == ac6::FrontendState::NewGame);
  REQUIRE(frontend.advance());
  REQUIRE(frontend.state() == ac6::FrontendState::Briefing);
  REQUIRE(frontend.advance());
  REQUIRE(frontend.state() == ac6::FrontendState::Hangar);
  REQUIRE(frontend.advance());
  REQUIRE(frontend.state() == ac6::FrontendState::Loading);
  REQUIRE(frontend.advance());
  REQUIRE(frontend.state() == ac6::FrontendState::Mission);
  REQUIRE(!frontend.advance());

  ac6::MissionObjectiveDatabase objectives;
  REQUIRE(objectives.add({1, {1, "frontend_terminal_fixture", true,
                              ac6::ObjectiveState::Pending,
                              ac6::ObjectiveCondition::Manual, 0}}));
  ac6::MissionAssetDatabase assets;
  ac6::MissionExecution execution(*catalog.find(1), &assets, &objectives);
  REQUIRE(execution.launch(ac6_test::launch_fixture()));
  REQUIRE(execution.activate_objective(1));
  REQUIRE(execution.complete_objective(1));
  (void)execution.tick(1.0f / 60.0f, {});
  REQUIRE(frontend.enter_debrief(execution));
  REQUIRE(frontend.state() == ac6::FrontendState::Debrief);
  REQUIRE(!frontend.advance());
  REQUIRE(frontend.state() == ac6::FrontendState::Debrief);
  REQUIRE(frontend.return_to_campaign());
  REQUIRE(frontend.state() == ac6::FrontendState::NewGame);
  return 0;
}
