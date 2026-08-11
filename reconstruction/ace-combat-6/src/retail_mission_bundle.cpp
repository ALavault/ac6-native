#include "ac6/retail_mission_bundle.h"

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
  RetailMissionBundle result;
  result.config_ = config;
  result.bundle_ = std::move(*campaign);
  return result;
}

}  // namespace ac6::retail
