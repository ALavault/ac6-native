#include "test_fixtures.h"

int main() {
  ac6::CombatWorld combat;
  REQUIRE(combat.add_unit({4097, 1, {0.0f, 0.0f, 0.0f}, 100.0f, 100.0f, 1.0f, true}));
  REQUIRE(combat.add_unit({4098, 2, {0.0f, 0.0f, 20.0f}, 80.0f, 80.0f, 1.0f, true}));
  REQUIRE(combat.add_weapon({7, 20.0f, 40.0f, 0.0f, 100.0f}));
  REQUIRE(combat.lock_target(4097, 4098));
  REQUIRE(combat.fire(4097, 7));
  REQUIRE(combat.active_projectiles() == 1);
  combat.tick(1.0f);
  REQUIRE(combat.apply_damage(4098, 80.0f));
  REQUIRE(!combat.unit(4098)->active);
  REQUIRE(combat.damage_events() == 1);

  ac6::UnitRegistry units;
  ac6::MissionWaveDirector waves;
  REQUIRE(waves.add({1, 1, {5000, 2, 119, false},
                     {5000, 2, {0.0f, 0.0f, 40.0f}, 40.0f, 40.0f, 1.0f, true}}));
  REQUIRE(waves.spawn_due(1, 1, units, combat));
  REQUIRE(waves.pending(1) == 0 && waves.spawned(1) == 1);
  return 0;
}
