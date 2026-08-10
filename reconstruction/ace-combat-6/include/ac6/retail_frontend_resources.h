#pragma once

#include "ac6/retail_content.h"

#include <array>
#include <cstdint>
#include <optional>

namespace ac6::retail {

struct RetailFrontendFontSummary final {
  std::uint32_t data_table_entry{};
  std::uint64_t payload_size{};
  std::uint32_t fhm_nodes{};
  std::uint32_t nfh_nodes{};
  bool operator==(const RetailFrontendFontSummary&) const = default;
};

// Validates the PAL frontend font/glyph closure without decoding or
// synthesising text.  NFH leaves are checked recursively through the same
// bounded FHM reader used by the mission scene; an incomplete cache is never
// treated as an English-only fallback.
class RetailFrontendResources final {
 public:
  static std::optional<RetailFrontendResources> open(
      const RetailContentStore& store) noexcept;

  bool complete() const noexcept { return complete_; }
  const Sha256Digest& content_index_sha256() const noexcept {
    return content_index_sha256_;
  }
  const std::array<RetailFrontendFontSummary, 7>& fonts() const noexcept {
    return fonts_;
  }
  bool has_locale_slot(std::uint32_t slot) const noexcept {
    return slot < kPalFrontendLocaleDataTableEntries.size() &&
           fonts_[slot + 1u].nfh_nodes != 0;
  }

 private:
  Sha256Digest content_index_sha256_{};
  std::array<RetailFrontendFontSummary, 7> fonts_{};
  bool complete_{};
};

}  // namespace ac6::retail
