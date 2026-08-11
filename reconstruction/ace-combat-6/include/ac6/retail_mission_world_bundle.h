#pragma once

#include "ac6/retail_campaign_bundle.h"
#include "ac6/retail_fhm_view.h"

#include <cstdint>
#include <optional>
#include <span>

namespace ac6::retail {

// PAL world/mapset entries are consecutive and sit outside the fifteen
// campaign scenario entries.  The mapping is data-table identity, not a
// guessed filename or a generated-resource name.
std::optional<std::uint32_t> mission_world_data_table_entry(
    std::uint32_t mission_id) noexcept;

// The common world boundary shared by the qualified PAL campaign maps.  It
// validates only structure that is present in all fifteen payloads; geometry,
// material and texture consumers remain in their own bounded readers.
class RetailMissionWorldBundle final {
 public:
  static std::optional<RetailMissionWorldBundle> open(
      const RetailContentStore& store, std::uint32_t mission_id);
  static std::optional<RetailMissionWorldBundle> open_entry(
      const RetailContentStore& store, std::uint32_t data_table_entry);

  std::uint32_t mission_id() const noexcept { return mission_id_; }
  std::uint32_t data_table_entry() const noexcept {
    return bundle_->data_table_entry();
  }
  const Sha256Digest& content_index_sha256() const noexcept {
    return bundle_->content_index_sha256();
  }
  std::uint32_t root_child_count() const noexcept { return bundle_->child_count(); }

  // Root child 21 is the map hierarchy and root child 22 is the mapset.
  std::optional<RetailFhmView> map() const noexcept;
  std::optional<RetailFhmView> mapset() const noexcept;
  std::optional<std::span<const std::uint8_t>> map_resource(
      std::uint32_t index) const noexcept;
  std::optional<std::span<const std::uint8_t>> mapset_resource(
      std::uint32_t index) const noexcept;

 private:
  std::uint32_t mission_id_{};
  std::optional<RetailCampaignBundle> bundle_;
};

}  // namespace ac6::retail
