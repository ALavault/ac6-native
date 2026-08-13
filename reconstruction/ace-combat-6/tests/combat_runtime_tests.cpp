#include "test_fixtures.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

bool write_file(const std::filesystem::path& path, const std::string& text) {
  std::ofstream output(path, std::ios::binary);
  output << text;
  return static_cast<bool>(output);
}

// The retail wave manifest, loaded transactionally: a manifest with one bad
// line must be refused whole and must not disturb what is already loaded.
void check_retail_waves(const std::filesystem::path& manifests) {
  const std::filesystem::path source = manifests / "waves.tsv";
  if (!std::filesystem::exists(source)) return;
  const std::string valid = read_file(source);

  ac6::MissionWaveDirector waves;
  REQUIRE(waves.load_manifest(source));
  REQUIRE(waves.pending(1) == 230);

  // The faction column is the retail faction byte plus one: three factions,
  // 140, 42 and 48 records.
  std::size_t first = 0;
  std::size_t second = 0;
  std::size_t third = 0;
  for (const ac6::MissionWaveEntrySnapshot& entry : waves.snapshot().entries) {
    if (entry.spawn.combat.faction == 1) first += 1;
    if (entry.spawn.combat.faction == 2) second += 1;
    if (entry.spawn.combat.faction == 3) third += 1;
  }
  REQUIRE(first == 140 && second == 42 && third == 48);

  const std::filesystem::path scratch =
      std::filesystem::temp_directory_path() / "ac6-retail-waves.tsv";
  REQUIRE(write_file(scratch, valid + "1\t1\tnot-a-number\n"));
  REQUIRE(!waves.load_manifest(scratch));
  REQUIRE(waves.pending(1) == 230);        // the accepted state survives

  REQUIRE(write_file(scratch, valid + valid));
  REQUIRE(!waves.load_manifest(scratch));  // duplicate unit ids
  REQUIRE(waves.pending(1) == 230);
  std::filesystem::remove(scratch);
}

}  // namespace

int main(int argc, char** argv) {
  ac6::CombatWorld combat;
  REQUIRE(combat.add_unit({4097, 1, {0.0f, 0.0f, 0.0f}, 100.0f, 100.0f, 1.0f, true}));
  REQUIRE(combat.add_unit({4098, 2, {0.0f, 0.0f, 20.0f}, 80.0f, 80.0f, 1.0f, true}));
  REQUIRE(combat.add_weapon({7, 80.0f, 100.0f, 0.0f, 100.0f}));
  REQUIRE(combat.lock_target(4097, 4098));
  REQUIRE(combat.fire(4097, 7));
  REQUIRE(combat.active_projectiles() == 1);
  combat.tick(1.0f);
  REQUIRE(combat.active_projectiles() == 0);
  REQUIRE(!combat.unit(4098)->active);
  REQUIRE(combat.damage_events() == 1);

  ac6::UnitRegistry units;
  ac6::MissionWaveDirector waves;
  REQUIRE(waves.add({1, 1, {5000, 2, 119, false},
                     {5000, 2, {0.0f, 0.0f, 40.0f}, 40.0f, 40.0f, 1.0f, true}}));
  REQUIRE(waves.spawn_due(1, 1, units, combat));
  REQUIRE(waves.pending(1) == 0 && waves.spawned(1) == 1);

  if (argc >= 2) check_retail_waves(argv[1]);
  return 0;
}
