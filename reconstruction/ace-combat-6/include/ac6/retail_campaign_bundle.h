#pragma once

#include "ac6/retail_container_index.h"
#include "ac6/retail_content.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ac6::retail {

// One qualified campaign DATA.TBL payload and its root FHM table. The bytes
// remain owned by this object, so child spans are valid for its lifetime.
class RetailCampaignBundle final {
 public:
  static std::optional<RetailCampaignBundle> open(
      const RetailContentStore& store, std::uint32_t mission_id);
  static std::optional<RetailCampaignBundle> open_entry(
      const RetailContentStore& store, std::uint32_t data_table_entry);

  std::uint32_t mission_id() const noexcept { return mission_id_; }
  std::uint32_t data_table_entry() const noexcept { return data_table_entry_; }
  const Sha256Digest& content_index_sha256() const noexcept {
    return content_index_sha256_;
  }
  std::uint32_t child_count() const noexcept { return root_.count; }
  std::span<const std::uint8_t> payload() const noexcept { return bytes_; }
  std::optional<std::span<const std::uint8_t>> child(
      std::uint32_t index) const noexcept;

 private:
  std::uint32_t mission_id_{};
  std::uint32_t data_table_entry_{};
  Sha256Digest content_index_sha256_{};
  std::vector<std::uint8_t> bytes_;
  ContainerIndex root_{};
};

}  // namespace ac6::retail
