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

ac6::retail::RetailMode2AxisSuppressionGuard admitted_suppression_guard() {
  ac6::retail::RetailMode2AxisSuppressionGuard guard;
  guard.manager_mode_at_190 = 1U;
  guard.object_descriptor_pointer_at_19c = 0xB5000000U;
  guard.object_serial_at_b0 = 0x12345678U;
  guard.expected_serial_at_1a0 = 0x12345678U;
  return guard;
}

void check_axis_suppression_guard() {
  using namespace ac6::retail;

  const RetailMode2AxisSuppressionGuard admitted = admitted_suppression_guard();
  check(should_suppress_mode2_camera_axes(admitted, true),
        "all retail guards admit a positive tunnel result");
  check(!should_suppress_mode2_camera_axes(admitted, false),
        "a negative tunnel result preserves the axes");

  RetailMode2AxisSuppressionGuard refused = admitted;
  refused.manager_state_at_3c4 = 1U;
  check(!should_suppress_mode2_camera_axes(refused, true),
        "manager+0x3C4 refuses suppression when nonzero");

  refused = admitted;
  refused.manager_mode_at_190 = 2U;
  check(!should_suppress_mode2_camera_axes(refused, true),
        "manager+0x190 refuses suppression outside mode 1");

  refused = admitted;
  refused.object_descriptor_pointer_at_19c = 0U;
  check(!should_suppress_mode2_camera_axes(refused, true),
        "a null CGaObjDesc pointer refuses suppression");

  refused = admitted;
  refused.object_serial_at_b0 ^= 1U;
  check(!should_suppress_mode2_camera_axes(refused, true),
        "a stale CGaObjDesc serial refuses suppression");

  check(!should_suppress_mode2_camera_axes({}, true),
        "a bare legacy suppression boolean fails closed");
}

void check_direct_target_selector() {
  using namespace ac6::retail;

  // This fixture is also micro-executed from canonical PAL 0x82262A28 in
  // analysis/microexec/camera/mode2-target-selector-direct.ppc.json.
  RetailMode2DirectTargetInput direct;
  direct.target_present = true;
  direct.manager_accepts_target = true;
  direct.target_at_e88 = 0.5F;
  direct.target_at_e8c = 0.25F;
  direct.gain_at_350 = 1.0F;
  direct.gain_at_360 = 2.0F;
  direct.gain_at_364 = 0.5F;
  direct.response_rate_at_368 = 0.25F;
  direct.frame_delta = 1.0F;
  const auto selected = select_mode2_direct_camera_rotation(direct);
  check(selected.has_value(),
        "the direct mode-2 target selector accepts retail fields");
  if (selected.has_value()) {
    check_bits(selected->target_at_3a0, 0x3F800000u,
               "positive +0xE88 selects manager+0x360");
    check_bits(selected->target_at_3a4, 0xBE000000u,
               "manager+0x364 scales the negated +0xE8C axis");
    check_bits(selected->target_at_3a8, 0x3F800000u,
               "mode 2 shares the selected gain for +0x3A8");
    check_bits(selected->response, 0x3E800000u,
               "manager+0x368 is multiplied by frame delta");
    check(!selected->wrap_at_3a4,
          "the direct manager+0x4A8 == 0 path does not request wrapping");
    const auto stepped = step_mode2_camera_rotation(*selected);
    check(stepped.has_value(),
          "the selected targets compose with the rotation core");
    if (stepped.has_value()) {
      check_bits(stepped->rotation_at_3a0, 0x3E800000u,
                 "native +0x3A0 matches the retail micro-execution");
      check_bits(stepped->rotation_at_3a4, 0xBD000000u,
                 "native +0x3A4 matches the retail micro-execution");
      check_bits(stepped->rotation_at_3a8, 0x3E800000u,
                 "native +0x3A8 matches the retail micro-execution");
    }
  }

  RetailMode2DirectTargetInput negative = direct;
  negative.target_at_e88 = -2.0F;
  negative.target_at_e8c = -3.0F;
  const auto clamped = select_mode2_direct_camera_rotation(negative);
  check(clamped.has_value(), "finite out-of-range target axes are clamped");
  if (clamped.has_value()) {
    check_bits(clamped->target_at_3a0, 0xBF800000u,
               "negative +0xE88 selects +0x350 then clamps to -1");
    check_bits(clamped->target_at_3a4, 0x3F000000u,
               "negated +0xE8C clamps to +1 before scaling");
  }

  RetailMode2DirectTargetInput absent = direct;
  absent.target_present = false;
  const auto absent_result = select_mode2_direct_camera_rotation(absent);
  check(absent_result.has_value() && absent_result->target_at_3a0 == 0.0F &&
            absent_result->target_at_3a4 == 0.0F &&
            absent_result->target_at_3a8 == 0.0F,
        "an absent target preserves the retail zero-axis path");

  RetailMode2DirectTargetInput refused = direct;
  refused.manager_accepts_target = false;
  const auto refused_result = select_mode2_direct_camera_rotation(refused);
  check(refused_result.has_value() && refused_result->target_at_3a0 == 0.0F &&
            refused_result->target_at_3a4 == 0.0F,
        "manager+0x4A0 gates the target reads");

  RetailMode2DirectTargetInput suppressed = direct;
  suppressed.suppression_guard = admitted_suppression_guard();
  suppressed.suppress_axes = true;
  const auto suppressed_result =
      select_mode2_direct_camera_rotation(suppressed);
  check(suppressed_result.has_value() &&
            suppressed_result->target_at_3a0 == 0.0F &&
            suppressed_result->target_at_3a4 == 0.0F &&
            suppressed_result->target_at_3a8 == 0.0F,
        "the later identity query can suppress both axes");

  RetailMode2DirectTargetInput unqualified = direct;
  unqualified.suppress_axes = true;
  const auto unqualified_result =
      select_mode2_direct_camera_rotation(unqualified);
  check(unqualified_result.has_value() &&
            unqualified_result->target_at_3a0 != 0.0F &&
            unqualified_result->target_at_3a4 != 0.0F,
        "an injected result cannot bypass the structured retail guards");

  RetailMode2DirectTargetInput invalid = direct;
  invalid.gain_at_360 = std::numeric_limits<float>::infinity();
  check(!select_mode2_direct_camera_rotation(invalid).has_value(),
        "non-finite direct selector state fails closed");
}

