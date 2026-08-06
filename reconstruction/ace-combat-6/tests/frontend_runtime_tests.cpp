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
  return 0;
}
