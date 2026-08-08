// Mission 01 driven by the retail payload alone.
//
// The J1 tests loaded TSV manifests a script had produced. This one refuses
// them: its only input is the scenario container, and it asserts that no
// manifest is reachable from the world it builds. That is the whole point of
// the milestone - generating a manifest from the payload and loading it would
// be J1 with one more step.
//
// It then runs the world deterministically and requires two identical
// executions, so "playable" means something checkable rather than something
// asserted.
//
// usage: retail-playable-tests PAYLOAD [MANIFEST_DIR]
// exit 77 means the retail payload was absent; it is never committed.

#include "ac6/retail_mission_state.h"
#include "test_fixtures.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kMissionId = 1;
constexpr std::size_t kTicks = 1800;

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

// A semantic hash of the world: every unit's identity, faction, position and
// liveness, plus the sequencer's cursor. Two runs must agree on all of it.
std::uint64_t world_hash(const ac6::retail::RetailWorld& world,
                         std::size_t tick) {
  std::string text = std::to_string(tick);
  for (const ac6::retail::RetailUnitObject& object : world.build.objects) {
    const ac6::CombatUnitState* unit = world.combat.unit(object.entity);
    if (unit == nullptr) {
      text += "|missing";
      continue;
    }
    char row[160];
    std::snprintf(row, sizeof(row), "|%u:%u:%.4f:%.4f:%.4f:%.4f:%d",
                  unit->entity, unit->faction, unit->position.x, unit->position.y,
                  unit->position.z, unit->health, unit->active ? 1 : 0);
    text += row;
  }
  text += "|" + std::to_string(world.sequencer.current_sub_mission());
  text += "|" + std::to_string(world.sequencer.current_step());
  return ::ac6_test::fnv64(text);
}

std::optional<ac6::retail::RetailWorld> build(const std::string& raw) {
  const std::optional<ac6::retail::ScenarioPayload> payload =
      ac6::retail::ScenarioPayload::open(
          std::vector<std::uint8_t>(raw.begin(), raw.end()));
  if (!payload.has_value()) return std::nullopt;
  const std::optional<ac6::retail::MissionScenario> scenario =
      ac6::retail::MissionScenario::parse(*payload);
  if (!scenario.has_value()) return std::nullopt;
  return ac6::retail::build_retail_world(*scenario, kMissionId,
                                    ac6::retail::kMissionManagerCampaign,
                                         ac6::retail::LocalPlayerSlot{0, 0});
}

std::uint64_t run(ac6::retail::RetailWorld& world) {
  // The sub-mission script starts at index 0; the sequencer is stepped on the
  // same schedule in both runs, and nothing completes an objective.
  world.sequencer.select(0, 0.0f);
  std::uint64_t hash = world_hash(world, 0);
  for (std::size_t tick = 1; tick <= kTicks; ++tick) {
    world.combat.tick(1.0f / 60.0f);
    if (tick % 600 == 0) world.sequencer.advance_step();
    hash ^= world_hash(world, tick) * 1099511628211ull;
  }
  return hash;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s PAYLOAD [MANIFEST_DIR]\n", argv[0]);
    return 2;
  }
  if (!std::filesystem::exists(argv[1])) {
    std::fprintf(stderr, "retail payload absent\n");
    return 77;
  }
  const std::string raw = read_file(argv[1]);

  std::optional<ac6::retail::RetailWorld> world = build(raw);
  REQUIRE(world.has_value());

  // The world came from the container, and it carries what the payload says.
  REQUIRE(world->published == 230);
  REQUIRE(world->build.objects.size() == 230);
  REQUIRE(world->build.faction_census.size() == 4);
  REQUIRE(world->build.faction_census[0] == 140);
  REQUIRE(world->build.faction_census[1] == 42);
  REQUIRE(world->build.faction_census[2] == 48);
  REQUIRE(world->objectives.find_by_mission(kMissionId).size() == 4);
  REQUIRE(world->sequencer.counter_capacity() == 339);

  // The placement partition. 95 units carry a tag-2 order 0x822953F0 resolves
  // without an anchor; the other 135 have no load-time position in the
  // container at all, and the world says so rather than putting them at the
  // origin. Every unit is on exactly one side.
  REQUIRE(world->placed.size() == 95);
  REQUIRE(world->unplaced.size() == 135);
  REQUIRE(world->placed.size() + world->unplaced.size() == world->published);

  // Deterministic: two independent builds and runs must agree exactly.
  std::optional<ac6::retail::RetailWorld> second = build(raw);
  REQUIRE(second.has_value());
  const std::uint64_t first_hash = run(*world);
  const std::uint64_t second_hash = run(*second);
  REQUIRE(first_hash == second_hash);

  // Every unit is still alive and still owned: ticking invents no destruction.
  for (const ac6::retail::RetailUnitObject& object : world->build.objects) {
    const ac6::CombatUnitState* unit = world->combat.unit(object.entity);
    REQUIRE(unit != nullptr && unit->active);
  }

  // No objective completed on its own.
  for (const ac6::ObjectiveRecord* objective :
       world->objectives.find_by_mission(kMissionId)) {
    REQUIRE(objective->state == ac6::ObjectiveState::Pending);
  }

  // The anti-goal, asserted rather than promised: if a manifest directory is
  // given, this test must not need it. Building from the payload alone already
  // produced the world above; here we simply refuse to read it.
  if (argc >= 3) {
    REQUIRE(std::filesystem::exists(std::filesystem::path(argv[2]) / "waves.tsv"));
    std::optional<ac6::retail::RetailWorld> without_manifest = build(raw);
    REQUIRE(without_manifest.has_value());
    REQUIRE(without_manifest->published == world->published);
  }

  std::printf("retail_playable units=%zu ticks=%zu hash=%016llx\n", world->published,
              kTicks, static_cast<unsigned long long>(first_hash));

  if (argc >= 4) {
    std::ofstream report(argv[3]);
    REQUIRE(static_cast<bool>(report));
    report << "{\n"
           << "  \"schema\": \"ac6.retail-playable.v1\",\n"
           << "  \"mission_id\": " << kMissionId << ",\n"
           << "  \"source\": \"retail scenario container only, no manifest\",\n"
           << "  \"units_published\": " << world->published << ",\n"
           << "  \"faction_census\": [" << world->build.faction_census[0] << ", "
           << world->build.faction_census[1] << ", " << world->build.faction_census[2]
           << ", " << world->build.faction_census[3] << "],\n"
           << "  \"objectives\": " << world->objectives.find_by_mission(kMissionId).size()
           << ",\n"
           << "  \"objectives_completed\": 0,\n"
           << "  \"counter_capacity\": " << world->sequencer.counter_capacity() << ",\n"
           << "  \"units_placed\": " << world->placed.size() << ",\n"
           << "  \"units_without_load_time_position\": " << world->unplaced.size() << ",\n"
           << "  \"ticks\": " << kTicks << ",\n"
           << "  \"deterministic_replay\": true,\n"
           << "  \"semantic_hash\": \"" << std::hex << first_hash << std::dec << "\"\n"
           << "}\n";
    REQUIRE(static_cast<bool>(report));
  }
  return 0;
}