void check_indirect_scalar_tail() {
  using namespace ac6::retail;

  // The intermediate axes are the positive control micro-executed from
  // canonical PAL 0x8226283C in mode2-indirect-scalar-tail.ppc.json.
  RetailMode2IndirectTargetInput indirect;
  indirect.first_axis = 2.0F;
  indirect.second_axis = -1.0F;
  indirect.gain_at_350 = 1.0F;
  indirect.gain_at_360 = 4.0F;
  indirect.gain_at_364 = 2.0F;
  indirect.response_rate_at_368 = 0.25F;
  indirect.frame_delta = 1.0F;
  const auto axes = normalise_mode2_indirect_camera_axes(indirect);
  check(axes.has_value(),
        "the indirect scalar tail accepts finite retail fields");
  if (axes.has_value()) {
    check_bits(axes->first, 0x3F000000u,
               "the positive axis matches the retail scalar-tail control");
    check_bits(axes->second, 0xBF000000u,
               "the negative axis matches the retail scalar-tail control");
  }

  const auto selected = select_mode2_indirect_camera_rotation(indirect);
  check(selected.has_value(),
        "the indirect axes compose with mode 2 selection");
  if (selected.has_value()) {
    check_bits(selected->target_at_3a0, 0x40000000u,
               "the positive axis is rescaled by manager+0x360");
    check_bits(selected->target_at_3a4, 0xBF800000u,
               "the second axis is rescaled by manager+0x364");
    check_bits(selected->target_at_3a8, 0x40000000u,
               "the indirect mode-2 target is shared with +0x3A8");
    check(selected->wrap_at_3a4,
          "manager+0x4A8 requests the retail shortest-path wrap");
    const auto stepped = step_mode2_camera_rotation(*selected);
    check(stepped.has_value(),
          "the indirect selector composes with the rotation core");
    if (stepped.has_value()) {
      check_bits(stepped->rotation_at_3a0, 0x3F000000u,
                 "the indirect +0x3A0 transition keeps retail grouping");
      check_bits(stepped->rotation_at_3a4, 0xBE800000u,
                 "the indirect +0x3A4 transition keeps retail grouping");
    }
  }

  RetailMode2IndirectTargetInput bounded = indirect;
  bounded.first_axis = -3.0F;
  bounded.second_axis = 5.0F;
  const auto bounded_axes = normalise_mode2_indirect_camera_axes(bounded);
  check(bounded_axes.has_value() && bounded_axes->first == -1.0F &&
            bounded_axes->second == 1.0F,
        "negative and second axes use +0x350/+0x364 and clamp symmetrically");

  RetailMode2IndirectTargetInput zero_limits = indirect;
  zero_limits.gain_at_350 = 0.0F;
  zero_limits.gain_at_360 = 0.0F;
  zero_limits.gain_at_364 = 0.0F;
  const auto zero_axes = normalise_mode2_indirect_camera_axes(zero_limits);
  check(zero_axes.has_value() && zero_axes->first == 0.0F &&
            zero_axes->second == 0.0F,
        "zero limits take the retail zero result without division");

  RetailMode2IndirectTargetInput suppressed = indirect;
  suppressed.suppression_guard = admitted_suppression_guard();
  suppressed.suppress_axes = true;
  const auto suppressed_result =
      select_mode2_indirect_camera_rotation(suppressed);
  check(suppressed_result.has_value() &&
            suppressed_result->target_at_3a0 == 0.0F &&
            suppressed_result->target_at_3a4 == 0.0F,
        "the shared query can suppress indirect axes after gain selection");

  RetailMode2IndirectTargetInput invalid = indirect;
  invalid.gain_at_364 = -1.0F;
  check(!normalise_mode2_indirect_camera_axes(invalid).has_value(),
        "an unqualified negative scalar-tail limit fails closed");
  invalid = indirect;
  invalid.first_axis = std::numeric_limits<float>::infinity();
  check(!select_mode2_indirect_camera_rotation(invalid).has_value(),
        "non-finite indirect selector state fails closed");
}

