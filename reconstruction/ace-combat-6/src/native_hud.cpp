#include "ac6/native_hud.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ac6 {

namespace {

constexpr std::uint32_t kHudGreen = 0xFF63F5A5u;
constexpr std::uint32_t kHudAmber = 0xFFFFC857u;
constexpr std::uint32_t kHudRed = 0xFFFF5C5Cu;
constexpr std::uint32_t kHudBlue = 0xFF68B5FFu;
constexpr std::uint32_t kHudDim = 0xFF173A42u;

bool rect(NativeRenderTarget& target, std::uint32_t x0, std::uint32_t y0,
          std::uint32_t x1, std::uint32_t y1, std::uint32_t color) noexcept {
  return target.draw_hud_rect(x0, y0, x1, y1, color);
}

bool outline(NativeRenderTarget& target, std::uint32_t x0, std::uint32_t y0,
             std::uint32_t x1, std::uint32_t y1, std::uint32_t thickness,
             std::uint32_t color) noexcept {
  if (x0 > x1 || y0 > y1 || thickness == 0) return false;
  const std::uint32_t right = x1 < thickness ? x1 : x1 - thickness + 1u;
  const std::uint32_t bottom = y1 < thickness ? y1 : y1 - thickness + 1u;
  return rect(target, x0, y0, x1, y0 + thickness - 1u, color) &&
         rect(target, x0, bottom, x1, y1, color) &&
         rect(target, x0, y0, x0 + thickness - 1u, y1, color) &&
         rect(target, right, y0, x1, y1, color);
}

bool digit(NativeRenderTarget& target, std::uint32_t x, std::uint32_t y,
           std::uint32_t scale, std::uint32_t value, std::uint32_t color) noexcept {
  if (value > 9u || scale == 0) return false;
  constexpr std::array<std::array<bool, 7>, 10> segments{{
      {{true, true, true, true, true, true, false}},
      {{false, true, true, false, false, false, false}},
      {{true, true, false, true, true, false, true}},
      {{true, true, true, true, false, false, true}},
      {{false, true, true, false, false, true, true}},
      {{true, false, true, true, false, true, true}},
      {{true, false, true, true, true, true, true}},
      {{true, true, true, false, false, false, false}},
      {{true, true, true, true, true, true, true}},
      {{true, true, true, true, false, true, true}},
  }};
  const std::uint32_t thickness = std::max(1u, scale / 2u);
  const std::uint32_t width = 5u * scale;
  const std::uint32_t height = 7u * scale;
  const auto draw = [&](std::size_t index, std::uint32_t sx, std::uint32_t sy,
                        std::uint32_t ex, std::uint32_t ey) noexcept {
    return !segments[value][index] || rect(target, x + sx, y + sy, x + ex, y + ey, color);
  };
  return draw(0, 0, 0, width, thickness - 1u) &&
         draw(1, width - thickness + 1u, 0, width, height / 2u - 1u) &&
         draw(2, width - thickness + 1u, height / 2u + 1u, width, height) &&
         draw(3, 0, height - thickness + 1u, width, height) &&
         draw(4, 0, height / 2u + 1u, thickness - 1u, height) &&
         draw(5, 0, 0, thickness - 1u, height / 2u - 1u) &&
         draw(6, 0, height / 2u - thickness / 2u, width, height / 2u + thickness / 2u);
}

bool number(NativeRenderTarget& target, std::uint32_t x, std::uint32_t y,
           std::uint32_t scale, std::uint32_t value, std::uint32_t color) noexcept {
  value = std::min(value, 999999u);
  std::array<std::uint32_t, 6> digits{};
  std::size_t count = 0;
  do {
    digits[count++] = value % 10u;
    value /= 10u;
  } while (value != 0 && count < digits.size());
  const std::uint32_t advance = 7u * scale;
  bool result = true;
  for (std::size_t index = 0; index < count; ++index) {
    result = digit(target, x + static_cast<std::uint32_t>(count - index - 1u) * advance,
                   y, scale, digits[index], color) && result;
  }
  return result;
}

std::uint32_t scaled_abs(float value) noexcept {
  if (!std::isfinite(value)) return 0;
  return static_cast<std::uint32_t>(std::clamp(std::lround(std::abs(value) * 10.0f), 0l,
                                               999999l));
}

std::uint32_t state_color(ObjectiveState state) noexcept {
  switch (state) {
    case ObjectiveState::Active: return kHudAmber;
    case ObjectiveState::Complete: return kHudGreen;
    case ObjectiveState::Failed: return kHudRed;
    case ObjectiveState::Pending: return kHudDim;
  }
  return kHudDim;
}

}  // namespace

