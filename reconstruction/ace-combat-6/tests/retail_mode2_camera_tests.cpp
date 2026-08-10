#include "ac6/retail_mode2_camera.h"

#include "ac6/retail_campaign_bundle.h"
#include "ac6/retail_content.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <string_view>

namespace {

int failures = 0;

void check(bool condition, const char *message) {
  if (!condition) {
    std::printf("FAIL  %s\n", message);
    ++failures;
  }
}

void check_bits(float value, std::uint32_t expected, const char *message) {
  check(std::bit_cast<std::uint32_t>(value) == expected, message);
}

ac6::retail::RetailMode2ShakeState shake_fixture(bool nonzero_z = true) {
  ac6::retail::RetailMode2ShakeState state;
  state.start = {1.0F, 2.0F, nonzero_z ? 3.0F : 0.0F, 0.0F};
  state.target = {5.0F, 8.0F, nonzero_z ? 11.0F : 0.0F, 0.0F};
  state.output = {0.2F, 0.3F, nonzero_z ? 0.4F : 0.0F, 0.0F};
  state.velocity = {0.1F, 0.2F, nonzero_z ? 0.3F : 0.0F, 0.0F};
  state.elapsed = 0.05F;
  return state;
}

ac6::retail::RetailMode2DynamicInput reset_input() {
  ac6::retail::RetailMode2DynamicInput input;
  input.player_present = true;
  input.player_is_current = true;
  input.frame_delta = 1.0F / 60.0F;
  input.shake = shake_fixture(false);
  return input;
}

void check_shake_integrator() {
  using namespace ac6::retail;
  const std::array<std::uint16_t, 2> draws{123u, 456u};
  const std::optional<RetailMode2ShakeStepResult> low =
      step_mode2_camera_shake(shake_fixture(), 0.1F, 2.0F, 0.01F, draws);
  check(low.has_value() && low->random_draws_consumed == 0,
        "the low-phase shake step consumes no random draw");
  if (low.has_value()) {
    check_bits(low->state.velocity[0], 0x3E0F5C29u,
               "low phase velocity x keeps retail multiply/add rounds");
    check_bits(low->state.velocity[1], 0x3E851EB8u,
               "low phase velocity y keeps retail multiply/add rounds");
    check_bits(low->state.output[0], 0x3E4E3BCDu,
               "low phase output x is the fused retail integration");
    check_bits(low->state.output[1], 0x3E9AEE64u,
               "low phase output y is the fused retail integration");
    check_bits(low->state.output[2], 0x41000000u,
               "the output z lane retains target minus start");
    check_bits(low->state.velocity[2], 0x00000000u,
               "retail explicitly clears velocity z");
    check_bits(low->state.elapsed, 0x3D75C290u,
               "elapsed advances after the output update");
  }

  RetailMode2ShakeState high_state = shake_fixture();
  high_state.elapsed = 0.075F;
  const std::optional<RetailMode2ShakeStepResult> high =
      step_mode2_camera_shake(high_state, 0.1F, 2.0F, 0.01F, draws);
  check(high.has_value() && high->random_draws_consumed == 0,
        "the high-phase shake step consumes no random draw");
  if (high.has_value()) {
    check_bits(high->state.velocity[0], 0x3CA3D70Cu,
               "high phase doubles/scales/subtracts velocity x");
    check_bits(high->state.velocity[1], 0x3DA3D70Bu,
               "high phase doubles/scales/subtracts velocity y");
    check_bits(high->state.output[0], 0x3E4D013Bu,
               "high phase output x is pinned");
    check_bits(high->state.output[1], 0x3E9A0276u,
               "high phase output y is pinned");
  }

  RetailMode2ShakeState refresh_state = shake_fixture(false);
  refresh_state.elapsed = 0.11F;
  const std::optional<RetailMode2ShakeStepResult> refreshed =
      step_mode2_camera_shake(refresh_state, 0.1F, 2.0F, 0.01F,
                              {0u, 32767u});
  check(refreshed.has_value() && refreshed->random_draws_consumed == 2,
        "elapsed strictly above the period consumes exactly two RNG draws");
  if (refreshed.has_value()) {
    check(refreshed->state.start == refresh_state.target &&
              refreshed->state.target ==
                  std::array<float, 4>{-2.0F, 2.0F, 0.0F, 0.0F},
          "0x8225C178 maps the two injected endpoint draws exactly");
    check_bits(refreshed->state.output[0], 0x3E4C154Du,
               "refreshed target produces the pinned x output");
    check_bits(refreshed->state.output[1], 0x3E994AF5u,
               "refreshed target produces the pinned y output");
  }

  const std::optional<RetailMode2ShakeStepResult> inert =
      step_mode2_camera_shake(shake_fixture(), 0.0F, 2.0F, 0.01F, draws);
  check(inert.has_value() && inert->state == shake_fixture() &&
            inert->random_draws_consumed == 0,
        "a sub-epsilon period returns the state unchanged");
}

void check_dynamic_branches() {
  using namespace ac6::retail;
  const std::array<float, 4> base{1.0F, 2.0F, 3.0F, 4.0F};

  RetailMode2DynamicInput guarded = reset_input();
  guarded.player_present = false;
  const auto guard_result = apply_mode2_dynamic_offset(base, guarded);
  check(guard_result.has_value() &&
            guard_result->branch == RetailMode2DynamicBranch::GuardedOut &&
            guard_result->local_offset == base &&
            guard_result->shake == guarded.shake,
        "a missing current player leaves offset and shake state unchanged");

  RetailMode2DynamicInput reset = reset_input();
  const auto reset_result = apply_mode2_dynamic_offset(base, reset);
  check(reset_result.has_value() &&
            reset_result->branch == RetailMode2DynamicBranch::Reset &&
            reset_result->local_offset == base &&
            reset_result->shake == RetailMode2ShakeState{},
        "the low player field branch clears every qualified shake field");

  RetailMode2DynamicInput direct = reset_input();
  direct.player_at_8a0 = 0.5F;
  direct.manager_at_370 = 4.0F;
  direct.frame_delta = 0.01F;
  const auto direct_result = apply_mode2_dynamic_offset(base, direct);
  check(direct_result.has_value() &&
            direct_result->branch == RetailMode2DynamicBranch::Direct &&
            direct_result->random_draws_consumed == 0,
        "a non-trivial +0x8A0 field selects the direct-amplitude branch");
  if (direct_result.has_value()) {
    check_bits(direct_result->local_offset[0], 0x3F99C77Au,
               "direct branch adds the retail x output to the base offset");
    check_bits(direct_result->local_offset[1], 0x40135DCCu,
               "direct branch adds the retail y output to the base offset");
    check_bits(direct_result->local_offset[2], 0x40400000u,
               "direct branch keeps the invariant zero z output");
  }

  RetailMode2DynamicInput scaled = reset_input();
  scaled.player_at_890 = 10.0F;
  scaled.player_at_894 = 10.0F;
  scaled.player_at_898 = 12.0F;
  scaled.manager_at_374 = 3.0F;
  scaled.shake = {};
  const auto scaled_result = apply_mode2_dynamic_offset(base, scaled);
  check(scaled_result.has_value() &&
            scaled_result->branch == RetailMode2DynamicBranch::Scaled &&
            scaled_result->local_offset == base,
        "the +0x890 threshold selects and executes the scaled branch");

  check(std::string_view(mode2_dynamic_branch_name(
            RetailMode2DynamicBranch::GuardedOut)) == "guarded_out" &&
            std::string_view(mode2_dynamic_branch_name(
                RetailMode2DynamicBranch::Reset)) == "reset" &&
            std::string_view(mode2_dynamic_branch_name(
                RetailMode2DynamicBranch::Direct)) == "direct" &&
            std::string_view(mode2_dynamic_branch_name(
                RetailMode2DynamicBranch::Scaled)) == "scaled",
        "all dynamic branches have stable audit names");
}

void check_dynamic_refusals() {
  using namespace ac6::retail;
  const std::array<float, 4> base{};
  RetailMode2DynamicInput invalid = reset_input();
  invalid.player_at_890 = std::numeric_limits<float>::quiet_NaN();
  check(!apply_mode2_dynamic_offset(base, invalid).has_value(),
        "non-finite dynamic input fails closed");
  invalid = reset_input();
  invalid.random_draws[1] = 32768u;
  check(!apply_mode2_dynamic_offset(base, invalid).has_value(),
        "an RNG result outside the retail 15-bit range fails closed");
  RetailMode2ShakeState invalid_shake = shake_fixture();
  invalid_shake.output[0] = std::numeric_limits<float>::infinity();
  check(!step_mode2_camera_shake(invalid_shake, 0.1F, 2.0F, 0.01F,
                                 {0u, 0u})
             .has_value(),
        "non-finite carried shake state fails closed");

  invalid = reset_input();
  invalid.player_at_8a0 = 1.0F;
  invalid.manager_at_370 = std::numeric_limits<float>::max();
  invalid.shake = {};
  invalid.shake.output[0] = std::numeric_limits<float>::max();
  const std::array<float, 4> huge_base{
      std::numeric_limits<float>::max(), 0.0F, 0.0F, 0.0F};
  check(!apply_mode2_dynamic_offset(huge_base, invalid).has_value(),
        "overflow while adding the dynamic output fails closed");
}

void check_rotation_core() {
  using namespace ac6::retail;
  RetailMode2RotationInput input;
  input.target_at_3a0 = 0.5F;
  input.target_at_3a4 = -0.75F;
  input.target_at_3a8 = 0.25F;
  input.response = 0.5F;
  const std::optional<RetailMode2RotationState> result =
      step_mode2_camera_rotation(input);
  check(result.has_value(), "the bounded camera rotation core accepts finite state");
  if (result.has_value()) {
    check_bits(result->rotation_at_3a0, 0x3E800000u,
               "rotation +0x3A0 uses the retail fused interpolation");
    check_bits(result->rotation_at_3a4, 0xBEC00000u,
               "rotation +0x3A4 uses the retail fused interpolation");
    check_bits(result->rotation_at_3a8, 0x3E000000u,
               "rotation +0x3A8 uses the retail fused interpolation");
  }

  RetailMode2RotationInput wrapped;
  wrapped.current.rotation_at_3a4 = 3.0F;
  wrapped.target_at_3a4 = -3.0F;
  wrapped.response = 0.5F;
  wrapped.wrap_at_3a4 = true;
  const auto wrapped_result = step_mode2_camera_rotation(wrapped);
  check(wrapped_result.has_value() &&
            std::fabs(wrapped_result->rotation_at_3a4) <= 3.1415928F,
        "rotation +0x3A4 follows the shortest wrapped path");

  RetailMode2RotationInput snapped;
  snapped.target_at_3a0 = 0.0005F;
  snapped.target_at_3a4 = -0.0005F;
  snapped.target_at_3a8 = 0.0005F;
  snapped.response = 1.0F;
  const auto snapped_result = step_mode2_camera_rotation(snapped);
  check(snapped_result.has_value() &&
            snapped_result->rotation_at_3a0 == 0.0F &&
            snapped_result->rotation_at_3a4 == 0.0F &&
            snapped_result->rotation_at_3a8 == 0.0005F,
        "the retail 0x001 snap clears only +0x3A0/+0x3A4");

  RetailMode2RotationInput invalid;
  invalid.current.rotation_at_3a0 = std::numeric_limits<float>::infinity();
  check(!step_mode2_camera_rotation(invalid).has_value(),
        "non-finite camera rotation state fails closed");
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
  const RetailMode2DynamicInput dynamic = reset_input();
  for (std::uint32_t group = 0; group < kRetailCameraGroups; ++group) {
    const RetailCameraRecord *record = cameras->record(group, 2);
    const std::optional<std::array<float, 4>> base =
        record != nullptr ? record->offset(0) : std::nullopt;
    const std::optional<RetailMode2DynamicResult> adjusted =
        base.has_value() ? apply_mode2_dynamic_offset(*base, dynamic)
                         : std::nullopt;
    if (adjusted.has_value() &&
        adjusted->branch == RetailMode2DynamicBranch::Reset &&
        transform_mode2_camera_locator(state, adjusted->local_offset)
            .has_value()) {
      ++resolved;
    }
  }
  check(resolved == kRetailCameraGroups,
        "mode-2 dynamic reset and locators resolve for all 15 retail groups");
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
  check_shake_integrator();
  check_dynamic_branches();
  check_dynamic_refusals();
  check_rotation_core();
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
