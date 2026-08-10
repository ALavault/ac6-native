#include "ac6/retail_camera_table.h"

#include <cmath>
#include <cstring>
#include <numbers>

namespace ac6::retail {
namespace {

float read_be_float(const std::uint8_t* bytes) noexcept {
  const std::uint32_t bits = (static_cast<std::uint32_t>(bytes[0]) << 24) |
                             (static_cast<std::uint32_t>(bytes[1]) << 16) |
                             (static_cast<std::uint32_t>(bytes[2]) << 8) |
                             static_cast<std::uint32_t>(bytes[3]);
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

bool record_is_bounded(const RetailCameraRecord& record) noexcept {
  for (const float value : record.fields) {
    if (!std::isfinite(value)) return false;
  }
  return record.ease_rate() > 0.0F && record.fov_radians() > 0.0F &&
         record.fov_radians() < std::numbers::pi_v<float> &&
         record.alternate_fov_radians() > 0.0F &&
         record.alternate_fov_radians() < std::numbers::pi_v<float>;
}

}  // namespace

std::optional<std::array<float, 4>> RetailCameraRecord::offset(
    std::size_t stage) const noexcept {
  if (stage >= 4) return std::nullopt;
  const std::size_t first = stage * 4;
  return std::array<float, 4>{fields[first], fields[first + 1],
                              fields[first + 2], fields[first + 3]};
}

std::optional<RetailCameraTable> RetailCameraTable::open(
    std::span<const std::uint8_t> bytes) noexcept {
  constexpr std::size_t kRecordCount = kRetailCameraGroups * kRetailCameraViews;
  if (bytes.size() != kRecordCount * kRetailCameraRecordBytes) {
    return std::nullopt;
  }

  RetailCameraTable table;
  for (std::size_t record_index = 0; record_index < kRecordCount;
       ++record_index) {
    RetailCameraRecord& record = table.records_[record_index];
    const std::size_t base = record_index * kRetailCameraRecordBytes;
    for (std::size_t field = 0; field < record.fields.size(); ++field) {
      record.fields[field] = read_be_float(bytes.data() + base + field * 4);
    }
    if (!record_is_bounded(record)) return std::nullopt;
  }
  return table;
}

std::optional<RetailCameraTable> RetailCameraTable::open(
    const RetailCampaignBundle& common_bundle) noexcept {
  if (common_bundle.data_table_entry() != kRetailCameraTableEntry ||
      common_bundle.child_count() != 55) {
    return std::nullopt;
  }
  const std::optional<std::span<const std::uint8_t>> bytes =
      common_bundle.child(kRetailCameraTableChild);
  return bytes.has_value() ? open(*bytes) : std::nullopt;
}

const RetailCameraRecord* RetailCameraTable::record(
    std::uint32_t group, std::uint32_t view_mode) const noexcept {
  if (group >= kRetailCameraGroups || view_mode == 0 ||
      view_mode > kRetailCameraViews) {
    return nullptr;
  }
  return &records_[group * kRetailCameraViews + (view_mode - 1)];
}

std::optional<std::uint32_t> RetailCameraTable::group_for_loadout(
    const CampaignLoadout& loadout) noexcept {
  if (!loadout.valid() || loadout.aircraft_id > kRetailCameraGroups) {
    return std::nullopt;
  }
  return loadout.aircraft_id - 1;
}

const RetailCameraRecord* RetailCameraTable::record_for_loadout(
    const CampaignLoadout& loadout, std::uint32_t view_mode) const noexcept {
  const std::optional<std::uint32_t> group = group_for_loadout(loadout);
  return group.has_value() ? record(*group, view_mode) : nullptr;
}

}  // namespace ac6::retail
