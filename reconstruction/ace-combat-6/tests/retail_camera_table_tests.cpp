#include "ac6/retail_camera_table.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::printf("FAIL  %s\n", message);
    ++failures;
  }
}

void write_be_float(std::vector<std::uint8_t>& bytes, std::size_t offset,
                    float value) {
  const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
  bytes[offset] = static_cast<std::uint8_t>(bits >> 24);
  bytes[offset + 1] = static_cast<std::uint8_t>(bits >> 16);
  bytes[offset + 2] = static_cast<std::uint8_t>(bits >> 8);
  bytes[offset + 3] = static_cast<std::uint8_t>(bits);
}

std::vector<std::uint8_t> valid_table() {
  using namespace ac6::retail;
  std::vector<std::uint8_t> bytes(kRetailCameraGroups * kRetailCameraViews *
                                  kRetailCameraRecordBytes);
  for (std::size_t group = 0; group < kRetailCameraGroups; ++group) {
    for (std::size_t view = 0; view < kRetailCameraViews; ++view) {
      const std::size_t base =
          (group * kRetailCameraViews + view) * kRetailCameraRecordBytes;
      write_be_float(bytes, base + 0x00, static_cast<float>(group));
      write_be_float(bytes, base + 0x04, static_cast<float>(view + 1));
      write_be_float(bytes, base + 0x08, -6.0F - static_cast<float>(group));
      write_be_float(bytes, base + 0x58, 7.0F);
      write_be_float(bytes, base + 0x68, 0.8F);
      write_be_float(bytes, base + 0x6C, 0.9F);
    }
  }
  return bytes;
}

}  // namespace

int main() {
  using namespace ac6::retail;

  const RetailCameraModeSelection opening = retail_opening_camera_mode();
  check(opening.raw_mode == 0 && opening.view_mode == 1,
        "the qualified campaign opening selector maps raw zero to view one");
  for (const RetailCameraModeSelection expected : {
           RetailCameraModeSelection{0, 1}, RetailCameraModeSelection{1, 1},
           RetailCameraModeSelection{2, 2}, RetailCameraModeSelection{3, 3}}) {
    check(resolve_retail_camera_mode(expected.raw_mode) == expected,
          "the retail raw camera selector maps to its table view");
  }
  check(!resolve_retail_camera_mode(4).has_value(),
        "an unsupported raw camera selector fails closed");

  const std::vector<std::uint8_t> bytes = valid_table();
  const std::optional<RetailCameraTable> table = RetailCameraTable::open(bytes);
  check(table.has_value(), "a bounded 15 x 3 table opens");
  if (!table.has_value()) return 1;

  const RetailCameraRecord* record = table->record(4, 2);
  check(record != nullptr, "explicit group and retail view mode resolve");
  if (record != nullptr) {
    const std::optional<std::array<float, 4>> offset = record->offset(0);
    check(offset.has_value() && (*offset)[0] == 4.0F &&
              (*offset)[1] == 2.0F && (*offset)[2] == -10.0F,
          "big-endian offset fields decode at their on-disc offsets");
    check(record->ease_rate() == 7.0F && record->fov_radians() == 0.8F &&
              record->alternate_fov_radians() == 0.9F,
          "derived scalar fields decode at +0x58/+0x68/+0x6c");
    check(!record->offset(4).has_value(), "offset stage 4 fails closed");
  }
  check(table->record(15, 1) == nullptr, "group 15 fails closed");
  check(table->record(0, 0) == nullptr, "view mode 0 fails closed");
  check(table->record(0, 4) == nullptr, "view mode 4 fails closed");

  for (std::uint32_t aircraft_id = 1; aircraft_id <= kRetailCameraGroups;
       ++aircraft_id) {
    const ac6::CampaignLoadout loadout{aircraft_id, 1, true};
    const std::optional<std::uint32_t> group =
        RetailCameraTable::group_for_loadout(loadout);
    check(group.has_value() && *group == aircraft_id - 1,
          "one-based native aircraft ID maps to its retail ordinal");
    for (std::uint32_t view_mode = 1; view_mode <= kRetailCameraViews;
         ++view_mode) {
      check(table->record_for_loadout(loadout, view_mode) ==
                table->record(aircraft_id - 1, view_mode),
            "loadout selects the direct retail camera group");
    }
  }
  check(!RetailCameraTable::group_for_loadout({0, 1, true}).has_value(),
        "unset aircraft fails closed");
  check(!RetailCameraTable::group_for_loadout({16, 1, true}).has_value(),
        "aircraft outside the retail table fails closed");
  check(!RetailCameraTable::group_for_loadout({1, 0, true}).has_value(),
        "an incomplete loadout fails closed");
  check(!RetailCameraTable::group_for_loadout({1, 1, false}).has_value(),
        "loadout without capability data fails closed");
  check(table->record_for_loadout({1, 1, true}, 0) == nullptr,
        "loadout does not relax the retail view bounds");

  check(!RetailCameraTable::open(
             std::span<const std::uint8_t>(bytes).first(bytes.size() - 1))
             .has_value(),
        "a truncated table fails closed");

  std::vector<std::uint8_t> invalid = bytes;
  write_be_float(invalid, 0x20, std::numeric_limits<float>::quiet_NaN());
  check(!RetailCameraTable::open(invalid).has_value(),
        "a non-finite record fails closed");
  invalid = bytes;
  write_be_float(invalid, 0x68, 0.0F);
  check(!RetailCameraTable::open(invalid).has_value(),
        "a zero FOV fails closed");
  invalid = bytes;
  write_be_float(invalid, 0x58, -1.0F);
  check(!RetailCameraTable::open(invalid).has_value(),
        "a negative ease rate fails closed");

  if (failures == 0) std::printf("retail camera table OK\n");
  return failures == 0 ? 0 : 1;
}
