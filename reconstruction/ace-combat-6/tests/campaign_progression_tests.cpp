#include "ac6/campaign_progression.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

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

  ac6::CampaignProgression shared_resource;
  REQUIRE(shared_resource.add({1, {1, 9, 9}, 1, {}}));
  REQUIRE(shared_resource.add({2, {2, 9, 9}, 1, {}}));
  REQUIRE(shared_resource.finalize());

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
  REQUIRE(encoded.size() == 40);
  ac6::CampaignProgression restored;
  REQUIRE(restored.add({1, {1, 9, 9}, 2, {}}));
  REQUIRE(restored.add({2, {2, 10, 10}, 1, {1}}));
  REQUIRE(restored.finalize());
  REQUIRE(restored.decode_snapshot(encoded));
  REQUIRE(restored.status(1)->state == ac6::CampaignMissionState::Completed);
  REQUIRE(restored.is_available(2));
  REQUIRE(!restored.decode_snapshot({}));

  ac6::CampaignProgression active_campaign;
  REQUIRE(active_campaign.add({1, {1, 9, 9}, 2, {}}));
  REQUIRE(active_campaign.finalize());
  REQUIRE(active_campaign.enter_briefing(1));
  REQUIRE(active_campaign.set_loadout(1, {7, 8, true}));
  REQUIRE(active_campaign.begin(1));
  REQUIRE(active_campaign.complete_objective(1, 0));
  const ac6::CampaignSaveSnapshot active_snapshot = active_campaign.snapshot();
  ac6::CampaignProgression active_restored;
  REQUIRE(active_restored.add({1, {1, 9, 9}, 2, {}}));
  REQUIRE(active_restored.finalize());
  REQUIRE(active_restored.restore(active_snapshot));
  REQUIRE(active_restored.status(1)->state == ac6::CampaignMissionState::Active);
  const ac6::CampaignLoadout expected_loadout{7, 8, true};
  REQUIRE(active_restored.status(1)->objective_mask == 1 &&
          active_restored.status(1)->loadout == expected_loadout);

  ac6::CampaignSaveStore saves;
  REQUIRE(saves.save(3, campaign.snapshot()));
  REQUIRE(!saves.save(0, campaign.snapshot()));
  const char* save_file = "ac6-test-campaign-save.ac6s";
  REQUIRE(saves.write_file(save_file));
  ac6::CampaignSaveStore loaded_saves;
  REQUIRE(loaded_saves.read_file(save_file));
  REQUIRE(loaded_saves.load(3) != nullptr &&
          *loaded_saves.load(3) == campaign.snapshot());
  const char* bad_save = "ac6-test-bad-campaign-save.ac6s";
  { std::ofstream out(bad_save, std::ios::binary); out << "bad"; }
  REQUIRE(!loaded_saves.read_file(bad_save));
  REQUIRE(loaded_saves.load(3) != nullptr);
  std::remove(save_file);
  std::remove(bad_save);

  std::vector<std::uint8_t> empty_snapshot;
  ac6::CampaignProgression fresh;
  REQUIRE(fresh.add({1, {1, 9, 9}, 1, {}}));
  REQUIRE(fresh.finalize());
  REQUIRE(fresh.encode_snapshot(empty_snapshot));
  REQUIRE(empty_snapshot.size() == 16);
  REQUIRE(fresh.decode_snapshot(empty_snapshot));
  REQUIRE(fresh.is_available(1));

  ac6::CampaignProgression invalid;
  REQUIRE(invalid.add({1, {1, 9, 9}, 1, {2}}));
  REQUIRE(invalid.add({2, {2, 10, 10}, 1, {1}}));
  REQUIRE(!invalid.finalize());

  const char* manifest = "ac6-test-campaign.tsv";
  {
    std::ofstream out(manifest);
    out << "1\t1\t9\t9\t2\t-\n";
    out << "2\t2\t10\t10\t1\t1\n";
  }
  ac6::CampaignProgression from_manifest;
  REQUIRE(from_manifest.load_manifest(manifest));
  REQUIRE(from_manifest.route_for_selector(2)->dpl_resource_id == 10);
  REQUIRE(from_manifest.is_available(1) && !from_manifest.is_available(2));
  std::remove(manifest);
  {
    std::ofstream out(manifest);
    out << "1\t1\t9\t9\t2\t-\n";
    out << "bad\n";
  }
  REQUIRE(!from_manifest.load_manifest(manifest));
  REQUIRE(from_manifest.is_available(1));
  std::remove(manifest);
  return 0;
}
