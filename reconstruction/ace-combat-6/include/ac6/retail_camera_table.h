#pragma once

#include "ac6/campaign_progression.h"
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

// The camera selector is stored as a raw word on the player, then translated
// by 0x82223AC0 before the 15 x 3 table is indexed.  The normal campaign
// constructor (0x82230AF8) starts with raw word 0; the canonical PAL image's
// per-player selector table is also zero-initialised at
// [0x826E4EB4]+0x70+0x4C68 for all three records.
struct RetailCameraModeSelection final {
  std::uint32_t raw_mode{};
  std::uint32_t view_mode{};
  bool operator==(const RetailCameraModeSelection &) const = default;
};

inline constexpr std::uint32_t kRetailOpeningCameraModeWord = 0;

// 0x82223AC0 maps raw 2 -> view 2, raw 3 -> view 3, and every other
// supported initial value -> view 1.  Values above 3 are intentionally
// rejected here: the native table has no corresponding retail view and the
// caller must not silently choose one.
std::optional<RetailCameraModeSelection>
resolve_retail_camera_mode(std::uint32_t raw_mode) noexcept;

RetailCameraModeSelection retail_opening_camera_mode() noexcept;

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
// modes, while 0x8225C510 copies the fields into the camera manager.
// 0x82276610 passes the zero-based aircraft ordinal returned by 0x82090438
// directly as that group. CampaignLoadout exposes the same value plus one so
// that zero can remain its fail-closed "not selected" sentinel.
class RetailCameraTable final {
 public:
  static std::optional<RetailCameraTable> open(
      std::span<const std::uint8_t> bytes) noexcept;
  static std::optional<RetailCameraTable> open(
      const RetailCampaignBundle& common_bundle) noexcept;

  const RetailCameraRecord* record(std::uint32_t group,
                                   std::uint32_t view_mode) const noexcept;
  static std::optional<std::uint32_t> group_for_loadout(
      const CampaignLoadout& loadout) noexcept;
  const RetailCameraRecord* record_for_loadout(
      const CampaignLoadout& loadout,
      std::uint32_t view_mode) const noexcept;

 private:
  std::array<RetailCameraRecord, kRetailCameraGroups * kRetailCameraViews>
      records_{};
};

}  // namespace ac6::retail
