#include "ac6/product_runtime.h"

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
  ac6::SessionSaveSnapshot snapshot{};
  snapshot.mission_id = 1;
  snapshot.flight = {120, 1.0f, 2.0f, 3.0f, 0.1f, -0.2f, 0.3f, 0.001f};
  snapshot.campaign.completed.push_back({1, 1});
  ac6::SessionSaveStore store;
  REQUIRE(store.save(2, snapshot));
  REQUIRE(!store.save(0, snapshot));
  const char* path = "ac6-test-session-save.ac6s";
  REQUIRE(store.write_file(path));

  ac6::SessionSaveStore loaded;
  REQUIRE(loaded.read_file(path));
  REQUIRE(loaded.load(2) != nullptr && *loaded.load(2) == snapshot);

  ac6::CampaignProgression campaign;
  REQUIRE(campaign.add({1, {1, 9, 9}, 1, {}}));
  REQUIRE(campaign.finalize());
  REQUIRE(campaign.restore(loaded.load(2)->campaign));
  REQUIRE(campaign.status(1)->state == ac6::CampaignMissionState::Completed);

  const char* bad_path = "ac6-test-bad-session-save.ac6s";
  { std::ofstream output(bad_path, std::ios::binary); output << "bad"; }
  REQUIRE(!loaded.read_file(bad_path));
  REQUIRE(loaded.load(2) != nullptr && *loaded.load(2) == snapshot);
  std::remove(path);
  std::remove(bad_path);
  return 0;
}