bool NativeHudRenderer::render(NativeRenderTarget& target, const WorldFrame& frame,
                               const MissionExecution& execution) noexcept {
  if (target.width() == 0 || target.height() == 0 ||
      !execution.launched() || execution.scenario().mission_id() != frame.mission_id) {
    return false;
  }

  snapshot_ = {};
  snapshot_.tick = frame.tick;
  snapshot_.speed = frame.speed;
  snapshot_.altitude = frame.position_y;
  snapshot_.active_units = frame.active_units;
  snapshot_.player_entity = frame.player_entity;
  snapshot_.target_entity = execution.combat().locked_target(frame.player_entity);
  snapshot_.target_locked = snapshot_.target_entity != 0 &&
      execution.combat().unit(snapshot_.target_entity) != nullptr &&
      execution.combat().unit(snapshot_.target_entity)->active;
  snapshot_.primary_weapon_id = execution.primary_weapon_id();
  snapshot_.weapon_count = execution.weapon_count();
  snapshot_.scenario_state = execution.scenario().state();
  const MissionDebrief debrief = execution.debrief();
  snapshot_.outcome = debrief.outcome;
  snapshot_.completed_objectives = debrief.completed_objectives;
  snapshot_.failed_objectives = debrief.failed_objectives;

  const std::vector<ObjectiveRecord> objectives = execution.scenario().objectives().snapshot();
  snapshot_.objective_count = static_cast<std::uint32_t>(objectives.size());
  for (const ObjectiveRecord& objective : objectives) {
    if (objective.state == ObjectiveState::Active ||
        (snapshot_.active_objective_id == 0 && objective.state == ObjectiveState::Pending)) {
      snapshot_.active_objective_id = objective.id;
    }
  }
  const RadioPlaybackSnapshot radio = execution.radio().snapshot();
  snapshot_.radio_message_id = radio.message_id;
  snapshot_.radio_state = radio.state;

  const std::uint32_t width = target.width();
  const std::uint32_t height = target.height();
  const std::uint32_t scale = std::max(1u, std::min(width, height) / 360u);
  const std::uint32_t margin = 8u * scale;
  const std::uint32_t thickness = std::max(1u, scale / 2u);
  const std::uint32_t panel_width = 150u * scale;
  const std::uint32_t panel_height = 48u * scale;
  const std::uint32_t bottom_y = height > margin + panel_height
      ? height - margin - panel_height : 0;
  bool drawn = true;

  // Flight reticle: fixed screen geometry, with visibility governed by the
  // native gameplay frame.
  const std::uint32_t cx = width / 2u;
  const std::uint32_t cy = height / 2u;
  const std::uint32_t arm = 12u * scale;
  const std::uint32_t gap = 5u * scale;
  drawn = rect(target, cx - arm, cy - thickness / 2u, cx - gap, cy + thickness / 2u, kHudGreen) && drawn;
  drawn = rect(target, cx + gap, cy - thickness / 2u, cx + arm, cy + thickness / 2u, kHudGreen) && drawn;
  drawn = rect(target, cx - thickness / 2u, cy - arm, cx + thickness / 2u, cy - gap, kHudGreen) && drawn;
  drawn = rect(target, cx - thickness / 2u, cy + gap, cx + thickness / 2u, cy + arm, kHudGreen) && drawn;
  snapshot_.reticle_visible = drawn;

  // Telemetry panel: speed and altitude are generated by the flight
  // integrator, never from a display fixture.
  const std::uint32_t telemetry_x = margin;
  drawn = outline(target, telemetry_x, bottom_y, telemetry_x + panel_width, bottom_y + panel_height,
                  thickness, kHudGreen) && drawn;
  drawn = number(target, telemetry_x + 5u * scale, bottom_y + 5u * scale, scale,
                 scaled_abs(frame.speed), kHudGreen) && drawn;
  drawn = number(target, telemetry_x + 82u * scale, bottom_y + 5u * scale, scale,
                 scaled_abs(frame.position_y), kHudBlue) && drawn;
  const std::uint32_t speed_bar = std::min(45u * scale,
      static_cast<std::uint32_t>(std::clamp(std::abs(frame.speed) * 2.0f, 0.0f,
                                            static_cast<float>(45u * scale))));
  drawn = rect(target, telemetry_x + 5u * scale, bottom_y + 39u * scale,
               telemetry_x + 5u * scale + speed_bar, bottom_y + 42u * scale, kHudGreen) && drawn;
  snapshot_.telemetry_visible = drawn;

  // Weapon/loadout panel.  The primary weapon and store count come from the
  // launched MissionLaunchDefinition consumed by MissionExecution.
  if (snapshot_.primary_weapon_id != 0 || snapshot_.weapon_count != 0) {
    const std::uint32_t weapon_x = width > margin + panel_width ? width - margin - panel_width : 0;
    drawn = outline(target, weapon_x, bottom_y, weapon_x + panel_width, bottom_y + panel_height,
                    thickness, kHudAmber) && drawn;
    drawn = number(target, weapon_x + 5u * scale, bottom_y + 5u * scale, scale,
                   snapshot_.primary_weapon_id, kHudAmber) && drawn;
    drawn = number(target, weapon_x + 82u * scale, bottom_y + 5u * scale, scale,
                   snapshot_.weapon_count, kHudAmber) && drawn;
    const std::uint32_t stores = std::min(45u * scale, snapshot_.weapon_count * 8u * scale);
    drawn = rect(target, weapon_x + 5u * scale, bottom_y + 39u * scale,
                 weapon_x + 5u * scale + stores, bottom_y + 42u * scale, kHudAmber) && drawn;
    snapshot_.weapon_visible = drawn;
  }

  // Objective panel is emitted only when the native scenario actually owns
  // objective records.  This avoids presenting a synthetic Mission 01 label
  // when the retail scenario manifest is not yet qualified.
  if (snapshot_.objective_count != 0) {
    const std::uint32_t objective_x = margin;
    const std::uint32_t objective_y = margin;
    ObjectiveState state = ObjectiveState::Pending;
    for (const ObjectiveRecord& objective : objectives) {
      if (objective.id == snapshot_.active_objective_id) {
        state = objective.state;
        break;
      }
    }
    const std::uint32_t color = state_color(state);
    drawn = outline(target, objective_x, objective_y, objective_x + panel_width,
                    objective_y + panel_height, thickness, color) && drawn;
    drawn = number(target, objective_x + 5u * scale, objective_y + 5u * scale, scale,
                   snapshot_.active_objective_id, color) && drawn;
    drawn = number(target, objective_x + 82u * scale, objective_y + 5u * scale, scale,
                   snapshot_.completed_objectives, kHudGreen) && drawn;
    snapshot_.objective_visible = drawn;
  }

  // Radar panel uses live combat-unit positions relative to the player.
  const std::uint32_t radar_width = 76u * scale;
  const std::uint32_t radar_x = width > margin + radar_width ? width - margin - radar_width : 0;
  const std::uint32_t radar_y = margin;
  drawn = outline(target, radar_x, radar_y, radar_x + radar_width, radar_y + radar_width,
                  thickness, kHudBlue) && drawn;
  const std::uint32_t radar_cx = radar_x + radar_width / 2u;
  const std::uint32_t radar_cy = radar_y + radar_width / 2u;
  drawn = rect(target, radar_cx - thickness, radar_cy - thickness,
               radar_cx + thickness, radar_cy + thickness, kHudGreen) && drawn;
  if (const CombatUnitState* target_unit = execution.combat().unit(snapshot_.target_entity);
      target_unit != nullptr && target_unit->active) {
    const CombatUnitState* player_unit = execution.combat().unit(frame.player_entity);
    if (player_unit != nullptr) {
      const float dx = target_unit->position.x - player_unit->position.x;
      const float dz = target_unit->position.z - player_unit->position.z;
      const auto radar_coord = [](float value, std::uint32_t center,
                                  std::uint32_t half) noexcept {
        const float normalized = std::clamp(value / 64.0f, -1.0f, 1.0f);
        return static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(std::lround(static_cast<float>(center) + normalized * half)),
            0, static_cast<int>(center + half)));
      };
      const std::uint32_t tx = radar_coord(dx, radar_cx, radar_width / 2u - 2u * scale);
      const std::uint32_t ty = radar_coord(dz, radar_cy, radar_width / 2u - 2u * scale);
      drawn = rect(target, tx, ty, tx + 2u * scale, ty + 2u * scale,
                   snapshot_.target_locked ? kHudRed : kHudAmber) && drawn;
    }
  }
  snapshot_.target_visible = snapshot_.target_entity != 0;
  snapshot_.radar_visible = drawn;

  if (snapshot_.radio_message_id != 0 || !execution.scenario().radio_history().empty()) {
    const std::uint32_t radio_width = std::min(width - 2u * margin, 230u * scale);
    const std::uint32_t radio_x = (width - radio_width) / 2u;
    const std::uint32_t radio_y = height > 2u * margin + 30u * scale
        ? height - 2u * margin - 30u * scale : margin;
    drawn = outline(target, radio_x, radio_y, radio_x + radio_width, radio_y + 24u * scale,
                    thickness, kHudBlue) && drawn;
    drawn = number(target, radio_x + 5u * scale, radio_y + 5u * scale, scale,
                   snapshot_.radio_message_id, kHudBlue) && drawn;
    const float duration = std::max(0.001f, radio.duration_seconds);
    const std::uint32_t progress = static_cast<std::uint32_t>(std::clamp(
        radio.elapsed_seconds / duration, 0.0f, 1.0f) * static_cast<float>(radio_width - 10u * scale));
    drawn = rect(target, radio_x + 5u * scale, radio_y + 20u * scale,
                 radio_x + 5u * scale + progress, radio_y + 22u * scale, kHudBlue) && drawn;
    snapshot_.radio_visible = drawn;
  }

  if (snapshot_.scenario_state == ScenarioState::Paused ||
      snapshot_.outcome != MissionOutcome::InProgress) {
    const std::uint32_t panel_x = width / 2u - 45u * scale;
    const std::uint32_t panel_y = height / 2u - 18u * scale;
    const std::uint32_t color = snapshot_.scenario_state == ScenarioState::Paused
        ? kHudAmber : (snapshot_.outcome == MissionOutcome::Success ? kHudGreen : kHudRed);
    drawn = outline(target, panel_x, panel_y, panel_x + 90u * scale,
                    panel_y + 36u * scale, thickness, color) && drawn;
    snapshot_.pause_visible = snapshot_.scenario_state == ScenarioState::Paused;
    snapshot_.outcome_visible = snapshot_.outcome != MissionOutcome::InProgress;
  }

  snapshot_.pixel_writes = target.hud_pixel_writes();
  snapshot_.unique_pixels = target.hud_unique_pixels();
  return drawn;
}

}  // namespace ac6
