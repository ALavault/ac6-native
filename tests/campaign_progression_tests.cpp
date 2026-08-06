#include "ac6/campaign_progression.h"

#include <cstdio>
#include <cstdlib>

namespace {
void require(bool condition, const char* expression, int line) {
  if (!condition) {
    std::fprintf(stderr, "REQUIRE failed at line %d: %s\n", line, expression);
    std::abort();
  }
}
#define REQUIRE(value) require((value), #value, __LINE__)
}

int main() {
  ac6::CampaignProgression campaign;
  REQUIRE(campaign.add({1, {1, 9, 9}, 2, {}}));
  REQUIRE(campaign.add({2, {2, 10, 10}, 1, {1}}));
  REQUIRE(!campaign.add({3, {2, 11, 11}, 1, {}}));
  REQUIRE(campaign.finalize());
  REQUIRE(campaign.is_available(1));
  REQUIRE(!campaign.is_available(2));
  REQUIRE(campaign.route_for_selector(1) != nullptr);
  REQUIRE(campaign.route_for_selector(1)->data_table_entry == 9);
  REQUIRE(campaign.route_for_selector(3) == nullptr);

  REQUIRE(campaign.enter_briefing(1));
  REQUIRE(!campaign.begin(1));
  REQUIRE(!campaign.set_loadout(1, {0, 7, true}));
  REQUIRE(!campaign.set_loadout(1, {7, 0, true}));
  REQUIRE(!campaign.set_loadout(1, {7, 8, false}));
  REQUIRE(campaign.set_loadout(1, {7, 8, true}));
  REQUIRE(campaign.begin(1));
  REQUIRE(campaign.complete_objective(1, 0));
  REQUIRE(!campaign.complete(1));
  REQUIRE(campaign.complete_objective(1, 1));
  REQUIRE(campaign.complete(1));
  REQUIRE(campaign.status(1)->state == ac6::CampaignMissionState::Completed);
  REQUIRE(campaign.is_available(2));

  std::vector<std::uint8_t> encoded;
  REQUIRE(campaign.encode_snapshot(encoded));
  REQUIRE(encoded.size() == 24);
  ac6::CampaignProgression restored;
  REQUIRE(restored.add({1, {1, 9, 9}, 2, {}}));
  REQUIRE(restored.add({2, {2, 10, 10}, 1, {1}}));
  REQUIRE(restored.finalize());
  REQUIRE(restored.decode_snapshot(encoded));
  REQUIRE(restored.status(1)->state == ac6::CampaignMissionState::Completed);
  REQUIRE(restored.is_available(2));
  REQUIRE(!restored.decode_snapshot({}));

  ac6::CampaignProgression invalid;
  REQUIRE(invalid.add({1, {1, 9, 9}, 1, {2}}));
  REQUIRE(invalid.add({2, {2, 10, 10}, 1, {1}}));
  REQUIRE(!invalid.finalize());
  return 0;
}
