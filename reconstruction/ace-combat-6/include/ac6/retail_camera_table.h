#pragma once

#include "ac6/retail_campaign_bundle.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace ac6::retail {

inline constexpr std::uint32_t kRetailCameraTableEntry = 1;
inline constexpr std::uint32_t kRetailCameraTableChild = 36;
inline constexpr std::size_t kRetailCameraGroups = 15;
inline constexpr std::size_t kRetailCameraViews = 3;
inline constexpr std::size_t kRetailCameraRecordBytes = 144;

// One on-disc camera record. Fields without a derived semantic name remain
// addressable as raw floats instead of acquiring an invented meaning.
struct RetailCameraRecord final {
  std::array<float, kRetailCameraRecordBytes / sizeof(float)> fields{};

  std::optional<std::array<float, 4>> offset(
      std::size_t stage) const noexcept;
  float ease_rate() const noexcept { return fields[0x58 / sizeof(float)]; }
  float fov_radians() const noexcept { return fields[0x68 / sizeof(float)]; }
  float alternate_fov_radians() const noexcept {
    return fields[0x6C / sizeof(float)];
  }
};

// 0x821D5EF8 loads DATA.TBL[1] and sends FHM child 36 to 0x8225C478.
// 0x8225C4A0 indexes its 144-byte records as 15 groups x three retail view
// modes, while 0x8225C510 copies the fields into the camera manager. The
// aircraft-to-group binding is intentionally outside this reader until its
// selector is derived; callers must provide both indices explicitly.
class RetailCameraTable final {
 public:
  static std::optional<RetailCameraTable> open(
      std::span<const std::uint8_t> bytes) noexcept;
  static std::optional<RetailCameraTable> open(
      const RetailCampaignBundle& common_bundle) noexcept;

  const RetailCameraRecord* record(std::uint32_t group,
                                   std::uint32_t view_mode) const noexcept;

 private:
  std::array<RetailCameraRecord, kRetailCameraGroups * kRetailCameraViews>
      records_{};
};

}  // namespace ac6::retail
