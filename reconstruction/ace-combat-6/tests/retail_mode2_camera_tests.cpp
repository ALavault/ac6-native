#include "ac6/retail_mode2_camera.h"

#include "ac6/retail_campaign_bundle.h"
#include "ac6/retail_content.h"

#include <array>
#include <cstdio>
#include <limits>
#include <optional>

namespace {

int failures = 0;

void check(bool condition, const char *message) {
  if (!condition) {
    std::printf("FAIL  %s\n", message);
    ++failures;
  }
}

void check_unit_transform() {
  using namespace ac6::retail;
  RetailMode2CameraState state;
  state.player_basis = identity_basis();
  state.player_position = {10.0F, 20.0F, 30.0F};
  const std::optional<RetailMode2CameraLocator> locator =
      transform_mode2_camera_locator(state, {1.0F, 2.0F, 3.0F, 99.0F});
  check(locator.has_value() && locator->basis == identity_basis() &&
            locator->position == std::array<float, 3>{11.0F, 22.0F, 33.0F},
        "identity player basis places the local offset in world space");
}

void check_transposed_dot_products() {
  using namespace ac6::retail;
  RetailMode2CameraState state;
  state.player_basis.rows[0] = {1.0F, 2.0F, 3.0F, 4.0F};
  state.player_basis.rows[1] = {5.0F, 6.0F, 7.0F, 8.0F};
  state.player_basis.rows[2] = {9.0F, 10.0F, 11.0F, 12.0F};
  state.player_position = {100.0F, 200.0F, 300.0F};
  const std::optional<RetailMode2CameraLocator> locator =
      transform_mode2_camera_locator(state, {2.0F, 3.0F, 4.0F, 101.0F});
  check(locator.has_value() &&
            locator->position == std::array<float, 3>{153.0F, 262.0F, 371.0F},
        "the three transposed retail dot products use xyz and ignore w");
}

void check_rotation_order() {
  using namespace ac6::retail;
  RetailMode2CameraState state;
  state.player_basis.rows[0] = {1.0F, 2.0F, 3.0F, 4.0F};
  state.player_basis.rows[1] = {5.0F, 6.0F, 7.0F, 8.0F};
  state.player_basis.rows[2] = {9.0F, 10.0F, 11.0F, 12.0F};
  state.rotation_at_3a4 = 0.375F;
  state.rotation_at_3a0 = -0.625F;
  RetailBasis expected = state.player_basis;
  rotate_820A9B30(expected, state.rotation_at_3a4);
  rotate_820A99F8(expected, state.rotation_at_3a0);
  const std::optional<RetailMode2CameraLocator> locator =
      transform_mode2_camera_locator(state, {0.0F, 0.0F, 0.0F, 0.0F});
  check(locator.has_value() && locator->basis == expected,
        "manager +0x3a4 rotates first and +0x3a0 rotates second");
}

void check_refusals_and_record_base() {
  using namespace ac6::retail;
  RetailMode2CameraState state;
  state.player_basis = identity_basis();
  RetailCameraRecord record;
  record.fields[0] = 4.0F;
  record.fields[1] = 5.0F;
  record.fields[2] = 6.0F;
  record.fields[3] = 7.0F;
  const auto direct =
      transform_mode2_camera_locator(state, {4.0F, 5.0F, 6.0F, 7.0F});
  const auto from_record = resolve_mode2_base_camera_locator(record, state);
  check(direct.has_value() && from_record == direct,
        "record stage zero supplies the base manager offset");

  RetailMode2CameraState invalid = state;
  invalid.player_position[1] = std::numeric_limits<float>::quiet_NaN();
  check(!transform_mode2_camera_locator(invalid, {0.0F, 0.0F, 0.0F, 0.0F})
             .has_value(),
        "non-finite live state fails closed");
  check(!transform_mode2_camera_locator(
             state, {0.0F, std::numeric_limits<float>::infinity(), 0.0F, 0.0F})
             .has_value(),
        "non-finite local offset fails closed");
  invalid = state;
  invalid.player_position[0] = std::numeric_limits<float>::max();
  check(!transform_mode2_camera_locator(
             invalid, {std::numeric_limits<float>::max(), 0.0F, 0.0F, 0.0F})
             .has_value(),
        "overflow during world placement fails closed");
}

void check_qualified_cache(const char *cache_path) {
  using namespace ac6::retail;
  ac6::RetailContentStore store;
  check(store.open(cache_path), "the qualified retail cache opens");
  if (!store.valid())
    return;
  const std::optional<RetailCampaignBundle> common =
      RetailCampaignBundle::open_entry(store, kRetailCameraTableEntry);
  const std::optional<RetailCameraTable> cameras =
      common.has_value() ? RetailCameraTable::open(*common) : std::nullopt;
  check(cameras.has_value(), "the common retail camera table opens");
  if (!cameras.has_value())
    return;

  RetailMode2CameraState state;
  state.player_basis = identity_basis();
  std::size_t resolved = 0;
  for (std::uint32_t group = 0; group < kRetailCameraGroups; ++group) {
    const RetailCameraRecord *record = cameras->record(group, 2);
    if (record != nullptr &&
        resolve_mode2_base_camera_locator(*record, state).has_value()) {
      ++resolved;
    }
  }
  check(resolved == kRetailCameraGroups,
        "mode-2 base locators resolve for all 15 retail aircraft groups");
  const RetailCameraRecord *first = cameras->record(0, 2);
  const auto offset = first != nullptr ? first->offset(0) : std::nullopt;
  check(offset.has_value() &&
            *offset == std::array<float, 4>{0.0F, 0.86F, -5.9F, 0.0F},
        "qualified aircraft group zero mode-2 base offset is pinned");
  if (offset.has_value()) {
    std::printf("qualified group0 mode2 offset=%.9g,%.9g,%.9g,%.9g\n",
                (*offset)[0], (*offset)[1], (*offset)[2], (*offset)[3]);
  }
}

} // namespace

int main(int argc, char **argv) {
  check_unit_transform();
  check_transposed_dot_products();
  check_rotation_order();
  check_refusals_and_record_base();
  if (argc >= 2)
    check_qualified_cache(argv[1]);
  if (failures == 0)
    std::printf("retail mode-2 camera base transform OK\n");
  return failures == 0 ? 0 : 1;
}
