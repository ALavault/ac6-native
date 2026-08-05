#pragma once

#include <cstdint>

#include "ac6/product_runtime.h"

namespace ac6 {

// The native HUD is deliberately data-only at its input boundary.  The
// renderer consumes the same WorldFrame, combat world, scenario, radio and
// debrief state as gameplay; it has no Mission 01 constants or demo values.
struct NativeHudSnapshot {
  std::uint64_t tick{};
  float speed{};
  float altitude{};
  std::uint32_t active_units{};
  EntityId player_entity{};
  EntityId target_entity{};
  bool target_locked{};
  std::uint32_t primary_weapon_id{};
  std::uint32_t weapon_count{};
  std::uint32_t objective_count{};
  std::uint32_t active_objective_id{};
  std::uint32_t completed_objectives{};
  std::uint32_t failed_objectives{};
  std::uint32_t radio_message_id{};
  RadioPlaybackState radio_state{RadioPlaybackState::Idle};
  MissionOutcome outcome{MissionOutcome::InProgress};
  ScenarioState scenario_state{ScenarioState::Loading};
  bool reticle_visible{};
  bool telemetry_visible{};
  bool weapon_visible{};
  bool target_visible{};
  bool radar_visible{};
  bool objective_visible{};
  bool radio_visible{};
  bool outcome_visible{};
  bool pause_visible{};
  std::uint64_t pixel_writes{};
  std::uint64_t unique_pixels{};
};

class NativeHudRenderer final {
 public:
  void reset() noexcept { snapshot_ = {}; }
  bool render(NativeRenderTarget& target, const WorldFrame& frame,
              const MissionExecution& execution) noexcept;
  const NativeHudSnapshot& snapshot() const noexcept { return snapshot_; }

 private:
  NativeHudSnapshot snapshot_{};
};

}  // namespace ac6
