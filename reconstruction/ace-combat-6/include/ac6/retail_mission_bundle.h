#pragma once

#include "ac6/campaign_progression.h"
#include "ac6/retail_campaign_bundle.h"

#include <cstdint>
#include <optional>
#include <span>

namespace ac6::retail {

enum class RetailDifficulty : std::uint8_t {
  Easy = 0,
  Normal = 1,
  Hard = 2,
  Expert = 3,
  Ace = 4,
};

struct RetailMissionBundleConfig final {
  std::uint32_t mission_id{};
  RetailDifficulty difficulty{RetailDifficulty::Normal};
  CampaignLoadout loadout{};
};

// Product mission boundary. It owns the qualified campaign payload and keeps
// the gameplay choices that selected it, so a direct payload cannot silently
// become an interactive mission.
class RetailMissionBundle final {
 public:
  static std::optional<RetailMissionBundle> open(
      const RetailContentStore& store, RetailMissionBundleConfig config);

  std::uint32_t mission_id() const noexcept { return config_.mission_id; }
  RetailDifficulty difficulty() const noexcept { return config_.difficulty; }
  const CampaignLoadout& loadout() const noexcept { return config_.loadout; }
  std::uint32_t data_table_entry() const noexcept { return bundle_->data_table_entry(); }
  const Sha256Digest& content_index_sha256() const noexcept {
    return bundle_->content_index_sha256();
  }
  std::uint32_t child_count() const noexcept { return bundle_->child_count(); }
  std::span<const std::uint8_t> payload() const noexcept { return bundle_->payload(); }
  std::optional<std::span<const std::uint8_t>> child(std::uint32_t index) const noexcept {
    return bundle_->child(index);
  }

 private:
  RetailMissionBundleConfig config_{};
  std::optional<RetailCampaignBundle> bundle_;
};

}  // namespace ac6::retail
