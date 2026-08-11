#include "ac6/retail_mission_bundle.h"

#include <vector>

namespace ac6::retail {

std::optional<RetailMissionBundle> RetailMissionBundle::open(
    const RetailContentStore& store, RetailMissionBundleConfig config) {
  if (!store.valid() || config.mission_id == 0 ||
      config.mission_id > kPalCampaignDataTableEntries.size() ||
      !config.loadout.valid() ||
      static_cast<std::uint8_t>(config.difficulty) >
          static_cast<std::uint8_t>(RetailDifficulty::Ace)) {
    return std::nullopt;
  }
  std::optional<RetailCampaignBundle> campaign =
      RetailCampaignBundle::open(store, config.mission_id);
  if (!campaign.has_value()) return std::nullopt;
  const std::optional<std::span<const std::uint8_t>> scenario_bytes = campaign->child(0);
  if (!scenario_bytes.has_value()) return std::nullopt;
  std::vector<std::uint8_t> scenario_copy(scenario_bytes->begin(), scenario_bytes->end());
  std::optional<ScenarioPayload> scenario_payload =
      ScenarioPayload::open(std::move(scenario_copy));
  if (!scenario_payload.has_value()) return std::nullopt;
  std::optional<MissionScenario> scenario = MissionScenario::parse(*scenario_payload);
  if (!scenario.has_value()) return std::nullopt;
  RetailMissionBundle result;
  result.config_ = config;
  result.bundle_ = std::move(*campaign);
  result.scenario_payload_ = std::move(*scenario_payload);
  result.scenario_ = std::move(*scenario);
  return result;
}

}  // namespace ac6::retail
