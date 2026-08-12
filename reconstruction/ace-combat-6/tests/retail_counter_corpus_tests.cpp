#include "ac6/retail_content.h"
#include "ac6/retail_mission_bundle.h"
#include "test_fixtures.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

namespace {

// These are slot-1 allocation capacities, not counts of counters whose
// gameplay meaning has been identified.
constexpr std::array<std::uint16_t, 15> kCounterCapacities{
    339, 104, 168, 219, 423, 335, 297, 326, 176, 213, 403, 234, 471, 274, 190};
constexpr std::array<std::size_t, 15> kProducerCounts{
    232, 11, 4, 13, 198, 10, 246, 257, 16, 6, 128, 10, 213, 2135, 171};
constexpr std::array<std::uint16_t, 15> kMaximumCounterIds{
    332, 97, 45, 84, 315, 269, 262, 320, 144, 104, 207, 130, 468, 273, 189};
constexpr std::size_t kTotalProducers = 3650;

using OrderLocation = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>;
using CorpusLocation =
    std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>;

void check_producer_locations(std::uint32_t mission_id,
                              const ac6::retail::MissionScenario& scenario,
                              std::set<CorpusLocation>& corpus_locations) {
  std::set<OrderLocation> authored_locations;
  for (const ac6::retail::ScenarioOrderRecord& order : scenario.orders()) {
    if (order.tag != 6) continue;
    REQUIRE(authored_locations
                .emplace(order.unit_index, order.act_index, order.order_index)
                .second);
  }

  std::set<OrderLocation> producer_locations;
  for (const ac6::retail::ScenarioFlagOrder& producer :
       scenario.flag_orders()) {
    REQUIRE(producer.unit_index < scenario.units().size());
    REQUIRE(producer.counter_id < scenario.counter_capacity());
    REQUIRE(producer_locations
                .emplace(producer.unit_index, producer.act_index,
                         producer.order_index)
                .second);
    REQUIRE(corpus_locations
                .emplace(mission_id, producer.unit_index, producer.act_index,
                         producer.order_index)
                .second);
  }
  REQUIRE(producer_locations == authored_locations);
}

void check_tag7_boundary(std::uint32_t mission_id,
                         const ac6::retail::MissionScenario& scenario) {
  std::size_t tag7_steps = 0;
  std::vector<std::uint16_t> condition_ids;
  std::vector<std::uint8_t> condition_targets;
  for (const ac6::retail::ScenarioSubMission& sub_mission :
       scenario.sub_missions()) {
    REQUIRE(sub_mission.step_conditions.size() == sub_mission.step_tags.size());
    for (std::size_t step = 0; step < sub_mission.step_tags.size(); ++step) {
      if (sub_mission.step_tags[step] == 7) ++tag7_steps;
      const std::optional<ac6::retail::ScenarioStepCondition>& condition =
          sub_mission.step_conditions[step];
      if (!condition.has_value()) continue;
      REQUIRE(sub_mission.step_tags[step] == 7);
      REQUIRE(condition->counter_id < scenario.counter_capacity());
      REQUIRE(condition->target_sub_mission < scenario.sub_missions().size());
      condition_ids.push_back(condition->counter_id);
      condition_targets.push_back(condition->target_sub_mission);
    }
  }

  if (mission_id == 7) {
    REQUIRE(tag7_steps == 4);
    REQUIRE(condition_ids == std::vector<std::uint16_t>({263, 264, 97, 97}));
    REQUIRE(condition_targets == std::vector<std::uint8_t>({1, 2, 3, 3}));
  } else {
    REQUIRE(tag7_steps == 0);
    REQUIRE(condition_ids.empty());
    REQUIRE(condition_targets.empty());
  }
}

void check_mission01_details(const ac6::retail::MissionScenario& scenario) {
  std::set<std::uint16_t> ids;
  std::size_t set_operations = 0;
  std::size_t add_operations = 0;
  for (const ac6::retail::ScenarioFlagOrder& producer :
       scenario.flag_orders()) {
    ids.insert(producer.counter_id);
    if (producer.operation == 0) ++set_operations;
    if (producer.operation == 1) ++add_operations;
  }
  REQUIRE(scenario.flag_orders().size() == 232);
  REQUIRE(ids.size() == 133);
  REQUIRE(*ids.begin() == 45);
  REQUIRE(*ids.rbegin() == 332);
  REQUIRE(set_operations == 231);
  REQUIRE(add_operations == 1);
  REQUIRE(set_operations + add_operations == scenario.flag_orders().size());
}

}  // namespace

int main() {
  const char* cache_root = std::getenv("AC6_RETAIL_CACHE");
  if (cache_root == nullptr || *cache_root == '\0') {
    std::fprintf(stdout, "retail_counter_corpus_skipped=no_cache\n");
    return 77;
  }

  ac6::RetailContentStore store;
  if (!store.open(cache_root)) {
    std::fprintf(stderr, "retail_counter_corpus=fail cache=%s detail=%s\n",
                 ac6::retail_content_error_name(store.error()),
                 store.detail().c_str());
    return 1;
  }

  std::size_t total_producers = 0;
  std::set<CorpusLocation> corpus_locations;
  for (std::uint32_t mission_id = 1; mission_id <= 15; ++mission_id) {
    const std::size_t matrix_index = mission_id - 1;
    const std::optional<ac6::retail::RetailMissionBundle> bundle =
        ac6::retail::RetailMissionBundle::open(
            store,
            {mission_id, ac6::retail::RetailDifficulty::Normal, {1, 1, true}});
    REQUIRE(bundle.has_value());
    REQUIRE(bundle->scenario().has_value());
    const ac6::retail::MissionScenario& scenario = *bundle->scenario();
    REQUIRE(scenario.counter_capacity() == kCounterCapacities[matrix_index]);
    REQUIRE(scenario.flag_orders().size() == kProducerCounts[matrix_index]);
    const auto maximum = std::max_element(
        scenario.flag_orders().begin(), scenario.flag_orders().end(),
        [](const ac6::retail::ScenarioFlagOrder& left,
           const ac6::retail::ScenarioFlagOrder& right) {
          return left.counter_id < right.counter_id;
        });
    REQUIRE(maximum != scenario.flag_orders().end());
    REQUIRE(maximum->counter_id == kMaximumCounterIds[matrix_index]);

    check_producer_locations(mission_id, scenario, corpus_locations);
    check_tag7_boundary(mission_id, scenario);
    if (mission_id == 1) check_mission01_details(scenario);
    total_producers += scenario.flag_orders().size();
  }

  REQUIRE(total_producers == kTotalProducers);
  REQUIRE(corpus_locations.size() == kTotalProducers);
  std::fprintf(stdout,
               "retail_counter_corpus=pass missions=15 producers=%zu "
               "micro_execution=none\n",
               total_producers);
  return 0;
}