void check_mode3_gain_curve() {
  using namespace ac6::retail;
  constexpr std::array<float, 4> coefficients{1.0F, 2.0F, 4.0F, 8.0F};
  const auto quarter = evaluate_mode3_camera_gain_curve(coefficients, 0.25F);
  check(quarter.has_value(), "the mode-3 cubic gain accepts finite state");
  if (quarter.has_value()) {
    check_bits(
        *quarter, 0x3FFA0000u,
        "the native cubic gain matches retail 0x8225D660 micro-execution");
  }
  const auto below = evaluate_mode3_camera_gain_curve(coefficients, -2.0F);
  const auto above = evaluate_mode3_camera_gain_curve(coefficients, 3.0F);
  check(below.has_value() && *below == coefficients.front() &&
            above.has_value() && *above == coefficients.back(),
        "the cubic parameter is clamped to the retail endpoints");
  auto invalid = coefficients;
  invalid[2] = std::numeric_limits<float>::quiet_NaN();
  check(!evaluate_mode3_camera_gain_curve(invalid, 0.5F).has_value(),
        "non-finite cubic state fails closed");
}

void check_mode3_axis_normaliser() {
  using namespace ac6::retail;

  // Exact result of canonical PAL 0x8225C680 for x=1,y=0, including the small
  // negative residue returned by the cosine-like retail helper.
  const RetailMode3AxisFactors axis_x_factors{
      1.0F, std::bit_cast<float>(std::uint32_t{0xB33BBD2E})};
  const auto axis_x = normalise_mode3_camera_axes(
      RetailMode3AxisInput{1.0F, 0.0F, false}, axis_x_factors);
  check(axis_x.has_value(), "the mode-3 x-axis retail control is accepted");
  if (axis_x.has_value()) {
    check_bits(axis_x->first, 0x3F800000u,
               "mode-3 first output matches 0x8225C680 micro-execution");
    check_bits(axis_x->second, 0xB33BBD2Eu,
               "mode-3 second output keeps the retail cosine residue");
  }

  const RetailMode3AxisFactors first_only{1.0F, 0.0F};
  const auto default_scale = normalise_mode3_camera_axes(
      RetailMode3AxisInput{0.5F, 0.0F, false}, first_only);
  const auto alternate_scale = normalise_mode3_camera_axes(
      RetailMode3AxisInput{0.5F, 0.0F, true}, first_only);
  check(default_scale.has_value() && alternate_scale.has_value(),
        "both qualified mode-3 radial scales are accepted");
  if (default_scale.has_value() && alternate_scale.has_value()) {
    check_bits(default_scale->first, 0x3F200000u,
               "manager+0x4A8 clear selects the retail 1.25 scale");
    check_bits(alternate_scale->first, 0x3F000000u,
               "manager+0x4A8 set selects the retail 1.0 scale");
  }

  // These exact float words distinguish retail's fused y*y+x*x from rounding
  // y*y separately before the addition: the resulting magnitudes differ by
  // one ulp (0x3EB1EBB7 versus 0x3EB1EBB6).
  const auto fused_radius = normalise_mode3_camera_axes(
      RetailMode3AxisInput{
          std::bit_cast<float>(std::uint32_t{0x3E85ACE4}),
          std::bit_cast<float>(std::uint32_t{0x3E6AD561}), true},
      first_only);
  check(fused_radius.has_value(), "the fused retail radius control is accepted");
  if (fused_radius.has_value()) {
    check_bits(fused_radius->first, 0x3EB1EBB7u,
               "the radius keeps float x*x then fused y*y+x*x grouping");
  }

  const auto clamped = normalise_mode3_camera_axes(
      RetailMode3AxisInput{2.0F, 0.0F, false},
      RetailMode3AxisFactors{0.25F, -0.5F});
  check(clamped.has_value() && clamped->first == 0.25F &&
            clamped->second == -0.5F,
        "the scaled radius is clamped to one before both products");

  const auto signed_zero = normalise_mode3_camera_axes(
      RetailMode3AxisInput{-0.0F, 0.0F, false},
      RetailMode3AxisFactors{1.0F, 1.0F});
  check(signed_zero.has_value(), "the retail zero/zero branch is accepted");
  if (signed_zero.has_value()) {
    check_bits(signed_zero->first, 0x80000000u,
               "the first slot preserves its negative zero sign");
    check_bits(signed_zero->second, 0x00000000u,
               "the second slot preserves its positive zero sign");
  }

  RetailMode3AxisInput invalid{1.0F, 0.0F, false};
  invalid.x = std::numeric_limits<float>::quiet_NaN();
  check(!normalise_mode3_camera_axes(invalid, first_only).has_value(),
        "a non-finite mode-3 axis fails closed");
  invalid = {1.0F, std::numeric_limits<float>::infinity(), false};
  check(!normalise_mode3_camera_axes(invalid, first_only).has_value(),
        "a non-finite second mode-3 axis fails closed");
  invalid = {1.0F, 0.0F, false};
  check(!normalise_mode3_camera_axes(
             invalid,
             RetailMode3AxisFactors{
                 std::numeric_limits<float>::infinity(), 0.0F})
             .has_value(),
        "a non-finite injected retail factor fails closed");
  invalid = {std::numeric_limits<float>::max(), 0.0F, false};
  check(!normalise_mode3_camera_axes(invalid, first_only).has_value(),
        "overflow in the separately rounded x square fails closed");
  invalid = {0.0F, std::numeric_limits<float>::max(), false};
  check(!normalise_mode3_camera_axes(invalid, first_only).has_value(),
        "overflow in the fused radius square fails closed");

  // The bounded output is directly useful to the already-qualified gain and
  // rotation stages without claiming the still-open complete mode-3 selector.
  const auto composed_axes = normalise_mode3_camera_axes(
      RetailMode3AxisInput{0.3F, 0.4F, true},
      RetailMode3AxisFactors{0.6F, 0.8F});
  check(composed_axes.has_value(),
        "finite normalized mode-3 axes are available to later stages");
  if (composed_axes.has_value()) {
    const auto gain = evaluate_mode3_camera_gain_curve(
        std::array<float, 4>{1.0F, 1.0F, 1.0F, 1.0F},
        std::fabs(composed_axes->second));
    std::optional<RetailMode2RotationState> stepped;
    if (gain.has_value()) {
      RetailMode2RotationInput rotation;
      rotation.target_at_3a0 = *gain * composed_axes->first;
      rotation.target_at_3a4 = composed_axes->second;
      rotation.target_at_3a8 = *gain * composed_axes->first;
      rotation.response = 0.5F;
      stepped = step_mode2_camera_rotation(rotation);
    }
    check(stepped.has_value(),
          "mode-3 axes compose with the bounded gain and rotation cores");
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
  check_axis_suppression_guard();
  check_direct_target_selector();
  check_indirect_scalar_tail();
  check_mode3_axis_normaliser();
  check_mode3_gain_curve();
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
